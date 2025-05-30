#!/usr/bin/env python3
""" ALS database maintenence tool """

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=wrong-import-order, too-many-locals

import argparse
import datetime
import dateutil.relativedelta
import logging
import os
import re
import sqlalchemy as sa
import sys
import tabulate
from typing import Any, Dict, List, NamedTuple, NoReturn, Optional
import yaml

import utils


def unused_arguments(*args) -> None:  # pylint: disable=unused-argument
    """ Sink for unused arguments """
    return


def error(msg: str) -> NoReturn:
    """ Prints given msg as error message and exit abnormally """
    logging.error(msg)
    sys.exit(1)


def error_if(cond: Any, msg: str) -> None:
    """ If condition evaluates to true prints given msg as error message and
    exits abnormally """
    if cond:
        error(msg)


def start_month_idx(year_month: Optional[str],
                    keep_months: Optional[int]) -> int:
    """ Month index that corresponds to given month in the past

    Arguments:
    year_month  -- "year-month" string or None
    keep_months -- Number of months to keep (1 - only current) or None
    Returns month index of interval end """
    error_if((year_month is None) == (keep_months is None),
             "Either --keep_from or --keep_months (but not both) should be "
             "specified")
    if year_month:
        try:
            dt = datetime.datetime.strptime(year_month, "%Y-%m")
        except ValueError:
            error("Invalid format of --keep_from. Must be 'year-month'")
        error_if(dt > (datetime.datetime.now() +
                       dateutil.relativedelta.relativedelta(months=1)),
                 "--keep_from must be in the past")
        return max(0, utils.get_month_idx(year=dt.year, month=dt.month))
    assert keep_months is not None
    error_if(keep_months <= 0, "--keep_months must be greater than 1")
    return max(0, utils.get_month_idx() - (keep_months - 1))


def end_month_idx(year_month: Optional[str],
                  months_ahead: Optional[int]) -> int:
    """ Month index that corresponds to given month past now

    Arguments:
    year_month   -- "year-month" string or None
    months_ahead -- Number of months ahead or None
    Returns month index of interval end """
    error_if(
        (year_month is None) == (months_ahead is None),
        "Either --up_to or --months_ahead (but not both) should be specified")
    if year_month:
        try:
            dt = datetime.datetime.strptime(year_month, "%Y-%m")
        except ValueError:
            error("Invalid format of --up_to. Must be 'year-month'")
        error_if(dt < (datetime.datetime.now() -
                       dateutil.relativedelta.relativedelta(months=1)),
                 "--up_to must be past now")
        return utils.get_month_idx(year=dt.year, month=dt.month)
    assert months_ahead is not None
    error_if(months_ahead < 0, "--months_ahead must be nonnegative")
    return utils.get_month_idx() + months_ahead


