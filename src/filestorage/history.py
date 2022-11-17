#!/usr/bin/env python3

# Copyright 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Provides HTTP server for getting history.
"""

import os
import errno
import re
import logging
import waitress
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
import filestorage_config

NET_TIMEOUT = 60 # The amount of time, in seconds, to wait for the server response

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

def generateHtml(schema, baseurl, dirs, files):
    flask.logger.debug("generateHtml({}, {}, {})".format(baseurl, dirs, files))
    dirs.sort()
    files.sort()

    html = """<!DOCTYPE html>
<html>
<head>
    <meta content="text/html; charset="utf-8">
</head>
<body>
<h1>Directory listing for """

    burl = ""
    baseSplit = baseurl.split("/")
    if len(baseSplit) >= 4:
        url = schema + "://" + baseSplit[2] + "/dbg"
        flask.logger.debug("url {}".format(url))
        html += "<a href=" + url + ">" + "dbg" + "</a> "
        i = 4
        while i < len(baseSplit):
            url += "/" + baseSplit[i]
            html += " <a href=" + url + ">" + "/" + baseSplit[i] + "</a> "
            i += 1
        burl = url


    html += """</h1><hr>
<ul>
"""

    for e in dirs:
        html += "<li><a href=" + burl + "/" + e + "><b>" + e + """/</b></a></li>
"""
    for e in files:
        html += "<li><a href=" + burl + "/" + e + ">" + e + """</a></li>
"""

    html += """</ul>
<hr>
</body>
</html>
"""

    flask.logger.debug(html)

    return html.encode()


class ObjInt:
    """ Abstract class for data prot operations """
    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def isdir(self):
        pass

    @abc.abstractmethod
    def list(self):
        pass

    @abc.abstractmethod
    def read(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        pass


class ObjIntLocalFS(ObjInt):
    def __init__(self, file_name):
        self.__file_name = file_name

    def isdir(self):
        return os.path.isdir(self.__file_name)

    def list(self):
        flask.logger.debug("ObjIntLocalFS.list")
        ls = os.listdir(self.__file_name)
        files = [f for f in ls if os.path.isfile(os.path.join(self.__file_name, f))]
        dirs = [f for f in ls if os.path.isdir(os.path.join(self.__file_name, f))]
        return dirs, files

    def read(self):
        flask.logger.debug("ObjIntLocalFS.read({})".format(self.__file_name))
        if os.path.isfile(self.__file_name):
            with open(self.__file_name, "rb") as hfile:
                return hfile.read()
        return None


class ObjIntGoogleCloudBucket(ObjInt):
    def __init__(self, file_name):
        self.__file_name = file_name
        self.__blob = bucket.blob(self.__file_name)

    def isdir(self):
        return not self.__blob.exists()

    def list(self):
        flask.logger.debug("ObjIntGoogleCloudBucket.list")
        blobs = bucket.list_blobs(prefix=self.__file_name + "/")
        files = []
        dirs = set()
        for blob in blobs:
            name = blob.name.removeprefix(self.__file_name + "/")
            if name.count("/"):
                dirs.add(name.split("/")[0])
            else:
                files.append(name)
        return list(dirs), files

    def read(self):
        blob = bucket.blob(self.__file_name)
        return blob.download_as_bytes(raw_download=True,
                                      timeout=NET_TIMEOUT)


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
    schema = "http"
    if prefix == "dbgs":
        schema = "https"
    elif prefix != "dbg":
        flask.logger.error('get_local_path: wrong path {}'.format(path))
        abort(403, 'Forbidden')
    path = flask.config["DBG_LOCATION"] + path[len(prefix):]
    flask.logger.debug("get_local_path() {}".format(path))
    return path, schema


@flask.route('/'+'<path:path>', methods=['GET'])
def get(path):
    ''' File download handler. '''
    flask.logger.debug('get method={}, path={}'.format(request.method, path))
    path, schema = get_local_path(path)

    try:
        objst = Objstorage()
        with objst.open(path) as hobj:
            if hobj.isdir() is True:
                dirs, files = hobj.list()
                return generateHtml(schema, request.base_url, dirs, files)
            else:
                data = hobj.read()
                return helpers.send_file(
                    io.BytesIO(data),
                    download_name=os.path.basename(path))
    except Exception as e:
        flask.logger.error(e)
        return abort(500)


if __name__ == '__main__':
    os.makedirs(flask.config["DBG_LOCATION"], exist_ok=True)
    waitress.serve(flask, host=flask.config["HISTORY_HOST"],
          port=flask.config["HISTORY_PORT"])

    #flask.run(host=flask.config['HISTORY_HOST'],
    #           port=flask.config['HISTORY_PORT'], debug=True)



