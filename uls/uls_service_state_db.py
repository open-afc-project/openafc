""" ULS service state database """

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=invalid-name, broad-exception-caught, wildcard-import
# pylint: disable=wrong-import-order, unused-wildcard-import
# pylint: disable=too-many-statements, too-many-branches
# pylint: disable=too-many-positional-arguments, too-many-arguments

import datetime
import enum
import requests
import shlex
import sqlalchemy as sa
import sqlalchemy.dialects.postgresql as sa_pg
import subprocess
from typing import Any, cast, Dict, Iterable, List, NamedTuple, Optional, Set
import urllib.parse

import db_creator
import db_utils
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

    Private attributes::
    _arg_db_dsn    -- Connection string as passed from command line or
                      environment
    _password_file -- Password file name or None
    _full_db_dsn   -- Connection string with password substituted
    _db_name       -- Database name from connection string
    _metadata      -- Metadata. Originally as predefined, after connection -
                      actual
    _engine        -- SqlAlchemy engine (set on connection, reset to None on
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

    def __init__(self, db_dsn: str, db_password_file: Optional[str]) -> None:
        """ Constructor

        Arguments:
        db_dsn           -- Connection string to state database
        db_password_file -- File with password to use in DSN
        """
        self._arg_db_dsn = db_dsn
        self._password_file = db_password_file
        self._full_db_dsn = \
            db_utils.substitute_password(
                dsn=db_dsn, password_file=db_password_file)
        self.db_name: str = \
            urllib.parse.urlsplit(self._full_db_dsn).path.strip("/")
        self.metadata = sa.MetaData()
        sa.Table(
            self.MILESTONE_TABLE_NAME,
            self.metadata,
            sa.Column("milestone", sa.Enum(DownloaderMilestone), index=True,
                      primary_key=True),
            sa.Column("region", sa.String(), index=True, primary_key=True),
            sa.Column("timestamp", sa.DateTime(timezone=True), nullable=False))
        sa.Table(
            self.ALARM_TABLE_NAME,
            self.metadata,
            sa.Column("alarm_type", sa.Enum(AlarmType), primary_key=True,
                      index=True),
            sa.Column("alarm_reason", sa.String(), primary_key=True,
                      index=True),
            sa.Column("timestamp", sa.DateTime(timezone=True), nullable=False))
        sa.Table(
            self.LOG_TABLE_NAME,
            self.metadata,
            sa.Column("log_type", sa.Enum(LogType), index=True, nullable=False,
                      primary_key=True),
            sa.Column("text", sa.String(), nullable=False),
            sa.Column("timestamp", sa.DateTime(timezone=True), nullable=False,
                      index=True))
        sa.Table(
            self.CHECKS_TABLE,
            self.metadata,
            sa.Column("check_type", sa.Enum(CheckType), primary_key=True,
                      index=True),
            sa.Column("check_item", sa.String(), primary_key=True, index=True),
            sa.Column("errmsg", sa.String(), nullable=True),
            sa.Column("timestamp", sa.DateTime(timezone=True), nullable=False))
        self._engine: Any = None

    def create_db(self, db_creator_url: Optional[str],
                  alembic_config: Optional[str] = None,
                  alembic_initial_version: Optional[str] = None,
                  alembic_head_version: Optional[str] = None) -> bool:
        """ Creates database if absent, optionally adjust if present

        Arguments:
        db_creator_url          -- REST API URL for database creation
        alembic_config          -- Alembic config file name. None to do no
                                   Alembic manipulations
        alembic_initial_version -- Alembic version to stamp alembicless
                                   database with
        alembic_head_version    -- Alembic version to stamp newly created
                                   database with
        Returns True on success, Fail on failure (if fail_on_error is False)
        """
        engine: Any = None
        try:
            if self._engine:
                self._engine.dispose()
                self._engine = None
            try:
                db_existed = \
                    db_creator.ensure_dsn(
                        dsn=self._arg_db_dsn,
                        password_file=self._password_file)[1]
            except RuntimeError as ex:
                error(f"Error creating state database "
                      f"'{db_utils.safe_dsn(self._arg_db_dsn)}': {ex}")
            engine = self._create_engine(self._full_db_dsn)
            if not db_existed:
                self.metadata.create_all(engine)
            if alembic_config:
                error_if(not os.path.isfile(alembic_config),
                         f"Alembic directory config file '{alembic_config}' "
                         "not found")
                alembic_head_version = alembic_head_version or "head"
                if not db_existed:
                    self._alembic(alembic_config=alembic_config,
                                  args=["stamp", alembic_head_version])
                else:
                    current_version = \
                        cast(str,
                             self._alembic(alembic_config=alembic_config,
                                           args=["current"],
                                           return_stdout=True))
                    if not current_version:
                        error_if(not alembic_initial_version,
                                 "Initial Alembic version must be provided")
                        assert alembic_initial_version is not None
                        self._alembic(alembic_config=alembic_config,
                                      args=["stamp", alembic_initial_version])
                    self._alembic(alembic_config=alembic_config,
                                  args=["upgrade", alembic_head_version])
            self._read_metadata()

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
            engine = self._create_engine(self._full_db_dsn)
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
            engine = self._create_engine(self._full_db_dsn)
            with engine.connect():
                pass
            metadata = sa.MetaData()
            metadata.reflect(bind=engine)
            for table_name in self.ALL_TABLE_NAMES:
                error_if(
                    table_name not in metadata.tables,
                    f"Table '{table_name}' not present in the database "
                    f"'{db_utils.safe_dsn(self._full_db_dsn)}'")
            self.metadata = metadata
        except sa.exc.SQLAlchemyError as ex:
            error(f"Can't connect to database "
                  f"'{db_utils.safe_dsn(self._full_db_dsn)}': {repr(ex)}")
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
                f"Invalid database DSN: '{db_utils.safe_dsn(dsn)}': {ex}")

    def _alembic(self, alembic_config: str, args: List[str],
                 return_stdout: bool = False) -> Optional[str]:
        """ Do an alembic operation

        Arguments:
        alembic_config -- Alembic config file name
        args           -- Command lin etail - operatopm amd argumends
        return_stdout  -- True to return stdout
        Returns stdout if requested, None otherwise
        """
        args = ["alembic", "-c", alembic_config] + args
        logging.info(" ".join(shlex.quote(arg) for arg in args))
        env = dict(os.environ)
        env["ULS_SERVICE_STATE_DB_DSN"] = self._arg_db_dsn
        env["ULS_SERVICE_STATE_DB_PASSWORD_FILE"] = self._password_file or ""
        try:
            p = subprocess.Popen(args, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, text=True, env=env)
            stdout, stderr = p.communicate()
        except OSError as ex:
            error(f"Alembic execution failed: {ex}")
        logging.info("\n".join(s for s in (stderr, stdout) if s))
        error_if(p.returncode,
                 f"Alembic execution failed with code {p.returncode}")
        return stdout if return_stdout else None

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
        timestamp = datetime.datetime.now(datetime.timezone.utc)
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
            timestamp = datetime.datetime.now(datetime.timezone.utc)
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
                   timestamp=datetime.datetime.now(datetime.timezone.utc))
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
            timestamp = datetime.datetime.now(datetime.timezone.utc)
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
