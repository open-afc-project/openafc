#!/usr/bin/env python

# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Configuration for filestorage.py.
"""

import sys
import os
import logging

# LOG_FILE or LOG_STREAM shold be None
LOG_FILE = None
LOG_STREAM = sys.stdout

# use "logging.ERROR" to shut it up
LOG_LEVEL = logging.DEBUG

# file location on the server
FILE_LOCATION = "/dl"

# where the server is listening
SERVER_HOST = os.getenv("FILESTORAGE_HOST", "0.0.0.0")
SERVER_PORT = os.getenv("FILESTORAGE_PORT", "5000")

FSVERSION = 'v1/'

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
