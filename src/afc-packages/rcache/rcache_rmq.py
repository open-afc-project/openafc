""" RabbitMQ sender and receiver """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, logging-fstring-interpolation
# pylint: disable=too-many-arguments, too-many-branches, too-many-nested-blocks
# pylint: disable=too-few-public-methods

import pika
import pydantic
import random
import string
from typing import cast, List, Optional, Set

from log_utils import error, get_module_logger
from rcache_models import RCACHE_RMQ_EXCHANGE_NAME, RmqReqRespKey
import db_utils

__all__ = ["RcacheRmq", "RcacheRmqConnection"]

LOGGER = get_module_logger()


class RcacheRmqConnection:
    """ Context manager to handle single read or write operation.

    Durable RabbitMQ connections are problematic, as they require responding to
    RMQ server's heartbeats and logistics of this without a separate thread is
    unclear. So single-shot connection and channel are created every time.

    Private attributes:
    _connection -- Pika connection adapter (corresponds to TCP connection to
                   RMQ server)
    _channel    -- Pika channel (corresponds to logical data stream)
    _for_rx     -- True for RX connection, False tot TX connection
    _queue_name -- Queue name
    """

    def __init__(self, url_params: pika.URLParameters,
                 tx_queue_name: Optional[str] = None) -> None:
        """ Constructor

        Arguments:
        url_params     -- RabbitMQ connection parameters, retrieved from URL
        tx_queue_name  -- Queue name for TX connection, None for RX connection
        """
        self._connection: Optional[pika.BlockingConnection] = None
        self._channel: \
            Optional[pika.adapters.blocking_connection.BlockingChannel] = None
        self._connection = pika.BlockingConnection(url_params)
        self._channel = self._connection.channel()
        self._channel.exchange_declare(exchange=RCACHE_RMQ_EXCHANGE_NAME,
                                       exchange_type="direct")
        self._for_rx = tx_queue_name is None
        if self._for_rx:
            self._queue_name = \
                "afc_response_queue_" + \
                "".join(random.choices(string.ascii_uppercase + string.digits,
                                       k=10))
            self._channel.queue_declare(queue=self._queue_name, exclusive=True)
            self._channel.queue_bind(queue=self._queue_name,
                                     exchange=RCACHE_RMQ_EXCHANGE_NAME)
        else:
            self._queue_name = cast(str, tx_queue_name)

    def rx_queue_name(self) -> str:
        """ Returns quyeue name for RX connection """
        assert self._for_rx
        return self._queue_name

    def send_response(self, req_cfg_digest: str, request: Optional[str],
                      response: Optional[str]) -> None:
        """ Send computed AFC Response

        Arguments:
        req_cfg_digest -- Request/config digest that identifies request
        request        -- Request as a string. None if cache update performed
                          by sender
        response       -- Response as a string. None on failure
        """
        assert not self._for_rx
        assert self._channel is not None
        ex: Exception
        try:
            self._channel.tx_select()
            self._channel.basic_publish(
                exchange=RCACHE_RMQ_EXCHANGE_NAME, routing_key=self._queue_name,
                body=RmqReqRespKey(
                    afc_req=request, afc_resp=response,
                    req_cfg_digest=req_cfg_digest).json(),
                properties=pika.BasicProperties(
                    content_type="application/json",
                    delivery_mode=pika.DeliveryMode.Transient),
                mandatory=False)
            self._channel.tx_commit()
        except pydantic.ValidationError as ex:
            error(f"Invalid arguments: {repr(ex)}")
        except pika.exceptions.AMQPError as ex:
            error(f"RabbitMQ send failed: {repr(ex)}")

    def receive_responses(self, req_cfg_digests: List[str],
                          timeout_sec: float) -> List[RmqReqRespKey]:
        """ Receive AFC responses

        Arguments:
        req_cfg_digests -- Request/config digests of expected responses
        timeout_sec     -- Timeout in seconds
        Returns list of request(optional)/response/digest triplets
        """
        assert self._for_rx
        assert self._connection is not None
        assert self._channel is not None
        remaining_responses: Set[str] = set(req_cfg_digests)
        ret: List[RmqReqRespKey] = []
        timer_id: Optional[int] = None
        ex: Exception
        try:
            if timeout_sec:
                timer_id = \
                    self._connection.call_later(timeout_sec,
                                                self._channel.cancel)
            body: bytes
            for _, _, body in \
                    self._channel.consume(
                        queue=self._queue_name, auto_ack=True, exclusive=True):
                try:
                    rrk = RmqReqRespKey.parse_raw(body)
                except pydantic.ValidationError as ex:
                    LOGGER.error(f"Decode error on AFC Response Info "
                                 f"arrived from Worker: {ex}")
                    continue
                if rrk.req_cfg_digest in remaining_responses:
                    remaining_responses.remove(rrk.req_cfg_digest)
                    ret.append(rrk)
                    if not remaining_responses:
                        self._channel.cancel()
            self._connection.remove_timeout(timer_id)
            return ret
        except pika.exceptions.AMQPError as ex:
            error(f"RabbitMQ receive failed: {repr(ex)}")
        return []  # Will never happen, appeasing pylint

    def close(self) -> None:
        """ Closing RabbitMQ connection """
        try:
            if self._for_rx and (self._channel is not None):
                self._channel.queue_delete(self._queue_name)
        except pika.exceptions.AMQPError:
            pass
        try:
            if self._connection is not None:
                self._connection.close()
                self._connection = None
                self._channel = None
        except pika.exceptions.AMQPError:
            pass

    def __enter__(self) -> "RcacheRmqConnection":
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.close()


class RcacheRmq:
    """ RabbitMQ synchronous sender/receiver

    Public attributes:
    rmq_dsn       -- RabbitMQ AMQP URI

    Private attributes:
    _url_params -- Connection parameters
    """

    def __init__(self, rmq_dsn: str) -> None:
        """ Constructor

        Arguments:
        rmq_dsn     -- RabbitMQ AMQP URI
        as_receiver -- True if will be used for RX, false if for TX
        """
        self.rmq_dsn = rmq_dsn
        try:
            self._url_params = pika.URLParameters(self.rmq_dsn)
        except pika.exceptions.AMQPError as ex:
            error(f"RabbitMQ URL '{db_utils.safe_dsn(self.rmq_dsn)}' has "
                  f"invalid syntax: {ex}")

    def create_connection(self, tx_queue_name: Optional[str] = None) \
            -> RcacheRmqConnection:
        """ Creates and returns connection context manager """
        return \
            RcacheRmqConnection(
                url_params=self._url_params, tx_queue_name=tx_queue_name)
