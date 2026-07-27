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
from psycopg2 import sql
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

# DSN scheme for Postgres
POSTGRES_SCHEME = "postgresql"


class DsnInfo:
    """ Parsed PostgreSQL DSN

    Attributes:
    host     -- Hostname
    port     -- Port. Default is DEFAULT_POSTGRES_PORT
    user     -- Username. Default is DEFAULT_POSTGRES_USER
    password -- Password. Default is DEFAULT_POSTGRES_PASSWORD
    password_defaulted -- True if no password file/argument/inline password
                was given and 'password' fell back to
                DEFAULT_POSTGRES_PASSWORD
    db       -- Database name
    dsn      -- DSN with password and schema properly substituted
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
        parsed_dsn = urllib.parse.urlparse(str(dsn))

        error_if(parsed_dsn.query,
                 f"DSN '{safe_dsn(dsn)}' contains query parameters; "
                 f"libpq host/hostaddr/port overrides are not permitted")

        self.host = parsed_dsn.hostname
        error_if(not self.host, f"DSN '{safe_dsn(dsn)}' missing hostname")

        self.db = (parsed_dsn.path or "").lstrip("/")
        error_if(not self.db, f"DSN '{safe_dsn(dsn)}' missing database name")

        self.port = parsed_dsn.port or DEFAULT_POSTGRES_PORT
        self.user = parsed_dsn.username or DEFAULT_POSTGRES_USER

        self.password_defaulted = False
        if password_file:
            error_if(not os.path.isfile(password_file),
                     f"Password file '{password_file}' not found")
            try:
                with open(password_file, encoding="ascii") as f:
                    self.password = f.read().strip()
            except OSError as ex:
                error(f"Error reading password file '{password_file}': {ex}")
        elif password:
            self.password = password
        else:
            # Recorded here (rather than raising immediately) because a
            # host/port-only DsnInfo parse without any provisioning intent
            # is legitimate (e.g. the allowlist-membership parses below);
            # callers that are about to CREATE USER/provision with this
            # password must check password_defaulted and fail closed
            # instead of silently minting a role with the well-known
            # default credential.
            self.password_defaulted = not parsed_dsn.password
            self.password = parsed_dsn.password or DEFAULT_POSTGRES_PASSWORD
        if parsed_dsn.scheme != POSTGRES_SCHEME:
            dsn = parsed_dsn._replace(scheme=POSTGRES_SCHEME).geturl()
        self.dsn = cast(str,
                        substitute_password(dsn=dsn, password=self.password))
        reparsed = urllib.parse.urlparse(self.dsn)
        error_if(
            (reparsed.hostname, reparsed.port or DEFAULT_POSTGRES_PORT) !=
            (self.host, self.port),
            f"DSN '{safe_dsn(self.dsn)}' host/port changed after password "
            f"substitution")


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
               local: bool = False,
               grant_readonly_role: Optional[str] = None) -> Tuple[str, bool]:
    """ Ensures that database and its user created

    Arguments:
    dsn                -- DSN to be ensured - possibly with absent/nominal password
    pasword_file       -- Optional name of file with password for DSN
    password           -- Optional password for dsn
    recreate           -- True to recreate database if it exists (note that users are
                          never recreated)
    owner              -- False means that user in 'dsn' is not the database owner.
                          Hence if database does not exist, this function fails
    local              -- True to create database/user creation locally, false to
                          request remote DbCreator REST API
    grant_readonly_role -- Optional PostgreSQL role name that should receive
                          CONNECT + SELECT privileges on the created database
                          (used to grant read-only role access on
                          databases that read-only datasources need to query)
    Returns passed (dsn, existed) tuple (where 'dsn' is value of 'dsn'
    parameter, 'existed' is True if database and user already existed. Raises
    RuntimeError in case of failure
    """
    recreate = to_bool(recreate)
    owner = to_bool(owner)
    dsn = str(dsn)
    error_if(recreate and not owner,
             f"Database '{safe_dsn(dsn)}' recreation may only be done by "
             f"prospective owner")
    desired_dsn_info = DsnInfo(dsn=dsn, password_file=password_file,
                               password=password)
    error_if(desired_dsn_info.password_defaulted,
             f"DSN '{safe_dsn(dsn)}' has no password and no password "
             f"file/argument was provided; refusing to fall back to the "
             f"well-known default password")
    for ident in (desired_dsn_info.user, desired_dsn_info.db):
        error_if((not ident) or
                 (not all(c.isalnum() or (c in "_-") for c in str(ident))),
                 f"Invalid user/database identifier in DSN '{safe_dsn(dsn)}'")
    password = desired_dsn_info.password
    # Request parameters made straight from (some) function parameters - poor
    # man's RPC
    req_params = \
        {arg_name: str(arg_value) for arg_name, arg_value in locals().items()
         if (arg_value is not None) and
         (arg_name not in ("password_file", "local", "desired_dsn_info",
                           "password"))}  # password sent in body, not query
    # The dsn arg may embed a password inline (e.g. postgresql://user:secret@host/db).
    # The receiving side obtains the password from the JSON body, so forward only the
    # password-stripped version in the query string.
    if "dsn" in req_params:
        req_params["dsn"] = safe_dsn(dsn)

    # Validate host:port against allowlist
    allowed = False
    for env in os.environ:
        if env.startswith(DB_CREATOR_DSN_ENV_PREFIX):
            creator_dsn = os.environ[env]
            if creator_dsn:
                creator_dsn_info = DsnInfo(dsn=creator_dsn)
                if (creator_dsn_info.host, creator_dsn_info.port) == (desired_dsn_info.host, desired_dsn_info.port):
                    allowed = True
                    break
    error_if(not allowed, f"DSN host:port '{desired_dsn_info.host}:{desired_dsn_info.port}' not in allowlist")

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
        # The request body carries the database-owner password and the
        # request headers carry AFC_INTERNAL_TOKEN; both are reusable
        # credentials. Anchor the transport-security decision here: shipped
        # defaults are plain http:// on the internal compose bridge, so
        # this is opt-out (AFC_DB_CREATOR_ALLOW_INSECURE_HTTP defaults to
        # true) rather than opt-in, but it gives operators an explicit knob
        # to require TLS on this seam.
        if urllib.parse.urlparse(db_creator_url).scheme != "https":
            error_if(
                not to_bool(os.environ.get(
                    "AFC_DB_CREATOR_ALLOW_INSECURE_HTTP", "true")),
                f"DB Creator REST API URL '{DB_CREATOR_URL_ENV}'="
                f"'{db_creator_url}' does not use https:// and "
                f"AFC_DB_CREATOR_ALLOW_INSECURE_HTTP is not set; refusing "
                f"to POST a database-owner password/internal token in "
                f"cleartext")
        _internal_token = os.environ.get("AFC_INTERNAL_TOKEN")
        if not _internal_token:
            _token_file = os.environ.get("AFC_INTERNAL_TOKEN_FILE")
            if _token_file:
                try:
                    with open(_token_file) as fh:
                        _internal_token = fh.read().strip() or None
                except OSError:
                    pass
        _headers = ({"x-afc-internal-token": _internal_token}
                    if _internal_token else {})
        try:
            resp = requests.post(db_creator_url, params=req_params,
                                 headers=_headers,
                                 json={"password": password} if password else None,
                                 timeout=30)
            if not resp.ok:
                error(f"Unable to create database '{safe_dsn(dsn)}': "
                      f"HTTP {resp.status_code} from {db_creator_url}: "
                      f"{resp.text[:200]}")
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

        error_if(creator_dsn_info.password_defaulted,
                 f"DB Creator DSN '{env}' has no password and no matching "
                 f"'{DB_CREATOR_PASSWORD_FILE_ENV_PREFIX}*' password file "
                 f"is set; refusing to fall back to the well-known default "
                 f"password")
        creator_engine = sa.create_engine(creator_dsn_info.dsn)
        same_user = creator_dsn_info.user == desired_dsn_info.user

        # Creating user if it does not exist
        user_created = False
        try:
            with creator_engine.connect() as conn:
                raw_conn = conn.connection.driver_connection if hasattr(
                    conn.connection, 'driver_connection') else conn.connection
                query_role = sql.SQL("SELECT 1 FROM pg_roles WHERE rolname = {}").format(
                    sql.Literal(desired_dsn_info.user))
                rp = conn.execute(sa.text(query_role.as_string(raw_conn)))
                if not rp.fetchall():
                    query_create = sql.SQL("CREATE USER {} WITH PASSWORD {} LOGIN").format(
                        sql.Identifier(desired_dsn_info.user),
                        sql.Literal(desired_dsn_info.password)
                    )
                    conn.execute(sa.text(query_create.as_string(raw_conn)))
                    user_created = True
        except sa.exc.SQLAlchemyError as ex:
            error(f"Error creating user '{desired_dsn_info.user}': {ex}")

        # (Re)creating database
        try:
            with creator_engine.connect() as conn:
                raw_conn = conn.connection.driver_connection if hasattr(
                    conn.connection, 'driver_connection') else conn.connection
                if recreate:
                    # Dropping existing database if recreate requested
                    conn.execute(sa.text("COMMIT"))
                    query_drop = sql.SQL("DROP DATABASE IF EXISTS {}").format(
                        sql.Identifier(desired_dsn_info.db))
                    conn.execute(sa.text(query_drop.as_string(raw_conn)))
                    db_exists = False
                else:
                    # Checking if database exists
                    query_db = sql.SQL("SELECT 1 FROM pg_database WHERE datname = {}").format(
                        sql.Literal(desired_dsn_info.db))
                    rp = conn.execute(sa.text(query_db.as_string(raw_conn)))
                    db_exists = bool(rp.fetchall())
            if not db_exists:
                error_if(not owner,
                         f"Database '{safe_dsn(dsn)}' may only be created by "
                         f"owner")
                # Creating database if needed
                with creator_engine.connect() as conn:
                    raw_conn = conn.connection.driver_connection if hasattr(
                        conn.connection, 'driver_connection') else conn.connection
                    conn.execute(sa.text("COMMIT"))
                    if not same_user:
                        query_create_db = sql.SQL("CREATE DATABASE {} WITH OWNER {}").format(
                            sql.Identifier(desired_dsn_info.db),
                            sql.Identifier(desired_dsn_info.user)
                        )
                    else:
                        query_create_db = sql.SQL("CREATE DATABASE {}").format(
                            sql.Identifier(desired_dsn_info.db))
                    conn.execute(sa.text(query_create_db.as_string(raw_conn)))
            if (not owner) and (not same_user) and user_created:
                with creator_engine.connect() as conn:
                    raw_conn = conn.connection.driver_connection if hasattr(
                        conn.connection, 'driver_connection') else conn.connection
                    query_grant = sql.SQL("GRANT ALL PRIVILEGES ON DATABASE {} TO {}").format(
                        sql.Identifier(desired_dsn_info.db),
                        sql.Identifier(desired_dsn_info.user)
                    )
                    conn.execute(sa.text(query_grant.as_string(raw_conn)))
        except sa.exc.SQLAlchemyError as ex:
            error(f"Failed to {'re' if recreate else ''}create "
                  f"database '{desired_dsn_info.db}': {ex}")

        # Pre-create PostGIS extension on newly created database if supported
        if not db_exists:
            try:
                _creator_parts = urllib.parse.urlparse(creator_dsn_info.dsn)
                _db_dsn = _creator_parts._replace(
                    path=f"/{desired_dsn_info.db}").geturl()
                _db_engine = sa.create_engine(_db_dsn)
                try:
                    with _db_engine.connect() as conn:
                        conn.execute(sa.text("COMMIT"))
                        conn.execute(sa.text("CREATE EXTENSION IF NOT EXISTS postgis"))
                finally:
                    _db_engine.dispose()
            except Exception:
                pass

        # Grant CONNECT + SELECT to the designated read-only role
        # so read-only datasources can query the newly-created DB
        # without holding the application read/write credential.
        # Two separate connections are required:
        #   1. postgres-DB context: GRANT CONNECT ON DATABASE
        #   2. target-DB context: GRANT SELECT + ALTER DEFAULT PRIVILEGES
        if grant_readonly_role and not db_exists:
            try:
                # Validate role name identifier (same rules as user/db above)
                error_if(
                    not all(c.isalnum() or c in "_-"
                            for c in grant_readonly_role),
                    f"Invalid grant_readonly_role identifier: "
                    f"'{grant_readonly_role}'")
                with creator_engine.connect() as conn:
                    raw_conn = conn.connection.driver_connection \
                        if hasattr(conn.connection, 'driver_connection') \
                        else conn.connection
                    conn.execute(sa.text("COMMIT"))
                    q_connect = sql.SQL(
                        "GRANT CONNECT ON DATABASE {} TO {}").format(
                        sql.Identifier(desired_dsn_info.db),
                        sql.Identifier(grant_readonly_role))
                    conn.execute(sa.text(
                        q_connect.as_string(raw_conn)))
                # Connect to the specific database to issue schema-level grants
                _creator_parts = urllib.parse.urlparse(
                    creator_dsn_info.dsn)
                _db_dsn = _creator_parts._replace(
                    path=f"/{desired_dsn_info.db}").geturl()
                _db_engine = sa.create_engine(_db_dsn)
                try:
                    with _db_engine.connect() as conn:
                        raw_conn = conn.connection.driver_connection \
                            if hasattr(conn.connection, 'driver_connection') \
                            else conn.connection
                        conn.execute(sa.text("COMMIT"))
                        q_usage = sql.SQL(
                            "GRANT USAGE ON SCHEMA public TO {}").format(
                            sql.Identifier(grant_readonly_role))
                        conn.execute(sa.text(
                            q_usage.as_string(raw_conn)))
                        q_select = sql.SQL(
                            "GRANT SELECT ON ALL TABLES IN SCHEMA "
                            "public TO {}").format(
                            sql.Identifier(grant_readonly_role))
                        conn.execute(sa.text(
                            q_select.as_string(raw_conn)))
                        q_defpriv = sql.SQL(
                            "ALTER DEFAULT PRIVILEGES IN SCHEMA public "
                            "GRANT SELECT ON TABLES TO {}").format(
                            sql.Identifier(grant_readonly_role))
                        conn.execute(sa.text(
                            q_defpriv.as_string(raw_conn)))
                        q_seq = sql.SQL(
                            "GRANT SELECT ON ALL SEQUENCES IN SCHEMA "
                            "public TO {}").format(
                            sql.Identifier(grant_readonly_role))
                        conn.execute(sa.text(q_seq.as_string(raw_conn)))
                finally:
                    _db_engine.dispose()
            except sa.exc.SQLAlchemyError as ex:
                error(f"Failed to grant readonly privileges on "
                      f"'{desired_dsn_info.db}' to '{grant_readonly_role}': "
                      f"{ex}")

        # Final check
        dsn_connectable(desired_dsn_info.dsn, fail_if_not=True)
        return (dsn, False)
    finally:
        if creator_engine:
            creator_engine.dispose()
