#!/usr/bin/env python3
# Embedding/extracting FSID table to/from FS SQLite database

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=wrong-import-order, invalid-name
# pylint: disable=too-many-instance-attributes

import argparse
from collections.abc import Iterator, Sequence
import csv
import os
import sqlalchemy as sa
import sys
from typing import Any, Dict, List, Mapping, NamedTuple, Optional, Union

# Name of table for FSID in FS Database
FSID_TABLE_NAME = "fsid_history"


def warning(warnmsg: str) -> None:
    """ Print given warning message """
    print(f"{os.path.basename(__file__)}: Warning: {warnmsg}", file=sys.stderr)


def error(errmsg: str) -> None:
    """ Print given error message and exit """
    print(f"{os.path.basename(__file__)}: Error: {errmsg}", file=sys.stderr)
    sys.exit(1)


def error_if(condition: Any, errmsg: str) -> None:
    """ If given condition met - print given error message and exit """
    if condition:
        error(errmsg)


class Record:
    """ Stuff about and around of records in FSID table

    Private attributes:
    _fields -- FSID record as dictionary, ordered by database field names (i.e.
               a database row dictionary). Absent fields not included
    """
    class SchemaCheckResult(NamedTuple):
        """ Results of database or CSV schema check """
        # Fields in csv/database not known to script
        extra_fields: List[str]

        # Optional fields known to script not found in csv/database
        missing_optional_fields: List[str]

        # Required fields known to script not found in csv/database
        missing_required_fields: List[str]

        def is_ok(self) -> bool:
            """ True if fields in csv/database are the same as in script """
            return not (self.extra_fields or self.missing_optional_fields or
                        self.missing_required_fields)

        def is_fatal(self) -> bool:
            """ True if csv/database lacks some mandatory fields
            """
            return not self.missing_required_fields

        def errmsg(self) -> str:
            """ Error message on field nomenclature mismatch. Empty of all OK
            """
            ret: List[str] = []
            for fields, who, what in \
                    [(self.missing_required_fields, "required", "missing"),
                     (self.missing_optional_fields, "optional", "missing"),
                     (self.extra_fields, "unknown", "present")]:
                if fields:
                    ret.append(f"Following {who} fields are {what}: "
                               f"{', '.join(fields)}")
            return ". ".join(ret)

    # Database dictionary row data type
    RecordDataType = Mapping[str, Optional[Union[str, int, float]]]

    # Dictionary stored in this class data type
    StoredDataType = Dict[str, Union[str, int, float]]

    # Field descriptor
    FieldDsc = NamedTuple("FieldDsc", [("csv_name", str),
                                       ("column", sa.Column)])

    # Fields of FSID table
    _FIELDS = [
        FieldDsc("FSID",
                 sa.Column("fsid", sa.Integer(), primary_key=True)),
        FieldDsc("Region",
                 sa.Column("region", sa.String(10), nullable=True)),
        FieldDsc("Callsign",
                 sa.Column("callsign", sa.String(16), nullable=False)),
        FieldDsc("Path Number",
                 sa.Column("path_number", sa.Integer(), nullable=False)),
        FieldDsc("Center Frequency (MHz)",
                 sa.Column("center_freq_mhz", sa.Float(), nullable=False)),
        FieldDsc("Bandwidth (MHz)",
                 sa.Column("bandwidth_mhz", sa.Float(), nullable=False))]

    # Index of field descriptors by CSV names
    _BY_CSV_NAME = {fd.csv_name: fd for fd in _FIELDS}

    # Index of field descriptors by DB names
    _BY_COLUMN = {fd.column.name: fd for fd in _FIELDS}

    def __init__(self, data_dict: "Record.RecordDataType", is_csv: bool) \
            -> None:
        """ Constructor

        Arguments:
        data_dict -- Row data dictionary
        is_csv    -- True if data_dict is from CSV (field names from heading
                     row, values all strings), False if from DB (field names
                     are column names, values are of proper types)
        """
        self._fields: "Record.StoredDataType"
        if is_csv:
            self._fields = {}
            for csv_name, value in data_dict.items():
                fd = self._BY_CSV_NAME.get(csv_name)
                if (fd is None) or (value is None) or (value == ""):
                    continue
                field_name = fd.column.name
                try:
                    if isinstance(fd.column.type, sa.String):
                        self._fields[field_name] = value
                    elif isinstance(fd.column.type, sa.Integer):
                        self._fields[field_name] = int(value)
                    elif isinstance(fd.column.type, sa.Float):
                        self._fields[field_name] = float(value)
                    else:
                        assert not (f"Internal error: data type "
                                    f"{fd.column.type} not supported by "
                                    f"script")
                except ValueError as ex:
                    error(f"Value '{value}' not valid for field of type "
                          f"'{fd.column.type}': {ex}")
        else:
            self._fields = \
                {name: value for name, value in data_dict.items()
                 if (name in self._BY_COLUMN) and (value is not None)}

    def db_dict(self) -> "Record.StoredDataType":
        """ Row value as dictionary for inserting to DB """
        return self._fields

    def csv_list(self) -> List[str]:
        """ Row value as list of strings for writing to CSV """
        ret: List[str] = []
        for fd in self._FIELDS:
            value = self._fields.get(fd.column.name)
            ret.append("" if value is None else str(value))
        return ret

    @classmethod
    def csv_heading(cls) -> List[str]:
        """ List of CSV headings """
        return [fd.csv_name for fd in cls._FIELDS]

    @classmethod
    def db_columns(cls) -> List[sa.Column]:
        """ List of DB column descriptors """
        return [fd.column for fd in cls._FIELDS]

    @classmethod
    def check_table(cls, table_fields: "Sequence[str]") \
            -> "Record.SchemaCheckResult":
        """ Check database table columns schema in this class """
        return cls.SchemaCheckResult(
            extra_fields=[field for field in table_fields
                          if field not in cls._BY_COLUMN],
            missing_optional_fields=[fd.column.name for fd in cls._FIELDS
                                     if fd.column.nullable and
                                     (fd.column.name not in table_fields)],
            missing_required_fields=[fd.column.name for fd in cls._FIELDS
                                     if (not fd.column.nullable) and
                                     (fd.column.name not in table_fields)])

    @classmethod
    def check_csv(cls, csv_headings: List[str]) -> "Record.SchemaCheckResult":
        """ Check CSV headings against schema in this class """
        return cls.SchemaCheckResult(
            extra_fields=[cn for cn in csv_headings
                          if cn not in cls._BY_CSV_NAME],
            missing_optional_fields=[fd.csv_name for fd in cls._FIELDS
                                     if fd.column.nullable and
                                     (fd.csv_name not in csv_headings)],
            missing_required_fields=[fd.column.name for fd in cls._FIELDS
                                     if (not fd.column.nullable) and
                                     (fd.csv_name not in csv_headings)])

    @classmethod
    def transaction_length(cls, legacy: bool = False) -> int:
        """ Maximum length of SqlAlchemy transaction.

        Frankly, I do not remember where from I got these two constants, and
        this is not that easy to google """
        return (999 if legacy else 32000) // len(cls._FIELDS)

    @classmethod
    def num_fields(cls) -> int:
        """ Number of fields in record """
        return len(cls._FIELDS)


