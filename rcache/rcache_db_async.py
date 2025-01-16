""" Asynchronous part of AFC Request Cache database stuff """
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

from log_utils import dp, error, error_if, FailOnError
from rcache_db import RcacheDb
from rcache_models import ApDbRespState, FuncSwitch, LatLonRect, ApDbPk
import secret_utils

__all__ = ["RcacheDbAsync"]


class RcacheDbAsync(RcacheDb):
    """ Asynchronous work with database, used in Request cache service """

    # Name of Postgres asynchronous driver (to use in DSN)
    ASYNC_DRIVER_NAME = "asyncpg"

    def __init__(self, rcache_db_dsn: str,
                 rcache_db_password_file: Optional[str]) -> None:
        """ Constructor

        Arguments:
        rcache_db_dsn           -- Postgres database connection string
        rcache_db_password_file -- Name of file with password to use in DSN
        """
        super().__init__(rcache_db_dsn=rcache_db_dsn,
                         rcache_db_password_file=rcache_db_password_file)

    async def disconnect(self) -> None:
        """ Disconnect database """
        if self._engine:
            await self._engine.dispose()
            self._engine = None

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
        assert self._engine is not None
        assert len(rows) <= self.max_update_records()
        try:
            ins = sa_pg.insert(self.ap_table).values(rows)
            ins = \
                ins.on_conflict_do_update(
                    index_elements=self.ap_pk_columns,
                    set_={col_name: ins.excluded[col_name]
                          for col_name in self.ap_table.columns.keys()
                          if col_name not in self.ap_pk_columns})
            async with self._engine.begin() as conn:
                await conn.execute(ins)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Cache database upsert failed: {ex}")

    async def invalidate(self, ruleset: Optional[str] = None,
                         limit: Optional[int] = None) -> int:
        """ Invalidate cache - completely or for given config

        Arguments:
        ruleset -- None for complete invalidation, ruleset ID for config-based
                   invalidation
        limit   -- Optional maximum number of rows to invalidate (10000000
                   rows invalidates for half an hour!)
        Returns number of rows invalidated
        """
        assert self._engine
        try:
            upd = sa.update(self.ap_table).\
                values(state=ApDbRespState.Invalid.name)
            if ruleset:
                upd = upd.where(self.ap_table.c.config_ruleset == ruleset)
            if limit is None:
                upd = \
                    upd.where(
                        self.ap_table.c.state == ApDbRespState.Valid.name)
            else:
                pk_columns = [self.ap_table.c[col_name]
                              for col_name in self.ap_pk_columns]
                upd = \
                    upd.where(
                        sa.tuple_(*pk_columns).in_(
                            sa.select(pk_columns).
                            where(self.ap_table.c.state ==
                                  ApDbRespState.Valid.name).
                            limit(limit)))
            async with self._engine.begin() as conn:
                rp = await conn.execute(upd)
                return rp.rowcount
        except sa.exc.SQLAlchemyError as ex:
            error(f"Cache database invalidation failed: {ex}")
        return 0  # Will never happen

    async def spatial_invalidate(self, rect: LatLonRect) -> None:
        """ Spatial invalidation

        Arguments:
        rect -- Lat/lon rectangle to invalidate
        """
        assert self._engine is not None
        c_lat = self.ap_table.c.lat_deg
        c_lon = self.ap_table.c.lon_deg
        try:
            upd = sa.update(self.ap_table).\
                where((self.ap_table.c.state == ApDbRespState.Valid.name) &
                      (c_lat >= rect.min_lat) & (c_lat <= rect.max_lat) &
                      (((c_lon >= rect.min_lon) & (c_lon <= rect.max_lon)) |
                       ((c_lon >= (rect.min_lon - 360)) &
                        (c_lon <= (rect.max_lon - 360))) |
                       ((c_lon >= (rect.min_lon + 360)) &
                        (c_lon <= (rect.max_lon + 360))))).\
                values(state=ApDbRespState.Invalid.name)
            async with self._engine.begin() as conn:
                await conn.execute(upd)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Cache database spatial invalidation failed: {ex}")

    async def reset_precomputations(self) -> None:
        """ Mark records in precomputation state as invalid """
        assert self._engine
        try:
            upd = sa.update(self.ap_table).\
                where(self.ap_table.c.state == ApDbRespState.Precomp.name).\
                values(state=ApDbRespState.Invalid.name)
            async with self._engine.begin() as conn:
                await conn.execute(upd)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Cache database unprecomputation failed: {ex}")

    async def num_precomputing(self) -> int:
        """ Return number of requests currently being precomputed """
        assert self._engine is not None
        try:
            sel = sa.select([sa.func.count()]).select_from(self.ap_table).\
                where(self.ap_table.c.state == ApDbRespState.Precomp.name)
            async with self._engine.begin() as conn:
                rp = await conn.execute(sel)
                return rp.fetchone()[0]
        except sa.exc.SQLAlchemyError as ex:
            error(f"Cache database spatial precomputing count query failed: "
                  f"{ex}")
        return 0  # Will never happen. Appeasing MyPy

    async def get_invalid_reqs(self, limit: int) -> List[str]:
        """ Return list of invalidated requests, marking them as being
        precomputed

        Arguments:
        limit -- Maximum number of requests to return
        Returns list of requests as strings
        """
        assert self._engine is not None
        try:
            sq = sa.select([self.ap_table.c.serial_number,
                            self.ap_table.c.rulesets,
                            self.ap_table.c.cert_ids]).\
                where(self.ap_table.c.state == ApDbRespState.Invalid.name).\
                order_by(sa.desc(self.ap_table.c.last_update)).\
                limit(limit)
            upd = sa.update(self.ap_table).\
                values({"state": ApDbRespState.Precomp.name}).\
                where(sa.tuple_(self.ap_table.c.serial_number,
                                self.ap_table.c.rulesets,
                                self.ap_table.c.cert_ids).in_(sq)).\
                returning(self.ap_table.c.request)
            async with self._engine.begin() as conn:
                rp = await conn.execute(upd)
                return [row[0] for row in rp]
        except sa.exc.SQLAlchemyError as ex:
            error(f"Cache database invalidated query failed: {ex}")
        return 0  # Will never happen. Appeasing MyPy

    async def get_num_invalid_reqs(self) -> int:
        """ Returns number of invalidated records """
        assert self._engine is not None
        try:
            sel = sa.select([sa.func.count()]).select_from(self.ap_table).\
                where(self.ap_table.c.state == ApDbRespState.Invalid.name)
            async with self._engine.begin() as conn:
                rp = await conn.execute(sel)
                return rp.fetchone()[0]
        except sa.exc.SQLAlchemyError as ex:
            error(f"Cache database invalidated count query failed: {ex}")
        return 0  # Will never happen. Appeasing MyPy

    async def get_cache_size(self) -> int:
        """ Returns total number entries in cache (including nonvalid) """
        assert self._engine is not None
        try:
            sel = sa.select([sa.func.count()]).select_from(self.ap_table)
            async with self._engine.begin() as conn:
                rp = await conn.execute(sel)
                return rp.fetchone()[0]
        except sa.exc.SQLAlchemyError as ex:
            error(f"Cache database size query failed: {ex}")
        return 0  # Will never happen. Appeasing MyPy

    async def delete(self, pk: ApDbPk) -> None:
        """ Delete row by primary key """
        assert self._engine is not None
        try:
            d = sa.delete(self.ap_table)
            for k, v in pk.dict().items():
                d = d.where(self.ap_table.c[k] == v)
                async with self._engine.begin() as conn:
                    await conn.execute(d)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Cache database removal failed: {ex}")

    async def get_switch(self, sw: FuncSwitch) -> bool:
        """ Gets value of given switch """
        assert self._engine is not None
        if self.SWITCHES_TABLE_NAME not in self.metadata.tables:
            return True
        try:
            table = self.metadata.tables[self.SWITCHES_TABLE_NAME]
            sel = sa.select([table.c.state]).where(table.c.name == sw.name)
            async with self._engine.begin() as conn:
                rp = await conn.execute(sel)
                v = rp.first()
                return True if v is None else v[0]
        except sa.exc.SQLAlchemyError as ex:
            error(f"Error reading switch value for '{sw.name}': {ex}")
        return True  # Will never happen. Appeasing MyPy

    async def set_switch(self, sw: FuncSwitch, state: bool) -> None:
        """ Sets value of given switch """
        assert self._engine is not None
        error_if(self.SWITCHES_TABLE_NAME not in self.metadata.tables,
                 f"Table '{self.SWITCHES_TABLE_NAME}' not found in "
                 f"'{self.db_name}' database")
        try:
            table = self.metadata.tables[self.SWITCHES_TABLE_NAME]
            ins = sa_pg.insert(table).values(name=sw.name, state=state)
            ins = ins.on_conflict_do_update(index_elements=["name"],
                                            set_={"state": state})
            async with self._engine.begin() as conn:
                await conn.execute(ins)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Switch setting upsert failed: {ex}")

    def _create_engine(self, dsn) -> Any:
        """ Creates asynchronous SqlAlchemy engine """
        try:
            parts = urllib.parse.urlsplit(dsn)
        except ValueError as ex:
            error(f"Invalid database DSN syntax: "
                  f"'{secret_utils.safe_dsn(dsn)}': {ex}")
        if self.ASYNC_DRIVER_NAME not in parts:
            dsn = \
                urllib.parse.urlunsplit(
                    parts._replace(
                        scheme=f"{parts.scheme}+{self.ASYNC_DRIVER_NAME}"))
        try:
            return sa_async.create_async_engine(dsn, pool_pre_ping=True)
        except sa.exc.SQLAlchemyError as ex:
            error(
                f"Invalid database DSN: '{secret_utils.safe_dsn(dsn)}': {ex}")
        return None  # Will never happen. Appeasing pylint
