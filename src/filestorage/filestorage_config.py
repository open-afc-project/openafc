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

# AFC_OBJST_LOG_FILE or LOG_STREAM should be None
AFC_OBJST_LOG_FILE = os.getenv("AFC_OBJST_LOG_FILE", "/proc/self/fd/2")
LOG_STREAM = None

# Relevant are "DEBUG", "INFO" and "ERROR" 
AFC_OBJST_LOG_LVL = os.getenv("AFC_OBJST_LOG_LVL", "ERROR")

# supported AFC_OBJST_MEDIA backends are "GoogleCloudBucket" and "LocalFS"
AFC_OBJST_MEDIA = os.getenv("AFC_OBJST_MEDIA", "LocalFS")

# file download/upload location on the server in case of AFC_OBJST_MEDIA=LocalFS
FILE_LOCATION = os.getenv("AFC_OBJST_LOCAL_DIR", "/storage")

# where the server is listening
AFC_OBJST_HOST = os.getenv("AFC_OBJST_HOST", "0.0.0.0")
# The Dockerfile does "EXPOSE 5000"
AFC_OBJST_PORT = os.getenv("AFC_OBJST_PORT", 5000)

AFC_OBJST_HIST_HOST = os.getenv("AFC_OBJST_HIST_HOST", "0.0.0.0")
AFC_OBJST_HIST_PORT = os.getenv("AFC_OBJST_HIST_PORT", 4999)

AFC_OBJST_GOOGLE_CLOUD_CREDENTIALS_JSON = os.getenv("AFC_OBJST_GOOGLE_CLOUD_CREDENTIALS_JSON")
AFC_OBJST_GOOGLE_CLOUD_BUCKET = os.getenv("AFC_OBJST_GOOGLE_CLOUD_BUCKET")

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
