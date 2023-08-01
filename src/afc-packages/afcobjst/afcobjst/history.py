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
import logging
import io
import abc
import waitress
from flask import Flask, request, helpers, abort
import google.cloud.storage
from .objstconf import ObjstConfigInternal

NET_TIMEOUT = 60 # The amount of time, in seconds, to wait for the server response

hist_app = Flask(__name__)
hist_app.config.from_object(ObjstConfigInternal())

if hist_app.config['AFC_OBJST_LOG_FILE']:
    logging.basicConfig(filename=hist_app.config['AFC_OBJST_LOG_FILE'],
                        level=hist_app.config['AFC_OBJST_LOG_LVL'])
else:
    logging.basicConfig(level=hist_app.config['AFC_OBJST_LOG_LVL'])

if hist_app.config["AFC_OBJST_MEDIA"] == "GoogleCloudBucket":
    os.environ["GOOGLE_APPLICATION_CREDENTIALS"] = hist_app.config["AFC_OBJST_GOOGLE_CLOUD_CREDENTIALS_JSON"]
    client = google.cloud.storage.client.Client()
    bucket = client.bucket(hist_app.config["AFC_OBJST_GOOGLE_CLOUD_BUCKET"])

def generateHtml(rurl, path, dirs, files):
    hist_app.logger.debug(f"generateHtml({rurl}, {path}, {dirs}, {files})")
    dirs.sort()
    files.sort()
    vpath = "history"
    if path is not None and path != "":
        vpath += "/" + path

    html = """<!DOCTYPE html>
<html>
<head>
    <meta content="text/html; charset="utf-8">
</head>
<body>
<h1>Directory listing for """

    path_split = vpath.split("/")
    url = rurl
    if url[len(url)-1] == "/":
        url = url[:len(url)-1]
    i = 0
    for dir in path_split:
        if i != 0:
            url += "/" + dir
        html += " <a href=" + url + ">" + "/" + dir + "</a> "
        i = i + 1

    html += """</h1><hr>
<ul>
"""

    for e in dirs:
        html += "<li><a href=" + rurl + "/" + path + "/" + e + "><b>" + e + """/</b></a></li>
"""
    for e in files:
        html += "<li><a href=" + rurl + "/" + path + "/" + e + ">" + e + """</a></li>
"""

    html += """</ul>
<hr>
</body>
</html>
"""

    hist_app.logger.debug(html)

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
        hist_app.logger.debug("ObjIntLocalFS.list")
        ls = os.listdir(self.__file_name)
        files = [f for f in ls if os.path.isfile(os.path.join(self.__file_name, f))]
        dirs = [f for f in ls if os.path.isdir(os.path.join(self.__file_name, f))]
        return dirs, files

    def read(self):
        hist_app.logger.debug("ObjIntLocalFS.read({})".format(self.__file_name))
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
        hist_app.logger.debug("ObjIntGoogleCloudBucket.list")
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
        hist_app.logger.debug("Objstorage.open({})".format(name))
        if hist_app.config["AFC_OBJST_MEDIA"] == "GoogleCloudBucket":
            return ObjIntGoogleCloudBucket(name)
        if hist_app.config["AFC_OBJST_MEDIA"] == "LocalFS":
            return ObjIntLocalFS(name)
        raise Exception("Unsupported AFC_OBJST_MEDIA \"{}\"".
                        format(hist_app.config["AFC_OBJST_MEDIA"]))


@hist_app.route('/', defaults={'path': ""}, methods=['GET'])
@hist_app.route('/'+'<path:path>', methods=['GET'])
def get(path):
    ''' File download handler. '''
    # ratapi URL preffix
    rurl = request.args["url"]
    hist_app.logger.debug(f'get method={request.method}, path={path} url={rurl}')
    # local path in the storage
    lpath = os.path.join(hist_app.config["AFC_OBJST_FILE_LOCATION"], "history", path)

    try:
        objst = Objstorage()
        with objst.open(lpath) as hobj:
            if hobj.isdir() is True:
                dirs, files = hobj.list()
                return generateHtml(rurl, path, dirs, files)
            data = hobj.read()
            return helpers.send_file(
                io.BytesIO(data),
                download_name=os.path.basename(path))
    except Exception as e:
        hist_app.logger.error(e)
        return abort(500)


if __name__ == '__main__':
    os.makedirs(os.path.join(hist_app.config["AFC_OBJST_FILE_LOCATION"], "history"), exist_ok=True)
    waitress.serve(hist_app, port=hist_app.config["AFC_OBJST_HIST_PORT"], host="0.0.0.0")

    #hist_app.run(port=hist_app.config['AFC_OBJST_HIST_PORT'], host="0.0.0.0", debug=True)

