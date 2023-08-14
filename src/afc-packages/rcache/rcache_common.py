""" AFC Request cache common utility stuff """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=unnecessary-pass, logging-fstring-interpolation
# pylint: disable=global-statement

import inspect
import logging
import os
import sys
import traceback
from typing import Any, Callable, Optional, Type

__all__ = ["dp", "error", "error_if", "FailOnError", "get_module_logger",
           "include_stack_to_error_log", "set_dp_printer",
           "set_error_exception"]

# Exception type to raise on error()/error_if()
_error_exception_type: type[BaseException] = SystemExit

# True to include stack to log messages, created by error()/error_if()
_include_stack_to_error_log: bool = False

# How to print debug messages
_dp_printer: Callable[[str], None] = lambda s: print(s, file=sys.stderr)


def dp(*args, **kwargs) -> None:  # pylint: disable=invalid-name
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
    _dp_printer(f"DP {frameinfo.function}()@{frameinfo.lineno}: {msg}")


def get_module_logger(caller_level: int = 1) -> logging.Logger:
    """ Returns logger object, named after caller module

    Arguments:
    caller_level -- How far up on stack is the caller (1 - immediate caller,
                    etc.)
    Returns logging.Logger object
    """
    caller_frame = inspect.stack()[caller_level]
    caller_module = \
        os.path.splitext(os.path.basename(caller_frame.filename))[0]
    return logging.getLogger(caller_module)


def set_error_exception(exc_type: Type[BaseException]) -> Type[BaseException]:
    """ Set Exception to raise on error()/error_if(). Returns previous one """
    global _error_exception_type
    assert _error_exception_type is not None
    ret = _error_exception_type
    _error_exception_type = exc_type
    return ret


def set_dp_printer(dp_printer: Callable[[str], None]) -> Callable[[str], None]:
    """ Sets new dp() printing method, returns previous one """
    global _dp_printer
    ret = _dp_printer
    _dp_printer = dp_printer
    return ret


def include_stack_to_error_log(state: bool) -> bool:
    """ Sets if stack trace should be included into log message, generated by
    error()/error_if(). Returns previous setting
    """
    global _include_stack_to_error_log
    ret = _include_stack_to_error_log
    _include_stack_to_error_log = state
    return ret


def error(msg: str, caller_level: int = 1) -> None:
    """ Generates error exception and write it to log

    Arguments:
    msg          -- Message to put to exception and to include into log
    caller_level -- How fur up stack is the caller (1 - immediate caller).
                    Used to retrieve logger for caller's module
    """
    stack_trace = f"Most recent call last:\n" \
        f"{''.join(traceback.format_stack()[:-caller_level])}\n" \
        if _include_stack_to_error_log else ""
    get_module_logger(caller_level + 1).error(f"{stack_trace}{msg}")
    raise _error_exception_type(msg)


def error_if(cond: Any, msg: str) -> None:
    """ If given condition met - raise and write to log an error with given
    message """
    if cond:
        error(msg=msg, caller_level=2)


class FailOnError:
    """ Context that conditionally intercepts exceptions, raised by
    error()/error_if()

    Private attributes:
    _prev_error_exception_type -- Previously used error exception type if
                                  errors are intercepted, None if not
                                  intercepted
    """
    class _IntermediateErrorException(Exception):
        """ Exception used for interception """
        pass

    def __init__(self, fail_on_error: bool) -> None:
        """ Constructor

        fail_on_error -- False to intercept error exception
        """
        self._prev_error_exception_type: Optional[Type[BaseException]] = \
            None if fail_on_error \
            else set_error_exception(FailOnError._IntermediateErrorException)

    def __enter__(self) -> None:
        """ Context entry """
        pass

    def __exit__(self, exc_type: Any, exc_value: Any, exc_tb: Any) -> bool:
        """ Context exit

        Arguments:
        exc_type  -- Type of exception that caused context leave, None for
                     normal leave
        exc_value -- Value of exception that caused context leave. None for
                     normal leave
        exc_tb    -- Traceback of exception that caused context leave, None for
                     normal leave
        Returns True if exception was processed, False if it should be
        propagated
        """
        if (self._prev_error_exception_type is not None) and \
                (exc_type == FailOnError._IntermediateErrorException):
            set_error_exception(self._prev_error_exception_type)
            return True
        return False
