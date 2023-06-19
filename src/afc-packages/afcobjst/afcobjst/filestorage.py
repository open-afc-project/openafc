#!/usr/bin/env python3

# Copyright 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Provides HTTP server for file exchange between Celery clients and workers.
"""

import os
import logging
import shutil
import abc
import waitress
from flask import Flask, request, abort, make_response
import google.cloud.storage
from .objstconf import ObjstConfigInternal

NET_TIMEOUT = 600 # The amount of time, in seconds, to wait for the server response

objst_app = Flask(__name__)
objst_app.config.from_object(ObjstConfigInternal())

if objst_app.config['AFC_OBJST_LOG_FILE']:
    logging.basicConfig(filename=objst_app.config['AFC_OBJST_LOG_FILE'],
                        level=objst_app.config['AFC_OBJST_LOG_LVL'])
else:
    logging.basicConfig(level=objst_app.config['AFC_OBJST_LOG_LVL'])

if objst_app.config["AFC_OBJST_MEDIA"] == "GoogleCloudBucket":
    os.environ["GOOGLE_APPLICATION_CREDENTIALS"] = objst_app.config["AFC_OBJST_GOOGLE_CLOUD_CREDENTIALS_JSON"]
    client = google.cloud.storage.client.Client()
    bucket = client.bucket(objst_app.config["AFC_OBJST_GOOGLE_CLOUD_BUCKET"])

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
        with open(self._file_name, "rb") as hfile:
            return hfile.read()

    def head(self):
        return os.path.exists(self._file_name)

    def delete(self):
        if os.path.exists(self._file_name):
            if os.path.isdir(self._file_name):
                shutil.rmtree(self._file_name)
            else:
                os.remove(self._file_name)

    def __mkdir_local(self, path):
        objst_app.logger.debug('__mkdir_local({})'.format(path))
        os.makedirs(path, exist_ok=True)


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
        objst_app.logger.debug("Objstorage.open({})".format(name))
        if objst_app.config["AFC_OBJST_MEDIA"] == "GoogleCloudBucket":
            return ObjIntGoogleCloudBucket(name)
        if objst_app.config["AFC_OBJST_MEDIA"] == "LocalFS":
            return ObjIntLocalFS(name)


def get_local_path(path):
    path = os.path.join(objst_app.config["AFC_OBJST_FILE_LOCATION"], path)
    return path


@objst_app.route('/'+'<path:path>', methods=['POST'])
def post(path):
    ''' File upload handler. '''
    try:
        objst_app.logger.debug('post method={}, path={}'.format(request.method, path))
        path = get_local_path(path)

        data = None

        if 'file' in request.files:
            if not request.files['file']:
                objst_app.logger.error('No file in request')
                abort(400)
            if request.files['file'].filename == '':
                objst_app.logger.error('Empty filename')
                abort(400)
            path = os.path.join(path, request.files['file'].filename)
            data = request.files['file'].read()
        else:
            data = request.get_data()

        objst = Objstorage()
        with objst.open(path) as hobj:
            hobj.write(data)
    except Exception as e:
        objst_app.logger.error(e)
        return abort(500)

    return make_response('OK', 200)


@objst_app.route('/'+'<path:path>', methods=["DELETE"])
def delete(path):
    ''' File/dir delete handler. '''
    objst_app.logger.debug('delete method={}, path={}'.format(request.method, path))
    path = get_local_path(path)

    try:
        objst = Objstorage()
        with objst.open(path) as hobj:
            hobj.delete()
    except Exception as e:
        objst_app.logger.error(e)
        return make_response('File not found', 404)

    return make_response('OK', 204)


@objst_app.route('/', defaults={'path': ''}, methods=['HEAD'])
@objst_app.route('/'+'<path:path>', methods=['HEAD'])  # handle URL with filename
def head(path):
    ''' Is file exist handler. '''
    objst_app.logger.debug('head method={}, path={}'.format(request.method, path))
    path = get_local_path(path)

    try:
        objst = Objstorage()
        with objst.open(path) as hobj:
            if hobj.head():
                return make_response('OK', 200)
            else:
                return make_response('File not found', 404)
    except Exception as e:
        objst_app.logger.error(e)
        return abort(500)


@objst_app.route('/'+'<path:path>', methods=['GET'])  # handle URL with filename
def get(path):
    ''' File download handler. '''
    objst_app.logger.debug('get method={}, path={}'.format(request.method, path))
    path = get_local_path(path)

    try:
        objst = Objstorage()
        with objst.open(path) as hobj:
            data = hobj.read()
            if data:
                return data
            objst_app.logger.error('{}: File not found'.format(path))
            return make_response('File not found', 404)
    except Exception as e:
        objst_app.logger.error(e)
        return abort(500)


if __name__ == '__main__':
    objst_app.logger.debug("port={} AFC_OBJST_FILE_LOCATION={} AFC_OBJST_MEDIA={}".
                       format(objst_app.config['AFC_OBJST_PORT'],
                              objst_app.config['AFC_OBJST_FILE_LOCATION'],
                              objst_app.config["AFC_OBJST_MEDIA"]))
    os.makedirs(objst_app.config['AFC_OBJST_FILE_LOCATION'], exist_ok=True)
    # production env:
    waitress.serve(objst_app, port=objst_app.config['AFC_OBJST_PORT'], host="0.0.0.0")
    # Development env:
    #objst_app.run(port=objst_app.config['AFC_OBJST_PORT'], host="0.0.0.0", debug=True)

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