class Db:
    """ Handles all database-related stuff

    Should be used as a context manager (with 'with')

    Private attributes:
    _engine                       -- SqlAlchemy engine
    _conn                         -- SqlAlchemy connection
    _metadata                     -- SqlAlchemy metadata
    _fsid_table                   -- SqlAlchemy table for FSID table, None if
                                     it is not in the table
    _bulk                         -- List of record dictionaries for subsequent
                                     bulk insert
    _use_legacy_transaction_limit -- True to use legacy transaction length,
                                     False for use more recent one
    _current_transaction_length   -- Currently used maximum transaction length
    """
    def __init__(self, database_file: str,
                 recreate_fsid_table: bool = False) -> None:
        """ Constructor

        Arguments:
        database_file       -- SQLite database file
        recreate_fsid_table -- True to create FSID table anew
        """
        error_if(not os.path.isfile(database_file),
                 f"FS SQLite database '{database_file}' not found")
        self._database_file = database_file
        self._conn: sa.engine.Connection = None
        try:
            self._engine = \
                sa.create_engine("sqlite:///" + self._database_file)
            self._metadata = sa.MetaData()
            self._metadata.reflect(bind=self._engine)
            self._conn = self._engine.connect()
            self._fsid_table: Optional[sa.Table] = \
                self._metadata.tables.get(FSID_TABLE_NAME)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Error opening FS database '{self._database_file}': {ex}")
        if recreate_fsid_table:
            try:
                if self._fsid_table is not None:
                    self._metadata.drop_all(self._conn,
                                            tables=[self._fsid_table])
                    # Somehow drop_all() leaves table in metadata
                    self._metadata.remove(self._fsid_table)
                self._fsid_table = sa.Table(FSID_TABLE_NAME, self._metadata,
                                            *Record.db_columns())
                self._metadata.create_all(self._engine)
            except sa.exc.SQLAlchemyError as ex:
                error(f"Error recreating FSID table in FS database "
                      f"'{self._database_file}': {ex}")
        self._bulk: List[Record.RecordDataType] = []
        self._use_legacy_transaction_limit = False
        self._current_transaction_length = self._get_transaction_length()

    def fetchall(self) -> "Iterator[Record]":
        """ Iterator that reads all records of FSID table """
        try:
            for db_row in self._conn.execute(sa.select(self._fsid_table)).\
                    fetchall():
                yield Record(data_dict=db_row._mapping, is_csv=False)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Error reading FSID table from FS database "
                  f"'{self._database_file}': {ex}")

    def write_row(self, row: Record) -> None:
        """ Writes one row to FSID table """
        self._bulk.append(row.db_dict())
        if len(self._bulk) >= self._current_transaction_length:
            self._bulk_write()

    def fsid_fields(self) -> Optional[List[str]]:
        """ List of FSID table fields, None if table is absent """
        return None if self._fsid_table is None \
            else list(self._fsid_table.c.keys())

    def _get_transaction_length(self) -> int:
        """ Maximum length of SqlAlchemy transaction.

        Frankly, I do not remember where from I got these two constants, and
        this is not that easy to google """
        return (999 if self._use_legacy_transaction_limit else 32000) // \
            Record.num_fields()

    def _bulk_write(self) -> None:
        """ Do bulk write to database """
        if not self._bulk:
            return
        transaction: Optional[sa.engine.Transaction] = None
        assert self._fsid_table is not None
        try:
            transaction = self._conn.begin()
            ins = \
                sa.insert(self._fsid_table).execution_options(autocommit=False)
            self._conn.execute(ins, self._bulk)
            transaction.commit()
            transaction = None
            self._bulk = []
            return
        except (sa.exc.CompileError, sa.exc.OperationalError) as ex:
            error_if(self._use_legacy_transaction_limit,
                     f"Error reading FSID table to FS database "
                     f"'{self._database_file}': {ex}")
        finally:
            if transaction is not None:
                transaction.rollback()
                transaction = None
        self._use_legacy_transaction_limit = True
        self._current_transaction_length = self._get_transaction_length()
        try:
            for offset in range(0, len(self._bulk),
                                self._current_transaction_length):
                transaction = self._conn.begin()
                ins = sa.insert(self._fsid_table).\
                    execution_options(autocommit=False)
                self._conn.execute(
                    ins,
                    self._bulk[offset:
                               offset + self._current_transaction_length])
                transaction.commit()
                transaction = None
                self._bulk = []
            return
        finally:
            if transaction is not None:
                transaction.rollback()
                transaction = None

    def __enter__(self) -> "Db":
        """ Context entry - returns self """
        return self

    def __exit__(self, exc_type: Any, exc_value: Any, exc_tb: Any) -> None:
        """ Context exit """
        if self._conn is not None:
            if exc_value is None:
                self._bulk_write()
            self._conn.close()


