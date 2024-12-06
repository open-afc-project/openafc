#!/usr/bin/env python3
""" Grafana initialization/configuration utility """
#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, too-few-public-methods
# pylint: disable=raise-missing-from

import argparse
import jinja2
import os
import pydantic
import sqlalchemy as sa
import subprocess
import sys
from typing import Any, cast, List, NoReturn, Optional
import urllib.parse

import pydantic_utils
import secret_utils

SQLALCHEMY_DB_SCHEME = "postgresql"


def error(errmsg: str) -> NoReturn:
    """ Print given error message and exit """
    print(f"{os.path.basename(__file__)}: Error: {errmsg}", file=sys.stderr)
    sys.exit(1)


def error_if(condition: Any, errmsg: str) -> None:
    """If given condition met - print given error message and exit"""
    if condition:
        error(errmsg)


def do_create_db(args: Any) -> None:
    """ Implementation of 'create_db' subcommand

    Arguments:
    args -- Parsed command line arguments
    """
    settings = \
        cast(Settings,
             pydantic_utils.merge_args(settings_class=Settings, args=args))
    error_if(not settings.grafana_db_dsn, "Grafana DB DSN not specified")
    assert settings.grafana_db_dsn is not None

    try:
        grafana_dsn = \
            secret_utils.substitute_password(
                dsc="Grafana database", dsn=settings.grafana_db_dsn,
                password_file=settings.grafana_db_password_file)
    except RuntimeError as ex:
        error(str(ex))
    assert isinstance(grafana_dsn, str)
    grafana_dsn = \
        urllib.parse.urlunsplit(
            urllib.parse.urlsplit(grafana_dsn).
            _replace(scheme=SQLALCHEMY_DB_SCHEME))
    try:
        grafana_engine = sa.create_engine(grafana_dsn)
        with grafana_engine.connect():
            pass
        grafana_engine.dispose()
        exists = True
    except sa.exc.SQLAlchemyError:
        exists = False
    if exists and not settings.recreate_db:
        return
    error_if(not settings.init_db_dsn,
             "Initialization database DSN not specified")
    try:
        init_dsn = \
            secret_utils.substitute_password(
                dsc="Initialization database", dsn=settings.init_db_dsn,
                password_file=settings.init_db_password_file)
    except RuntimeError as ex:
        error(str(ex))
    grafana_db_name = urllib.parse.urlsplit(grafana_dsn).path.strip("/")
    if exists and settings.recreate_db:
        try:
            init_engine = sa.create_engine(init_dsn)
            with init_engine.connect() as conn:
                conn.execute("COMMIT")
                conn.execute(f'DROP DATABASE IF EXISTS "{grafana_db_name}"')
            init_engine.dispose()
        except sa.exc.SQLAlchemyError as ex:
            error(f"Unable to drop database '{grafana_db_name}': {ex}")
    try:
        init_engine = sa.create_engine(init_dsn)
        with init_engine.connect() as conn:
            conn.execute("COMMIT")
            conn.execute(f'CREATE DATABASE "{grafana_db_name}"')
        init_engine.dispose()
    except sa.exc.SQLAlchemyError as ex:
        error(f"Unable to create database '{grafana_db_name}': {ex}")


def dsn_parse_filter(dsn: str, part: str) -> str:
    """ Implementation of 'dsn_parse' Jinja filter

    Arguments:
    dsn  -- DSN to parse
    part -- Part name (attribute of result returned by urllib.parse.urlparse()
            Possible values: scheme, netlock, path, params, query, fragment,
            username, password, hostname, port
    Returns value of given attribute of given DSN
    """
    try:
        return str(getattr(urllib.parse.urlparse(dsn), part) or "")
    except AttributeError:
        raise jinja2.UndefinedError(f"DSN has no part named '{part}'")


