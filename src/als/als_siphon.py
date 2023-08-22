#!/usr/bin/env python3
# Tool for moving data from Kafka to PostgreSQL/PostGIS database

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=raise-missing-from, logging-fstring-interpolation
# pylint: disable=too-many-lines, invalid-name, consider-using-f-string
# pylint: disable=unnecessary-pass, unnecessary-ellipsis, too-many-arguments
# pylint: disable=too-many-instance-attributes, too-few-public-methods
# pylint: disable=wrong-import-order, too-many-locals, too-many-branches

from abc import ABC, abstractmethod
import argparse
from collections.abc import Iterable
import copy
import enum
import datetime
import dateutil.tz                                  # type: ignore
import geoalchemy2 as ga                            # type: ignore
import hashlib
import heapq
import inspect
import json
import kafka                                        # type: ignore
import kafka.errors                                 # type: ignore
import logging
import lz4.frame                                    # type: ignore
import math
import os
import re
import sqlalchemy as sa                             # type: ignore
import sqlalchemy.dialects.postgresql as sa_pg      # type: ignore
import sys
from typing import Any, Dict, Generic, List, NamedTuple, Optional, Set, \
    Tuple, Type, TypeVar, Union
import uuid

# This script version
VERSION = "0.1"

# Kafka topic for ALS logs
ALS_KAFKA_TOPIC = "ALS"

# Type for JSON objects
JSON_DATA_TYPE = Union[Dict[str, Any], List[Any]]

# Type for database column
COLUMN_DATA_TYPE = Optional[Union[int, float, str, bytes, bool,
                                  datetime.datetime, uuid.UUID, Dict, List]]

# Type for database row dictionary
ROW_DATA_TYPE = Dict[str, COLUMN_DATA_TYPE]

# Type for database operation result row
RESULT_ROW_DATA_TYPE = Dict[int, ROW_DATA_TYPE]

# Default port for Kafka servers
KAFKA_PORT = 9092

# Default Kafka server
DEFAULT_KAFKA_SERVER = f"localhost:{KAFKA_PORT}"

# Default Kafka client ID
DEFAULT_KAFKA_CLIENT_ID = "siphon_@"


def dp(*args, **kwargs):
    """Print debug message

    Arguments:
    args   -- Format and positional arguments. If latter present - formatted
              with %
    kwargs -- Keyword arguments. If present formatted with format()
    """
    msg = args[0] if args else ""
    if len(args) > 1:
        msg = msg % args[1:]
    if args and kwargs:
        msg = msg.format(**kwargs)
    fi = inspect.getframeinfo(inspect.currentframe().f_back)
    print("DP %s()@%d: %s" % (fi.function, fi.lineno, msg))


def error(msg: str) -> None:
    """ Prints given msg as error message and exit abnormally """
    logging.error(msg)
    sys.exit(1)


def error_if(cond: Any, msg: str) -> None:
    """ If condition evaluates to true prints given msg as error message and
    exits abnormally """
    if cond:
        error(msg)


class LineNumber:
    """ Utility functions around line numbers """
    # Function names to ignore by LineNumber.exc()
    _EXC_STACK_IGNORE: Set[str] = set()

    @classmethod
    def exc(cls) -> Optional[int]:
        """ Line number of last exception """
        def is_ignored_tb(t: Any) -> bool:
            """ True if given frame should be ignored, as it is not in this
            module or marked to be ignored """
            f_code = t.tb_frame.f_code
            return (os.path.basename(f_code.co_filename) !=
                    os.path.basename(__file__)) or \
                (f_code.co_name in cls._EXC_STACK_IGNORE)

        last_local_line: Optional[int] = None
        tb = sys.exc_info()[2]
        while tb is not None:
            if not is_ignored_tb(tb):
                last_local_line = tb.tb_lineno
            tb = tb.tb_next
        return last_local_line

    @classmethod
    def current(cls) -> int:
        """ Current caller's line number """
        f = inspect.currentframe()
        assert (f is not None) and (f.f_back is not None)
        return inspect.getframeinfo(f.f_back).lineno

    @classmethod
    def stack_trace_ignore(cls, func):
        """ Decorator to mark functions for ignore in exc() """
        cls._EXC_STACK_IGNORE.add(func.__name__)
        return func


class ErrorBase(Exception):
    """ Base class for exceptions in this module

    Attributes
    msg       -- Diagnostics message
    code_line -- Source code line number
    data      -- Optional pertinent data - string or JSON dictionary
    """
    def __init__(self, msg: str, code_line: Optional[int],
                 data: Optional[Union[str, bytes, JSON_DATA_TYPE]] = None) \
            -> None:
        """ Constructor

        Arguments:
        msg       -- Diagnostics message
        code_line -- Source code line number
        data      -- Optional pertinent data - string or JSON dictionary
        """
        super().__init__(msg)
        self.msg = msg
        self.code_line = code_line
        self.data = data


class AlsProtocolError(ErrorBase):
    """ Exception class for errors in ALS protocol """
    pass


class JsonFormatError(ErrorBase):
    """ Exception class for errors in AFC response/request/config JSON data """
    pass


class DbFormatError(ErrorBase):
    """ Exception class for DB format inconsistencies error """
    def __init__(self, msg: str, code_line: Optional[int]) -> None:
        """ Constructor

        Arguments:
        msg       -- Diagnostics message
        code_line -- Source code line number
        """
        super().__init__(msg, code_line=code_line)


# MYPY APPEASEMENT STUFF

@LineNumber.stack_trace_ignore
def ms(s: Any) -> str:
    """ Makes sure that given value is a string.
    Asserts otherwise"""
    assert isinstance(s, str)
    return s


@LineNumber.stack_trace_ignore
def js(s: Any) -> str:
    """ Makes sure that given value is a string.
    raises TypeError otherwise """
    if not isinstance(s, str):
        raise TypeError(f"Unexpected '{s}'. Should be string")
    return s


@LineNumber.stack_trace_ignore
def jb(b: Any) -> bytes:
    """ Makes sure that given value is a bytestring.
    raises TypeError otherwise """
    if not isinstance(b, bytes):
        raise TypeError(f"Unexpected '{b}'. Should be bytes")
    return b


@LineNumber.stack_trace_ignore
def ji(i: Any) -> int:
    """ Makes sure that given value is an integer.
    raises TypeError otherwise """
    if not isinstance(i, int):
        raise TypeError(f"Unexpected '{i}'. Should be integer")
    return i


@LineNumber.stack_trace_ignore
def jd(d: Any) -> dict:
    """ Makes sure that given value is a dictionary.
    raises TypeError otherwise """
    if not isinstance(d, dict):
        raise TypeError(f"Unexpected '{d}'. Should be dictionary")
    return d


@LineNumber.stack_trace_ignore
def jod(d: Any) -> Optional[dict]:
    """ Makes sure that given value is a dictionary or None.
    raises TypeError otherwise """
    if not ((d is None) or isinstance(d, dict)):
        raise TypeError(f"Unexpected '{d}'. Should be optional dictionary")
    return d


@LineNumber.stack_trace_ignore
def jl(ll: Any) -> list:
    """ Makes sure that given value is a list.
    raises TypeError otherwise """
    if not isinstance(ll, list):
        raise TypeError(f"Unexpected '{ll}'. Should be list")
    return ll


@LineNumber.stack_trace_ignore
def ju(u: Any) -> uuid.UUID:
    """ Makes sure that given value is an UUID.
    raises TypeError otherwise """
    if not isinstance(u, uuid.UUID):
        raise TypeError(f"Unexpected '{u}'. Should be UUID")
    return u


@LineNumber.stack_trace_ignore
def jdt(dt: Any) -> datetime.datetime:
    """ Makes sure that given value is a datetime.datetime object.
    raises TypeError otherwise """
    if not isinstance(dt, datetime.datetime):
        raise TypeError(f"Unexpected '{dt}'. Should be datetime")
    return dt


def get_month_idx() -> int:
    """ Computes month index """
    d = datetime.datetime.now()
    return (d.year - 2022) * 12 + (d.month - 1)


class BytesUtils:
    """ Various bytestring-related conversions """

    @classmethod
    def json_to_bytes(cls, j: JSON_DATA_TYPE) -> bytes:
        """ Converts JSON dictionary to bytes.
        Representation is the most compact: no whitespaces

        Arguments:
        j -- Dictionary or list
        Returns UTF8 bytes string
        """
        return json.dumps(j, separators=(',', ':')).encode("utf-8")

    @classmethod
    def text_to_uuid(cls, text: str) -> uuid.UUID:
        """ Computes UUID, generated from text's MD5 digest

        Arguments:
        text -- Text to compute UUID of
        Returns UUID, made from MD5, computed from UTF8-encoded text
        """
        return uuid.UUID(bytes=hashlib.md5(text.encode("utf-8")).digest())

    @classmethod
    def json_to_uuid(cls, j: JSON_DATA_TYPE) -> uuid.UUID:
        """ Computes UUID, generated from JSON MD5 digest.
        JSON first converted to compact (non spaces) string representation,
        then to UTF-8 encoded bytes, then MD5 is computed

        Arguments:
        j -- Dictionary or listUUIDbytes string
        Returns UUID
        """
        return uuid.UUID(bytes=hashlib.md5(cls.json_to_bytes(j)).digest())


class DatabaseBase(ABC):
    """ Base class of database handlers

    Attributes:
    engine    -- Database engine
    metadata  -- Database metadata
    conn      -- Default database connection
    db_name   -- Database name
    _disposed -- True if disposed
    """

    # Driver part to use in Postgres database connection strings
    DB_DRIVER = "postgresql+psycopg2://"

    # Parts of connection string
    ConnStrParts = \
        NamedTuple(
            "ConnStrParts",
            # Driver part with trailing '://'
            [("driver", str),
             # Username
             ("user", str),
             # Host name
             ("host", str),
             # Port number
             ("port", str),
             # Database name
             ("database", str),
             # Parameters with leading '?' or empty string
             ("params", str)])

    def __init__(self, arg_conn_str: Optional[str],
                 arg_password: Optional[str]) -> None:
        """ Constructor

        Arguments:
        arg_conn_str -- Connection string from command line or None
        arg_password -- Password from command line or None
        """
        self._disposed = True
        try:
            self.engine = \
                self.create_engine(arg_conn_str=arg_conn_str,
                                   arg_password=arg_password)
            self.db_name = \
                self.parse_conn_str(arg_conn_str=arg_conn_str).database
            self.metadata = sa.MetaData()
            self.metadata.reflect(bind=self.engine)
            self._disposed = False
            self.conn = self.engine.connect()
            logging.info(f"{self.name_for_logs()} database connected")
        except sa.exc.SQLAlchemyError as ex:
            error(f"Can't open {self.name_for_logs()} database: {ex}")

    def dispose(self) -> None:
        """ Explicit cleanup """
        if not self._disposed:
            self._disposed = True
            self.engine.dispose()

    @classmethod
    def parse_conn_str(cls, arg_conn_str: Optional[str]) \
            -> "DatabaseBase.ConnStrParts":
        """ Parse argument/default connection string

        Arguments:
        arg_conn_str -- Connection string from command line or None
        Returns ConnStrParts
        """
        cs_re = \
            re.compile(
                r"^((?P<driver>[^/ ]+)://)?"
                r"(?P<user>[^@/ ]+?)?"
                r"(@(?P<host>[^:/ ]+)(:(?P<port>\d+))?)?"
                r"(/(?P<database>[^?]+))?"
                r"(?P<params>\?\S.+)?$")
        m_arg = cs_re.match((arg_conn_str or "").strip())
        error_if(not m_arg,
                 f"{cls.name_for_logs()} database connection string "
                 f"'{arg_conn_str or ''}' has invalid format")
        assert m_arg is not None
        m_def = cs_re.match(cls.default_conn_str())
        assert m_def is not None
        error_if(m_arg.group("driver") and
                 (m_arg.group("driver") != m_def.group("driver")),
                 f"{cls.name_for_logs()} database connection string has "
                 f"invalid database driver name '{m_arg.group('driver')}'. "
                 f"If specified it must be '{m_def.group('driver')}'")
        return \
            cls.ConnStrParts(
                driver=m_def.group("driver"),
                user=m_arg.group("user") or m_def.group("user"),
                host=m_arg.group("host") or m_def.group("host"),
                port=m_arg.group("port") or m_def.group("port"),
                database=m_arg.group("database") or m_def.group("database"),
                params=(m_arg.group("params") or m_def.group("params")) or "")

    @classmethod
    def create_engine(cls, arg_conn_str: Optional[str],
                      arg_password: Optional[str]) -> sa.engine.base.Engine:
        """ Creates SqlAlchemy engine

        Arguments:
        arg_conn_str -- Connection string from command line or None
        arg_password -- Password from command line or None
        Returns SqlAlchemy engine
        """
        conn_str_parts = cls.parse_conn_str(arg_conn_str=arg_conn_str)
        return \
            sa.create_engine(
                f"{cls.DB_DRIVER}"
                f"{conn_str_parts.user}"
                f"{(':' + arg_password) if arg_password else ''}"
                f"@{conn_str_parts.host}:{conn_str_parts.port}/"
                f"{conn_str_parts.database}{conn_str_parts.params}")

    @classmethod
    @abstractmethod
    def default_conn_str(cls) -> str:
        """ Default connection string """
        ...

    @classmethod
    @abstractmethod
    def name_for_logs(cls) -> str:
        """ Database alias name to use for logs and error messages """
        ...


class AlsDatabase(DatabaseBase):
    """ ALS Database handler """

    @classmethod
    def default_conn_str(cls) -> str:
        """ Default connection string """
        return "postgresql://postgres@localhost:5432/ALS"

    @classmethod
    def name_for_logs(cls) -> str:
        """ Database alias name to use for logs and error messages """
        return "ALS"


class LogsDatabase(DatabaseBase):
    """ Log database handler """

    # Log record with data to write to database
    Record = NamedTuple("Record", [("source", str),
                                   ("time", datetime.datetime),
                                   ("log", JSON_DATA_TYPE)])

    @classmethod
    def default_conn_str(cls) -> str:
        """ Default connection string """
        return "postgresql://postgres@localhost:5432/AFC_LOGS"

    @classmethod
    def name_for_logs(cls) -> str:
        """ Database alias name to use for logs and error messages """
        return "Logs"

    def write_log(self, topic: str, records: List["LogsDatabase.Record"]) \
            -> None:
        """ Write a bunch of log records to database

        Arguments:
        topic   -- Kafka topic - serves a database table name
        records -- List of records to write
        """
        if not records:
            return
        try:
            if topic not in self.metadata.tables:
                sa.Table(
                    topic, self.metadata,
                    sa.Column("source", sa.Text(), index=True),
                    sa.Column("time", sa.DateTime(timezone=True), index=True),
                    sa.Column("log", sa_pg.JSON()),
                    keep_existing=True)
                self.metadata.create_all(self.conn, checkfirst=True)
            ins = sa.insert(self.metadata.tables[topic]).\
                values([{"source": r.source,
                         "time": r.time,
                         "log": r.log}
                        for r in records])
            self.conn.execute(ins)
        except sa.exc.SQLAlchemyError as ex:
            logging.error(f"Error writing {topic} log table: {ex}")


class InitialDatabase(DatabaseBase):
    """ Initial Postgres database (context fro creation of other databases)
    handler """

    class IfExists(enum.Enum):
        """ What to do if database being created already exists """
        Skip = "skip"
        Drop = "drop"
        Exc = "exc"

    @classmethod
    def default_conn_str(cls) -> str:
        """ Default connection string """
        return "postgresql://postgres@localhost:5432/postgres"

    @classmethod
    def name_for_logs(cls) -> str:
        """ Database alias name to use for logs and error messages """
        return "Initial"

    def create_db(self, db_name: str,
                  if_exists: "InitialDatabase.IfExists",
                  conn_str: str, password: Optional[str] = None,
                  template: Optional[str] = None) -> bool:
        """ Create database

        Arguments:
        db_name   -- Name of database to create
        if_exists -- What to do if database already exists
        conn_str  -- Connection string to database being created
        password  -- Password for connection string to database being created
        template  -- Name of template database to use
        Returns True if database was created, False if it already exists
        """
        with self.engine.connect() as conn:
            try:
                if if_exists == self.IfExists.Drop:
                    conn.execute("commit")
                    conn.execute(f'drop database if exists "{db_name}"')
                conn.execute("commit")
                template_clause = f' template "{template}"' if template else ""
                conn.execute(f'create database "{db_name}"{template_clause}')
                logging.info(f"Database '{db_name}' successfully created")
                return True
            except sa.exc.ProgrammingError:
                if if_exists != self.IfExists.Skip:
                    raise
                engine = self.create_engine(arg_conn_str=conn_str,
                                            arg_password=password)
                engine.execute("select 1")
                engine.dispose()
                logging.info(
                    f"Already existing database '{db_name}' will be used")
                return False

    def drop_db(self, db_name: str) -> None:
        """ Drop given database """
        with self.engine.connect() as conn:
            conn.execute("commit")
            conn.execute(f'drop database "{db_name}"')


