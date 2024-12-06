""" Common helper functions for Pydantic """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

import os
try:
    import pydantic
except ImportError:
    # To have access to, day, safe_dsn() even without pydantic
    pass
from typing import Any, Dict, List


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
    assert props is not None, \
        f"Command line argument '--{arg}' not found in settings class " \
        f"{settings_class.schema()['title']}"
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


def merge_args(settings_class: Any, args: Any) -> "pydantic.BaseSettings":
    """ Merges settings from command line arguments and Pydantic settings

    Arguments:
    settings_class -- Type, derived from pydantic.BaseSettings
    args           -- Arguments, parsed by ArgumentParser
    Returns Object of type derived from pydantic.BaseSettings
    """
    kwargs: Dict[str, Any] = \
        {k: getattr(args, k) for k in settings_class.schema()["properties"]
         if getattr(args, k, None) not in (None, False)}
    return settings_class(**kwargs)
