#!/usr/bin/env python

# Copyright 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Provides HTTP server for file exchange between Celery clients and workers.
"""

import os
import errno
import re
from flask import Flask, request, helpers, abort, make_response
import logging
import shutil
from werkzeug.utils import secure_filename

flask = Flask(__name__)
flask.config.from_pyfile('filestorage_config.py')

if flask.config['LOG_STREAM']:
    logging.basicConfig(stream=flask.config['LOG_STREAM'],
                        level=flask.config['LOG_LEVEL'])
elif flask.config['LOG_FILE']:
    logging.basicConfig(filename=flask.config['LOG_FILE'],
                        level=flask.config['LOG_LEVEL'])
else:
    logging.basicConfig(level=flask.config['LOG_LEVEL'])

loc = {
    "pro": flask.config['PRO_LOCATION'],
    "cfg": flask.config['CFG_LOCATION'],
    "dbg": flask.config['DBG_LOCATION']
}

#pattern = re.compile(flask.config['FILE_LOCATION']+"/"+"^[0-9a-fA-F]{32}/.*")
#def check_path(path):
#    path = os.path.join(flask.config['FILE_LOCATION'], path)
#    path = os.path.abspath(path)
#    return pattern.match(path)

def get_local_path(path):
    prefix = path.split('/')[0]
    if prefix not in loc:
        flask.logger.error('get_local_path: wrong path {}'.format(path))
        abort(403, 'Forbidden')
    path = loc[prefix] + path[len(prefix):]
    return path


def mkdir_local(path):
    flask.logger.debug('mkdir_local path={}'.format(path))
    try:
        os.makedirs(path)
    except OSError as e:
        if e.errno != errno.EEXIST:
            flask.logger.error('mkdir_local: something wrong {}'.format(path))
            raise


@flask.route('/'+'<path:path>', methods=['POST'])
def post(path):
    ''' File upload handler. '''
    flask.logger.debug('post method={}, path={}'.format(request.method, path))
    path = get_local_path(path)

    if 'file' in request.files:
        if not request.files['file']:
            flask.logger.error('No file in request')
            abort(422, 'Unprocessable Entity')
        if request.files['file'].filename == '':
            flask.logger.error('Empty filename')
            abort(422, 'Unprocessable Entity')
        path = os.path.join(path, request.files['file'].filename)
        mkdir_local(os.path.dirname(path))
        request.files['file'].save(path)
        return make_response('OK', 200)
    mkdir_local(os.path.dirname(path))
    with open(path, 'wb') as f:
        f.write(request.get_data())
    flask.logger.debug('POST path {}'.format(path))
    return make_response('OK', 200)


@flask.route('/'+'<path:path>', methods=["DELETE"])
def delete(path):
    ''' File/dir delete handler. '''
    flask.logger.debug('delete method={}, path={}'.format(request.method, path))
    path = get_local_path(path)

    if os.path.exists(path):
        if os.path.isdir(path):
            shutil.rmtree(path)
        else:
            os.remove(path)
        flask.logger.debug('DELETE {}'.format(path))
    return make_response('OK', 204)


@flask.route('/'+'<path:path>', methods=['HEAD'])  # handle URL with filename
def head(path):
    ''' Is file exist handler. '''
    flask.logger.debug('head method={}, path={}'.format(request.method, path))
    path = get_local_path(path)

    if not os.path.exists(path):
        abort(404, 'File not found.')
    return make_response('OK', 200)


@flask.route('/'+'<path:path>', methods=['GET'])  # handle URL with filename
def get(path):
    ''' File download handler. '''
    flask.logger.debug('get method={}, path={}'.format(request.method, path))
    path = get_local_path(path)

    if not os.path.exists(path):
        flask.logger.error('{}: File not found'.format(path))
        abort(404, 'File not found.')
    flask.logger.debug('GET {}'.format(path))
    if os.path.isdir(path):
        return make_response('OK', 204)
    return helpers.send_file(path)


if __name__ == '__main__':
    flask.logger.debug("host={} port={} FILE_LOCATION={}".format(flask.config['SERVER_HOST'], flask.config['SERVER_PORT'], flask.config['FILE_LOCATION']))

    mkdir_local(flask.config['FILE_LOCATION'])
    # production env:
    import waitress
    waitress.serve(flask, host=flask.config['SERVER_HOST'],
          port=flask.config['SERVER_PORT'])
    # Development env:
    #flask.run(host=flask.config['SERVER_HOST'],
    #           port=flask.config['SERVER_PORT'], debug=True)

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
