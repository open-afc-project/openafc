#!/usr/bin/env python3

# Copyright 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Provides HTTP server for file exchange between Celery clients and workers.
"""

import hashlib
import hmac
import os
import logging
import shutil
import socket
import tempfile
import abc
import fcntl
import tempfile
import time
from flask import Flask, request, abort, make_response
from werkzeug.utils import secure_filename
from .objstconf import ObjstConfigInternal

NET_TIMEOUT = 600  # The amount of time, in seconds, to wait for the server response
SEM_TIMEOUT = 60  # Per file semaphore timeout

objst_app = Flask(__name__)
objst_app.config.from_object(ObjstConfigInternal())


def _derive_objst_bearer_token(key):
    """ Purpose-bound objstore bearer token:
    HMAC-SHA256(key, 'afc-objst-bearer-v1').

    Clients (fstorage fst.py, ratapi History) send this subkey instead of
    the raw AFC_OBJST_API_KEY, so a bearer-header observer on plaintext
    objstore HTTP traffic cannot recompute the RMQ response-signing key
    (rcache_models._derive_rmq_resp_hmac_key), which is derived from the
    same raw secret under a different label.
    """
    return hmac.new(key.encode("utf-8"), b"afc-objst-bearer-v1",
                    hashlib.sha256).hexdigest()


def _load_objst_api_key():
    """ Return the expected objst bearer token (derived from
    AFC_OBJST_API_KEY_FILE) or None. """
    key_file = os.environ.get("AFC_OBJST_API_KEY_FILE")
    if key_file and os.path.isfile(key_file):
        with open(key_file) as f:
            key = f.read().strip()
        return _derive_objst_bearer_token(key) if key else None
    return None


@objst_app.before_request
def _require_objst_bearer_token():
    """ Require bearer token on all routes except the healthcheck. """
    if request.endpoint == "healthcheck":
        return
    expected = _load_objst_api_key()
    if expected is None:
        objst_app.logger.error(
            "objst: AFC_OBJST_API_KEY_FILE not configured — "
            "all requests rejected")
        abort(503)
    auth_header = request.headers.get("Authorization", "")
    if not auth_header.startswith("Bearer "):
        abort(401)
    supplied = auth_header[len("Bearer "):]
    if not hmac.compare_digest(supplied.encode(), expected.encode()):
        abort(403)


if objst_app.config['AFC_OBJST_LOG_FILE']:
    logging.basicConfig(filename=objst_app.config['AFC_OBJST_LOG_FILE'],
                        level=objst_app.config['AFC_OBJST_LOG_LVL'])
else:
    logging.basicConfig(level=objst_app.config['AFC_OBJST_LOG_LVL'])

if objst_app.config["AFC_OBJST_MEDIA"] == "GoogleCloudBucket":
    import google.cloud.storage
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
        pass  # No resources to clean up in base class


class ObjIntLocalFS(ObjInt):
    # Fixed pool size for per-slot lock files. Distinct paths may map to
    # the same lock (a collision merely serializes unrelated operations),
    # but the number of lock files is hard-bounded. flock locks — unlike
    # the POSIX named semaphores used previously — are released by the
    # kernel when the holder's descriptor closes, INCLUDING when the
    # holding worker is SIGKILLed/OOM-killed mid-critical-section, so a
    # crashed worker can never leave a slot permanently wedged.
    SEM_POOL_SIZE = 256
    LOCK_DIR = os.path.join(tempfile.gettempdir(), "afc_objst_locks")

    def __lock(self, name):
        # Hash the path onto the fixed-size lock-file pool. Callers acquire
        # exactly one pool lock at a time, so collisions cannot
        # deadlock — they only serialize unrelated paths against each other.
        slot = int.from_bytes(
            hashlib.sha256(name.encode()).digest()[:8], "big") \
            % self.SEM_POOL_SIZE
        os.makedirs(self.LOCK_DIR, exist_ok=True)
        lock_path = os.path.join(self.LOCK_DIR, "afc_%03d.lock" % slot)
        fd = os.open(lock_path, os.O_CREAT | os.O_RDWR, 0o600)
        deadline = time.monotonic() + SEM_TIMEOUT
        while True:
            try:
                # Non-blocking attempt + short (gevent-patchable) sleep:
                # a blocking flock is a native call that would stall the
                # whole event loop of a gevent worker for up to the full
                # timeout, freezing its unrelated in-flight requests.
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                return fd
            except (BlockingIOError, InterruptedError):
                if time.monotonic() >= deadline:
                    os.close(fd)
                    raise TimeoutError(
                        "objst lock slot %03d busy for %d s" %
                        (slot, SEM_TIMEOUT))
                time.sleep(0.05)

    def __unlock(self, fd):
        # The kernel drops the flock when the descriptor is closed, even
        # if this line is never reached because the process died — no
        # persistent /dev/shm object, no owner-death wedge, nothing to
        # unlink or re-initialise on startup.
        fcntl.flock(fd, fcntl.LOCK_UN)
        os.close(fd)

    def write(self, data):
        self.__mkdir_local(os.path.dirname(self._file_name))
        sem = self.__lock(self._file_name)
        try:
            with open(self._file_name, 'wb') as f:
                if hasattr(data, "read"):
                    shutil.copyfileobj(data, f)
                else:
                    f.write(data)
        finally:
            self.__unlock(sem)

    def read(self):
        if not os.path.isfile(self._file_name):
            return None
        sem = self.__lock(self._file_name)
        try:
            with open(self._file_name, "rb") as hfile:
                ret = hfile.read()
        finally:
            self.__unlock(sem)
        return ret

    def head(self):
        sem = self.__lock(self._file_name)
        try:
            ret = os.path.exists(self._file_name)
        finally:
            self.__unlock(sem)
        return ret

    def delete(self):
        """ During recursive dir delete only the dir is protected by semaphore from
        parallel use. Files in the dir arn't protected. """
        root = os.path.realpath(objst_app.config["AFC_OBJST_FILE_LOCATION"])
        full_path = os.path.realpath(self._file_name)
        if os.path.commonpath([root, full_path]) != root:
            return
        if os.path.exists(self._file_name):
            sem = self.__lock(self._file_name)
            try:
                if os.path.isdir(self._file_name):
                    shutil.rmtree(self._file_name)
                else:
                    os.remove(self._file_name)
            finally:
                self.__unlock(sem)
        # Cascade-delete the '<name>.hmac' signed sidecar some writers
        # (e.g. MTLS._rebuild_cert_bundle's non-empty branch) pair with the
        # main artifact. Deleting only the main file and leaving a
        # still-HMAC-valid sidecar for the old content lets a later
        # re-upload of the same bytes pass the sidecar's signature check —
        # keep the write/delete artifact sets symmetric at the storage
        # boundary regardless of which consumer issued the delete.
        sidecar = self._file_name + ".hmac"
        sidecar_full = os.path.realpath(sidecar)
        if os.path.commonpath([root, sidecar_full]) == root and \
                os.path.isfile(sidecar):
            sem = self.__lock(sidecar)
            try:
                os.remove(sidecar)
            finally:
                self.__unlock(sem)

    def __mkdir_local(self, path):
        os.makedirs(path, exist_ok=True)


