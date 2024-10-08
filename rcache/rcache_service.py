""" Implementation of cache service activities """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, invalid-name, too-many-arguments
# pylint: disable=too-many-instance-attributes, too-few-public-methods
# pylint: disable=too-many-nested-blocks

import aiohttp
import asyncio
import datetime
import http
import json
import logging
import math
import pydantic
from queue import Queue
import time
import traceback
from typing import Any, Dict, Optional, Set, Tuple, Union

import als
from log_utils import dp, error_if, get_module_logger
from rcache_db_async import RcacheDbAsync
from rcache_models import AfcReqRespKey, RcacheUpdateReq, ApDbPk, ApDbRecord, \
    FuncSwitch, RatapiAfcConfig, RatapiRulesetIds, RcacheInvalidateReq, \
    LatLonRect, RcacheSpatialInvalidateReq, RcacheStatus

__all__ = ["RcacheService"]

# Module logger
LOGGER = get_module_logger()
LOGGER.setLevel(logging.INFO)

# Default maximum distance between FS ands AP in kilometers
DEFAULT_MAX_MAX_LINK_DISTANCE_KM = 200.

# Number of degrees per kilometer
DEGREES_PER_KM = 1 / (60 * 1.852)

# Default length of averaging window
AVERAGING_WINDOW_SIZE = 10

# Maximum number of rows to invalidate at a time (None if all)
# Currently used for complete/ruleset invalidation and not for spatial one (as
# there is, probably, no need to)
INVALIDATION_CHUNK_SIZE = 1000


class Ema:
    """ Exponential moving average for some value or its rate

    Public attributes:
    ema -- Current average value

    Private attributes:
    _weight     -- Weighting factor (2/(window_length+1)
    _is_rate    -- True if rate is averaged, False if value
    _prev_value -- Previous value
    """

    def __init__(self, win_size: int, is_rate: bool) -> None:
        """ Constructor

        Arguments:
        win_length -- Window length
        is_rate    -- True for rate averaging, False for value averaging
        """
        self._weight = 2 / (win_size + 1)
        self._is_rate = is_rate
        self.ema: float = 0
        self._prev_value: float = 0

    def periodic_update(self, new_value: float) -> None:
        """ Update with new value """
        measured_value = new_value
        if self._is_rate:
            measured_value -= self._prev_value
        self._prev_value = new_value
        self.ema += self._weight * (measured_value - self.ema)