# Fully qualified position in Kafka queue on certain Kafka cluster
KafkaPosition = \
    NamedTuple("KafkaPosition",
               [("topic", str), ("partition", int), ("offset", int)])


class KafkaPositions:
    """ Collection of partially-processed Kafka messages' offsets

    Private attributes:
    _topics -- By-topic collection of by-partition collection of offset status
               information
    """
    class OffsetInfo:
        """ Information about single offset

        Attributes:
        kafka_offset -- Offset in partition
        processed    -- Processed status
        """
        def __init__(self, kafka_offset: int) -> None:
            self.kafka_offset = kafka_offset
            self.processed = False

        def __lt__(self, other: "KafkaPositions.OffsetInfo") -> bool:
            """ Offset-based comparison for heap queue use """
            return self.kafka_offset < other.kafka_offset

        def __eq__(self, other: Any) -> bool:
            """ Equality comparison in vase heap queue will need it """
            return isinstance(other, self.__class__) and \
                (self.kafka_offset == other.kafka_offset)

        def __repr__(self) -> str:
            """ Debug print representation """
            return f"<{self.kafka_offset}, {'T' if self.processed else 'F'}>"

    class PartitionOffsets:
        """ Collection of offset information objects within partition

        Private attributes:
        _queue   -- Heap queue of offset information objects
        _catalog -- Catalog of offset information objects by offset
        """
        def __init__(self):
            """ Constructor """
            self._queue: List["KafkaPositions.OffsetInfo"] = []
            self._catalog: Dict[int, "KafkaPositions.OffsetInfo"] = {}

        def is_empty(self) -> bool:
            """ True if collection is empty (hence might be safely deleted) """
            return not self._queue

        def add(self, offset: int) -> None:
            """ Add information about (not processed) offset to collection """
            if offset in self._catalog:
                return
            oi = KafkaPositions.OffsetInfo(offset)
            heapq.heappush(self._queue, oi)
            self._catalog[offset] = oi

        def mark_processed(self, offset: Optional[int]) -> None:
            """ Mark given offset or all topic offsets as processed """
            if offset is not None:
                if offset in self._catalog:
                    self._catalog[offset].processed = True
            else:
                for offset_info in self._catalog.values():
                    offset_info.processed = True

        def get_processed_offset(self) -> Optional[int]:
            """ Computes partition commit level

            Returns Maximum offset at and below which all offsets marked as
            processed. None if there is no such offset. Offsets at and below
            returned offset are removed from collection """
            ret: Optional[int] = None
            while self._queue and self._queue[0].processed:
                ret = heapq.heappop(self._queue).kafka_offset
                assert ret is not None
                del self._catalog[ret]
            return ret

        def __repr__(self) -> str:
            """ Debug print representation """
            return f"<{self._catalog}>"

    def __init__(self):
        """ Constructor """
        self._topics: Dict[str, Dict[int,
                                     "KafkaPositions.PartitionOffsets"]] = {}

    def add(self, kafka_position: KafkaPosition) -> None:
        """ Add given position (topic/partition/offset) as nonprocessed """
        partition_offsets = \
            self._topics.setdefault(kafka_position.topic, {}).\
            get(kafka_position.partition)
        if partition_offsets is None:
            partition_offsets = self.PartitionOffsets()
            self._topics[kafka_position.topic][kafka_position.partition] = \
                partition_offsets
        partition_offsets.add(kafka_position.offset)

    def mark_processed(self, kafka_position: Optional[KafkaPosition] = None,
                       topic: Optional[str] = None) -> None:
        """ Mark given position or all positions in a topic as processed

        Arguments:
        kafka_position -- Position to mark as processed or None
        topic          -- Topic to mark as processed or None """
        if kafka_position is not None:
            partition_offsets = \
                self._topics.setdefault(kafka_position.topic, {}).\
                get(kafka_position.partition)
            if partition_offsets is not None:
                partition_offsets.mark_processed(kafka_position.offset)
        if topic is not None:
            for partition_offsets in self._topics.get(topic, {}).values():
                partition_offsets.mark_processed(None)

    def get_processed_offsets(self) -> Dict[str, Dict[int, int]]:
        """ Computes commit levels for all offsets in collection

        Returns by-topic/partition commit levels (offets at or below which are
        all marked processed). Ofets at or below returned levels are removed
        from collection """
        ret: Dict[str, Dict[int, int]] = {}
        for topic, partitions in self._topics.items():
            for partition, partition_offsets in partitions.items():
                processed_offset = partition_offsets.get_processed_offset()
                if processed_offset is not None:
                    ret.setdefault(topic, {})[partition] = processed_offset
            for partition in ret.get(topic, {}):
                if partitions[partition].is_empty():
                    del partitions[partition]
        return ret


# Type for Kafka keys of ALS messages
AlsMessageKeyType = bytes


class AlsMessage:
    """ Single ALS message (AFC Request, Response or Config)

    Attributes:
    raw_msg         -- Message in raw form (as received from Kafka)
    version         -- Message format version
    afc_server      -- AFC Server ID
    time_tag        -- Time tag
    msg_type        -- Message type (one of AlsMessage.MsgType)
    json_str        -- Content of AFC Request/Response/Config as string
    customer        -- Customer (for Config) or None
    geo_data_id     -- Geodetic data ID (if Config) or None
    uls_id          -- ULS ID (if Config) or None
    request_indexes -- Indexes of requests to which config is related (if
                       Config) or None
    """
    # ALS message format version
    FORMAT_VERSION = "1.0"

    class MsgType(enum.Enum):
        """ ALS message type string """
        Request = "AFC_REQUEST"
        Response = "AFC_RESPONSE"
        Config = "AFC_CONFIG"

    # Maps values to MsgType enum instances
    value_to_type = {t.value: t for t in MsgType}

    def __init__(self, raw_msg: bytes) -> None:
        """ Constructor

        Arguments:
        raw_msg -- Message value as retrieved from Kafka """
        self.raw_msg = raw_msg
        try:
            msg_dict = json.loads(raw_msg)
        except json.JSONDecodeError as ex:
            raise AlsProtocolError(f"Malforemed JSON of ALS message: {ex}",
                                   code_line=LineNumber.exc())
        try:
            self.version: str = msg_dict["version"]
            self.afc_server: str = msg_dict["afcServer"]
            self.time_tag = datetime.datetime.fromisoformat(msg_dict["time"])
            self.msg_type: "AlsMessage.MsgType" = \
                self.value_to_type[msg_dict["dataType"]]
            self.json_str: str = msg_dict["jsonData"]
            is_config = self.msg_type == self.MsgType.Config
            self.customer: Optional[str] \
                = msg_dict["customer"] if is_config else None
            self.geo_data_id: Optional[str] = \
                msg_dict["geoDataVersion"] if is_config else None
            self.uls_id: Optional[str] = \
                msg_dict["ulsId"] if is_config else None
            self.request_indexes: Optional[Set[int]] = \
                set(int(i) for i in msg_dict.get("requestIndexes", [])) \
                if is_config else None
        except (LookupError, TypeError, ValueError) as ex:
            raise AlsProtocolError(f"Invalid content of ALS message: {ex}",
                                   code_line=LineNumber.exc(), data=msg_dict)
        if self.version != self.FORMAT_VERSION:
            raise AlsProtocolError(
                f"Unsupported format version: '{self.version}'",
                code_line=LineNumber.exc(), data=msg_dict)
        if not isinstance(self.json_str, str):
            raise AlsProtocolError("'jsonData' missing",
                                   code_line=LineNumber.exc(), data=msg_dict)
        if is_config and not \
                all(isinstance(x, str) for x in
                    (self.customer, self.geo_data_id, self.uls_id)):
            raise AlsProtocolError(
                "Missing config fields",
                code_line=LineNumber.current(), data=msg_dict)


class AlsMessageBundle:
    """ Request/Response/Config(s) bundle

    Private attributes:
    _message_key      -- Kafka message key
    _kafka_positions  -- KafkaPositions (registry of completed/incomplete
                         offsets)
    _afc_server       -- AFC Server ID
    _last_update      -- Time of last ALS message (from local clock)
    _request_msg      -- Request message as JSON dictionary (None if not yet
                         arrived)
    _request_timetag  -- Timetag of request message (None if not yet arrived)
    _response_msg     -- Response message as JSON dictionary (None if not yet
                         arrived)
    _response_timetag -- Timetag of response message (None if not yet arrived)
    _configs          -- Dictionary of AfcConfigInfo objects, ordered by
                         individual request sequential indexes (or None if for
                         all requests)
    _assembled        -- True if bundle has all necessary parts
    _store_parts      -- Bundle in StoreParts representation. None if not yet
                         computed
    _als_positions    -- Set of positions of ALS messages used in this bundle
    """
    # Top-level JSON keys in 'invariant_json' dictionary
    JRR_REQUEST_KEY = "request"
    JRR_RESPONSE_KEY = "response"
    JRR_CONFIG_TEXT_KEY = "afc_config_text"
    JRR_CUSTOMER_KEY = "customer"
    JRR_ULS_KEY = "uls_data_id"
    JRR_GEO_KEY = "geo_data_id"

    # Information about single AFC request/response
    RequestResponse = \
        NamedTuple(
            "RequestResponse",
            # Dictionary indexed by 'JRR_...' keys. 'response' has empty (or
            # nonexistent) 'availabilityExpireTime' field. Both 'request' and
            # 'response' have 'requestId' field removed
            [("invariant_json", JSON_DATA_TYPE),
             # 'availabilityExpireTime', retrieved from 'response'
             ("expire_time", Optional[datetime.datetime])])

    # AFC Configuration information
    AfcConfigInfo = \
        NamedTuple(
            "AfcConfigInfo", [("config_str", str), ("customer", str),
                              ("geo_data_id", str), ("uls_id", str)])

    # Messages arranged to form used in convenient for store in DB
    StoreParts = \
        NamedTuple(
            "StoreParts",
            # AFC Server ID
            [("afc_server", str),
             # AFC Request message with empty
             # 'availableSpectrumInquiryRequests' list
             ("rx_envelope", JSON_DATA_TYPE),
             # AFC Response message with empty
             # 'availableSpectrumInquiryResponses' list
             ("tx_envelope", JSON_DATA_TYPE),
             # AFC Request message timetag
             ("rx_timetag", datetime.datetime),
             # AFC Response message timetag
             ("tx_timetag", datetime.datetime),
             # Dictionary of RequestResponse objects, indexed by 'requestId'
             # field values
             ("request_responses", Dict[str, RequestResponse]),
             # List of requests with no responses
             ("orphan_requests", List[JSON_DATA_TYPE]),
             # List of responses with no requests
             ("orphan_responses", List[JSON_DATA_TYPE])])

    def __init__(self, message_key: AlsMessageKeyType,
                 kafka_positions: KafkaPositions) -> None:
        """ Constructor

        message_key     -- Raw key from Kafka message
        kafka_positions -- KafkaPositions (registry of completed/incomplete
                           offsets)
        """
        self._message_key = message_key
        self._kafka_positions = kafka_positions
        self._afc_server = ""
        self._last_update = datetime.datetime.now()
        self._request_msg: Optional[Dict[str, Any]] = None
        self._request_timetag: Optional[datetime.datetime] = None
        self._response_msg: Optional[JSON_DATA_TYPE] = None
        self._response_timetag: Optional[datetime.datetime] = None
        self._configs: Dict[Optional[int],
                            "AlsMessageBundle.AfcConfigInfo"] = {}
        self._assembled = False
        self._store_parts: Optional["AlsMessageBundle.StoreParts"] = None
        self._als_positions: Set[KafkaPosition] = set()

    def message_key(self) -> AlsMessageKeyType:
        """ Kafka message key """
        return self._message_key

    def assembled(self) -> bool:
        """ True if bundle fully assembled (ave got all pertinent ALS messages)
        """
        return self._assembled

    def last_update(self) -> datetime.datetime:
        """ Local time of last update """
        return self._last_update

    def dump(self) -> JSON_DATA_TYPE:
        """ Dump for debug purposes """
        if self._store_parts is not None:
            return self._store_parts._asdict()
        return \
            {"key": self._message_key.decode("latin-1"),
             "afc_server": self._afc_server,
             "last_update": self._last_update.isoformat(),
             "request_msg": self._request_msg,
             "request_timetag":
                self._request_timetag.isoformat() if self._request_timetag
                else None,
             "response_msg": self._response_msg,
             "response_timetag":
                self._response_timetag.isoformat() if self._response_timetag
                else None,
             "configs": {k: cfg._asdict() for k, cfg in self._configs.items()}
                if self._configs else None}

    def request_count(self) -> int:
        """ Number of container requests """
        assert self._request_msg is not None
        try:
            return \
                len(jl(self._request_msg["availableSpectrumInquiryRequests"]))
        except (LookupError, TypeError, ValueError) as ex:
            raise JsonFormatError(f"Requests not found: {ex}",
                                  code_line=LineNumber.exc(),
                                  data=self._request_msg)

    def update(self, message: AlsMessage, position: KafkaPosition) -> None:
        """ Adds arrived ALS message

        Arguments:
        message  -- Kafka raw message value
        position -- Kafka message position
        """
        self._last_update = datetime.datetime.now()
        if self._assembled:
            return
        try:
            self._afc_server = message.afc_server
            if message.msg_type == AlsMessage.MsgType.Request:
                if self._request_msg is not None:
                    return
                try:
                    self._request_msg = jd(json.loads(message.json_str))
                except json.JSONDecodeError:
                    raise JsonFormatError(
                        "Malformed JSON in AFC Request message",
                        code_line=LineNumber.exc(), data=message.json_str)
                self._request_timetag = message.time_tag
            elif message.msg_type == AlsMessage.MsgType.Response:
                if self._response_msg is not None:
                    return
                try:
                    self._response_msg = json.loads(message.json_str)
                except json.JSONDecodeError:
                    raise JsonFormatError(
                        "Malformed JSON in AFC Response message",
                        code_line=LineNumber.exc(), data=message.json_str)
                self._response_timetag = message.time_tag
            else:
                if message.msg_type != AlsMessage.MsgType.Config:
                    raise ValueError(
                        f"Unexpected ALS message type: {message.msg_type}")
                assert message.msg_type == AlsMessage.MsgType.Config

                config_info = \
                    self.AfcConfigInfo(
                        config_str=js(message.json_str),
                        customer=js(message.customer),
                        geo_data_id=js(message.geo_data_id),
                        uls_id=js(message.uls_id))
                if message.request_indexes:
                    for i in message.request_indexes:
                        self._configs[i] = config_info
                else:
                    self._configs[None] = config_info
            self._check_config_indexes(message)
            self._assembled = (self._request_msg is not None) and \
                (self._response_msg is not None) and bool(self._configs) and \
                ((None in self._configs) or
                 (len(self._configs) == self.request_count()))
            self._als_positions.add(position)
        except (LookupError, TypeError, ValueError) as ex:
            raise JsonFormatError(f"ALS message decoding problem: {ex}",
                                  code_line=LineNumber.exc(),
                                  data=message.raw_msg)

    def take_apart(self) -> "AlsMessageBundle.StoreParts":
        """ Return (assembled) message contents in StoreParts form """
        assert self._assembled
        if self._store_parts:
            return self._store_parts
        self._store_parts = \
            self.StoreParts(
                afc_server=self._afc_server,
                rx_envelope=jd(self._request_msg),
                tx_envelope=jd(self._response_msg),
                rx_timetag=jdt(self._request_timetag),
                tx_timetag=jdt(self._response_timetag),
                request_responses={}, orphan_requests=[], orphan_responses=[])
        requests: List[JSON_DATA_TYPE] = \
            jl(jd(self._request_msg)["availableSpectrumInquiryRequests"])
        responses: List[JSON_DATA_TYPE] = \
            jl(jd(self._response_msg)["availableSpectrumInquiryResponses"])
        jd(self._store_parts.rx_envelope)[
            "availableSpectrumInquiryRequests"] = []
        jd(self._store_parts.tx_envelope)[
            "availableSpectrumInquiryResponses"] = []
        response_map = {jd(r)["requestId"]: r for r in responses}
        for req_idx, request in enumerate(requests):
            req_id = js(jd(request)["requestId"])
            response = jod(response_map.get(req_id))
            if response is None:
                self._store_parts.orphan_requests.append(request)
                continue
            del response_map[req_id]

            config_info = \
                self._configs[req_idx if req_idx in self._configs else None]
            expire_time_str: Optional[str] = \
                response.get("availabilityExpireTime")
            if expire_time_str is not None:
                expire_time_str = expire_time_str.replace("Z", "+00:00")
                if "+" not in expire_time_str:
                    expire_time_str += "+00:00"
                expire_time = datetime.datetime.fromisoformat(expire_time_str)
                response["availabilityExpireTime"] = ""
            else:
                expire_time = None
            jd(request)["requestId"] = ""
            response["requestId"] = ""
            self._store_parts.request_responses[req_id] = \
                self.RequestResponse(
                    invariant_json={
                        self.JRR_REQUEST_KEY: request,
                        self.JRR_RESPONSE_KEY: response,
                        self.JRR_CONFIG_TEXT_KEY: config_info.config_str,
                        self.JRR_CUSTOMER_KEY: config_info.customer,
                        self.JRR_ULS_KEY: config_info.uls_id,
                        self.JRR_GEO_KEY: config_info.geo_data_id},
                    expire_time=expire_time)
        for orphan in response_map.values():
            self._store_parts.orphan_responses.append(orphan)
        return self._store_parts

    def _check_config_indexes(self, message: AlsMessage) -> None:
        """ Ensure that config indexes in Config ALS message are valid """
        if (self._request_msg is None) or (not self._configs):
            return
        rc = self.request_count()
        if not all(0 <= i < rc for i in self._configs if i is not None):
            raise AlsProtocolError(
                f"Out of range config indexes found while processing ALS "
                f"message with key '{self._message_key!r}'",
                code_line=LineNumber.current(), data=message.raw_msg)

    def __lt__(self, other) -> bool:
        """ Comparison for by-time heap queue """
        assert isinstance(other, self.__class__)
        return self._last_update < other._last_update

    def __eq__(self, other) -> bool:
        """ Equality comparison for by-time heap queue """
        return isinstance(other, self.__class__) and \
            (self._message_key == other._message_key)

    def __del__(self):
        """ Destructor (marks ALS messages as processed """
        for als_position in self._als_positions:
            self._kafka_positions.mark_processed(als_position)


