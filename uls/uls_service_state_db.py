""" ULS service state database """

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=invalid-name, broad-exception-caught, wildcard-import
# pylint: disable=wrong-import-order, unused-wildcard-import

import datetime
import enum
import secret_utils
import sqlalchemy as sa
import sqlalchemy.dialects.postgresql as sa_pg
from typing import Any, Dict, Iterable, List, NamedTuple, Optional, Set
import urllib.parse
from uls_service_common import *

# FS database downloader milestone. Names used in database and Prometheus
# metrics. Please change judiciously (if at all)
DownloaderMilestone = \
    enum.Enum("DownloaderMilestone",
              [
                  # Service birth (first write to milestone database)
                  "ServiceBirth",
                  # Service start
                  "ServiceStart",
                  # Download start
                  "DownloadStart",
                  # Download success
                  "DownloadSuccess",
                  # FS region changed
                  "RegionChanged",
                  # FS database file updated
                  "DbUpdated",
                  # External parameters checked
                  "ExtParamsChecked",
                  # Healthcheck performed
                  "Healthcheck",
                  # Beacon sent
                  "BeaconSent",
                  # Alarm sent
                  "AlarmSent"])

# Type of log
LogType = \
    enum.Enum("LogType",
              [
                  # Log of last download attempt
                  "Last",
                  # Log of last failed attempt
                  "LastFailed",
                  # Log of last completed update
                  "LastCompleted"])

# Information about retrieved log record
LogInfo = \
    NamedTuple(
        "LogInfo",
        [("text", str), ("timestamp", datetime.datetime),
         ("log_type", LogType)])

# Check type
CheckType = enum.Enum("CheckType", ["ExtParams", "FsDatabase"])

# Information about check
CheckInfo = \
    NamedTuple(
        "CheckInfo",
        [
            # Check type
            ("check_type", CheckType),
            # Item checked
            ("check_item", str),
            # Error message (None if check passed)
            ("errmsg", Optional[str]),
            # Check timestamp
            ("timestamp", datetime.datetime)])


# Alarm types
AlarmType = enum.Enum("AlarmType", ["MissingMilestone", "FailedCheck"])

# Information about alarm
AlarmInfo = \
    NamedTuple(
        "AlarmInfo",
        [
         # Type of alarm
         ("alarm_type", AlarmType),
         # Specific reason for alarm (name of missing milestone,
         # name of offending external file, etc.)
         ("alarm_reason", str),
         # Alarm timestramp
         ("timestamp", datetime.datetime)])


