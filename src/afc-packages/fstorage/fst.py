# coding=utf-8

# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Provides wrappers for RATAPI file operations
"""

import abc
import hashlib
import hmac
import os
import inspect
import logging
import requests
from appcfg import ObjstConfig

app_log = logging.getLogger(__name__)
conf = ObjstConfig()


def _derive_objst_bearer_token(key):
    """ Purpose-bound objstore bearer token:
    HMAC-SHA256(key, 'afc-objst-bearer-v1').

    The raw provisioned secret must never transit the wire: it is also the
    input of the RMQ response-signing key derivation
    (rcache_models._derive_rmq_resp_hmac_key), so a bearer-header observer
    on plaintext objstore HTTP traffic must not obtain material from which
    that signing key can be recomputed.  Sending a per-purpose subkey means
    neither wire value yields the other (or the raw secret).
    """
    return hmac.new(key.encode("utf-8"), b"afc-objst-bearer-v1",
                    hashlib.sha256).hexdigest()


def _get_objst_auth_headers():
    """ Return Authorization bearer header if AFC_OBJST_API_KEY_FILE is set. """
    key_file = os.environ.get("AFC_OBJST_API_KEY_FILE")
    if key_file and os.path.isfile(key_file):
        with open(key_file) as f:
            key = f.read().strip()
        if key:
            return {"Authorization":
                    f"Bearer {_derive_objst_bearer_token(key)}"}
    return {}


class DataInt:
    """ Abstract class for data prot operations """
    __metaclass__ = abc.ABCMeta

    def __init__(self, file_name):
        self._file_name = file_name

    @abc.abstractmethod
    def write(self, data):
        pass

    @abc.abstractmethod
    def read(self, max_bytes=None):
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


class DataIntHttp(DataInt):
    """ Data prot operations for the HTTP server prot """

    def write(self, data):
        """ write data to prot """
        app_log.debug("DataIntHttp.write({})".format(self._file_name))
        r = requests.post(self._file_name, data=data,
                          headers=_get_objst_auth_headers(), timeout=30)
        if not r.ok:
            raise RuntimeError("Cant post file")

    def read(self, max_bytes=None):
        """ read data from prot

        max_bytes, when set, rejects objects larger than that many bytes
        BEFORE buffering them into memory (the object body is untrusted
        input; see the dispatcher mTLS-bundle install path).
        """
        app_log.debug("DataIntHttp.read({})".format(self._file_name))
        r = requests.get(self._file_name, stream=True,
                         headers=_get_objst_auth_headers(), timeout=30)
        if r.ok:
            r.raw.decode_content = False
            if max_bytes is None:
                return r.raw.read()
            # Reject on the declared length first, then bound the actual
            # streamed read (Content-Length may be absent or wrong).
            content_length = r.headers.get("Content-Length")
            if content_length is not None:
                try:
                    declared = int(content_length)
                except ValueError:
                    raise RuntimeError(
                        "Object {} has malformed Content-Length".format(
                            self._file_name))
                if declared > max_bytes:
                    raise RuntimeError(
                        "Object {} exceeds size limit of {} bytes".format(
                            self._file_name, max_bytes))
            chunks = []
            total = 0
            while total <= max_bytes:
                chunk = r.raw.read(65536)
                if not chunk:
                    break
                chunks.append(chunk)
                total += len(chunk)
            if total > max_bytes:
                raise RuntimeError(
                    "Object {} exceeds size limit of {} bytes".format(
                        self._file_name, max_bytes))
            return b"".join(chunks)
        if r.status_code == 404:
            # Distinguish "object absent" from transport/server errors so
            # callers (e.g. afctask.Task ownership preservation) can treat
            # only genuine absence as benign and fail closed on the rest.
            raise FileNotFoundError(
                "Object {} not found".format(self._file_name))
        raise Exception("Cant get file")

    def head(self):
        """ is data exist in prot """
        app_log.debug("DataIntHttp.exists({})".format(self._file_name))
        r = requests.head(self._file_name,
                          headers=_get_objst_auth_headers(), timeout=30)
        return r.ok

    def delete(self):
        """ remove data from prot """
        app_log.debug("DataIntHttp.delete({})".format(self._file_name))
        r = requests.delete(self._file_name,
                            headers=_get_objst_auth_headers(), timeout=30)
        # A 404 means the object is already gone, which satisfies the
        # caller's intent; any other non-2xx status (401/403/503/5xx)
        # means the object may still exist and must fail loudly
        # (mirrors write()'s error handling).
        if not r.ok and r.status_code != 404:
            raise RuntimeError("Cant delete file")


class DataIfBaseV1():
    """ Object storage access """
    HTTP = "HTTP"
    HTTPS = "HTTPS"

    # HTTPS connection timeout before falling to HTTP
    HTTPS_TIMEOUT = 0.5

    def __init__(self):
        assert self._host is not None, "Missing host"
        assert self._port is not None, "Missing port"
        assert self._prot in (self.HTTP, self.HTTPS), "Wrong or missing scheme"
        self._pref = self._prot + "://" + \
            self._host + ":" + str(self._port) + "/"

    def open(self, r_name):
        """ Create FileInt instance """
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        app_log.debug("DataIfBaseV1.open({})".format(r_name))
        return DataIntHttp(r_name)

    def healthcheck(self):
        """ Call healthcheck """
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        app_log.debug("DataIfBaseV1.healthcheck()")
        return requests.get(self._pref + '/healthy', timeout=30)

    @staticmethod
    def httpsProbe(host, port):
        if not host or not port:
            raise Exception("Missing host:port")
        url = "https://" + host + ":" + str(port) + "/"
        try:
            requests.head(url, timeout=DataIfBaseV1.HTTPS_TIMEOUT)
        except requests.exceptions.ConnectionError:  # fall to http
            app_log.debug("httpsProbe() fall to HTTP")
            return False
        # use https
        app_log.debug("httpsProbe() HTTPS ok")
        return True


class DataIf(DataIfBaseV1):
    """ Wrappers for RATAPI data operations """

    def __init__(self, prot=None, host=None, port=None):
        # Assign default args from env vars
        self._host = host
        if self._host is None:
            self._host = conf.AFC_OBJST_HOST

        self._port = port
        if self._port is None:
            self._port = conf.AFC_OBJST_PORT

        self._prot = prot
        if self._prot is None:
            self._prot = conf.AFC_OBJST_SCHEME

        DataIfBaseV1.__init__(self)

        app_log.debug("DataIf.__init__: prot={} host={} port={} _pref={}"
                      .format(self._prot, self._host, self._port, self._pref))

    def rname(self, baseName):
        """ Return remote file name by basename """
        return self._pref + baseName

    def open(self, baseName):
        """ Create FileInt instance """
        return DataIfBaseV1.open(self, self.rname(baseName))

    def getProtocol(self):
        return self._prot, self._host, self._port

# vim: sw=4:et:tw=80:cc=+1
