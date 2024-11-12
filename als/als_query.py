#!/usr/bin/env python3
""" Tool for querying ALS database """

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=wrong-import-order, too-many-branches, too-many-locals
# pylint: disable=too-many-arguments, broad-exception-caught, invalid-name
# pylint: disable=too-many-statements, too-few-public-methods, too-many-lines
# pylint: disable=too-many-boolean-expressions, consider-using-f-string
# pylint: disable=too-many-positional-arguments

# Brief structural overview
# - error() and error_if() used to report errors
# - db_engine() creates SqlAlchemy engine and reads database metadata
# - decode_timezone() decodes --timezone parameter
# - decode_time() decodes --time_from and --time_to parameters
# - Printer base/interface/factory class for output format context classes
# - TablePrinter, CsvPrinter, JsonPrinter implement respective output formats
# - apply_common_limits() applies time/count filters to SqlAlchemy select
# - print_sql_if() conditionally prints SQL statement from SqlAlchemy select
# - do_log() implements log subcommand
# - Channels utility class around channels and frequencies
# - Point Utility class around GeoAlchemy points
# - AlsSelectComponent Base class for SqlAlchemy selectlist, filters and sorter
#   components
# - AlsOut base/factory/interface class for SqlAlchemy selectlist and sorter
#   components
# - SimpleAlsOut, ReqRespAlsOut, DurationAlsOut, CertificatesAlsOut,
#   LocationAlsOut, DistanceAlsOut, PsdAlsOut, EirpAlsOut, ErrorAlsOut.
#   Implementations of various selectlist and sorter components
# - AlsFilterbase/factory/interface class for SqlAlchemy filters
# - SimpleAlsFilter, DistAlsFilter, RespCodeAlsFilter, PsdAlsFilter,
#   EirpAlsFilter. Implementation of various filter components
# - AlsJoin, als_joins Join descriptors
# - get_als_line_row_lists() makes single SELECT into ALS database
# - do_als() implements als subcommand
# - do_timezones() implements timezones subcommand
# - do_help() implements help subcommand
# - main() top level dispatcher, performs command line arguments parsing

import abc
from collections.abc import Iterator
import argparse
import contextlib
import csv
import datetime
import enum
import geoalchemy2 as ga
try:
    from icecream import ic
except ImportError:
    def ic(*a):
        """ Appeasing pycodestyle. Should be lambda """
        return None if not a else (a[0] if len(a) == 1 else a)
import json
import logging
import lz4.frame
import os
import pytimeparse
import pytz
import re
import sqlalchemy as sa
import sys
import tabulate
from typing import Any, Dict, List, NamedTuple, NoReturn, Optional,  Set, \
    Tuple, Union

import utils

# Default database names for ALS and JSON logs
ALS_DB = "ALS"
LOG_DB = "AFC_LOGS"

# Environment variables for connection strings and password files
ALS_CONN_ENV = "POSTGRES_ALS_CONN_STR"
LOG_CONN_ENV = "POSTGRES_LOG_CONN_STR"
ALS_PASSWORD_ENV = "POSTGRES_ALS_PASSWORD_FILE"
LOG_PASSWORD_ENV = "POSTGRES_LOG_PASSWORD_FILE"


class PrintFormat(enum.Enum):
    """ Print formats """
    TABLE = "table"
    CSV = "csv"
    JSON = "json"
    JSON_INDENT = "indent"


def error(msg: str) -> NoReturn:
    """ Prints given msg as error message and exit abnormally """
    logging.error(msg)
    sys.exit(1)


def error_if(cond: Any, msg: str) -> None:
    """ If condition evaluates to true prints given msg as error message and
    exits abnormally """
    if cond:
        error(msg)


def unused_arguments(*args) -> None:  # pylint: disable=unused-argument
    """ Sink for unused arguments """
    return


def db_engine(arg_dsn: Optional[str], password_file: Optional[str],
              default_db: str, dsn_env: str, password_file_env: str,
              name_for_logs: str) -> Tuple[sa.engine.Engine, sa.MetaData]:
    """ Creates database engine and metadata

    Arguments:
    arg_dsn           -- DSN from command line or None
    password_file     -- Password file name from command line or None
    default_db        -- default database name
    dsn_env           -- Name of environment variable that may contain DSN
    password_file_env -- Name of environment variable that may contain
                         password file name
    name_for_logs     -- Database name to use in error messages
    Returns (engine, metadata) tuple
    """
    try:
        dsn = utils.dsn(arg_dsn=arg_dsn, password_file=password_file,
                        default_db=default_db, dsn_env=dsn_env,
                        password_file_env=password_file_env,
                        name_for_logs=name_for_logs)
    except ValueError as ex:
        error(str(ex))
    try:
        engine = sa.create_engine(dsn)
    except sa.exc.SQLAlchemyError as ex:
        error(f"Error connecting to {name_for_logs} database: {ex}")
    metadata = sa.MetaData()
    try:
        metadata.reflect(bind=engine)
    except sa.exc.SQLAlchemyError as ex:
        error(f"Error reading metadata of {name_for_logs} database: {ex}")
    return (engine, metadata)


def decode_timezone(tz: Optional[str]) -> datetime.tzinfo:
    """ Decode timezone, specified in command line

    Arguments:
    tz - None (for UTC) or timezone name
    Returns timezone object
    """
    if tz is None:
        # Default is UTC
        return datetime.timezone.utc
    # Timezones known to pytz
    try:
        return pytz.timezone(tz)
    except pytz.UnknownTimeZoneError:
        pass
    # Timezone by GMT (UTC) offset
    m = re.match(r"^(UTC|GMT)?(\+|-)?(\d{1,2})(:(\d\d))?$", tz)
    if m is not None:
        hour = int(m.group(3))
        minute = int(m.group(5)) if m.group(5) else 0
        sign = -1 if m.group(2) == "-" else 1
        return \
            datetime.timezone(
                offset=datetime.timedelta(hours=hour * sign,
                                          minutes=minute * sign),
                name=f"GMT{'+' if sign > 0 else '-'}{hour:02d}:{minute:02d}")
    # Timezone by abbreviation (note that PST is Philippine Standard Time :D )
    ret_offset: Optional[datetime.timedelta] = None
    ret: Optional[datetime.tzinfo] = None
    for tz_name in pytz.all_timezones:
        timezone = pytz.timezone(tz_name)
        if datetime.datetime.now(tz=timezone).strftime("%Z") != tz:
            continue
        offset = timezone.utcoffset(datetime.datetime.now())
        if ret_offset is None:
            ret_offset = offset
            ret = timezone
        elif ret_offset != offset:
            # Ambiguous abbreviation (such as PST)
            ret_offset = None
            ret = None
            break
    if ret is not None:
        return ret
    error(f"Timezone named {tz} not found")


def decode_time(t: Optional[str], default_tz: datetime.tzinfo) \
        -> Optional[datetime.datetime]:
    """ Decode time, specified in some command line argument

    Arguments:
    t          -- None or ISO formatted time (maybe with some omissions)
    default_tz -- Timezone to use if not specified in command line
    Returns None or timezoned datetime
    """
    if not t:
        return None
    m = \
        re.match(
            r"^((((?P<year>\d\d\d\d)-)?((?P<month>\d\d?)-))?(?P<day>\d\d?))?"
            r"(T(?P<hour>\d\d?)(:(?P<minute>\d\d?)(:(?P<second>\d\d?"
            r"(\.\d*)?)?)?)?)?"
            r"((?P<tzsign>\+|-)(?P<tzhour>\d\d?)(:(?P<tzmin>\d\d))?)?$", t)
    error_if(not m, f"Invalid ISO timedate format: {t}")
    assert m is not None
    now = datetime.datetime.now()
    kwargs: Dict[str, Union[int, float, datetime.tzinfo]] = {}
    timezone = default_tz
    try:
        for field, default in \
                [("year", now.year), ("month", now.month), ("day", now.day),
                 ("hour", 0), ("minute", 0), ("second", 0)]:
            s = m.group(field)
            kwargs[field] = \
                default if s is None \
                else (float(s) if "." in s else int(s))
        ret = datetime.datetime(**kwargs)
        if m.group("tzsign"):
            tz_sign = 1 if m.group("tzsign") == "+" else -1
            tz_hour = int(m.group("tzhour"))
            tz_min = int(m.group("tzmin")) if m.group("tzmin") else 0
            timezone = \
                datetime.timezone(
                    offset=datetime.timedelta(hours=tz_sign * tz_hour,
                                              minutes=tz_sign * tz_min))
        return ret.astimezone(timezone)
    except Exception as ex:
        error(f"Invalid ISO timedate format: {ex}")


