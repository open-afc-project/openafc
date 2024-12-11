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
