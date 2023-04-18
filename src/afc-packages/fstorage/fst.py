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
import os
import inspect
import logging
import requests
from appcfg import ObjstConfig

app_log = logging.getLogger(__name__)
conf = ObjstConfig()

class DataInt:
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


class DataIntHttp(DataInt):
    """ Data prot operations for the HTTP server prot """

    def write(self, data):
        """ write data to prot """
        app_log.debug("DataIntHttp.write({})".format(self._file_name))
        r = requests.post(self._file_name, data=data)
        if not r.ok:
            raise Exception("Cant post file")

    def read(self):
        """ read data from prot """
        app_log.debug("DataIntHttp.read({})".format(self._file_name))
        r = requests.get(self._file_name, stream=True)
        if r.ok:
            r.raw.decode_content = False
            return r.raw.read()
        raise Exception("Cant get file")

    def head(self):
        """ is data exist in prot """
        app_log.debug("DataIntHttp.exists({})".format(self._file_name))
        r = requests.head(self._file_name)
        return r.ok

    def delete(self):
        """ remove data from prot """
        app_log.debug("DataIntHttp.delete({})".format(self._file_name))
        requests.delete(self._file_name)


class DataIfBaseV1():
    """ Object storage access """
    AUTO = "auto"
    HTTP = "http"
    HTTPS = "https"

    # HTTPS connection timeout before falling to HTTP
    HTTPS_TIMEOUT = 0.5

    def __init__(self, probeHttps):
        def httpsProbe(host, port, probeHttps):
            if probeHttps is False:
                return False
            if not host or not port:
                raise Exception("Missing host:port")
            url = "https://" + host + ":" + str(port) + "/"
            try:
                requests.head(url, timeout=self.HTTPS_TIMEOUT)
            except requests.exceptions.ConnectionError:  # fall to http
                app_log.debug("httpsProbe() fall to HTTP")
                return False
            # use https
            app_log.debug("httpsProbe() HTTPS ok")
            return True

        if self._prot == self.AUTO:
            if httpsProbe(self._host, self._port, probeHttps):
                self._prot = self.HTTPS
            else:
                self._prot = self.HTTP

        self._pref = None
        if self._prot == self.HTTP:
            self._pref = "http://" + self._host + ":" + str(self._port) + "/"
        elif self._prot == self.HTTPS:
            self._pref = "https://" + self._host + ":" + str(self._port) + "/"

    def open(self, r_name):
        """ Create FileInt instance """
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        app_log.debug("DataIfBaseV1.open({})".format(r_name))
        return DataIntHttp(r_name)


class DataIf(DataIfBaseV1):
    """ Wrappers for RATAPI data operations """
    def __init__(self, prot=DataIfBaseV1.AUTO, host=None, port=None, probeHttps=None):
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        # Assign default args from env vars
        self._host = host
        if host is None:
            self._host =conf.AFC_OBJST_HOST
        self._port = port
        if port is None:
            self._port = conf.AFC_OBJST_PORT
        scheme = conf.AFC_OBJST_SCHEME
        if probeHttps is None:
            probeHttps = False

        # Clarify protocol by env.vars
        if prot == self.AUTO:
            if scheme == "HTTPS":
                prot = self.HTTPS
            if scheme == "HTTP":
                prot = self.HTTP

        # Check protocol
        if prot not in (self.HTTP, self.HTTPS, self.AUTO):
            raise Exception("Bad prot arg")

        self._prot = prot

        DataIfBaseV1.__init__(self, probeHttps)

        app_log.debug("DataIf.__init__: prot={} host={} port={} probeHttps={} scheme={} _pref={}"
                      .format(self._prot, self._host, self._port, probeHttps, scheme, self._pref))

    def rname(self, baseName):
        """ Return remote file name by basename """
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        return self._pref + baseName

    def open(self, baseName):
        """ Create FileInt instance """
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        return DataIfBaseV1.open(self, self.rname(baseName))

    def getProtocol(self):
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        return self._prot, self._host, self._port

# vim: sw=4:et:tw=80:cc=+1