class Printer(contextlib.AbstractContextManager):
    """ Abstract base/interface class for print method context managers

    Private attributes:
    _headings -- None or list of heading names (even if it is None initially,
                 it must be set before first print)
    _alignment - Dictionary that maps column indices to specified alignment.
                 Alignment, if specified is ">" for right, "<" fore left
    _timezone -- None or timezone for datetime print
    _count    -- Number of already printed rows
    """
    def __init__(self) -> None:
        """ Constructor """
        self._headings: Optional[List[str]] = None
        self._timezone: Optional[datetime.tzinfo] = None
        self._count = 0
        self._alignment: Dict[int, str] = {}

    def get_headings(self) -> List[str]:
        """ List of heading names (must be set to the moment of call) """
        assert self._headings is not None
        return self._headings

    def has_headings(self) -> bool:
        """ True if headings were set """
        return self._headings is not None

    def set_headings(self, headings: Optional[List[str]]) -> None:
        """ Set heading names (may only be done before first print)

        Arguments:
        headings - None or list of headings. Headings may be terminated by
                   alignment specification character: '>' or '<'
        """
        assert self._count == 0
        self._alignment = {}
        if headings is None:
            self._headings = None
            return
        self._headings = []
        for idx, heading in enumerate(headings):
            if heading and (heading[-1] in "<>"):
                self._headings.append(heading[: -1])
                self._alignment[idx] = heading[-1]
            else:
                self._headings.append(heading)

    def get_alignment(self, col_idx: int) -> Optional[str]:
        """ Returns alignment for given column, if it was specified or None """
        return self._alignment.get(col_idx)

    def set_timezone(self, timezone: Optional[datetime.tzinfo]) -> None:
        """ Set timezone (may only be done before first print) """
        assert self._count == 0
        self._timezone = timezone

    @property
    def count(self) -> int:
        """ Number of already printed rows """
        return self._count

    def print(self, row: List[Any]) -> None:
        """ Print single row """
        assert self._headings is not None
        assert len(row) == len(self._headings)
        row = list(row)
        for idx, field in enumerate(row):
            if isinstance(field, datetime.datetime):
                assert self._timezone is not None
                row[idx] = field.astimezone(self._timezone).isoformat()
        self.do_print(row)
        self._count += 1

    @abc.abstractmethod
    def do_print(self, row: List[Any]) -> None:
        """ Implementation of print(), must be defined in derived class """
        unused_arguments(row)

    @classmethod
    def factory(cls, print_format: str, headings: Optional[List[str]] = None,
                timezone: Optional[datetime.tzinfo] = None) -> "Printer":
        """ Factory returning required print object

        Arguments:
        print_format  -- Print format that defines what class to use
        timezone      -- Timezone for printing timetags
        headings      -- List of headings or None
        Returns object derived from Printer
        """
        ret: Optional["Printer"] = None
        match print_format:
            case PrintFormat.TABLE.value:
                ret = TablePrinter()
            case PrintFormat.CSV.value:
                ret = CsvPrinter()
            case PrintFormat.JSON.value:
                ret = JsonPrinter(indent=False)
            case PrintFormat.JSON_INDENT.value:
                ret = JsonPrinter(indent=True)
        assert ret is not None
        ret.set_headings(headings)
        ret.set_timezone(timezone)
        return ret


class TablePrinter(Printer):
    """ Table printer context manager

    Private attributes:
    _rows               -- List of table rows (each being a list of strings)
    _default_alignments -- List of default alignments
    """
    def __init__(self) -> None:
        """ Constructor """
        super().__init__()
        self._rows: List[List[Any]] = []
        self._default_alignments: List[str] = []

    def do_print(self, row: List[Any]) -> None:
        """ Row print implementation """
        if not self._default_alignments:
            self._default_alignments = ["left"] * len(self.get_headings())
        for idx, value in enumerate(row):
            if isinstance(value, (int, float)):
                self._default_alignments[idx] = "right"
        self._rows.append([str(i) for i in row])

    def __exit__(self, exc_type: Any, exc_value: Any, traceback: Any) -> None:
        """ Context manager exit """
        unused_arguments(exc_value, traceback)
        if exc_type is not None:
            return
        if not self._default_alignments:
            self._default_alignments = ["left"] * len(self.get_headings())
        if self.has_headings():
            print(
                tabulate.tabulate(
                    self._rows, headers=self.get_headings(),
                    tablefmt="github",
                    colalign=[{"<": "left", ">": "right",
                               None: self._default_alignments[idx]}
                              [self.get_alignment(idx)]
                              for idx in range(len(self.get_headings()))]
                    if self._rows else None))


class CsvPrinter(Printer):
    """ CSV printer context manager

    Private attributes:
    _sink   -- CsvSink object for retrieving generated CSV line
    _writer -- CSV Writer object
    """
    class CsvSink:
        """ File-like object that allows to retrieve written line

        Private attributes:
        _chunks - List of written chunks of last line """
        def __init__(self) -> None:
            """ Constructor """
            self._chunks: List[str] = []

        def write(self, s: str) -> None:
            """ Write a chunk """
            self._chunks.append(s)

        def retrieve(self) -> str:
            """ Retrieves chunks as line, stripped of line end, clears chunk
            list """
            ret = "".join(self._chunks).rstrip("\r\n")
            self._chunks = []
            return ret

    def __init__(self) -> None:
        """ Constructor """
        super().__init__()
        self._sink = self.CsvSink()
        self._writer = csv.writer(self._sink)

    def do_print(self, row: List[Any]) -> None:
        """ Output implementation """
        if self.count == 0:
            self._writer.writerow(self.get_headings())
            print(self._sink.retrieve())
        self._writer.writerow(row)
        print(self._sink.retrieve())

    def __exit__(self, exc_type: Any, exc_value: Any, traceback: Any) -> None:
        """ Context manager exit """
        unused_arguments(exc_type, exc_value, traceback)


class JsonPrinter(Printer):
    """ JSON printer print context manager

    Private attributes:
    _indent -- True to print indented JSON
    """
    def __init__(self, indent: bool) -> None:
        """ Constructor

        Arguments:
        indent -- True to print indented JSON
        """
        super().__init__()
        self._indent = indent
        print("[")

    def do_print(self, row: List[Any]) -> None:
        """ Output implementation """
        if self.count != 0:
            print(",")
        print(json.dumps(dict(zip(self.get_headings(), row)),
                         indent=4 if self._indent else None),
              end="")

    def __exit__(self, exc_type: Any, exc_value: Any, traceback: Any) -> None:
        """ Context manager exit """
        unused_arguments(exc_value, traceback)
        if exc_type is not None:
            return
        if self.count != 0:
            print(",")
        print("]")


def apply_common_limits(s: sa.sql.selectable.Select,
                        time_column: sa.Column, args: Any) \
        -> sa.sql.selectable.Select:
    """ Apply time/count SqlAlchemy limits, specified by command line switches
    common to 'log' and 'als' subcommands

    Arguments:
    s           -- Select statement to apply limits to
    time_column -- Column containing time
    args        -- Parsed command line arguments
    Returns modified Select statement
    """
    default_tz = decode_timezone(args.timezone)
    if args.max_count is not None:
        s = s.limit(args.max_count)
    if args.max_age is not None:
        s = \
            s.where(
                (sa.sql.functions.now() - time_column) <=
                sa.func.make_interval(0, 0, 0, 0, 0, 0, args.max_age))
    if args.time_from:
        s = s.where(time_column >= decode_time(t=args.time_from,
                                               default_tz=default_tz))
    if args.time_to:
        s = s.where(time_column <= decode_time(t=args.time_to,
                                               default_tz=default_tz))
    return s


def print_sql_if(cond: Any, s: Any, engine: sa.engine.Engine) -> None:
    """ Conditionally print SQL statement

    Argument:
    cond   -- Condition. Print only id evaluates to True
    s      -- SQL statement to print
    engine -- Engine that defines dialect to print for
    """
    if not cond:
        return
    logging.info(s.compile(engine, compile_kwargs={"literal_binds": True}))


def do_log(args: Any) -> None:
    """Execute "log" command.

    Arguments:
    args -- Parsed command line arguments
    """
    engine, metadata = \
        db_engine(arg_dsn=args.dsn, password_file=args.password_file,
                  default_db=LOG_DB, dsn_env=LOG_CONN_ENV,
                  password_file_env=LOG_PASSWORD_ENV,
                  name_for_logs="JSON logs")
    if args.topics:
        error_if(args.sources or args.SELECT,
                 "--sources and SELECT arguments may not be specified with "
                 "--topics")
        for topic in sorted(metadata.tables.keys()):
            print(topic)
        return
    error_if(args.topic and ((args.topic not in metadata.tables) or
                             ("source" not in metadata.tables[args.topic].c)),
             "Topic not found")
    if args.sources:
        error_if(args.SELECT, "SELECT may not be specified with --sources")
        if args.topic:
            s = sa.select([metadata.tables[args.topic].c.source]).\
                distinct().order_by(metadata.tables[args.topic].c.source)
            if args.max_count:
                s = s.limit(args.max_count)
            try:
                with engine.connect() as conn:
                    print_sql_if(args.verbose, s=s, engine=engine)
                    rp = conn.execute(s)
                    for row in rp:
                        print(row[0])
            except sa.exc.SQLAlchemyError as ex:
                error(f"Database access error: {ex}")
        else:
            with Printer.factory(print_format=args.format,
                                 headings=["Topic", "Source"]) as printer:
                for topic in sorted(metadata.tables.keys()):
                    if "source" not in metadata.tables[topic].c:
                        continue
                    s = sa.select([metadata.tables[topic].c.source]).\
                        distinct().order_by(metadata.tables[topic].c.source)
                    if args.max_count is not None:
                        s = s.limit(args.max_count - printer.count)
                    try:
                        with engine.connect() as conn:
                            print_sql_if(args.verbose, s=s, engine=engine)
                            rp = conn.execute(s)
                            for row in rp:
                                printer.print([topic, row[0]])
                    except sa.exc.SQLAlchemyError as ex:
                        error(f"Database access error: {ex}")
                    if (args.max_count is not None) and \
                            (printer.count >= args.max_count):
                        break
        return
    if args.SELECT or args.topic:
        error_if(
            args.SELECT and
            (args.topic or args.max_age or args.time_from or args.time_to
             or args.max_count or args.oldest_first),
            "SELECT may not be used together with --topic or time-related "
            "switches")
        try:
            s1: Union[sa.sql.selectable.Select, sa.sql.elements.TextClause]
            if args.SELECT:
                s1 = sa.text("SELECT " + " ".join(args.SELECT))
            else:
                time_column = metadata.tables[args.topic].c["time"]
                s1 = \
                    apply_common_limits(
                        s=sa.select(metadata.tables[args.topic]),
                        time_column=time_column, args=args).\
                    order_by(time_column.desc() if args.oldest_first
                             else time_column)
            with engine.connect() as conn, \
                    Printer.factory(
                        print_format=args.format,
                        timezone=decode_timezone(args.timezone)) as printer:
                print_sql_if(args.verbose, s=s1, engine=engine)
                rp = conn.execute(s1)
                for row in rp:
                    if printer.count == 0:
                        printer.set_headings(list(row._mapping.keys()))
                    printer.print(row._mapping.values())
        except sa.exc.SQLAlchemyError as ex:
            error(f"Database access error: {ex}")
        return
    error("Nothing to do!")


