""" Database requests and their scalers (bundlers) """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, too-many-locals, too-many-branches
# pylint: disable=too-many-nested-blocks, too-few-public-methods
# pylint: disable=unnecessary-pass, invalid-name, consider-using-from-import
# pylint: disable=broad-exception-caught, too-many-arguments
# pylint: disable=too-many-instance-attributes

import asyncio
from collections.abc import Coroutine
import urllib.parse
import sqlalchemy as sa
import sqlalchemy.ext.asyncio as sa_async
import time
import traceback
from typing import Any, Callable, cast, Dict, Generic, List, NamedTuple, \
    Optional, Set, TypeVar, Union

import afcmodels.hardcoded_relations as hardcoded_relations
import afc_server_models
from log_utils import dp, error, error_if, get_module_logger, safe_dsn
import rcache_models
import secret_utils

__all__ = ["AfcCertReq", "AfcCertResp", "AfcServerDb"]

# Logger for this module
LOGGER = get_module_logger()


class Certification(NamedTuple):
    """ Identifies single certification """
    # Ruleset ID (corresponds to certification domain)
    ruleset_name: str
    # Certification ID (within Ruleset ID)
    certification_id: str


class AfcCertReq:
    """ Certification information request
    Has same structure as Rest_DeviceDescriptor, except for different field
    names. Supposed to be immutable

    Public attributes:
    serial         -- AP Serial Number
    certifications -- AP certifications (set of Certification objects)
    """
    def __init__(self, dev_dsc: afc_server_models.Rest_DeviceDescriptor_1_4) \
            -> None:
        """ Constructor from Rest_DeviceDescriptor_1_4 object """
        self.serial = dev_dsc.serialNumber
        self.certifications = \
            {Certification(ruleset_name=c.rulesetId, certification_id=c.id)
             for c in dev_dsc.certificationId}

    def __hash__(self) -> int:
        """ Hash function """
        return hash(self.serial) + sum(hash(ci) for ci in self.certifications)

    def __eq__(self, other: Any) -> bool:
        """ Equality comparison """
        return isinstance(other, self.__class__) and \
            (self.serial == other.serial) and \
            (self.certifications == other.certifications)


class AfcCertResp:
    """ Certificate information response

    Public attributes:
    cert_responses - List of CertResp objects that correspond to certifications
                     from request
    """
    class CertResp(NamedTuple):
        """ Certification information fetched from RatDB """
        # Ruleset ID (corresponds to certification domain)
        ruleset_name: str
        # Optional location flags
        location_flags: Optional[int]
        # Certification not found in RatDB
        cert_undefined: bool
        # Certification blacklisted in RatDB
        cert_denied: bool
        # Given serial number blacklisted for certification
        serial_denied: bool

    # Certification deny criterion descriptor
    DenyCriterion = \
        NamedTuple(
            "DenyCriterion",
            [
             # Function to call to check if certificate is denied
             ("predicate", Callable[["AfcCertResp.CertResp"], bool]),
             # Deny explanation
             ("explanation", str)])

    # Certification deny criteria
    _deny_criteria = \
        [DenyCriterion(
            predicate=lambda cr: getattr(cr, "cert_undefined"),
            explanation="certificate not found"),
         DenyCriterion(
            predicate=lambda cr: getattr(cr, "cert_denied"),
            explanation="certificate denied"),
         DenyCriterion(
            predicate=lambda cr: getattr(cr, "serial_denied"),
            explanation="AP serial number denied"),
         DenyCriterion(
            predicate=lambda cr:
                ((getattr(cr, "location_flags") or 0) &
                 hardcoded_relations.CERT_ID_LOCATION_OUTDOOR) == 0,
            explanation="outdoor operation not allowed")]

    def __init__(self, bypass_checks: Optional[AfcCertReq] = None) -> None:
        """Constructor

        Arguments:
        bypass_checks -- None or request for which everything should be allowed
        """
        self.cert_responses: List["AfcCertResp.CertResp"] = []
        if bypass_checks is not None:
            for cert in bypass_checks.certifications:
                self.cert_responses.append(
                    self.CertResp(
                        ruleset_name=cert.ruleset_name,
                        location_flags=hardcoded_relations.
                        CERT_ID_LOCATION_INDOOR |
                        hardcoded_relations.CERT_ID_LOCATION_OUTDOOR,
                        cert_undefined=False, cert_denied=False,
                        serial_denied=False))

    def add_cert_resp(self, cert_resp: "AfcCertResp.CertResp", serial: str,
                      certification_id: str) -> None:
        """ Add response info for one certification

        Arguments:
        cert_resp        -- Certification response info being added
        serial           -- Serial number from certificate request
        certification_id -- Certificate ID from certificate request
        """
        if cert_resp.cert_undefined:
            # Override from special certificates
            scp = \
                hardcoded_relations.SpecialCertifications.get_properties(
                    cert_id=certification_id, serial_number=serial)
            if scp is not None:
                cert_resp = \
                    self.CertResp(
                        ruleset_name=cert_resp.ruleset_name,
                        location_flags=scp.location_flags,
                        cert_undefined=False,
                        cert_denied=cert_resp.cert_denied,
                        serial_denied=cert_resp.serial_denied)
        self.cert_responses.append(cert_resp)

    def allowed_cert_resps(self) -> List["AfcCertResp.CertResp"]:
        """ Returns certifications infos for which request was not denied """
        return [cr for cr in self.cert_responses
                if not any(dc.predicate(cr) for dc in self._deny_criteria)]

    def deny_reason(self) -> str:
        """ Returns deny reasons message for each of given certifications """
        reasons: List[str] = []
        for cr in self.cert_responses:
            for dc in self._deny_criteria:
                if dc.predicate(cr):
                    reasons.append(f"{cr.ruleset_name}: {dc.explanation}")
        return ", ".join(reasons)


