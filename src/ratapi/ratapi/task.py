# coding=utf-8

# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Provides replacement for AsyncResult routines
"""

import logging
import json
import time
import data_if

LOGGER = logging.getLogger(__name__)

class task():
    """ Replacement for AsyncResult class and self serialization"""
    def __init__(self, dataif):
        LOGGER.debug("task.__init__(id={})".format(dataif.id))
        self.dataif = dataif

    def get(self):
        data = None
        try:
            with self.dataif.open("pro", "status.json") as hfile:
                data = hfile.read()
        except:
            LOGGER.debug("task.get() no {}".format(self.dataif.rname("pro", "status.json")))
            return self.toDict("PENDING")
        stat = json.loads(data)

        LOGGER.debug("task.get() {}".format(stat))
        if (not 'status' in stat or
            not 'user_id' in stat or
            not 'history_dir' in stat or
            not 'hash' in stat or
            not 'runtime_opts' in stat or
            not 'state_root' in stat
            ):
            LOGGER.error("task.get() bad {}: {}".
                         format(self.dataif.rname("pro", "status.json"), stat))
            raise Exception("Bad status.json")
        if (stat['status'] != "PROGRESS" and
            stat['status'] != "SUCCESS" and
            stat['status'] != "FAILURE"):
            LOGGER.error("task.get() bad ['status'] {} in {}".
                         format(stat['status'], self.dataif.rname("pro", "status.json")))
            raise Exception("Bad status in status.json")

        return stat

    def wait(self, timeout=200, delay=2):
        LOGGER.debug("task.wait()")
        stat = None
        time0 = time.clock()
        while True:
            stat = self.get()
            LOGGER.debug("task.wait() {}".format(stat['status']))
            if (stat['status'] == "SUCCESS" or stat['status'] == "FAILURE"):
                LOGGER.debug("task.wait() return {}".format(stat['status']))
                return stat
            if (time. clock() - time0) > timeout:
                LOGGER.debug("task.wait() timeout")
                return self.toDict("FAILURE")
            time.sleep(delay)
        LOGGER.debug("task.wait() exit")

    def ready(self, stat):
        return stat['status'] == "SUCCESS" or stat['status'] == "FAILURE"

    def successful(self, stat):
        return stat['status'] == "SUCCESS"

    def toDict(self, status, runtime_opts=None, exit_code=0):
        return {
            'status': status,
            'user_id': self.dataif.user_id,
            'history_dir': self.dataif.history_dir,
            'hash': self.dataif.id,
            'runtime_opts': runtime_opts,
            'exit_code': exit_code,
            'state_root': self.dataif.state_root
            }

    def toJson(self, stat):
        print("toJson({})".format(stat))
        data = json.dumps(stat)
        with self.dataif.open("pro", "status.json") as hfile:
            hfile.write(data)