class Channels:
    """ Utility functions around 6GHz channels """
    # MHzs per unit of channel number
    MHZ_PER_N = 5

    # Describes group of channels of same bandwidth at equal intervals
    Comb = NamedTuple("Comb",
                      [
                       # First channel in group
                       ("first", int),
                       # Last channel in group
                       ("last", int),
                       # Numerical step of channels in group
                       ("step", int),
                       # Minimum frequency of first channel
                       ("start_mhz", int),
                       # Channel width in MHz
                       ("width_mhz", int)])
    _combs = \
        [
            Comb(first=2, last=2, step=4, start_mhz=5925, width_mhz=20),
            Comb(first=1, last=233, step=4, start_mhz=5945, width_mhz=20),
            Comb(first=3, last=227, step=8, start_mhz=5945, width_mhz=40),
            Comb(first=7, last=215, step=16, start_mhz=5945, width_mhz=80),
            Comb(first=15, last=207, step=32, start_mhz=5945, width_mhz=160),
            Comb(first=31, last=191, step=32, start_mhz=5945, width_mhz=320)]

    @classmethod
    def is_valid_channel(cls, channel: int, width_mhz: Optional[int]) -> bool:
        """ True if given channel number is valid (at all of for given
        bandwidth) """
        for comb in cls._combs:
            if (comb.first <= channel <= comb.last) and \
                    (((channel - comb.first) % comb.step) == 0) and \
                    ((width_mhz is None) or (comb.width_mhz == width_mhz)):
                return True
        return False

    @classmethod
    def is_valid_width(cls, width_mhz: int) -> bool:
        """ True id given bandwidth is valid """
        return any(comb.width_mhz == width_mhz for comb in cls._combs)

    @classmethod
    def channels_in_num_range(cls, range_start: int, range_end: int,
                              width_mhz: Optional[int]) -> List[int]:
        """ Channels in given numerical range (all or of given bandwidth) """
        for comb in cls._combs:
            if (width_mhz is not None) and \
                    ((comb.width_mhz != width_mhz) or
                     (comb.first == comb.last)):
                continue
            return [c for c in range(comb.first, comb.last + 1, comb.step)
                    if range_start <= c <= range_end]
        error(f" Invalid channel width: {width_mhz}")

    @classmethod
    def channels_in_freq_range(cls, range_start_mhz: Optional[float],
                               range_end_mhz: Optional[float],
                               width_mhz: Optional[int]) -> List[int]:
        """ Channels intersecting given frequency range (all or of given
        bandwidth) """
        ret: List[int] = []
        for comb in cls._combs:
            if (width_mhz is not None) and (comb.width_mhz != width_mhz):
                continue
            for channel in range(comb.first, comb.last + 1, comb.step):
                channel_start_mhz = \
                    comb.start_mhz + cls.MHZ_PER_N * (channel - comb.first)
                channel_end_mhz = channel_start_mhz + comb.width_mhz
                if ((range_start_mhz is None) or
                    (range_start_mhz < channel_end_mhz)) and \
                        ((range_end_mhz is None) or
                         (range_end_mhz > channel_start_mhz)):
                    ret.append(channel)
        return ret


class Point:
    """ WGS84 point

    Private attributes:
    _lat -- North-positive latitude in degrees
    _lon -- East-positive longitude in degrees
    """
    def __init__(self, lat_lon: str) -> None:
        """ Constructor

        Arguments:
        lat_lon -- Point coordinates as specified in --pos parameter
        """
        m = \
            re.match(
                r"^(?P<lat>(\+|-)?(\d+(\.\d*)?))(?P<lathem>[nNsS])?(,\s*|\s+)"
                r"(?P<lon>(\+|-)?(\d+(\.\d*)?))(?P<lonhem>[eEwW])?$",
                lat_lon)
        error_if(not m, f"Geodetic position '{lat_lon}' has invalid format")
        assert m is not None
        self._lat = float(m.group("lat")) * \
            (-1 if (m.group("lathem") or "") in "sS" else 1)
        self._lon = float(m.group("lon")) * \
            (-1 if (m.group("lonhem") or "") in "wW" else 1)

    def as_ga2(self) -> str:
        """ Returns FeoAlchemy point representation string """
        return f"POINT({self._lon} {self._lat})"


class AlsSelectComponent:
    """ Base class for select-related components (output items, wheres) of
    ALS Queries

    Private attributes:
    _table_names -- List of table names, associated with component. First table
                    contains columns, used in components. The whole set of
                    tables link this table(s) with pertinent columns to
                    'request_response_in_message' table (that itself might be
                    omitted, if not containing pertinent columns)
    """

    class Context:
        """ Common context used in components' generation

        Private attributes:
        _metadata      -- ALS Database metadata
        _table_aliases -- Dictionary of table aliases, indexed by alias name
        _position      -- None or position, specified by --pos
        _use_miles     -- True to use miles in command line and output
                          distances, False for kilometers
        """
        # Meters in mile
        METERS_IN_MILE = 1609.344
        # Meters in kilometer
        METERS_IN_KM = 1000

        def __init__(self) -> None:
            """ Constructor """
            self._metadata: Optional[sa.MetaData] = None
            self._table_aliases: Dict[str, Any] = {}
            self._position: Optional[Point] = None
            self._use_miles = False

        def set_metadata(self, metadata: sa.MetaData) -> None:
            """ Sets ALS Database metadata """
            self._metadata = metadata

        def set_position(self, position: Optional[Point]) -> None:
            """ Sets --pos position """
            self._position = position

        def set_use_miles(self, use_miles: bool) -> None:
            """ Sets use miles or kilometers """
            self._use_miles = use_miles

        def make_aias(self, src_name: str, alias_name: str) -> None:
            """ Creates table alias

            Arguments:
            src_name   -- Name of table in metadata
            alias_name -- Name of alias table being created
            """
            assert self._metadata is not None
            error_if(src_name not in self._metadata.tables,
                     f"Table '{src_name}' not found in ALS database")
            error_if(alias_name in self._table_aliases,
                     f"Alias name '{alias_name}' already defined")
            error_if(alias_name in self._metadata.tables,
                     f"Attempt to use name of existing ALS database table "
                     f"'{alias_name}' as an alias")
            self._table_aliases[alias_name] = \
                self._metadata.tables[src_name].alias(alias_name)

        def get_table(self, table_name: str) -> Any:
            """ Get metadata of table or alias by name """
            assert self._metadata is not None
            ret: Any = \
                self._metadata.tables.get(table_name) \
                if table_name in self._metadata.tables \
                else self._table_aliases.get(table_name)
            error_if(ret is None,
                     f"Table/alias name '{table_name}' not found in ALS "
                     f"database")
            return ret

        def get_position_ga2(self) -> str:
            """ Returns --pos position as Position object """
            error_if(not self._position, "Position not specified")
            assert self._position is not None
            return self._position.as_ga2()

        def to_meters(self, km_or_mile: float) -> float:
            """ Converts distance in external measurement units (miles or
            kilometers) to meters that are used by GeoAlchemy """
            return km_or_mile * \
                (self.METERS_IN_MILE if self._use_miles else self.METERS_IN_KM)

        def to_display_units(self, meters: float) -> float:
            """ Converts distance from meters to external units (miles or
            kilometers) """
            return meters / \
                (self.METERS_IN_MILE if self._use_miles else self.METERS_IN_KM)

    # Context singleton
    context = Context()

    def __init__(self, table_names: List[str]) -> None:
        """ Constructor

        Arguments:
        table_names -- List of table names, associated with component. First
                       table contains columns, used in components. The whole
                       set of tables link this table(s) with pertinent columns
                       to 'request_response_in_message' table (that itself
                       might be omitted, if not containing pertinent columns)
        """
        self._table_names = table_names

    @property
    def table_names(self) -> List[str]:
        """ Pertinent table names """
        return self._table_names

    def column_label(self, column_name: str,
                     table_name: Optional[str] = None) -> str:
        """ Generate select column label, that has 'table_column' form

        Arguments:
        column_name -- Column name part of label
        table_name  -- Table name part of label. None for first table in
                       'tabel_names'
        Returns column label in 'table_column' form
        """
        assert column_name is not None
        return f"{table_name or self._table_names[0]}_{column_name}"

    def column(self, column_name: str, table_name: Optional[str] = None) \
            -> sa.Column:
        """ Returns metadata of given column of given table

        Arguments:
        column_name -- Column name
        table_name  -- Table or alias name
        Returns column metadata """
        assert column_name is not None
        return self.context.get_table(table_name or self._table_names[0]).\
            c[column_name]


