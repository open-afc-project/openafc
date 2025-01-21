""" Database/user creation """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=too-many-arguments, too-many-positional-arguments
# pylint: disable=too-few-public-methods, too-many-locals, too-many-branches
# pylint: disable=too-many-statements, wrong-import-order

import os
import requests
import sqlalchemy as sa
from typing import cast, Optional, Tuple, Union
import urllib.parse

from db_utils import error, error_if, safe_dsn, substitute_password

__all__ = ["ensure_dsn"]

# Environment variable containing DB Creator REST API URL
DB_CREATOR_URL_ENV = "AFC_DB_CREATOR_URL"

# Prefix for db creator user/database (usually `postgres/postgres`) DSNs
# on various servers
DB_CREATOR_DSN_ENV_PREFIX = "AFC_DB_CREATOR_DSN_"

# Prefix for correspondent password filename environment variables
DB_CREATOR_PASSWORD_FILE_ENV_PREFIX = "AFC_DB_CREATOR_PASSWORD_FILE_"

# Default PostgreSQL port
DEFAULT_POSTGRES_PORT = 5432

# Default PostgreSQL username
DEFAULT_POSTGRES_USER = "postgres"

# Default PostgreSQL password
DEFAULT_POSTGRES_PASSWORD = "postgres"


class DsnInfo:
    """ Parsed PostgreSQL DSN

    Attributes:
    host     -- Hostname
    port     -- Port. Default is DEFAULT_POSTGRES_PORT
    user     -- Username. Default is DEFAULT_POSTGRES_USER
    password -- Password. Default is DEFAULT_POSTGRES_PASSWORD
    db       -- Database name
    dsn      -- DSN with password properly substituted
    """
    def __init__(self, dsn: str, password_file: Optional[str] = None,
                 password: Optional[str] = None) -> None:
        """ Constructor

        Arguments:
        dsn           -- DSN to parse. Must contain schema, host and database
                         name
        password_file -- File with password. If specified - used as password
                         source (regardless of password in 'dsn' and
                         'password')
        password      -- Password. If specified and 'password file is
                         unspecified - used as password source regardless of
                         password in 'dsn'
        """
        parsed_dsn = urllib.parse.urlparse(dsn)

        self.host = parsed_dsn.hostname
        error_if(not self.host, f"DSN '{safe_dsn(dsn)}' missing hostname")

        self.db = (parsed_dsn.path or "").lstrip("/")
        error_if(not self.db, f"DSN '{safe_dsn(dsn)}' missing database name")

        self.port = parsed_dsn.port or DEFAULT_POSTGRES_PORT
        self.user = parsed_dsn.username or DEFAULT_POSTGRES_USER

        if password_file:
            error_if(not os.path.isfile(password_file),
                     f"Password file '{password_file}' not found")
            try:
                with open(password_file, encoding="ascii") as f:
                    self.password = f.read()
            except OSError as ex:
                error(f"Error reading password file '{password_file}': {ex}")
        elif password:
            self.password = password
        else:
            self.password = parsed_dsn.password or DEFAULT_POSTGRES_PASSWORD

        self.dsn = cast(str,
                        substitute_password(dsn=dsn, password=self.password))


def dsn_connectable(dsn: str, fail_if_not: bool = False) -> bool:
    """ True if DSN may be connected to """
    try:
        engine: Optional[sa.Engine] = None
        engine = sa.create_engine(dsn)
        with engine.connect():
            return True
    except sa.exc.SQLAlchemyError as ex:
        error_if(fail_if_not, f"Can't connect to '{safe_dsn(dsn)}': {ex}")
        return False
    finally:
        if engine:
            engine.dispose()


def to_bool(v: Optional[Union[str, bool, int]]) -> bool:
    """ Convert to bool value that, possibly, came from REST API query """
    if isinstance(v, (bool, int)) or (v is None):
        return bool(v)
    if isinstance(v, str):
        if v.lower() in ("true", "yes", "1"):
            return True
        if v.lower() in ("", "false", "no", "0"):
            return False
    error(f"Not a recognizable representation of boolean value: '{v}'")


