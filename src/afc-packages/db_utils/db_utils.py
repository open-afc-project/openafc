#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
""" Assorted DSN-related utilities """

# pylint: disable=too-many-arguments, invalid-name

import os
import shlex
import subprocess
from typing import Any, cast, Dict, List, NoReturn, Optional
import urllib.parse

__all__ = ["substitute_password", "safe_dsn", "alembic_ensure_version"]

# Entry names in credential secret file
CREDENTIAL_USERNAME_FIELD = "username"
CREDENTIAL_PASSWORD_FIELD = "password"


def error(msg: str) -> NoReturn:
    """ Raises error with given message """
    raise RuntimeError(msg)


def error_if(pred: Any, msg: str) -> None:
    """ If given predicate is met - raises error with given message """
    if pred:
        error(msg)


def substitute_password(dsn: Optional[str] = None,
                        password: Optional[str] = None,
                        password_file: Optional[str] = None,
                        optional: bool = False) -> Optional[str]:
    """ Substitutes password into given DSN

    Arguments:
    dsn           -- DSN as string
    password      -- Optional password
    password_file -- Optional name of file with password. Ignored if `password`
                     specified
    optional      -- True if it is DSN may be None/empty
    Returns resulting DSN (None if it is optional and absent)
    """
    # What if DSN is unspecified/empty
    if not dsn:
        error_if(not optional, "DSN not specified")
        return dsn

    # Obtaining password
    if (password is None) and password_file:
        error_if(not os.path.isfile(password_file),
                 f"Password file '{password_file}' not found")
        with open(password_file, encoding="ascii") as f:
            password = f.read()
    if password is None:
        return dsn

    # Substituting password
    dsn_parts = urllib.parse.urlparse(dsn)
    netloc = ""
    replacement: Optional[str]
    for partname, separator, replacement in [("username", "", None),
                                             ("password", ":", password),
                                             ("hostname", "@", None),
                                             ("port", ":", None)]:
        part: Optional[str] = replacement or getattr(dsn_parts, partname)
        if part:
            netloc += f"{separator}{part}"
    return dsn_parts._replace(netloc=netloc).geturl()


def safe_dsn(dsn: Optional[str]) -> Optional[str]:
    """ Returns DSN without password (if there was any) """
    if not dsn:
        return dsn
    try:
        parsed = urllib.parse.urlparse(dsn)
        if not parsed.password:
            return dsn
        return \
            urllib.parse.urlunparse(
                parsed._replace(
                    netloc=parsed.netloc.replace(":" + parsed.password,
                                                 ":<PASSWORD>")))
    except Exception:
        return dsn


def alembic_ensure_version(
        alembic_config: Optional[str], existing_database: bool,
        initial_version: Optional[str], head_version: Optional[str],
        env: Optional[Dict[str, str]] = None) \
        -> Optional[str]:
    """ Ensures that database is of head alembic version

    Arguments:
    alembic_config    -- Alembic config file. None to use default
    existing_database -- True if database already exists (and should be
                         upgraded to head version), False if it newly created
                         (and should be stamped by head version)
    initial_version   -- Version to stamp existing database with (vefore
                         upgrade) if it has none. Must present if database
                         exists and not stamped, otherwise can be None
    head_version      -- Head version. None to use default head version
    env               -- None or dictionary with additional environment \
                         variables
    Returns None on success, error message on fail
    """
    if env:
        env = {**os.environ, **env}
    alembic_args = ["alembic"]
    if alembic_config:
        if not os.path.isfile(alembic_config):
            return f"Alembic config file'{alembic_config}' not found"
        alembic_args += ["-c", alembic_config]

    try:
        def alembic(args: List[str]) -> str:
            """ Executes alembic command with given subcommand name and
            arguments
            Returns stdout
            """
            print(" ".join(shlex.quote(arg) for arg in (alembic_args + args)))
            try:
                cp = \
                    subprocess.run(alembic_args + args, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, encoding="utf-8",
                                   errors="ignore", env=env, check=False)
                if cp.stdout:
                    print(cp.stdout)
                if cp.stderr:
                    print(cp.stderr)
                if cp.returncode:
                    raise RuntimeError(f"Alembic execution failed with code "
                                       f"{cp.returncode}: {cp.stderr}")
                return cast(str, cp.stdout)
            except OSError as ex:
                raise RuntimeError(f"Unable to run alembic: {ex}")

        if existing_database:
            current_version = alembic(["current"]).strip()
            if not current_version:
                if not initial_version:
                    return "Initial alembic version must be provided"
                alembic(["stamp", initial_version])
            alembic(["upgrade", head_version or "head"])
        else:
            alembic(["stamp", head_version or "head"])
        return None
    except RuntimeError as ex:
        return str(ex)