class CertificationList:
    """ List of AP Certifications

    Private attributes:
    _certifications -- Dictionary of certifications, indexed by 0-based indices
                       in list from 'certificationId' field
    """
    # Single certification
    Certification = \
        NamedTuple("Certification", [("ruleset_id", str),
                                     ("certification_id", str)])

    def __init__(self, json_data: Optional[List[JSON_DATA_TYPE]] = None) \
            -> None:
        """ Constructor

        Arguments:
        json_data -- Optional JSON dictionary - value of 'certificationId'
                     field to read self from
        """
        self._certifications: Dict[int, "CertificationList.Certification"] = {}
        if json_data is not None:
            try:
                for c in json_data:
                    cert: Dict[str, Any] = jd(c)
                    ruleset_id = cert["rulesetId"]
                    certification_id = cert["id"]
                    if not (isinstance(ruleset_id, str),
                            isinstance(certification_id, str)):
                        raise TypeError()
                    self._certifications[len(self._certifications)] = \
                        self.Certification(
                            ruleset_id=js(ruleset_id),
                            certification_id=js(certification_id))
            except (LookupError, TypeError, ValueError):
                raise JsonFormatError(
                    "Invalid DeviceDescriptor.certificationId format",
                    code_line=LineNumber.exc(), data=json_data)

    def add_certification(
            self, index: int,
            certification: "CertificationList.Certification") -> None:
        """ Adds single certification

        Arguments:
        index         -- 0-based certification indexc in certification list
        certification -- Certification to add
        """
        self._certifications[index] = certification

    def get_uuid(self) -> uuid.UUID:
        """ UUID of certification list (computed over JSON list of
        certidications) """
        return \
            BytesUtils.json_to_uuid(
                [{"rulesetId": self._certifications[idx].ruleset_id,
                  "id": self._certifications[idx].certification_id}
                 for idx in sorted(self._certifications.keys())])

    def certifications(self) -> List["CertificationList.Certification"]:
        """ List of Certification objects """
        return \
            [self._certifications[idx] for idx in sorted(self._certifications)]

    def __eq__(self, other: Any) -> bool:
        """ Eqquality comparison """
        return isinstance(other, self.__class__) and \
            (self._certifications == other._certifications)

    def __hash__(self) -> int:
        """ Hash over certifications """
        return \
            sum(idx + hash(cert) for idx, cert in self._certifications.items())


class RegRuleList:
    """ List of regulatory rules

    Privatew attributes:
    _reg_rules - By-index in list dictionary of regulatory rules names """
    def __init__(self, json_data: Optional[List[Any]] = None) -> None:
        """ Constructor

        Arguments:
        json_data -- Optional content of 'rulesetIds' field to read self from
        """
        self._reg_rules: Dict[int, str] = {}
        if json_data is not None:
            try:
                for reg_rule in json_data:
                    if not isinstance(reg_rule, str):
                        raise TypeError()
                    self._reg_rules[len(self._reg_rules)] = reg_rule
            except (LookupError, TypeError, ValueError):
                raise JsonFormatError("Invalid regulatory rule format",
                                      code_line=LineNumber.exc(),
                                      data=json_data)

    def add_rule(self, index: int, reg_rule: str) -> None:
        """ Add regulatory rule:

        Arguments:
        index    -- 0-based rule index in rule list
        reg_rule -- Rulew name
        """
        self._reg_rules[index] = reg_rule

    def get_uuid(self) -> uuid.UUID:
        """ Computes digest of self """
        return \
            BytesUtils.json_to_uuid(
                [self._reg_rules[idx]
                 for idx in sorted(self._reg_rules.keys())])

    def reg_rules(self) -> List[str]:
        """ List of rule names """
        return [self._reg_rules[idx] for idx in sorted(self._reg_rules.keys())]

    def __eq__(self, other: Any) -> bool:
        """ Equality comparison """
        return isinstance(other, self.__class__) and \
            (self._reg_rules == other._reg_rules)

    def __hash__(self) -> int:
        """ Hash value """
        return sum(idx + hash(rr) for idx, rr in self._reg_rules.items())


class AlsTableBase:
    """ Common part of ALS database table initialization

    Protected attributes:
    _adb        -- AlsDatabase object
    _table_name -- Table name
    _table      -- SQLAlchemy Table object
    """
    # Name of month index column
    MONTH_IDX_COL_NAME = "month_idx"

    def __init__(self, adb: AlsDatabase, table_name: str) -> None:
        """ Constructor
        adb        -- AlsDatabase object
        table_name -- List of sa
        """
        self._adb = adb
        self._table_name = table_name
        if self._table_name not in self._adb.metadata.tables:
            raise \
                DbFormatError(
                    f"'{self._table_name}' table not found in ALS database",
                    code_line=LineNumber.current())
        self._table: sa.Table = self._adb.metadata.tables[self._table_name]

    def get_column(self, name: str, expected_type: Optional[Type] = None) \
            -> sa.Column:
        """ Returns given column object

        Arguments:
        name          -- Column name
        expected_type -- Expected column type (None to not check)
        Returns correspondent sa.Column object
        """
        ret = self._table.c.get(name)
        if ret is None:
            raise DbFormatError(f"Column '{name}' not found in table "
                                f"'{self._table_name}' of ALS database",
                                code_line=LineNumber.current())
        if (expected_type is not None) and \
                (not isinstance(ret.type, expected_type)):
            raise DbFormatError(f"Column '{name}' of '{self._table_name}' "
                                f"table of ALS database has unexpected type",
                                code_line=LineNumber.current())
        return ret

    def get_month_idx_col(self) -> sa.Column:
        """ Returns an instance of 'month_idx' column """
        return self.get_column(self.MONTH_IDX_COL_NAME, sa.SmallInteger)


class Lookups:
    """ Collection of lookups

    Private attributes:
    _lookups -- List of registered LookupBase objects
    """
    def __init__(self):
        """ Constructor """
        self._lookups: List["LookupBase"] = []

    def register(self, lookup: "LookupBase") -> None:
        """ Register newly-created lookup """
        self._lookups.append(lookup)

    def reread(self) -> None:
        """ Signal all lookups to reread self (e.g. after transsaction failure)
        """
        for lookup in self._lookups:
            lookup.reread()


# Generic type name for lookup key value (usually int or UUID)
LookupKey = TypeVar("LookupKey")
# Generic type name for lookup table value
LookupValue = TypeVar("LookupValue")


class LookupBase(AlsTableBase, Generic[LookupKey, LookupValue], ABC):
    """ Generic base class for lookup tables (database tables, also contained
    in memory for speed of access)

    Private attributes:
    _by_value     -- Dictionary of lookup keys, ordered by (value, month_index)
                     keys
    _value_column -- SQLALchemy column for lookup tables where value contained
                     in some column of a single row. None for other cases (e.g.
                     when value should be constructed from several rows)
    _need_reread  -- True if dictionary should be reread from database on next
                     update_db()
    """
    def __init__(self, adb: AlsDatabase, table_name: str, lookups: Lookups,
                 value_column_name: Optional[str] = None) -> None:
        """ Constructor

        Arguments:
        adb               -- AlsDatabase object
        table_name        -- Database table name
        lookups           -- Lookup collection to register self in
        value_column_name -- Optional name of column containing lookup value.
                             None for lookups that contain value in more than
                             one row/column
        """
        AlsTableBase.__init__(self, adb=adb, table_name=table_name)
        lookups.register(self)
        self._value_column: Optional[sa.Column] = \
            None if value_column_name is None \
            else self.get_column(value_column_name)
        self._by_value: Dict[Tuple[LookupValue, int], LookupKey] = {}
        self._need_reread = True

    def reread(self) -> None:
        """ Request reread on next update """
        self._need_reread = True

    def update_db(self, values: Iterable[LookupValue], month_idx: int) -> None:
        """ Update lookup table with new lookup values (if any)

        Arguments:
        values   -- Sequence of lookup values (some of which may, other may not
                    already be in the table)
        month_id -- Month index to use in new records
        """
        self._reread_if_needed()
        new_value_months: Set[Tuple[LookupValue, int]] = \
            {(value, month_idx) for value in values} - \
            set(self._by_value.keys())
        if not new_value_months:
            return
        rows: List[ROW_DATA_TYPE] = []
        for value_month in new_value_months:
            if self._value_column is None:
                self._by_value[value_month] = \
                    self._key_from_value(value_month[0])
            rows += self._rows_from_value(*value_month)
        try:
            ins = sa_pg.insert(self._table).values(rows).\
                on_conflict_do_nothing()
            if self._value_column is not None:
                ins = ins.returning(self._table)
            result = self._adb.conn.execute(ins)
            if self._value_column is not None:
                for row in result:
                    key = self._key_from_row(row)
                    value = self._value_from_row_create(row)
                    self._by_value[(value, month_idx)] = key
                    assert isinstance(key, int)
                    new_value_months.remove((value, month_idx))
                for value_month in new_value_months:
                    s = sa.select([self._table]).\
                        where(self._value_column == value_month[0])
                    result = self._adb.conn.execute(s)
                    self._by_value[value_month] = \
                        self._key_from_row(list(result)[0])
        except (sa.exc.SQLAlchemyError, TypeError, ValueError) as ex:
            raise DbFormatError(
                f"Error updating '{self._table_name}': {ex}",
                code_line=LineNumber.exc())

    def key_for_value(self, value: LookupValue, month_idx: int) \
            -> LookupKey:
        """ Returns lookup key for given value """
        return self._by_value[(value, month_idx)]

    @abstractmethod
    def _key_from_row(self, row: ROW_DATA_TYPE) -> LookupKey:
        """ Required 'virtual' function. Returns key contained in given table
        row dictionary """
        ...

    @abstractmethod
    def _value_from_row_create(self, row: ROW_DATA_TYPE) -> LookupValue:
        """ Required 'virtual' function. Creates (possibly incomplete) lookup
        value contained in given table row dictionary """
        ...

    def _reread_if_needed(self) -> None:
        """ Reread lookup from database if requested """
        if not self._need_reread:
            return
        by_key: Dict[Tuple[LookupKey, int], LookupValue] = {}
        try:
            for row in self._adb.conn.execute(sa.select([self._table])):
                key = (self._key_from_row(row),
                       row[AlsTableBase.MONTH_IDX_COL_NAME])
                value = by_key.get(key)
                if value is None:
                    by_key[key] = \
                        self._value_from_row_create(row)
                else:
                    self._value_from_row_update(row, value)
        except (sa.exc.SQLAlchemyError, TypeError, ValueError) as ex:
            raise DbFormatError(f"Error reading '{self._table_name}': {ex}",
                                code_line=LineNumber.exc())
        self._by_value = {(by_key[(key, month_idx)], month_idx): key
                          for key, month_idx in by_key}
        self._need_reread = False

    def _value_from_row_update(self, row: ROW_DATA_TYPE,
                               value: LookupValue) -> None:
        """ Optional 'virtual' function. Updates incomplete value with data
        from given row dictionary. Call of this function only happens for
        lookup tables, whose values contained in a single row """
        raise NotImplementedError(f"_value_from_row_update() not implemented "
                                  f"for '{self._table_name}' table")

    def _key_from_value(self, value: LookupValue) -> LookupKey:
        """ Optional 'virtual' function. Computes lookup key from lookup value.
        Only called for tables without 'value_column' """
        raise NotImplementedError(f"_key_from_value() not implemented for "
                                  f"'{self._table_name}' table")

    @abstractmethod
    def _rows_from_value(self, value: LookupValue, month_idx: int) \
            -> List[ROW_DATA_TYPE]:
        """ Required 'virtual' function. List of database rows from given
        lookup value and month index """
        ...


class CertificationsLookup(LookupBase[uuid.UUID, CertificationList]):
    """ Certifications' lookup

    Private attributes:
    _col_digest     -- Certifications' digest column
    _col_index      -- Index in Certifications' list column
    _col_month_idx  -- Month index column
    _col_ruleset_id -- National Registration Authority name column
    _col_id         -- Certificate ID column
    """
    # Table name
    TABLE_NAME = "certification"

    def __init__(self, adb: AlsDatabase, lookups: Lookups) -> None:
        """ Constructor

        Arguments:
        adb     -- AlsDatabase object
        lookups -- Lookup collection to register self in
        """
        super().__init__(adb=adb, table_name=self.TABLE_NAME, lookups=lookups)
        self._col_digest = self.get_column("certifications_digest", sa_pg.UUID)
        self._col_index = self.get_column("certification_index",
                                          sa.SmallInteger)
        self._col_month_idx = self.get_month_idx_col()
        self._col_ruleset_id = self.get_column("ruleset_id", sa.Text)
        self._col_id = self.get_column("certification_id", sa.Text)

    def _key_from_row(self, row: ROW_DATA_TYPE) -> uuid.UUID:
        """ Certifications' digest for given row dictionary """
        return uuid.UUID(js(row[ms(self._col_digest.name)]))

    def _value_from_row_create(self, row: ROW_DATA_TYPE) -> CertificationList:
        """ Returns partial certification list from given row dictionary """
        ret = CertificationList()
        self._value_from_row_update(row, ret)
        return ret

    def _value_from_row_update(self, row: ROW_DATA_TYPE,
                               value: CertificationList) -> None:
        """ Updates given partial certification list from given row dictionary
        """
        value.add_certification(
            index=ji(row[ms(self._col_index.name)]),
            certification=CertificationList.Certification(
                ruleset_id=js(row[ms(self._col_ruleset_id.name)]),
                certification_id=js(row[ms(self._col_id.name)])))

    def _key_from_value(self, value: CertificationList) -> uuid.UUID:
        """ Table (semi) key from Certifications object """
        return value.get_uuid()

    def _rows_from_value(self, value: CertificationList, month_idx: int) \
            -> List[ROW_DATA_TYPE]:
        """ List of rows dictionaries, representing given Certifications object
        """
        ret: List[ROW_DATA_TYPE] = []
        for cert_idx, certification in enumerate(value.certifications()):
            ret.append(
                {ms(self._col_digest.name): value.get_uuid().urn,
                 ms(self._col_index.name): cert_idx,
                 ms(self._col_month_idx.name): month_idx,
                 ms(self._col_ruleset_id.name): certification.ruleset_id,
                 ms(self._col_id.name): certification.certification_id})
        return ret


class AfcConfigLookup(LookupBase[uuid.UUID, str]):
    """ AFC Configs lookup table

    Private attributes:
    _col_digest    -- Digest computed over AFC Config string column
    _col_month_idx -- Month index column
    _col_text      -- AFC Config text representation column
    _col_json      -- AFC Config JSON representation column
    """
    # Table name
    TABLE_NAME = "afc_config"

    def __init__(self, adb: AlsDatabase, lookups: Lookups) -> None:
        """ Constructor

        Arguments:
        adb     -- AlsDatabase object
        lookups -- Lookup collection to register self in
        """
        super().__init__(adb=adb, table_name=self.TABLE_NAME, lookups=lookups)
        self._col_digest = self.get_column("afc_config_text_digest",
                                           sa_pg.UUID)
        self._col_month_idx = self.get_month_idx_col()
        self._col_text = self.get_column("afc_config_text", sa.Text)
        self._col_json = self.get_column("afc_config_json", sa_pg.JSON)

    def _key_from_row(self, row: ROW_DATA_TYPE) -> uuid.UUID:
        """ Returns AFC Config digest stored in a row dictionary """
        return uuid.UUID(js(row[ms(self._col_digest.name)]))

    def _value_from_row_create(self, row: ROW_DATA_TYPE) -> str:
        """ Returns AFC Config string stored in row dictionary """
        return js(row[ms(self._col_text.name)])

    def _key_from_value(self, value: str) -> uuid.UUID:
        """ Computes AFC config digest from AFC Config string """
        return BytesUtils.text_to_uuid(value)

    def _rows_from_value(self, value: str, month_idx: int) \
            -> List[ROW_DATA_TYPE]:
        """ Returns row dictionary from AFC Config string """
        try:
            config_json = json.loads(value)
        except json.JSONDecodeError as ex:
            raise JsonFormatError(f"Malformed AFC Config JSON: {ex}",
                                  code_line=LineNumber.exc(), data=value)
        return [{ms(self._col_digest.name):
                 BytesUtils.text_to_uuid(value).urn,
                 ms(self._col_month_idx.name): month_idx,
                 ms(self._col_text.name): value,
                 ms(self._col_json.name): config_json}]


