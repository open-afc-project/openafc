""" Sending AFC Requests to computation in AFC Engine """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, too-many-arguments
# pylint: disable=too-many-instance-attributes

import aio_pika
import asyncio
import os
import pydantic
import random
import string
import time
import traceback
from typing import Any, Dict, Optional, Set

import afc_worker
import fst
from log_utils import dp, error, error_if, get_module_logger
import rcache_models
import db_utils

# Logger for this module
LOGGER = get_module_logger()

__all__ = ["AfcServerCompute"]


class AfcServerCompute:
    """ Sender AFC Requests to computation in AFC Engine

    Private attributes:
    _rmq_dsn             -- AMQP DSN to RabbitMQ
    _rmq_password_file   -- Optional password file for AMQP DSN
    _engine_request_type -- --request-type parameter for AFC Engine
    _worker_mnt_root     -- --mnt-path parameter for AFC Engine
    _dataif              -- Interface to ObjStore
    _rmq_rx_queue_name   -- Name of RMQ queue Worker uses to send responses
    _request_futures     -- Per request hash collection of futures
                            (encapsulated into FutureHolder objects)
    _celery_sender_tasks -- Set of detached tasks, sending requests to celery
    _rmq_reader_task     -- Task that reads responses from RMQ queue
    _stopping            -- True if stopping initiated
    """
    class FutureHolder:
        """ Holder of async.Future that allows it to be put to set

        Public attributes:
        seq    -- Globally unique serial number
        future -- Future object
        """
        # Sequential number to use on next creation
        _next_seq = 0

        def __init__(self) -> None:
            """ Default constructor (allocates new serial number and future)
            """
            self.seq = AfcServerCompute.FutureHolder._next_seq
            AfcServerCompute.FutureHolder._next_seq += 1
            self.future: "asyncio.Future[Optional[str]]" = asyncio.Future()

        def __hash__(self) -> int:
            """ Hash value """
            return self.seq

        def __eq__(self, other: Any) -> bool:
            """ Equality comparison """
            return isinstance(other, self.__class__) \
                and (self.seq == other.seq)

    def __init__(self, rmq_dsn: str, rmq_password_file: Optional[str],
                 engine_request_type: str, worker_mnt_root: str) -> None:
        """ Constructor

        Arguments:
        rmq_dsn             -- AMQP DSN to RabbitMQ
        rmq_password_file   -- Optional password for AMQP DSN
        engine_request_type -- Value for --request-type AFC Engine parameter
        worker_mnt_root     -- Value for --mnt-path AFC Engine  parameter
        """
        self._rmq_dsn = rmq_dsn
        self._rmq_password_file = rmq_password_file
        self._engine_request_type = engine_request_type
        self._worker_mnt_root = worker_mnt_root
        self._dataif = fst.DataIf()
        self._rmq_rx_queue_name = \
            "afc_response_queue_" + \
            "".join(random.choices(string.ascii_uppercase + string.digits,
                                   k=10))
        self._request_futures: \
            Dict[str, Set["AfcServerCompute.FutureHolder"]] = {}
        self._celery_sender_tasks: Set[asyncio.Task] = set()
        self._rmq_reader_task = \
            asyncio.create_task(self._rmq_reader_worker(), name="RMQ Reader")
        self._stopping = False

    async def process_request(
            self, request_str: str, config_str: str, req_cfg_digest: str,
            runtime_opt: int, task_id: str, history_dir: Optional[str],
            deadline: float) -> Optional[str]:
        """ Process AFC Engine computation request

        Arguments:
        request_str    -- AFC Request as string
        config_str     -- AFC Config as string
        req_cfg_digest -- Hash of request and config
        runtime_opt    -- Value for --runtime_opt AFC Engine parameter
        task_id        -- Unique request ID
        history_dir    -- None or ObjStore history directory
        deadline       -- Deadline as seconds since Epoch
        Returns response (None in case of error) or generates TimeoutError
        """
        timeout = deadline - time.time()
        if timeout <= 0:
            raise TimeoutError()
        future_holder = AfcServerCompute.FutureHolder()
        try:
            future_holders = self._request_futures.get(req_cfg_digest)
            if future_holders is None:
                future_holders = {future_holder}
                self._request_futures[req_cfg_digest] = future_holders
                celery_sender_task = \
                    asyncio.create_task(
                        asyncio.to_thread(
                            self._send_req_to_celery, request_str=request_str,
                            config_str=config_str,
                            req_cfg_digest=req_cfg_digest,
                            runtime_opt=runtime_opt, task_id=task_id,
                            history_dir=history_dir, deadline=deadline),
                        name=f"Celery sender {future_holder.seq}")
                self._celery_sender_tasks.add(celery_sender_task)
                celery_sender_task.add_done_callback(
                    self._celery_sender_tasks.discard)
            else:
                future_holders.add(future_holder)
            await asyncio.wait_for(future_holder.future, timeout=timeout)
            return future_holder.future.result()
        finally:
            assert future_holders is not None
            future_holders.remove(future_holder)
            if not future_holders:
                del self._request_futures[req_cfg_digest]

    async def close(self) -> None:
        """ Gracefully stops/closes everything """
        self._stopping = True
        self._rmq_reader_task.cancel()
        await self._rmq_reader_task
        for future_holders in self._request_futures.values():
            for future_holder in future_holders:
                if not future_holder.future.done():
                    future_holder.future.cancel()

    async def _rmq_reader_worker(self) -> None:
        """ Worker function of RMQ reader task """
        try:
            full_dsn = \
                db_utils.substitute_password(
                    dsn=self._rmq_dsn, password_file=self._rmq_password_file,
                    optional=True)
            connection = await aio_pika.connect_robust(full_dsn)
            async with connection:
                channel = await connection.channel()
                exchange = \
                    await channel.declare_exchange(
                        name=rcache_models.RCACHE_RMQ_EXCHANGE_NAME,
                        type=aio_pika.ExchangeType.DIRECT)
                queue = \
                    await channel.declare_queue(
                        name=self._rmq_rx_queue_name, exclusive=True)
                await queue.bind(exchange)
                async with queue.iterator(no_ack=True) as queue_iter:
                    async for msg in queue_iter:
                        try:
                            rrk = \
                                rcache_models.RmqReqRespKey.parse_raw(msg.body)
                        except pydantic.ValidationError as ex:
                            LOGGER.error(f"Decode error on AFC Response Info "
                                         f"arrived from Worker: {ex}")
                            continue
                        error_if(rrk.afc_req,
                                 "RCACHE_UPDATE_ON_SEND must be unset or set "
                                 "to TRUE")
                        future_holders = \
                            self._request_futures.get(rrk.req_cfg_digest)
                        for future_holder in (future_holders or set()):
                            if not future_holder.future.done():
                                future_holder.future.set_result(rrk.afc_resp)
        except Exception as ex:
            for line in traceback.format_exception(ex):
                LOGGER.critical(line)
            error(f"Unhandled exception in RMQ reader worker task {ex}")

    def _send_req_to_celery(
            self, request_str: str, config_str: str, req_cfg_digest: str,
            runtime_opt: int, task_id: str, history_dir: Optional[str],
            deadline: float) -> None:
        """ Called on separate thread to AFC Engine request via Celery """
        if self._stopping:
            return
        try:
            if history_dir:
                for fname, content in [("analysisRequest.json", request_str),
                                       ("afc_config.json", config_str)]:
                    with self._dataif.open(os.path.join(history_dir, fname)) \
                            as hfile:
                        hfile.write(content.encode("utf-8"))
        except Exception as ex:
            LOGGER.error(f"Failed to write request to objstore: {ex}")
        try:
            prot, host, port = self._dataif.getProtocol()
            afc_worker.run.apply_async(
                kwargs={
                    "prot": prot,
                    "host": host,
                    "port": port,
                    "request_type": self._engine_request_type,
                    "task_id": task_id,
                    "hash_val": req_cfg_digest,
                    "config_path": None,
                    "history_dir": history_dir,
                    "runtime_opts": runtime_opt,
                    "mntroot": self._worker_mnt_root,
                    "rcache_queue": self._rmq_rx_queue_name,
                    "request_str": request_str,
                    "config_str": config_str,
                    "deadline": deadline})
        except Exception as ex:
            error(f"Failed to send request to AFC Engine worker: {ex}")
