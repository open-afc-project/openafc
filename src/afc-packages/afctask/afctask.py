# coding=utf-8

# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
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
import os

LOGGER = logging.getLogger(__name__)


class Task():
    """ Replacement for AsyncResult class and self serialization"""

    STAT_PENDING = "PENDING"
    STAT_PROGRESS = "PROGRESS"
    STAT_SUCCESS = "SUCCESS"
    STAT_FAILURE = "FAILURE"

    def __init__(self, task_id, dataif, hash_val=None, history_dir=None,
                 is_internal_request=False, owner_id=None):
        import re
        if not re.match(r'^[0-9a-fA-F-]{36}$', str(task_id)):
            raise ValueError("Invalid task_id format")
        LOGGER.debug(f"Task.__init__() {task_id}")
        self.__dataif = dataif
        self.__task_id = str(task_id)
        # If an existing status.json is present, read the owner_id from it so
        # subsequent toJson() calls (e.g. from the Celery worker updating status
        # to PROGRESS/SUCCESS) do not silently clear the ownership field that
        # was written at task-creation time.
        persisted_owner_id = owner_id
        if owner_id is None:
            try:
                fstatus = os.path.join("/responses", self.__task_id, "status.json")
                with self.__dataif.open(fstatus) as hfile:
                    existing = json.loads(hfile.read())
                    persisted_owner_id = existing.get('owner_id')
            except Exception:  # noqa: BLE001
                pass
        self.__stat = {
            'status': self.STAT_PENDING,
            'history_dir': history_dir,
            'hash': hash_val,
            'runtime_opts': None,
            'exit_code': 0,
            'is_internal_request': is_internal_request,
            'owner_id': persisted_owner_id,
        }

    def get(self):
        LOGGER.debug("Task.get()")
        data = None
        fstatus = os.path.join("/responses", self.__task_id, "status.json")
        try:
            with self.__dataif.open(fstatus) as hfile:
                data = hfile.read()
        except Exception:
            LOGGER.debug("task.get() no {}".format(
                self.__dataif.rname(fstatus)))
            return self.__toDict(self.STAT_PENDING)
        stat = json.loads(data)

        LOGGER.debug("task.get() {}".format(stat))
        if ('status' not in stat or
                'history_dir' not in stat or
                'hash' not in stat or
                'runtime_opts' not in stat or
                'exit_code' not in stat):
            LOGGER.error("task.get() bad status.json: {}".format(stat))
            raise ValueError("Bad status.json")
        # PENDING written by build_task before the Celery task is dispatched is
        # a valid "queued, not yet started" state.  Treat it the same as a
        # missing status.json so that wait() keeps polling instead of raising
        # immediately.  The priority queue ensures high-priority (USER) tasks
        # are dequeued before NORMAL tasks as soon as the worker is free.
        if stat['status'] == self.STAT_PENDING:
            LOGGER.debug("task.get() task still PENDING (queued, not started)")
            return self.__toDict(self.STAT_PENDING)
        if stat['status'] not in (self.STAT_PROGRESS, self.STAT_SUCCESS,
                                  self.STAT_FAILURE):
            LOGGER.error(
                "task.get() unexpected status %r in status.json", stat['status'])
            raise ValueError("Unexpected status in status.json: %r"
                             % stat['status'])
        self.__stat = stat
        return self.__stat

    def wait(self, timeout, delay=2):
        LOGGER.debug("Task.wait() timeout={timeout}")
        stat = None
        time0 = time.time()
        while True:
            time.sleep(delay)
            stat = self.get()
            LOGGER.debug("task.wait() status {}".format(stat['status']))
            if (stat['status'] == Task.STAT_SUCCESS or
                    stat['status'] == Task.STAT_FAILURE):
                return stat
            if stat['status'] == Task.STAT_PENDING:
                # Task is queued but the worker hasn't picked it up yet.
                # The priority queue will deliver high-priority (USER) tasks
                # before NORMAL tasks as soon as the current task finishes.
                LOGGER.debug("task.wait() task still queued (PENDING), "
                             "elapsed=%.1fs", time.time() - time0)
            if (time.time() - time0) > timeout:
                LOGGER.error("task.wait() timeout")
                return self.__toDict(Task.STAT_PROGRESS)
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
        fstatus = os.path.join("/responses", self.__task_id, "status.json")
        with self.__dataif.open(fstatus) as hfile:
            LOGGER.debug("toJson() write {}".format(data))
            hfile.write(data)

    def forget(self):
        fstatus = os.path.join("/responses", self.__task_id, "status.json")
        with self.__dataif.open(fstatus) as hfile:
            hfile.delete()

    def getStat(self):
        return self.__stat

    def getDataif(self):
        return self.__dataif

    def getId(self):
        return self.__task_id
