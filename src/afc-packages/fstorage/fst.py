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
import shutil
import errno
import requests


app_log = logging.getLogger(__name__)


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


class DataIntFs(DataInt):
    """ Data prot operations for the local FS prot """

    def __mkdir(self, path):
        try:
            os.makedirs(path, 0o777)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise e
        else:
            app_log.debug("__mkBaseDir({})".format(path))
        self.__fixMod(path)

    def __fixMod(self, path):
        # ratapi, workers, and user console are run under root and fbrat users.
        # In the future, all those processes will be executed
        # under "fbrat" 1003:1003,
        # and this function will be removed.
        try:
            os.chown(path, 1003, 1003)
            mode = os.stat(path).st_mode
            if mode & 0o111:
                mode |= 0o111
            mode |= 0o666
            os.chmod(path, mode)
        except OSError:
            pass

    def write(self, data):
        """ write data to prot """
        app_log.debug("DataIntFs.write() {}".format(self._file_name))
        self.__mkdir(os.path.dirname(self._file_name))
        with open(self._file_name, "wb") as f:
            f.write(data)
        self.__fixMod(self._file_name)

    def read(self):
        """ read data from prot """
        app_log.debug("DataIntFs.read({})".format(self._file_name))
        r = None
        with open(self._file_name, "rb") as f:
            r = f.read()
        if r is None:
            raise Exception("Cant get file")
        return r

    def head(self):
        """ is data exist in prot """
        app_log.debug("DataIntFs.exists({})".format(self._file_name))
        return os.path.exists(self._file_name)

    def delete(self):
        """ remove data from prot """
        app_log.debug("DataIntFs.delete({})".format(self._file_name))
        if os.path.exists(self._file_name):
            if os.path.isdir(self._file_name):
                shutil.rmtree(self._file_name, ignore_errors=True)
            else:
                os.remove(self._file_name)


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
    REMOTE = "remote"
    LOCAL = "local"
    HTTP = "http"
    HTTPS = "https"

    # HTTPS connection timeout before falling to HTTP
    HTTPS_TIMEOUT = 0.5

    LHISTORYDIR = "history"

    def __init__(self, probeHttps):
        def httpsProbe(host, port, probeHttps):
            if probeHttps is False:
                return False
            if not host or not port:
                raise Exception("Missing host:port")
            url = "https://" + host + ":" + str(port) + "/" + "cfg"
            try:
                requests.head(url, timeout=self.HTTPS_TIMEOUT)
            except requests.exceptions.ConnectionError:  # fall to http
                app_log.debug("httpsProbe() fall to HTTP")
                return False
            # use https
            app_log.debug("httpsProbe() HTTPS ok")
            return True

        if self._prot == self.REMOTE:
            if httpsProbe(self._host, self._port, probeHttps):
                self._prot = self.HTTPS
            else:
                self._prot = self.HTTP

        self._rpath = {
            "pro": None,
            "cfg": None,
            "dbg": None,
            "cert": None,
            }
        if self._prot == self.LOCAL and self._fsroot and self._mnt_root:
            self._rpath["pro"] = os.path.join(self._mnt_root, "responses")
            self._rpath["cfg"] = os.path.join(self._mnt_root, "afc_config")
            self._rpath["dbg"] = os.path.join(self._fsroot, self.LHISTORYDIR)
        elif self._prot == self.HTTP and self._host and self._port:
            pref = "http://" + self._host + ":" + str(self._port) + "/"
            self._rpath["pro"] = pref + "pro"
            self._rpath["cfg"] = pref + "cfg"
            self._rpath["dbg"] = pref + "dbg"
            self._rpath["cert"] = pref + "cert"
        elif self._prot == self.HTTPS and self._host and self._port:
            pref = "https://" + self._host + ":" + str(self._port) + "/"
            self._rpath["pro"] = pref + "pro"
            self._rpath["cfg"] = pref + "cfg"
            self._rpath["dbg"] = pref + "dbg"

    def open(self, r_name):
        """ Create FileInt instance """
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        app_log.debug("DataIfBaseV1.open({})".format(r_name))
        if self._prot == self.LOCAL:
            return DataIntFs(r_name)
        if self._prot in (self.HTTP, self.HTTPS):
            return DataIntHttp(r_name)
        raise Exception("Undeclared backend")


class DataIf(DataIfBaseV1):
    """ Wrappers for RATAPI data operations """
    def __init__(self, prot=DataIfBaseV1.AUTO, host=None, port=None,
                 fsroot=None, probeHttps=None, mntroot=None):
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        # Assign default args from env vars
        self._host = host
        if "AFC_OBJST_HOST" in os.environ and host is None:
            self._host = os.environ["AFC_OBJST_HOST"]
        self._port = port
        if "AFC_OBJST_PORT" in os.environ and port is None:
            self._port = os.environ["AFC_OBJST_PORT"]
        scheme = None
        if "AFC_OBJST_SCHEME" in os.environ:
            scheme = os.environ["AFC_OBJST_SCHEME"]
            if probeHttps is None:
                probeHttps = False
        self._fsroot = fsroot
        self._mnt_root = mntroot

        # check args
        if self._port is not None and not self._port.isdigit():
            raise "Invalid port. Try to remove the quotes around AFC_OBJST_PORT value"
        if scheme is not None and scheme != "HTTPS" and scheme != "HTTP":
            raise "Invalid scheme. Try to remove the quotes around AFC_OBJST_SCHEME value"

        # Clarify protocol by env.vars
        if prot == self.AUTO:
            if self._host and self._port:
                prot = self.REMOTE
            else:
                prot = self.LOCAL
        if prot == self.REMOTE:
            if scheme == "HTTPS":
                prot = self.HTTPS
            if scheme == "HTTP":
                prot = self.HTTP

        # Check protocol
        if prot == self.LOCAL:
            if not fsroot:
                raise Exception("Missing fsroot")
            if not mntroot:
                raise Exception("Missing mntroot")
        elif prot in (self.HTTP, self.HTTPS, self.REMOTE):
            if not self._host or not self._port:
                raise Exception("Missing host:port")
        else:
            raise Exception("Bad prot arg")

        self._prot = prot

        DataIfBaseV1.__init__(self, probeHttps)

        app_log.debug("DataIf.__init__: prot={} host={} port={} fsroot={} probeHttps={} scheme={} mntroot={} _rpath={}".format(self._prot, self._host, self._port, self._fsroot, probeHttps, scheme, self._mnt_root, self._rpath))

    def rname(self, type, baseName):
        """ Return remote file name by basename """
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        return self._rpath[type] + "/" + baseName

    def open(self, type, baseName):
        """ Create FileInt instance """
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        return DataIfBaseV1.open(self, self.rname(type, baseName))

    def isFsBackend(self):
        return self._prot == self.LOCAL

    def getProtocol(self):
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        return self._prot, self._host, self._port, self._fsroot, self._mnt_root

# vim: sw=4:et:tw=80:cc=+1