def read_file_filter(filename: str) -> str:
    """ Implementation of read_file jinja filter

    Arguments:
    filename -- Name of file to read
    Returns file content as string """
    try:
        if not os.path.isfile(filename):
            raise jinja2.UndefinedError(f"File '{filename}' not found")
        with open(filename, encoding="ascii") as f:
            return f.read()
    except OSError as ex:
        raise jinja2.UndefinedError(f"Error reading '{filename}': {ex}")


def do_jinja(args: Any) -> None:
    """ Implementation of 'jinja' subcommand

    Arguments:
    args -- Parsed command line arguments
    """
    error_if(not os.path.isdir(args.FROM),
             f"Source directory '{args.FROM}' not found")
    env = jinja2.Environment(loader=jinja2.FileSystemLoader(args.FROM))
    env.filters["dsn_parse"] = dsn_parse_filter
    env.filters["read_file"] = read_file_filter
    for dirpath, _, filenames in os.walk(args.FROM):
        out_dir: Optional[str] = None
        for fn in filenames:
            out_fn = fn
            if args.strip_ext:
                stem, ext = os.path.splitext(fn)
                if ext != args.strip_ext:
                    continue
                out_fn = stem
            rel_dir = \
                os.path.dirname(
                    os.path.relpath(os.path.join(dirpath, fn), args.FROM))
            template = env.get_template(os.path.join(rel_dir, fn))
            if out_dir is None:
                out_dir = os.path.join(args.TO, rel_dir)
                try:
                    os.makedirs(out_dir, exist_ok=True)
                except OSError as ex:
                    error(f"Error creating '{out_dir}' directory: {ex}")

            full_out_fn = os.path.join(out_dir, rel_dir, out_fn)
            try:
                with open(os.path.join(dirpath, full_out_fn), "w",
                          encoding="utf-8") as f:
                    f.write(template.render(**os.environ))
            except OSError as ex:
                error(f"Error writing '{full_out_fn}': {ex}")
        if not args.recursive:
            break


def do_reset_admin_password(args: Any) -> None:
    """ Implementation of 'reset_admin_password' subcommand

    Arguments:
    args -- Parsed command line arguments
    """
    settings = \
        cast(Settings,
             pydantic_utils.merge_args(settings_class=Settings, args=args))
    error_if(not settings.admin_password_file,
             "Grafana admin password specified")
    assert settings.admin_password_file is not None
    error_if(not os.path.isfile(settings.admin_password_file),
             f"Password file '{settings.admin_password_file}' not found")
    with open(settings.admin_password_file, encoding="ascii") as f:
        password = f.read()
    try:
        args = ["grafana", "cli"]
        if settings.grafana_dir:
            args += ["--homepath", settings.grafana_dir]
        if settings.grafana_config:
            args += ["--config", settings.grafana_config]
        args += ["admin", "reset-admin-password", password]
        subprocess.check_call(args)
    except (OSError, subprocess.SubprocessError):
        error("Attempt to (re)set Grafana admin password failed")


def do_help(args: Any) -> None:
    """ Implementation of 'help' subcommand

    Arguments:
    args -- Parsed command line arguments (also contains 'argument_parser' and
            'subparsers' fields)
    """
    if args.subcommand is None:
        args.argument_parser.print_help()
    else:
        args.subparsers.choices[args.subcommand].print_help()