class StringLookup(LookupBase[int, str]):
    """ Lookup table with string values and sequential integer keys

    Private attributes:
    _col_id        -- Sequential index column
    _col_month_idx -- Month index column
    _col_value     -- String value column
    """
    # Lookup parameters
    Params = NamedTuple(
        "Params",
        # Table name
        [("table_name", str),
         # Sequential index column name
         ("id_col_name", str),
         # String value column name
         ("value_col_name", str)])
    # Parameter for AFC Server name lookup
    AFC_SERVER_PARAMS = Params(table_name="afc_server",
                               id_col_name="afc_server_id",
                               value_col_name="afc_server_name")
    # Parameters for Customer name lookup
    CUSTOMER_PARAMS = Params(table_name="customer",
                             id_col_name="customer_id",
                             value_col_name="customer_name")
    # Parameters for ULS ID lookup
    ULS_PARAMS_PARAMS = Params(table_name="uls_data_version",
                               id_col_name="uls_data_version_id",
                               value_col_name="uls_data_version")
    # Parameters for Geodetic data ID lookup
    GEO_DATA_PARAMS = Params(table_name="geo_data_version",
                             id_col_name="geo_data_version_id",
                             value_col_name="geo_data_version")

    def __init__(self, adb: AlsDatabase, params: "StringLookup.Params",
                 lookups: Lookups) -> None:
        """ Constructor

        Arguments:
        adb     -- AlsDatabase object
        params  -- Lookup parameters
        lookups -- Lookup collection to register self in
        """
        super().__init__(adb=adb, table_name=params.table_name,
                         value_column_name=params.value_col_name,
                         lookups=lookups)
        self._col_id = self.get_column(params.id_col_name, sa.Integer)
        self._col_month_idx = self.get_month_idx_col()
        self._col_value = self.get_column(params.value_col_name, sa.Text)

    def _key_from_row(self, row: ROW_DATA_TYPE) -> int:
        """ Key from row dictionary """
        return ji(row[ms(self._col_id.name)])

    def _value_from_row_create(self, row: ROW_DATA_TYPE) -> str:
        """ Value from row dictionary """
        return js(row[ms(self._col_value.name)])

    def _rows_from_value(self, value: str, month_idx: int) \
            -> List[ROW_DATA_TYPE]:
        """ Lookup table row dictionary for a value """
        return [{ms(self._col_month_idx.name): month_idx,
                 ms(self._col_value.name): value}]


# Generic type parameter for data key, used in data dictionaries, passed to
# update_db().
# Nature of this key might be different - it can be primary data key (usually
# data digest), foreign key or even key for consistency (to not create
# list-based version of update_db()).
# Digest primary keys (the most typical option) are expensive to compute, so
# their values should be reused, not recomputed
TableUpdaterDataKey = TypeVar("TableUpdaterDataKey")
# Generic type parameter for data value stored in table - type for values of
# data dictionary passed to update_db(). Most often a JSON object (e.g. from
# AFC message) to be written to table - maybe along with to dependent tables
TableUpdaterData = TypeVar("TableUpdaterData")


class TableUpdaterBase(AlsTableBase,
                       Generic[TableUpdaterDataKey, TableUpdaterData], ABC):
    """ Base class for tables being updated (no in-memory data copy)

    Private attributes:
    _json_obj_name       -- Name of JSON object that corresponds to data in
                            table (or something descriptive) for error
                            reporting purposes
    _data_key_columns    -- List of columns that correspond to data key
                            (usually - primary table key without 'month_idx'
                            column). Used to collect information for
                            _update_foreign_sources(), empty means not to call
                            _update_foreign_sources()
    """
    def __init__(self, adb: AlsDatabase, table_name: str, json_obj_name: str,
                 data_key_column_names: Optional[List[str]] = None) -> None:
        """ Constructor

        Arguments:
        adb                   -- AlsDatabase object
        table_name            -- Table name
        json_obj_name         -- Name of JSON object that corresponds to data
                                 in table (or something descriptive) for error
                                 reporting purposes
        data_key_column_names -- List of names of columns that correspond to
                                 data key (usually - primary table key without
                                 'month_idx' column). Used to collect
                                 information for _update_foreign_sources(),
                                 None means not to call
                                 _update_foreign_sources()
        """
        AlsTableBase.__init__(self, adb=adb, table_name=table_name)
        self._json_obj_name = json_obj_name
        self._data_key_columns = \
            [self.get_column(col_name, None)
             for col_name in (data_key_column_names or [])]

    def update_db(self,
                  data_dict: Dict[TableUpdaterDataKey, TableUpdaterData],
                  month_idx: int) -> None:
        """ Write data to table (unless they are already there)

        Arguments:
        data_dict -- Data dictionary (one item per record)
        month_idx -- Value for 'month_idx' column
        """
        try:
            if not data_dict:
                return
            rows: List[ROW_DATA_TYPE] = []
            self._update_lookups(data_objects=data_dict.values(),
                                 month_idx=month_idx)
            row_infos: \
                Dict[TableUpdaterDataKey,
                     Tuple[TableUpdaterData, List[ROW_DATA_TYPE]]] = {}
            for data_key, data_object in data_dict.items():
                rows_for_data = self._make_rows(data_key=data_key,
                                                data_object=data_object,
                                                month_idx=month_idx)
                rows += rows_for_data
                row_infos[data_key] = (data_object, rows_for_data)
            self._update_foreign_targets(row_infos=row_infos,
                                         month_idx=month_idx)
        except (LookupError, TypeError, ValueError) as ex:
            raise JsonFormatError(
                f"Invalid {self._json_obj_name} object format: {ex}",
                code_line=LineNumber.exc())
        ins = sa_pg.insert(self._table).values(rows).on_conflict_do_nothing()
        if self._data_key_columns:
            ins = ins.returning(*self._data_key_columns)
        try:
            result = self._adb.conn.execute(ins)
        except (sa.exc.SQLAlchemyError) as ex:
            raise DbFormatError(f"Error updating '{self._table_name}': {ex}",
                                code_line=LineNumber.exc())
        if not self._data_key_columns:
            return
        inserted_rows: \
            Dict[TableUpdaterDataKey,
                 Tuple[ROW_DATA_TYPE, TableUpdaterData,
                       RESULT_ROW_DATA_TYPE]] = {}
        for result_row_idx, result_row in enumerate(result):
            data_key = \
                self._data_key_from_result_row(
                    result_row=result_row, result_row_idx=result_row_idx)
            inserted_rows[data_key] = (row_infos[data_key][1][0],
                                       data_dict[data_key], result_row)
        self._update_foreign_sources(inserted_rows=inserted_rows,
                                     month_idx=month_idx)

    def _update_lookups(self, data_objects: Iterable[TableUpdaterData],
                        month_idx: int) -> None:
        """ Updates lookups, references by current table.
        Optional 'virtual' function

        Arguments:
        data_objects -- Sequence of data objects being inserted
        month_idx    -- Value for 'month_idx' column
        """
        pass

    @abstractmethod
    def _make_rows(self, data_key: TableUpdaterDataKey,
                   data_object: TableUpdaterData,
                   month_idx: int) -> List[ROW_DATA_TYPE]:
        """ Generates list of table row dictionaries for given object.
        Mandatory 'virtual' function

        Argument:
        data_key    -- Data key for a data object
        data_object -- Data object
        month_idx   -- Value for 'month_idx' column
        Returns list of row dictionaries
        """
        ...

    def _update_foreign_targets(
            self,
            row_infos: Dict[TableUpdaterDataKey,
                            Tuple[TableUpdaterData, List[ROW_DATA_TYPE]]],
            month_idx: int) -> None:
        """ Updates tables pointed to by foreign keys of this table.
        Optional 'virtual' function

        Arguments:
        row_infos -- Dictionary of data objects and row lists generated from
                     them, indexed by data keys
        month_idx -- Value for 'month_idx' column
        """
        pass

    def _data_key_from_result_row(
            self, result_row: Tuple[Any, ...], result_row_idx: int) \
            -> TableUpdaterDataKey:
        """ Returns data key from given result row (comprised of columns,
        passed to constructor as 'data_key_column_names' argument). Called only
        if _update_foreign_sources() should be called. Default implementation
        (presented here) returns value from first and only column

        Arguments:
        result_row     -- Result row (list of values of columns, passed as
                          'data_key_columns' parameter
        result_row_idx -- 0-based index of result row
        Returns data key, computed from result row and row index
        """
        assert len(result_row) == 1
        return result_row[0]

    def _update_foreign_sources(
            self,
            inserted_rows: Dict[TableUpdaterDataKey,
                                Tuple[ROW_DATA_TYPE, TableUpdaterData,
                                      RESULT_ROW_DATA_TYPE]],
            month_idx: int) -> None:
        """ Updates tables whose foreign keys point to this table.
        Optional 'virtual' function that only called if 'data_key_column_names'
        was passed to constructor

        Arguments:
        inserted_rows -- Information about newly-inserted rows (dictionary of
                         (row_dictionary, data_object, result_row) tuples,
                         ordered by data keys
        month_idx     -- Month index
        """
        raise NotImplementedError(f"_update_foreign_sources() not implemented "
                                  f"for table '{self._table_name}'")


class DeviceDescriptorTableUpdater(TableUpdaterBase[uuid.UUID,
                                                    JSON_DATA_TYPE]):
    """ Updater of device descriptor table.
    Data key is digest, computed over device descriptor JSON string

    Private data:
    _cert_lookup          -- Certificates' lookup
    _col_digest           -- Digest column
    _col_month_idx        -- Month index column
    _col_serial           -- AP Serial Number column
    _col_cert_digest      -- Certificates' digest column
    """
    TABLE_NAME = "device_descriptor"

    def __init__(self, adb: AlsDatabase, cert_lookup: CertificationsLookup) \
            -> None:
        """ Constructor

        Arguments:
        adb              -- AlsDatabase object
        cert_lookup      -- Certificates' lookup
        """
        super().__init__(adb=adb, table_name=self.TABLE_NAME,
                         json_obj_name="DeviceDescriptor")
        self._cert_lookup = cert_lookup
        self._col_digest = self.get_column("device_descriptor_digest",
                                           sa_pg.UUID)
        self._col_month_idx = self.get_month_idx_col()
        self._col_serial = self.get_column("serial_number", sa.Text)
        self._col_cert_digest = self.get_column("certifications_digest",
                                                sa_pg.UUID)

    def _update_lookups(self, data_objects: Iterable[JSON_DATA_TYPE],
                        month_idx: int) -> None:
        """ Update used lookups

        Arguments:
        data_objects -- Sequence of JSON dictionaries with device descriptors
        row_lookup   -- Rows to be written to database, ordered by device
                        descriptor digests
        month_idx    -- Month index
        """
        cert_lists: List[CertificationList] = []
        for d in data_objects:
            j = jd(d)
            try:
                cert_lists.append(CertificationList(jl(j["certificationId"])))
            except LookupError as ex:
                raise JsonFormatError(
                    f"Certifications not found: {ex}",
                    code_line=LineNumber.exc(), data=j)
        self._cert_lookup.update_db(cert_lists, month_idx=month_idx)

    def _make_rows(self, data_key: uuid.UUID, data_object: JSON_DATA_TYPE,
                   month_idx: int) -> List[ROW_DATA_TYPE]:
        """ Generates table row dictionary for given device descriptor JSON

        Arguments:
        data_key    -- Data key (DeviceDescriptor JSON digest)
        data_object -- DeviceDescriptor JSON
        month_idx   -- Month index

        Returns list of single row dictionary
        """
        try:
            json_object = jd(data_object)
            return [{ms(self._col_digest.name): data_key.urn,
                     ms(self._col_month_idx.name): month_idx,
                     ms(self._col_serial.name): json_object["serialNumber"],
                     ms(self._col_cert_digest.name):
                         self._cert_lookup.key_for_value(
                             CertificationList(json_object["certificationId"]),
                             month_idx=month_idx).urn}]
        except (LookupError, TypeError, ValueError) as ex:
            raise \
                JsonFormatError(
                    f"Invalid device DeviceDescriptor format: '{ex}'",
                    code_line=LineNumber.exc(), data=data_object)


class LocationTableUpdater(TableUpdaterBase[uuid.UUID, JSON_DATA_TYPE]):
    """ Locations table updater.
    Data key is digest over JSON Location object, data value is JSON Location
    object

    Private attributes:
    _col_digest             -- Digest over JSON Locatopn object column
    _col_month_idx          -- Month index column
    _col_location           -- Geodetic location column
    _col_loc_uncertainty    -- Location uncertainty in meters column
    _col_loc_type           -- Location type
                               (ellipse/radialPolygon/linearPolygon) column
    _col_deployment_type    -- Location deployment (indoor/outdoor) column
    _col_height             -- Height in meters column
    _col_height_uncertainty -- Height uncertainty in meters column
    _col_height_type        -- Height type (AGL/AMSL) column
    """
    # Table name
    TABLE_NAME = "location"
    # Point geodetic coordinates - in North/East positive degrees
    Point = NamedTuple("Point", [("lat", float), ("lon", float)])
    # Length of one degree in meters in latitudinal direction
    DEGREE_M = 6_371_000 * math.pi / 180

    def __init__(self, adb: AlsDatabase) -> None:
        """ Constructor

        Arguments:
        adb -- AlsDatabase object
        """
        super().__init__(adb=adb, table_name=self.TABLE_NAME,
                         json_obj_name="Location")
        self._col_digest = self.get_column("location_digest", sa_pg.UUID)
        self._col_month_idx = self.get_month_idx_col()
        self._col_location = self.get_column("location_wgs84", ga.Geography)
        self._col_loc_uncertainty = self.get_column("location_uncertainty_m",
                                                    sa.Float)
        self._col_loc_type = self.get_column("location_type", sa.Text)
        self._col_deployment_type = self.get_column("deployment_type",
                                                    sa.Integer)
        self._col_height = self.get_column("height_m", sa.Float)
        self._col_height_uncertainty = \
            self.get_column("height_uncertainty_m", sa.Float)
        self._col_height_type = self.get_column("height_type", sa.Text)

    def _make_rows(self, data_key: uuid.UUID, data_object: JSON_DATA_TYPE,
                   month_idx: int) -> List[ROW_DATA_TYPE]:
        """ Makes table row dictionary from Location JSON data object

        Arguments:
        data_key    -- Data key (Location JSON digest)
        data_object -- Data object (Location JSON)
        month_idx   -- Month index
        Returns single-element row dictionary list
        """
        try:
            json_object = jd(data_object)
            j_elev = json_object["elevation"]
            ret: ROW_DATA_TYPE = \
                {ms(self._col_digest.name): data_key.urn,
                 ms(self._col_month_idx.name): month_idx,
                 ms(self._col_deployment_type.name):
                    ji(json_object.get("indoorDeployment", 0)),
                 ms(self._col_height.name): float(j_elev["height"]),
                 ms(self._col_height_type.name): str(j_elev["heightType"]),
                 ms(self._col_height_uncertainty.name):
                    float(j_elev["verticalUncertainty"])}
            loc_uncertainty: float
            if "ellipse" in json_object:
                ret[ms(self._col_loc_type.name)] = "ellipse"
                j_ellipse = jd(json_object["ellipse"])
                center = self._get_point(jd(j_ellipse["center"]))
                loc_uncertainty = float(j_ellipse["majorAxis"])
            elif "radialPolygon" in json_object:
                ret[ms(self._col_loc_type.name)] = "radialPolygon"
                j_r_poly = jd(json_object["radialPolygon"])
                loc_uncertainty = 0
                center = self._get_point(jd(j_r_poly["center"]))
                for j_v in j_r_poly["outerBoundary"]:
                    loc_uncertainty = max(loc_uncertainty,
                                          float(j_v["length"]))
            else:
                j_l_poly = jd(json_object["linearPolygon"])
                ret[ms(self._col_loc_type.name)] = "linearPolygon"
                center_lat: float = 0
                center_lon: float = 0
                lon0: Optional[float] = None
                for j_p in j_l_poly["outerBoundary"]:
                    p = self._get_point(jd(j_p))
                    center_lat += p.lat
                    if lon0 is None:
                        lon0 = p.lon
                    center_lon += self._same_hemisphere(p.lon, lon0)
                center_lat /= len(j_l_poly["outerBoundary"])
                center_lon /= len(j_l_poly["outerBoundary"])
                if center_lon <= -180:
                    center_lon += 360
                elif center_lon > 180:
                    center_lon -= 360
                center = self.Point(lat=center_lat, lon=center_lon)
                loc_uncertainty = 0
                for j_p in jl(j_l_poly["outerBoundary"]):
                    p = self._get_point(jd(j_p))
                    loc_uncertainty = max(loc_uncertainty,
                                          self._dist(center, p))
            ret[ms(self._col_loc_uncertainty.name)] = loc_uncertainty
            ret[ms(self._col_location.name)] = \
                f"POINT({center.lon} {center.lat})"
            return [ret]
        except (LookupError, TypeError, ValueError) as ex:
            raise JsonFormatError(f"Invalid Location format: '{ex}'",
                                  code_line=LineNumber.exc(),
                                  data=data_object)

    def _get_point(self, j_p: dict) -> "LocationTableUpdater.Point":
        """ Point object from JSON

        Arguments:
        j_p -- JSON Point object
        Returns Point object
        """
        return self.Point(lat=j_p["latitude"], lon=j_p["longitude"])

    def _same_hemisphere(self, lon: float, root_lon: float) -> float:
        """ Makes actually close longitudes numerically close

        Arguments:
        lon      -- Longitude in question
        root_lon -- Longitude in hemisphere in question
        Returns if longitudes are on the opposite sides of 180 - returns
        appropriately corrected first one (even if it will go beyond
        [-180, 180]. Otherwise - just returns first longitude
        """
        if lon < (root_lon - 180):
            lon += 360
        elif lon > (root_lon + 180):
            lon -= 360
        return lon

    def _dist(self, p1: "LocationTableUpdater.Point",
              p2: "LocationTableUpdater.Point") -> float:
        """ Approximate distance in meters beteen two geodetic points """
        lat_dist = (p1.lat - p2.lat) * self.DEGREE_M
        lon_dist = \
            (p1.lon - self._same_hemisphere(p2.lon, p1.lon)) * \
            math.cos((p1.lat + p2.lat) / 2 * math.pi / 180) * \
            self.DEGREE_M
        return math.sqrt(lat_dist * lat_dist + lon_dist * lon_dist)