def do_prepare_sql(cfg: Dict[str, Any], args: argparse.Namespace) -> None:
    """ Execute "do_prepare_sql" command.

    Arguments:
    cfg  -- Yaml config file in dictionary form
    args -- Parsed command line arguments
    """
    error_if(args.prefix and (args.months_ahead is not None),
             "--prefix and --months_ahead can't be specified tohether")
    error_if(not os.path.isfile(args.INPUT_FILE),
             f"Input file '{args.INPUT_FILE}' not found")
    try:
        with open(args.INPUT_FILE, encoding="utf-8") as f:
            sql = f.read()
    except OSError as ex:
        error(f"Error reading input file '{args.INPUT_FILE}': {ex}")
    tables: List[str] = []
    references: Dict[str, List[str]] = {}
    try:
        with open(args.OUTPUT_FILE, "w", encoding="utf-8") as f:
            def write_sql(sql: str) -> None:
                """ Writes SQL statement to output file, applying some final
                post-processing """
                if args.if_not_exists:
                    sql = \
                        re.sub(r"^CREATE TABLE", "CREATE TABLE IF NOT EXISTS",
                               sql)
                for repl in cfg["replaces"]:
                    sql = re.sub(repl["from"], repl["to"], sql)
                f.write(f"{sql.rstrip(';')};\n")

            for line in cfg.get("prologue", []):
                write_sql(line)
            for stmt in re.split(r"(?<=;)\s*", sql):
                if not stmt:
                    continue
                if any(f'TABLE "{ttr}"' in stmt
                       for ttr in cfg.get("tables_to_remove", [])):
                    continue
                m_ref = \
                    re.match(
                        r'^ALTER TABLE "(\S+?)"(.|\n)*REFERENCES "(\S+?)"',
                        stmt)
                if m_ref:
                    references.setdefault(m_ref.group(1), []).\
                        append(m_ref.group(3))
                m_create = re.match(r'^CREATE TABLE "(\S+?)"', stmt)
                if m_create:
                    tables.append(m_create.group(1))
                if args.prefix:
                    if m_create:
                        write_sql(
                            f"{stmt[:m_create.start(1)]}{args.prefix}"
                            f"{stmt[m_create.start(1):]}")
                    continue
                if m_create:
                    if args.up_to or (args.months_ahead is not None):
                        write_sql(f"{stmt.rstrip(';')} "
                                  f"PARTITION BY LIST (month_idx)")
                        for month_idx in \
                                range(utils.get_month_idx(),
                                      end_month_idx(
                                          year_month=args.up_to,
                                          months_ahead=args.months_ahead) + 1):
                            table_name = \
                                cfg["partition_name_template"].\
                                format(table_name=m_create.group(1),
                                       month_idx=month_idx)
                            write_sql(
                                f'CREATE TABLE "{table_name}" PARTITION OF '
                                f'"{m_create.group(1)}" FOR VALUES IN '
                                f'({month_idx})')
                    else:
                        write_sql(stmt)
                else:
                    write_sql(stmt)
    except OSError as ex:
        error(f"Error writing output file '{args.OUTPUT_FILE}': {ex}")
    if args.print_tables:
        pr: List[str] = []
        while len(pr) < len(tables):
            for table in tables:
                if (table not in pr) and \
                        all((referred in pr)
                            for referred in references.get(table, [])):
                    pr.append(table)
        for table in pr:
            print(table)


# Information about single partition table
PartitionInfo = \
    NamedTuple("PartitionInfo", [("month_idx", int), ("table_name", str)])


class Db:
    """ Database operations

    Private attributes:
    _engine  -- SqlAlchemy engine
    _verbose -- True to print SQL statements being executed
    """
    def __init__(self, cfg: Dict[str, Any], dsn: Optional[str],
                 password_file: Optional[str], verbose: bool) -> None:
        """ Constructor

        Arguments:
        cfg           -- Config as dictionary
        dsn           -- DSN passed in command line
        password_file -- Password filr name passed in command line
        verbose       -- True to print SQL statement executed
        """
        try:
            dsn = \
                utils.dsn(
                    name_for_logs="ALS database", arg_dsn=dsn,
                    dsn_env=cfg["als_conn_env"], password_file=password_file,
                    password_file_env=cfg["als_password_env"],
                    default_db=cfg["als_db_name"])
        except ValueError as ex:
            error(str(ex))
        try:
            self._engine = sa.create_engine(dsn)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Error connecting to ALS database: {ex}")
        self._cfg = cfg
        self._verbose = verbose

    def execute(self, sql_stmt: str) -> Any:
        """ Execute given SQL statement. Return whatever connection.execute()
        returns """
        try:
            if self._verbose:
                logging.info(sql_stmt)
            with self._engine.connect() as conn:
                return conn.execute(sa.text(sql_stmt))
        except sa.exc.SQLAlchemyError as ex:
            error(str(ex))

    def get_partitions(self) -> Dict[str, List[PartitionInfo]]:
        """ Returns dictionary of partition month indices, indexed by base
        table names """
        dummy_month_idx = 999999
        partition_regex = \
            self._cfg["partition_name_template"].format(
                table_name=r"(.+)", month_idx=dummy_month_idx)
        partition_regex = \
            "^" + partition_regex.replace(str(dummy_month_idx), r"(\d+)") + "$"
        ret: Dict[str, List[PartitionInfo]] = {}
        for row in \
                self.execute(
                    "SELECT table_name FROM information_schema.tables "
                    "WHERE table_schema = 'public' AND "
                    "table_type = 'BASE TABLE';").fetchall():
            table = row[0]
            m = re.match(partition_regex, table)
            if not m:
                continue
            ret.setdefault(m.group(1), []).append(
                PartitionInfo(month_idx=int(m.group(2)), table_name=table))
        return ret


