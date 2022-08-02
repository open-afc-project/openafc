#!/usr/bin/env python2
# coding=utf-8

# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
"""
Unittest of data_if.py
"""

import os
import logging
import data_if
import task

logging.basicConfig(level=logging.DEBUG)

class Test(data_if.DataIf):
    __ftypes = ["cfg", "pro", "dbg"]
    def test(self):
        for file_type in self.__ftypes:
            fname = "file_in_" + file_type
            with self.open(file_type, fname) as hfile:
                hfile.write(fname)

        for file_type in self.__ftypes:
            fname = "file_in_" + file_type
            with self.open(file_type, fname) as hfile:
                if not hfile.head():
                    raise Exception("head({}) error".format(fname))
            fname = fname + ".blahblah"
            with self.open(file_type, fname) as hfile:
                if hfile.head():
                    raise Exception("head({}) error".format(fname))

        for file_type in self.__ftypes:
            fname = "file_in_" + file_type
            with self.open(file_type, fname) as hfile:
                data = hfile.read()

        for file_type in self.__ftypes:
            fname = "file_in_" + file_type
            with self.open(file_type, fname) as hfile:
                hfile.delete()

if __name__ == "__main__":
    os.unsetenv("FILESTORAGE_HOST")
    os.unsetenv("FILESTORAGE_PORT")
    os.unsetenv("FILESTORAGE_SCHEME")
    t = Test(fsroot="/tmp")
    t.test()
    del t
    os.environ["FILESTORAGE_HOST"] = "127.0.0.1"
    os.environ["FILESTORAGE_PORT"] = "5000"
    os.environ["FILESTORAGE_SCHEME"] = "HTTP"
    t = Test(fsroot="/tmp")
    t.test()
    del t
    print("At ease!")
