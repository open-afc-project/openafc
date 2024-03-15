#!/usr/bin/env python3
""" Tool for testing Rcache """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, invalid-name, too-many-arguments
# pylint: disable=too-many-instance-attributes, too-many-statements
# pylint: disable=too-many-locals, too-many-branches

import aiohttp
import asyncio
import argparse
import copy
import datetime
import hashlib
import json
import os
import random
import re
import sqlalchemy as sa
import sqlalchemy.ext.asyncio as sa_async
import sys
import tabulate
from typing import Any, Dict, List, Optional, Set, Tuple, Union
import urllib.parse

from rcache_common import dp, error, error_if
from rcache_models import AfcReqRespKey, ApDbRecord, ApDbRespState, \
    LatLonRect, RcacheInvalidateReq, RcacheSpatialInvalidateReq, \
    RcacheStatus, RcacheUpdateReq

# Environment variable with connection string to Postgres DB
POSTGRES_DSN_ENV = "RCACHE_POSTGRES_DSN"

# Environment variable with URL to Rcache service
RCACHE_URL_ENV = "RCACHE_SERVICE_URL"

# Environment variable for localhost's port of Rcache service
RCACHE_PORT_ENV = "RCACHE_CLIENT_PORT"

# Default number of simultaneous streams in mas operations
DEFAULT_MASS_THREADS = 10

# Default number of request in one AFC message
DEFAULT_BATCH_SIZE = 1

# Default number of lookups
DEFAULT_LOOKUP_COUNT = 1000000

# Default periodicity of status reports
DEFAULT_PERIODICITY = 1000

# SqlAlchemy asynchronous driver name
ASYNC_DRIVER_NAME = "asyncpg"

# Name of table in RCache
TABLE_NAME = "aps"

# Number of retries
RETRIES = 6


