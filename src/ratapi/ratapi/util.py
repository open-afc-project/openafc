''' Shared utility classes and functions for views.
'''

import os
import json
import logging
import flask
import tempfile
import shutil
import uuid
from celery import Celery
from werkzeug.exceptions import HTTPException
from werkzeug.http import HTTP_STATUS_CODES
from werkzeug._internal import _get_environ

import als

#: LOGGER for this module
LOGGER = logging.getLogger(__name__)


class Response(flask.Response):
    ''' A derived Response object with adjusted default values.
    '''
    #: HTTP 1.1 (RFC 7231) explicitly allows relative Location URIs
    #: but Qt 5.9 does not accept relative URLs
    autocorrect_location_header = True


class AFCEngineException(HTTPException):
    ''' Custom exception class for AFC Engine.
        Used when the AFC Engine encounters an internal error.
    '''

    code = 550    # custom error code for AFC Engine Exceptions
    description = 'Your request was unable to be processed because of an unknown error.'

    def __init__(self, description=None, response=None, exit_code=None):
        HTTPException.__init__(self)
        if description is not None:
            self.description = description
        self.response = response
        self.exit_code = exit_code

    @property
    def name(self):
        """The status name."""
        return HTTP_STATUS_CODES.get(self.code, 'AFC Engine Error')

    def get_description(self, environ=None):
        """Get the description."""
        return self.description

    def get_body(self, environ=None):
        """Get the HTML body."""
        return flask.json.dumps(dict(
            type='AFC Engine Exception',
            exitCode=self.exit_code,
            description=self.description  # ,
            # env=environ
        ))

    def get_headers(self, environ=None):
        """Get a list of headers."""
        return [('Content-Type', 'application/json')]

    def get_response(self, environ=None):
        """Get a response object.  If one was passed to the exception
        it's returned directly.

        :param environ: the optional environ for the request.  This
                        can be used to modify the response depending
                        on how the request looked like.
        :return: a :class:`Response` object or a subclass thereof.
        """
        if self.response is not None:
            return self.response
        if environ is not None:
            environ = _get_environ(environ)
        headers = self.get_headers(environ)
        return Response(self.get_body(environ), self.code, headers)


class TemporaryDirectory(object):
    """Context manager for temporary directories"""

    def __init__(self, prefix):
        self.prefix = prefix
        self.name = None

    def __enter__(self):
        self.name = tempfile.mkdtemp(prefix=self.prefix)
        return self.name

    def __exit__(self, exc_type, exc_val, trace):
        shutil.rmtree(self.name)


def getQueueDirectory(task_queue_dir, analysis_type):
    ''' creates a unique directory under the task_queue_dir
    and returns the name to the caller. Caller responsible for cleanup.
    '''

    unique = str(uuid.uuid4())
    dir_name = analysis_type + '-' + unique

    full_path = os.path.join(task_queue_dir, dir_name)
    os.mkdir(full_path)
    return full_path


def redirect(location, code):
    ''' Generate a response to redirect.

    :param location: The URI to redirect to.
    :type location: unicode or str
    :param code: The redirect status code.
    :type code: int
    :return: The response object.
    :rtype: :py:cls:`Response`
    '''
    resp = Response(status=code)
    resp.location = location
    return resp


def require_default_uls():
    ''' Copies all default ULS Database files to variable directory as symlinks.
        These will then be accessible through web dav interface
    '''
    # copy default uls database files to var
    for uls_file in os.listdir(flask.current_app.config['DEFAULT_ULS_DIR']):
        if os.path.exists(
            os.path.join(
                flask.current_app.config['NFS_MOUNT_PATH'],
                'rat_transfer',
                'ULS_Database',
                uls_file)):
            continue
        os.symlink(
            os.path.join(
                flask.current_app.config['DEFAULT_ULS_DIR'],
                uls_file),
            os.path.join(
                flask.current_app.config['NFS_MOUNT_PATH'],
                'rat_transfer',
                'ULS_Database',
                uls_file))


class PrefixMiddleware(object):
    ''' Apply an application root path if necessary.
    '''

    def __init__(self, app, prefix=''):
        self.app = app
        self.prefix = prefix

    def __call__(self, environ, start_response):

        # we need to strip prefix from path info before flask can handle it
        if environ['PATH_INFO'].startswith(self.prefix):
            environ['PATH_INFO'] = environ['PATH_INFO'][len(self.prefix):]
            LOGGER.debug('Path: %s', environ['PATH_INFO'])
            environ['SCRIPT_NAME'] = self.prefix
            LOGGER.debug('Script: %s', environ['SCRIPT_NAME'])
            return self.app(environ, start_response)
        else:
            # here the prefix has already been stripped by apache so just set
            # script and pass on
            environ['SCRIPT_NAME'] = self.prefix
            return self.app(environ, start_response)


class HeadersMiddleware(object):
    def __init__(self, app):
        self.app = app

    def __call__(self, environ, start_response):
        def custom_start_response(status, headers, exc_info=None):
            headers.append(('Cache-Control', "max-age=0"))
            return start_response(status, headers, exc_info)
        return self.app(environ, custom_start_response)


def three_way_dict_diff(d_from, d_to):
    """ Recursive dictionary difference
    Returns (added, removed, changed) tuple, containing dictionaries that
    describe change. Leaves of 'added' are new values, leaves of 'removed' are
    old values, leaves of 'changed' are {"from": old_value, "to": new_value}
    tuples
    """
    added = {}
    removed = {}
    changed = {}
    if not (isinstance(d_from, dict) and isinstance(d_to, dict)):
        # This should not be, but still may happen at top level
        changed['uncomparable'] = {'from': d_from, 'to': d_to}
    else:
        # Now the intended case - both entities are dictionaries
        for k, v_from in d_from.items():
            if k in d_to:
                v_to = d_to[k]
                if isinstance(v_from, dict) and isinstance(v_to, dict):
                    for diff_dict, dest_dict in \
                            zip(three_way_dict_diff(v_from, v_to),
                                (added, removed, changed)):
                        if diff_dict:
                            dest_dict[k] = diff_dict
                elif v_from != v_to:
                    changed[k] = {'from': v_from, 'to': v_to}
            else:
                removed[k] = v_from
        for k, v_to in d_to.items():
            if k not in d_from:
                added[k] = v_to
    return (added, removed, changed)


def als_log_afc_config_change(old_config, new_config, region, user, source):
    als_record = \
        {'action':
         ('update' if old_config else 'create') if new_config else 'delete',
         'user': user, 'region': region, 'from': source}
    jsons = {'content': new_config, 'previous': old_config}
    if new_config and old_config:
        jsons.update(
            {k: v for k, v in
             zip(('added_items', 'removed_items', 'changed_items'),
                 three_way_dict_diff(old_config, new_config))})
    for k, v in jsons.items():
        if v:
            als_record[k] = v
    als.als_json_log('afc_config', als_record)
