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

def generateHtml(baseurl, dirs, files):
    flask.logger.debug("generateHtml({}, {}, {})".format(baseurl, dirs, files))
    dirs.sort()
    files.sort()

    html = """<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
</head>
<body>
<h1>"""

    baseSplit = baseurl.split("/")
    if len(baseSplit) >= 4:
        i = 3
        url = baseSplit[0] + "/" + baseSplit[1] + "/" + baseSplit[2]
        while i < len(baseSplit):
            url += "/" + baseSplit[i]
            html += " <a href=" + url + ">" + "/" + baseSplit[i] + "</a> "
            i+=1
    else:
        baseSplit = ""


    html += """</h1>
<table>
"""

    for e in dirs:
        html += "<tr><td><a href=" + baseurl + "/" + e + "><b>" + e + """</b></a></td></tr>
"""
    for e in files:
        html += "<tr><td><a href=" + baseurl + "/" + e + ">" + e + """</a></td></tr>
"""

    html += """</table>
</body>
</html>
"""

    flask.logger.debug(html)

    return html.encode()


class ObjInt:
    """ Abstract class for data prot operations """
    __metaclass__ = abc.ABCMeta

    def __init__(self, file_name):
        self._file_name = file_name

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
    def isdir(self):
        return os.path.isdir(self._file_name)

    def list(self):
        flask.logger.debug("ObjIntLocalFS.list")
        ls = os.listdir(self._file_name)
        files = [f for f in ls if os.path.isfile(os.path.join(self._file_name, f))]
        dirs = [f for f in ls if os.path.isdir(os.path.join(self._file_name, f))]
        return dirs, files

    def read(self):
        flask.logger.debug("ObjIntLocalFS.read({})".format(self._file_name))
        if os.path.isfile(self._file_name):
            with open(self._file_name, "rb") as hfile:
                return hfile.read()
        return None


class ObjIntGoogleCloudBucket(ObjInt):
    def read(self):
        blob = bucket.blob(self._file_name)
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
    if prefix != "dbg":
        flask.logger.error('get_local_path: wrong path {}'.format(path))
        abort(403, 'Forbidden')
    path = flask.config["DBG_LOCATION"] + path[len(prefix):]
    flask.logger.debug("get_local_path() {}".format(path))
    return path


@flask.route('/'+'<path:path>', methods=['GET'])
def get(path):
    ''' File download handler. '''
    flask.logger.debug('get method={}, path={}'.format(request.method, path))
    path = get_local_path(path)

    try:
        objst = Objstorage()
        with objst.open(path) as hobj:
            if hobj.isdir() is True:
                dirs, files = hobj.list()
                return generateHtml(request.base_url, dirs, files)
            else:
                data = hobj.read()
                if data:
                    flask.logger.debug("get data={}".format(data))
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
    os.makedirs(flask.config["DBG_LOCATION"], exist_ok=True)

    #waitress.serve(flask, host=flask.config["HISTORY_HOST"],
    #      port=flask.config["HISTORY_PORT"])

    flask.run(host=flask.config['HISTORY_HOST'],
               port=flask.config['HISTORY_PORT'], debug=True)