class RcacheService:
    """ Manager of all server-side actions

    Private attributes:
    _start_time                    -- When service started
    _precompute_quota              -- Maximum number of precomputing requests
                                      in flight
    _afc_req_url                   -- REST API URL to send requests for
                                      precomputation. None for no
                                      precomputation
    _rulesets_url                  -- REST API URL for getting list of active
                                      Ruleset IDs. None to use default maximum
                                      AP FS distance
    _config_retrieval_url          -- REST API URL for retrieving AFC Config by
                                      Ruleset ID. None to use default maximum
                                      AP FS distance
    _db                            -- Database manager
    _db_connected_event            -- Set after db connected
    _update_queue                  -- Queue for arrived update requests
    _invalidation_queue            -- Queue for arrived invalidation requests
    _precompute_event              -- Event that triggers precomputing task
                                      (set on invalidations and update
                                      requests)
    _main_tasks                    -- Set of top level tasks
    _precomputer_subtasks          -- References that keep individual
                                      precomputer tasks out of oblivion
    _updated_count                 -- Number of processed updates
    _precompute_count              -- Number of initiated precomputations
    _updated_rate_ema              -- Average rate of database write
    _update_queue_size_ema         -- Average length of update queue
    _precomputation_rate_ema       -- Average rate of initiated precomputations
    _all_tasks_running             -- True while no tasks crashed
    _schedule_lag_ema              -- Average scheduling delay
    """

    def __init__(self, rcache_db_dsn: str,
                 rcache_db_password_file: Optional[str],
                 precompute_quota: int, afc_req_url: Optional[str],
                 rulesets_url: Optional[str],
                 config_retrieval_url: Optional[str]) -> None:
        """ Constructor

        Arguments:
        rcache_db_dsn           -- Postgres database DSN
        rcache_db_password_file -- Optional name of secret credentials file for
                                   Postgres database
        precompute_quota        -- Maximum number of precomputing requests in
                                   flight
        afc_req_url             -- REST API URL to send requests for
                                   precomputation. None for no precomputation
        rulesets_url            -- REST API URL for getting list of active
                                   Ruleset IDs. None to use default maximum AP
                                   FS distance
        config_retrieval_url    -- REST API URL for retrieving AFC Config by
                                   Ruleset ID. None to use default maximum AP
                                   FS distance
        """
        als.als_initialize(client_id="rcache_sevice")
        self._start_time = datetime.datetime.now()
        self._db = \
            RcacheDbAsync(rcache_db_dsn=rcache_db_dsn,
                          rcache_db_password_file=rcache_db_password_file)
        self._db_connected_event = asyncio.Event()
        self._afc_req_url = afc_req_url
        self._rulesets_url = rulesets_url.rstrip("/") if rulesets_url else None
        self._config_retrieval_url = config_retrieval_url.rstrip("/") \
            if config_retrieval_url else None
        self._invalidation_queue: \
            Queue[Union[RcacheInvalidateReq, RcacheSpatialInvalidateReq]] = \
            asyncio.Queue()
        self._update_queue: Queue[AfcReqRespKey] = asyncio.Queue()
        self._precompute_event = asyncio.Event()
        self._precompute_event.set()
        self._precompute_quota = 0
        self.precompute_quota = precompute_quota
        self._main_tasks: Set[asyncio.Task] = set()
        for worker in (self._invalidator_worker, self._updater_worker,
                       self._precomputer_worker, self._averager_worker):
            self._main_tasks.add(asyncio.create_task(worker()))
        self._precomputer_subtasks: Set[asyncio.Task] = set()
        self._updated_count = 0
        self._precompute_count = 0
        self._updated_rate_ema = Ema(win_size=AVERAGING_WINDOW_SIZE,
                                     is_rate=True)
        self._update_queue_len_ema = Ema(win_size=AVERAGING_WINDOW_SIZE,
                                         is_rate=False)
        self._precomputation_rate_ema = Ema(win_size=AVERAGING_WINDOW_SIZE,
                                            is_rate=True)
        self._all_tasks_running = True
        self._schedule_lag_ema = Ema(win_size=AVERAGING_WINDOW_SIZE,
                                     is_rate=False)

    async def get_invalidation_enabled(self) -> bool:
        """ Current invalidation enabled state """
        return await self._db.get_switch(FuncSwitch.Invalidate)

    async def set_invalidation_enabled(self, value: bool) -> None:
        """ Enables/disables invalidation """
        self._log_invalidation(enabled=value)
        await self._db.set_switch(FuncSwitch.Invalidate, value)

    async def get_precomputation_enabled(self) -> bool:
        """ Current precomputation enabled state """
        return await self._db.get_switch(FuncSwitch.Precompute)

    async def set_precomputation_enabled(self, value: bool) -> None:
        """ Enables/disables precomputation """
        self._log_precomputation(enabled=value)
        await self._db.set_switch(FuncSwitch.Precompute, value)

    async def get_update_enabled(self) -> bool:
        """ Current update enabled state """
        return await self._db.get_switch(FuncSwitch.Update)

    async def set_update_enabled(self, value: bool) -> None:
        """ Enables/disables update """
        self._log_update(enabled=value)
        await self._db.set_switch(FuncSwitch.Update, value)

    @property
    def precompute_quota(self) -> int:
        """ Returns current precompute quota """
        return self._precompute_quota

    @precompute_quota.setter
    def precompute_quota(self, value: int) -> None:
        """ Sets precompute quota """
        self._log_precomputation(quota=value)
        error_if(value < 0, f"Precompute quota of {value} is invalid")
        self._precompute_quota = value

    def check_db_server(self) -> bool:
        """ Check if database server can be connected to """
        return self._db.check_server()

    def healthy(self) -> bool:
        """ Service is in healthy status """
        return self._all_tasks_running and \
            self._db_connected_event.is_set()

    async def connect_db(self, create_if_absent=False, recreate_db=False,
                         recreate_tables=False) -> None:
        """ Connect to database """
        if create_if_absent or recreate_db or recreate_tables:
            self._db.create_db(recreate_db=recreate_db,
                               recreate_tables=recreate_db)
        await self._db.connect()
        err = ApDbRecord.check_db_table(self._db.ap_table)
        error_if(err, f"Request cache database has unexpected format: {err}")
        self._db_connected_event.set()

    async def shutdown(self) -> None:
        """ Shut service down """
        while self._main_tasks:
            task = self._main_tasks.pop()
            task.cancel()
        while self._precomputer_subtasks:
            task = self._precomputer_subtasks.pop()
            task.cancel()
        await self._db.disconnect()

    def update(self, cache_update_req: RcacheUpdateReq) -> None:
        """ Enqueue arrived update requests """
        for rrk in cache_update_req.req_resp_keys:
            self._update_queue.put_nowait(rrk)

    def invalidate(
            self,
            invalidation_req: Union[RcacheInvalidateReq,
                                    RcacheSpatialInvalidateReq]) -> None:
        """ Enqueue arrived invalidation requests """
        if isinstance(invalidation_req, RcacheInvalidateReq):
            if invalidation_req.ruleset_ids is None:
                self._log_invalidation(invalidate_all=True)
            else:
                for ruleset_id in invalidation_req.ruleset_ids:
                    self._log_invalidation(invalidate_ruleset_id=ruleset_id)
        elif isinstance(invalidation_req, RcacheSpatialInvalidateReq):
            for tile in invalidation_req.tiles:
                self._log_invalidation(invalidate_tile=tile)
        self._invalidation_queue.put_nowait(invalidation_req)

    async def get_status(self) -> RcacheStatus:
        """ Returns service status """
        num_invalid_entries = await self._db.get_num_invalid_reqs() \
            if self._db_connected_event.is_set() else -1
        return \
            RcacheStatus(
                up_time=datetime.datetime.now() - self._start_time,
                db_connected=self._db_connected_event.is_set(),
                all_tasks_running=self._all_tasks_running,
                invalidation_enabled=await self.get_invalidation_enabled(),
                precomputation_enabled=await self.get_precomputation_enabled(),
                update_enabled=await self.get_update_enabled(),
                precomputation_quota=self._precompute_quota,
                num_valid_entries=(max(0,
                                       await self._db.get_cache_size() -
                                       num_invalid_entries))
                if self._db_connected_event.is_set() else -1,
                num_invalid_entries=num_invalid_entries,
                update_queue_len=self._update_queue.qsize(),
                update_count=self._updated_count,
                avg_update_write_rate=round(self._updated_rate_ema.ema, 2),
                avg_update_queue_len=round(self._update_queue_len_ema.ema, 2),
                num_precomputed=self._precompute_count,
                active_precomputations=len(self._precomputer_subtasks),
                avg_precomputation_rate=round(
                    self._precomputation_rate_ema.ema, 3),
                avg_schedule_lag=round(self._schedule_lag_ema.ema, 3))

    async def _updater_worker(self) -> None:
        """ Cache updater task worker """
        try:
            await self._db_connected_event.wait()
            while True:
                update_bulk: Dict[Tuple[Any, ...], Dict[str, Any]] = {}
                rrk = await self._update_queue.get()
                while True:
                    try:
                        dr = ApDbRecord.from_req_resp_key(rrk)
                    except pydantic.ValidationError as ex:
                        LOGGER.error(
                            f"Invalid format of cache update data: {ex}")
                    else:
                        if dr is not None:
                            row_dict = dr.dict()
                            update_bulk[self._db.get_ap_pk(row_dict)] = \
                                row_dict
                    if (len(update_bulk) == self._db.max_update_records()) or \
                            self._update_queue.empty():
                        break
                    rrk = await self._update_queue.get()
                if update_bulk and await self.get_update_enabled():
                    await self._db.update_cache(list(update_bulk.values()))
                    self._updated_count += len(update_bulk)
                    self._precompute_event.set()
        except asyncio.CancelledError:
            return
        except BaseException as ex:
            self._all_tasks_running = False
            LOGGER.error(f"Updater task unexpectedly aborted:\n"
                         f"{''.join(traceback.format_exception(ex))}")

    async def _invalidator_worker(self) -> None:
        """ Cache invalidator task worker """
        try:
            await self._db_connected_event.wait()
            while True:
                req = await self._invalidation_queue.get()
                while not await self.get_invalidation_enabled():
                    await asyncio.sleep(1)
                invalid_before = await self._report_invalidation()
                if isinstance(req, RcacheInvalidateReq):
                    if req.ruleset_ids is None:
                        while await self._db.invalidate(
                                limit=INVALIDATION_CHUNK_SIZE):
                            pass
                        await self._report_invalidation(
                            "Complete invalidation", invalid_before)
                    else:
                        for ruleset_id in req.ruleset_ids:
                            while await self._db.invalidate(
                                    ruleset_id, limit=INVALIDATION_CHUNK_SIZE):
                                pass
                            invalid_before = \
                                await self._report_invalidation(
                                    f"AFC Config for ruleset '{ruleset_id}' "
                                    f"invalidation", invalid_before)
                else:
                    assert isinstance(req, RcacheSpatialInvalidateReq)
                    max_link_distance_km = \
                        await self._get_max_max_link_distance_km()
                    max_link_distance_deg = \
                        max_link_distance_km * DEGREES_PER_KM
                    for rect in req.tiles:
                        lon_reduction = \
                            max(math.cos(
                                math.radians(
                                    (rect.min_lat + rect.max_lat) / 2)),
                                1 / 180)
                        await self._db.spatial_invalidate(
                            LatLonRect(
                                min_lat=rect.min_lat - max_link_distance_deg,
                                max_lat=rect.max_lat + max_link_distance_deg,
                                min_lon=rect.min_lon -
                                max_link_distance_deg / lon_reduction,
                                max_lon=rect.max_lon +
                                max_link_distance_deg / lon_reduction))
                        invalid_before = \
                            await self._report_invalidation(
                                f"Spatial invalidation for tile "
                                f"<{rect.short_str()}> with clearance of "
                                f"{max_link_distance_km}km",
                                invalid_before)
                self._precompute_event.set()
        except asyncio.CancelledError:
            return
        except BaseException as ex:
            self._all_tasks_running = False
            LOGGER.error(f"Invalidator task unexpectedly aborted:\n"
                         f"{''.join(traceback.format_exception(ex))}")

    async def _report_invalidation(
            self, dsc: Optional[str] = None,
            invalid_before: Optional[int] = None) -> int:
        """ Make a log record on invalidation, compute invalid count

        Argumentsa:
        dsc            -- Invalidation description. None to not make log print
        invalid_before -- Number of invalid records before operation. Might be
                          None if dsc is None
        Returns number of invalid records after operation
        """
        ret = await self._db.get_num_invalid_reqs()
        if dsc is not None:
            assert invalid_before is not None
            LOGGER.info(f"{dsc}: {invalid_before} was invalidated before "
                        f"operation, {ret} is invalidated after operation, "
                        f"increase of {ret - invalid_before}")
        return ret

    async def _single_precompute_worker(self, req: str) -> None:
        """ Single request precomputer subtask worker """
        try:
            async with aiohttp.ClientSession() as session:
                assert self._afc_req_url is not None
                async with session.post(self._afc_req_url,
                                        json=json.loads(req)) as resp:
                    if resp.ok:
                        return
            await self._db.delete(ApDbPk.from_req(req_str=req))
        except (asyncio.CancelledError,
                aiohttp.client_exceptions.ServerDisconnectedError):
            # Frankly, it's beyond my understanding why ServerDisconnectedError
            # happens before shutdown initiated by uvicorn in Compose
            # environment...)
            return
        except BaseException as ex:
            LOGGER.error(f"Precomputation subtask for request '{req}' "
                         f"unexpectedly aborted:\n"
                         f"{''.join(traceback.format_exception(ex))}")

    async def _precomputer_worker(self) -> None:
        """ Precomputer task worker """
        if self._afc_req_url is None:
            return
        try:
            await self._db_connected_event.wait()
            await self._db.reset_precomputations()
            while True:
                while not await self.get_precomputation_enabled():
                    await asyncio.sleep(1)
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
                    self._precompute_count += 1
                    task = \
                        asyncio.create_task(
                            self._single_precompute_worker(req))
                    self._precomputer_subtasks.add(task)
                    task.add_done_callback(self._precomputer_subtasks.discard)
        except asyncio.CancelledError:
            return
        except BaseException as ex:
            self._all_tasks_running = False
            LOGGER.error(f"Precomputer task unexpectedly aborted:\n"
                         f"{''.join(traceback.format_exception(ex))}")

    async def _get_max_max_link_distance_km(self) -> float:
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
                    return ret
            except aiohttp.ClientError as ex:
                LOGGER.error(f"Error retrieving maximum maxLinkDistance: {ex}")
            except pydantic.ValidationError as ex:
                LOGGER.error(f"Error decoding response: {ex}")
        LOGGER.error(f"Default maximum maxinkDistance of "
                     f"{DEFAULT_MAX_MAX_LINK_DISTANCE_KM}km will be used")
        return DEFAULT_MAX_MAX_LINK_DISTANCE_KM

    async def _averager_worker(self) -> None:
        """ Averager task worker """
        try:
            while True:
                timetag = time.time()
                await asyncio.sleep(1)
                self._schedule_lag_ema.periodic_update(
                    time.time() - timetag - 1)
                self._update_queue_len_ema.periodic_update(
                    self._update_queue.qsize())
                self._updated_rate_ema.periodic_update(self._updated_count)
                self._precomputation_rate_ema.periodic_update(
                    self._precompute_count)
        except asyncio.CancelledError:
            return
        except BaseException as ex:
            self._all_tasks_running = False
            LOGGER.error(f"Averager task unexpectedly aborted:\n"
                         f"{''.join(traceback.format_exception(ex))}")

    def _log_invalidation(
            self, enabled: Optional[bool] = None, invalidate_all: bool = False,
            invalidate_ruleset_id: Optional[str] = None,
            invalidate_tile: Optional[LatLonRect] = None) -> None:
        """ Make ALS log invalidation-related record

        Arguments:
        enabled               -- New invalidation state. None if unchanged
        invalidate_all        -- True fie invalidate-all request
        invalidate_ruleset_id -- Ruleset ID ti invalidate or None
        invalidate_tile       -- Tile to invalidate or None
        """
        als.als_json_log(
            "rcache_invalidation",
            {
                "enabled": enabled,
                "invalidate_all": invalidate_all,
                "invalidate_ruleset_id": invalidate_ruleset_id,
                "invalidate_rect":
                    {"min_lat": invalidate_tile.min_lat,
                     "max_lat": invalidate_tile.max_lat,
                     "min_lon": invalidate_tile.min_lon,
                     "max_lon": invalidate_tile.max_lon} if invalidate_tile
                    else None})

    def _log_update(self, enabled: Optional[bool] = None) -> None:
        """ Make ALS log update-related record """
        als.als_json_log("rcache_update", {"enabled": enabled})

    def _log_precomputation(self, enabled: Optional[bool] = None,
                            quota: Optional[int] = None) -> None:
        """ Make ALS log precomputation-related record

        Arguments:
        enabled -- New precomputation state. None if unchanged
        quota   - New precomputation quota. None if unchanged
        """
        als.als_json_log("rcache_precomputation",
                         {"enabled": enabled, "quota": quota})
