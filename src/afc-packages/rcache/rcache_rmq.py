""" RabbitMQ sender and receiver """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, logging-fstring-interpolation
# pylint: disable=too-many-arguments, too-many-branches, too-many-nested-blocks

import pika
import pydantic
import random
import string
from typing import List, NamedTuple, Optional, Set

from rcache_common import error, FailOnError, get_module_logger, safe_dsn
from rcache_models import RmqReqRespKey

__all__ = ["RcacheRmq"]

LOGGER = get_module_logger()


class RcacheRmq:
    """ RabbitMQ synchronous sender/receiver

    Public attributes:
    rmq_dsn       -- RabbitMQ AMQP URI
    rx_queue_name -- Queue name used for reception

    Private attributes:
    _connection -- Stuff pertinent to synchronous connection
    """
    # Exchange name
    EXCHANGE_NAME = "RcacheExchange"

    # Number of reconnection attempts if connection is lost
    RECONNECT_ATTEMPTS = 3

    # Stuff pertinent to synchronous connection
    _Connection = NamedTuple("_Connection",
                             [("connection", pika.BlockingConnection),
                              ("channel", pika.channel.Channel)])

    def __init__(self, rmq_dsn: str) -> None:
        """ Constructor

        Arguments:
        rmq_dsn -- RabbitMQ AMQP URI
        """
        self.rmq_dsn = rmq_dsn
        try:
            self._conn_params = pika.URLParameters(self.rmq_dsn)
        except pika.exceptions.AMQPError as ex:
            error(f"RabbitMQ URL '{safe_dsn(self.rmq_dsn)}' has invalid "
                  f"syntax: {ex}")
        self._connection: Optional["RcacheRmq._Connection"] = None
        self.rx_queue_name = \
            "afc_response_queue_" + \
            "".join(random.choices(string.ascii_uppercase + string.digits,
                                   k=10))

    def connect(self, as_receiver: bool, fail_on_error: bool = True) -> bool:
        """ Connect to RabbitMQ server

        Connection established automatically when
        send_response()/receive_responses() is called. This function allows to
        check connectability synchronously

        Arguments:
        as_receiver   -- True to connect as receiver, False if as transmitter
        fail_on_error -- True to fail on error, False to return False
        Returns True on success, False on known fail if fail_on_error is False
        """
        with FailOnError(fail_on_error):
            try:
                self._establish_connection(as_receiver=as_receiver)
            except pika.exceptions.AMQPError as ex:
                error(f"RabbitMQ connection failed: {ex}")
            return True
        return False

    def disconnect(self) -> None:
        """ Disconnect from server (if connected) """
        if not self._connection:
            return
        try:
            self._connection.connection.close()
        except pika.exceptions.AMQPError:
            pass
        self._connection = None

    def send_response(self, queue_name: str, req_cfg_digest: str,
                      request: Optional[str], response: Optional[str],
                      fail_on_error: bool = True) -> bool:
        """ Send computed AFC Response

        Arguments:
        queue_name     -- Name of queue to send response to
        req_cfg_digest -- Request/config digest that identifies request
        request        -- Request as a string. None if cache update performed
                          by sender
        response       -- Response as a string. None on failure
        fail_on_error  -- True to fail on error, False to return False
        Returns True on success, False on known fail if fail_on_error is False
        """
        with FailOnError(fail_on_error):
            for _ in range(self.RECONNECT_ATTEMPTS):
                pika_exception = None
                ex: Exception
                try:
                    self._establish_connection(as_receiver=False)
                    self._connection.channel.basic_publish(
                        exchange=self.EXCHANGE_NAME, routing_key=queue_name,
                        body=RmqReqRespKey(
                            afc_req=request, afc_resp=response,
                            req_cfg_digest=req_cfg_digest).json(),
                        properties=pika.BasicProperties(
                            content_type="application/json",
                            delivery_mode=pika.DeliveryMode.Transient),
                        mandatory=False)
                    return True
                except pydantic.ValidationError as ex:
                    error(f"Invalid arguments: {ex}")
                except (pika.exceptions.ConnectionClosedByBroker,
                        pika.exceptions.AMQPChannelError) as ex:
                    # Don't recover
                    pika_exception = ex
                    break
                except pika.exceptions.AMQPConnectionError as ex:
                    # Will try recovery
                    self._connection = None
                    pika_exception = ex
                except pika.exceptions.AMQPError as ex:
                    # Don't recover
                    self._connection = None
                    pika_exception = ex
                    break
            if not self._connection:
                error(f"RabbitMQ send failed: {pika_exception}")
        return False

    def receive_responses(self, req_cfg_digests: List[str],
                          timeout_sec: float, fail_on_error: bool = True) \
            -> Optional[List[RmqReqRespKey]]:
        """ Receive AFC responses

        Arguments:
        req_cfg_digests -- Request/config digests of expected responses
        timeout_sec     -- Timeout in seconds
        fail_on_error   -- True to fail on error, False to return None
        Returns On success - list of request(optional)/response/digest
        triplets. None on failure if fail_on_error was False
        """
        remaining_responses: Set[str] = set(req_cfg_digests)
        ret: List[RmqReqRespKey] = []
        timer_id: Optional[int] = None
        with FailOnError(fail_on_error):
            for _ in range(self.RECONNECT_ATTEMPTS):
                pika_exception = None
                ex: Exception
                try:
                    self._establish_connection(as_receiver=True)
                    assert self._connection is not None
                    if timeout_sec:
                        timer_id = \
                            self._connection.connection.call_later(
                                timeout_sec, self._connection.channel.cancel)
                    method: pika.spec.Basic.Deliver
                    properties: pika.spec.BasicProperties
                    body: bytes
                    for method, properties, body in \
                            self._connection.channel.consume(
                                queue=self.rx_queue_name, auto_ack=True,
                                exclusive=True):
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
                                self._connection.channel.cancel()
                    return ret
                except (pika.exceptions.ConnectionClosedByBroker,
                        pika.exceptions.AMQPChannelError) as ex:
                    # Don't recover
                    pika_exception = ex
                    break
                except pika.exceptions.AMQPConnectionError as ex:
                    # Will try recovery
                    self._connection = None
                    pika_exception = ex
                except pika.exceptions.AMQPError as ex:
                    # Don't recover
                    self._connection = None
                    pika_exception = ex
                    break
                finally:
                    if timer_id is not None:
                        try:
                            self._connection.connection.remove_timeout(
                                timer_id)
                        except pika.exceptions.AMQPError:
                            pass
                        timer_id = None
            if not self._connection:
                error(f"RabbitMQ receive failed: {pika_exception}")
        return None

    def _establish_connection(self, as_receiver: bool) -> None:
        """ Establish connection to RabbitMQ (if it was not established)

        Arguments:
        as_receiver -- True for message receiver, False for message sender
        """
        if self._connection is not None:
            return
        connection = pika.BlockingConnection(self._conn_params)
        channel = connection.channel()
        channel.exchange_declare(exchange=self.EXCHANGE_NAME,
                                 exchange_type="direct")
        if as_receiver:
            channel.queue_declare(queue=self.rx_queue_name, exclusive=True)
            channel.queue_bind(queue=self.rx_queue_name,
                               exchange=self.EXCHANGE_NAME)
        self._connection = self._Connection(connection=connection,
                                            channel=channel)
