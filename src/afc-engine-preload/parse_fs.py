#!/usr/bin/env python3
# Tool for querying ALS database

# Copyright (C) 2023 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

import sys
import os

noofdirs = 0
nooffiles = 0
max_size = 0
max_name_len = 0
max_tab = 0


def write_obj(name, size, tab, outfile):
    # print(("\t"*tab) + "{} {}".format(name, size))
    outfile.write(("\t" * tab).encode('utf-8'))
    if tab > 0:
        global max_tab
        if tab > max_tab:
            max_tab = tab
    outfile.write(name.encode('utf-8'))
    outfile.write("\0".encode('utf-8'))
    outfile.write(size.to_bytes(8, byteorder="little", signed=True))
    if size != 0:
        global nooffiles
        global max_size
        nooffiles = nooffiles + 1
        if size > max_size:
            max_size = size
    else:
        global noofdirs
        noofdirs = noofdirs + 1
    global max_name_len
    if len(name) > max_name_len:
        max_name_len = len(name)


def sort_key(direntry):
    return direntry.name


def parse_dir(path, tab, outfile):
    if not os.listdir(path):
        return

    if tab >= 0:
        write_obj(os.path.basename(path), 0, tab, outfile)

    dirs = []
    files = []
    for dire in os.scandir(path):
        if dire.is_dir(follow_symlinks=False):
            dirs.append(dire)
        elif dire.is_file(follow_symlinks=False) and dire.stat(follow_symlinks=False).st_size > 0:
            files.append(dire)
    dirs.sort(key=sort_key)
    files.sort(key=sort_key)
    for dire in dirs:
        parse_dir(dire.path, tab + 1, outfile)
    for dire in files:
        write_obj(dire.name, dire.stat().st_size, tab + 1, outfile)


if __name__ == '__main__':
    if len(sys.argv) <= 2 or not os.path.isdir(sys.argv[1]):
        print("usage: {} static_data_path list_path".format(sys.argv[0]))
        sys.exit()
    with open(sys.argv[2], "wb") as outfile:
        outfile.write(noofdirs.to_bytes(4, byteorder="little", signed=False))
        outfile.write(nooffiles.to_bytes(4, byteorder="little", signed=False))
        outfile.write(max_tab.to_bytes(1, byteorder="little", signed=False))
        parse_dir(os.path.normpath(sys.argv[1]), -1, outfile)
    with open(sys.argv[2], "r+b") as outfile:
        outfile.seek(0)
        outfile.write(noofdirs.to_bytes(4, byteorder="little", signed=False))
        outfile.write(nooffiles.to_bytes(4, byteorder="little", signed=False))
        outfile.write(max_tab.to_bytes(1, byteorder="little", signed=False))

    print("dirs {} files {} max tab {} max size {} max name length {}".format(
        noofdirs, nooffiles, max_tab, hex(max_size), max_name_len))
