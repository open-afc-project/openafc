""" Common helper functions for Pydantic """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

import os
import re
try:
    import pydantic
except ImportError:
    pass
try:
    from pydantic_settings import BaseSettings
except ImportError:
    pass
from typing import Any, Dict, List

# Field/env-var names matching this pattern (case-insensitive) carry
# credentials (DSNs may embed an inline password, tokens/keys/secrets are
# credentials themselves) and must never be echoed verbatim into --help
# output. This print-path is outside the logging-based
# _CredentialRedactFilter (appcfg.py), which only covers logging.Filter
# emission, so it needs its own redaction here.
_SENSITIVE_NAME_RE = re.compile(
    r"(dsn|password|secret|token|key)", re.IGNORECASE)


def env_help(settings_class: Any, arg: str, prefix: str = ". ") -> str:
    """ Prints help on environment variable for given command line argument
    (aka setting name).

    Environment variable name must be explicitly defined in Field() with
    validation_alias= (pydantic v2) or env= (pydantic v1)

    Arguments:
    settings_class -- Type, derived from pydantic.BaseSettings
    arg            -- Command line argument
    prefix         -- Prefix to use if result is nonempty
    Returns fragment for help message
    """
    field_info = settings_class.model_fields.get(arg)
    assert field_info is not None, \
        f"Command line argument '--{arg}' not found in settings class " \
        f"{settings_class.__name__}"
    ret: List[str] = []
    if field_info.default is not None:
        ret.append(f"Default is '{field_info.default}'")
    alias = field_info.validation_alias
    if alias:
        env_name = alias if isinstance(alias, str) else str(alias)
        ret.append(f"May be set with '{env_name}' environment variable")
        value = os.environ.get(env_name)
        if value is not None:
            if _SENSITIVE_NAME_RE.search(env_name) or \
                    _SENSITIVE_NAME_RE.search(arg):
                # Credential-bearing setting: never echo the raw value,
                # even redacted, to avoid leaking DSN passwords/tokens/keys
                # into --help output (and any retained CI/container logs of
                # that invocation).
                ret[-1] += " (currently set, value redacted)"
            else:
                ret[-1] += f" (which is currently '{value}')"
    if field_info.is_required():
        ret.append("This parameter is mandatory")
    return (prefix + ". ".join(ret)) if ret else ""


def merge_args(settings_class: Any, args: Any) -> "BaseSettings":
    """ Merges settings from command line arguments and Pydantic settings

    Arguments:
    settings_class -- Type, derived from pydantic.BaseSettings
    args           -- Arguments, parsed by ArgumentParser
    Returns Object of type derived from pydantic.BaseSettings
    """
    kwargs: Dict[str, Any] = \
        {k: getattr(args, k) for k in settings_class.model_fields
         if getattr(args, k, None) not in (None, False)}
    return settings_class(**kwargs)