class AlsOut(AlsSelectComponent):
    """ ALS output element

    Private attributes:
    _cmd         -- Command line name (OUT and --order_by parameters)
    _help        -- Description of output element
    _orderable   -- True if may be used in --order_by clause
    _many_to_one -- True if may create multiple rows in select output
    """
    # Dictionary of all components, indexed by command line name
    _all: Dict[str, "AlsOut"] = {}

    def __init__(self, cmd: str, help_: str, table_names: List[str],
                 orderable: bool = True, many_to_one: bool = False) -> None:
        """ Constructor

        Arguments:
        cmd         -- Command line name (OUT and --order_by parameters)
        help_       -- Description of output element
        table_names -- List of table names, associated with component. First
                       table contains columns, used in components. The whole
                       set of tables link this table(s) with pertinent columns
                       to 'request_response_in_message' table (that itself
                       might be omitted, if not containing pertinent columns)
        orderable   -- True if may be used in --order_by clause
        many_to_one -- True if may create multiple rows in select output
        """
        super().__init__(table_names=table_names)
        self._cmd = cmd
        self._help = help_
        self._orderable = orderable
        self._many_to_one = many_to_one

    @property
    def cmd(self) -> str:
        """ Name used in command line """
        return self._cmd

    @property
    def help(self) -> str:
        """ Description of output element """
        return self._help

    @property
    def orderable(self) -> bool:
        """ True if may be used in --order_by clause """
        return self._orderable

    @property
    def many_to_one(self) -> bool:
        """ True if may create multiple rows in select output """
        return self._many_to_one

    def add_columns(self, column_dict: Dict[str, Any]) -> None:
        """ 'Abstract' function that adds columns to select list

        Arguments:
        column_dict -- Dictionary, whose values will be used as select list.
                       Indexed by column labels (to avoid adding same column
                       many times)
        """
        unused_arguments(column_dict)
        raise \
            NotImplementedError(
                f"{self.__class__.__name__}.add_columns() not implemented")

    def order_by(self) -> List[Any]:
        """ Optional 'abstract' function that returns list of components to
       'order by' clause of select """
        error(f"{self.__class__.__name__}.order_by() not implemented")

    def collate(self, rows: List[Any]) -> Any:
        """ Abstract function that picks values from rows, corresponded to
        single request/response (i.e. to single output row)
        """
        unused_arguments(rows)
        raise \
            NotImplementedError(
                f"{self.__class__.__name__}.collate() not implemented")

    def _get_row0(self, rows: List[Any], column_name: str,
                  table_name: Optional[str] = None) -> Any:
        """ Reads element from first row

        Arguments:
        rows        -- List of rows (first element of it is used)
        column_name -- Column name part of column label
        table_name  -- Table name part of column label. None to use name of
                       first table, associated with component
        Returns item of given label from first row
        """
        assert rows
        return rows[0][self.column_label(table_name=table_name,
                                         column_name=column_name)]

    def _get_unique(
            self, rows: List[Any], column_names: List[str],
            table_name: Optional[str] = None) -> Iterator[NamedTuple]:
        """ Return sequence of unique column value combinations

        Arguments:
        rows         -- List of rows to read from
        column_names -- List column name parts of column labels
        table_name   -- Table name part of column labels, None for first table
                        name
        Returns iterator over unique named tuples, indexed by column names (not
        labels!)
        """
        NT = NamedTuple("NT", [(column_name, Any)
                               for column_name in column_names])
        returned: Set[NT] = set()
        for row in rows:
            values = {column_name:
                      row[self.column_label(table_name=table_name,
                                            column_name=column_name)]
                      for column_name in column_names}
            ret = NT(**values)
            if any(value is not None for value in values.values()) and \
                    (ret not in returned):
                yield ret
            returned.add(ret)

    def _add_column(self, column_dict: Dict[str, Any], column_name: str,
                    table_name: Optional[str] = None, expr: Any = None) \
            -> None:
        """ Adds column to select column dictionary, if it is not yet there

        Arguments:
        column_dict -- Dictionary, whose values will be used as select list.
                       Indexed by column labels (to avoid adding same column
                       many times)
        column_name -- Column name
        table_name  -- Table name. None for first table on list
        expr        -- Expression to have in select list. None to have column
                       object
        """
        label = self.column_label(column_name=column_name,
                                  table_name=table_name)
        if label not in column_dict:
            column_dict[label] = \
                (self.column(column_name=column_name, table_name=table_name)
                 if expr is None else expr).label(label)

    @classmethod
    def all(cls) -> Dict[str, "AlsOut"]:
        """ Dictionary of all known output components """
        if not cls._all:
            for ao in [
                    SimpleAlsOut(
                        cmd="time", help_="Request RX time",
                        table_names=["afc_message"],
                        column_name="rx_time"),
                    SimpleAlsOut(
                        cmd="server", help_="AFC Server identity",
                        table_names=["afc_server", "afc_message"],
                        column_name="afc_server_name"),
                    ReqRespAlsOut(
                        cmd="request", help_="AFC Request (e.g. for WebUI)",
                        is_req=True, is_msg=False),
                    ReqRespAlsOut(
                        cmd="response", help_="AFC Response",
                        is_req=False, is_msg=False),
                    ReqRespAlsOut(
                        cmd="req_msg", help_="AFC Request message",
                        is_req=True, is_msg=True),
                    ReqRespAlsOut(
                        cmd="resp_msg", help_="AFC Response message",
                        is_req=False, is_msg=True),
                    DurationAlsOut(
                        cmd="duration", help_="Message processing duration"),
                    SimpleAlsOut(
                        cmd="serial", help_="AP serial number",
                        table_names=["device_descriptor", "request_response"],
                        column_name="serial_number"),
                    CertificatesAlsOut(
                        cmd="certificates", help_="AP Certifications"),
                    LocationAlsOut(
                        cmd="location", help_="AP Location"),
                    DistanceAlsOut(
                        cmd="distance",
                        help_="AP distance from request point"),
                    PsdAlsOut(
                        cmd="psd", help_="Max PSD results"),
                    EirpAlsOut(
                        cmd="eirp", help_="Max EIRP results"),
                    SimpleAlsOut(
                        cmd="config", help_="AFC Config",
                        table_names=["afc_config", "request_response"],
                        column_name="afc_config_text"),
                    SimpleAlsOut(
                        cmd="region",
                        help_="Region aka customer (AFC Config ID)",
                        table_names=["customer", "request_response"],
                        column_name="customer_name"),
                    ErrorAlsOut(
                        cmd="error",
                        help_="Response code and supplemental info"),
                    SimpleAlsOut(
                        cmd="dn", help_="mTLS DN",
                        table_names=["mtls_dn", "afc_message"],
                        column_name="dn_json"),
                    SimpleAlsOut(
                        cmd="uls", help_="ULS data version",
                        table_names=["uls_data_version", "request_response"],
                        column_name="uls_data_version"),
                    SimpleAlsOut(
                        cmd="geo", help_="Geodetic data version",
                        table_names=["geo_data_version", "request_response"],
                        column_name="geo_data_version")]:
                cls._all[ao.cmd] = ao
        return cls._all


class SimpleAlsOut(AlsOut):
    """ Simple output component (one orderable column verbatim)

    Private attributes:
    _column_name -- Name of column that contains desired value
    """
    def __init__(self, cmd: str, help_: str, table_names: List[str],
                 column_name: str) -> None:
        """ Constructor

        Arguments:
        cmd         -- Command line name (OUT and --order_by parameters)
        help_       -- Description of output element
        table_names -- List of table names, associated with component. First
                       table contains columns, used in components. The whole
                       set of tables link this table(s) with pertinent columns
                       to 'request_response_in_message' table (that itself
                       might be omitted, if not containing pertinent columns)
        column_name -- Name of column that contains desired value
        """
        super().__init__(cmd=cmd, help_=help_, table_names=table_names)
        self._column_name = column_name

    def add_columns(self, column_dict: Dict[str, Any]) -> None:
        """ Add columns to select list """
        self._add_column(column_dict, column_name=self._column_name)

    def order_by(self) -> List[Any]:
        """ Order by clause """
        return [self.column(column_name=self._column_name)]

    def collate(self, rows: List[Any]) -> Any:
        """ Read value from row list """
        return self._get_row0(rows, column_name=self._column_name)


class ReqRespAlsOut(AlsOut):
    """ ALS Request/response or message

    Private_attributes:
    _is_req -- True for request, False for response
    _is_msg -- True for message, false for individual request/response
    """
    def __init__(self, cmd: str, help_: str, is_req: bool, is_msg: bool) \
            -> None:
        """ Constructor

        Arguments:
        cmd    -- Command line name (OUT and --order_by parameters)
        help_  -- Description of output element
        is_req -- True for request, False for response
        is_msg -- True for message, false for individual request/response
        """
        self._is_req = is_req
        self._is_msg = is_msg
        super().__init__(
            cmd=cmd, help_=help_,
            table_names=[
                self._json_table_name(), "request_response"] +
            (["afc_message", self._envelope_table_name()] if self._is_msg
             else []),
            orderable=False)

    def add_columns(self, column_dict: Dict[str, Any]) -> None:
        """ Add columns to select list """
        self._add_column(column_dict, table_name="request_response_in_message",
                         column_name="request_id")
        if not self._is_req:
            self._add_column(column_dict,
                             table_name="request_response_in_message",
                             column_name="expire_time")
        self._add_column(column_dict,
                         table_name=self._json_table_name(),
                         column_name="compressed_json_data")
        if self._is_msg:
            self._add_column(column_dict,
                             table_name=self._envelope_table_name(),
                             column_name="envelope_json")

    def collate(self, rows: List[Any]) -> Any:
        """ Read value from row list """
        req_resp = \
            json.loads(
                lz4.frame.decompress(
                    self._get_row0(rows, table_name=self._json_table_name(),
                                   column_name="compressed_json_data")))
        req_resp["requestId"] = \
            self._get_row0(rows, table_name="request_response_in_message",
                           column_name="request_id")
        if not self._is_req:
            dt: Optional[datetime.datetime] = None
            if req_resp.get("response", {}).get("responseCode") == 0:
                dt = self._get_row0(rows,
                                    table_name="request_response_in_message",
                                    column_name="expire_time")
            if dt is not None:
                req_resp["availabilityExpireTime"] = \
                    dt.strftime("%Y-%m-%dT%H:%M:%SZ")
            elif "availabilityExpireTime" in req_resp:
                del req_resp["availabilityExpireTime"]
        if not self._is_msg:
            return req_resp
        msg = self._get_row0(rows, table_name=self._envelope_table_name(),
                             column_name="envelope_json")
        msg["availableSpectrumInquiryRequests" if self._is_req
            else "availableSpectrumInquiryResponses"] = [req_resp]
        return msg

    def _json_table_name(self) -> str:
        """ Name of JSON Table alias to use """
        return "compressed_json_rx" if self._is_req else "compressed_json_tx"

    def _envelope_table_name(self) -> str:
        """ Name  of envelope table to use """
        assert self._is_msg
        return "rx_envelope" if self._is_req else "tx_envelope"