# Generic type for request queue requests
ReqType = TypeVar("ReqType")
# Generic type for request queue responses
RespType = TypeVar("RespType")


class DbPipeline(Generic[ReqType, RespType]):
    """ Pipeline, that enqueues database requests and processes them in batches

    Private attributes:
    _name            -- Pipeline name
    _db_access       -- Function to perform DB lookup
    _max_reqs        -- Maximum number of request in single DB lookup
    _request_futures -- Per-request dictionary of lists of futures waiting for
                        response
    _request_queue   -- Queue of pending requests
    _stopping        -- Stop initiated
    _task            -- Pipe worker task
    """
    class StopReq:
        """ Cancel message to put to queue to unblock worker """
        pass

    def __init__(
            self, name: str,
            db_access: Callable[[Set[ReqType]],
                                Coroutine[Any, Any, Dict[ReqType, RespType]]],
            max_reqs: int) -> None:
        """ Constructor

        Arguments:
        name      -- Pipeline name
        db_access -- Database access function
        max_reqs  -- Maximum number of requests per database access
        """
        self._name = name
        self._db_access = db_access
        self._max_reqs = max_reqs
        self._request_futures: \
            Dict[ReqType, List["asyncio.Future[RespType]"]] = {}
        self._request_queue: \
            asyncio.Queue[Union[ReqType, "DbPipeline.StopReq"]] = \
            asyncio.Queue()
        self._stopping = False
        self._task: asyncio.Task = asyncio.create_task(self._worker(),
                                                       name=self._name)

    async def process_req(self, req: ReqType, deadline: float) -> RespType:
        """ Process given request

        Arguments:
        req      -- Database fetch request
        deadline -- Request completion deadline (seconds since the Epoch)
        Returns database request result. Generates some kind of timeout
        exception on missed deadline
        """
        timeout = deadline - time.time()
        if self._stopping or (timeout <= 0):
            raise TimeoutError()
        future_result: "asyncio.Future[RespType]" = asyncio.Future()
        sibling_futures = self._request_futures.get(req)
        if sibling_futures:
            sibling_futures.append(future_result)
        else:
            self._request_futures[req] = [future_result]
            self._request_queue.put_nowait(req)
        await asyncio.wait_for(future_result, timeout=timeout)
        return cast(RespType, future_result.result())

    async def stop(self) -> None:
        """ Stop worker, clean up requests """
        self._stopping = True
        self._request_queue.put_nowait(DbPipeline.StopReq())
        await self._task

    async def _worker(self) -> None:
        """ Worker task that does database access """
        try:
            while True:
                # Wait for something to appear in the queue
                while True:
                    req = await self._request_queue.get()
                    if self._stopping:
                        for futures in self._request_futures.values():
                            for future in futures:
                                if not future.done():
                                    future.cancel()
                        return
                    assert not isinstance(req, DbPipeline.StopReq)
                    if self._still_expected(req):
                        break
                # Grabbing more requests from queue
                reqs = {req}
                while (len(reqs) <= self._max_reqs) and \
                        (not self._request_queue.empty()):
                    req = self._request_queue.get_nowait()
                    assert not self._stopping
                    assert not isinstance(req, DbPipeline.StopReq)
                    if not self._still_expected(req):
                        continue
                    reqs.add(req)
                # Do the deed
                responses = await self._db_access(reqs)
                # Reporting results, unblocking pending process_req()
                for req, resp in responses.items():
                    futures1 = self._request_futures.get(req)
                    if futures1 is None:
                        continue
                    del self._request_futures[req]
                    for future in futures1:
                        if not future.done():
                            future.set_result(resp)
        except Exception as ex:
            for line in traceback.format_exception(ex):
                LOGGER.critical(line)
            error(f"Unhandled exception in {self._name} worker task: {ex}")

    def _still_expected(self, req: ReqType) -> bool:
        """ Returns true if there are unexpired futures on given request.
        If there are none - request removed from request dictionary """
        futures = self._request_futures[req]
        if any(not future.done() for future in futures):
            return True
        del self._request_futures[req]
        return False


