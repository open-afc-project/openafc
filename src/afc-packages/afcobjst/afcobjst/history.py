#!/usr/bin/env python3

# Copyright 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Provides HTTP server for getting history.
"""

import urllib.parse
from markupsafe import escape
import markupsafe
import hmac
import os
import logging
import io
import abc
import waitress
from flask import Flask, request, helpers, abort
import google.cloud.storage
from .objstconf import ObjstConfigInternal

NET_TIMEOUT = 60  # The amount of time, in seconds, to wait for the server response

hist_app = Flask(__name__)
hist_app.config.from_object(ObjstConfigInternal())


def _load_objst_api_key():
    """ Return objst/hist API key from file (AFC_OBJST_API_KEY_FILE) or None. """
    key_file = os.environ.get("AFC_OBJST_API_KEY_FILE")
    if key_file and os.path.isfile(key_file):
        with open(key_file) as f:
            return f.read().strip() or None
    return None


@hist_app.before_request
def _require_hist_bearer_token():
    """ Require a bearer token on every history route.

    The sibling objst_app gates its routes the same way; hist_app previously
    had no authentication, exposing the AFC history directory unauthenticated
    to any host that could reach the container.
    """
    if request.endpoint == "healthcheck":
        return
    expected = _load_objst_api_key()
    if expected is None:
        hist_app.logger.error(
            "hist: AFC_OBJST_API_KEY_FILE not configured — "
            "all requests rejected")
        abort(503)
    auth_header = request.headers.get("Authorization", "")
    if not auth_header.startswith("Bearer "):
        abort(401)
    supplied = auth_header[len("Bearer "):]
    if not hmac.compare_digest(supplied.encode(), expected.encode()):
        abort(403)


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
    if path:  # Simplifies `is not None and path != ""`
        vpath += "/" + path

    html = """<!DOCTYPE html>
<html>
<head>
    <meta content="text/html; charset=utf-8">
</head>
<body>
<h1>Directory listing for """

    path_split = vpath.split("/")

    # Strip trailing slash once to avoid index checking later
    url = rurl.rstrip("/")

    for i, directory in enumerate(path_split):
        if i != 0:
            url += "/" + directory
        # Escape exactly when injecting into the HTML template using f-strings
        html += f' <a href="{escape(url)}">/{escape(directory)}</a> '

    html += "</h1><hr>\n<ul>\n"

    for d in dirs:
        # Build the raw href path
        href = "/".join(s for s in (rurl.rstrip("/"), path, d) if s)
        # Inject and escape
        html += f'<li><a href="{escape(href)}"><b>{escape(d)}/</b></a></li>\n'

    for f in files:
        href = "/".join(s for s in (rurl.rstrip("/"), path, f) if s)
        html += f'<li><a href="{escape(href)}">{escape(f)}</a></li>\n'

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
        pass  # No resources to clean up in base class


class ObjIntLocalFS(ObjInt):
    def __init__(self, file_name):
        self.__file_name = file_name

    def isdir(self):
        return os.path.isdir(self.__file_name)

    def list(self):
        hist_app.logger.debug("ObjIntLocalFS.list")
        ls = os.listdir(self.__file_name)
        files = [f for f in ls if os.path.isfile(
            os.path.join(self.__file_name, f))]
        dirs = [f for f in ls if os.path.isdir(
            os.path.join(self.__file_name, f))]
        return dirs, files

    def read(self):
        hist_app.logger.debug(
            "ObjIntLocalFS.read({})".format(self.__file_name))
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
        raise RuntimeError("Unsupported AFC_OBJST_MEDIA \"{}\"".
                           format(hist_app.config["AFC_OBJST_MEDIA"]))


@hist_app.route('/', defaults={'path': ""}, methods=['GET'])
@hist_app.route('/' + '<path:path>', methods=['GET'])
def get(path):
    ''' File download handler. '''
    # ratapi URL preffix
    rurl = request.args["url"]
    # Validate scheme to block non-HTTP(S) URI schemes, and reject any URL that
    # carries a network location (host) or is protocol-relative ('//host'): the
    # link base must stay same-origin so generateHtml() never emits off-origin
    # navigation hrefs.
    import urllib.parse as _urlparse
    _parsed_rurl = _urlparse.urlparse(rurl)
    if _parsed_rurl.scheme not in ('http', 'https', ''):
        return abort(400)
    if rurl.startswith('//') or _parsed_rurl.netloc:
        # Only same-origin is permitted; the proxy supplies request.base_url.
        # A client-supplied off-origin base is rejected.
        if _parsed_rurl.netloc != request.host:
            return abort(400)
    fwd_proto = request.headers.get('X-Forwarded-Proto')
    if (fwd_proto == 'https') and (request.scheme == "http"):
        rurl = rurl.replace("http:", "https:")
    hist_app.logger.debug(
        f'get method={request.method}, path={path} url={rurl}')
    # local path in the storage
    hroot = os.path.realpath(os.path.join(
        hist_app.config["AFC_OBJST_FILE_LOCATION"], "history"))
    lpath = os.path.realpath(os.path.join(hroot, path))
    # This uses os.path.commonpath instead of manual os.sep checks
    if lpath != hroot and os.path.commonpath([lpath, hroot]) != hroot:
        hist_app.logger.error(
            "get invalid path rejected: {}".format(path))
        return abort(400)

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
    os.makedirs(os.path.join(
        hist_app.config["AFC_OBJST_FILE_LOCATION"], "history"), exist_ok=True)
    waitress.serve(
        hist_app, port=hist_app.config["AFC_OBJST_HIST_PORT"], host="127.0.0.1")

    # hist_app.run(port=hist_app.config['AFC_OBJST_HIST_PORT'], host="0.0.0.0", debug=True)
