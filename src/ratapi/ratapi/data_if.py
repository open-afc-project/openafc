# coding=utf-8

# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Provides wrappers for RATAPI file operations
"""
import sys
import os
import errno
import shutil
import logging
import requests
import hashlib
import datetime
import abc
import urlparse
import uuid

RESPONSE_DIR = "responses"  # cache directory in STATE_ROOT_PATH
HISTORY_DIR = "history"     # history directory in STATE_ROOT_PATH
CONFIG_DIR = "afc_config"       # afc_config files directory in STATE_ROOT_PATH

LOGGER = logging.getLogger(__name__)

def mkfolder(path):
    try:
        # ratapi and workers run under different users now.
        # Remove the mode and chmod-s below when this is fixed.
        os.makedirs(path, 0o777)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise e
    else:
        LOGGER.debug("mkfolder({})".format(path))

class DataInt:
    """ Abstract class for data backend operations """
    __metaclass__ = abc.ABCMeta

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
            with open(src, "rb") as f:
                self.write(f.read())
            f.close()

    def read_file(self, dest):
        """ Will be removed. Copy file from remote storage to local fs """
        LOGGER.debug("DataIntFs.read_file({}) <- {}".format(dest, self.file_name))
        if self.file_name != dest and not os.path.exists(dest):
            mkfolder(os.path.dirname(dest))
            with open(dest, "wb") as f:
                f.write(self.read())
            f.close()
            try:
                os.chmod(dest, 0o666)
            except:
                pass

    def write(self, data):
        """ write data to backend """
        LOGGER.debug("DataIntFs.write() {}".format(self.file_name))
        mkfolder(os.path.dirname(self.file_name))
        with open(self.file_name, "wb") as f:
            f.write(data)
        f.close()
        try:
            os.chmod(self.file_name, 0o666)
        except:
            pass
        if (not self.head()):
             raise Exception("Write failed")

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
        r = requests.get(self.file_name, stream=True)
        if r.ok:
            r.raw.decode_content = False
            return r.raw.read()
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

    def __init__(self, id, user_id, history_dir, state_root):
        """ Check IO backend """
        self.id = id
        self.user_id = user_id
        self.history_dir = history_dir
        self.state_root = state_root
        self.file_types = {
            "pro": None,
            "cfg": None,
            "dbg": None
            }
        if self.id and self.state_root:
            self.tmp = os.path.join(self.state_root, RESPONSE_DIR, self.id)
        if ('FILESTORAGE_HOST' in os.environ and
                'FILESTORAGE_PORT' in os.environ):
            self.backend = "http"
            self.host = os.environ['FILESTORAGE_HOST']
            self.port = os.environ['FILESTORAGE_PORT']
            if self.id:
                self.file_types["pro"] = "http://" + self.host + ":" + str(self.port) + "/" + "pro" + "/" + self.id
            if self.user_id:
                self.file_types["cfg"] = "http://" + self.host + ":" + str(self.port) + "/" + "cfg" + "/" + str(self.user_id)
            if self.history_dir:
                self.file_types["dbg"] = "http://" + self.host + ":" + str(self.port) + "/" + "dbg" + "/" + self.history_dir
        else:
            self.backend = "fs"
            if self.id:
                self.file_types["pro"] = os.path.join(self.state_root, RESPONSE_DIR, self.id)
            if self.user_id:
                self.file_types["cfg"] = os.path.join(self.state_root, CONFIG_DIR, str(self.user_id))
            if self.history_dir:
                self.file_types["dbg"] = os.path.join(self.local_history_dir(), self.history_dir)
        LOGGER.debug("DataIf.__init__(id={}, userid={}, history_dir={}, state_root={}) backend={}".
                     format(self.id, self.user_id, self.history_dir, self.state_root, self.backend))

    def rname(self, file_type, base_name=""):
        """ Return remote file name by basename """
        if self.backend == "fs":
            return os.path.join(self.file_types[file_type], base_name)
        if self.backend == "http":
            return self.file_types[file_type] + "/" + base_name
        raise Exception("Undeclared backend")

    def lname(self, base_name=""):
        """ Return local file path by basename """
        return os.path.join(self.tmp, base_name)

    def open_by_name(self, r_name):
        """ Create FileInt instance """
        LOGGER.debug("DataIf.open_by_name({})".format(r_name))
        if self.backend == "fs":
            return DataIntFs(r_name)
        if self.backend == "http":
            return DataIntHttp(r_name)
        raise Exception("Undeclared backend")

    def open(self, file_type, base_name):
        """ Create FileInt instance """
        return self.open_by_name(self.rname(file_type, base_name))

    def genId(self, conf, req):
        """ Set id to md5 of both buffers
        md5 id is intended for request result caching. Until cache implementation,
        ID is replaced with a unique number.
        """
        #hash = hashlib.md5()
        #hash.update(conf)
        #hash.update(req)
        #self.id = hash.hexdigest()
        self.id = str(uuid.uuid4())
        if (self.state_root):
            self.tmp = os.path.join(self.state_root, RESPONSE_DIR, self.id)
        if self.backend == "http":
            self.file_types["pro"] = "http://" + self.host + ":" + str(self.port) + "/" + "pro" + "/" + self.id
        else:
            self.file_types["pro"] = os.path.join(self.state_root, RESPONSE_DIR, self.id)
        LOGGER.debug("DataIf.genId id={}".format(self.id))

    def get_id(self):
            return self.id

    def mktmpdir(self):
        """ Create afc-engine temporary dir """
        LOGGER.debug("DataIf.mktmpdir() {}".format(self.tmp))
        mkfolder(self.tmp)

    def rmtmpdir(self, but=None):
        """ Remove afc-engine temporary dir """
        LOGGER.debug("rmtmpdir({}) {}".format(but, self.tmp))
        if self.backend == "http" and not but:
            if os.path.exists(self.tmp):
                shutil.rmtree(self.tmp)
        elif self.backend == "fs" and but:
            for fname in os.listdir(self.tmp):
                if fname not in but:
                    LOGGER.debug("        rm {}".format(fname))
                    os.remove(os.path.join(self.tmp, fname))
                else:
                    LOGGER.debug("        keep {}".format(fname))
            if not os.listdir(self.tmp):
                shutil.rmtree(self.tmp)

    def local_history_dir(self):
        """ Return local history directory """
        if self.backend == "fs":
            return os.path.join(self.state_root, HISTORY_DIR)
        return None

    def history_url(self, url, path):
        """ Build url for "Debug Files" key """
        newurl = None
        u = urlparse.urlparse(url)
        if self.backend == "fs":
            newurl = u.scheme + "://" + u.netloc + path
        elif self.backend == "http":
            newurl = u.scheme + "://" + u.hostname + ":" + str(os.getenv("HISTORY_EXTERNAL_PORT")) + "/"
        LOGGER.debug("history_url({}, {}) {}".format(url, path, newurl))
        return newurl

# vim: sw=4:et:tw=80:cc=+1
