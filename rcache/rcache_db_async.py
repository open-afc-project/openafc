""" Synchronous part of AFC Request Cache database stuff """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, invalid-name, useless-parent-delegation

import urllib.parse
import sqlalchemy as sa
import sqlalchemy.ext.asyncio as sa_async
import sqlalchemy.dialects.postgresql as sa_pg
from typing import Any, Dict, List, Optional

from rcache_common import dp, error, error_if, FailOnError
from rcache_db import ReqCacheDb
from rcache_models import DbRespState

__all__ = ["ReqCacheDbAsync"]


class ReqCacheDbAsync(ReqCacheDb):
    """ Asynchronous work with database, used in Request cache service """

    # Name of Postgres asynchronous driver (to use in DSN)
    ASYNC_DRIVER_NAME = "asyncpg"

    def __init__(self, rcache_db_dsn: str) -> None:
        """ Constructor

        Arguments:
        rcache_db_dsn -- Postgres database connection string
        """
        super().__init__(rcache_db_dsn)

    async def connect(self, fail_on_error=True) -> bool:
        """ Connect to database, that is assumed to be existing

        Arguments:
        fail_on_error    -- True to fail on error, False to return success
                            status
        Returns True on success, Fail on failure (if fail_on_error is False)
        """
        engine: Any = None
        with FailOnError(fail_on_error):
            try:
                if self._engine is not None:
                    self._engine.dispose()
                    self._engine = None
                error_if(not self.rcache_db_dsn,
                         "AFC Request Cache URL was not specified")
                engine = self._create_engine(self.rcache_db_dsn)
                dsn_parts = urllib.parse.urlsplit(self.rcache_db_dsn)
                self.db_name = dsn_parts.path.strip("/")
                self._read_metadata()
                async with engine.connect():
                    pass
                self._engine = engine
                engine = None
                return True
            finally:
                if engine is not None:
                    await engine.dispose()
        return False

    async def update_cache(self, rows: List[Dict[str, Any]]) -> None:
        """ Update cache with computed AFC Requests

        Arguments:
        rows -- List of request/response/request-config digest triplets
        """
        if not rows:
            return
        assert self._engine
        assert len(rows) <= self.max_update_records()
        try:
            ins = sa_pg.insert(self.table).values(rows).\
                on_conflict_do_update(
                    index_elements=[c.name
                                    for c in self.table.c if c.primary_key],
                    set_={c.name: c
                          for c in self.table.c if not c.primary_key})
            async with self._engine.begin() as conn:
                await conn.execute(ins)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Cache database upsert failed: {ex}")

    async def invalidate(self, ruleset: Optional[str] = None) -> None:
        """ Invalidate cache - completely or for given config

        Arguments:
        ruleset -- None for complete invalidation, ruleset ID for config-based
                   invalidation
        """
        assert self._engine
        try:
            upd = sa.update(self.table).\
                where(self.table.c.state == DbRespState.Valid.name).\
                values(state=DbRespState.Invalid.name)
            if ruleset:
                upd = upd.where(self.table.c.config_ruleset == ruleset)
            async with self._engine.begin() as conn:
                await conn.execute(upd)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Cache database invalidation failed: {ex}")

    async def spatial_invalidate(self, rect: "ReqCacheDbAsync.Rect") -> None:
        """ Spatial invalidation

        Arguments:
        rect -- Lat/lon rectangle to invalidate
        """
        assert self._engine
        c_lat = self.table.c.lat_deg
        c_lon = self.table.c.lon_deg
        try:
            upd = sa.update(self.table).\
                where((self.table.c.state == DbRespState.Valid.name) &
                      (c_lat >= rect.min_lat) & (c_lat <= rect.max_lat) &
                      (((c_lon >= rect.min_lon) & (c_lon <= rect.max_lon)) |
                       ((c_lon >= (rect.min_lon - 360)) &
                        (c_lon <= (rect.max_lon - 360))) |
                       ((c_lon >= (rect.min_lon + 360)) &
                        (c_lon <= (rect.max_lon + 360))))).\
                values(state=DbRespState.Invalid.name)
            async with self._engine.begin() as conn:
                await conn.execute(upd)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Cache database spatial invalidation failed: {ex}")

    async def num_precomputing(self) -> int:
        """ Return number of requests currently being precomputed """
        assert self._engine
        try:
            sel = sa.select([sa.func.count()]).\
                where(self.table.c.state == DbRespState.Invalid.name)
            async with self._engine.begin() as conn:
                rp = await conn.execute(sel)
                return rp.scalar()
        except sa.exc.SQLAlchemyError as ex:
            error(f"Cache database spatial precomputing count query failed: "
                  f"{ex}")

    async def get_invalid_reqs(self, limit: int) -> List[str]:
        """ Return list of invalidated requests

        Arguments:
        limit -- Maximum number of requests to return
        Returns list of requests as strings
        """
        assert self._engine
        try:
            sq = sa.select([self.table.c.serial_number,
                            self.table.c.rulesets,
                            self.table.c.cert_ids]).\
                where(self.table.c.state == DbRespState.Invalid.name).\
                limit(limit)
            upd = sa.update(self.table).\
                values({"state": DbRespState.Precomp.name}).\
                where(sa.tuple_(self.table.c.serial_number,
                                self.table.c.rulesets,
                                self.table.c.cert_ids).in_(sq)).\
                returning(self.table.c.request)
            async with self._engine.begin() as conn:
                rp = await conn.execute(upd)
                return [row[0] for row in rp]
        except sa.exc.SQLAlchemyError as ex:
            error(f"Cache database invalidated query failed: {ex}")

    def _create_engine(self, dsn) -> Any:
        """ Creates asynchronous SqlAlchemy engine """
        try:
            parts = urllib.parse.urlsplit(dsn)
        except ValueError as ex:
            error(f"Invalid database DSN syntax: '{dsn}': {ex}")
        if self.ASYNC_DRIVER_NAME not in parts:
            dsn = \
                urllib.parse.urlunsplit(
                    parts._replace(
                        scheme=f"{parts.scheme}+{self.ASYNC_DRIVER_NAME}"))
        try:
            return sa_async.create_async_engine(dsn)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Invalid database DSN: '{dsn}': {ex}")
        return None  # Will never happen. Appeasing pylint
