#!/usr/bin/env python3

# Copyright 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Provides env var config for filestorage and history.
"""

import os
from appcfg import ObjstConfigBase, InvalidEnvVar


class ObjstConfigInternal(ObjstConfigBase):
    """Filestorage internal config"""

    def __init__(self):
        ObjstConfigBase.__init__(self)
        self.AFC_OBJST_LOG_FILE = os.getenv(
            "AFC_OBJST_LOG_FILE", "/proc/self/fd/2")
        self.AFC_OBJST_LOG_LVL = os.getenv("AFC_OBJST_LOG_LVL", "ERROR")
        # supported AFC_OBJST_MEDIA backends are "GoogleCloudBucket" and
        # "LocalFS"
        self.AFC_OBJST_MEDIA = os.getenv("AFC_OBJST_MEDIA", "LocalFS")
        if self.AFC_OBJST_MEDIA not in ("GoogleCloudBucket", "LocalFS"):
            raise InvalidEnvVar("Invalid AFC_OBJST_MEDIA env var.")
        if self.AFC_OBJST_MEDIA == "LocalFS":
            # file download/upload location on the server in case of
            # AFC_OBJST_MEDIA=LocalFS
            self.AFC_OBJST_FILE_LOCATION = os.getenv(
                "AFC_OBJST_LOCAL_DIR", "/storage")
        else:
            self.AFC_OBJST_GOOGLE_CLOUD_CREDENTIALS_JSON = os.getenv(
                "AFC_OBJST_GOOGLE_CLOUD_CREDENTIALS_JSON")
            self.AFC_OBJST_GOOGLE_CLOUD_BUCKET = os.getenv(
                "AFC_OBJST_GOOGLE_CLOUD_BUCKET")
