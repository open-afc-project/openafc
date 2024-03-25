""" Common stuff for ULS Service related scripts """

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=wrong-import-order, invalid-name,
# pylint: disable=logging-fstring-interpolation, global-statement

import __main__
import inspect
import logging
import os
import pydantic
import sys
from typing import Any, Dict, List, Optional

# Default directory for status filers
DEFAULT_STATUS_DIR = os.path.join(os.path.dirname(__file__), "status")

# Exit code to use in error()
error_exit_code = 1

# Logging level to use in error()
error_log_level = logging.CRITICAL


def set_error_exit_params(exit_code: Optional[int] = None,
                          log_level: Optional[int] = None) -> None:
    """ Sets what to do in error

    Arguments:
    exit_code -- None or what exit code to use
    log_level -- None or log level to use
    """
    global error_exit_code, error_log_level
    if exit_code is not None:
        error_exit_code = exit_code
    if log_level is not None:
        error_log_level = log_level


def error(msg: str) -> None:
    """ Prints given msg as error message and exit abnormally """
    logging.log(error_log_level, msg)
    sys.exit(error_exit_code)


def error_if(cond: Any, msg: str) -> None:
    """ If condition evaluates to true prints given msg as error message and
    exits abnormally """
    if cond:
        error(msg)


def dp(*args, **kwargs) -> None:
    """Print debug message

    Arguments:
    args   -- Format and positional arguments. If latter present - formatted
              with %
    kwargs -- Keyword arguments. If present formatted with format()
    """
    msg = args[0] if args else ""
    if len(args) > 1:
        msg = msg % args[1:]
    if args and kwargs:
        msg = msg.format(**kwargs)
    cur_frame = inspect.currentframe()
    assert (cur_frame is not None) and (cur_frame.f_back is not None)
    frameinfo = inspect.getframeinfo(cur_frame.f_back)
    logging.info(f"DP {frameinfo.function}()@{frameinfo.lineno}: {msg}")


def setup_logging(verbose: bool) -> None:
    """ Logging setup

    Arguments:
    verbose -- enable debug printout
    """
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(
        logging.Formatter(
            f"{os.path.basename(__main__.__file__)}. "
            f"%(levelname)s: %(asctime)s %(message)s"))
    logging.getLogger().addHandler(console_handler)
    logging.getLogger().setLevel(
        logging.DEBUG if verbose else logging.INFO)
    if verbose:
        logging.getLogger('sqlalchemy.engine').setLevel(logging.INFO)


def env_help(settings_class: Any, arg: str, prefix: str = ". ") -> str:
    """ Prints help on environment variable for given command line argument
    (aka setting name).

    Environment variable name must be explicitly defined in Field() with 'env='

    Arguments:
    settings_class -- Type, derived from pydantic.BaseSettings
    arg            -- Command line argument
    prefix         -- Prefix to use if result is nonempty
    Returns fragment for help message
    """
    props = settings_class.schema()["properties"].get(arg)
    error_if(props is None,
             f"Command line argument '--{arg}' not found in settings class "
             f"{settings_class.schema()['title']}")
    assert props is not None
    ret: List[str] = []
    default = props.get("default")
    if default is not None:
        ret.append(f"Default is '{default}'")
    if "env" in props:
        ret.append(f"May be set with '{props['env']}' environment variable")
        value = os.environ.get(props["env"])
        if value is not None:
            ret[-1] += f" (which is currently '{value}')"
    if "default" not in props:
        ret.append("This parameter is mandatory")
    return (prefix + ". ".join(ret)) if ret else ""


def merge_args(settings_class: Any, args: Any) -> pydantic.BaseSettings:
    """ Merges settings from command line arguments and Pydantic settings

    Arguments:
    settings_class -- Type, derived from pydantic.BaseSettings
    args           -- Arguments, parsed by ArgumentParser
    Returns Object of type derived from pydantic.BaseSettings
    """
    kwargs: Dict[str, Any] = \
        {k: getattr(args, k) for k in settings_class.schema()["properties"]
         if getattr(args, k) not in (None, False)}
    return settings_class(**kwargs)