class ObjIntGoogleCloudBucket(ObjInt):
    def write(self, data):
        blob = bucket.blob(self._file_name)
        if hasattr(data, "read"):
            blob.upload_from_file(data,
                                  content_type="application/octet-stream",
                                  timeout=NET_TIMEOUT)
        else:
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
                                  delimeter="/",
                                  timeout=NET_TIMEOUT)
        return blobs is not None

    def delete(self):
        blobs = client.list_blobs(bucket,
                                  prefix=self._file_name,
                                  delimeter="/",
                                  timeout=NET_TIMEOUT)
        for blob in blobs:
            try:
                blob.delete(timeout=NET_TIMEOUT)
            except Exception:
                pass  # ignore google.cloud.exceptions.NotFound
        # Cascade-delete the '<name>.hmac' signed sidecar (see
        # ObjIntLocalFS.delete for rationale) so a delete on this backend
        # cannot leave a still-HMAC-valid sidecar for stale content.
        try:
            bucket.blob(self._file_name + ".hmac").delete(timeout=NET_TIMEOUT)
        except Exception:
            pass  # ignore google.cloud.exceptions.NotFound


class Objstorage:
    def open(self, name):
        """ Create ObjInt instance """
        if objst_app.config["AFC_OBJST_MEDIA"] == "GoogleCloudBucket":
            return ObjIntGoogleCloudBucket(name)
        if objst_app.config["AFC_OBJST_MEDIA"] == "LocalFS":
            return ObjIntLocalFS(name)


