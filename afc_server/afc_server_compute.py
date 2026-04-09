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
import secrets
import time
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
            "afc_response_queue_" + secrets.token_hex(5)
        self._request_futures: \
            Dict[str, Set["AfcServerCompute.FutureHolder"]] = {}
        self._celery_sender_tasks: Set[asyncio.Task] = set()
        self._rmq_reader_task = \
            asyncio.create_task(self._rmq_reader_worker(), name="RMQ Reader")
        self._stopping = False

    async def process_request(
            self, request_str: str, original_request_str: str, config_str: str,
            req_cfg_digest: str, runtime_opt: int, task_id: str,
            history_dir: Optional[str], deadline: float,
            low_priority: bool = False) -> Optional[str]:
        """ Process AFC Engine computation request

        Arguments:
        request_str          -- AFC Request as string
        original_request_str -- Original AFC Request as string (without added
                                vendor extensions)
        config_str           -- AFC Config as string
        req_cfg_digest       -- Hash of request and config
        runtime_opt          -- Value for --runtime_opt AFC Engine parameter
        task_id              -- Unique request ID
        history_dir          -- None or ObjStore history directory
        deadline             -- Deadline as seconds since Epoch
        low_priority         -- True for background precompute requests; Celery
                                task is dispatched at CELERY_PRIORITY_PRECOMPUTE
                                so interactive user requests jump ahead in queue
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
                            original_request_str=original_request_str,
                            config_str=config_str,
                            req_cfg_digest=req_cfg_digest,
                            runtime_opt=runtime_opt, task_id=task_id,
                            history_dir=history_dir, deadline=deadline,
                            low_priority=low_priority),
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
        # Wrap the channel loop in a retry-with-backoff outer loop.
        # A single ChannelInvalidStateError / ChannelClosed event
        # (broker restart, basic_cancel timeout under load) caused this task to
        # exit permanently, leaving _request_futures unsettled forever while
        # the healthcheck continued to return 200.
        _RETRY_BASE_SEC = 1.0
        _RETRY_MAX_SEC = 60.0
        _retry_delay = _RETRY_BASE_SEC
        while not self._stopping:
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
                    _retry_delay = _RETRY_BASE_SEC  # reset on successful connect
                    async with queue.iterator(no_ack=True) as queue_iter:
                        async for msg in queue_iter:
                            try:
                                rrk = \
                                    rcache_models.RmqReqRespKey.model_validate_json(msg.body)
                            except pydantic.ValidationError as ex:
                                LOGGER.error(f"Decode error on AFC Response Info "
                                             f"arrived from Worker: {ex}")
                                continue
                            # SUB-0138-13: a principal holding only the RMQ
                            # broker credential (not the objstore API key)
                            # can otherwise forge a message here that
                            # settles a pending AP request future with an
                            # arbitrary spectrum grant. resp_hmac is signed
                            # by the worker with the objstore API key (a
                            # distinct secret), so require it to verify.
                            if not rcache_models.verify_rmq_resp_hmac(
                                    rrk.req_cfg_digest, rrk.afc_resp,
                                    rrk.resp_hmac):
                                LOGGER.error(
                                    "Rejected RMQ response for digest %s: "
                                    "missing/invalid resp_hmac — message "
                                    "was not authenticated as coming from "
                                    "a holder of the objstore API key",
                                    rrk.req_cfg_digest)
                                continue
                            future_holders = \
                                self._request_futures.get(rrk.req_cfg_digest)
                            for future_holder in (future_holders or set()):
                                if not future_holder.future.done():
                                    future_holder.future.set_result(rrk.afc_resp)
            except asyncio.CancelledError:
                return
            except Exception as ex:
                if self._stopping:
                    LOGGER.debug(f"RMQ reader cleanup on shutdown: {ex}")
                    return
                LOGGER.warning(
                    f"RMQ reader error (will retry in {_retry_delay:.0f}s): {ex}")
                try:
                    await asyncio.sleep(_retry_delay)
                except asyncio.CancelledError:
                    return
                _retry_delay = min(_retry_delay * 2, _RETRY_MAX_SEC)

    def _send_req_to_celery(
            self, request_str: str, original_request_str: str, config_str: str,
            req_cfg_digest: str, runtime_opt: int, task_id: str,
            history_dir: Optional[str], deadline: float,
            low_priority: bool = False) -> None:
        """ Called on separate thread to AFC Engine request via Celery """
        if self._stopping:
            return
        priority = afc_worker.CELERY_PRIORITY_PRECOMPUTE \
            if low_priority else afc_worker.CELERY_PRIORITY_NORMAL
        try:
            if history_dir:
                for fname, content in [("analysisRequest.json", request_str),
                                       ("afc_config.json", config_str)]:
                    with self._dataif.open(os.path.join(history_dir, fname)) \
                            as hfile:
                        hfile.write(content.encode("utf-8"))
        except Exception as ex:
            LOGGER.error(f"Failed to write request to objstore: {ex}")
        # Write config to a deterministic objstore path so the worker reads it
        # from the operator trust domain (objstore) rather than the broker
        # message body.  Path matches afc_worker's config_path validation regex.
        config_path: Optional[str] = None
        try:
            config_path = f"/afc_config/{req_cfg_digest}/afc_config.json"
            with self._dataif.open(config_path) as hfile:
                hfile.write(config_str.encode("utf-8"))
        except Exception as ex:
            LOGGER.warning(
                f"Failed to write config to objstore ({config_path}): {ex}; "
                f"worker will fall back to broker-supplied config_str")
            config_path = None
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
                    "config_path": config_path,
                    "history_dir": history_dir,
                    "runtime_opts": runtime_opt,
                    "mntroot": self._worker_mnt_root,
                    "rcache_queue": self._rmq_rx_queue_name,
                    "request_str": request_str,
                    "original_request_str": original_request_str,
                    "config_str": config_str,
                    "deadline": deadline},
                priority=priority)
        except Exception as ex:
            error(f"Failed to send request to AFC Engine worker: {ex}")