class CompressedJsonTableUpdater(TableUpdaterBase[uuid.UUID, JSON_DATA_TYPE]):
    """ Compressed JSONs table

    Private attributes:
    _col_digest    -- Digest over uncompressed JSON column
    _col_month_idx -- Month index column
    _col_data      -- Compressed JSON column
    """
    # Table name
    TABLE_NAME = "compressed_json"

    def __init__(self, adb: AlsDatabase) -> None:
        """ Constructor

        Arguments:
        adb -- AlsDatabase object
        """
        super().__init__(adb=adb, table_name=self.TABLE_NAME,
                         json_obj_name="Request/Response")
        self._col_digest = self.get_column("compressed_json_digest",
                                           sa_pg.UUID)
        self._col_month_idx = self.get_month_idx_col()
        self._col_data = self.get_column("compressed_json_data",
                                         sa.LargeBinary)

    def _make_rows(self, data_key: uuid.UUID, data_object: JSON_DATA_TYPE,
                   month_idx: int) -> List[ROW_DATA_TYPE]:
        """ Makes row dictionary

        Arguments:
        data_key    -- Digest over JSON
        data_object -- JSON itself
        month_idx   -- Month index
        """
        return [{ms(self._col_digest.name): data_key.urn,
                 ms(self._col_month_idx.name): month_idx,
                 ms(self._col_data.name):
                 lz4.frame.compress(BytesUtils.json_to_bytes(data_object))}]


class MaxEirpTableUpdater(TableUpdaterBase[uuid.UUID, JSON_DATA_TYPE]):
    """ Updater for Max EIRP table.
    Data key is digest, used in request_response table (i.e. digest computed
    over AlsMessageBundle.RequestResponse.invariant_json).
    Data value is this object itself
    (AlsMessageBundle.RequestResponse.invariant_json)

    Private attributes:
    _col_digest    -- Digest over invariant_json column
    _col_month_idx -- Month index column
    _col_op_class  -- Channel operating class column
    _col_channel   -- Channel number column
    _col_eirp      -- Maximum EIRP in dBm column
    """
    # Table name
    TABLE_NAME = "max_eirp"

    def __init__(self, adb: AlsDatabase) -> None:
        """ Constructor

        Arguments:
        adb -- AlsDatabase object
        """
        super().__init__(adb=adb, table_name=self.TABLE_NAME,
                         json_obj_name="AvailableChannelInfo")
        self._col_digest = self.get_column("request_response_digest",
                                           sa_pg.UUID)
        self._col_month_idx = self.get_month_idx_col()
        self._col_op_class = self.get_column("channel", sa.SmallInteger)
        self._col_channel = self.get_column("channel", sa.SmallInteger)
        self._col_eirp = self.get_column("max_eirp_dbm", sa.Float)

    def _make_rows(self, data_key: uuid.UUID, data_object: JSON_DATA_TYPE,
                   month_idx: int) -> List[ROW_DATA_TYPE]:
        """ Prepares list of row dictionaries

        Arguments:
        data_key    -- Digest over 'invariant_json'
        data_object -- 'invariant_json' itself
        month_idx   -- Month index
        Returns list or row dictionaries
        """
        ret: List[ROW_DATA_TYPE] = []
        try:
            for av_chan_info in data_object:
                av_chan_info_j = jd(av_chan_info)
                op_class: int = av_chan_info_j["globalOperatingClass"]
                for channel, eirp in zip(av_chan_info_j["channelCfi"],
                                         av_chan_info_j["maxEirp"]):
                    ret.append({ms(self._col_digest.name): data_key.urn,
                                ms(self._col_month_idx.name): month_idx,
                                ms(self._col_op_class.name): op_class,
                                ms(self._col_channel.name): channel,
                                ms(self._col_eirp.name): eirp})
        except (LookupError, TypeError, ValueError) as ex:
            raise \
                JsonFormatError(f"Invalid AvailableChannelInfo format: '{ex}'",
                                code_line=LineNumber.exc(), data=data_object)
        return ret


class MaxPsdTableUpdater(TableUpdaterBase[uuid.UUID, JSON_DATA_TYPE]):
    """ Updater for Max PSD table.
    Data key is digest, used in request_response table (i.e. digest computed
    over AlsMessageBundle.RequestResponse.invariant_json).
    Data value is thsi object itsef
    (AlsMessageBundle.RequestResponse.invariant_json)

    Private attributes:
    _col_digest    -- Digest over invariant_json column
    _col_month_idx -- Month index column
    _col_low       -- Lower frequency range bound in MHz column
    _col_high      -- High frequency range bound in MHz column
    _col_psd       -- Maximum PSD in dBm/MHz column
    """
    TABLE_NAME = "max_psd"

    def __init__(self, adb: AlsDatabase) -> None:
        """ Constructor

        Arguments:
        adb -- AlsDatabase object
        """
        super().__init__(adb=adb, table_name=self.TABLE_NAME,
                         json_obj_name="AvailableFrequencyInfo")
        self._col_digest = self.get_column("request_response_digest",
                                           sa_pg.UUID)
        self._col_month_idx = self.get_month_idx_col()
        self._col_low = self.get_column("low_frequency_mhz", sa.SmallInteger)
        self._col_high = self.get_column("high_frequency_mhz", sa.SmallInteger)
        self._col_psd = self.get_column("max_psd_dbm_mhz", sa.Float)

    def _make_rows(self, data_key: uuid.UUID, data_object: JSON_DATA_TYPE,
                   month_idx: int) -> List[ROW_DATA_TYPE]:
        """ Prepares list of row dictionaries

        Arguments:
        data_key    -- Digest over 'invariant_json'
        data_object -- 'invariant_json' itself
        month_idx   -- Month index
        Returns list or row dictionaries
        """
        ret: List[ROW_DATA_TYPE] = []
        try:
            for av_freq_info in data_object:
                av_freq_info_j = jd(av_freq_info)
                freq_range = jd(av_freq_info_j["frequencyRange"])
                ret.append(
                    {ms(self._col_digest.name): data_key.urn,
                     ms(self._col_month_idx.name): month_idx,
                     ms(self._col_low.name): freq_range["lowFrequency"],
                     ms(self._col_high.name): freq_range["highFrequency"],
                     ms(self._col_psd.name): av_freq_info_j["maxPsd"]})
        except (LookupError, TypeError, ValueError) as ex:
            raise \
                JsonFormatError(
                    f"Invalid AvailableFrequencyInfo format: '{ex}'",
                    code_line=LineNumber.exc(), data=data_object)
        return ret


class RequestResponseTableUpdater(TableUpdaterBase[uuid.UUID, JSON_DATA_TYPE]):
    """ Updater for request/response table
    Data key is digest, used in request_response table (i.e. digest computed
    over AlsMessageBundle.RequestResponse.invariant_json).
    Data value is thsi object itsef
    (AlsMessageBundle.RequestResponse.invariant_json)

    Private attributes:
    _afc_config_lookup        -- AFC Config lookup
    _customer_lookup          -- Customer name lookup
    _uls_lookup               -- ULS ID lookup
    _geo_data_lookup          -- Geodetic data lookup
    _compressed_json_updater  -- Compressed JSON table updater
    _dev_desc_updater         -- Device Descriptor table updater
    _location_updater         -- Location table updater
    _max_eirp_updater         -- Maximum EIRP table updater
    _max_psd_updater          -- Maximum PSD table updater
    _col_digest               -- Digest over 'invariant_json' column
    _col_month_idx            -- Month index column
    _col_afc_config_digest    -- AFC Config digest column
    _col_customer_id          -- Customer ID column
    _col_uls_data_id          -- ULS Data ID column
    _col_geo_data_id          -- Geodetic data ID column
    _col_req_digest           -- Request digest column
    _col_resp_digest          -- Response digest column
    _col_dev_desc_digest      -- Device Descriptor digets column
    _col_loc_digest           -- Location digest column
    _col_response_code        -- Response code column
    _col_response_description -- Response description column
    _col_response_data        -- Response data column
    """
    TABLE_NAME = "request_response"

    def __init__(self, adb: AlsDatabase, afc_config_lookup: AfcConfigLookup,
                 customer_lookup: StringLookup, uls_lookup: StringLookup,
                 geo_data_lookup: StringLookup,
                 compressed_json_updater: CompressedJsonTableUpdater,
                 dev_desc_updater: DeviceDescriptorTableUpdater,
                 location_updater: LocationTableUpdater,
                 max_eirp_updater: MaxEirpTableUpdater,
                 max_psd_updater: MaxPsdTableUpdater) -> None:
        """ Constructor

        Arguments:
        adb                      -- AlsDatabase object
        afc_config_lookup        -- AFC Config lookup
        customer_lookup          -- Customer name lookup
        uls_lookup               -- ULS ID lookup
        geo_data_lookup          -- Geodetic data lookup
        compressed_json_updater  -- Compressed JSON table updater
        dev_desc_updater         -- Device Descriptor table updater
        location_updater         -- Location table updater
        max_eirp_updater         -- Maximum EIRP table updater
        max_psd_updater          -- Maximum PSD table updater
        """
        super().__init__(adb=adb, table_name=self.TABLE_NAME,
                         json_obj_name="RequestResponse")
        self._afc_config_lookup = afc_config_lookup
        self._customer_lookup = customer_lookup
        self._uls_lookup = uls_lookup
        self._geo_data_lookup = geo_data_lookup
        self._compressed_json_updater = compressed_json_updater
        self._dev_desc_updater = dev_desc_updater
        self._location_updater = location_updater
        self._max_eirp_updater = max_eirp_updater
        self._max_psd_updater = max_psd_updater
        self._col_digest = self.get_column("request_response_digest",
                                           sa_pg.UUID)
        self._col_month_idx = self.get_month_idx_col()
        self._col_afc_config_digest = self.get_column("afc_config_text_digest",
                                                      sa_pg.UUID)
        self._col_customer_id = self.get_column("customer_id", sa.Integer)
        self._col_uls_data_id = self.get_column("uls_data_version_id",
                                                sa.Integer)
        self._col_geo_data_id = self.get_column("geo_data_version_id",
                                                sa.Integer)
        self._col_req_digest = self.get_column("request_json_digest",
                                               sa_pg.UUID)
        self._col_resp_digest = self.get_column("response_json_digest",
                                                sa_pg.UUID)
        self._col_dev_desc_digest = self.get_column("device_descriptor_digest",
                                                    sa_pg.UUID)
        self._col_loc_digest = self.get_column("location_digest", sa_pg.UUID)
        self._col_response_code = self.get_column("response_code", sa.Integer)
        self._col_response_description = \
            self.get_column("response_description", sa.Text)
        self._col_response_data = self.get_column("response_data", sa.Text)

    def _update_lookups(self, data_objects: Iterable[JSON_DATA_TYPE],
                        month_idx: int) -> None:
        """ Update used lookups

        Arguments:
        data_objects -- Sequence of 'invariant_json' objects
        month_idx    -- Month index
        """
        configs: List[str] = []
        customers: List[str] = []
        ulss: List[str] = []
        geos: List[str] = []
        for json_obj in data_objects:
            json_object = jd(json_obj)
            configs.append(
                js(json_object[AlsMessageBundle.JRR_CONFIG_TEXT_KEY]))
            customers.append(
                js(json_object[AlsMessageBundle.JRR_CUSTOMER_KEY]))
            ulss.append(js(json_object[AlsMessageBundle.JRR_ULS_KEY]))
            geos.append(js(json_object[AlsMessageBundle.JRR_GEO_KEY]))
        self._afc_config_lookup.update_db(values=configs, month_idx=month_idx)
        self._customer_lookup.update_db(values=customers, month_idx=month_idx)
        self._uls_lookup.update_db(values=ulss, month_idx=month_idx)
        self._geo_data_lookup.update_db(values=geos, month_idx=month_idx)

    def _make_rows(self, data_key: uuid.UUID, data_object: JSON_DATA_TYPE,
                   month_idx: int) -> List[ROW_DATA_TYPE]:
        """ Makes database row dictionary

        Arguments:
        data_key    -- Digest of 'invariant_json' object
        data_object -- 'invariant_json' object itself
        month_idx   -- Month index
        Returns single-element row dictionary
        """
        try:
            json_dict = jd(data_object)
            resp_status: Dict[str, Any] = \
                jd(json_dict[AlsMessageBundle.JRR_RESPONSE_KEY]["response"])
            success = resp_status["responseCode"] == 0
            resp_data: Optional[str] = None
            if not success:
                resp_data = ""
                for field_name, field_value in \
                        (resp_status.get("supplementalInfo", {}) or
                         {}).items():
                    if field_value and \
                            (field_name in ("missingParams", "invalidParams",
                                            "unexpectedParams")):
                        resp_data += \
                            ("," if resp_data else "") + ",".join(field_value)
            return [{
                ms(self._col_digest.name): data_key.urn,
                ms(self._col_month_idx.name): month_idx,
                ms(self._col_afc_config_digest.name):
                    self._afc_config_lookup.key_for_value(
                        json_dict[AlsMessageBundle.JRR_CONFIG_TEXT_KEY],
                        month_idx=month_idx).urn,
                ms(self._col_customer_id.name):
                    self._customer_lookup.key_for_value(
                        json_dict[AlsMessageBundle.JRR_CUSTOMER_KEY],
                        month_idx=month_idx),
                ms(self._col_uls_data_id.name):
                    self._uls_lookup.key_for_value(
                        json_dict[AlsMessageBundle.JRR_ULS_KEY],
                        month_idx=month_idx),
                ms(self._col_geo_data_id.name):
                    self._geo_data_lookup.key_for_value(
                        json_dict[AlsMessageBundle.JRR_GEO_KEY],
                        month_idx=month_idx),
                ms(self._col_req_digest.name):
                    BytesUtils.json_to_uuid(
                        json_dict[AlsMessageBundle.JRR_REQUEST_KEY]).urn,
                ms(self._col_resp_digest.name):
                    BytesUtils.json_to_uuid(
                        json_dict[AlsMessageBundle.JRR_RESPONSE_KEY]).urn,
                ms(self._col_dev_desc_digest.name):
                    BytesUtils.json_to_uuid(
                        json_dict[AlsMessageBundle.JRR_REQUEST_KEY][
                            "deviceDescriptor"]).urn,
                ms(self._col_loc_digest.name):
                    BytesUtils.json_to_uuid(
                        json_dict[AlsMessageBundle.JRR_REQUEST_KEY][
                            "location"]).urn,
                ms(self._col_response_code.name):
                    json_dict[AlsMessageBundle.JRR_RESPONSE_KEY][
                        "response"]["responseCode"],
                ms(self._col_response_description.name):
                    resp_status.get("shortDescription"),
                ms(self._col_response_data.name): resp_data}]
        except (LookupError, TypeError, ValueError) as ex:
            raise JsonFormatError(
                f"Invalid Request or Response format: '{ex}'",
                code_line=LineNumber.exc(), data=data_object)

    def _update_foreign_targets(
                self,
                row_infos: Dict[uuid.UUID,
                                Tuple[JSON_DATA_TYPE, List[ROW_DATA_TYPE]]],
                month_idx: int) -> None:
        """ Updates tables this one references

        Arguments:
        row_infos -- Dictionary of data objects and row lists generated from
                     them, indexed by data keys
        month_idx -- Month index
        """
        updated_jsons: Dict[uuid.UUID, JSON_DATA_TYPE] = {}
        updated_dev_desc: Dict[uuid.UUID, JSON_DATA_TYPE] = {}
        updated_locations: Dict[uuid.UUID, JSON_DATA_TYPE] = {}
        for json_obj, rows in row_infos.values():
            json_object = jd(json_obj)
            row = rows[0]
            updated_jsons[
                uuid.UUID(js(row[ms(self._col_req_digest.name)]))] = \
                json_object[AlsMessageBundle.JRR_REQUEST_KEY]
            updated_jsons[
                uuid.UUID(js(row[ms(self._col_resp_digest.name)]))] = \
                json_object[AlsMessageBundle.JRR_RESPONSE_KEY]
            updated_dev_desc[uuid.UUID(
                js(row[ms(self._col_dev_desc_digest.name)]))] = \
                jd(json_object[AlsMessageBundle.JRR_REQUEST_KEY])[
                    "deviceDescriptor"]
            updated_locations[
                uuid.UUID(js(row[ms(self._col_loc_digest.name)]))] = \
                jd(jd(json_object[AlsMessageBundle.JRR_REQUEST_KEY])[
                    "location"])
        self._compressed_json_updater.update_db(data_dict=updated_jsons,
                                                month_idx=month_idx)
        self._dev_desc_updater.update_db(data_dict=updated_dev_desc,
                                         month_idx=month_idx)
        self._location_updater.update_db(data_dict=updated_locations,
                                         month_idx=month_idx)

    def _update_foreign_sources(
            self,
            inserted_rows: Dict[uuid.UUID, Tuple[ROW_DATA_TYPE,
                                                 JSON_DATA_TYPE,
                                                 RESULT_ROW_DATA_TYPE]],
            month_idx: int) -> None:
        """ Updates compressed JSONs, device descriptors, locations, EIRPs, PSD
        tables for those table rows that were inserted

        Arguments:
        inserted_rows    -- Ordered by digest rows/objects/inserted rows that
                            were inserted
        conflicting rows -- Ordered by digest rows/objects that were not
                            inserted (ignored)
        month_idx        -- Month index
        """
        updated_eirps: Dict[uuid.UUID, JSON_DATA_TYPE] = {}
        updated_psds: Dict[uuid.UUID, JSON_DATA_TYPE] = {}
        for digest, (_, json_obj, _) in inserted_rows.items():
            json_object = jd(json_obj)
            updated_eirps[digest] = \
                json_object[AlsMessageBundle.JRR_RESPONSE_KEY].get(
                    "availableChannelInfo") or []
            updated_psds[digest] = \
                json_object[AlsMessageBundle.JRR_RESPONSE_KEY].get(
                    "availableFrequencyInfo") or []
        self._max_eirp_updater.update_db(data_dict=updated_eirps,
                                         month_idx=month_idx)
        self._max_psd_updater.update_db(data_dict=updated_psds,
                                        month_idx=month_idx)


