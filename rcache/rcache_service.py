""" Implementation of cache service activities """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, invalid-name, too-many-arguments
# pylint: disable=too-many-instance-attributes

import aiohttp
import asyncio
import http
import json
import math
import pydantic
from queue import Queue
from typing import Any, Dict, List, Optional, Tuple, Union

from rcache_common import dp, error_if, get_module_logger
from rcache_db_async import ReqCacheDbAsync
from rcache_models import AfcReqRespKey, RcacheUpdateReq, DbRecord, \
    RatapiAfcConfig, RatapiRulesetIds, RcacheInvalidateReq, LatLonRect, \
    RcacheSpatialInvalidateReq

__all__ = ["ReqCacheService"]

# Module logger
LOGGER = get_module_logger()

# Default maximum distance between FS ands AP in kilometers
DEFAULT_MAX_MAX_LINK_DISTANCE_KM = 130.

# Number of degrees per kilometer
DEGREES_PER_KM = 1 / (60 * 1.852)


class ReqCacheService:
    """ Manager of all server-side actions

    Arguments:
    _precompute_quota     -- Maximum number of precomputing requests in flight
    _afc_req_url          -- REST API URL to send requests for precomputation.
                             None for no precomputation
    _rulesets_url         -- REST API URL for getting list of active Ruleset
                             IDs. None to use default maximum AP FS distance
    _config_retrieval_url -- REST API URL for retrieving AFC Config by Ruleset
                             ID. None to use default maximum AP FS distance
    _db                   -- Database manager
    _update_queue         -- Queue for arrived update requests
    _invalidation_queue   -- Queue for arrived invalidation requests
    _precompute_event     -- Event that triggers precomputing task (set on
                             invalidations and update requests)
    _precomputer_session  -- Precomputer's aiohttp session for parallel update
                             requests
    """
    def __init__(self, rcache_db_dsn: str, precompute_quota: int,
                 afc_req_url: Optional[str], rulesets_url: Optional[str],
                 config_retrieval_url: Optional[str]) -> None:
        self._db = ReqCacheDbAsync(rcache_db_dsn)
        self._afc_req_url = afc_req_url
        self._rulesets_url = rulesets_url.rstrip("/") if rulesets_url else None
        self._config_retrieval_url = config_retrieval_url.rstrip("/") \
            if config_retrieval_url else None
        self._invalidation_queue: \
            Queue[Union[RcacheInvalidateReq, RcacheSpatialInvalidateReq]] = \
            asyncio.Queue()
        self._update_queue: Queue[AfcReqRespKey] = asyncio.Queue()
        self._precompute_event = asyncio.Event()
        self._precompute_quota = precompute_quota
        self._precomputer_session = \
            aiohttp.ClientSession(connector=aiohttp.TCPConnector(limit=0))
        asyncio.create_task(self._invalidator_task())
        asyncio.create_task(self._updater_task())
        asyncio.create_task(self._precomputer_task())

    def check_db_server(self) -> bool:
        """ Check if database server can be connected to """
        return self._db.check_server()

    async def connect_db(self, create_if_absent=False, recreate_db=False,
                   recreate_tables=False) -> None:
        """ Connect to database """
        if create_if_absent or recreate_db or recreate_tables:
            self._db.create_db(recreate_db=recreate_db,
                               recreate_tables=recreate_db)
        await self._db.connect()
        err = DbRecord.check_db_table(self._db.table)
        error_if(err, f"Request cache database has unexpected format: {err}")

    def disconnect_db(self) -> None:
        """ Disconnect from database """
        self._db.disconnect()

    def update(self, cache_update_req: RcacheUpdateReq) -> None:
        """ Enqueue arrived update requests """
        for rrk in cache_update_req.req_resp_keys:
            self._update_queue.put_nowait(rrk)

    def invalidate(
            self,
            invalidation_req: Union[RcacheInvalidateReq,
                                    RcacheSpatialInvalidateReq]) -> None:
        """ Enqueue arrived invalidation requests """
        self._invalidation_queue.put_nowait(invalidation_req)

    async def _updater_task(self) -> None:
        """ Cache updater task """
        while True:
            update_bulk: Dict[Tuple[Any, ...], Dict[str, Any]] = {}
            rrk = await self._update_queue.get()
            while True:
                try:
                    dr = DbRecord.from_req_resp_key(rrk)
                except pydantic.ValidationError as ex:
                    LOGGER.error(f"Invalid format of cache update data: {ex}")
                else:
                    if dr is not None:
                        row_dict = dr.dict()
                        update_bulk[self._db.get_pk(row_dict)] = row_dict
                if (len(update_bulk) == self._db.max_update_records()) or \
                        self._update_queue.empty():
                    break
                rrk = await self._update_queue.get()
            if update_bulk:
                await self._db.update_cache(list(update_bulk.values()))
                self._precompute_event.set()

    async def _invalidator_task(self) -> None:
        """ Cache invalidator task """
        while True:
            req = await self._invalidation_queue.get()
            if isinstance(req, RcacheInvalidateReq):
                if req.ruleset_ids is None:
                    await self._db.invalidate()
                else:
                    for ruleset_id in req.ruleset_ids:
                        await self._db.invalidate(ruleset_id)
            else:
                assert isinstance(req, RcacheSpatialInvalidateReq)
                max_link_distance_deg = \
                    await self._get_max_max_link_distance_deg()
                for rect in req.tiles:
                    lon_reduction = \
                        max(math.cos(
                            math.radians((rect.min_lat + rect.max_lat) / 2)),
                            1/180)
                    await self._db.spatial_invalidate(
                        LatLonRect(
                            min_lat=rect.min_lat - max_link_distance_deg,
                            max_lat=rect.max_lat + max_link_distance_deg,
                            min_lon=rect.min_lon -
                            max_link_distance_deg / lon_reduction,
                            max_lon=rect.max_lon +
                            max_link_distance_deg / lon_reduction))
            self._precompute_event.set()

    async def _precomputer_task(self) -> None:
        """ Precomputer task """
        if self._afc_req_url is None:
            return
        while True:
            await self._precompute_event.wait()
            self._precompute_event.clear()
            remaining_quota = \
                self._precompute_quota - await self._db.num_precomputing()
            if remaining_quota <= 0:
                continue
            invalid_reqs = \
                await self._db.get_invalid_reqs(limit=remaining_quota)
            if not invalid_reqs:
                continue
            self._precompute_event.set()
            for req in invalid_reqs:
                asyncio.create_task(
                    self._precomputer_session.post(
                        self._afc_req_url, json=json.loads(req)))

    async def _get_max_max_link_distance_deg(self) -> float:
        """ Retrieves maximum AP-FS distance in unit of latitude degrees """
        if (self._rulesets_url is not None) and \
                (self._config_retrieval_url is not None):
            ret: Optional[float] = None
            try:
                async with aiohttp.ClientSession() as session:
                    async with session.get(self._rulesets_url) as resp:
                        if resp.status != http.HTTPStatus.OK.value:
                            raise aiohttp.ClientError(
                                "Can't receive list of active configurations")
                        rulesets = \
                            RatapiRulesetIds.parse_obj(await resp.json())
                    for ruleset in rulesets.rulesetId:
                        async with session.get(
                                f"{self._config_retrieval_url}/{ruleset}") \
                                as resp:
                            if resp.status != http.HTTPStatus.OK.value:
                                continue
                            maxLinkDistance = \
                                RatapiAfcConfig.parse_obj(
                                    await resp.json()).maxLinkDistance
                        if (ret is None) or (maxLinkDistance > ret):
                            ret = maxLinkDistance
                if ret is not None:
                    return ret * DEGREES_PER_KM
            except aiohttp.ClientError as ex:
                LOGGER.error(f"Error retrieving maximum maxLinkDistance: {ex}")
            except pydantic.ValidationError as ex:
                LOGGER.error(f"Error decoding response: {ex}")
        LOGGER.error(f"Default maximum maxinkDistance of "
                     f"{DEFAULT_MAX_MAX_LINK_DISTANCE_KM}km will be used")
        return DEFAULT_MAX_MAX_LINK_DISTANCE_KM * DEGREES_PER_KM