class DurationAlsOut(AlsOut):
    """ Request computation duration """
    def __init__(self, cmd: str, help_: str) -> None:
        """ Constructor

        Arguments:
        cmd   -- Command line name (OUT and --order_by parameters)
        help_ -- Description of output element
        """
        super().__init__(cmd=cmd, help_=help_, table_names=["afc_message"])

    def add_columns(self, column_dict: Dict[str, Any]) -> None:
        """ Add columns to select list """
        self._add_column(column_dict, column_name="rx_time")
        self._add_column(column_dict, column_name="tx_time")

    def order_by(self) -> List[Any]:
        """ Order by clause """
        return [self.column(column_name="tx_time") -
                self.column(column_name="rx_time")]

    def collate(self, rows: List[Any]) -> Any:
        """ Read value from row list """
        return (self._get_row0(rows, column_name="tx_time") -
                self._get_row0(rows, column_name="rx_time")).total_seconds()


class CertificatesAlsOut(AlsOut):
    """ ALS Certificates """
    def __init__(self, cmd: str, help_: str) -> None:
        """ Constructor

        Arguments:
        cmd   -- Command line name (OUT and --order_by parameters)
        help_ -- Description of output element
        """
        super().__init__(cmd=cmd, help_=help_,
                         table_names=["certification", "device_descriptor",
                                      "request_response"],
                         many_to_one=True)

    def add_columns(self, column_dict: Dict[str, Any]) -> None:
        """ Add columns to select list """
        self._add_column(column_dict, column_name="ruleset_id")
        self._add_column(column_dict, column_name="certification_id")

    def order_by(self) -> List[Any]:
        """ Order by clause """
        return [self.column(column_name="ruleset_id"),
                self.column(column_name="certification_id")]

    def collate(self, rows: List[Any]) -> Any:
        """ Read value from row list """
        return \
            [
                {"ruleset": getattr(nt, "ruleset_id"),
                 "cert": getattr(nt, "certification_id")}
                for nt in self._get_unique(rows=rows,
                                           column_names=["ruleset_id",
                                                         "certification_id"])]


class LocationAlsOut(AlsOut):
    """ AP Location """
    def __init__(self, cmd: str, help_: str) -> None:
        """ Constructor

        Arguments:
        cmd   -- Command line name (OUT and --order_by parameters)
        help_ -- Description of output element
        """
        super().__init__(cmd=cmd, help_=help_,
                         table_names=["location", "request_response"],
                         orderable=False)

    def add_columns(self, column_dict: Dict[str, Any]) -> None:
        """ Add columns to select list """
        self._add_column(column_dict, column_name="location_wgs84",
                         expr=self.column("location_wgs84").ST_AsText())
        self._add_column(column_dict, column_name="location_uncertainty_m")
        self._add_column(column_dict, column_name="location_type")
        self._add_column(column_dict, column_name="deployment_type")
        self._add_column(column_dict, column_name="height_m")
        self._add_column(column_dict, column_name="height_uncertainty_m")
        self._add_column(column_dict, column_name="height_type")

    def collate(self, rows: List[Any]) -> Any:
        """ Read value from row list """
        m = \
            re.search(
                r"POINT\(\s*([+-.0-9]+)\s+([+-.0-9]+)\)",
                self._get_row0(rows, column_name="location_wgs84"))
        wgs84 = ""
        for group, hemi, num_width in [(2, "NS", 9), (1, "EW", 10)]:
            if not m:
                wgs84 = "Unknown"
                break
            v = float(m.group(group))
            num_s = "{0:{num_width}.6f}".format(abs(v), num_width=num_width)
            wgs84 += f"{' ' if wgs84 else ''}{num_s}{hemi[0 if v >= 0 else 1]}"
        return \
            {
                "wgs84": wgs84,
                "type": self._get_row0(rows, column_name="location_type"),
                "loc_uncertainty_m":
                self._get_row0(rows, column_name="location_uncertainty_m"),
                "deployment":
                self._get_row0(rows, column_name="deployment_type"),
                "height_m": self._get_row0(rows, column_name="height_m"),
                "height_uncertainty_m":
                self._get_row0(rows, column_name="height_uncertainty_m"),
                "height_type": self._get_row0(rows, column_name="height_type")}


class DistanceAlsOut(AlsOut):
    """ Distance from given position """
    def __init__(self, cmd: str, help_: str) -> None:
        """ Constructor

        Arguments:
        cmd   -- Command line name (OUT and --order_by parameters)
        help_ -- Description of output element
        """
        super().__init__(cmd=cmd, help_=help_,
                         table_names=["location", "request_response"])

    def add_columns(self, column_dict: Dict[str, Any]) -> None:
        """ Add columns to select list """
        self._add_column(
            column_dict, column_name="distance",
            expr=ga.functions.ST_Distance(
                ga.functions.ST_GeographyFromText
                (self.context.get_position_ga2()),
                sa.cast(self.column(column_name="location_wgs84"),
                        ga.types.Geography())))

    def order_by(self) -> List[Any]:
        """ Order by clause """
        return [self.column(column_name="distance")]

    def collate(self, rows: List[Any]) -> Any:
        """ Read value from row list """
        return \
            self.context.to_display_units(
                self._get_row0(rows, column_name="distance"))


class PsdAlsOut(AlsOut):
    """ PSD Responses """
    def __init__(self, cmd: str, help_: str) -> None:
        """ Constructor

        Arguments:
        cmd   -- Command line name (OUT and --order_by parameters)
        help_ -- Description of output element
        """
        super().__init__(cmd=cmd, help_=help_,
                         table_names=["max_psd", "request_response"],
                         many_to_one=True)

    def add_columns(self, column_dict: Dict[str, Any]) -> None:
        """ Add columns to select list """
        self._add_column(column_dict, column_name="low_frequency_mhz")
        self._add_column(column_dict, column_name="high_frequency_mhz")
        self._add_column(column_dict, column_name="max_psd_dbm_mhz")

    def order_by(self) -> List[Any]:
        """ Order by clause """
        return [self.column(column_name="max_psd_dbm_mhz")]

    def collate(self, rows: List[Any]) -> Any:
        """ Read value from row list """
        return [
            {"range":
             f"[{getattr(nt, 'low_frequency_mhz')}-"
             f"{getattr(nt, 'high_frequency_mhz')}]MHz",
             "psd": getattr(nt, "max_psd_dbm_mhz")}
            for nt in self._get_unique(rows=rows,
                                       column_names=["low_frequency_mhz",
                                                     "high_frequency_mhz",
                                                     "max_psd_dbm_mhz"])]


class EirpAlsOut(AlsOut):
    """ EIRP Responses """
    def __init__(self, cmd: str, help_: str) -> None:
        """ Constructor

        Arguments:
        cmd   -- Command line name (OUT and --order_by parameters)
        help_ -- Description of output element
        """
        super().__init__(cmd=cmd, help_=help_,
                         table_names=["max_eirp", "request_response"],
                         many_to_one=True)

    def add_columns(self, column_dict: Dict[str, Any]) -> None:
        """ Add columns to select list """
        self._add_column(column_dict, column_name="channel")
        self._add_column(column_dict, column_name="max_eirp_dbm")

    def order_by(self) -> List[Any]:
        """ Order by clause """
        return [self.column(column_name="max_eirp_dbm")]

    def collate(self, rows: List[Any]) -> Any:
        """ Read value from row list """
        return [
            {"channel": getattr(nt, "channel"),
             "eirp": getattr(nt, "max_eirp_dbm")}
            for nt in self._get_unique(rows=rows,
                                       column_names=["channel",
                                                     "max_eirp_dbm"])]


class ErrorAlsOut(AlsOut):
    """ Computation error responses """
    def __init__(self, cmd: str, help_: str) -> None:
        """ Constructor

        Arguments:
        cmd   -- Command line name (OUT and --order_by parameters)
        help_ -- Description of output element
        """
        super().__init__(cmd=cmd, help_=help_,
                         table_names=["request_response"])

    def add_columns(self, column_dict: Dict[str, Any]) -> None:
        """ Add columns to select list """
        self._add_column(column_dict, column_name="response_code")
        self._add_column(column_dict, column_name="response_description")
        self._add_column(column_dict, column_name="response_data")

    def order_by(self) -> List[Any]:
        """ Order by clause """
        return [self.column(column_name="response_code"),
                self.column(column_name="response_description"),
                self.column(column_name="response_data")]

    def collate(self, rows: List[Any]) -> Any:
        """ Read value from row list """
        ret: Dict[str, Any] = {}
        for item, column_name in [("code", "response_code"),
                                  ("description", "response_description"),
                                  ("supplemental", "response_data")]:
            value = self._get_row0(rows, column_name=column_name)
            if value is not None:
                ret[item] = value
        return ret


class ReqRespKeyAlsOut(AlsOut):
    """ 'Invisible' output that holds key that uniquely identifies individual
    request/response """
    def __init__(self) -> None:
        """ Constructor """
        super().__init__(cmd="__req_resp_key__", help_="",
                         table_names=["request_response_in_message"])

    def add_columns(self, column_dict: Dict[str, Any]) -> None:
        """ Add columns to select list """
        self._add_column(column_dict, column_name="message_id")
        self._add_column(column_dict, column_name="request_id")

    def collate(self, rows: List[Any]) -> Any:
        """ Read value from row list """
        return (self._get_row0(rows, column_name="message_id"),
                self._get_row0(rows, column_name="request_id"))