class EnvelopeTableUpdater(TableUpdaterBase[uuid.UUID, JSON_DATA_TYPE]):
    """ Request/response envelope tables.
    Keys are digests over envelope JSON
    Values are envelope JSONs themselves

    Private attributes
    _col_digest    -- Digest column
    _col_month_idx -- Month index column
    _col_data      -- Envelope JSON column
    """
    # Parameters
    Params = NamedTuple(
        "Params",
        # Table name
        [("table_name", str),
         # Name of digest column
         ("digest_col_name", str),
         # Name of envelope JSON column
         ("value_col_name", str)])
    # Parameters for AFC Request envelope table
    RX_ENVELOPE_PARAMS = Params(table_name="rx_envelope",
                                digest_col_name="rx_envelope_digest",
                                value_col_name="envelope_json")
    # Parameters for AFC Response envelope table
    TX_ENVELOPE_PARAMS = Params(table_name="tx_envelope",
                                digest_col_name="tx_envelope_digest",
                                value_col_name="envelope_json")

    def __init__(self, adb: AlsDatabase,
                 params: "EnvelopeTableUpdater.Params") -> None:
        """ Constructor

        Arguments:
        adb    -- AlsDatabase object
        params -- Table parameters
        """
        super().__init__(adb=adb, table_name=params.table_name,
                         json_obj_name="RequestResponseMessageEnvelope")
        self._col_digest = self.get_column(params.digest_col_name,
                                           sa_pg.UUID)
        self._col_month_idx = self.get_month_idx_col()
        self._col_data = self.get_column(params.value_col_name, sa_pg.JSON)

    def _make_rows(self, data_key: uuid.UUID, data_object: JSON_DATA_TYPE,
                   month_idx: int) -> List[ROW_DATA_TYPE]:
        """ Makes row dictionary

        Arguments:
        data_key    -- Digest of envelope JSON
        data_object -- Envelope JSON
        month_idx   -- Month index
        """
        return \
            [{ms(self._col_digest.name): data_key.urn,
             ms(self._col_month_idx.name): month_idx,
             ms(self._col_data.name): data_object}]


# Type for key of request/response association key data
RequestResponseAssociationTableDataKey = \
    NamedTuple(
        "RequestResponseAssociationTableDataKey",
        # Serial ID of AFC message
        [("message_id", int),
         # Id of request/response within message
         ("request_id", str)])


class RequestResponseAssociationTableUpdater(
        TableUpdaterBase[RequestResponseAssociationTableDataKey,
                         AlsMessageBundle.RequestResponse]):
    """ Updater of request/response association table (intermediary between
    message table and request/response table)

    Private attributes:
    _col_message_id           -- Message serial ID column
    _col_req_id               -- Request ID column
    _col_month_idx            -- Month index column
    _col_rr_digest            -- Request/response (invariant_json) digest
                                 column
    _col_expire_time          -- Response expiration time column
    _request_response_updater -- Request/response table updater
    """
    TABLE_NAME = "request_response_in_message"

    def __init__(
            self, adb: AlsDatabase,
            request_response_updater: RequestResponseTableUpdater) -> None:
        """ Constructor

        Arguments:
        adb                      -- AlsDatabase object
        request_response_updater -- Request/response table updater
        """
        super().__init__(
            adb=adb, table_name=self.TABLE_NAME,
            json_obj_name="RequestResponseAssociation")
        self._col_message_id = self.get_column("message_id", sa.BigInteger)
        self._col_req_id = self.get_column("request_id", sa.Text)
        self._col_month_idx = self.get_month_idx_col()
        self._col_rr_digest = self.get_column("request_response_digest",
                                              sa_pg.UUID)
        self._col_expire_time = self.get_column("expire_time",
                                                sa.DateTime)
        self._request_response_updater = request_response_updater

    def _make_rows(self,
                   data_key: RequestResponseAssociationTableDataKey,
                   data_object: AlsMessageBundle.RequestResponse,
                   month_idx: int) -> List[ROW_DATA_TYPE]:
        """ Make row dictionary

        Argument:
        data_key    -- Row in message table and request index
        data_object -- AlsMessageBundle.RequestResponse object
        month_idx   -- Month index
        Returns single-element row dictionary list
        """
        return [{ms(self._col_message_id.name): data_key.message_id,
                 ms(self._col_req_id.name): data_key.request_id,
                 ms(self._col_month_idx.name): month_idx,
                 ms(self._col_rr_digest.name):
                 BytesUtils.json_to_uuid(data_object.invariant_json).urn,
                 ms(self._col_expire_time.name): data_object.expire_time}]

    def _update_foreign_targets(
            self,
            row_infos: Dict[RequestResponseAssociationTableDataKey,
                            Tuple[AlsMessageBundle.RequestResponse,
                                  List[ROW_DATA_TYPE]]],
            month_idx: int) -> None:
        """ Updates tables pointed to by foreign keys of this table.

        Arguments:
        row_infos -- Dictionary of data objects and row lists generated from
                     them, indexed by data keys
        month_idx -- Value for 'month_idx' column
        """
        self._request_response_updater.update_db(
            data_dict={
                uuid.UUID(js(rows[0][ms(self._col_rr_digest.name)])):
                rr.invariant_json
                for rr, rows in row_infos.values()},
            month_idx=month_idx)


class DecodeErrorTableWriter(AlsTableBase):
    """ Writer of decode error table

    Private attributes:
    _col_msg       -- Error message column
    _col_line      -- Code line number column
    _col_data      -- Supplementary data column
    _col_time      -- Timetag column
    _col_month_idx -- Month index column
    """
    TABLE_NAME = "decode_error"

    def __init__(self, adb: AlsDatabase) -> None:
        """ Constructor

        Arguments:
        adb -- AlsDatabase object
        """
        super().__init__(adb=adb, table_name=self.TABLE_NAME)
        self._col_id = self.get_column("id", sa.BigInteger)
        self._col_time = self.get_column("time", sa.DateTime)
        self._col_msg = self.get_column("msg", sa.Text)
        self._col_line = self.get_column("code_line", sa.Integer)
        self._col_data = self.get_column("data", sa.Text)
        self._col_month_idx = self.get_month_idx_col()
        self._conn = self._adb.engine.connect()

    def write_decode_error(
            self, msg: str, line: Optional[int],
            data: Optional[Union[bytes, str, JSON_DATA_TYPE]] = None) -> None:
        """ Writes decode error to table

        Arguments:
        msg  -- Error message
        line -- Script line number
        data -- Supplementary data
        """
        if isinstance(data, bytes):
            data = data.decode("latin-1")
        elif isinstance(data, (list, dict)):
            data = json.dumps(data)
        ins = sa.insert(self._table).values(
            {ms(self._col_month_idx.name): get_month_idx(),
             ms(self._col_msg.name): msg,
             ms(self._col_line.name): line,
             ms(self._col_data.name): data,
             ms(self._col_time.name):
             datetime.datetime.now(dateutil.tz.tzlocal())})
        self._conn.execute(ins)


class AfcMessageTableUpdater(TableUpdaterBase[int, AlsMessageBundle]):
    """ AFC Message table
    Keys are, for no better alternatives, 0-based indices of messages, passed
    to update_db()
    Data objects are AlsMessageBundle objects

    Private attributes:
    _col_message_id       -- Message serial ID column
    _col_month_idx        -- Month index column
    _col_afc_server       -- AFC Server ID column
    _col_rx_time          -- AFC Request timetag column
    _col_tx_time          -- AFC Response timetag column
    _rx_envelope_digest   -- AFC Request envelope digest column
    _tx_envelope_digest   -- AFC Response envelope digest column
    _afc_server_lookup    -- Lookup fr AFC Server names
    _rr_assoc_updater     -- Updater of message to request/response association
                             table
    _rx_envelope_updater  -- Updater for AFC Request envelope table
    _tx_envelope_updater  -- Updater for AFC Response envelope table
    _decode_error_writer  -- Decode error table writer
    """
    # Table name
    TABLE_NAME = "afc_message"

    def __init__(self, adb: AlsDatabase, afc_server_lookup: StringLookup,
                 rr_assoc_updater: RequestResponseAssociationTableUpdater,
                 rx_envelope_updater: EnvelopeTableUpdater,
                 tx_envelope_updater: EnvelopeTableUpdater,
                 decode_error_writer: DecodeErrorTableWriter) -> None:
        """ Constructor

        Arguments:
        adb                  -- AlsDatabase object
        rr_assoc_updater     -- Updater of message to request/response
                                association table
        rx_envelope_updater  -- Updater for AFC Request envelope table
        tx_envelope_updater  -- Updater for AFC Response envelope table
        decode_error_writer  -- Decode error table writer
        """
        super().__init__(adb=adb, table_name=self.TABLE_NAME,
                         json_obj_name="AlsMessageBundle",
                         data_key_column_names=["message_id"])
        self._col_message_id = self.get_column("message_id", sa.BigInteger)
        self._col_month_idx = self.get_month_idx_col()
        self._col_afc_server = self.get_column("afc_server", sa.Integer)
        self._col_rx_time = self.get_column("rx_time", sa.DateTime)
        self._col_tx_time = self.get_column("tx_time", sa.DateTime)
        self._rx_envelope_digest = self.get_column("rx_envelope_digest",
                                                   sa_pg.UUID)
        self._tx_envelope_digest = self.get_column("tx_envelope_digest",
                                                   sa_pg.UUID)

        self._afc_server_lookup = afc_server_lookup
        self._rr_assoc_updater = rr_assoc_updater
        self._rx_envelope_updater = rx_envelope_updater
        self._tx_envelope_updater = tx_envelope_updater
        self._decode_error_writer = decode_error_writer

    def _update_lookups(self, data_objects: Iterable[AlsMessageBundle],
                        month_idx: int) -> None:
        """ Update used lookups

        Arguments:
        data_objects -- Sequence of AlsMessageBundle being added to database
        month_idx    -- Month index
        """
        self._afc_server_lookup.update_db(
            values=[b.take_apart().afc_server for b in data_objects],
            month_idx=month_idx)

    def _make_rows(self, data_key: int, data_object: AlsMessageBundle,
                   month_idx: int) -> List[ROW_DATA_TYPE]:
        """ Makes table row dictionary from data object

        Arguments:
        data_key    -- Data key (sequential index in dictionary, passed to
                       update_db())
        data_object -- AlsMessageBundle to make row from
        month_idx   -- Month index
        Single-element list of row dictionaries
        """
        parts = data_object.take_apart()
        for orphans, name in \
                [(parts.orphan_requests, "Requests without responses"),
                 (parts.orphan_responses, "Responses without requests")]:
            for orphan in orphans:
                self._decode_error_writer.write_decode_error(
                    msg=f"{name} received from {parts.afc_server}",
                    line=LineNumber.current(),
                    data=orphan)
        return [{ms(self._col_month_idx.name): month_idx,
                 ms(self._col_afc_server.name):
                 self._afc_server_lookup.key_for_value(parts.afc_server,
                                                       month_idx),
                 ms(self._col_rx_time.name): parts.rx_timetag,
                 ms(self._col_tx_time.name): parts.tx_timetag,
                 ms(self._rx_envelope_digest.name):
                 BytesUtils.json_to_uuid(parts.rx_envelope).urn,
                 ms(self._tx_envelope_digest.name):
                 BytesUtils.json_to_uuid(parts.tx_envelope).urn}]

    def _update_foreign_targets(
            self,
            row_infos: Dict[int, Tuple[AlsMessageBundle, List[ROW_DATA_TYPE]]],
            month_idx: int) -> None:
        """ Update RX/TX envelopes that are not yet in database

        Arguments:
        row_infos -- Dictionary of data objects and row lists generated from
                     them, indexed by data keys
        month_idx -- Month index
        """
        self._rx_envelope_updater.update_db(
            data_dict={uuid.UUID(js(rows[0]
                                    [ms(self._rx_envelope_digest.name)])):
                       bundle.take_apart().rx_envelope
                       for bundle, rows in row_infos.values()},
            month_idx=month_idx)
        self._tx_envelope_updater.update_db(
            data_dict={uuid.UUID(js(rows[0]
                                    [ms(self._tx_envelope_digest.name)])):
                       bundle.take_apart().tx_envelope
                       for bundle, rows in row_infos.values()},
            month_idx=month_idx)

    def _update_foreign_sources(
            self,
            inserted_rows: Dict[int, Tuple[ROW_DATA_TYPE,
                                           AlsMessageBundle,
                                           RESULT_ROW_DATA_TYPE]],
            month_idx: int) -> None:
        """ Updates request/response association tables and (for unique
        requests/responses) dependent tables

        inserted_rows    -- Information about inserted rows - row dictionaries,
                            data objects, result rows. Ordered by 0-based
                            indices of inserted rows
        month_idx        -- Month index
        """
        rr_dict: Dict[RequestResponseAssociationTableDataKey,
                      AlsMessageBundle.RequestResponse] = {}
        for _, message_bundle, inserted_row in inserted_rows.values():
            parts = message_bundle.take_apart()
            for req_id, request_response in parts.request_responses.items():
                rr_dict[
                    RequestResponseAssociationTableDataKey(
                        message_id=ji(inserted_row[0]), request_id=req_id)] = \
                        request_response
        self._rr_assoc_updater.update_db(data_dict=rr_dict,
                                         month_idx=month_idx)

    def _data_key_from_result_row(self, result_row: Tuple[Any, ...],
                                  result_row_idx: int) -> int:
        """ Data key from rows written to database

        Arguments:
        result_row     -- Insert result tuple
        result_row_idx -- 0-based index i insert results
        Returns the latter
        """
        return result_row_idx