class Settings(pydantic.BaseSettings):
    """ Command line arguments that also may be specified through environment
    variables """
    init_db_dsn: Optional[pydantic.PostgresDsn] = \
        pydantic.Field(None, env="GRAFANA_DATABASE_INIT_URL")
    init_db_password_file: Optional[str] = \
        pydantic.Field(None, env="GRAFANA_DATABASE_INIT_PASSWORD_FILE")
    grafana_db_dsn: Optional[pydantic.PostgresDsn] = \
        pydantic.Field(None, env="GRAFANA_DATABASE_URL")
    grafana_db_password_file: Optional[str] = \
        pydantic.Field(None, env="GRAFANA_DATABASE_PASSWORD_FILE")
    recreate_db: bool = pydantic.Field(False)
    admin_password_file: Optional[str] = \
        pydantic.Field(None, env="GRAFANA_ADMIN_PASSWORD_FILE")
    grafana_dir: Optional[str] = pydantic.Field(None, env="GF_PATHS_HOME")
    grafana_config: Optional[str] = pydantic.Field(None, env="GF_PATHS_CONFIG")

    @pydantic.root_validator(pre=True)
    @classmethod
    def remove_empty(cls, v: Any) -> Any:
        """ Prevalidator that removes empty values (presumably from environment
        variables) to force use of defaults
        """
        if not isinstance(v, dict):
            return v
        for key, value in list(v.items()):
            if value in (None, "", []):
                del v[key]
        return v


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = \
        argparse.ArgumentParser(
            description="Grafana initialization/configuration utility")
    subparsers = argument_parser.add_subparsers(dest="subcommand",
                                                metavar="SUBCOMMAND")

    parser_create_db = \
        subparsers.add_parser(
            "create_db", help="Create Grafana database")
    parser_create_db.add_argument(
        "--init_db_dsn", metavar="DSN_OF_INIT_DB",
        help=f"DSN of Postgres database used to create Grafana database "
        f"(usually it is database named 'postgres'). Ignored if Grafana "
        f"database creation is not required"
        f"{pydantic_utils.env_help(Settings, 'init_db_dsn')}")
    parser_create_db.add_argument(
        "--init_db_password_file", metavar="PASSWORD_FILE_OF_INIT_DB",
        help=f"File with password for DSN, specified by --init_db_dsn"
        f"{pydantic_utils.env_help(Settings, 'init_db_password_file')}")
    parser_create_db.add_argument(
        "--grafana_db_dsn", metavar="DSN_OF_GRAFANA_DB",
        help=f"Grafana Postgres database DSN. This parameter is required"
        f"{pydantic_utils.env_help(Settings, 'grafana_db_dsn')}")
    parser_create_db.add_argument(
        "--grafana_db_password_file", metavar="PASSWORD_FILE_OF_GRAFANA_DB",
        help=f"File with password for DSN, specified by --grafana_db_dsn"
        f"{pydantic_utils.env_help(Settings, 'grafana_db_password_file')}")
    parser_create_db.add_argument(
        "--recreate_db", action="store_true",
        help=f"Recreate Grafana DB if it exists"
        f"{pydantic_utils.env_help(Settings, 'recreate_db')}")
    parser_create_db.set_defaults(func=do_create_db)

    parser_jinja = \
        subparsers.add_parser(
            "jinja",
            help="Instantiate Grafana provisioning templates wirh Jinja using "
            "environment variables")
    parser_jinja.add_argument(
        "--recursive", action="store_true", help="Instantiate recursively")
    parser_jinja.add_argument(
        "--strip_ext", metavar=".EXT",
        help="Source files' extension to strip")
    parser_jinja.add_argument(
        "FROM", help="Source directory")
    parser_jinja.add_argument(
        "TO", help="Destination directory")
    parser_jinja.set_defaults(func=do_jinja)

    parser_reset_admin_password = \
        subparsers.add_parser(
            "reset_admin_password", help="Reset Grafana admin password")
    parser_reset_admin_password.add_argument(
        "--admin_password_file", metavar="PASSWORD_FILE",
        help=f"File containing Grafana admin password"
        f"{pydantic_utils.env_help(Settings, 'admin_password_file')}")
    parser_reset_admin_password.add_argument(
        "--grafana_dir", metavar="GRAFANA_INSTALL_DIR",
        help=f"Grafana install dir"
        f"{pydantic_utils.env_help(Settings, 'grafana_dir')}")
    parser_reset_admin_password.add_argument(
        "--grafana_config", metavar="GRAFANA_CONFIG_FILE",
        help=f"Grafana config or config override file"
        f"{pydantic_utils.env_help(Settings, 'grafana_config')}")

    parser_reset_admin_password.set_defaults(func=do_reset_admin_password)

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
    args.func(args)


if __name__ == "__main__":
    main(sys.argv[1:])
