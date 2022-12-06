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

# LOG_FILE or LOG_STREAM should be None
LOG_FILE = os.getenv("OBJST_LOG_FILE", "/proc/self/fd/2")
LOG_STREAM = None

# Relevant are "DEBUG", "INFO" and "ERROR" 
LOG_LEVEL = os.getenv("OBJST_LOG_LEVEL", "ERROR")

# supported OBJSTORAGE backends are "GoogleCloudBucket" and "LocalFS"
OBJSTORAGE = os.getenv("OBJSTORAGE", "LocalFS")

# file download/upload location on the server in case of OBJSTORAGE=LocalFS
FILE_LOCATION = os.getenv("FILESTORAGE_DIR", "/storage")

PRO_LOCATION = FILE_LOCATION + "/responses"
CFG_LOCATION = FILE_LOCATION + "/afc_config"
DBG_LOCATION = FILE_LOCATION + "/history"

# where the server is listening
SERVER_HOST = os.getenv("FILESTORAGE_HOST", "0.0.0.0")
# The Dockerfile does "EXPOSE 5000"
SERVER_PORT = os.getenv("FILESTORAGE_PORT", 5000)

HISTORY_HOST = os.getenv("HISTORY_HOST", "0.0.0.0")
HISTORY_PORT = os.getenv("HISTORY_PORT", 4999)

GOOGLE_CLOUD_CREDENTIALS_JSON = os.getenv("GOOGLE_CLOUD_CREDENTIALS_JSON")
GOOGLE_CLOUD_BUCKET = os.getenv("GOOGLE_CLOUD_BUCKET")

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