def do_add_partitions(cfg: Dict[str, Any], args: argparse.Namespace) -> None:
    """ Execute "add_partitions" command.

    Arguments:
    cfg  -- Yaml config file in dictionary form
    args -- Parsed command line arguments
    """
    end_month = \
        end_month_idx(year_month=args.up_to, months_ahead=args.months_ahead)
    db = Db(cfg=cfg, dsn=args.dsn,  password_file=args.password_file,
            verbose=args.verbose)
    current_month_idx = utils.get_month_idx()
    for parent_table, partitions in db.get_partitions().items():
        last_month_idx = max(partitions)
        for month_idx in range(current_month_idx, end_month + 1):
            if any(pi.month_idx == month_idx for pi in partitions):
                continue
            partition_name = \
                cfg["partition_name_template"].format(table_name=parent_table,
                                                      month_idx=month_idx)
            db.execute(
                f"CREATE TABLE IF NOT EXISTS {partition_name} "
                f"PARTITION ON {parent_table} FOR VALUES IN ({month_idx});")


def do_remove_partitions(cfg: Dict[str, Any], args: argparse.Namespace) \
        -> None:
    """ Execute "remove_partitions" command.

    Arguments:
    cfg  -- Yaml config file in dictionary form
    args -- Parsed command line arguments
    """
    start_month = start_month_idx(year_month=args.keep_from,
                                  keep_months=args.keep_months)
    db = Db(cfg=cfg, dsn=args.dsn,  password_file=args.password_file,
            verbose=args.verbose)
    for parent_table, partitions in db.get_partitions().items():
        for pi in partitions:
            if pi.month_idx >= start_month:
                continue
            db.execute(f"DROP TABLE IF EXISTS {pi.table_name} CASCADE;")


def do_partition_info(cfg: Dict[str, Any], args: argparse.Namespace) -> None:
    """ Execute "partition_info" command.

    Arguments:
    cfg  -- Yaml config file in dictionary form
    args -- Parsed command line arguments
    """
    db = Db(cfg=cfg, dsn=args.dsn,  password_file=args.password_file,
            verbose=args.verbose)
    partition_dict = db.get_partitions()
    base_date = \
        datetime.datetime(year=cfg["month_idx_base_year"], month=1, day=1)
    table_rows: List[List[str]] = []
    for parent_table in sorted(partition_dict.keys()):
        partitions = partition_dict[parent_table]
        if args.by_parent:
            first_idx = min(pi.month_idx for pi in partitions)
            last_idx = max(pi.month_idx for pi in partitions)
            first_date = base_date + \
                dateutil.relativedelta.relativedelta(months=first_idx)
            last_date = base_date + \
                dateutil.relativedelta.relativedelta(months=last_idx)
            table_rows.append(
                [parent_table,
                 f"{datetime.datetime.strftime(first_date, '%b %Y')} "
                 f"({first_idx})",
                 f"{datetime.datetime.strftime(last_date, '%b %Y')} "
                 f"({last_idx})"])
        else:
            for table_name in sorted([pi.table_name for pi in partitions]):
                print(table_name)
    if args.by_parent:
        print(
            tabulate.tabulate(
                table_rows, headers=["Table", "First Month", "Last Month"],
                tablefmt="github"))


