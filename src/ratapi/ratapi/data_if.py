# coding=utf-8

# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Provides wrappers for RATAPI file operations
"""
import os
import errno
import shutil
import uuid
import logging
import flask
import requests
import hashlib
import datetime
import abc

RESPONSE_DIR = "responses"  # cache directory in STATE_ROOT_PATH
HISTORY_DIR = "history"     # history directory in STATE_ROOT_PATH
CONFIG_DIR = "config"       # afc_config files directory in STATE_ROOT_PATH

LOGGER = logging.getLogger(__name__)

def mkfolder(path):
    try:
        os.makedirs(path)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise e
    else:
        LOGGER.debug("mkfolder({})".format(path))

class DataInt:
    """ Abstract class for data backend operations """
    __metaclass__ = abc.ABCMeta

    file_name = None

    def __init__(self, file_name):
        self.file_name = file_name

    @abc.abstractmethod
    def write_file(self, src):
        pass

    @abc.abstractmethod
    def read_file(self, dest):
        pass

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
    """ Data backend operations for the local FS backend """

    def write_file(self, src):
        """ Copy file or data from local fs to remote storage """
        LOGGER.debug("DataIntFs.write_file({}) -> {}".format(src, self.file_name))
        if not os.path.exists(src):
            LOGGER.error("write_file(): {} not exist".format(src))
            raise Exception("Source file doesnt found")
        if src != self.file_name:
            mkfolder(os.path.dirname(self.file_name))
            with open(src, "rb") as f:
                self.write(f.read())
            f.close()

    def read_file(self, dest):
        """ Will be removed. Copy file from remote storage to local fs """
        LOGGER.debug("DataIntFs.read_file({}) <- {}".format(dest, self.file_name))
        if self.file_name != dest:
            mkfolder(os.path.dirname(dest))
            with open(dest, "wb") as f:
                f.write(self.read())
            f.close()

    def write(self, data):
        """ write data to backend """
        LOGGER.debug("DataIntFs.write() {}".format(self.file_name))
        with open(self.file_name, "wb") as f:
            f.write(data)

    def read(self):
        """ read data from backend """
        LOGGER.debug("DataIntFs.read({})".format(self.file_name))
        r = None
        with open(self.file_name) as f:
            r = f.read()
        if r is None:
            raise Exception("Cant get file")
        else:
            return r

    def head(self):
        """ is data exist in backend """
        LOGGER.debug("DataIntFs.exists({})".format(self.file_name))
        return os.path.exists(self.file_name)

    def delete(self):
        """ remove data from backend """
        LOGGER.debug("DataIntFs.delete({})".format(self.file_name))
        if os.path.exists(self.file_name):
            if os.path.isdir(self.file_name):
                shutil.rmtree(self.file_name, ignore_errors=True)
            else:
                os.remove(self.file_name)


class DataIntHttp(DataInt):
    """ Data backend operations for the HTTP server backend """

    def write_file(self, src):
        """ Copy file or data from local fs to remote storage """
        LOGGER.debug("DataIntHttp.write_file({}) -> {}".format(src, self.file_name))
        if not os.path.exists(src):
            LOGGER.error("write_file(): {} not exist".format(src))
            raise Exception("Source file doesnt found")
        with open(src, "rb") as f:
            self.write(f.read())
        f.close()

    def read_file(self, dest):
        """ Will be removed. Copy file from remote storage to local fs """
        LOGGER.debug("DataIntHttp.read_file({}) <- {}".format(dest, self.file_name))
        mkfolder(os.path.dirname(dest))
        with open(dest, "wb") as f:
            f.write(self.read())
        f.close()

    def write(self, data):
        """ write data to backend """
        LOGGER.debug("DataIntHttp.write({})".format(self.file_name))
        r = requests.post(self.file_name, data=data)
        if not r.ok:
            raise Exception("Cant post file")

    def read(self):
        """ read data from backend """
        LOGGER.debug("DataIntHttp.read({})".format(self.file_name))
        r = requests.get(self.file_name)
        if r.ok:
            return r.content
        raise Exception("Cant get file")

    def head(self):
        """ is data exist in backend """
        LOGGER.debug("DataIntHttp.exists({})".format(self.file_name))
        r = requests.head(self.file_name)
        return r.ok

    def delete(self):
        """ remove data from backend """
        LOGGER.debug("DataIntHttp.delete({})".format(self.file_name))
        requests.delete(self.file_name)


class DataIf_v1():
    """ Wrappers for RATAPI data operations """
    backend = None  # None, "fs" or "http"
    host = "localhost"
    port = 5000
    id = None
    state_root = None
    file_types = {}
    user_id = None
    user_name = None
    tmp = None

    def __init__(self, id, user_id, user_name, state_root):
        """ Check IO backend """
        self.id = id
        self.user_id = user_id
        self.user_name = user_name
        self.state_root = state_root
        self.tmp = os.path.join(state_root, RESPONSE_DIR, self.id)
        if ('FILESTORAGE_HOST' in os.environ and
                'FILESTORAGE_PORT' in os.environ):
            self.backend = "http"
            self.host = os.environ['FILESTORAGE_HOST']
            self.port = os.environ['FILESTORAGE_PORT']
            self.file_types["pro"] = "http://" + self.host + ":" + str(self.port) + "/" + "pro" + "/" + self.id
            self.file_types["cfg"] = "http://" + self.host + ":" + str(self.port) + "/" + "cfg" + "/" + str(self.user_id)
            self.file_types["dbg"] = "http://" + self.host + ":" + str(self.port) + "/" + "dbg" + "/" + self.user_name + '-' + str(datetime.datetime.now().isoformat())
        else:
            self.backend = "fs"
            self.file_types["pro"] = os.path.join(state_root, RESPONSE_DIR, self.id)
            self.file_types["cfg"] = os.path.join(state_root, CONFIG_DIR, str(self.user_id))
            self.file_types["dbg"] = os.path.join(state_root, HISTORY_DIR, self.user_name, str(datetime.datetime.now().isoformat()))
        LOGGER.debug("DataIf.__init__(id={}, userid={}, username={}, state_root={}) backend={}".format(self.id, self.user_id, self.user_name, self.state_root, self.backend))

    def __rname(self, file_type, base_name=""):
        """ Return remote file name by basename """
        if self.backend == "fs":
            return os.path.join(self.file_types[file_type], base_name)
        if self.backend == "http":
            return self.file_types[file_type] + "/" + base_name
        raise Exception("Undeclared backend")

    def lname(self, base_name=""):
        """ Return local file path by basename """
        return os.path.join(self.tmp, base_name)

    def open(self, file_type, base_name):
        """ Create FileInt instance """
        r_name = self.__rname(file_type, base_name)
        LOGGER.debug("DataIf.open({})".format(r_name))
        if self.backend == "fs":
            return DataIntFs(r_name)
        if self.backend == "http":
            return DataIntHttp(r_name)
        raise Exception("Undeclared backend")

    def genId(self, conf, req):
        """ Set id to md5 of both buffers """
        hash = hashlib.md5()
        hash.update(conf)
        hash.update(req)
        self.id = hash.hexdigest()
        LOGGER.debug("DataIf.setDataId id={}".format(self.id))

    def tmpdir(self):
        return self.tmp

    def mktmpdir(self):
        mkfolder(self.tmp)

    def rmtmpdir(self, fromWorker):
        """ Remove local temporary dir """
        if self.backend == "http" or (self.backend == "fs" and not fromWorker):
            LOGGER.debug("rmtmpdir({}) {}".format(fromWorker, self.tmp))
            if os.path.exists(self.tmp):
                shutil.rmtree(self.tmp)

# vim: sw=4:et:tw=80:cc=+1