def ensure_dsn(dsn: str, password_file: Optional[str] = None,
               password: Optional[str] = None,
               recreate: Optional[Union[str, bool, int]] = None,
               owner: Optional[Union[str, bool, int]] = True,
               local: bool = False) -> Tuple[str, bool]:
    """ Ensures that database and its user created

    Arguments:
    dsn          -- DSN to be ensured - possibly with absent/nominal password
    pasword_file -- Optional name of file with password for DSN
    password     -- Optional password for dsn
    recreate     -- True to recreate database if it exists (note that users are
                    never recreated)
    owner        -- False means that user in 'dsn' is not the database owner.
                    Hence if database does not exist, this function fails
    local        -- True to create database/user creation locally, false to
                    request remote DbCreator REST API
    Returns passed (dsn, existed) tuple (where 'dsn' is value of 'dsn'
    parameter, 'existed' is True if database and user already existed. Raises
    RuntimeError in case of failure
    """
    recreate = to_bool(recreate)
    owner = to_bool(owner)
    error_if(recreate and not owner,
             f"Database '{safe_dsn(dsn)}' recreation may only be done by "
             f"prospective owner")
    desired_dsn_info = DsnInfo(dsn=dsn, password_file=password_file,
                               password=password)
    password = desired_dsn_info.password
    # Request parameters made straight from (some) function parameters - poor
    # man's RPC
    req_params = \
        {arg_name: str(arg_value) for arg_name, arg_value in locals().items()
         if (arg_value is not None) and
         (arg_name not in ("password_file", "local", "desired_dsn_info"))}

    # Maybe DSN is already available?
    if dsn_connectable(desired_dsn_info.dsn):
        return (dsn, True)

    # So, something (user and/or database) needs to be created
    if not local:
        # Creation through REST API
        db_creator_url = os.environ.get(DB_CREATOR_URL_ENV)
        error_if(not db_creator_url,
                 f"DB Creator REST API URL environment variable "
                 f"'{DB_CREATOR_URL_ENV}' not specified")
        assert db_creator_url is not None
        try:
            requests.post(db_creator_url, params=req_params, timeout=30)
        except requests.exceptions.RequestException as ex:
            error(f"Unable to create database '{safe_dsn(dsn)}': {ex}")
        return (dsn, False)

    # Creation itself (REST API implementation)
    try:
        creator_engine: Optional[sa.engine.Engine] = None

        # First find creator DSN and password
        # Looking for creator ('postgres user') DSN matching requested server
        for env in os.environ:
            if not env.startswith(DB_CREATOR_DSN_ENV_PREFIX):
                continue
            creator_dsn = os.environ[env]
            if not creator_dsn:
                continue
            creator_dsn_info = \
                DsnInfo(
                    dsn=creator_dsn,
                    password_file=os.environ.get(
                        DB_CREATOR_PASSWORD_FILE_ENV_PREFIX +
                        env[len(DB_CREATOR_DSN_ENV_PREFIX):]))
            if (creator_dsn_info.host, creator_dsn_info.port) == \
                    (desired_dsn_info.host, desired_dsn_info.port):
                break  # Found
        else:
            # Not found
            error(f"DB Creator DSN for "
                  f"'{desired_dsn_info.host}:{desired_dsn_info.port}' not "
                  f"found among {DB_CREATOR_DSN_ENV_PREFIX}* environment "
                  f"variables")

        creator_engine = sa.create_engine(creator_dsn_info.dsn)
        same_user = creator_dsn_info.user == desired_dsn_info.user

        # Creating user if it does not exist
        try:
            with creator_engine.connect() as conn:
                rp = conn.execute(
                    sa.text(f"SELECT 1 FROM pg_roles "
                            f"WHERE rolname = '{desired_dsn_info.user}'"))
                if not rp.fetchall():
                    conn.execute(
                        sa.text(f'CREATE USER "{desired_dsn_info.user}" '
                                f'WITH PASSWORD :password LOGIN'),
                        password=desired_dsn_info.password)
        except sa.exc.SQLAlchemyError as ex:
            error(f"Error creating user '{desired_dsn_info.user}': {ex}")

        # (Re)creating database
        try:
            with creator_engine.connect() as conn:
                if recreate:
                    # Dropping existing database if recreate requested
                    conn.execute(sa.text("COMMIT"))
                    conn.execute(sa.text(f'DROP DATABASE IF EXISTS '
                                         f'"{desired_dsn_info.db}"'))
                    db_exists = False
                else:
                    # Checking if database exists
                    rp = conn.execute(
                        sa.text(f"SELECT 1 FROM pg_database "
                                f"WHERE datname = '{desired_dsn_info.db}'"))
                    db_exists = bool(rp.fetchall())
            if not db_exists:
                error_if(not owner,
                         f"Database '{safe_dsn(dsn)}' may only be created by "
                         f"owner")
                # Creating database if needed
                with creator_engine.connect() as conn:
                    owner_clause = f' WITH OWNER "{desired_dsn_info.user}"' \
                        if not same_user else ""
                    conn.execute(sa.text("COMMIT"))
                    conn.execute(
                        sa.text(f'CREATE DATABASE "{desired_dsn_info.db}"'
                                f'{owner_clause}'))
            if (not owner) and (not same_user) and user_created:
                conn.execute(
                    sa.text(f'GRANT ALL PRIVILEGES ON DATABASE '
                            f'"{desired_dsn_info.db}" TO '
                            f'"{desired_dsn_info.user}"'))
        except sa.exc.SQLAlchemyError as ex:
            error(f"Failed to {'re' if recreate else ''}create "
                  f"database '{desired_dsn_info.db}': {ex}")

        # Final check
        dsn_connectable(desired_dsn_info.dsn, fail_if_not=True)
        return (dsn, False)
    finally:
        if creator_engine:
            creator_engine.dispose()
