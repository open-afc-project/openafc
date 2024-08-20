""" Logging-related utilities """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=broad-exception-caught, unnecessary-pass, global-statement

import inspect
import logging
import os
import sys
import traceback
from typing import Any, Dict, Callable, List, NoReturn, Optional, Type
import urllib.parse


__all__ = ["dp", "error", "error_if", "FailOnError", "get_module_logger",
           "include_stack_to_error_log", "safe_dsn", "set_dp_printer",
           "set_error_exception", "set_parent_logger"]

# Exception type to raise on error()/error_if()
_error_exception_type: type[BaseException] = SystemExit

# True to include stack to log messages, created by error()/error_if()
_error_include_stack: bool = False

# How to print debug messages
_dp_printer: Callable[[str], None] = lambda s: print(s, file=sys.stderr)


def dp(*args, to_str: Optional[Callable[[Any], str]] = None) -> None:
    """Print debug message

    Arguments:
    args   -- One or more items to print
    to_str -- Function for conversion values to strings
    """
    values: List[str] = []
    for arg in args:
        if to_str is not None:
            values.append(to_str(arg))
        elif "__repr__" in getattr(type(arg), "__dict__", {}):
            values.append(repr(arg))
        elif "__str__" in getattr(type(arg), "__dict__", {}):
            values.append(str(arg))
        elif hasattr(arg, "__dict__"):
            values.append(str(arg.__dict__))
        else:
            values.append(str(arg))
    cur_frame = inspect.currentframe()
    assert (cur_frame is not None) and (cur_frame.f_back is not None)
    caller_frameinfo = inspect.getframeinfo(cur_frame.f_back)
    _dp_printer(
        f"DP {caller_frameinfo.function}()@{caller_frameinfo.lineno}: "
        f"{', '.join(values)}")


def set_dp_printer(dp_printer: Callable[[str], None]) -> Callable[[str], None]:
    """ Sets new dp() printing method, returns previous one """
    global _dp_printer
    ret = _dp_printer
    _dp_printer = dp_printer
    return ret


class LoggerProxy:
    """ Delayer of logger instantaition until the first use

    Allows to apply program/component parameters before instantiation

    Private attributes:
    _name   -- Logger name to be used at creation
    _logger -- None or actual logger object

    Private class attributes:
    _loggers       -- Dictionary of created loggers, ordered by name
    _parent_logger -- Optional name of parent logger
    """
    _loggers: Dict[str, "LoggerProxy"] = {}
    _parent_logger = ""

    def __init__(self, name: str) -> None:
        """ Constructor

        Arguments:
        name -- Logger name
        """
        self._name = name
        self._logger: Optional[logging.Logger] = None

    def __getattr__(self, attr: str) -> Any:
        """ Instantiates logger on first use and delegates to it """
        if self._logger is None:
            self._logger = logging.getLogger(self._name)
        assert self._logger is not None
        return getattr(self._logger, attr)

    @classmethod
    def set_parent_logger(cls, parent_logger: str) -> None:
        """ Sets parent logger name """
        cls._parent_logger = parent_logger

    @classmethod
    def get_module_logger(cls, caller_level: int) -> "LoggerProxy":
        """ Returns logger object, named after caller module

        Arguments:
        caller_level -- How far up on stack is the caller (1 - immediate
                        caller, etc.)
        Returns logging.Logger object
        """
        caller_frame = inspect.stack()[caller_level]
        caller_module = \
            os.path.splitext(os.path.basename(caller_frame.filename))[0]
        logger_name = \
            ".".join(n for n in [cls._parent_logger, caller_module] if n)
        if logger_name not in cls._loggers:
            cls._loggers[logger_name] = cls(logger_name)
        return cls._loggers[logger_name]


def set_parent_logger(parent_logger: str) -> None:
    """ Sets parent logger name """
    LoggerProxy.set_parent_logger(parent_logger)


def get_module_logger(caller_level: int = 1) -> LoggerProxy:
    """ Returns logger object, named after caller module

    Arguments:
    caller_level -- How far up on stack is the caller (1 - immediate caller,
                    etc.)
    Returns logging.Logger object
    """
    return LoggerProxy.get_module_logger(caller_level=caller_level + 1)


def set_error_exception(exc_type: Type[BaseException]) -> Type[BaseException]:
    """ Set Exception to raise on error()/error_if(). Returns previous one """
    global _error_exception_type
    assert _error_exception_type is not None
    ret = _error_exception_type
    _error_exception_type = exc_type
    return ret


def include_stack_to_error_log(state: bool) -> bool:
    """ Sets if stack trace should be included into log message, generated by
    error()/error_if(). Returns previous setting
    """
    global _error_include_stack
    ret = _error_include_stack
    _error_include_stack = state
    return ret


def error(msg: str, caller_level: int = 1) -> NoReturn:
    """ Generates error exception and write it to log

    Arguments:
    msg          -- Message to put to exception and to include into log
    caller_level -- How fur up stack is the caller (1 - immediate caller).
                    Used to retrieve logger for caller's module
    """
    stack_trace = f"Most recent call last:\n" \
        f"{''.join(traceback.format_stack()[:-caller_level])}\n" \
        if _error_include_stack else ""
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