class AlsFilter(AlsSelectComponent):
    """ Base class for filters

    Private attributes:
    _arg_name -- Name of command line filtering parameter
    """

    # Dictionary of all filter objects, indexed by command line parameter names
    _all: Dict[str, "AlsFilter"] = {}

    def __init__(self, arg_name: str, table_names: List[str]) -> None:
        """ Constructor

        Arguments:
        arg_name    -- Name of command line filtering parameter
        table_names -- List of table names, associated with component. First
                       table contains columns, used in components. The whole
                       set of tables link this table(s) with pertinent columns
                       to 'request_response_in_message' table (that itself
                       might be omitted, if not containing pertinent columns)
        """
        super().__init__(table_names=table_names)
        self._arg_name = arg_name

    @property
    def arg_name(self) -> str:
        """ Command line parameter name """
        return self._arg_name

    def apply_filter(self, s: sa.sql.selectable.Select, arg_value: Any) \
            -> sa.sql.selectable.Select:
        """ 'Abstract' function that applies 'where' clause to select statement

        Arguments:
        s         -- Select statement to apply clause to
        arg_value -- Command line parameter value
        Returns modified select statement """
        unused_arguments(s, arg_value)
        raise \
            NotImplementedError(
                f"{self.__class__.__name__}.apply_filter() not implemented")

    @classmethod
    def all(cls) -> Dict[str, "AlsFilter"]:
        """ Dictionary of all known filters """
        if not cls._all:
            for af in [
                    DistAlsFilter(arg_name="dist"),
                    SimpleAlsFilter(
                        arg_name="region",
                        table_names=["customer", "request_response"],
                        column_name="customer_name"),
                    SimpleAlsFilter(
                        arg_name="serial",
                        table_names=["device_descriptor", "request_response"],
                        column_name="serial_number"),
                    SimpleAlsFilter(
                        arg_name="certificate",
                        table_names=["certification", "device_descriptor",
                                     "request_response"],
                        column_name="certification_id"),
                    SimpleAlsFilter(
                        arg_name="ruleset",
                        table_names=["certification", "device_descriptor",
                                     "request_response"],
                        column_name="ruleset_id"),
                    MtlsCnAlsFilter(arg_name="cn"),
                    RespCodeAlsFilter(arg_name="resp_code"),
                    PsdAlsFilter(arg_name="psd"),
                    EirpAlsFilter(arg_name="eirp")]:
                cls._all[af.arg_name] = af
        return cls._all


class SimpleAlsFilter(AlsFilter):
    """ Simple filter, that filter one column by equality

    Private attributes:
    _column_name -- Name of column to filter by
    """
    def __init__(self, arg_name: str, table_names: List[str],
                 column_name: str) -> None:
        """ Constructor

        Arguments:
        arg_name    -- Name of command line filtering parameter
        table_names -- List of table names, associated with component. First
                       table contains columns, used in components. The whole
                       set of tables link this table(s) with pertinent columns
                       to 'request_response_in_message' table (that itself
                       might be omitted, if not containing pertinent columns)
        column_name -- Name of column to filter by
        """
        super().__init__(arg_name=arg_name, table_names=table_names)
        self._column_name = column_name

    def apply_filter(self, s: sa.sql.selectable.Select, arg_value: Any) \
            -> sa.sql.selectable.Select:
        """ 'Applies 'where' clause to select statement

        Arguments:
        s         -- Select statement to apply clause to
        arg_value -- Command line parameter value
        Returns modified select statement """
        return \
            s.where(self.column(column_name=self._column_name) == arg_value)


class DistAlsFilter(AlsFilter):
    """ Filter by distance """
    def __init__(self, arg_name: str) -> None:
        """ Constructor

        Arguments:
        arg_name    -- Name of command line filtering parameter
        """
        super().__init__(arg_name=arg_name,
                         table_names=["location", "request_response"])

    def apply_filter(self, s: sa.sql.selectable.Select, arg_value: Any) \
            -> sa.sql.selectable.Select:
        """ 'Applies 'where' clause to select statement

        Arguments:
        s         -- Select statement to apply clause to
        arg_value -- Command line parameter value
        Returns modified select statement """
        return \
            s.where(
                ga.functions.ST_Distance(
                    ga.functions.ST_GeographyFromText(
                        self.context.get_position_ga2()),
                    sa.cast(self.column(column_name="location_wgs84"),
                            ga.types.Geography())) <=
                self.context.to_meters(arg_value))


class MtlsCnAlsFilter(AlsFilter):
    """ Filter by mTLS CN """
    def __init__(self, arg_name: str) -> None:
        """ Constructor

        Arguments:
        arg_name    -- Name of command line filtering parameter
        """
        super().__init__(arg_name=arg_name,
                         table_names=["mtls_dn", "afc_message"])

    def apply_filter(self, s: sa.sql.selectable.Select, arg_value: Any) \
            -> sa.sql.selectable.Select:
        """ 'Applies 'where' clause to select statement

        Arguments:
        s         -- Select statement to apply clause to
        arg_value -- Command line parameter value
        Returns modified select statement """
        return \
            s.where(self.column(column_name="dn_json")["CN"].astext ==
                    arg_value) \
            if arg_value else \
            s.where((self.column(column_name="dn_text_digest",
                                 table_name="afc_message").is_(None)) |
                    sa.not_(self.column(column_name="dn_json").has_key("CN")))


class RespCodeAlsFilter(AlsFilter):
    """ Filter by ALS Response code(s) """
    def __init__(self, arg_name: str) -> None:
        """ Constructor

        Arguments:
        arg_name    -- Name of command line filtering parameter
        """
        super().__init__(arg_name=arg_name, table_names=["request_response"])

    def apply_filter(self, s: sa.sql.selectable.Select, arg_value: Any) \
            -> sa.sql.selectable.Select:
        """ 'Applies 'where' clause to select statement

        Arguments:
        s         -- Select statement to apply clause to
        arg_value -- Command line parameter value
        Returns modified select statement """
        codes: List[int] = []
        for code_s in re.split(r",\s*|\s+", arg_value):
            try:
                codes.append(int(code_s))
            except (TypeError, ValueError) as ex:
                error(f"--resp_code has invalid syntax: {ex}")
        error_if(not codes, "No codes specified in --resp_code")
        return s.where(self.column(column_name="response_code").in_(codes))


class PsdAlsFilter(AlsFilter):
    """ Filter by PSD frequency range """
    def __init__(self, arg_name: str) -> None:
        """ Constructor

        Arguments:
        arg_name    -- Name of command line filtering parameter
        """
        super().__init__(arg_name=arg_name,
                         table_names=["max_psd", "request_response"])

    def apply_filter(self, s: sa.sql.selectable.Select, arg_value: Any) \
            -> sa.sql.selectable.Select:
        """ 'Applies 'where' clause to select statement

        Arguments:
        s         -- Select statement to apply clause to
        arg_value -- Command line parameter value
        Returns modified select statement """
        m = re.match(r"^([0-9.]+)?(-([0-9.]+))?$", arg_value)
        error_if(not m, f"Frequency range '{arg_value}' has invalid syntax")
        assert m is not None
        try:
            start_mhz: Optional[float] = \
                float(m.group(1)) if m.group(1) else None
            end_mhz: Optional[float] = \
                float(m.group(3)) if m.group(3) else None
        except (ValueError, TypeError) as ex:
            error(f"Frequency range '{arg_value}' has invalid syntax: {ex}")
        freq_conditions: List[Any] = []
        if start_mhz is not None:
            freq_conditions.append(
                self.column(column_name="high_frequency_mhz") > start_mhz)
        if end_mhz is not None:
            freq_conditions.append(
                self.column(column_name="low_frequency_mhz") < end_mhz)
        return s.where(sa.and_(sa.true(), *freq_conditions))


class EirpAlsFilter(AlsFilter):
    """ Filter by EIRP channels """
    def __init__(self, arg_name: str) -> None:
        """ Constructor

        Arguments:
        arg_name    -- Name of command line filtering parameter
        """
        super().__init__(arg_name=arg_name,
                         table_names=["max_eirp", "request_response"])

    def apply_filter(self, s: sa.sql.selectable.Select, arg_value: Any) \
            -> sa.sql.selectable.Select:
        """ 'Applies 'where' clause to select statement

        Arguments:
        s         -- Select statement to apply clause to
        arg_value -- Command line parameter value
        Returns modified select statement """
        channels: Set[int] = set()
        for chan_freq in re.split(r",\s*|\s+", arg_value):
            m = re.match(r"([0-9.]+)?(-([0-9.]+))?(:(\d+))?$", chan_freq)
            error_if(not m, f"Invalid channel set specification: {arg_value}")
            assert m is not None
            try:
                start = float(m.group(1)) if m.group(1) else None
                end = float(m.group(3)) if m.group(3) else None
                width_mhz = int(m.group(5)) if m.group(5) else None
            except (ValueError, TypeError):
                error(f"Invalid channel set specification: {arg_value}")
            error_if(
                (width_mhz is not None) and
                (not Channels.is_valid_width(width_mhz)),
                f"Invalid channel width: {width_mhz}")
            if ((start is not None) and (start > 1000)) or \
                    ((end is not None) and (end > 1000)) or \
                    ((start is None) and (end is None) and
                     (width_mhz is not None)):
                range_channels = \
                    Channels.channels_in_freq_range(
                        range_start_mhz=start, range_end_mhz=end,
                        width_mhz=width_mhz)
                error_if(not range_channels,
                         f"No channels found in range '{chan_freq}'")
                channels.update(range_channels)
            elif (start is not None) and (end is None):
                error_if(start != int(start),
                         f"Invalid channel set specification: {arg_value}")
                error_if(
                    not Channels.is_valid_channel(channel=int(start),
                                                  width_mhz=width_mhz),
                    f"Channel number {int(start)} is invalid"
                    f"{' for ' + str(width_mhz) + 'MHz' if width_mhz else ''}")
                channels.add(int(start))
            elif (start is not None) and (end is not None):
                error_if(start != int(start),
                         f"Invalid channel set specification: {arg_value}")
                error_if(end != int(end),
                         f"Invalid channel set specification: {arg_value}")
                range_channels = \
                    Channels.channels_in_num_range(
                        range_start=int(start), range_end=int(end),
                        width_mhz=width_mhz)
                error_if(not range_channels,
                         f"No channels found in range '{chan_freq}'")
                channels.update(range_channels)
            else:
                error(f"Invalid channel set specification: {arg_value}")
        error_if(not channels, "No channels found for --eirp")
        return s.where(self.column(column_name="channel").in_(list(channels)))