def do_check(args: Any) -> None:
    """Execute "extract" command.

    Arguments:
    args -- Parsed command line arguments
    """
    with Db(args.SQLITE_FILE) as db:
        if db.fsid_fields() is None:
            print(f"No FSID table in '{args.SQLITE_FILE}'")
            sys.exit(1)


def do_extract(args: Any) -> None:
    """Execute "extract" command.

    Arguments:
    args -- Parsed command line arguments
    """
    try:
        with Db(args.SQLITE_FILE) as db:
            fields = db.fsid_fields()
            error_if(fields is None,
                     f"FS database '{args.SQLITE_FILE}' does not contain FSID "
                     f"history table named '{FSID_TABLE_NAME}'")
            assert fields is not None
            scr = Record.check_table(fields)
            error_if(scr.is_fatal() if args.partial else (not scr.is_ok()),
                     scr.errmsg())
            if not scr.is_ok():
                warning(scr.errmsg())
            with open(args.FSID_FILE, mode="w", newline="", encoding="utf-8") \
                    as f:
                csv_writer = csv.writer(f, lineterminator="\n")
                csv_writer.writerow(Record.csv_heading())
                for record in db.fetchall():
                    csv_writer.writerow(record.csv_list())
    except OSError as ex:
        error(f"Error writing FSID table file '{args.FSID_FILE}': {ex}")