class IncompleteAlsBundles:
    """ Collection of ALS bundles, for which not all messages arrived yet

    Private attributes:
    _kafka_positions -- Collection of uncommitted Kafka positions
    _bundle_queue    -- Heap queue of ALS bundles, arranged by last update
    _bundle_map      -- Maps Kafka message keys to bundles
    """
    def __init__(self, kafka_positions: KafkaPositions) -> None:
        """ Constructor

        Arguments:
        kafka_positions -- Collection of uncommitted Kafka positions
        """
        self._kafka_positions = kafka_positions
        self._bundle_queue: List[AlsMessageBundle] = []
        self._bundle_map: Dict[AlsMessageKeyType, AlsMessageBundle] = {}

    def add_message(self, message_key: AlsMessageKeyType, message: AlsMessage,
                    kafka_position: KafkaPosition) -> None:
        """ Adds ALS message

        Arguments:
        message_key    -- Kafka message key
        message        -- AlsMessage
        kafka_position -- Message's position in Kafka queue
        """
        bundle = self._bundle_map.get(message_key)
        if bundle is None:
            bundle = AlsMessageBundle(message_key=message_key,
                                      kafka_positions=self._kafka_positions)
            heapq.heappush(self._bundle_queue, bundle)
            self._bundle_map[message_key] = bundle
        bundle.update(message, kafka_position)
        heapq.heapify(self._bundle_queue)

    def get_oldest_bundle(self) -> Optional[AlsMessageBundle]:
        """ Get the oldest bundle (None if collection is empty) """
        return self._bundle_queue[0] if self._bundle_queue else None

    def get_incomplete_count(self) -> int:
        """ Number of incomplete (not yet assembled) bundles """
        return sum((0 if b.assembled() else 1) for b in self._bundle_queue)

    def remove_oldest_bundle(self) -> None:
        """ Removes oldest bundle from collection """
        assert self._bundle_queue
        ret = heapq.heappop(self._bundle_queue)
        del self._bundle_map[ret.message_key()]

    def fetch_assembled(
            self, max_bundles: Optional[int] = None,
            max_requests: Optional[int] = None) -> List[AlsMessageBundle]:
        """ Fetch and remove from collection assembled bundles (all or some)

        Arguments:
        max_bundles  -- Maximum number of bundles or None
        max_requests -- Maximum total number of requests or None
        Returns list of bundles
        """
        ret: List[AlsMessageBundle] = []
        idx = 0
        num_requests = 0
        while (idx < len(self._bundle_queue)) and \
                ((max_bundles is None) or (len(ret) < max_bundles)):
            bundle = self._bundle_queue[idx]
            if not bundle.assembled():
                idx += 1
                continue
            if (max_requests is not None) and \
                    ((num_requests + bundle.request_count()) > max_requests):
                break
            ret.append(bundle)
            del self._bundle_map[bundle.message_key()]
            self._bundle_queue.pop(idx)
            heapq.heapify(self._bundle_queue)
        return ret


class KafkaConsumerParams:
    """ Kafka consumer constructor parameters.
    Subset of the whole set of consumer parameters. Values set to None are not
    passed to constructor - defaults are used

    Attributes:
    bootstrap_servers   -- List of 'server[:port]' Kafka server addresses
    client_id           -- Client ID to use in Kafka logs
    security_procol     -- "SSL" or 'PLAINTEXT'
    ssl_keyfile         -- Client private key file
    ssl_check_hostname  -- True to verify server identity. Default (None) is
                           True
    ssl_cafile          -- CA file for certificate verification. Default
                           (None) is None
    ssl_certfile        -- Client certificate PEM file name. Default (None) is
                           None
    ssl_crlfile         -- File name for CRL certificate expiration check.
                           Default (None) is None
    ssl_ciphers         -- Available ciphers in OpenSSL cipher list format.
                           Default (None) is None
    enable_auto_commit  -- True if auto commit enabled
    group_id            -- Consumer group ID

    servers             -- Value of --kafka_servers. Converted to
                           'bootstrap_servers' in constructor
    """
    def __init__(self, **kwargs) -> None:
        self.client_id: Optional[str] = None
        self.security_protocol: Optional[str] = None
        self.ssl_keyfile: Optional[str] = None
        self.ssl_cafile: Optional[str] = None
        self.ssl_certfile: Optional[str] = None
        self.ssl_crlfile: Optional[str] = None
        self.ssl_ciphers: Optional[str] = None
        self.ssl_check_hostname: Optional[bool] = None
        self.servers: Optional[str] = None
        for k, v in kwargs.items():
            assert k in self.__dict__
            setattr(self, k, v)
        self.enable_auto_commit = False
        self.group_id = "ALS"
        self.auto_offset_reset = "earliest"

        self.bootstrap_servers: List[str] = \
            self.servers.split(",") if self.servers else [DEFAULT_KAFKA_SERVER]
        self.servers = None

        self.client_id = self.client_id or DEFAULT_KAFKA_CLIENT_ID
        if self.client_id.endswith("@"):
            self.client_id = self.client_id[:-1] + \
                "".join(f"{b:02X}" for b in os.urandom(10))

    def constructor_kwargs(self) -> Dict[str, Any]:
        """ Kwargs to pass to constructor. Only keys with values are used """
        return \
            {k: v for k, v in self.__dict__.items() if v not in ("", None)}


class KafkaClient:
    """ Wrapper over KafkaConsumer

    Private attributes:
    _consumer                -- KafkaConsumer object
    _topic_regex_str         -- Regex string on what topics to subscribe
    _subscribed_topics       -- Set of currently subscribed topics
    _resubscribe_interval    -- Minimum time interval before subscription
                                checks
    _last_subscription_check -- Moment when subscription was last time checked
    """
    # Kafka message data
    MessageInfo = \
        NamedTuple(
            "MessageInfo",
            # Message position (topic/partition/offset)
            [("position", KafkaPosition),
             # Message raw key
             ("key", Optional[bytes]),
             # Message raw value
             ("value", bytes)])

    def __init__(self, consumer_params: KafkaConsumerParams,
                 topic_regex_str: str, resubscribe_interval_s: int) -> None:
        """ Constructor

        Arguments:
        consumer_params        -- KafkaConsumerParams object - Kafka server
                                  consumer connection parameters
        topic_regex_str        -- Regex string on what topics to subscribe
        resubscribe_interval_s -- How often (interval in seconds)
                                  subscription_check() will actually check
                                  subscription. 0 means - on each call.
        """
        self._consumer = \
            kafka.KafkaConsumer(**consumer_params.constructor_kwargs())
        self._topic_regex_str = topic_regex_str
        self._resubscribe_interval = \
            datetime.timedelta(seconds=resubscribe_interval_s)
        self._subscribed_topics: Set[str] = set()
        self._last_subscription_check = \
            datetime.datetime.now() - self._resubscribe_interval

    def connected(self) -> bool:
        """ True if Kafka server connected """
        return self._consumer.bootstrap_connected()

    def subscription_check(self) -> None:
        """ If it's time - check if new matching topics arrived and resubscribe
        if so """
        if (datetime.datetime.now() - self._last_subscription_check) < \
                self._resubscribe_interval:
            return
        topic_regex = re.compile(self._topic_regex_str)
        current_topics = \
            {t for t in self._consumer.topics() if topic_regex.match(t)}
        if current_topics <= self._subscribed_topics:
            return
        self._subscribed_topics = current_topics
        self._consumer.subscribe(pattern=self._topic_regex_str)
        self._last_subscription_check = datetime.datetime.now()

    def poll(self, timeout_ms: int, max_records: int,
             update_offsets: bool = True) \
            -> Dict[str, List["KafkaClient.MessageInfo"]]:
        """ Poll for new messages

        Arguments:
        timeout_ms     -- Poll timeout in milliseconds. 0 to return immediately
        max_records    -- Maximum number of records to poll
        update_offsets -- Mark message are read
        Returns by-topic dictionary of MessageInfo objects
        """
        result = \
            self._consumer.poll(timeout_ms=timeout_ms, max_records=max_records,
                                update_offsets=update_offsets)
        ret: Dict[str, List["KafkaClient.MessageInfo"]] = {}
        for topic_partition, messages in result.items():
            ret[topic_partition.topic] = \
                [self.MessageInfo(
                    position=KafkaPosition(
                        topic=topic_partition.topic,
                        partition=topic_partition.partition, offset=m.offset),
                    key=m.key, value=m.value) for m in messages]
        return ret

    def commit(self, positions: Dict[str, Dict[int, int]]) -> None:
        """ Commit given message positions

        Arguments:
        positions -- By-topic then by-partition maximum commited offsets
        """
        offsets: Dict[kafka.TopicPartition, kafka.OffsetAndMetadata] = {}
        for topic, partitions in positions.items():
            for partition, offset in partitions.items():
                offsets[kafka.TopicPartition(topic, partition)] = \
                    kafka.OffsetAndMetadata(offset, None)
        self._consumer.commit(offsets)


class Siphon:
    """ Siphon (Kafka reader / DB updater

    Private attributes:
    _adb                     -- AlsDatabase object or None
    _ldb                     -- LogsDatabase object or None
    _kafka_client            -- KafkaClient consumer wrapper object
    _decode_error_writer     -- Decode error table writer
    _lookups                 -- Lookup collection
    _cert_lookup             -- Certificates' lookup
    _afc_config_lookup       -- AFC Configs lookup
    _afc_server_lookup       -- AFC Server name lookup
    _customer_lookup         -- Customer name lookup
    _uls_lookup              -- ULS ID lookup
    _geo_data_lookup         -- Geodetic Data ID lookup
    _dev_desc_updater        -- DeviceDescriptor table updater
    _location_updater        -- Location table updater
    _compressed_json_updater -- Compressed JSON table updater
    _max_eirp_updater        -- Maximum EIRP table updater
    _max_psd_updater         -- Maximum PSD table updater
    _req_resp_updater        -- Request/Response table updater
    _rx_envelope_updater     -- AFC Request envelope table updater
    _tx_envelope_updater     -- AFC Response envelope tabgle updater
    _req_resp_assoc_updater  -- Request/Response to Message association table
                                updater
    _afc_message_updater     -- Message table updater
    _kafka_positions         -- Nonprocessed Kafka positions' collection
    _als_bundles             -- Incomplete ALS Bundler collection
    _report_progress         -- True to report progress periodically
    _progress_info           -- ProgressInfo object with cumulative statistics
    _prev_progress_info      -- None or previously reported ProgressInfo
    """
    class ProgressInfo:
        """ Progress information

        Public attributes:
        kafka_received      -- Number of Kafka messages received
        kafka_polls         -- Number of Kafka polls
        als_received        -- Number of ALS messages (all types) received
        log_processed       -- Number of JSON logs received and processed
        als_written         -- Number of ALS bundles written to DB
        als_retired         -- Number of ALS bundles retired (aged incomplete)
        als_format_failures -- Number of ALS JSON format failures
        log_format_failures -- Number of JSON Log format failures
        timetag             -- Time of creation or last report generation
        """
        def __init__(self) -> None:
            """ Constructor """
            self.kafka_received: int = 0
            self.kafka_polls: int = 0
            self.als_received: int = 0
            self.log_processed: int = 0
            self.als_written: int = 0
            self.als_retired: int = 0
            self.als_format_failures: int = 0
            self.log_format_failures: int = 0
            self.timetag: datetime.datetime = datetime.datetime.now()

        def report(self, prev: Optional["Siphon.ProgressInfo"],
                   als_bundles: IncompleteAlsBundles) -> str:
            """ Progress report

            Arguments:
            prev        -- ProgressInfo used at previous report generation or
                           None
            als_bundles -- IncompleteAlsBundles object
            Returns progress report string
            """
            self.timetag = datetime.datetime.now()
            age_sec = 0 if prev is None \
                else (self.timetag - prev.timetag).total_seconds()
            ret = \
                f"{datetime.datetime.now()}. " \
                f"Kafka {self.kafka_received} msg in {self.kafka_polls} polls"
            if prev:
                ret += \
                    f" ({self.rate(prev, 'kafka_received', age_sec)} msg/s, " \
                    f"{self.rate(prev, 'kafka_polls', age_sec)} poll/s)"
            ret += \
                f"\nALS {self.als_received} msg received, " \
                f"{self.als_written} DB writes, {self.als_retired} retires"
            if prev:
                ret += \
                    f" ({self.rate(prev, 'als_received', age_sec)} msg/s, " \
                    f"{self.rate(prev, 'als_written', age_sec)} DB/s, " \
                    f"{self.rate(prev, 'als_retired', age_sec)} ret/s)"
            ret += \
                f"\nALS incomplete {als_bundles.get_incomplete_count()}, " \
                f"{self.als_retired} retired"
            if prev:
                ret += f" ({self.rate(prev, 'als_retired', age_sec)} retire/s"
            ret += f"\nLogs {self.log_processed}"
            if prev:
                ret += \
                    f" ({self.rate(prev, 'log_processed', age_sec)} log/s)"
            ret += f"\nErr {self.als_format_failures} ALS, " \
                f"{self.log_format_failures} Log"
            if prev:
                ret += \
                    f" ({self.rate(prev, 'als_format_failures', age_sec)} " \
                    f"ALS err/s, " \
                    f"{self.rate(prev, 'log_format_failures', age_sec)} " \
                    f"log err/s)"
            return ret

        def rate(self, prev: "Siphon.ProgressInfo", attr: str,
                 duration_sec: float) -> float:
            """ Per-second difference in value of given attribute

            Arguments:
            prev         -- ProgressReport object with previous values
            attr         -- Atribute of interest
            duration_sec -- Time elapsed in seconds
            Returns per-second change
            """
            return (getattr(self, attr) - getattr(prev, attr)) / duration_sec

    # Number of messages fetched from Kafka in single access
    KAFKA_MAX_RECORDS = 1000
    # Kafka server access timeout if system is idle
    KAFKA_IDLE_TIMEOUT_MS = 1000
    # Maximum age (time since last update) of ALS Bundle
    ALS_MAX_AGE_SEC = 1000
    # Maximum number of message bundles to write to database
    ALS_MAX_MESSAGE_UPDATE = 1000
    # Progress Report Interval in seconds
    PROGRESS_REPORT_INTERVAL_SEC = 5

    def __init__(self, adb: Optional[AlsDatabase], ldb: Optional[LogsDatabase],
                 kafka_client: KafkaClient, report_progress: bool) -> None:
        """ Constructor

        Arguments:
        adb             -- AlsDatabase object or None
        ldb             -- LogsDatabase object or None
        kafka_client    -- KafkaClient
        report_progress -- True to report progress periodically
        """
        error_if(not (adb or ldb),
                 "Neither ALS nor Logs database specified. Nothing to do")
        self._adb = adb
        self._ldb = ldb
        self._kafka_client = kafka_client
        if self._adb:
            self._decode_error_writer = DecodeErrorTableWriter(adb=self._adb)
            self._lookups = Lookups()
            self._cert_lookup = CertificationsLookup(adb=self._adb,
                                                     lookups=self._lookups)
            self._afc_config_lookup = AfcConfigLookup(adb=self._adb,
                                                      lookups=self._lookups)
            self._afc_server_lookup = \
                StringLookup(adb=self._adb,
                             params=StringLookup.AFC_SERVER_PARAMS,
                             lookups=self._lookups)
            self._customer_lookup = \
                StringLookup(adb=self._adb,
                             params=StringLookup.CUSTOMER_PARAMS,
                             lookups=self._lookups)
            self._uls_lookup = \
                StringLookup(adb=self._adb,
                             params=StringLookup.ULS_PARAMS_PARAMS,
                             lookups=self._lookups)
            self._geo_data_lookup = \
                StringLookup(adb=self._adb,
                             params=StringLookup.GEO_DATA_PARAMS,
                             lookups=self._lookups)
            self._dev_desc_updater = \
                DeviceDescriptorTableUpdater(
                    adb=self._adb, cert_lookup=self._cert_lookup)
            self._location_updater = LocationTableUpdater(adb=self._adb)
            self._compressed_json_updater = \
                CompressedJsonTableUpdater(adb=self._adb)
            self._max_eirp_updater = MaxEirpTableUpdater(adb=self._adb)
            self._max_psd_updater = MaxPsdTableUpdater(adb=self._adb)
            self._req_resp_updater = \
                RequestResponseTableUpdater(
                    adb=self._adb, afc_config_lookup=self._afc_config_lookup,
                    customer_lookup=self._customer_lookup,
                    uls_lookup=self._uls_lookup,
                    geo_data_lookup=self._geo_data_lookup,
                    compressed_json_updater=self._compressed_json_updater,
                    dev_desc_updater=self._dev_desc_updater,
                    location_updater=self._location_updater,
                    max_eirp_updater=self._max_eirp_updater,
                    max_psd_updater=self._max_psd_updater)
            self._rx_envelope_updater = \
                EnvelopeTableUpdater(
                    adb=self._adb,
                    params=EnvelopeTableUpdater.RX_ENVELOPE_PARAMS)
            self._tx_envelope_updater = \
                EnvelopeTableUpdater(
                    adb=self._adb,
                    params=EnvelopeTableUpdater.TX_ENVELOPE_PARAMS)
            self._req_resp_assoc_updater = \
                RequestResponseAssociationTableUpdater(
                    adb=self._adb,
                    request_response_updater=self._req_resp_updater)
            self._afc_message_updater = \
                AfcMessageTableUpdater(
                    adb=self._adb, afc_server_lookup=self._afc_server_lookup,
                    rr_assoc_updater=self._req_resp_assoc_updater,
                    rx_envelope_updater=self._rx_envelope_updater,
                    tx_envelope_updater=self._tx_envelope_updater,
                    decode_error_writer=self._decode_error_writer)
        self._kafka_positions = KafkaPositions()
        self._als_bundles = \
            IncompleteAlsBundles(kafka_positions=self._kafka_positions)
        self._report_progress = report_progress
        self._progress_info = self.ProgressInfo()
        self._prev_progress_info: Optional["Siphon.ProgressInfo"] = None

    def main_loop(self) -> None:
        """ Read/write loop """
        busy = True
        while True:
            self._print_progress_report()
            kafka_messages_by_topic = \
                self._kafka_client.poll(
                    timeout_ms=0 if busy else self.KAFKA_IDLE_TIMEOUT_MS,
                    max_records=self.KAFKA_MAX_RECORDS)
            self._progress_info.kafka_received += \
                sum(len(msgs) for msgs in kafka_messages_by_topic.values())
            self._progress_info.kafka_polls += 1

            busy = bool(kafka_messages_by_topic)
            for topic, kafka_messages in kafka_messages_by_topic.items():
                if topic == ALS_KAFKA_TOPIC:
                    self._progress_info.als_received += len(kafka_messages)
                    self._read_als_kafka_messages(kafka_messages)
                else:
                    self._process_log_kafka_messages(topic, kafka_messages)
                    self._progress_info.log_processed += len(kafka_messages)
            if self._adb:
                busy |= self._write_als_messages()
                busy |= self._timeout_als_messages()
                busy |= self._commit_kafka_offsets()
            self._kafka_client.subscription_check()

    def _read_als_kafka_messages(
            self, kafka_messages: List[KafkaClient.MessageInfo]) -> None:
        """ Put fetched ALS Kafka messages to store of incomplete bundles

        Arguments:
        topic          -- ALS Topic name
        kafka_messages -- List of raw Kafka messages
        """
        for kafka_message in kafka_messages:
            self._kafka_positions.add(kafka_message.position)
            try:
                assert kafka_message.key is not None
                self._als_bundles.add_message(
                    message_key=kafka_message.key,
                    message=AlsMessage(raw_msg=kafka_message.value),
                    kafka_position=kafka_message.position)
            except (AlsProtocolError, JsonFormatError) as ex:
                self._progress_info.als_format_failures += 1
                self._decode_error_writer.write_decode_error(
                    msg=ex.msg, line=ex.code_line, data=ex.data)
                self._kafka_positions.mark_processed(
                    kafka_position=kafka_message.position)

    def _process_log_kafka_messages(
            self, topic: str, kafka_messages: List[KafkaClient.MessageInfo]) \
            -> None:
        """ Process non-ALS (i.e. JSON Log) messages for one topic

        Arguments:
        topic          -- ALS Topic name
        kafka_messages -- List of Kafka messages
        """
        records: List[LogsDatabase.Record] = []
        for kafka_message in kafka_messages:
            self._kafka_positions.add(kafka_message.position)
            try:
                log_message = json.loads(kafka_message.value)
                records.append(
                    LogsDatabase.Record(
                        source=log_message["source"],
                        time=datetime.datetime.fromisoformat(
                            log_message["time"]),
                        log=json.loads(log_message["jsonData"])))
            except (json.JSONDecodeError, LookupError, TypeError, ValueError) \
                    as ex:
                self._progress_info.log_format_failures += 1
                logging.error(
                    f"Can't decode log message '{kafka_message.value}': {ex}")
        if records:
            transaction: Optional[Any] = None
            try:
                transaction = self._ldb.conn.begin()
                self._ldb.write_log(topic=topic, records=records)
                transaction.commit()
                transaction = None
            finally:
                if transaction is not None:
                    transaction.rollback()
        self._kafka_positions.mark_processed(topic=topic)

    def _write_als_messages(self) -> bool:
        """ Write complete ALS Bundles to ALS database.
        Returns True if any work was done """
        assert self._adb is not None
        month_idx = get_month_idx()
        transaction: Optional[Any] = None
        try:
            data_dict = \
                dict(
                    enumerate(self._als_bundles.fetch_assembled(
                        max_bundles=self.ALS_MAX_MESSAGE_UPDATE)))
            if not data_dict:
                return False
            transaction = self._adb.conn.begin()
            self._afc_message_updater.update_db(data_dict, month_idx=month_idx)
            transaction.commit()
            transaction = None
            self._progress_info.als_written += len(data_dict)
        except JsonFormatError as ex:
            self._progress_info.als_format_failures += 1
            if transaction is not None:
                transaction.rollback()
                transaction = None
            self._lookups.reread()
            self._decode_error_writer.write_decode_error(
                ex.msg, line=ex.code_line, data=ex.data)
        finally:
            if transaction is not None:
                transaction.rollback()
        return True

    def _timeout_als_messages(self) -> bool:
        """ Throw away old incomplete ALS messages.
        Returns True if any work was done """
        boundary = datetime.datetime.now() - \
            datetime.timedelta(seconds=self.ALS_MAX_AGE_SEC)
        ret = False
        while True:
            oldest_bundle = self._als_bundles.get_oldest_bundle()
            if (oldest_bundle is None) or \
                    (oldest_bundle.last_update() > boundary):
                break
            ret = True
            self._progress_info.als_retired += 1
            self._als_bundles.remove_oldest_bundle()
            self._decode_error_writer.write_decode_error(
                "Incomplete message bundle removed",
                line=LineNumber.current(),
                data=oldest_bundle.dump())
        return ret

    def _commit_kafka_offsets(self) -> bool:
        """ Commit completed Kafka offsets.
        Returns True if any work was done """
        completed_offsets = self._kafka_positions.get_processed_offsets()
        if not completed_offsets:
            return False
        self._kafka_client.commit(completed_offsets)
        return True

    def _print_progress_report(self) -> None:
        """ Report progress if it's time """
        if not self._report_progress or \
                ((datetime.datetime.now() -
                  self._progress_info.timetag).total_seconds() <
                 self.PROGRESS_REPORT_INTERVAL_SEC):
            return
        print(self._progress_info.report(prev=self._prev_progress_info,
                                         als_bundles=self._als_bundles),
              flush=True)
        self._prev_progress_info = copy.copy(self._progress_info)


