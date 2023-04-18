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

class ObjstConfigExternal(ObjstConfigBase):
    """Filestorage external config"""
    def __init__(self):
        ObjstConfigBase.__init__(self)
        self.AFC_OBJST_HOST = os.getenv("AFC_OBJST_HOST")
        self.AFC_OBJST_HIST_HOST = os.getenv("AFC_OBJST_HIST_HOST")

        self.AFC_OBJST_SCHEME = None
        if "AFC_OBJST_SCHEME" in os.environ:
            self.AFC_OBJST_SCHEME = os.environ["AFC_OBJST_SCHEME"]
            if self.AFC_OBJST_SCHEME not in ("HTTPS", "HTTP"):
                raise InvalidEnvVar("Invalid AFC_OBJST_SCHEME env var.")
