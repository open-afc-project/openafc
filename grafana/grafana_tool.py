#!/usr/bin/env python3
""" Grafana initialization/configuration utility """
#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, too-few-public-methods
# pylint: disable=raise-missing-from, missing-timeout, too-many-branches
# pylint: disable=too-many-boolean-expressions, too-many-locals
# pylint: disable=too-many-statements

import argparse
import copy
import http
import http.client
from icecream import ic
import jinja2
import json
import operator
import os
import pydantic
import re
import requests
import sqlalchemy as sa
import subprocess
import sys
import tabulate
from typing import Any, cast, Dict, List, NoReturn, Optional, Set, Union
import urllib.parse
import yaml

import pydantic_utils
import db_utils

SQLALCHEMY_DB_SCHEME = "postgresql"
DEFAULT_URL = "http://localhost:3000/grafana"
ADMIN_USER_ENV = "GF_SECURITY_ADMIN_USER"
DEFAULT_DOCKER_SOCKET = "/var/run/docker.sock"
DEFAULT_SERVICE_NAME = "grafana"


def error(errmsg: str) -> NoReturn:
    """ Print given error message and exit """
    print(f"{os.path.basename(__file__)}: Error: {errmsg}", file=sys.stderr)
    sys.exit(1)


def error_if(condition: Any, errmsg: str) -> None:
    """If given condition met - print given error message and exit"""
    if condition:
        error(errmsg)