class AfcServerDb:
    """ Rcache/Ratdb database access and its scalers

    Private attributes:
    _ratdb_engine               -- Engine for RatDB
    _ratdb_meta                 -- Metadata of RatDB database (empty initially)
    _rcache_engine              -- Engine for Rcache DB
    _rcache_meta                -- Metadata for Rcache DB (empty initially)
    _rcache_lookup_pipeline     -- DbPipeline for Rcache lookup requests
    _cert_lookup_pipeline       -- DbPipeline for certification check requests
    _afc_config_lookup_pipeline -- DbPipeline for AFC Config lookup requests
    _bypass_cert                -- True to bypass certificate database query
    _bypass_rcache              -- True to bypass Rcache
    _sample_rcache_reply        -- Rcache reply to use if bypassed
    """
    # Name of Postgres asynchronous driver (to use in DSN)
    _ASYNC_DRIVER_NAME = "asyncpg"
    # Name of Rcache DB table with response cache
    _RCACHE_AP_TABLE = "aps"
    # Name of RatDB table with AP/Certifications blacklist
    _RATDB_DENY_TABLE = "access_point_deny"
    # Name of RatDB table with certification information
    _RATDB_CERT_TABLE = "cert_id"
    # Name of RatDB table with ruleset ID names
    _RATDB_RULESET_TABLE = "aaa_ruleset"
    # Name of RatDB table with AFC Configs
    _RATDB_AFC_CONFIG_TABLE = "AFCConfig"
    # Maximum size of Rcache lookup
    _MAX_RCACHE_LOOKUP = 1000
    # Maximum size of Certification information lookup
    _MAX_CERT_LOOKUP = 1000
    # Maximum size of AFC Config lookup
    _MAX_AFC_CONFIG_LOOKUP = 1000

    def __init__(self, ratdb_dsn: str, ratdb_password_file: Optional[str],
                 rcache_dsn: str, rcache_password_file: Optional[str],
                 bypass_cert: bool = False, bypass_rcache: bool = False) \
            -> None:
        """ Constructor

        Arguments:
        ratdb_dsn            -- RatDB DSN (maybe without password)
        ratdb_password_file  -- Optional name of file with password for RatDB
                                DSN
        rcache_dsn           -- Rcache DB DSN (maybe without password)
        rcache_password_file -- Optional name of file with password for Rcache
                                DB DSN
        bypass_cert          -- True to bypass certificate database query
        bypass_rcache        -- True to bypass Rcache
        """
        self._ratdb_meta = sa.MetaData()
        self._rcache_meta = sa.MetaData()
        self._ratdb_engine = \
            self._create_engine(
                dsn=ratdb_dsn, password_file=ratdb_password_file, dsc="RatDB")
        self._rcache_engine = \
            self._create_engine(
                dsn=rcache_dsn, password_file=rcache_password_file,
                dsc="rcache database")
        self._rcache_lookup_pipeline: DbPipeline[str, Optional[str]] = \
            DbPipeline(name="Rcache Lookup", db_access=self._lookup_rcache,
                       max_reqs=self._MAX_RCACHE_LOOKUP)
        self._cert_lookup_pipeline: DbPipeline[AfcCertReq, AfcCertResp] = \
            DbPipeline(name="Certification lookup",
                       db_access=self._get_cert_infos,
                       max_reqs=self._MAX_CERT_LOOKUP)
        self._afc_config_lookup_pipeline: \
            DbPipeline[str, Optional[Dict[str, Any]]] = \
            DbPipeline(name="AFC Config lookup",
                       db_access=self._get_afc_configs,
                       max_reqs=self._MAX_AFC_CONFIG_LOOKUP)
        self._bypass_cert = bypass_cert
        self._bypass_rcache = bypass_rcache
        self._sample_rcache_reply: Optional[str] = None

    async def close(self) -> None:
        """ Stop workers and dispose of SqlAlchemy resources """
        async with asyncio.TaskGroup() as tg:
            tg.create_task(self._rcache_lookup_pipeline.stop())
            tg.create_task(self._cert_lookup_pipeline.stop())
            tg.create_task(self._afc_config_lookup_pipeline.stop())
        for engine, name in [(self._ratdb_engine, "RatDB"),
                             (self._rcache_engine, "Rcache DB")]:
            try:
                await engine.dispose()
            except (sa.exc.SQLAlchemyError, OSError) as ex:
                error(f"{name} dispose error: {ex}")

    async def lookup_rcache(self, req_cfg_digest: str, deadline: float) \
            -> Optional[str]:
        """ Lookup rcache for given request/config digest """
        return await self._rcache_lookup_pipeline.process_req(req_cfg_digest,
                                                              deadline)

    async def get_cert_info(self, cert_req: AfcCertReq, deadline: float) \
            -> AfcCertResp:
        """ Lookup Certification information for AP """
        return await self._cert_lookup_pipeline.process_req(cert_req, deadline)

    async def get_afc_config(self, ruleset_name: str, deadline: float) \
            -> Optional[Dict[str, Any]]:
        """ Lookup AFC Config for given ruleset """
        return \
            await self._afc_config_lookup_pipeline.process_req(ruleset_name,
                                                               deadline)

    async def _lookup_rcache(self, req_cfg_digests: Set[str]) \
            -> Dict[str, Optional[str]]:
        """ Fetching cached responses

        Arguments:
        req_cfg_digests -- Set of request/config digests
        Returns Dictionary of found responses, indexed by request/config
        digests. Not found responses returned as None
        """
        ap_table = \
            await self._get_table(
                table_name=self._RCACHE_AP_TABLE, meta=self._rcache_meta,
                engine=self._rcache_engine, db_name="Rcache")

        if self._bypass_rcache:
            if self._sample_rcache_reply is None:
                try:
                    s = sa.select([ap_table])
                    async with self._rcache_engine.connect() as conn:
                        rp = await conn.execute(s)
                        row = rp.first()
                        if row:
                            self._sample_rcache_reply = \
                                rcache_models.ApDbRecord.parse_obj(row).\
                                get_patched_response()
                except (sa.exc.SQLAlchemyError, OSError) as ex:
                    error(f"Rcache DB lookup error: {ex}")
            return {d: self._sample_rcache_reply for d in req_cfg_digests}

        try:
            s = sa.select([ap_table]).\
                where((ap_table.c.req_cfg_digest.in_(req_cfg_digests)) &
                      (ap_table.c.state ==
                       rcache_models.ApDbRespState.Valid.name))
            async with self._rcache_engine.connect() as conn:
                rp = await conn.execute(s)
                ret: Dict[str, Optional[str]] = \
                    {rec.req_cfg_digest:
                     rcache_models.ApDbRecord.parse_obj(rec).
                     get_patched_response()
                     for rec in rp.fetchall()}
                for req_cfg_digest in (req_cfg_digests - set(ret.keys())):
                    ret[req_cfg_digest] = None
                return ret
        except (sa.exc.SQLAlchemyError, OSError) as ex:
            error(f"Rcache DB lookup error: {ex}")
        return {}  # Unreachable code. Appeasing MyPy

    async def _get_cert_infos(self, reqs: Set[AfcCertReq]) \
            -> Dict[AfcCertReq, AfcCertResp]:
        """ Get information about certifications

        Arguments:
        reqs -- Set of AfcCertReq objects
        Returns Per-request dictionary of responses
        """
        ret: Dict[AfcCertReq, AfcCertResp] = {}
        if self._bypass_cert:
            for req in reqs:
                ret[req] = AfcCertResp(bypass_checks=req)
            return ret

        req_serials: Set[str] = set()
        req_certifications: Set[Certification] = set()
        for req in reqs:
            req_serials.add(req.serial)
            req_certifications.update(req.certifications)
        deny_table = \
            await self._get_table(
                table_name=self._RATDB_DENY_TABLE, meta=self._ratdb_meta,
                engine=self._ratdb_engine, db_name="RatDB")
        cert_table = \
            await self._get_table(
                table_name=self._RATDB_CERT_TABLE, meta=self._ratdb_meta,
                engine=self._ratdb_engine, db_name="RatDB")
        ruleset_table = \
            await self._get_table(
                table_name=self._RATDB_RULESET_TABLE, meta=self._ratdb_meta,
                engine=self._ratdb_engine, db_name="RatDB")
        class CertInfo(NamedTuple):
            """ Information about certification from select result """
            location_flags: int
            denied_serials: Set[Optional[str]] = set()
        try:
            s = sa.select(
                    [ruleset_table.c.name, cert_table.c.certification_id,
                     cert_table.c.location, deny_table.c.id,
                     deny_table.c.serial_number]).select_from(
                        ruleset_table.
                        join(cert_table,
                             ruleset_table.c.id == cert_table.c.ruleset_id).
                        join(deny_table,
                             (deny_table.c.certification_id ==
                              cert_table.c.certification_id), isouter=True)
                        ).where(
                            sa.tuple_(ruleset_table.c.name,
                                      cert_table.c.certification_id).in_(
                                [(cert.ruleset_name, cert.certification_id)
                                 for cert in req_certifications])
                        ).where(deny_table.c.serial_number.is_(None) |
                                deny_table.c.serial_number.in_(
                                    list(req_serials)))

            cert_infos: Dict[Certification, CertInfo] = {}
            async with self._ratdb_engine.connect() as conn:
                rp = await conn.execute(s)
                for row in rp.fetchall():
                    res_certification = \
                        Certification(
                            certification_id=row["certification_id"],
                            ruleset_name=row["name"])
                    cert_info = \
                        cert_infos.setdefault(
                            res_certification,
                            CertInfo(location_flags=row["location"]))
                    if row["id"] is not None:
                        cert_info.denied_serials.add(row["serial_number"])
        except (sa.exc.SQLAlchemyError, OSError) as ex:
            error(f"RatDB certification lookup error: {ex}")
        # Note that resulting table contains excessive information: for each
        # denied certificate it contains all denied serials (from original list
        # of serials due to second 'where'), even if they were not in original
        # requests (highly unlikely, but possible)
        for req in reqs:
            if req in ret:
                continue
            ret[req] = AfcCertResp()
            for certification in req.certifications:
                optional_cert_info = cert_infos.get(certification)
                ret[req].add_cert_resp(
                    AfcCertResp.CertResp(
                        ruleset_name=certification.ruleset_name,
                        location_flags=None if optional_cert_info is None
                        else optional_cert_info.location_flags,
                        cert_undefined=optional_cert_info is None,
                        cert_denied=(optional_cert_info is not None) and
                        (None in optional_cert_info.denied_serials),
                        serial_denied=(optional_cert_info is not None) and
                        (req.serial in optional_cert_info.denied_serials)),
                    serial=req.serial,
                    certification_id=certification.certification_id)
        return ret

    async def _get_afc_configs(self, ruleset_ids: Set[str]) \
            -> Dict[str, Optional[Dict[str, Any]]]:
        """ Read AFC Configs

        Arguments:
        ruleset_ids -- Set of Ruleset IDs for which to retrieve AFC Configs
        Returns per-Ruleset ID dictionary of AFC Config in dictionary form,
        Nones for missing configs
        """
        afc_config_table = \
            await self._get_table(
                table_name=self._RATDB_AFC_CONFIG_TABLE, meta=self._ratdb_meta,
                engine=self._ratdb_engine, db_name="RatDB")
        ruleset_to_region: Dict[str, Optional[str]] = {}
        regions: Set[str] = set()
        for ruleset_id in ruleset_ids:
            try:
                region = \
                    hardcoded_relations.RulesetVsRegion.ruleset_to_region(
                        ruleset_id, exc=KeyError)
                ruleset_to_region[ruleset_id] = region
                regions.add(region)
            except KeyError:
                ruleset_to_region[ruleset_id] = None
        if not regions:
            return {ruleset_id: None for ruleset_id in ruleset_ids}

        try:
            s = sa.select([afc_config_table.c.config]).where(
                afc_config_table.c.config["regionStr"].astext.in_(list(regions)))
            region_to_config: Dict[str, Dict[str, Any]] = {}
            async with self._ratdb_engine.connect() as conn:
                rp = await conn.execute(s)
                for row in rp.fetchall():
                    region_to_config[row.config["regionStr"]] = row.config
            ret: Dict[str, Optional[Dict[str, Any]]] = {}
            for ruleset_id, region_or_none in ruleset_to_region.items():
                ret[ruleset_id] = \
                    None if region_or_none is None \
                    else region_to_config.get(region_or_none)
        except (sa.exc.SQLAlchemyError, OSError) as ex:
            error(f"RatDB AFC Config lookup error: {ex}")
        return ret

    async def _get_table(self, table_name: str, meta: sa.MetaData,
                         engine: sa_async.AsyncEngine, db_name: str) \
            -> sa.Table:
        """  Get table of given name from given database

        Arguments:
        table_name -- Table name
        meta       -- DB metadata (maybe not yet fetched)
        engine     -- SqlAlchemy engine
        db_name    -- Database name (for error messages)
        Returns SqlAlchemy Table object
        """
        ret = meta.tables.get(table_name)
        if ret is not None:
            return ret
        try:
            async with engine.connect() as conn:
                await conn.run_sync(meta.reflect)
        except (sa.exc.SQLAlchemyError, OSError) as ex:
            error(f"Error reading metadata from {db_name} database: {ex}")
        ret = meta.tables.get(table_name)
        error_if(ret is None,
                 f"Table {table_name} not found in {db_name} database")
        assert ret is not None
        return ret

    def _create_engine(self, dsn: str, password_file: Optional[str],
                       dsc: str) -> sa_async.AsyncEngine:
        """ Create asynchronous AFC Engine

        Arguments:
        dsn           -- DB Connection string (possibly without password)
        password_file -- Optional file with password
        dsc           -- Description to use in error messages
        Returns asynchronous engine
        """
        try:
            parts = urllib.parse.urlsplit(dsn)
        except ValueError as ex:
            error(f"Invalid database DSN syntax: '{safe_dsn(dsn)}': {ex}")
        if not any(self._ASYNC_DRIVER_NAME in part for part in parts):
            dsn = \
                urllib.parse.urlunsplit(
                    parts._replace(
                        scheme=f"{parts.scheme}+{self._ASYNC_DRIVER_NAME}"))
        dsn = \
            secret_utils.substitute_password(
                dsc="rcache database", dsn=dsn, password_file=password_file,
                optional=True)
        try:
            engine = sa_async.create_async_engine(dsn, pool_pre_ping=True)
        except (sa.exc.SQLAlchemyError, OSError) as ex:
            error(f"Error opening {dsc} DSN '{safe_dsn(dsn)}': {ex}")
        return engine
