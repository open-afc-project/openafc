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
import data_if
import logging
import filecmp

logging.basicConfig(level=logging.DEBUG)

class testv1(data_if.DataIf_v1):
    __ftypes = ["cfg", "pro", "dbg"]
    def test(self):
        self.mktmpdir()
        for file_type in self.__ftypes:
            fname = self.lname("file_in_" + file_type)
            with open(fname, "wb") as hfile:
                hfile.write(fname)

        for file_type in self.__ftypes:
            fname = "file_in_" + file_type
            with self.open(file_type, fname) as hfile:
                hfile.write_file(self.lname(fname))

        for file_type in self.__ftypes:
            fname = "file_in_" + file_type
            with self.open(file_type, fname) as hfile:
                hfile.read_file(self.lname(fname+".copy"))

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
            fname = self.lname("file_in_" + file_type)
            if not filecmp.cmp(fname, fname + ".copy"):
                raise Exception("cmp({}, {}) error".format(fname, fname + ".copy"))

        for file_type in self.__ftypes:
            fname = "file_in_" + file_type
            with self.open(file_type, fname) as hfile:
                hfile.delete()

        self.rmtmpdir()

if __name__ == '__main__':
    t = testv1(None, None, None, None)
    del t
    t = testv1(None, None, None, "/var/lib/fbrat")
    del t
    t = testv1(None, 1, "user123", "/var/lib/fbrat")
    del t
    t = testv1("12345678901234567890123456789012", 1, "user123", "/var/lib/fbrat")
    t.test()
    del t
    os.environ['FILESTORAGE_HOST'] = "127.0.0.1"
    os.environ['FILESTORAGE_PORT'] = "5000"
    t = testv1("12345678901234567890123456789012", 1, "user123", "/var/lib/fbrat")
    t.test()
    del t
    print("At ease!")