class AlsJoin(NamedTuple):
    """ Information on how to join table to 'frame' (set of already joined
    tables) """
    # Name of table/alias being joined
    joined_table_name: str
    # To which table that is already in frame this table is being joined
    frame_table_name: str
    # Column name in table being joined
    column_name: str
    # Column name in frame table (None if same as 'column name')
    frame_column_name: Optional[str] = None
    # True for outer join
    outer: bool = False


# Join descriptors for ALS tables in feasible order
als_joins = \
    [AlsJoin(joined_table_name="afc_message",
             frame_table_name="request_response_in_message",
             column_name="message_id"),
     AlsJoin(joined_table_name="afc_server",
             frame_table_name="afc_message",
             column_name="afc_server_id",
             frame_column_name="afc_server"),
     AlsJoin(joined_table_name="rx_envelope",
             frame_table_name="afc_message",
             column_name="rx_envelope_digest"),
     AlsJoin(joined_table_name="mtls_dn",
             frame_table_name="afc_message",
             column_name="dn_text_digest", outer=True),
     AlsJoin(joined_table_name="request_response",
             frame_table_name="request_response_in_message",
             column_name="request_response_digest"),
     AlsJoin(joined_table_name="afc_config",
             frame_table_name="request_response",
             column_name="afc_config_text_digest"),
     AlsJoin(joined_table_name="customer",
             frame_table_name="request_response",
             column_name="customer_id"),
     AlsJoin(joined_table_name="uls_data_version",
             frame_table_name="request_response",
             column_name="uls_data_version_id"),
     AlsJoin(joined_table_name="geo_data_version",
             frame_table_name="request_response",
             column_name="geo_data_version_id"),
     AlsJoin(joined_table_name="location",
             frame_table_name="request_response",
             column_name="location_digest"),
     AlsJoin(joined_table_name="device_descriptor",
             frame_table_name="request_response",
             column_name="device_descriptor_digest"),
     AlsJoin(joined_table_name="compressed_json_rx",
             frame_table_name="request_response",
             column_name="compressed_json_digest",
             frame_column_name="request_json_digest"),
     AlsJoin(joined_table_name="compressed_json_tx",
             frame_table_name="request_response",
             column_name="compressed_json_digest",
             frame_column_name="response_json_digest"),
     AlsJoin(joined_table_name="max_psd",
             frame_table_name="request_response",
             column_name="request_response_digest"),
     AlsJoin(joined_table_name="max_eirp",
             frame_table_name="request_response",
             column_name="request_response_digest"),
     AlsJoin(joined_table_name="certification",
             frame_table_name="device_descriptor",
             column_name="certifications_digest")]


def get_als_line_row_lists(outs: List[str], args: Any,
                           engine: sa.engine.Engine, metadata: sa.MetaData) \
        -> Dict[Any, List[Any]]:
    """ Retrieval of all or some output items

    Arguments:
    outs     -- List of OUT items to retrieve
    args     -- Command line arguments
    engine   -- SqlAlchemy engine
    metadata -- Database metadata
    Returns dictionary, in which every entry represents single output 'line',
    indexed by some stable key. Each value is a list of select rows, that
    contains data for this output 'line'
    """
    rrk_out = ReqRespKeyAlsOut()
    column_dict: Dict[str, Any] = {}
    if not args.distinct:
        rrk_out.add_columns(column_dict)
    table_names: Set[str] = {"request_response_in_message"}
    for out_name in outs:
        out_dsc = AlsOut.all()[out_name]
        table_names.update(out_dsc.table_names)
        out_dsc.add_columns(column_dict)
    s = sa.select(list(column_dict.values()))
    if args.distinct:
        s = s.distinct()
    for ob_name in args.order_by:
        m = re.match(r"^([^,]+)(,desc)?$", ob_name, flags=re.IGNORECASE)
        error_if(not m, f"Invalid --order_by specifier: '{ob_name}'")
        assert m is not None
        als_out = AlsOut.all().get(m.group(1))
        error_if(als_out is None, f"Unknown --order_by item '{m.group(1)}")
        assert als_out is not None
        error_if(not als_out.orderable, f"Can't order by '{m.group(1)}'")
        assert als_out is not None
        table_names.update(als_out.table_names)
        for ob in als_out.order_by():
            if m.group(2):
                s = s.order_by(ob.desc())
            else:
                s = s.order_by(ob)
    if any(arg is not None
           for arg in (args.max_age, args.time_from, args.time_to)):
        table_names.add("afc_message")
    for filter_arg, filter_dsc in AlsFilter.all().items():
        if getattr(args, filter_arg) is None:
            continue
        table_names.update(filter_dsc.table_names)
        s = filter_dsc.apply_filter(s, arg_value=getattr(args, filter_arg))
    s = \
        apply_common_limits(
            s, args=args,
            time_column=metadata.tables["afc_message"].c.rx_time)

    joined = {"request_response_in_message"}
    j = AlsSelectComponent.context.get_table("request_response_in_message")
    for aj in als_joins:
        if aj.joined_table_name not in table_names:
            continue
        assert aj.frame_table_name in joined
        assert aj.joined_table_name not in joined
        joined.add(aj.joined_table_name)
        frame_table = AlsSelectComponent.context.get_table(aj.frame_table_name)
        joined_table = \
            AlsSelectComponent.context.get_table(aj.joined_table_name)
        j = j.join(joined_table,
                   (joined_table.c.month_idx == frame_table.c.month_idx) &
                   (joined_table.c[aj.column_name] ==
                    frame_table.c[aj.frame_column_name or aj.column_name]),
                   isouter=aj.outer)
    s = s.select_from(j)
    ret: Dict[Any, List[Any]] = {}
    try:
        with engine.connect() as conn:
            print_sql_if(args.verbose, s=s, engine=engine)
            rp = conn.execute(s)
            for idx, row in enumerate(rp):
                if args.distinct:
                    ret[idx] = [row]
                else:
                    ret.setdefault(rrk_out.collate([row]), []).append(row)
    except sa.exc.SQLAlchemyError as ex:
        error(f"Database access error: {ex}")
    return ret


def do_als(args: Any) -> None:
    """Execute "als" command.

    Arguments:
    args -- Parsed command line arguments
    """
    if args.outs:
        with Printer.factory(
                print_format=PrintFormat.TABLE.value,
                headings=["Name", "Meaning", "Sortable"]) as printer:
            for als_out in AlsOut.all().values():
                printer.print(
                    [als_out.cmd, als_out.help, "Yes"
                     if als_out.orderable else "No"])
        return
    engine, metadata = \
        db_engine(arg_dsn=args.dsn, password_file=args.password_file,
                  default_db=ALS_DB, dsn_env=ALS_CONN_ENV,
                  password_file_env=ALS_PASSWORD_ENV, name_for_logs="ALS")
    timezone = decode_timezone(args.timezone)
    if args.decode_errors:
        columns = metadata.tables["decode_error"].c
        s = \
            apply_common_limits(
                sa.select([columns.time, columns.code_line, columns.msg,
                           columns.data]).order_by(columns.time),
                args=args, time_column=columns.time)
        with engine.connect() as conn, \
                Printer.factory(
                    print_format=args.format, timezone=timezone,
                    headings=["Time", "Line", "Message", "Data"]) as printer:
            print_sql_if(args.verbose, s=s, engine=engine)
            rp = conn.execute(s)
            for row in rp:
                printer.print(row._mapping.values())
        return
    error_if(not args.OUT, "No output items specified")
    unknown_outs = [out for out in args.OUT if out not in AlsOut.all()]
    error_if(unknown_outs,
             f"Invalid values foe OUT parameter: {', '.join(unknown_outs)}")
    AlsSelectComponent.context.set_metadata(metadata)
    AlsSelectComponent.context.set_position(args.pos if args.pos else None)
    AlsSelectComponent.context.set_use_miles(args.miles)
    AlsSelectComponent.context.make_aias(src_name="compressed_json",
                                         alias_name="compressed_json_tx")
    AlsSelectComponent.context.make_aias(src_name="compressed_json",
                                         alias_name="compressed_json_rx")
    unknown_outs = \
        [out for out in args.OUT if out not in AlsOut.all()]
    error_if(unknown_outs,
             f"Unknown OUT specifiers: {', '.join(unknown_outs)}")
    line_dict: Dict[Any, List[Any]] = {}
    if args.distinct:
        mto_outs = [out for out in args.OUT if AlsOut.all()[out].many_to_one]
        error_if(len(mto_outs) > 1,
                 f"This items may not be printed together if --distinct is "
                 f"specified: {', '.join(mto_outs)}")
        for key, rows in \
                get_als_line_row_lists(outs=args.OUT, args=args, engine=engine,
                                       metadata=metadata).items():
            line_dict[key] = [AlsOut.all()[out].collate(rows=rows)
                              for out in args.OUT]
    else:
        out_idx = 0
        while out_idx < len(args.OUT):
            mto_count = 0
            outs: List[str] = []
            for out in args.OUT[out_idx:]:
                if AlsOut.all()[out].many_to_one:
                    mto_count += 1
                    if mto_count > 1:
                        break
                outs.append(out)
            keys_to_delete: List[Any] = []
            for key, rows in \
                    get_als_line_row_lists(outs=outs, args=args,
                                           engine=engine,
                                           metadata=metadata).items():
                if out_idx:
                    line = line_dict.get(key)
                    if line is None:
                        keys_to_delete.append(key)
                        continue
                else:
                    line = []
                    line_dict[key] = line
                for out in outs:
                    line.append(AlsOut.all()[out].collate(rows=rows))
            for key in keys_to_delete:
                del line_dict[key]
            out_idx += len(outs)
    with Printer.factory(print_format=args.format, headings=args.OUT,
                         timezone=timezone) as printer:
        for line in line_dict.values():
            printer.print(line)


