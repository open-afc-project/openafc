#!/usr/bin/env python3

# Copyright 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Provides HTTP server for getting history.
"""

import os
import errno
import re
import logging
import http.server
import filestorage_config

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    os.makedirs(filestorage_config.DBG_LOCATION, exist_ok=True)
    os.chdir(filestorage_config.DBG_LOCATION)
    logging.debug("HTTPServer host={} port={} dir={}".format(filestorage_config.HISTORY_HOST, filestorage_config.HISTORY_PORT, filestorage_config.DBG_LOCATION))
    with http.server.HTTPServer((filestorage_config.HISTORY_HOST, int(filestorage_config.HISTORY_PORT)), http.server.SimpleHTTPRequestHandler) as server:
        server.serve_forever()
