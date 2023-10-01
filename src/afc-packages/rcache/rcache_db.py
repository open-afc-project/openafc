""" Synchronous part of AFC Request Cache database stuff """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, invalid-name, too-many-statements,
# pylint: disable=too-many-branches

import sqlalchemy as sa
from typing import Any, cast, Dict, List, Optional, Tuple
import urllib.parse

from rcache_common import dp, error, error_if, FailOnError, get_module_logger
from rcache_models import ApDbRespState, ApDbRecord

__all__ = ["RcacheDb"]

# Logger for this module
LOGGER = get_module_logger()


class RcacheDb:
    """ Base/synchronous part of cache pPostgres database handling

    Public attributes:
    metadata       -- Database metadata. Default after construction, actual
                      after connection
    rcache_db_dsn  -- Database connection string. May be None e.g. if this
                      object is used for Alembic
    db_name        -- Database name. None before connect()
    ap_table       -- Table object for AP table. None before connect()

    Private attributes:
    _engine        -- SqlAlchemy Engine object. None before connect()
    _ap_pk_columns -- Tuple of primary key's names. None when 'table' is None
    """

    class _RootDb:
        """ Context manager that encapsulates everexisting Postgres root
        database

        Public attributes:
        dsn  -- Root database connection string
        conn -- Root database connection

        Private attributes:
        _engine -- Root database engine
        """
        # Name of root database (used for database creation
        ROOT_DB_NAME = "postgres"

        def __init__(self, dsn: str) -> None:
            """ Constructor

            Arguments:
            dsn -- Connection string to some (nonroot) database on same server
            """
            self.dsn = \
                urllib.parse.urlunsplit(
                    urllib.parse.urlsplit(dsn).
                    _replace(path=f"/{self.ROOT_DB_NAME}"))
            self._engine: Any = None
            self.conn: Any = None
            try:
                self._engine = sa.create_engine(self.dsn)
                self.conn = self._engine.connect()
            except sa.exc.SQLAlchemyError as ex:
                error(
                    f"Can't connect to root database '{self.dsn}': {ex}")
            finally:
                if (self.conn is None) and (self._engine is not None):
                    # Connection failed
                    self._engine.dispose()

        def __enter__(self) -> "RcacheDb._RootDb":
            """ Context entry """
            return self

        def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
            """ Context exit """
            if self.conn is not None:
                self.conn.close()
            if self._engine is not None:
                self._engine.dispose()

    # Maximum number of fields in one UPDATE
    _MAX_UPDATE_FIELDS = 32767

    # Name of request cache table in database
    AP_TABLE_NAME = "aps"

    # Name of enable/disable switches table in database
    SWITCHES_TABLE_NAME = "switches"


    def __init__(self, rcache_db_dsn: Optional[str] = None) -> None:
        """ Constructor

        Arguments:
        rcache_db_dsn -- Database connection string. May be None for Alembic
                         use
        """
        self.metadata = sa.MetaData()
        # This declaration must be kept in sync with rcache_models.ApDbRecord
        sa.Table(
            self.AP_TABLE_NAME,
            self.metadata,
            sa.Column("serial_number", sa.String(), nullable=False,
                      primary_key=True),
            sa.Column("rulesets", sa.String(), nullable=False,
                      primary_key=True),
            sa.Column("cert_ids", sa.String(), nullable=False,
                      primary_key=True),
            sa.Column("state", sa.Enum(ApDbRespState), nullable=False,
                      index=True),
            sa.Column("config_ruleset", sa.String(), nullable=False,
                      index=True),
            sa.Column("lat_deg", sa.Float(), nullable=False, index=True),
            sa.Column("lon_deg", sa.Float(), nullable=False, index=True),
            sa.Column("last_update", sa.DateTime(), nullable=False,
                      index=True),
            sa.Column("req_cfg_digest", sa.String(), nullable=False,
                      index=True, unique=True),
            sa.Column("validity_period_sec", sa.Float(), nullable=True),
            sa.Column("request", sa.String(), nullable=False),
            sa.Column("response", sa.String(), nullable=False))
        sa.Table(
            self.SWITCHES_TABLE_NAME,
            self.metadata,
            sa.Column("name", sa.String(), nullable=False, primary_key=True),
            sa.Column("state", sa.Boolean(), nullable=False))
        self.rcache_db_dsn = rcache_db_dsn
        self.db_name: Optional[str] = \
            urllib.parse.urlsplit(self.rcache_db_dsn).path.strip("/") \
            if self.rcache_db_dsn else None
        self._engine: Any = None
        self.ap_table: Optional[sa.Table] = None
        self._ap_pk_columns: Optional[Tuple[str, ...]] = None

    def max_update_records(self) -> int:
        """ Maximum number of records in one update """
        return self._MAX_UPDATE_FIELDS // \
            len(self.metadata.tables[self.AP_TABLE_NAME].c)

    def check_server(self) -> bool:
        """ True if database server can be connected """
        error_if(not self.rcache_db_dsn,
                 "AFC Response Cache URL was not specified")
        assert self.rcache_db_dsn is not None
        with FailOnError(False), self._RootDb(self.rcache_db_dsn) as rdb:
            rdb.conn.execute("SELECT 1")
            return True
        return False

    def create_db(self, recreate_db=False, recreate_tables=False,
                  fail_on_error=True) -> bool:
        """ Creates database if absent, optionally adjust if present

        Arguments:
        recreate_db      -- Recreate database if it exists
        recreate_tables  -- Recreate known database tables if database exists
        fail_on_error    -- True to fail on error, False to return success
                            status
        Returns True on success, Fail on failure (if fail_on_error is False)
        """
        engine: Any = None
        with FailOnError(fail_on_error):
            try:
                if self._engine:
                    self._engine.dispose()
                    self._engine = None
                error_if(not self.rcache_db_dsn,
                         "AFC Response Cache URL was not specified")
                assert self.rcache_db_dsn is not None
                engine = self._create_sync_engine(self.rcache_db_dsn)
                if recreate_db:
                    with self._RootDb(self.rcache_db_dsn) as rdb:
                        try:
                            rdb.conn.execute("COMMIT")
                            rdb.conn.execute(
                                f'DROP DATABASE IF EXISTS "{self.db_name}"')
                        except sa.exc.SQLAlchemyError as ex:
                            error(f"Unable to drop database '{self.db_name}': "
                                  f"{ex}")
                try:
                    with engine.connect():
                        pass
                except sa.exc.SQLAlchemyError:
                    with self._RootDb(self.rcache_db_dsn) as rdb:
                        try:
                            rdb.conn.execute("COMMIT")
                            rdb.conn.execute(
                                f'CREATE DATABASE "{self.db_name}"')
                            with engine.connect():
                                pass
                        except sa.exc.SQLAlchemyError as ex1:
                            error(f"Unable to create target database: {ex1}")
                try:
                    if recreate_tables:
                        with engine.connect() as conn:
                            conn.execute("COMMIT")
                            conn.execute(
                                f'DROP TABLE IF EXISTS "{self.AP_TABLE_NAME}"')
                            conn.execute(
                                f'DROP TABLE IF EXISTS '
                                f'"{self.SWITCHES_TABLE_NAME}"')
                    if not sa.inspect(engine).has_table(self.AP_TABLE_NAME):
                        self.metadata.create_all(engine)
                    else:
                        self._read_metadata()
                except sa.exc.SQLAlchemyError as ex:
                    error(f"Unable to (re)create tables in the database "
                          f"'{self.db_name}': {ex}")
                self._update_ap_table()
                self._engine = engine
                engine = None
                return True
            finally:
                if engine:
                    engine.dispose()
        return False

    def connect(self, fail_on_error=True) -> bool:
        """ Connect to database, that is assumed to be existing

        Arguments:
        fail_on_error    -- True to fail on error, False to return success
                            status
        Returns True on success, Fail on failure (if fail_on_error is False)
        """
        engine: Any = None
        with FailOnError(fail_on_error):
            try:
                if self._engine:
                    self._engine.dispose()
                    self._engine = None
                error_if(not self.rcache_db_dsn,
                         "AFC Response Cache URL was not specified")
                engine = self._create_engine(self.rcache_db_dsn)
                dsn_parts = urllib.parse.urlsplit(self.rcache_db_dsn)
                self.db_name = cast(str, dsn_parts.path).strip("/")
                self._read_metadata()
                with engine.connect():
                    pass
                self._engine = engine
                engine = None
                return True
            finally:
                if engine:
                    engine.dispose()
        return False

    def disconnect(self) -> None:
        """ Disconnect database """
        if self._engine:
            self._engine.dispose()
            self._engine = None

    def lookup(self, req_cfg_digests: List[str]) -> Dict[str, str]:
        """ Request cache lookup

        Arguments:
        req_cfg_digests -- List of request/config digests
        Returns Dictionary of found requests, indexed by request/config digests
        """
        assert (self._engine is not None) and (self.ap_table is not None)
        s = sa.select([self.ap_table]).\
            where((self.ap_table.c.req_cfg_digest.in_(req_cfg_digests)) &
                  (self.ap_table.c.state == ApDbRespState.Valid.name))
        try:
            with self._engine.connect() as conn:
                rp = conn.execute(s)
                return {rec.req_cfg_digest:
                        ApDbRecord.parse_obj(rec).get_patched_response()
                        for rec in rp}
        except sa.exc.SQLAlchemyError as ex:
            error(f"Error querying '{self.db_name}: {ex}")
        return {}  # Will never happen, appeasing pylint

    def _read_metadata(self) -> None:
        """ Reads-in metadata (fill in self.metadata, self.ap_table) from an
        existing database """
        engine: Any = None
        try:
            engine = self._create_sync_engine(self.rcache_db_dsn)
            with engine.connect():
                pass
            metadata = sa.MetaData()
            metadata.reflect(bind=engine)
            for table_name in (self.AP_TABLE_NAME, self.SWITCHES_TABLE_NAME):
                error_if(
                    table_name not in metadata.tables,
                    f"Table '{table_name}' not present in the database in "
                    f"database '{self.rcache_db_dsn}'")
            self.metadata = metadata
            self._update_ap_table()
        except sa.exc.SQLAlchemyError as ex:
            error(f"Can't connect to database '{self.rcache_db_dsn}': {ex}")
        finally:
            if engine is not None:
                engine.dispose()

    def get_ap_pk(self, row: Dict[str, Any]) -> Tuple:
        """ Return primary key tuple for given AP row dictionary """
        assert self._ap_pk_columns is not None
        return tuple(row[c] for c in self._ap_pk_columns)

    def _update_ap_table(self) -> None:
        """ Reads 'table' and index columns from current metadata """
        self.ap_table = self.metadata.tables[self.AP_TABLE_NAME]
        self._ap_pk_columns = \
            tuple(c.name for c in self.ap_table.c if c.primary_key)

    def _create_engine(self, dsn) -> Any:
        """ Creates SqlAlchemy engine

        Overloaded in RcacheDbAsync to create asynchronous engine

        Returns Engine object
        """
        return self._create_sync_engine(dsn)

    def _create_sync_engine(self, dsn) -> Any:
        """ Creates synchronous SqlAlchemy engine

        Overloaded in RcacheDbAsync to create asynchronous engine

        Returns Engine object
        """
        try:
            return sa.create_engine(dsn)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Invalid database DSN: '{dsn}': {ex}")
        return None  # Will never happen, appeasing pylint