class RrkGen:
    """ Generator of request/response/key triplets """

    # Minimum latitude for generated coordinates
    MIN_LAT = 33

    # Maximum latitude for generated coordinates
    MAX_LAT = 48

    # Minimum longitude for generated coordinates
    MIN_LON = -116

    # Maximum longitude for generated coordinates
    MAX_LON = -95

    # Grid (number of points in one direction) of generated coordinates
    GRID_SIZE = 100000

    # AFC Request template
    REQUEST_TEMPLATE = json.loads("""{
  "availableSpectrumInquiryRequests": [
    {
      "inquiredChannels": [
        { "globalOperatingClass": 131 },
        { "globalOperatingClass": 132 },
        { "globalOperatingClass": 133 },
        { "globalOperatingClass": 134 },
        { "globalOperatingClass": 136 }
      ],
      "deviceDescriptor": {
        "serialNumber": "FSP43",
        "certificationId": [
          {"rulesetId": "US_47_CFR_PART_15_SUBPART_E", "id": "FCCID-FSP43"}
        ]
      },
      "inquiredFrequencyRange": [
        {"lowFrequency": 5925, "highFrequency": 6425},
        {"lowFrequency": 6525, "highFrequency": 6875}
      ],
      "location": {
        "indoorDeployment": 2,
        "elevation": {
          "verticalUncertainty": 10,
          "heightType": "AGL",
          "height": 83
        },
        "ellipse": {
          "center": {"latitude": 39.792935, "longitude": -105.018517},
          "orientation": 45,
          "minorAxis": 50,
          "majorAxis": 50
        }
      },
      "requestId": "0"
    }
  ],
  "version": "1.4"
}""")

    # AFC Response template
    RESPONSE_TEMPLATE = json.loads("""{
  "availableSpectrumInquiryResponses": [
    {
      "availabilityExpireTime": "2023-08-11T16:45:44Z",
      "availableChannelInfo": [
        {"channelCfi": [],
         "globalOperatingClass": 131,
         "maxEirp": []},
        {"channelCfi": [3, 11, 19, 27, 35, 43, 51, 59, 67],
         "globalOperatingClass": 132,
         "maxEirp": [-2.3, -2.3, 19.7, 34.8, 34.4, 34.5, 17.1, 31.2, 25.8]},
        {"channelCfi": [7, 23, 39, 55, 71, 87, 135, 151, 167 ],
         "globalOperatingClass": 133,
         "maxEirp": [0.7, 21.8, 36, 20.2, 28.9, 36, 27.3, 32.5, 20.1]},
        {"channelCfi": [15, 47, 79, 143],
         "globalOperatingClass": 134,
         "maxEirp": [3.8, 23.1, 32, 30.4]},
        {"channelCfi": [2],
         "globalOperatingClass": 136,
         "maxEirp": [22.4]}
      ],
      "availableFrequencyInfo": [
        {"frequencyRange": {"highFrequency": 5945, "lowFrequency": 5925},
         "maxPsd": 9.4},
        {"frequencyRange": {"highFrequency": 5965, "lowFrequency": 5945},
         "maxPsd": -18.4},
        {"frequencyRange": {"highFrequency": 6025, "lowFrequency": 5965},
         "maxPsd": -18.3},
        {"frequencyRange": {"highFrequency": 6045, "lowFrequency": 6025},
         "maxPsd": 7.9},
        {"frequencyRange": {"highFrequency": 6065, "lowFrequency": 6045},
         "maxPsd": 8},
        {"frequencyRange": {"highFrequency": 6085, "lowFrequency": 6065},
         "maxPsd": 18.8},
        {"frequencyRange": {"highFrequency": 6105, "lowFrequency": 6085},
         "maxPsd": 22.9},
        {"frequencyRange": {"highFrequency": 6125, "lowFrequency": 6105},
         "maxPsd": 18.3},
        {"frequencyRange": {"highFrequency": 6165, "lowFrequency": 6125},
         "maxPsd": 18.4},
        {"frequencyRange": {"highFrequency": 6185, "lowFrequency": 6165},
         "maxPsd": 18.5},
        {"frequencyRange": {"highFrequency": 6205, "lowFrequency": 6185},
         "maxPsd": 1.1},
        {"frequencyRange": {"highFrequency": 6245, "lowFrequency": 6205},
         "maxPsd": 15.1},
        {"frequencyRange": {"highFrequency": 6265, "lowFrequency": 6245},
         "maxPsd": 15.2},
        {"frequencyRange": {"highFrequency": 6305, "lowFrequency": 6265},
         "maxPsd": 9.8},
        {"frequencyRange": {"highFrequency": 6325, "lowFrequency": 6305},
         "maxPsd": 20.9},
        {"frequencyRange": {"highFrequency": 6345, "lowFrequency": 6325},
         "maxPsd": 21},
        {"frequencyRange": {"highFrequency": 6425, "lowFrequency": 6345},
         "maxPsd": 22.9},
        {"frequencyRange": {"highFrequency": 6565, "lowFrequency": 6525},
         "maxPsd": 22.9},
        {"frequencyRange": {"highFrequency": 6585, "lowFrequency": 6565},
         "maxPsd": 21.4},
        {"frequencyRange": {"highFrequency": 6605, "lowFrequency": 6585},
         "maxPsd": 21.5},
        {"frequencyRange": {"highFrequency": 6625, "lowFrequency": 6605},
         "maxPsd": 8.2},
        {"frequencyRange": {"highFrequency": 6645, "lowFrequency": 6625},
         "maxPsd": 8.3},
        {"frequencyRange": {"highFrequency": 6665, "lowFrequency": 6645},
         "maxPsd": 11.2},
        {"frequencyRange": {"highFrequency": 6685, "lowFrequency": 6665},
         "maxPsd": 13.4},
        {"frequencyRange": {"highFrequency": 6705, "lowFrequency": 6685},
         "maxPsd": 22.9},
        {"frequencyRange": {"highFrequency": 6725, "lowFrequency": 6705},
         "maxPsd": 19.3},
        {"frequencyRange": {"highFrequency": 6765, "lowFrequency": 6725},
         "maxPsd": 15.6},
        {"frequencyRange": {"highFrequency": 6805, "lowFrequency": 6765},
         "maxPsd": 12.5},
        {"frequencyRange": {"highFrequency": 6845, "lowFrequency": 6805},
         "maxPsd": 1.2},
        {"frequencyRange": {"highFrequency": 6865, "lowFrequency": 6845},
         "maxPsd": 22.9}
      ],
      "requestId": "0",
      "response": {"responseCode": 0, "shortDescription": "Success"},
      "rulesetId": "US_47_CFR_PART_15_SUBPART_E"
    }
  ],
  "version": "1.4"
}""")

    # Response template stripped of variable fields (filled on first use)
    _STRIPPED_RESPONSE: Optional[Dict[str, Any]] = None

    # 20MHz channels to chose from
    CHANNELS = [1, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 61,
                65, 69, 73, 77, 81, 85, 89, 93, 117, 121, 125, 129, 133, 137,
                141, 145, 149, 153, 157, 161, 165, 169, 173, 177, 181]

    @classmethod
    def rrk(cls, idx: int) -> AfcReqRespKey:
        """ For given request index generates correspondent AfcReqRespKey
        object """
        req_msg = copy.deepcopy(cls.REQUEST_TEMPLATE)
        req = req_msg["availableSpectrumInquiryRequests"][0]
        req["deviceDescriptor"]["serialNumber"] = "RCACHE_TOOL" + str(idx)
        req["location"]["ellipse"]["center"] = \
            {"latitude":
             (cls.MIN_LAT +
              (idx // cls.GRID_SIZE) * (cls.MAX_LAT - cls.MIN_LAT) /
              cls.GRID_SIZE),
             "longitude":
             (cls.MIN_LON +
              (idx % cls.GRID_SIZE) * (cls.MAX_LON - cls.MIN_LON) /
              cls.GRID_SIZE)}
        resp_msg = copy.deepcopy(cls.RESPONSE_TEMPLATE)
        resp = resp_msg["availableSpectrumInquiryResponses"][0]
        resp["availableChannelInfo"][0]["channelCfi"], \
            resp["availableChannelInfo"][0]["maxEirp"] = \
            cls._resp_channels(idx)
        return AfcReqRespKey(afc_req=json.dumps(req_msg),
                             afc_resp=json.dumps(resp_msg),
                             req_cfg_digest=cls.lookup_key(idx))

    @classmethod
    def lookup_key(cls, idx: int) -> str:
        """ For given request index generates search key """
        md5 = hashlib.md5()
        md5.update(str(idx).encode("ascii"))
        return md5.hexdigest()

    @classmethod
    def validate_response(cls, idx: int, resp: str) -> bool:
        """ True if given response matches given request index """
        resp_dict = json.loads(resp)
        if (resp_dict["availableSpectrumInquiryResponses"]["channelCfi"],
                resp_dict["availableSpectrumInquiryResponses"]["maxEirp"]) != \
                cls._resp_channels(idx):
            return False
        if cls._STRIPPED_RESPONSE is None:
            cls._STRIPPED_RESPONSE = copy.deepcopy(cls.RESPONSE_TEMPLATE)
            cls._resp_strip(cls._STRIPPED_RESPONSE)
        if cls._resp_strip(resp_dict) != cls._STRIPPED_RESPONSE:
            return False
        return True

    @classmethod
    def _resp_channels(cls, idx: int) -> Tuple[List[int], List[float]]:
        """ Response channels for given request index """
        channels: List[int] = []
        bit_idx = 0
        while idx:
            if idx & 1:
                channels.append(cls.CHANNELS[bit_idx])
            idx //= 2
            bit_idx += 1
        return (channels, [32.] * len(channels))

    @classmethod
    def _resp_strip(cls, resp_msg_dict: Dict[str, Any]) -> Dict[str, Any]:
        """ Strips variable fields of response object """
        resp_dict = resp_msg_dict["availableSpectrumInquiryResponses"][0]
        del resp_dict["availableChannelInfo"][0]["channelCfi"]
        del resp_dict["requestId"]
        return resp_msg_dict


class Reporter:
    """ Progress reporter

    Public attributes:
    start_time    -- Start datetime.datetime
    success_count -- Number of successful requests
    fail_count    -- Number of failed requests

    Private attributes:
    _total_count    -- Total count of requests that will be performed
    _periodicity    -- Report periodicity (e.g. 1000 - once in 1000 bumps)
    _last_print_len -- Length of last single-line print
    """
    def __init__(self, total_count: Optional[int] = None,
                 periodicity: int = DEFAULT_PERIODICITY) -> None:
        """ Constructor

        total_count -- Total count of requests that will be performed
        periodicity -- Report periodicity (e.g. 1000 - once in 1000 bumps)
        """
        self.start_time = datetime.datetime.now()
        self.success_count = 0
        self.fail_count = 0
        self._total_count = total_count
        self._periodicity = periodicity
        self._last_print_len = 0

    def bump(self, success: bool = True) -> None:
        """ Increment success or fail count """
        if success:
            self.success_count += 1
        else:
            self.fail_count += 1
        if ((self.success_count + self.fail_count) % self._periodicity) == 0:
            self.report(newline=False)

    def report(self, newline: bool = True) -> None:
        """ Make a report print

        Arguments:
        newline -- True to go to next line, False to print same line as before
        """
        completed = self.success_count + self.fail_count
        msg = f"{completed} completed "
        if self.fail_count:
            msg += f"(of them {self.fail_count} " \
                f"({self.fail_count * 100 / completed:0.2f}%) failed). "
        if self._total_count is not None:
            msg += f"{completed * 100 / self._total_count:.2f}%. "
        ts: Union[int, float] = \
            (datetime.datetime.now() - self.start_time).total_seconds()
        rate = completed / ts
        seconds = ts % 60
        ts = int(ts) // 60
        minutes = ts % 60
        hours = ts // 60
        if hours:
            msg += f"{hours}:"
        if hours or minutes:
            msg += f"{minutes:02}:"
        msg += f"{seconds:05.2f} elapsed ({rate:.2f} requests per second)"
        print(msg + (" " * min(0, self._last_print_len - len(msg))),
              end="\n" if newline else "\r", flush=True)
        self._last_print_len = 0 if newline else len(msg)


def rcache_url(args: Any, path: str) -> str:
    """ Returns Rcache REST URL

    Arguments:
    args -- Parsed command line arguments
    path -- Path inside teh service
    Returns URL
    """
    return urllib.parse.urljoin(args.rcache, path)


async def fill_worker(args: Any, reporter: Reporter,
                      req_queue: asyncio.Queue[Optional[int]]) -> None:
    """ Database fill worker

    Arguments:
    args       -- Parsed command line arguments
    reporter   -- Progress reporter object
    req_queue  -- Queue with requests
    """
    end = False
    batch: List[AfcReqRespKey] = []
    while not end:
        item = await req_queue.get()
        if item is None:
            end = True
        else:
            batch.append(RrkGen.rrk(item))
        if len(batch) < (1 if end else args.batch):
            continue
        update_req = RcacheUpdateReq(req_resp_keys=batch).dict()
        errmsg: Optional[str] = None
        if (not args.dry) or args.dry_remote:
            backoff_window = 1
            for _ in range(RETRIES):
                errmsg = None
                try:
                    async with aiohttp.ClientSession(trust_env=True) \
                            as session:
                        if args.dry_remote:
                            async with session.get(rcache_url(args,
                                                              "healthcheck")) \
                                    as resp:
                                if resp.ok:
                                    break
                                errmsg = f"{await resp.text()}. " \
                                    f"Status={resp.status}. " \
                                    f"Reason={resp.reason}"
                        else:
                            async with session.post(rcache_url(args, "update"),
                                                    json=update_req) as resp:
                                if resp.ok:
                                    break
                                errmsg = f"{await resp.text()}. " \
                                    f"Status={resp.status}. " \
                                    f"Reason={resp.reason}"
                except aiohttp.ClientError as ex:
                    errmsg = str(ex)
                    await asyncio.sleep(random.random() * backoff_window)
                    backoff_window *= 2
        for _ in range(len(batch)):
            reporter.bump(success=errmsg is None)
        batch = []
        if errmsg:
            print(errmsg)


async def do_mass_fill(args: Any) -> None:
    """ Execute "mass_fill" command.

    Arguments:
    args -- Parsed command line arguments
    """
    reporter = Reporter(total_count=args.max_idx - args.min_idx)
    req_queue: asyncio.Queue[Optional[int]] = asyncio.Queue()
    async with asyncio.TaskGroup() as tg:
        for _ in range(args.threads):
            tg.create_task(
                fill_worker(args=args, reporter=reporter, req_queue=req_queue))
        for idx in range(args.min_idx, args.max_idx):
            await req_queue.put(idx)
        for _ in range(args.threads):
            await req_queue.put(None)
    reporter.report()


async def lookup_worker(postgres_dsn: str, metadata: Optional[sa.MetaData],
                        min_idx: int, max_idx: int, batch_size: int,
                        count: int, dry: bool, reporter: Reporter) -> None:
    """ Lookup worker

    Arguments:
    postgres_dsn -- Postgres connection string
    min_idx      -- Minimum request index
    max_idx      -- Aftermaximum request index
    batch_size   -- Number of requests per message
    count        -- Number of lookups to perform
    dry          -- Don't do actual database operations
    reporter     -- Progress reporter object
    """
    dsn_parts = urllib.parse.urlsplit(postgres_dsn)
    async_dsn = \
        urllib.parse.urlunsplit(
            dsn_parts._replace(
                scheme=f"{dsn_parts.scheme}+{ASYNC_DRIVER_NAME}"))
    async_engine = None if dry else sa_async.create_async_engine(async_dsn)
    dry_result = \
        [ApDbRecord.from_req_resp_key(RrkGen.rrk(idx)).dict()
         for idx in range(batch_size)] if dry else None
    done = 0
    while done < count:
        bs = min(batch_size, count - done)
        batch: Set[str] = set()
        while len(batch) < bs:
            batch.add(RrkGen.lookup_key(random.randrange(min_idx, max_idx)))
        errmsg: Optional[str] = None
        if dry:
            assert dry_result is not None
            result = \
                [ApDbRecord.parse_obj(dry_result[idx]).get_patched_response()
                 for idx in range(len(batch))]
        else:
            assert async_engine is not None
            assert metadata is not None
            table = metadata.tables[TABLE_NAME]
            backoff_window = 1
            for _ in range(RETRIES):
                errmsg = None
                try:
                    s = sa.select(table).\
                        where((table.c.req_cfg_digest.in_(list(batch))) &
                              (table.c.state == ApDbRespState.Valid.name))
                    async with async_engine.connect() as conn:
                        rp = await conn.execute(s)
                    result = [ApDbRecord.parse_obj(rec).get_patched_response()
                              for rec in rp]
                    break
                except sa.exc.SQLAlchemyError as ex:
                    errmsg = str(ex)
                    result = []
                    await asyncio.sleep(random.random() * backoff_window)
                    backoff_window *= 2
        for _ in range(len(result)):
            reporter.bump()
        for _ in range(len(batch) - len(result)):
            reporter.bump(success=False)
        if errmsg:
            print(errmsg)
        done += len(batch)


async def do_mass_lookup(args: Any) -> None:
    """ Execute "mass_lookup" command.

    Arguments:
    args -- Parsed command line arguments
    """
    per_worker_count = args.count // args.threads
    reporter = Reporter(total_count=per_worker_count * args.threads)
    metadata: Optional[sa.MetaData] = None
    if not args.dry:
        engine = sa.create_engine(args.postgres)
        metadata = sa.MetaData()
        metadata.reflect(bind=engine)
        engine.dispose()
    async with asyncio.TaskGroup() as tg:
        for _ in range(args.threads):
            tg.create_task(
                lookup_worker(
                    postgres_dsn=args.postgres, metadata=metadata,
                    dry=args.dry, min_idx=args.min_idx, max_idx=args.max_idx,
                    batch_size=args.batch, count=per_worker_count,
                    reporter=reporter))
    reporter.report()


async def do_invalidate(args: Any) -> None:
    """ Execute "invalidate" command.

    Arguments:
    args -- Parsed command line arguments
    """
    invalidate_req: Dict[str, Any] = {}
    path: str = ""
    if args.enable or args.disable:
        error_if(args.all or args.tile or args.ruleset or
                 (args.enable and args.disable),
                 "Incompatible parameters")
        path = f"invalidation_state/{json.dumps(bool(args.enable))}"
    elif args.all:
        error_if(args.tile or args.ruleset, "Incompatible parameters")
        invalidate_req = RcacheInvalidateReq().dict()
        path = "invalidate"
    elif args.ruleset:
        error_if(args.tile, "Incompatible parameters")
        invalidate_req = RcacheInvalidateReq(ruleset_ids=args.ruleset).dict()
        path = "invalidate"
    elif args.tile:
        tiles: List[LatLonRect] = []
        for s in args.tile:
            m = \
                re.search(
                    r"^(?P<min_lat>[0-9.+-]+),(?P<min_lon>[0-9.+-]+)"
                    r"(,(?P<max_lat>[0-9.+-]+)(,(?P<max_lon>[0-9.+-]+))?)?$",
                    s)
            error_if(not m, f"Tile specification '{s}' has invalid format")
            assert m is not None
            try:
                min_lat = float(m.group("min_lat"))
                min_lon = float(m.group("min_lon"))
                max_lat = float(m.group("max_lat")) if m.group("max_lat") \
                    else (min_lat + 1)
                max_lon = float(m.group("max_lon")) if m.group("max_lon") \
                    else (min_lon + 1)
            except ValueError:
                error(not m, f"Tile specification '{s}' has invalid format")
            tiles.append(
                LatLonRect(min_lat=min_lat, min_lon=min_lon, max_lat=max_lat,
                           max_lon=max_lon))
        invalidate_req = RcacheSpatialInvalidateReq(tiles=tiles).dict()
        path = "spatial_invalidate"
    else:
        error("No invalidation type parameters specified")
    async with aiohttp.ClientSession() as session:
        kwargs = {}
        if invalidate_req:
            kwargs["json"] = invalidate_req
        async with session.post(rcache_url(args, path), **kwargs) as resp:
            error_if(not resp.ok,
                     f"Operation failed: {await resp.text()}")


async def do_precompute(args: Any) -> None:
    """ Execute "precompute" command.

    Arguments:
    args -- Parsed command line arguments
    """
    if args.enable or args.disable:
        error_if((args.quota is not None) or (args.enable and args.disable),
                 "Only one parameter should be specified")
        path = "precomputation_state"
        value = bool(args.enable)
    elif args.quota is not None:
        path = "precomputation_quota"
        value = args.quota
    else:
        error("At least one parameter should be specified")
    async with aiohttp.ClientSession() as session:
        async with session.post(rcache_url(args, f"{path}/{value}")) as resp:
            error_if(not resp.ok, f"Operation failed: {await resp.text()}")


async def do_update(args: Any) -> None:
    """ Execute "update" command.

    Arguments:
    args -- Parsed command line arguments
    """
    error_if(args.enable == args.disable,
             "Exactly one parameter should be specified")
    async with aiohttp.ClientSession() as session:
        async with session.post(
                rcache_url(args, f"update_state/{bool(args.enable)}")) as resp:
            error_if(not resp.ok, f"Operation failed: {await resp.text()}")


async def do_status(args: Any) -> None:
    """ Execute "status" command.

    Arguments:
    args -- Parsed command line arguments
    """
    while True:
        async with aiohttp.ClientSession() as session:
            async with session.get(rcache_url(args, "status")) as resp:
                status = RcacheStatus.parse_obj(await resp.json())
        print(tabulate.tabulate(status.dict().items(),
                                tablefmt="plain", colalign=("left", "right")))
        if args.interval is None:
            break
        await asyncio.sleep(args.interval)


def do_help(args: Any) -> None:
    """ Execute "help" command.

    Arguments:
    args -- Parsed command line arguments (also contains 'argument_parser' and
            'subparsers' fields)
    """
    if args.subcommand is None:
        args.argument_parser.print_help()
    else:
        args.subparsers.choices[args.subcommand].print_help()


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    default_postgres = os.environ.get(POSTGRES_DSN_ENV)

    default_rcache = os.environ.get(RCACHE_URL_ENV)
    if (default_rcache is None) and (RCACHE_PORT_ENV in os.environ):
        default_rcache = f"http://localhost:{os.environ[RCACHE_PORT_ENV]}"

    switches_mass = argparse.ArgumentParser(add_help=False)
    switches_mass.add_argument(
        "--min_idx", metavar="MIN_IDX", default=0, type=int,
        help="Minimum request index for mass fill/lookup operation. Default "
        "is 0")
    switches_mass.add_argument(
        "--max_idx", metavar="MAX_IDX", required=True, type=int,
        help="Post-maximum request index for mass fill/lookup operation. This "
        "parameter is mandatory")
    switches_mass.add_argument(
        "--threads", metavar="NUM_THREADS", default=DEFAULT_MASS_THREADS,
        type=int,
        help=f"Number of parallel threads during mass operation. Default is "
        f"{DEFAULT_MASS_THREADS}")
    switches_mass.add_argument(
        "--batch", metavar="BATCH_SIZE", default=DEFAULT_BATCH_SIZE, type=int,
        help=f"Batch length (corresponds to number of requests per message). "
        f"Default is {DEFAULT_BATCH_SIZE}")
    switches_mass.add_argument(
        "--dry", action="store_true",
        help="Dry run (don't do any database/service requests) to estimate "
        "client-side overhead")

    switches_postgres = argparse.ArgumentParser(add_help=False)
    switches_postgres.add_argument(
        "--postgres", metavar="CONN_STR", required=default_postgres is None,
        default=default_postgres,
        help="Connection string to Rcache Postgres database. " +
        (f"Default is {default_postgres}" if default_postgres is not None
         else "This parameter is mandatory"))

    switches_rcache = argparse.ArgumentParser(add_help=False)
    switches_rcache.add_argument(
        "--rcache", metavar="URL", required=default_rcache is None,
        default=default_rcache,
        help="URL to rcache service. " +
        (f"Default is {default_rcache}" if default_rcache is not None
         else "This parameter is mandatory"))

    argument_parser = argparse.ArgumentParser(
        description="Response cache test and manipulation tool")

    subparsers = argument_parser.add_subparsers(dest="subcommand",
                                                metavar="SUBCOMMAND")

    parser_mass_fill = subparsers.add_parser(
        "mass_fill", parents=[switches_mass, switches_rcache],
        help="Mass cache fill")
    parser_mass_fill.add_argument(
        "--dry_remote", action="store_true",
        help="Similar to --dry, but also makes a trivial 'get'. Useful for "
        "network performance estimation")
    parser_mass_fill.set_defaults(func=do_mass_fill)
    parser_mass_fill.set_defaults(is_async=True)

    parser_mass_lookup = subparsers.add_parser(
        "mass_lookup", parents=[switches_mass, switches_postgres],
        help="Mass cache lookup")
    parser_mass_lookup.add_argument(
        "--count", metavar="MUMBER_OF_LOOKUPS", default=DEFAULT_LOOKUP_COUNT,
        type=int,
        help=f"How many lookups to perform. Default is "
        f"{DEFAULT_LOOKUP_COUNT}")
    parser_mass_lookup.add_argument(
        "--check_response", action="store_true",
        help="Check response content. Note that response content might change "
        "because of invalidation/precomputation, thus they should be somehow "
        "disabled")
    parser_mass_lookup.set_defaults(func=do_mass_lookup)
    parser_mass_lookup.set_defaults(is_async=True)

    parser_invalidate = subparsers.add_parser(
        "invalidate", parents=[switches_rcache],
        help="Cache invalidation (all parameters are mutually exclusive)")
    parser_invalidate.add_argument(
        "--enable", action="store_true",
        help="Signal rcache service to enable cache invalidation (after it "
        "was previously disabled). All invalidation requests accumulated "
        "while disabled are fulfilled")
    parser_invalidate.add_argument(
        "--disable", action="store_true",
        help="Signal rcache service to disable cache invalidation")
    parser_invalidate.add_argument(
        "--all", action="store_true",
        help="Invalidate all cache")
    parser_invalidate.add_argument(
        "--tile", metavar="MIN_LAT,MIN_LON[,MAX_LAT,MAX_LON]", action="append",
        help="Tile to invalidate. Latitude/longitude are north/east positive "
        "degrees. Maximums, if not specified, are 'plus one degree' of "
        "minimums. This parameter may be specified several times")
    parser_invalidate.add_argument(
        "--ruleset", metavar="RULESET_ID", action="append",
        help="Config ruleset ID for entries to invalidate. This parameter may "
        "be specified several times")
    parser_invalidate.set_defaults(func=do_invalidate)
    parser_invalidate.set_defaults(is_async=True)

    parser_precompute = subparsers.add_parser(
        "precompute", parents=[switches_rcache],
        help="Set precomputation parameters")
    parser_precompute.add_argument(
        "--enable", action="store_true",
        help="Enable precomputation after it was previously disabled")
    parser_precompute.add_argument(
        "--disable", action="store_true",
        help="Disable precomputation (e.g. for development purposes)")
    parser_precompute.add_argument(
        "--quota", metavar="N", type=int,
        help="Set precompute quota - maximum number of simultaneous "
        "precomputation requests")
    parser_precompute.set_defaults(func=do_precompute)
    parser_precompute.set_defaults(is_async=True)

    parser_update = subparsers.add_parser(
        "update", parents=[switches_rcache],
        help="Enables/disables cache update")
    parser_update.add_argument(
        "--enable", action="store_true",
        help="Enable update after it was previously disabled")
    parser_update.add_argument(
        "--disable", action="store_true",
        help="Disable update. All update requests are dropped until emable")
    parser_update.set_defaults(func=do_update)
    parser_update.set_defaults(is_async=True)

    parser_status = subparsers.add_parser(
        "status", parents=[switches_rcache],
        help="Print service status")
    parser_status.add_argument(
        "--interval", metavar="SECONDS", type=float,
        help="Report status periodically with given interval (in seconds). "
        "Default is to report status once")
    parser_status.set_defaults(func=do_status)
    parser_status.set_defaults(is_async=True)

    # Subparser for 'help' command
    parser_help = subparsers.add_parser(
        "help", add_help=False,
        help="Prints help on given subcommand")
    parser_help.add_argument(
        "subcommand", metavar="SUBCOMMAND", nargs="?",
        choices=subparsers.choices,
        help="Name of subcommand to print help about (use " +
        "\"%(prog)s --help\" to get list of all subcommands)")
    parser_help.set_defaults(func=do_help, subparsers=subparsers,
                             argument_parser=argument_parser)

    if not argv:
        argument_parser.print_help()
        sys.exit(1)
    args = argument_parser.parse_args(argv)

    for predicate, argument in [("require_rcache", "rcache"),
                                ("require_postgres", "postgres")]:
        error_if(
            getattr(args, predicate, False) and (not getattr(args, argument)),
            f"--{argument} must be specified")
    try:
        if getattr(args, "is_async", False):
            asyncio.run(args.func(args))
        else:
            args.func(args)
    except SystemExit as ex:
        sys.exit(1 if isinstance(ex.code, str) else ex.code)
    except KeyboardInterrupt:
        print("^C")
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1:])