def get_local_path(path):
    # Reject any '..' path segment up front so callers cannot rely on
    # realpath collapsing dot-segments to land elsewhere under the root.
    if ".." in path.split("/"):
        abort(400)
    root = os.path.realpath(objst_app.config["AFC_OBJST_FILE_LOCATION"])
    full_path = os.path.realpath(os.path.join(root, path))
    if os.path.commonpath([root, full_path]) != root:
        abort(400)
    return full_path


@objst_app.route('/' + '<path:path>', methods=['POST'])
def post(path):
    ''' File upload handler. '''
    objst_app.logger.debug(f'post {path}')
    try:
        path = get_local_path(path)

        data = None

        if 'file' in request.files:
            if not request.files['file']:
                objst_app.logger.error('No file in request')
                abort(400)
            if request.files['file'].filename == '':
                objst_app.logger.error('Empty filename')
                abort(400)
            filename = secure_filename(request.files['file'].filename)
            if not filename:
                objst_app.logger.error('Invalid filename')
                abort(400)
            path = os.path.join(path, filename)
            root = os.path.realpath(objst_app.config["AFC_OBJST_FILE_LOCATION"])
            full_path = os.path.realpath(path)
            if os.path.commonpath([root, full_path]) != root:
                objst_app.logger.error('Invalid path')
                abort(400)
            path = full_path
            # werkzeug already parsed (and disk-spooled) the multipart body
            # under MAX_CONTENT_LENGTH - hand the spooled stream to the
            # backend instead of materialising the file in memory
            data = request.files['file'].stream
        else:
            # Stream the raw body to a disk-backed spool in bounded chunks
            # instead of buffering the attacker-chosen body in worker
            # memory. The running-size cap also covers chunked bodies that
            # carry no Content-Length header.
            max_len = objst_app.config.get("MAX_CONTENT_LENGTH")
            data = tempfile.SpooledTemporaryFile(max_size=1024 * 1024)
            received = 0
            while True:
                chunk = request.stream.read(64 * 1024)
                if not chunk:
                    break
                received += len(chunk)
                if max_len is not None and received > max_len:
                    abort(413)
                data.write(chunk)
            data.seek(0)

        objst = Objstorage()
        with objst.open(path) as hobj:
            hobj.write(data)
    except Exception as e:
        objst_app.logger.error(e)
        return abort(500)

    return make_response('OK', 200)


@objst_app.route('/' + '<path:path>', methods=["DELETE"])
def delete(path):
    ''' File/dir delete handler. '''
    objst_app.logger.debug(f'delete {path}')
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
# handle URL with filename
@objst_app.route('/' + '<path:path>', methods=['HEAD'])
def head(path):
    ''' Is file exist handler. '''
    objst_app.logger.debug(f'head {path}')
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


@objst_app.route('/objst_healthy', methods=['GET'])
def healthcheck():
    ''' Get method for healthcheck. '''
    msg = 'The objst is healthy'
    objst_app.logger.debug(
        f"{msg}."
        f" own ip: {socket.gethostbyname(socket.gethostname())}"
        f" from: {request.remote_addr}")
    return make_response(msg, 200)


# handle URL with filename
@objst_app.route('/' + '<path:path>', methods=['GET'])
def get(path):
    ''' File download handler. '''
    objst_app.logger.debug(f'get {path}')
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
    import waitress
    objst_app.logger.debug(
        "port={} AFC_OBJST_FILE_LOCATION={} AFC_OBJST_MEDIA={}". format(
            objst_app.config['AFC_OBJST_PORT'],
            objst_app.config['AFC_OBJST_FILE_LOCATION'],
            objst_app.config["AFC_OBJST_MEDIA"]))
    os.makedirs(objst_app.config['AFC_OBJST_FILE_LOCATION'], exist_ok=True)
    # production env:
    waitress.serve(
        objst_app, port=objst_app.config['AFC_OBJST_PORT'], host="0.0.0.0")
    # Development env:
    # objst_app.run(port=objst_app.config['AFC_OBJST_PORT'], host="0.0.0.0", debug=True)

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
