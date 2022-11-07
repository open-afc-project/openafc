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
import io
from flask import Flask, request, helpers, abort, make_response
import logging
import shutil
from werkzeug.utils import secure_filename
import abc
import google.cloud.storage

NET_TIMEOUT = 600 # The amount of time, in seconds, to wait for the server response

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

if flask.config["OBJSTORAGE"] == "GoogleCloudBucket":
    os.environ["GOOGLE_APPLICATION_CREDENTIALS"] = flask.config["GOOGLE_CLOUD_CREDENTIALS_JSON"]
    client = google.cloud.storage.client.Client()
    bucket = client.bucket(flask.config["GOOGLE_CLOUD_BUCKET"])

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

class ObjInt:
    """ Abstract class for data prot operations """
    __metaclass__ = abc.ABCMeta

    def __init__(self, file_name):
        self._file_name = file_name

    @abc.abstractmethod
    def write(self, data):
        pass

    @abc.abstractmethod
    def read(self):
        pass

    @abc.abstractmethod
    def head(self):
        pass

    @abc.abstractmethod
    def delete(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        pass


class ObjIntLocalFS(ObjInt):
    def write(self, data):
        self.__mkdir_local(os.path.dirname(self._file_name))
        with open(self._file_name, 'wb') as f:
            f.write(data)

    def read(self):
        if not os.path.isfile(self._file_name):
            return None
        with open(self._file_name) as hfile:
            return bytes(hfile.read(),"utf-8")

    def head(self):
        return os.path.exists(self._file_name)

    def delete(self):
        if os.path.exists(self._file_name):
            if os.path.isdir(self._file_name):
                shutil.rmtree(self._file_name)
            else:
                os.remove(self._file_name)

    def __mkdir_local(self, path):
        flask.logger.debug('__mkdir_local({})'.format(path))
        try:
            os.makedirs(path)
        except OSError as e:
            if e.errno != errno.EEXIST:
                flask.logger.error('mkdir_local: something wrong {}'.format(path))
                raise


class ObjIntGoogleCloudBucket(ObjInt):
    def write(self, data):
        blob = bucket.blob(self._file_name)
        blob.upload_from_string(data,
                                content_type="application/octet-stream",
                                timeout=NET_TIMEOUT)

    def read(self):
        blob = bucket.blob(self._file_name)
        return blob.download_as_bytes(raw_download=True,
                                      timeout=NET_TIMEOUT)

    def head(self):
        blobs = client.list_blobs(bucket,
                                  prefix=self._file_name,
                                  delimeter = "/",
                                  timeout=NET_TIMEOUT)
        return blobs is not None

    def delete(self):
        blobs = client.list_blobs(bucket,
                                  prefix=self._file_name,
                                  delimeter = "/",
                                  timeout=NET_TIMEOUT)
        for blob in blobs:
            try:
                blob.delete(timeout=NET_TIMEOUT)
            except:
                pass # ignore google.cloud.exceptions.NotFound


class Objstorage:
    def open(self, name):
        """ Create ObjInt instance """
        flask.logger.debug("Objstorage.open({})".format(name))
        if flask.config["OBJSTORAGE"] == "GoogleCloudBucket":
            return ObjIntGoogleCloudBucket(name)
        if flask.config["OBJSTORAGE"] == "LocalFS":
            return ObjIntLocalFS(name)
        raise Exception("Unsupported OBJSTORAGE \"{}\"".format(flask.config["OBJSTORAGE"]))


def get_local_path(path):
    prefix = path.split('/')[0]
    if prefix not in loc:
        flask.logger.error('get_local_path: wrong path {}'.format(path))
        abort(403, 'Forbidden')
    path = loc[prefix] + path[len(prefix):]
    return path


@flask.route('/'+'<path:path>', methods=['POST'])
def post(path):
    ''' File upload handler. '''
    try:
        flask.logger.debug('post method={}, path={}'.format(request.method, path))
        path = get_local_path(path)

        data = None

        if 'file' in request.files:
            if not request.files['file']:
                flask.logger.error('No file in request')
                abort(400)
            if request.files['file'].filename == '':
                flask.logger.error('Empty filename')
                abort(400)
            path = os.path.join(path, request.files['file'].filename)
            data = request.files['file'].read()
        else:
            data = request.get_data()

        objst = Objstorage()
        with objst.open(path) as hobj:
            hobj.write(data)
    except Exception as e:
        flask.logger.error(e)
        return abort(500)

    return make_response('OK', 200)


@flask.route('/'+'<path:path>', methods=["DELETE"])
def delete(path):
    ''' File/dir delete handler. '''
    flask.logger.debug('delete method={}, path={}'.format(request.method, path))
    path = get_local_path(path)

    try:
        objst = Objstorage()
        with objst.open(path) as hobj:
            hobj.delete()
    except Exception as e:
        flask.logger.error(e)
        return abort(500)

    return make_response('OK', 204)


@flask.route('/'+'<path:path>', methods=['HEAD'])  # handle URL with filename
def head(path):
    ''' Is file exist handler. '''
    flask.logger.debug('head method={}, path={}'.format(request.method, path))
    path = get_local_path(path)

    try:
        objst = Objstorage()
        with objst.open(path) as hobj:
            if hobj.head():
                return make_response('OK', 200)
            else:
                abort(404)
    except Exception as e:
        flask.logger.error(e)
        return abort(500)


@flask.route('/'+'<path:path>', methods=['GET'])  # handle URL with filename
def get(path):
    ''' File download handler. '''
    flask.logger.debug('get method={}, path={}'.format(request.method, path))
    path = get_local_path(path)

    try:
        objst = Objstorage()
        with objst.open(path) as hobj:
            data = hobj.read()
            if data:
                return helpers.send_file(
                    io.BytesIO(data),
                    mimetype='application/octet-stream')
            else:
                flask.logger.error('{}: File not found'.format(path))
                abort(404)
    except Exception as e:
        flask.logger.error(e)
        return abort(500)


if __name__ == '__main__':
    flask.logger.debug("host={} port={} FILE_LOCATION={} OBJSTORAGE={}".format(flask.config['SERVER_HOST'], flask.config['SERVER_PORT'], flask.config['FILE_LOCATION'], flask.config["OBJSTORAGE"]))

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
