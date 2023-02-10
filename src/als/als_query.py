#!/usr/bin/env python3
# Tool for querying ALS database

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

import argparse
import csv
import datetime
import json
import logging
import os
import psycopg2
import re
import sqlalchemy as sa
import sqlalchemy.dialects.postgresql as sa_pg
import subprocess
import sys
from typing import Any, List, Optional, Set

try:
    import geoalchemy2 as ga                        # type: ignore
except ImportError:
    pass

VERSION = "0.1"

DEFAULT_USER = "postgres"
DEFAULT_PORT = 5432

ALS_DB = "ALS"
LOG_DB = "AFC_LOGS"


def error(msg: str) -> None:
    """ Prints given msg as error message and exit abnormally """
    logging.error(msg)
    sys.exit(1)


def error_if(cond: Any, msg: str) -> None:
    """ If condition evaluates to true prints given msg as error message and
    exits abnormally """
    if cond:
        error(msg)


class DbConn:
    """ Database connection encapsulation

    Attributes:
    db_name  -- Database name
    engine   -- Database engine
    metadata -- Database metadata
    conn     -- Database connection
    """
    def __init__(self, conn_str: str, password: Optional[str], db_name: str) \
            -> None:
        """ Constructor

        Arguments:
        conn_str -- Abbreviated conneftion string, as specified in command line
        password -- Optional password
        db_name  -- Database name
        """
        self.db_name = db_name

        m = re.match(
            r"^(?P<user>[^ :\?]+@)?"
            r"(?P<cont>\^)?(?P<host>[^ :?]+)"
            r"(:(?P<port>\d+))?"
            r"(?P<options>\?.+)?$",
            conn_str)
        error_if(not m, f"Server string '{conn_str}' has invalid format")
        assert m is not None

        user = m.group("user") or DEFAULT_USER
        host = m.group("host")
        port = m.group("port") or str(DEFAULT_PORT)
        options = m.group("options") or ""
        if m.group("cont"):
            try:
                insp_str = subprocess.check_output(["docker", "inspect", host])
            except (OSError, subprocess.CalledProcessError) as ex:
                error(f"Failed to inspect container '{host}': {ex}")
            insp = json.loads(insp_str)
            try:
                networks = insp[0]["NetworkSettings"]["Networks"]
                host = networks[list(networks.keys())[0]]["IPAddress"]
            except (LookupError, TypeError, ValueError) as ex:
                error(f"Failed to find server IP address in container "
                      f"inspection: {ex}")
        try:
            full_conn_str = \
                f"postgresql+psycopg2://{user}" \
                f"{(':' + password) if password else ''}@{host}:{port}/" \
                f"{db_name}{options}"
            self.engine = sa.create_engine(full_conn_str)
            self.metadata = sa.MetaData()
            self.metadata.reflect(bind=self.engine)
            self.conn = self.engine.connect()
        except sa.exc.SQLAlchemyError as ex:
            error(f"Failed to connect to '{db_name}' at '{conn_str}' "
                  f"('{full_conn_str}'): {ex}")


class JsonEncoder(json.JSONEncoder):
    """ JSON encoder that handles unusual types """
    def default(self, o: Any) -> Any:
        """ Handles unusual data types """
        if isinstance(o, datetime.datetime):
            return o.isoformat()
        return super().default(o)


def do_log(args: Any) -> None:
    """Execute "log" command.

    Arguments:
    args -- Parsed command line arguments
    """
    db_conn = \
        DbConn(conn_str=args.server, password=args.password, db_name=LOG_DB)
    work_done = False
    if args.topics:
        work_done = True
        for topic in sorted(db_conn.metadata.tables.keys()):
            print(topic)
    if args.sources is not None:
        work_done = True
        sources: Set[str] = set()
        error_if(
            args.sources and (args.sources not in db_conn.metadata.tables),
            f"Topic '{args.sources}' not found")
        for topic in db_conn.metadata.tables.keys():
            if "source" not in db_conn.metadata.tables[topic].c:
                continue
            if args.sources and (args.sources != topic):
                continue
            table_sources = \
                db_conn.conn.execute(
                    sa.text(f'SELECT DISTINCT source FROM "{topic}"')).\
                    fetchall()
            sources |= {s[0] for s in table_sources}
        for source in sorted(sources):
            print(source)
    if args.SELECT:
        work_done = True
        try:
            rp = db_conn.conn.execute(
                sa.text("SELECT " + " ".join(args.SELECT)))
            if args.format == "bare":
                for record in rp:
                    error_if(
                        len(record) != 1,
                        f"Bare format assumes one field per result row "
                        f"(this query has {len(record)} fields per record)")
                    print(record[0])
            elif args.format == "json":
                print("[")
                for record in rp:
                    print("    " + json.dumps(record._asdict(),
                                              cls=JsonEncoder))
                print("]")
            elif args.format == "csv":
                csv_writer = csv.writer(sys.stdout)
                csv_writer.writerow(rp.keys())
                for record in rp:
                    csv_writer.writerow(record)
            else:
                error(f"Internal error: unsupported output format "
                      f"'{args.format}'")
        except sa.exc.SQLAlchemyError as ex:
            error(f"Database acces error: {ex}")
    error_if(not work_done, "Nothing to do!")


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
    # Database connection switches
    switches_server = argparse.ArgumentParser(add_help=False)
    switches_server.add_argument(
        "--server", "-s", metavar="[user@]{host|^container}[:port][?options]",
        required=True,
        help=f"PostgreSQL server connection information. Host part may be a "
        f"hostname, IP address, or container name or ID, preceded by '^' "
        f"(specifying container name would not work if script runs inside the "
        f"container). Default username is '{DEFAULT_USER}', default port is "
        f"{DEFAULT_PORT}. Options may specify various (e.g. SSL-related) "
        f"parameters (see "
        f"https://www.postgresql.org/docs/current/libpq-connect.html"
        f"#LIBPQ-CONNSTRING for details). This parameter is mandatory")
    switches_server.add_argument(
        "--password", metavar="PASSWORD",
        help="Postgres connection password (if required)")

    # Top level parser
    argument_parser = argparse.ArgumentParser(
        description=f"Tool for querying ALS database. V{VERSION}")
    subparsers = argument_parser.add_subparsers(dest="subcommand",
                                                metavar="SUBCOMMAND")

    # Subparser for "log" command
    parser_log = subparsers.add_parser(
        "log", parents=[switches_server],
        help="Read JSON log messages")
    parser_log.add_argument(
        "--topics", action="store_true",
        help="Print list of topics, stored in database")
    parser_log.add_argument(
        "--sources", metavar="[TOPIC]", nargs="?", const="",
        help="Print list of log sources - for all topics or for specific "
        "topic")
    parser_log.add_argument(
        "--format", "-f", choices=["bare", "json", "csv"], default="csv",
        help="Output format for 'SELECT' result. 'bare' is unadorned "
        "value-per-line output (must be just one field per result row), "
        "'csv' - CSV format (default), 'json' - JSON format")

    parser_log.add_argument(
        "SELECT", nargs="*",
        help="SELECT command body (without 'SELECT'). 'FROM' clause should "
        "use topic name, column names are 'time' (timetag), 'source' (AFC or "
        "whatever server ID) and 'log' (JSON log record). Surrounding quotes "
        "are optional")
    parser_log.set_defaults(func=do_log)

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
