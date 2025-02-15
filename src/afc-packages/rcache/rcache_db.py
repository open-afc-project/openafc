""" Synchronous part of AFC Request Cache database stuff """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, invalid-name, too-many-statements,
# pylint: disable=too-many-branches

try:
    import db_creator
except ImportError:
    pass
import geoalchemy2 as ga
import sqlalchemy as sa
import sys
from typing import Any, cast, Dict, List, Optional, Tuple
import urllib.parse

import db_utils
from log_utils import dp, error, error_if, FailOnError, get_module_logger
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
    ap_pk_columns  -- Tuple of primary key's names. None when 'table' is None

    Private attributes:
    _engine        -- SqlAlchemy Engine object. None before connect()
    """

    # Maximum number of fields in one UPDATE
    _MAX_UPDATE_FIELDS = 32767

    # Name of request cache table in database
    AP_TABLE_NAME = "aps"

    # Name of enable/disable switches table in database
    SWITCHES_TABLE_NAME = "switches"

    # All table names
    ALL_TABLE_NAMES = [AP_TABLE_NAME, SWITCHES_TABLE_NAME]

    def __init__(self, rcache_db_dsn: Optional[str] = None,
                 rcache_db_password_file: Optional[str] = None) -> None:
        """ Constructor

        Arguments:
        rcache_db_dsn           -- Database connection string. May be None for
                                   Alembic use
        rcache_db_password_file -- Optional file with password for DSN
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
            sa.Column("coordinates",
                      ga.Geography(geometry_type="POINT", srid=4326),
                      nullable=False, index=True),
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
        self.rcache_db_dsn = \
            db_utils.substitute_password(
                dsn=rcache_db_dsn, password_file=rcache_db_password_file,
                optional=True)
        self.db_name: Optional[str] = \
            urllib.parse.urlsplit(self.rcache_db_dsn).path.strip("/") \
            if self.rcache_db_dsn else None
        self._engine: Any = None
        self.ap_table: Optional[sa.Table] = None
        self.ap_pk_columns: Optional[Tuple[str, ...]] = None

    def max_update_records(self) -> int:
        """ Maximum number of records in one update """
        return self._MAX_UPDATE_FIELDS // \
            len(self.metadata.tables[self.AP_TABLE_NAME].c)

    def create_db(self, db_creator_url: Optional[str],
                  alembic_config: Optional[str] = None,
                  alembic_initial_version: Optional[str] = None,
                  alembic_head_version: Optional[str] = None,
                  fail_on_error=True) -> bool:
        """ Creates database if absent, ensures correct alembic version

        Arguments:
        db_creator_url          -- REST API URL for Postgres database creation
                                   or None
        alembic_config          -- Alembic config file. None to do no Alembic
        alembic_initial_version -- None or version to stamp alembicless
                                   database with (before upgrade)
        alembic_head_version    -- Current alembic version. None to use 'head'
        fail_on_error           -- True to fail on error, False to return
                                   success status
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
                error_if("requests" not in sys.modules,
                         "'requests' modules, used for Postgres database "
                         "creation, not installed")
                engine = self._create_sync_engine(self.rcache_db_dsn)

                try:
                    _, existed = db_creator.ensure_dsn(dsn=self.rcache_db_dsn)
                except RuntimeError as ex:
                    error(f"Error creating Rcache database "
                          f"'{db_utils.safe_dsn(self.rcache_db_dsn)}': {ex}")

                if alembic_config:
                    err = \
                        db_utils.alembic_ensure_version(
                            alembic_config=alembic_config,
                            existing_database=existed,
                            initial_version=alembic_initial_version,
                            head_version=alembic_head_version)
                    error_if(err, err)
                try:
                    self.metadata.create_all(engine)
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
        if self._engine:
            return True
        engine: Any = None
        with FailOnError(fail_on_error):
            try:
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

    def lookup(self, req_cfg_digests: List[str],
               try_reconnect=False) -> Dict[str, str]:
        """ Request cache lookup

        Arguments:
        req_cfg_digests -- List of request/config digests
        try_reconnect   -- On failure try reconnect
        Returns Dictionary of found requests, indexed by request/config digests
        """
        retry = False
        while True:
            if try_reconnect and (self._engine is None):
                self.connect()
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
                if retry or (not try_reconnect):
                    error(f"Error querying '{self.db_name}: {ex}")
                retry = True
                try:
                    self.disconnect()
                except sa.exc.SQLAlchemyError:
                    self._engine = None
                assert self._engine is None
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
            for table_name in self.ALL_TABLE_NAMES:
                error_if(
                    table_name not in metadata.tables,
                    f"Table '{table_name}' not present in the database "
                    f"'{db_utils.safe_dsn(self.rcache_db_dsn)}'")
            self.metadata = metadata
            self._update_ap_table()
        except sa.exc.SQLAlchemyError as ex:
            error(f"Can't connect to database "
                  f"'{db_utils.safe_dsn(self.rcache_db_dsn)}': {ex}")
        finally:
            if engine is not None:
                engine.dispose()

    def get_ap_pk(self, row: Dict[str, Any]) -> Tuple:
        """ Return primary key tuple for given AP row dictionary """
        assert self.ap_pk_columns is not None
        return tuple(row[c] for c in self.ap_pk_columns)

    def _update_ap_table(self) -> None:
        """ Reads 'table' and index columns from current metadata """
        self.ap_table = self.metadata.tables[self.AP_TABLE_NAME]
        self.ap_pk_columns = \
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
            return sa.create_engine(dsn, pool_pre_ping=True)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Invalid database DSN: '{db_utils.safe_dsn(dsn)}': "
                  f"{ex}")
        return None  # Will never happen, appeasing pylint