def do_timezones(args: Any) -> None:
    """Execute "timezones" command.

    Arguments:
    args -- Parsed command line arguments
    """
    unused_arguments(args)
    with Printer.factory(
            print_format=PrintFormat.TABLE.value,
            headings=["Name", "Abbrev", "GMT Offset>"]) as printer:
        for tz_name in sorted(pytz.all_timezones):
            tz = pytz.timezone(tz_name)
            offset_sec = \
                int(tz.utcoffset(datetime.datetime.now()).total_seconds())
            printer.print(
                [tz_name, datetime.datetime.now(tz=tz).strftime("%Z"),
                 f"{'-' if offset_sec < 0 else ''}"
                 f"{abs(offset_sec) // 3600}:"
                 f"{abs(offset_sec) % 3600 // 60:02d}"])


def do_help(args: Any) -> None:
    """Execute "help" command.

    Arguments:
    args -- Parsed command line arguments (also contains 'argument_parser' and
            'subparsers' fields)
    """
    if not args.subcommand:
        args.argument_parser.print_help()
    else:
        subparser = args.subparsers.choices.get(args.subcommand)
        error_if(subparser is None, f"Unknown subcommand '{args.subcommand}'")
        assert subparser is not None
        subparser.print_help()


def main(argv: List[str]) -> None:
    """Do the deed.

    Arguments:
    argv -- Program arguments
    """
    # Database connection string switches
    switches_dsn = argparse.ArgumentParser(add_help=False)
    switches_dsn.add_argument(
        "--dsn",
        metavar="[postgres://][USER][:PASSWORD][@HOST][:PORT][/DB][?OPTIONS]",
        help=f"PostgreSQL connection string (DSN). Omitted parts are taken "
        f"either from correspondent environment variable ({ALS_CONN_ENV} for "
        f"'als' subcommand, {LOG_CONN_ENV} for 'log' subcommand) or from "
        f"defaults. Also password may be taken from password file")
    switches_dsn.add_argument(
        "--password_file", metavar="PASSWORD_FILE",
        help=f"File containing password for connection string (DSN). If not "
        f"specified - file name taken from environment variable "
        f"({ALS_PASSWORD_ENV} for 'als' subcommand, {LOG_PASSWORD_ENV} for "
        f"'log' subcommand) or password is taken from connection string or "
        f"default value is used")
    switches_dsn.add_argument(
        "--verbose", action="store_true",
        help="Print SQL statements being executed")

    # Time interval switches
    switches_time = argparse.ArgumentParser(add_help=False)
    switches_time.add_argument(
        "--max_age", metavar="DURATION", type=pytimeparse.parse,
        help="From this timer ago to now. Duration specification similar to "
        "'sleep' command, e.g. '5m', 3w5d', etc. (units are w d h m s for "
        "week, day, hour, minute, second respectively)")
    switches_time.add_argument(
        "--timezone", metavar="TIMEZONE",
        help="Timezone name (e.g. UTC, US/Pacific, Asia/Urumqi, etc.) or "
        "[GMT|UTC][+|-]HH[:MM] (e.g. 11, GMT+3 or UTC-04:30). Unambiguous "
        "abbreviated names also supported, but they are risky: e.g. the only "
        "PST in the summer is Philippine time")
    switches_time.add_argument(
        "--time_from", metavar="[[[yyyy-]mm-]dd][T[hh[:mm[:ss]]]][tz]",
        help="Time interval start, specified as ISO formatted time with some "
        "possible omissions (current values assumed for date, 00 for time). "
        "If timezone not specified - one from --timezone is used (UTC if "
        "neither specified)")
    switches_time.add_argument(
        "--time_to", metavar="[[[yyyy-]mm-]dd][T[hh[:mm[:ss]]]][tz]",
        help="Time interval end, specified as ISO formatted time with some "
        "possible omissions (current values assumed for date, 00 for time). "
        "If timezone not specified - one from --timezone is used (UTC if "
        "neither specified)")

    # Common output parameters
    switches_output = argparse.ArgumentParser(add_help=False)
    switches_output.add_argument(
        "--max_count", metavar="MAX_COUNT", type=int,
        help="Maximum number of results to print (all by default)")
    switches_output.add_argument(
        "--format",
        choices=[f.value for f in PrintFormat.__members__.values()],
        default=PrintFormat.TABLE.value,
        help="Results print format: 'table' - ASCII table, 'csv' - CSV, "
        "'json' - unindented JSON, 'indented' - indented JSON")

    # Top level parser
    argument_parser = argparse.ArgumentParser(
        description="Tool for querying ALS and JSON logs databases")
    subparsers = argument_parser.add_subparsers(dest="subcommand",
                                                metavar="SUBCOMMAND")

    # Subparser for "log" command
    parser_log = subparsers.add_parser(
        "log", parents=[switches_dsn, switches_time, switches_output],
        help="Query JSON log messages")
    parser_log.add_argument(
        "--topics", action="store_true",
        help="Print list of topics, stored in database")
    parser_log.add_argument(
        "--topic", metavar="TOPIC",
        help="JSON log topic. If --sources - topic to print sources for, "
        "otherwise - topic to dump (subject to specified time and count "
        "limits)")
    parser_log.add_argument(
        "--sources", action="store_true",
        help="Print list of log sources - for all topics or for topic, "
        "specified by --topic")
    parser_log.add_argument(
        "--oldest_first", action="store_true",
        help="Sort in oldest-first order (default is latest-first")
    parser_log.add_argument(
        "SELECT", nargs="*",
        help="SELECT command body (without 'SELECT'). 'FROM' clause should "
        "use topic name, column names are 'time' (timetag), 'source' (AFC or "
        "whatever server ID) and 'log' (JSON log record). Surrounding quotes "
        "are optional")
    parser_log.set_defaults(func=do_log)

    # Subparser for "als" command
    parser_als = subparsers.add_parser(
        "als", parents=[switches_dsn, switches_time, switches_output],
        help="Query AFC request/response log")
    parser_als.add_argument(
        "--decode_errors", action="store_true",
        help="Print table of decode errors, encountered by ALS Siphon ordered "
        "by time")
    parser_als.add_argument(
        "--outs", action="store_true",
        help="Print annotated list of values for OUT parameter")
    parser_als.add_argument(
        "--miles", action="store_true",
        help="Distances are in miles (default is kilometers)")
    parser_als.add_argument(
        "--pos", metavar="LAT,LON", type=Point,
        help="FS position specified by WGS84 latitude and longitude degrees. "
        "Hemispheres specified with sign (north/east positive) or suffix "
        "(N/S/E/W). --dist (or, in future --azimuth) should also be specified")
    parser_als.add_argument(
        "--dist", metavar="DISTANCE", type=float,
        help="Maximum AP distance from position, specified by --pos. In "
        "kilometers or miles (see --miles)")
    parser_als.add_argument(
        "--region", metavar="REGION",
        help="Region (aka customer) name, that identifies AFC Config")
    parser_als.add_argument(
        "--serial", metavar="SERIAL",
        help="AP Serial number")
    parser_als.add_argument(
        "--certificate", metavar="CERTIFICATE",
        help="AP AFC certification name (identifies manufacturer)")
    parser_als.add_argument(
        "--ruleset", metavar="RULESET_ID",
        help="AP certification ruleset ID (identifies certification "
        "authority, i.e. country)")
    parser_als.add_argument(
        "--cn", metavar="MTLS_COMMON_NAME",
        help="CN (Common Name) field of AFC Request message mTLS certificate. "
        "Empty means messages without certificate")
    parser_als.add_argument(
        "--resp_code", metavar="CODE1[,CODE2...]",
        help="Response codes")
    parser_als.add_argument(
        "--psd", metavar="[FROM_MHZ][-TO_MHZ]",
        help="Filter output by PSD responses in given frequency range")
    parser_als.add_argument(
        "--eirp", metavar="CHANNELS_OR_FREQUENCIES",
        help="Filter output by EIRP responses. CHANNELS_OR_FREQUENCIES is "
        "comma-separated sequence of channel numbers (like '5'), channel "
        "number ranges (like '1-101'), channel ranges for given bandwidth "
        "(like '1-200:40'), MHz frequency ranges (like '6000-6500')")
    parser_als.add_argument(
        "--order_by", metavar="ITEM[,desc]", action="append", default=[],
        help=f"Sort output by given item. ',desc' means descending order. "
        f"Items are: "
        f"{', '.join([o.cmd for o in AlsOut.all().values() if o.orderable])}. "
        f"See --outs for meaning. This parameter may be specified more than "
        f"once")
    parser_als.add_argument(
        "--distinct", action="store_true",
        help=f"Only print distinct results. This parameter breaks to part "
        f"values of these items: "
        f"{', '.join([o.cmd for o in AlsOut.all().values() if o.many_to_one])}"
        f", and not more than one of them may be chosen for output (see "
        f"--outs for meaning of these items)")
    parser_als.add_argument(
        "OUT", nargs="*",
        help="Space separated list of items to output. Use --outs for "
        "annotated list of possibilities")
    parser_als.set_defaults(func=do_als)

    # Subparser for 'help' command
    parser_help = subparsers.add_parser(
        "help", add_help=False, usage="%(prog)s subcommand",
        help="Prints help on given subcommand")
    parser_help.add_argument(
        "subcommand", metavar="SUBCOMMAND", nargs="?",
        help="Name of subcommand to print help about")
    parser_help.set_defaults(func=do_help, subparsers=subparsers,
                             argument_parser=argument_parser,
                             unknown_args=True)

    # Subparser for "timezones" command
    parser_timezones = subparsers.add_parser(
        "timezones", help="Print list of known timezones")
    parser_timezones.set_defaults(func=do_timezones)

    if not argv:
        argument_parser.print_help()
        sys.exit(1)
    args, _ = argument_parser.parse_known_args(argv)
    if not getattr(args, "unknown_args", False):
        args = argument_parser.parse_args(argv)

    # Set up logging
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(
        logging.Formatter(
            f"{os.path.basename(__file__)}. %(levelname)s: %(message)s"))
    logging.getLogger().addHandler(console_handler)
    logging.getLogger().setLevel(logging.INFO)

    # Do the needful
    args.func(args)


if __name__ == "__main__":
    main(sys.argv[1:])