class StateDb:
    """ Status database access wrapper

    Public attributes:
    db_dsn   -- Connection string (might be None in Alembic environment)
    db_name  -- Database name from connection string (might be None in Alembic
                environment)
    metadata -- Metadata. Originally as predefined, after connection - actual

    Private attributes::
    _engine -- SqlAlchemy engine (set on connection, reset to None on
                disconnection or error)
    """
    # Name of table with milestones
    MILESTONE_TABLE_NAME = "milestones"

    # Name of table with alarming milestones
    ALARM_TABLE_NAME = "alarms"

    # Name of table with recent successful and unsuccessful logs
    LOG_TABLE_NAME = "logs"

    # Name of table with check results
    CHECKS_TABLE = "checks"

    # All known table names (not including Alembic, etc.)
    ALL_TABLE_NAMES = [MILESTONE_TABLE_NAME, ALARM_TABLE_NAME, LOG_TABLE_NAME,
                       CHECKS_TABLE]

    class _RootDb:
        """ Context wrapper to work with everpresent root database

        Public attributes:
        dsn  -- Connection string to root database
        conn -- Sqlalchemy connection object to root database

        Private attributes:
        _engine -- SqlAlchemy connection object
        """
        # Name of Postgres root database
        ROOT_DB_NAME = "postgres"

        def __init__(self, dsn: str) -> None:
            """ Constructor

            Arguments:
            dsn -- Connection string to (nonroot) database of interest
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
                    f"Can't connect to root database "
                    f"'{secret_utils.safe_dsn(self.dsn)}': {ex}")
            finally:
                if (self.conn is None) and (self._engine is not None):
                    # Connection failed
                    self._engine.dispose()

        def __enter__(self) -> "StateDb._RootDb":
            """ Context entry """
            return self

        def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
            """ Context exit """
            if self.conn is not None:
                self.conn.close()
            if self._engine is not None:
                self._engine.dispose()

    def __init__(self, db_dsn: Optional[str],
                 db_password_file: Optional[str]) -> None:
        """ Constructor

        Arguments:
        db_dsn           -- Connection string to state database. None in
                            Alembic environment
        db_password_file -- File with password to use in DSN
        """
        self.db_dsn = \
            secret_utils.substitute_password(
                dsc="ULS Service State Database", dsn=db_dsn,
                password_file=db_password_file, optional=True)
        db_dsn
        self.db_name: Optional[str] = \
            urllib.parse.urlsplit(self.db_dsn).path.strip("/") \
            if self.db_dsn else None
        self.metadata = sa.MetaData()
        sa.Table(
            self.MILESTONE_TABLE_NAME,
            self.metadata,
            sa.Column("milestone", sa.Enum(DownloaderMilestone), index=True,
                      primary_key=True),
            sa.Column("region", sa.String(), index=True, primary_key=True),
            sa.Column("timestamp", sa.DateTime(), nullable=False))
        sa.Table(
            self.ALARM_TABLE_NAME,
            self.metadata,
            sa.Column("alarm_type", sa.Enum(AlarmType), primary_key=True,
                      index=True),
            sa.Column("alarm_reason", sa.String(), primary_key=True,
                      index=True),
            sa.Column("timestamp", sa.DateTime(), nullable=False))
        sa.Table(
            self.LOG_TABLE_NAME,
            self.metadata,
            sa.Column("log_type", sa.Enum(LogType), index=True, nullable=False,
                      primary_key=True),
            sa.Column("text", sa.String(), nullable=False),
            sa.Column("timestamp", sa.DateTime(), nullable=False, index=True))
        sa.Table(
            self.CHECKS_TABLE,
            self.metadata,
            sa.Column("check_type", sa.Enum(CheckType), primary_key=True,
                      index=True),
            sa.Column("check_item", sa.String(), primary_key=True, index=True),
            sa.Column("errmsg", sa.String(), nullable=True),
            sa.Column("timestamp", sa.DateTime(), nullable=False))
        self._engine: Any = None

    def check_server(self) -> bool:
        """ True if database server can be connected """
        error_if(not self.db_dsn,
                 "FS downloader status database DSN was not specified")
        assert self.db_dsn is not None
        with self._RootDb(self.db_dsn) as rdb:
            rdb.conn.execute("SELECT 1")
            return True
        return False

    def create_db(self, recreate_db=False, recreate_tables=False) -> bool:
        """ Creates database if absent, optionally adjust if present

        Arguments:
        recreate_db     -- Recreate database if it exists
        recreate_tables -- Recreate known database tables if database exists
        Returns True on success, Fail on failure (if fail_on_error is False)
        """
        engine: Any = None
        try:
            if self._engine:
                self._engine.dispose()
                self._engine = None
            error_if(not self.db_dsn,
                     "FS downloader status database DSN was not specified")
            assert self.db_dsn is not None
            engine = self._create_engine(self.db_dsn)
            if recreate_db:
                with self._RootDb(self.db_dsn) as rdb:
                    try:
                        rdb.conn.execute("COMMIT")
                        rdb.conn.execute(
                            f'DROP DATABASE IF EXISTS "{self.db_name}"')
                    except sa.exc.SQLAlchemyError as ex:
                        error(f"Unable to drop database '{self.db_name}': "
                              f"{repr(ex)}")
            try:
                with engine.connect():
                    pass
            except sa.exc.SQLAlchemyError:
                with self._RootDb(self.db_dsn) as rdb:
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
                        for table_name in self.ALL_TABLE_NAMES:
                            conn.execute(
                                f'DROP TABLE IF EXISTS "{table_name}"')
                self.metadata.create_all(engine, checkfirst=True)
                self._read_metadata()
            except sa.exc.SQLAlchemyError as ex:
                error(f"Unable to (re)create tables in the database "
                      f"'{self.db_name}': {repr(ex)}")
            self._engine = engine
            engine = None
            return True
        finally:
            if engine:
                engine.dispose()

    def connect(self) -> None:
        """ Connect to database, that is assumed to be existing """
        if self._engine:
            return
        engine: Any = None
        try:
            error_if(not self.db_dsn,
                     "FS downloader status database DSN was not specified")
            engine = self._create_engine(self.db_dsn)
            self._read_metadata()
            with engine.connect():
                pass
            self._engine = engine
            engine = None
        finally:
            if engine:
                engine.dispose()

    def disconnect(self) -> None:
        """ Disconnect database """
        if self._engine:
            self._engine.dispose()
            self._engine = None

    def _read_metadata(self) -> None:
        """ Reads-in metadata from an existing database """
        engine: Any = None
        try:
            engine = self._create_engine(self.db_dsn)
            with engine.connect():
                pass
            metadata = sa.MetaData()
            metadata.reflect(bind=engine)
            for table_name in self.ALL_TABLE_NAMES:
                error_if(
                    table_name not in metadata.tables,
                    f"Table '{table_name}' not present in the database "
                    f"'{secret_utils.safe_dsn(self.db_dsn)}'")
            self.metadata = metadata
        except sa.exc.SQLAlchemyError as ex:
            error(f"Can't connect to database "
                  f"'{secret_utils.safe_dsn(self.db_dsn)}': {repr(ex)}")
        finally:
            if engine is not None:
                engine.dispose()

    def _execute(self, ops: List[Any]) -> Any:  # sa.CursorResult
        """ Execute database operations

        Arguments:
        ops -- List of SqlAlchemy operations to execute
        Returns resulting cursor
        """
        retry = False
        assert ops
        while True:
            if self._engine is None:
                self.connect()
            assert self._engine is not None
            try:
                with self._engine.connect() as conn:
                    for op in ops:
                        ret = conn.execute(op)
                return ret
            except sa.exc.SQLAlchemyError as ex:
                if retry:
                    error(f"FS downloader status database operation "
                          f"failed: {repr(ex)}")
                retry = True
                try:
                    self.disconnect()
                except sa.exc.SQLAlchemyError:
                    self._engine = None
                assert self._engine is None

    def _create_engine(self, dsn) -> Any:
        """ Creates synchronous SqlAlchemy engine

        Overloaded in RcacheDbAsync to create asynchronous engine

        Returns Engine object
        """
        try:
            return sa.create_engine(dsn)
        except sa.exc.SQLAlchemyError as ex:
            error(
                f"Invalid database DSN: '{secret_utils.safe_dsn(dsn)}': {ex}")
        return None  # Will never happen, appeasing pylint

    def write_milestone(self, milestone: DownloaderMilestone,
                        updated_regions: Optional[Iterable[str]] = None,
                        all_regions: Optional[List[str]] = None) -> None:
        """ Write milestone to state database

        Arguments:
        milestone        -- Milestone to write
        updated_regions  -- List of region strings of updated regions, None for
                            region-inspecific milestones
        all_regions      -- List of all region strings,  None for
                            region-inspecific milestones
        """
        table = self.metadata.tables[self.MILESTONE_TABLE_NAME]
        ops: List[Any] = []
        if all_regions is not None:
            ops.append(
                sa.delete(table).where(table.c.region.notin_(all_regions)).
                where(table.c.milestone == milestone.name))
        timestamp = datetime.datetime.now()
        ins = sa_pg.insert(table).\
            values(
                [{"milestone": milestone.name, "region": region or "",
                  "timestamp": timestamp}
                 for region in (updated_regions or [None])])
        ins = ins.on_conflict_do_update(
            index_elements=[c.name for c in table.c if c.primary_key],
            set_={c.name: ins.excluded[c.name]
                  for c in table.c if not c.primary_key})
        ops.append(ins)
        self._execute(ops)

    def read_milestone(self, milestone: DownloaderMilestone) \
            -> Dict[Optional[str], datetime.datetime]:
        """ Read milestone information from state database

        Arguments:
        milestone -- Milestone to read
        Returns by-region dictionary of milestone timetags
        """
        table = self.metadata.tables[self.MILESTONE_TABLE_NAME]
        sel = sa.select([table.c.region, table.c.timestamp]).\
            where(table.c.milestone == milestone.name)
        rp = self._execute([sel])
        return {rec.region or None: rec.timestamp for rec in rp}

    def write_alarm_reasons(
            self, reasons: Optional[Dict[AlarmType, Set[str]]] = None) \
            -> None:
        """ Write alarm reason to state database

        Arguments:
        reasons -- Map of alarm types to set of reasons
        """
        table = self.metadata.tables[self.ALARM_TABLE_NAME]
        ops: List[Any] = [sa.delete(table)]
        if reasons:
            timestamp = datetime.datetime.now()
            values: List[Dict[str, Any]] = []
            for alarm_type, alarm_reasons in reasons.items():
                values += [{"alarm_type": alarm_type.name,
                            "alarm_reason": alarm_reason,
                            "timestamp": timestamp}
                           for alarm_reason in alarm_reasons]
            ops.append(sa.insert(table).values(values))
        self._execute(ops)

    def read_alarm_reasons(self) -> List[AlarmInfo]:
        """ Read alarms information from database

        Returns set of missed milestones
        """
        table = self.metadata.tables[self.ALARM_TABLE_NAME]
        rp = self._execute(
            [sa.select(
                [table.c.alarm_type, table.c.alarm_reason,
                 table.c.timestamp])])
        return [AlarmInfo(alarm_type=AlarmType[rec.alarm_type],
                          alarm_reason=rec.alarm_reason,
                          timestamp=rec.timestamp)
                for rec in rp]

    def write_log(self, log_type: LogType, log: str) -> None:
        """ Write downloader log

        Arguments:
        log_type -- Type of log being written
        log      -- Log text
        """
        table = self.metadata.tables[self.LOG_TABLE_NAME]
        ins = sa_pg.insert(table).\
            values(log_type=log_type.name, text=log,
                   timestamp=datetime.datetime.now())
        ins = \
            ins.on_conflict_do_update(
                index_elements=["log_type"],
                set_={c.name: ins.excluded[c.name]
                      for c in table.c if not c.primary_key})
        self._execute([ins])

    def read_last_log(self, log_type: LogType) -> Optional[LogInfo]:
        """ Read log from database

        Arguments:
        log_type -- Type of log to retrieve
        Returns LogInfo object
        """
        table = self.metadata.tables[self.LOG_TABLE_NAME]
        sel = sa.select([table.c.log_type, table.c.text, table.c.timestamp]).\
            where(table.c.log_type == log_type.name)
        rp = self._execute([sel])
        row = rp.first()
        return \
            LogInfo(text=row.text, timestamp=row.timestamp,
                    log_type=LogType[row.log_type]) if row else None

    def write_check_results(
            self, check_type: CheckType,
            results: Optional[Dict[str, Optional[str]]] = None) -> None:
        """ Write check results

        Arguments:
        check_type -- Type of check
        results    -- Itemized results: dicxtionary contained error message for
                      failed checks, None for succeeded ones
        """
        table = self.metadata.tables[self.CHECKS_TABLE]
        ops: List[Any] = \
            [sa.delete(table).where(table.c.check_type == check_type.name)]
        if results:
            timestamp = datetime.datetime.now()
            ops.append(
                sa.insert(table).values(
                    [{"check_type": check_type.name, "check_item": key,
                      "errmsg": value, "timestamp": timestamp}
                     for key, value in results.items()]))
        self._execute(ops)

    def read_check_results(self) -> Dict[CheckType, List[CheckInfo]]:
        """ Read check_results
        Returns dictionary of CheckInfo items lists, indexed by check type
        """
        table = self.metadata.tables[self.CHECKS_TABLE]
        sel = sa.select([table.c.check_type, table.c.check_item,
                         table.c.errmsg, table.c.timestamp])
        rp = self._execute([sel])
        ret: Dict[CheckType, List[CheckInfo]] = {}
        for rec in rp:
            check_info = \
                CheckInfo(
                    check_type=CheckType[rec.check_type],
                    check_item=rec.check_item, errmsg=rec.errmsg,
                    timestamp=rec.timestamp)
            ret.setdefault(check_info.check_type, []).append(check_info)
        return ret
