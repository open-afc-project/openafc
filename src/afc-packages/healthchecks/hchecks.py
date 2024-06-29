#!/usr/bin/env python3
#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Description

test
"""

import os
import inspect
import logging
import sys
import requests
import socket
from appcfg import HealthchecksMsghndCfgIface
from kombu import Connection

app_log = logging.getLogger(__name__)


class BasicHealthcheck():
    def __init__(self) -> None:
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        self.url = ''

    def healthcheck(self) -> int:
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()"
                      f" url: {self.url}")
        result = 0
        try:
            rawresp = requests.get(self.url)
        except Exception as e:
            app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()"
                          f" exception: {type(e).__name__}")
            result = 1
        else:
            if rawresp.status_code != 200:
                app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()"
                              f" result: {rawresp}")
                result = 1
        return result


class ObjstHealthcheck(BasicHealthcheck):
    """ Provide basic healthcheck for Objst """

    def __init__(self, cfg) -> None:
        app_log.debug(f"({os.getpid()}) {self.__class__.__name__}()")
        self.url = 'http://' + cfg['AFC_OBJST_HOST'] + ':' + \
                   cfg['AFC_OBJST_PORT'] + '/objst_healthy'


class MsghndHealthcheck(BasicHealthcheck):
    """ Provide basic healthcheck for Msghnd """

    def __init__(self, cfg: HealthchecksMsghndCfgIface) -> None:
        app_log.debug(f"({os.getpid()}) {self.__class__.__name__}()")
        self.url = 'http://' + cfg.get_name() + ':' + \
                   cfg.get_port() + '/fbrat/ap-afc/healthy'

    @classmethod
    def from_hcheck_if(cls) -> None:
        app_log.debug(f"({os.getpid()}) {cls.__name__}()")
        return cls(HealthchecksMsghndCfgIface())


class RmqHealthcheck():
    """ Make basic connection for healthcheck """

    def __init__(self, broker_url) -> None:
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        self.connection = Connection(broker_url)

    def healthcheck(self) -> int:
        app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
        conn = self.connection
        res = False
        try:
            conn.connect()
            app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()"
                          f" ip: {socket.gethostbyname(socket.gethostname())}")
        except Exception as e:
            app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()"
                          f" exception {type(e).__name__}")
            return res
        is_conn = conn.connection.connected
        self.connection.close()
        if is_conn:
            return 0
        return 1


# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