def do_embed(args: Any) -> None:
    """Execute "embed" command.

    Arguments:
    args -- Parsed command line arguments
    """
    error_if(not os.path.isfile(args.FSID_FILE),
             f"FSID table CSV file '{args.FSID_FILE}' not found")
    with open(args.FSID_FILE, mode="r", newline="", encoding="utf-8") as f:
        headings: List[str] = []
        for csv_row in csv.reader(f):
            headings = csv_row
            break
    scr = Record.check_csv(headings)
    error_if(scr.is_fatal() if args.partial else (not scr.is_ok()),
             scr.errmsg())
    if not scr.is_ok():
        warning(scr.errmsg())
    try:
        with Db(args.SQLITE_FILE, recreate_fsid_table=True) as db:
            with open(args.FSID_FILE, mode="r", newline="", encoding="utf-8") \
                    as f:
                for csv_dict in csv.DictReader(f):
                    db.write_row(Record(csv_dict, is_csv=True))
    except OSError as ex:
        error(f"Error reading FSID table file '{args.FSID_FILE}': {ex}")


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


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    # Switches for database file
    switches_database = argparse.ArgumentParser(add_help=False)
    switches_database.add_argument(
        "SQLITE_FILE",
        help="SQLite file containing FS (aka ULS) database")

    # Switches for FSID CSV file
    switches_fsid = argparse.ArgumentParser(add_help=False)
    switches_fsid.add_argument(
        "FSID_FILE",
        help="CSV file containing FSID table")

    # Top level parser
    argument_parser = argparse.ArgumentParser(
        description="Embedding/extracting FSID table to/from FS (aka ULS) "
        "SQLite database")

    subparsers = argument_parser.add_subparsers(dest="subcommand",
                                                metavar="SUBCOMMAND")

    # Subparser for 'check' command
    parser_check = subparsers.add_parser(
        "check", parents=[switches_database],
        help="Exits normally if database contains FSID table (of whatever "
        "scheme), with error otherwise")
    parser_check.set_defaults(func=do_check)

    # Subparser for 'extract' command
    parser_extract = subparsers.add_parser(
        "extract", parents=[switches_database, switches_fsid],
        help="Extract FSID table from FS (aka ULS) Database")
    parser_extract.add_argument(
        "--partial", action="store_true",
        help="Ignore unknown database column names, fill with empty strings "
        "missing columns. By default database columns must match ones, known "
        "to the script")
    parser_extract.set_defaults(func=do_extract)

    # Subparser for 'embed' command
    parser_embed = subparsers.add_parser(
        "embed", parents=[switches_database, switches_fsid],
        help="Embed FSID table from FS (aka ULS) Database")
    parser_embed.add_argument(
        "--partial", action="store_true",
        help="Ignore unknown CSV column names, fill missing columns with "
        "NULL. By default CSV columns must match ones, known to the script")
    parser_embed.set_defaults(func=do_embed)

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
    args.func(args)


if __name__ == "__main__":
    main(sys.argv[1:])