def do_help(cfg: Dict[str, Any], args: argparse.Namespace,
            unknown_args: List[str]) -> None:
    """ Execute "help" command.

    Arguments:
    cfg          -- Yaml config file in dictionary form
    args         -- Parsed command line arguments (also contains
                    'argument_parser' and 'subparsers' fields)
    unknown_args -- Rest of command line (to which 'help' was prepended)
    """
    unused_arguments(unknown_args, cfg)
    if args.subcommand is None:
        args.argument_parser.print_help()
    else:
        args.subparsers.choices[args.subcommand].print_help()


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    cfg_file = os.path.splitext(__file__)[0] + ".yaml"
    error_if(not os.path.isfile(cfg_file),
             f"Config file '{cfg_file}' not found")
    try:
        with open(cfg_file, encoding="utf-8") as f:
            cfg = yaml.load(f, Loader=yaml.FullLoader)
    except (OSError, yaml.YAMLError) as ex:
        error(f"Error reading config file '{cfg_file}': {ex}")
    # Database connection string switches
    switches_dsn = argparse.ArgumentParser(add_help=False)
    switches_dsn.add_argument(
        "--dsn",
        metavar="[postgresql://][USER][:PASSWORD][@HOST][:PORT][/DB]"
        "[?OPTIONS]",
        help=f"PostgreSQL connection string (DSN) for ALS database. Omitted "
        f"parts are taken either {cfg['als_conn_env']} environment variable "
        f"or from defaults. Also password may be taken from password file")
    switches_dsn.add_argument(
        "--password_file", metavar="PASSWORD_FILE",
        help=f"File containing password for ALS database connection string "
        f"(DSN). If not specified - file name taken from "
        f"{cfg['als_password_env']} environment variable or password is taken "
        f"from connection string or default value is used")
    switches_dsn.add_argument(
        "--verbose", action="store_true",
        help="Print SQL statements being executed")

    switches_partitions = argparse.ArgumentParser(add_help=False)
    switches_partitions.add_argument(
        "--months_ahead", metavar="MONTHS_AHEAD", type=int,
        help="Prepare tables' partitions for given number of months ahead")
    switches_partitions.add_argument(
        "--up_to", metavar="YEAR-MONTH",
        help="Prepare tables' partitions for up to given month of given year")

    # Top level parser
    argument_parser = argparse.ArgumentParser(
        description="ALS database maintenence tool")
    subparsers = argument_parser.add_subparsers(dest="subcommand",
                                                metavar="SUBCOMMAND")

    # Subparser for "prepare_sql" command
    parser_prepare_sql = subparsers.add_parser(
        "prepare_sql", parents=[switches_partitions],
        help="Postprocess SQL, generated by dbdiagram.io")
    parser_prepare_sql.add_argument(
        "--prefix", metavar="DB_NAME_PREFIX",
        help="Prefix to add to names of generated tables. If this parameter "
        "specified, only nonsegmented tables are created (as intermediate "
        "containers for alembic miggrations) - indices, constraints and "
        "comments are not createed. Can't be specified together with "
        "--months_ahead or --up_to")
    parser_prepare_sql.add_argument(
        "--if_not_exists", action="store_true",
        help="Add 'IF NOT EXIST' clause to all 'CREATE TABLE'")
    parser_prepare_sql.add_argument(
        "--print_tables", action="store_true",
        help="Print list of tables")
    parser_prepare_sql.add_argument(
        "INPUT_FILE",
        help="Input SQL file, generated by dbdiagram.io")
    parser_prepare_sql.add_argument(
        "OUTPUT_FILE",
        help="Output SQL file to be used for ALS database creation. "
        "Partitioned if --months_ahead specified")
    parser_prepare_sql.set_defaults(func=do_prepare_sql)

    # Subparser for "add_partitions" command
    parser_add_partitions = subparsers.add_parser(
        "add_partitions", parents=[switches_dsn, switches_partitions],
        help="Add partitions for future months to partitioned ALS database")
    parser_add_partitions.set_defaults(func=do_add_partitions)

    # Subparser for "remove_partitions" command
    parser_remove_partitions = subparsers.add_parser(
        "remove_partitions", parents=[switches_dsn],
        help="Remove older partitions from ALS database")
    parser_remove_partitions.add_argument(
        "--keep_months", metavar="MONTHS_TO_KEEP", type=int,
        help="How many months to keep in the ALS database (1 - only the "
        "current)")
    parser_remove_partitions.add_argument(
        "--keep_from", metavar="YEAR-MONTH",
        help="Year and month of first partition to keep (drop those before)")
    parser_remove_partitions.set_defaults(func=do_remove_partitions)

    # Subparser for "partition_info" command
    parser_partition_info = subparsers.add_parser(
        "partition_info", parents=[switches_dsn],
        help="Prints partition information")
    parser_partition_info.add_argument(
        "--by_parent", action="store_true",
        help="Print summary by parent table. Default is to print list of "
        "partition tables")
    parser_partition_info.set_defaults(func=do_partition_info)

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

    if not argv:
        argument_parser.print_help()
        sys.exit(1)

    unknown_args: Optional[List[str]]
    args, unknown_args = argument_parser.parse_known_args(argv)
    if not getattr(args, "unknown_args", False):
        args = argument_parser.parse_args(argv)
        unknown_args = None

    # Set up logging
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(
        logging.Formatter(
            f"{os.path.basename(__file__)}. %(levelname)s: %(message)s"))
    logging.getLogger().addHandler(console_handler)
    logging.getLogger().setLevel(logging.INFO)

    # Do the needful
    if unknown_args is not None:
        args.func(cfg=cfg, args=args, unknown_args=unknown_args)
    else:
        args.func(cfg=cfg, args=args)


if __name__ == "__main__":
    main(sys.argv[1:])