class Settings(pydantic.BaseSettings):
    """ Command line arguments that also may be specified through environment
    variables """
    db_creator_url: Optional[pydantic.AnyHttpUrl] = \
        pydantic.Field(None, env="AFC_DB_CREATOR_URL")
    grafana_db_dsn: Optional[pydantic.PostgresDsn] = \
        pydantic.Field(None, env="GRAFANA_DATABASE_URL")
    grafana_db_password_file: Optional[str] = \
        pydantic.Field(None, env="GRAFANA_DATABASE_PASSWORD_FILE")
    admin_user: Optional[str] = \
        pydantic.Field(None, env="GF_SECURITY_ADMIN_USER")
    admin_password_file: Optional[str] = \
        pydantic.Field(None, env="GRAFANA_ADMIN_PASSWORD_FILE")
    grafana_dir: Optional[str] = pydantic.Field(None, env="GF_PATHS_HOME")
    grafana_config: Optional[str] = pydantic.Field(None, env="GF_PATHS_CONFIG")
    docker_socket_port: Optional[int] = \
        pydantic.Field(None, env="GRAFANA_DOCKER_SOCKET_PORT")
    compose_service: Optional[str] = \
        pydantic.Field(None, env="GRAFANA_COMPOSE_SERVICE")

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
            db_utils.substitute_password(
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
    if exists and not args.recreate_db:
        return
    error_if(not settings.db_creator_url,
             "Database creation REST API URL not specified")
    try:
        requests.post(settings.db_creator_url,
                      params={"dsn": settings.grafana_db_dsn,
                              "recreate": str(bool(args.recreate_db))})
    except requests.exceptions.RequestException as ex:
        error(f"Grafana database {'re' if args.recreate_db else ''}creation "
              f"error: {ex}")


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
    env = jinja2.Environment(loader=jinja2.FileSystemLoader(args.FROM),
                             keep_trailing_newline=True)
    env.filters["dsn_parse"] = dsn_parse_filter
    env.filters["read_file"] = read_file_filter
    for dirpath, _, filenames in os.walk(args.FROM):
        out_dir: Optional[str] = None
        for fn in filenames:
            out_fn = fn
            if args.strip_ext:
                stem, ext = os.path.splitext(fn)
                if ext == args.strip_ext:
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

            full_out_fn = os.path.join(out_dir, out_fn)
            try:
                with open(full_out_fn, "w", encoding="utf-8") as f:
                    rendered = \
                        template.render(
                            **{k: v for k, v in os.environ.items() if v})
                    f.write(rendered)
                    if args.print:
                        print(f"File: {full_out_fn}\n{rendered}====")
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


def get_auth(settings: Settings) -> Any:
    """ Returns requests' authentication object """
    error_if(not settings.admin_user, "Grafana admin username not specified")
    error_if(not settings.admin_password_file,
             "Grafana admin password file not specified")
    assert settings.admin_password_file is not None
    error_if(not os.path.isfile(settings.admin_password_file),
             f"Grafana admin password file '{settings.admin_password_file}' "
             f"not found")
    with open(settings.admin_password_file, encoding="ascii") as f:
        return (settings.admin_user, f.read())


def get_json(url: str, auth: Any, verbose: bool) -> Any:
    """ Get JSON from given REST API URL

    Arguments:
    url     -- REST API GET URL
    auth    -- Value for requests.get() auth parameter
    verbose -- Print URL being accessed
    Returns decoded JSON object. Doesn't catch exceptions
    """
    if verbose:
        print(url)
    r = requests.get(url, auth=auth)
    if not r.ok:
        msg = http.client.responses.get(r.status_code, "Unknown status code")
        raise requests.RequestException(f"{r.status_code}: {msg}")
    ret = r.json()
    return ret


def remove_ids(data: Union[Dict[str, Any], List[Dict[str, Any]]]) -> None:
    """ Recursive removal of integer IDs, UIDs, grafana URLs """
    if isinstance(data, list):
        for e in data:
            if isinstance(e, (list, dict)):
                remove_ids(e)
    elif isinstance(data, dict):
        for k, v in list(data.items()):
            if ((k == "id") and isinstance(v, int)) or (k == "uid"):
                del data[k]
            elif isinstance(v, (list, dict)):
                remove_ids(v)


def output(data: Union[Dict[str, Any], List[Dict[str, Any]]], indented: bool,
           use_yaml: bool, filename: Optional[str]) -> None:
    """ Output given data

    Arguments:
    data       -- Dada to output
    use_yaml   -- Use YAML output format (default is JSON)
    indented   -- Use Indented JSON format. Ignored if use_yaml is True
    filename   -- File to output to. None to print to stdout
    """
    s = yaml.dump(data, Dumper=yaml.Dumper, indent=2) if use_yaml \
        else json.dumps(data, indent=2 if indented else None)
    if filename:
        try:
            with open(filename, "w", encoding="utf-8") as f:
                f.write(s)
        except OSError as ex:
            error(f"Error writing file '{filename}': {ex}")
    else:
        print(s)


def do_rest(args: Any) -> None:
    """ Implementation of 'rest' subcommand

    Arguments:
    args -- Parsed command line arguments
    """
    settings = \
        cast(Settings,
             pydantic_utils.merge_args(settings_class=Settings, args=args))
    auth = get_auth(settings=settings)
    try:
        result = \
            get_json(
                url=f"{args.url.rstrip('/')}/api/{args.API}", auth=auth,
                verbose=args.verbose)
    except requests.RequestException as ex:
        error(f"{ex}")
    if args.remove_ids:
        remove_ids(result)
    output(result, indented=args.indented, use_yaml=args.yaml,
           filename=args.file)


def do_datasources(args: Any) -> None:
    """ Implementation of 'datasources' subcommand

    Arguments:
    args -- Parsed command line arguments
    """
    settings = \
        cast(Settings,
             pydantic_utils.merge_args(settings_class=Settings, args=args))
    auth = get_auth(settings=settings)
    datasources: List[Dict[str, Any]] = []
    url = args.url.rstrip("/")
    uids: Set[str] = set()
    for key_name, key_type, keys in [("name", "name", args.name),
                                     ("uid", "uid", args.uid)]:
        for key in keys:
            try:
                ds = \
                    get_json(
                        url=f"{url}/api/datasources/{key_type}/{key}",
                        auth=auth, verbose=args.verbose)
                uid = ds.get("uid")
                if uid is not None:
                    if uid in uids:
                        continue
                    uids.add(uid)
                assert isinstance(datasources, list)
                datasources.append(ds)
            except requests.RequestException as ex:
                error(f"Error retrieving datasource with {key_name} of {key}: "
                      f"{ex}")
    if not (args.name or args.uid):
        try:
            datasources = get_json(url=f"{url}/api/datasources", auth=auth,
                                   verbose=args.verbose)
        except (OSError, json.JSONDecodeError) as ex:
            error(f"Error retrieving datasources: {ex}")
    if args.list:
        print(
            tabulate.tabulate(
                [(ds.get("name"), ds.get("uid"), ds.get("type"))
                 for ds in datasources],
                headers=["Name", "UID", "Type"], tablefmt="github"))
    else:
        for_output: Union[List[Dict[str, Any]], Dict[str, Any]] = \
            copy.deepcopy(datasources)
        if args.remove_ids:
            remove_ids(for_output)
        if args.provisioning:
            for_output = \
                {"apiVersion": 1, "datasources": for_output,
                 "deleteDatasources":
                 [{"name": ds.get("name")}
                  for ds in cast(List[Dict[str, Any]], for_output)]}
        output(for_output, indented=args.indented, use_yaml=args.yaml,
               filename=args.file)


def relink_ds(d: Any, ds_uid_to_name=Dict[str, Any]) -> None:
    """ Recursive conversion of datasource references from UID to name

    Arguments:
    d              -- Current recursive level of data
    ds_uid_to_name -- Datasource UID to name translation """
    if isinstance(d, list):
        for e in d:
            if isinstance(e, (list, dict)):
                relink_ds(e, ds_uid_to_name=ds_uid_to_name)
    elif isinstance(d, dict):
        for k, v in list(d.items()):
            if (k == "datasource") and (isinstance(v, dict)) and \
                    (v.get("type") != "grafana") and ("uid" in v):
                ds_name = ds_uid_to_name.get(v.get("uid"))
                error_if(ds_name is None,
                         f"{v.get('type')} data source with uid "
                         f"'{v.get('uid')}' not found")
                d["datasource"] = ds_name
            elif isinstance(v, (list, dict)):
                relink_ds(v, ds_uid_to_name=ds_uid_to_name)


def do_dashboards(args: Any) -> None:
    """ Implementation of 'dashboards' subcommand

    Arguments:
    args -- Parsed command line arguments
    """
    settings = \
        cast(Settings,
             pydantic_utils.merge_args(settings_class=Settings, args=args))
    auth = get_auth(settings=settings)
    url = args.url.rstrip("/")
    root_folder = {"uid": "", "uri": ""}
    folders_by_uid: Dict[str, Dict[str, Any]] = \
        {root_folder["uid"]: root_folder}
    folders_by_name: Dict[str, Dict[str, Any]] = \
        {root_folder["uri"]: root_folder}
    dashboards_by_uid: Dict[str, Dict[str, Any]] = {}
    dashboards_by_name: Dict[str, List[Dict[str, Any]]] = {}
    dashboards_by_folder: Dict[str, List[Dict[str, Any]]] = {}
    try:
        for fd in get_json(url=f"{url}/api/search", auth=auth,
                           verbose=args.verbose):
            if fd.get("isDeleted"):
                continue
            if fd.get("type") == "dash-folder":
                folders_by_uid[fd.get("uid")] = fd
                folders_by_name[fd.get("uri")] = fd
            elif fd.get("type") == "dash-db":
                dashboards_by_uid[fd.get("uid")] = fd
                dashboards_by_name.setdefault(fd.get("title"), []).append(fd)
                dashboards_by_folder.setdefault(fd.get("folderUid", ""), []).\
                    append(fd)
        if not args.list:
            for name in args.name:
                error_if(name not in dashboards_by_name,
                         f"Dashboard '{name}' not found")
            for uid in args.uid:
                error_if(uid not in dashboards_by_uid,
                         f"Dashboard with UID '{uid}' not found")
            for folder in args.folder:
                error_if(folder not in folders_by_name,
                         f"Folder '{folder}' not found")
        lines: List[List[str]] = []
        dashboards: List[Dict[str, Any]] = []
        for folder_name in sorted(folders_by_name.keys()):
            folder = folders_by_name[folder_name]
            for dashboard in \
                    sorted(dashboards_by_folder.get(folder.get("uid"), []),
                           key=operator.itemgetter("title")):
                if args.list:
                    lines.append([cast(str, folder.get("uri")),
                                  cast(str, dashboard.get("title")),
                                  cast(str, dashboard.get("uid"))])
                else:
                    if (args.name or args.uid or args.folder) and \
                            (dashboard.get("title") not in args.name) and \
                            (dashboard.get("uid") not in args.uid) and \
                            (folder.get("uri") not in args.folder):
                        continue
                    dashboards.append(
                        get_json(
                            url=f"{url}/api/dashboards/uid/"
                            f"{dashboard.get('uid')}",
                            auth=auth, verbose=args.verbose))
    except requests.RequestException as ex:
        error(f"Error accessing Grafana API: {ex}")
    if args.list:
        print(
            tabulate.tabulate(
                lines, headers=["Folder", "Dashboard", "Dashboard UID"],
                tablefmt="github"))
    else:
        for_output: Union[List[Dict[str, Any]], Dict[str, Any]] = \
            copy.deepcopy(dashboards)
        if args.relink_ds:
            try:
                ds_uid_to_name = \
                    {ds.get("uid"): ds.get("name") for ds in
                     get_json(url=f"{url}/api/datasources", auth=auth,
                              verbose=args.verbose)}
            except (OSError, json.JSONDecodeError) as ex:
                error(f"Error retrieving datasources: {ex}")
            relink_ds(for_output, ds_uid_to_name=ds_uid_to_name)
        if args.remove_ids:
            remove_ids(for_output)
        if args.provisioning:
            assert isinstance(for_output, list)
            error_if(len(for_output) != 1,
                     "Provisioning requires exactly one resulting dashboard")
            for_output = for_output[0].get("dashboard", {})
        output(for_output, indented=args.indented, use_yaml=args.yaml,
               filename=args.file)


def do_install_plugins(args: Any) -> None:
    """ Implementation of 'install_plugins' subcommand

    Arguments:
    args -- Parsed command line arguments
    """
    settings = \
        cast(Settings,
             pydantic_utils.merge_args(settings_class=Settings, args=args))
    for pd in args.PLUGINS.split(","):
        m = re.match(r"^(([^;])+;)?([^ ]+)( (.+))?$", pd)
        error_if(not m, f"Invalid plugin specification: {pd}")
        assert m is not None
        url, name, version = m.group(2), m.group(3), m.group(5)
        print(
            f"Installing plugin {name}"
            f"{(' (version ' + version + ')') if version else ''}"
            f"{(' from ' + url) if url else ''}")
        args = ["grafana", "cli"]
        if settings.grafana_dir:
            args += ["--homepath", settings.grafana_dir]
        if settings.grafana_config:
            args += ["--config", settings.grafana_config]
        if url:
            args += ["--pluginUrl", url]
        args += ["plugins", "install", name]
        if version:
            args.append(version)
        try:
            subprocess.check_call(args)
        except (OSError, subprocess.SubprocessError):
            error("Plugin installation failed")


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


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """

    switches_url = argparse.ArgumentParser(add_help=False)
    switches_url.add_argument(
        "--url", metavar="http[s]://HOST:PORT[/ROOT]", default=DEFAULT_URL,
        help=f"Base part of grafana URL (used for for API access). Default is "
        f"'{DEFAULT_URL}'")
    switches_url.add_argument(
        "--admin_user", metavar="USERNAME",
        help=f"Grafana admin username"
        f"{pydantic_utils.env_help(Settings, 'admin_user')}")

    switches_admin_password = argparse.ArgumentParser(add_help=False)
    switches_admin_password.add_argument(
        "--admin_password_file", metavar="PASSWORD_FILE",
        help=f"File containing Grafana admin password"
        f"{pydantic_utils.env_help(Settings, 'admin_password_file')}")

    switches_fetch = argparse.ArgumentParser(add_help=False)
    switches_fetch.add_argument(
        "--remove_ids", action="store_true",
        help="Remove id and uid from retrieved definitions (to simplify "
        "subsequent provisioning)")
    switches_fetch.add_argument(
        "--indented", action="store_true",
        help="Use indents in printed JSON content")
    switches_fetch.add_argument(
        "--yaml", action="store_true",
        help="Print YAML, not JSON")
    switches_fetch.add_argument(
        "--file", metavar="FILENAME",
        help="Write to given file instead of print")
    switches_fetch.add_argument(
        "--verbose", action="store_true",
        help="Print REST API being used")

    switches_grafana_files = argparse.ArgumentParser(add_help=False)
    switches_grafana_files.add_argument(
        "--grafana_dir", metavar="GRAFANA_INSTALL_DIR",
        help=f"Grafana install directory"
        f"{pydantic_utils.env_help(Settings, 'grafana_dir')}")
    switches_grafana_files.add_argument(
        "--grafana_config", metavar="GRAFANA_CONFIG_FILE",
        help=f"Grafana config or config override file"
        f"{pydantic_utils.env_help(Settings, 'grafana_config')}")

    argument_parser = \
        argparse.ArgumentParser(
            description="Grafana initialization/configuration utility")
    subparsers = argument_parser.add_subparsers(dest="subcommand",
                                                metavar="SUBCOMMAND")

    parser_create_db = \
        subparsers.add_parser(
            "create_db", help="Create Grafana database")
    parser_create_db.add_argument(
        "--db_creator_url", metavar="DB_CREATOR_URL",
        help=f"REST API URL for Postgres databasse creation. Ignored if "
        f"Grafana database creation is not required"
        f"{pydantic_utils.env_help(Settings, 'db_creator_url')}")
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
        help="Recreate Grafana DB if it exists")
    parser_create_db.set_defaults(func=do_create_db)

    parser_jinja = \
        subparsers.add_parser(
            "jinja",
            help="Instantiate Grafana provisioning templates with Jinja using "
            "environment variables")
    parser_jinja.add_argument(
        "--recursive", action="store_true", help="Instantiate recursively")
    parser_jinja.add_argument(
        "--strip_ext", metavar=".EXT",
        help="Source files' extension to strip. Files without this extension "
        "are templatized without name change")
    parser_jinja.add_argument(
        "--print", action="store_true",
        help="Also print resulted files")
    parser_jinja.add_argument(
        "FROM", help="Source directory")
    parser_jinja.add_argument(
        "TO", help="Destination directory")
    parser_jinja.set_defaults(func=do_jinja)

    parser_reset_admin_password = \
        subparsers.add_parser(
            "reset_admin_password",
            parents=[switches_admin_password, switches_grafana_files],
            help="Reset Grafana admin password")
    parser_reset_admin_password.set_defaults(func=do_reset_admin_password)

    parser_rest = \
        subparsers.add_parser(
            "rest",
            parents=[switches_url, switches_admin_password, switches_fetch],
            help="Raw rest API operation (for now GET only)")
    parser_rest.add_argument(
        "API", help="Part of rest URL after /api/")
    parser_rest.set_defaults(func=do_rest)

    parser_datasources = \
        subparsers.add_parser(
            "datasources",
            parents=[switches_url, switches_admin_password, switches_fetch],
            help="Read Grafana datasources")
    parser_datasources.add_argument(
        "--list", action="store_true",
        help="Print datasource list")
    parser_datasources.add_argument(
        "--name", metavar="DATASOURCE_NAME", action="append", default=[],
        help="Print datasources with given name. This parameter may be "
        "specified more than once. By default (if '--uid' not specified), all "
        "datasources are printed")
    parser_datasources.add_argument(
        "--uid", metavar="DATASOURCE_UID", action="append", default=[],
        help="Print datasource with given UID. This parameter may be "
        "specified more than once. By default (if '--name' not specified), "
        "all datasources are printed")
    parser_datasources.add_argument(
        "--provisioning", action="store_true",
        help="Wrap to make provisioning descriptor")
    parser_datasources.set_defaults(func=do_datasources)

    parser_dashboards = \
        subparsers.add_parser(
            "dashboards",
            parents=[switches_url, switches_admin_password, switches_fetch],
            help="Read Grafana dashboards")
    parser_dashboards.add_argument(
        "--list", action="store_true",
        help="Print dashboard list")
    parser_dashboards.add_argument(
        "--name", metavar="DASHBOARD_NAME", action="append", default=[],
        help="Print dashboards with given name. This parameter may be "
        "specified more than once. By default (if other selection parameters "
        "not specified), all dashboards are printed")
    parser_dashboards.add_argument(
        "--uid", metavar="DASHBOARD_UID", action="append", default=[],
        help="Print dashboard with given UID. This parameter may be "
        "specified more than once. By default (if other selection parameters "
        "not specified), all dashboards are printed")
    parser_dashboards.add_argument(
        "--folder", metavar="FOLDER_NAME", action="append", default=[],
        help="Print dashboards from given folder. This parameter may be "
        "specified more than once. By default (if other selection parameters "
        "not specified), all dashboards are printed")
    parser_dashboards.add_argument(
        "--relink_ds", action="store_true",
        help="Convert uid-based datasource references to name-based ones")
    parser_dashboards.add_argument(
        "--provisioning", action="store_true",
        help="Strip to leave only part that goes to provisioning. Only "
        "allowed for a single dashboard")
    parser_dashboards.set_defaults(func=do_dashboards)

    parser_inatall_plugins = \
        subparsers.add_parser(
            "install_plugins", parents=[switches_grafana_files],
            help="Install plugins")
    parser_inatall_plugins.add_argument(
        "PLUGINS",
        help="Same format as GF_INSTALL_PLUGINS: comma separated "
        "[URL_OF_PLUGIN_ZIP;]PLUGIN_NAME[ PLUGIN_VERSION] sequence")
    parser_inatall_plugins.set_defaults(func=do_install_plugins)

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

    args, _ = argument_parser.parse_known_args(argv)
    if args.subcommand != "help":
        args = argument_parser.parse_args(argv)

    args.func(args)


if __name__ == "__main__":
    main(sys.argv[1:])
