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

LOGGER = logging.getLogger(__name__)
AFC_ENG_TIMEOUT = 600

class Task():
    """ Replacement for AsyncResult class and self serialization"""

    STAT_PENDING = "PENDING"
    STAT_PROGRESS = "PROGRESS"
    STAT_SUCCESS = "SUCCESS"
    STAT_FAILURE = "FAILURE"

    def __init__(self, task_id, dataif, fsroot, hash=None,
                 user_id=None, history_dir=None):
        LOGGER.debug("task.__init__(task_id={})".format(task_id))
        self.__dataif = dataif
        self.__task_id = task_id
        self.__stat = {
            'status': self.STAT_PENDING,
            'user_id': user_id,
            'history_dir': history_dir,
            'hash': hash,
            'runtime_opts': None,
            'exit_code': 0,
            'state_root': fsroot
            }

    def get(self):
        LOGGER.debug("Task.get()")
        data = None
        try:
            with self.__dataif.open("pro", self.__task_id +
                                  "/status.json") as hfile:
                data = hfile.read()
        except:
            LOGGER.debug("task.get() no {}/status.json".
                         format(self.__dataif.rname("pro", self.__task_id)))
            return self.__toDict(self.STAT_PENDING)
        stat = json.loads(data)

        LOGGER.debug("task.get() {}".format(stat))
        if ('status' not in stat or
                'user_id' not in stat or
                'history_dir' not in stat or
                'hash' not in stat or
                'runtime_opts' not in stat or
                'exit_code' not in stat or
                'state_root' not in stat):
            LOGGER.error("task.get() bad {}/status.json: {}".
                         format(self.__dataif.rname("pro", self.__task_id), stat))
            raise Exception("Bad status.json")
        if (stat['status'] != self.STAT_PROGRESS and
                stat['status'] != self.STAT_SUCCESS and
                stat['status'] != self.STAT_FAILURE):
            LOGGER.error("task.get() bad status {} in {}/status.json".
                         format(stat['status'],
                                self.__dataif.rname("pro", self.__task_id)))
            raise Exception("Bad status in status.json")
        self.__stat = stat
        return self.__stat

    def wait(self, delay=2, timeout=AFC_ENG_TIMEOUT):
        LOGGER.debug("task.wait()")
        stat = None
        time0 = time.clock()
        while True:
            stat = self.get()
            LOGGER.debug("task.wait() status {}".format(stat['status']))
            if (stat['status'] == Task.STAT_SUCCESS or
                    stat['status'] == Task.STAT_FAILURE):
                return stat
            if (time. clock() - time0) > timeout:
                LOGGER.debug("task.wait() timeout")
                return self.__toDict(Task.STAT_PROGRESS)
            time.sleep(delay)
        LOGGER.debug("task.wait() exit")

    def ready(self, stat):
        return stat['status'] == Task.STAT_SUCCESS or \
            stat['status'] == self.STAT_FAILURE

    def successful(self, stat):
        return stat['status'] == Task.STAT_SUCCESS

    def __toDict(self, status, runtime_opts=None, exit_code=0):
        self.__stat['status'] = status
        self.__stat['runtime_opts'] = runtime_opts
        self.__stat['exit_code'] = exit_code
        return self.__stat

    def toJson(self, status, runtime_opts=None, exit_code=0):
        LOGGER.debug("toJson({})".format(status))
        data = json.dumps(self.__toDict(status, runtime_opts, exit_code))
        with self.__dataif.open("pro", self.__task_id + "/status.json") as hfile:
            LOGGER.debug("toJson() write {}".format(data))
            hfile.write(data)

    def forget(self):
        with self.__dataif.open("pro", self.__task_id + "/status.json") as hfile:
            hfile.delete()

    def getStat(self):
        return self.__stat

    def getDataif(self):
        return self.__dataif

    def getId(self):
        return self.__task_id