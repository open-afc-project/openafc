#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
""" Utilities to deal with secrets """

# pylint: disable=too-many-arguments, invalid-name

import os
from typing import Any, NoReturn, Optional
import urllib.parse

__all__ = ["substitute_password"]

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


def substitute_password(dsc: str, dsn: Optional[str] = None,
                        password: Optional[str] = None,
                        password_file: Optional[str] = None,
                        optional: bool = False) -> Optional[str]:
    """ Substitutes password into given DSN

    Arguments:
    dsc           -- DSN description for error messages
    dsn           -- DSN as string
    password      -- Optional password
    password_file -- Optional name of file with password. Ignored if `password`
                     specified
    optional      -- True if it is DSN may be None/empty
    Returns resulting DSN (None if it is optional and absent)
    """
    # What if DSN is unspecified/empty
    if not dsn:
        error_if(not optional, f"DSN for {dsc} not specified")
        return dsn

    # Obtaining password
    if (password is None) and password_file:
        error_if(not os.path.isfile(password_file),
                 f"Password file '{password_file}' for {dsc} not found")
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
