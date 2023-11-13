""" Synchronous part of AFC Request Cache database stuff """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, too-many-arguments, invalid-name

import pydantic
import sys
from typing import Dict, List, Optional, Union

from rcache_common import error, error_if, get_module_logger, \
    include_stack_to_error_log, set_error_exception

try:
    from rcache_db import RcacheDb
except ImportError:
    pass

try:
    from rcache_models import AfcReqRespKey, LatLonRect, RcacheClientSettings
    from rcache_rcache import RcacheRcache
except ImportError:
    pass

try:
    from rcache_rmq import RcacheRmq, RcacheRmqConnection
except ImportError:
    pass


__all__ = ["RcacheClient"]

LOGGER = get_module_logger()


class RcacheClient:
    """ Response Cache - related code that executes on other containers

    Private attributes:
    _rcache_db      -- Optional RcacheDb object - for making DB lookups (needed
                       on Message Handler and RatApi)
    _rcache_rmq     -- Optional RabbitMQ server AMQP URI (Needed on Message
                       Handler, RatApi, Worker)
    _rmq_receiver   -- True if for RabbitMQ receiver, False for transmitter,
                       None if irrelevant
    _rcache_rcache  -- Request Cache service base URL. Needed on a side that
                       makes cache update (i.e. either on Message Handler and
                       RatApi, or on Worker) or invalidate (FS Update for
                       spatial invalidate, some other entity for full/config
                       invalidate)
    _update_on_send -- True to update cache on sender
    """
    def __init__(self, client_settings: RcacheClientSettings,
                 rmq_receiver: Optional[bool] = None) -> None:
        """ Constructor

        Arguments:
        client_settings -- Rcache client settings
        rmq_receiver    -- True if sends responses to RabbitMQ, False if
                           receives responses from RabbitMQ, None if neither
        """
        set_error_exception(RuntimeError)
        include_stack_to_error_log(True)

        self._rcache_db: Optional[RcacheDb] = None
        if client_settings.enabled and \
                (client_settings.postgres_dsn is not None):
            assert "rcache_db" in sys.modules
            self._rcache_db = RcacheDb(client_settings.postgres_dsn)

        self._rcache_rmq: Optional[RcacheRmq] = None
        if client_settings.enabled and \
                (client_settings.rmq_dsn is not None):
            assert "rcache_rmq" in sys.modules
            error_if(rmq_receiver is None,
                     "'rmq_receiver' parameter should be specified")
            self._rcache_rmq = RcacheRmq(client_settings.rmq_dsn)
            self._rmq_receiver = rmq_receiver
        self._update_on_send = client_settings.update_on_send

        self._rcache_rcache: Optional[RcacheRcache] = None
        if client_settings.enabled and \
                (client_settings.service_url is not None):
            assert "rcache_rcache" in sys.modules
            self._rcache_rcache = RcacheRcache(client_settings.service_url)

    def lookup_responses(self, req_cfg_digests: List[str]) -> Dict[str, str]:
        """ Lookup responses in Postgres cache database

        Arguments:
        req_cfg_digests -- List of lookup keys (Request/Config digests in
                           string form)
        Returns dictionary of found responses, indexed by lookup keys
        """
        assert self._rcache_db is not None
        return self._rcache_db.lookup(req_cfg_digests, try_reconnect=True)

    def rmq_send_response(self, queue_name: str, req_cfg_digest: str,
                          request: str, response: Optional[str]) \
            -> None:
        """ Send AFC response to RabbitMQ

        Arguments:
        queue_name     -- Name of RabbitMQ queue to send response to
        req_cfg_digest -- Request/Config digest (request identifiers)
        request        -- Request as string
        response       -- Response as string. None on failure
        """
        assert self._rcache_rmq is not None
        with self._rcache_rmq.create_connection(tx_queue_name=queue_name) \
                as rmq_conn:
            rmq_conn.send_response(
                req_cfg_digest=req_cfg_digest,
                request=None if self._update_on_send else request,
                response=response)
        if self._update_on_send and response:
            assert self._rcache_rcache is not None
            try:
                self._rcache_rcache.update_cache(
                    [AfcReqRespKey(afc_req=request, afc_resp=response,
                                   req_cfg_digest=req_cfg_digest)])
            except pydantic.ValidationError as ex:
                error(f"Invalid arguments syntax: '{ex}'")

    def rmq_create_rx_connection(self) -> RcacheRmqConnection:
        """ Creates RcacheRmqConnection.

        Must be called before opposite side starts transmitting. Object being
        returned is a context manager (may be used with 'with', or should be
        explicitly closed with its close() method
        """
        assert self._rcache_rmq is not None
        return self._rcache_rmq.create_connection()

    def rmq_receive_responses(self, rx_connection: RcacheRmqConnection,
                              req_cfg_digests: List[str],
                              timeout_sec: float) -> Dict[str, Optional[str]]:
        """ Receiver ARC responses from RabbitMQ queue

        Arguments:
        rx_connection   -- Previously created
        req_cfg_digests -- List of expected request/config digests
        timeout_sec     -- RX timeout in seconds
        Returns dictionary of responses (as strings), indexed by request/config
        digests. Failed responses represented by Nones
        """
        assert self._rcache_rmq is not None
        rrks = rx_connection.receive_responses(req_cfg_digests=req_cfg_digests,
                                               timeout_sec=timeout_sec)
        assert rrks is not None
        to_update = \
            [AfcReqRespKey.from_orm(rrk) for rrk in rrks
             if (rrk.afc_req is not None) and (rrk.afc_resp is not None)]
        if to_update:
            assert self._rcache_rcache is not None
            self._rcache_rcache.update_cache(rrks=to_update)
        return {rrk.req_cfg_digest: rrk.afc_resp for rrk in rrks}

    def rcache_invalidate(
            self, ruleset_ids: Optional[Union[str, List[str]]] = None) -> None:
        """ Invalidate cache - completely or for given configs

        Arguments:
        ruleset_ids -- None for complete invalidation, ruleset_id or list
                       thereof for invalidation for given ruleset IDs
        """
        assert self._rcache_rcache is not None
        self._rcache_rcache.invalidate_cache(ruleset_ids=[ruleset_ids]
                                             if isinstance(ruleset_ids, str)
                                             else ruleset_ids)

    def rcache_spatial_invalidate(self, tiles: List["LatLonRect"]) -> None:
        """ Spatial invalidation

        Arguments:
        tiles -- List of latitude/longitude rectangles containing changed FSs
        """
        assert self._rcache_rcache is not None
        self._rcache_rcache.spatial_invalidate_cache(tiles=tiles)