def read_sql_file(sql_file: str) -> str:
    """ Returns content of SQL file properly cleaned """
    with open(sql_file, encoding="ascii", newline=None) as f:
        content = f.read()

    # Removing -- and /* */ comments. Courtesy of stackoverflow :)
    def replacer(match: re.Match) -> str:
        """ Replacement callback """
        s = match.group(0)
        return " " if s.startswith('/') else s  # /* */ comment is separator

    return re.sub(
        r'--.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
        replacer, content, flags=re.DOTALL | re.MULTILINE)


def do_init_db(args: Any) -> None:
    """Execute "init" command.

    Arguments:
    args -- Parsed command line arguments
    """
    databases: Set[DatabaseBase] = set()
    try:
        try:
            init_db = InitialDatabase(arg_conn_str=args.init_postgres,
                                      arg_password=args.init_postgres_password)
            databases.add(init_db)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Connection to {InitialDatabase.name_for_logs()} database "
                  f"failed: {ex}")
        nothing_done = True
        for conn_str, password, sql_file, template, db_class, sql_required in \
                [(args.als_postgres, args.als_postgres_password, args.als_sql,
                  args.als_template, AlsDatabase, True),
                 (args.log_postgres, args.log_postgres_password, args.log_sql,
                  args.log_template, LogsDatabase, False)]:
            if not (conn_str or sql_file or template):
                continue
            nothing_done = False
            error_if(sql_file and (not os.path.isfile(sql_file)),
                     f"SQL file '{sql_file}' not found")
            error_if(
                sql_required and not sql_file,
                f"SQL file is required for {db_class.name_for_logs()} "
                f"database")
            created = False
            try:
                database = db_class.parse_conn_str(conn_str).database
                created = \
                    init_db.create_db(
                        db_name=database,
                        if_exists=InitialDatabase.IfExists(args.if_exists),
                        template=template, conn_str=conn_str,
                        password=password)
                db = db_class(arg_conn_str=conn_str, arg_password=password)
                databases.add(db)
                if created and sql_file:
                    db.engine.execute(sa.text(read_sql_file(sql_file)))
            except sa.exc.SQLAlchemyError as ex:
                error(f"{db_class.name_for_logs()} database initialization "
                      f"failed: {ex}")
                if created:
                    try:
                        init_db.drop_db(database)
                    except sa.exc.SQLAlchemyError:
                        pass
        error_if(nothing_done, "Nothing to do")
    finally:
        for db in databases:
            db.dispose()


def do_siphon(args: Any) -> None:
    """Execute "siphon" command.

    Arguments:
    args -- Parsed command line arguments
    """
    adb = AlsDatabase(arg_conn_str=args.als_postgres,
                      arg_password=args.als_postgres_password) \
        if args.als_postgres else None
    ldb = LogsDatabase(arg_conn_str=args.log_postgres,
                       arg_password=args.log_postgres_password) \
        if args.log_postgres else None
    try:
        kafka_params = \
            KafkaConsumerParams(
                **{arg[6:]: value for arg, value in args.__dict__.items()
                   if arg.startswith("kafka_")})
        kafka_client = \
            KafkaClient(
                consumer_params=kafka_params,
                topic_regex_str=ALS_KAFKA_TOPIC if not ldb
                else (f"^(?!.*{ALS_KAFKA_TOPIC}).*$" if not adb else ".+"),
                resubscribe_interval_s=5)
        siphon = Siphon(adb=adb, ldb=ldb, kafka_client=kafka_client,
                        report_progress=args.progress)
        siphon.main_loop()
    finally:
        if adb is not None:
            adb.dispose()
        if ldb is not None:
            ldb.dispose()


def do_init_siphon(args: Any) -> None:
    """Execute "init_siphon" command.

    Arguments:
    args -- Parsed command line arguments
    """
    do_init_db(args)
    do_siphon(args)


def do_help(args: Any) -> None:
    """Execute "help" command.

    Arguments:
    args -- Parsed command line arguments (also contains 'argument_parser' and
            'subparsers' fields)
    """
    if args.subcommand is None:
        args.argument_parser.print_help()
    else:
        args.subparsers.choices[args.subcommand].print_help()


def convert_bool_arg(s):
    """ Converts given string to boolean """
    s = s.lower()
    if s in ("true", "yes", "on", "1", "+"):
        return True
    if s in ("false", "no", "off", "0", "-"):
        return False
    raise ValueError(f"Wrong boolean value: '{s}'")


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    # Kafka server connection switches
    switches_kafka = argparse.ArgumentParser(add_help=False)
    switches_kafka.add_argument(
        "--kafka_servers", "-k", metavar="SERVER[:PORT][,SERVER2[:PORT2]...]",
        help=f"Comma-separated Kafka bootstrap server(s). Default is "
        f"'{DEFAULT_KAFKA_SERVER}'")
    switches_kafka.add_argument(
        "--kafka_client_id", metavar="CLIENT_ID[@]",
        help=f"ID of this instance to be used in Kafka logs. If ends with "
        f"'@' - supplemented with random string (to achieve uniqueness). "
        f"Default is '{DEFAULT_KAFKA_CLIENT_ID}'")
    switches_kafka.add_argument(
        "--kafka_security_protocol", choices=["", "PLAINTEXT", "SSL"],
        help="Security protocol to use. Default is 'PLAINTEXT'")
    switches_kafka.add_argument(
        "--kafka_ssl_keyfile", metavar="FILENAME",
        help="Client private key file for SSL authentication")
    switches_kafka.add_argument(
        "--kafka_ssl_check_hostname",
        metavar="[true/yes/on/1/+ or false/no/off/0/-]", nargs="?",
        const=True, default=None, type=convert_bool_arg,
        help="Match Kafka server name with its certificate. true/yes/on/1/+ "
        "for yes, false/no/off/0/- for no. Default (if switch is omitted or "
        "has no value) is yes")
    switches_kafka.add_argument(
        "--kafka_ssl_cafile", metavar="FILENAME",
        help="CA file for certificate verification")
    switches_kafka.add_argument(
        "--kafka_ssl_certfile", metavar="FILENAME",
        help="Client certificate PEM file name")
    switches_kafka.add_argument(
        "--kafka_ssl_crlfile", metavar="FILENAME",
        help="File name for CRL certificate expiration check")
    switches_kafka.add_argument(
        "--kafka_ssl_ciphers", metavar="CIPHERS",
        help="Available ciphers in OpenSSL cipher list format")

    switches_als_db = argparse.ArgumentParser(add_help=False)
    switches_als_db.add_argument(
        "--als_postgres",
        metavar="[driver://][user][@host][:port][/database][?...]",
        help=f"ALS Database connection string. If some part (driver, user, "
        f"host port database) is missing - it is taken from the default "
        f"connection string (which is '{AlsDatabase.default_conn_str()}'. "
        f"Connection  parameters may be specified after '?' - see "
        f"https://www.postgresql.org/docs/current/libpq-connect.html"
        f"#LIBPQ-CONNSTRING for details")
    switches_als_db.add_argument(
        "--als_postgres_password", metavar="PASSWORD",
        help="Password to use for ALS Database connection")

    switches_log_db = argparse.ArgumentParser(add_help=False)
    switches_log_db.add_argument(
        "--log_postgres",
        metavar="[driver://][user][@host][:port][/database][?...]",
        help=f"Log Database connection string. If some part (driver, user, "
        f"host port database) is missing - it is taken from the default "
        f"connection string (which is '{LogsDatabase.default_conn_str()}'. "
        f"Connection  parameters may be specified after '?' - see "
        f"https://www.postgresql.org/docs/current/libpq-connect.html"
        f"#LIBPQ-CONNSTRING for details. Default is not use log database")
    switches_log_db.add_argument(
        "--log_postgres_password", metavar="PASSWORD",
        help="Password to use for Log Database connection")

    switches_init = argparse.ArgumentParser(add_help=False)
    switches_init.add_argument(
        "--init_postgres",
        metavar="[driver://][user][@host][:port][/database][?...]",
        help=f"Connection string to initial database used as a context for "
        "other databases' creation. If some part (driver, user, host port "
        f"database) is missing - it is taken from the default connection "
        f"string (which is '{InitialDatabase.default_conn_str()}'. Connection "
        f"parameters may be specified after '?' - see "
        f"https://www.postgresql.org/docs/current/libpq-connect.html"
        f"#LIBPQ-CONNSTRING for details")
    switches_init.add_argument(
        "--init_postgres_password", metavar="PASSWORD",
        help="Password to use for initial database connection")
    switches_init.add_argument(
        "--if_exists", choices=["skip", "drop"], default="exc",
        help="What to do if database already exist: nothing (skip) or "
        "recreate (drop). Default is to fail")
    switches_init.add_argument(
        "--als_template", metavar="DB_NAME",
        help="Template database (e.g. bearer of required extensions) to use "
        "for ALS database creation. E.g. postgis/postgis image strangely "
        "assigns Postgis extension on 'template_postgis' database instead of "
        "on default 'template0/1'")
    switches_init.add_argument(
        "--log_template", metavar="DB_NAME",
        help="Template database to use for JSON Logs database creation")
    switches_init.add_argument(
        "--als_sql", metavar="SQL_FILE",
        help="SQL command file that creates tables, relations, etc. in ALS "
        "database. If neither this parameter nor --als_postgres is specified "
        "ALS database is not being created")
    switches_init.add_argument(
        "--log_sql", metavar="SQL_FILE",
        help="SQL command file that creates tables, relations, etc. in JSON "
        "log database. By default database created (if --log_postgres is "
        "specified) empty")

    switches_siphon = argparse.ArgumentParser(add_help=False)
    switches_siphon.add_argument(
        "--progress",
        metavar="[true/yes/on/1/+ or false/no/off/0/-]", nargs="?",
        const=True, default=None, type=convert_bool_arg,
        help="Print progress information")

    # Top level parser
    argument_parser = argparse.ArgumentParser(
        description=f"Tool for moving data from Kafka to PostgreSQL/PostGIS "
        f"database. V{VERSION}")
    subparsers = argument_parser.add_subparsers(dest="subcommand",
                                                metavar="SUBCOMMAND")
    parser_init_db = subparsers.add_parser(
        "init_db", parents=[switches_init, switches_als_db, switches_log_db],
        help="Initialize ALS and/or JSON Log database")
    parser_init_db.set_defaults(func=do_init_db)

    parser_siphon = subparsers.add_parser(
        "siphon",
        parents=[switches_kafka, switches_als_db, switches_log_db,
                 switches_siphon],
        help="Siphon data from Kafka queue to ALS database")
    parser_siphon.set_defaults(func=do_siphon)

    parser_init_siphon = subparsers.add_parser(
        "init_siphon",
        parents=[switches_init, switches_kafka, switches_als_db,
                 switches_log_db, switches_siphon],
        help="Combination of 'db_init' and 'siphon' for Docker convenience")
    parser_init_siphon.set_defaults(func=do_init_siphon)

    # Subparser for 'help' command
    parser_help = subparsers.add_parser(
        "help", add_help=False, usage="%(prog)s subcommand",
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
