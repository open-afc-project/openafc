""" Common functions of this component's scripts """

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=wrong-import-order, too-many-arguments

import os
from typing import Optional
import urllib.parse

import secret_utils

# Connection string scheme that should be ultimately used
ACTUAL_SCHEME = "postgresql+psycopg2"


def dsn(*, name_for_logs: str, arg_dsn: Optional[str] = None,
        dsn_env: Optional[str] = None, default_dsn: Optional[str] = None,
        password_file: Optional[str] = None,
        password_file_env: Optional[str] = None,
        default_db: Optional[str] = None) -> str:
    """ Creates database DSN from bits and pieces scattered around

    Arguments:
    name_for_logs     -- Database name to use in error messages
    arg_dsn           -- DSN from command line or None
    dsn_env           -- Name of environment variable that may contain DSN
    default_dsn       -- Default DSN to take missing pieces from
    password_file     -- Password file name from command line or None
    password_file_env -- Name of environment variable that may contain
                         password file name
    default_db        -- Default database name
    Returns created DSN or generatews ValueError exception
    """
    default_dsn = default_dsn or \
        f"postgresql://postgres:postgres@__REQUIRED__:5432/" \
        f"{default_db or '__REQUIRED__'}"
    default_parts = urllib.parse.urlparse(default_dsn)
    arg_dsn = arg_dsn or (os.environ.get(dsn_env) if dsn_env else None)
    if arg_dsn is None:
        arg_dsn = default_dsn
    else:
        if "://" not in arg_dsn:
            arg_dsn = f"{default_parts.scheme}://{arg_dsn}"
    assert arg_dsn is not None

    arg_parts = urllib.parse.urlparse(arg_dsn)
    netloc = ""
    for part_name, separator in [("username", ""), ("password", ":"),
                                 ("hostname", "@"), ("port", ":")]:
        part = getattr(arg_parts, part_name) or \
            getattr(default_parts, part_name)
        netloc += f"{separator}{part}"

    replacements = {"scheme": ACTUAL_SCHEME, "netloc": netloc}
    replacements.update(
        {field: getattr(default_parts, field)
         for field in default_parts._fields
         if (not getattr(arg_parts, field)) and
         getattr(default_parts, field) and (field not in replacements)})
    arg_parts = arg_parts._replace(**replacements)
    if getattr(arg_parts, "hostname") == "__REQUIRED__":
        raise \
            ValueError(f"DB server hostname not specified for {name_for_logs}")
    if getattr(arg_parts, "path") == "__REQUIRED__":
        raise ValueError(f"Database name not specified for {name_for_logs}")
    return \
        secret_utils.substitute_password(
            dsc=name_for_logs, dsn=arg_parts.geturl(),
            password_file=password_file or
            (os.environ.get(password_file_env) if password_file_env else None))
