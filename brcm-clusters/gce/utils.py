""" Common functionality for scripts defined n this folder """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

# pylint: disable=invalid-name, too-many-arguments

import json
import os
import shlex
import subprocess
import sys
import tempfile
from typing import Any, cast, List, NoReturn, Optional, Tuple
import yaml

# Default value (fnmatch expression) for internal cluster context
DEFAULT_INT_CONTEXT = ":*int"

# Default value (fnmatch expression) for external cluster context
DEFAULT_EXT_CONTEXT = ":*ext"

# Default release name for helm_install_ext/int.py
DEFAULT_RELEASE = "AUTO"

# Directory of this script and scripts that use it
SCRIPT_DIR = os.path.dirname(__file__)

# Default directory of sources (containing 'helm' and other subdirectories)
DEFAULT_SOURCE_ROOT = \
    os.path.normpath(os.path.abspath(os.path.join(SCRIPT_DIR, "../..")))

# Binary subdirectory under source root
HELM_BIN_SUBDIR = "helm/bin"

# Cluster-specific file, containing common values for soem scripts
VALUES_AFC_COMMON_YAML = "values-afc-common.yaml"


def error(errmsg: str) -> NoReturn:
    """Print given error message and exit"""
    print(f"{os.path.basename(sys.argv[0])}: Error: {errmsg}", file=sys.stderr)
    sys.exit(1)


def error_if(condition: Any, errmsg: str) -> None:
    """If given condition met - print given error message and exit"""
    if condition:
        error(errmsg)


def execute(args: List[str], return_output: bool = False) -> Optional[str]:
    """ Execute given command, optionally returning output """
    try:
        print(f">>> {' '.join(shlex.quote(arg) for arg in args)}")
        if return_output:
            return subprocess.check_output(args, encoding="utf-8")
        subprocess.check_call(args)
        return None
    except (OSError, subprocess.CalledProcessError) as ex:
        print(f"Execution failed: {ex}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(1)


def optional_args(args: List[Tuple[str, Optional[str], Any]]) -> List[str]:
    """ Optional subprocess arguments

    Arguments:
    args -- List of (param, value, predicate) tuples. Parameters with false
            predicates are skipped. Parameters with None value are generated
            without value
    Returns subprocess argument list for given argument descriptors
    """
    ret: List[str] = []
    for param, value, pred in args:
        if not pred:
            continue
        ret.append(param)
        if value is not None:
            ret.append(value)
    return ret


def helm_bin(source_root: Optional[str], script: str) -> str:
    """ Full path to script in helm/bin directory

    Arguments:
    source_root -- Source root directory (None for default which is parent
                   directory of this script)
    script      -- Script name
    Returns full path to given script
    """
    return os.path.abspath(os.path.join(source_root or DEFAULT_SOURCE_ROOT,
                                        HELM_BIN_SUBDIR, script))


def cluster_values() -> List[str]:
    """ Returns list of values for CLUSTER parameter of scripts """
    return \
        list(
            sorted(
                dn for dn in os.listdir(SCRIPT_DIR)
                if os.path.isdir(os.path.join(SCRIPT_DIR, dn)) and
                (not any(dn.startswith(ignored_prefix)
                         for ignored_prefix in (".", "__")))))


def cluster_filename(cluster: str, filename: str) -> str:
    """ Full path to YANML file in cluster-specific subdirectory of this script

    Arguments:
    cluster  -- This script's subdirectory containing cluster-specific files
    filename -- File name
    Returns full path to given file name
    """
    return os.path.abspath(os.path.join(SCRIPT_DIR, cluster, filename))


def values_afc_common(cluster: str) -> Any:
    """ Reads values-afc-common.yaml from given cluster-specific subdirectory

    Arguments:
    cluster  -- This script's subdirectory containing cluster-specific files
    Returns file content as dictionary
    """
    values_afc_common_yaml = \
        cluster_filename(cluster, VALUES_AFC_COMMON_YAML)
    error_if(not os.path.isfile(values_afc_common_yaml),
             f"{values_afc_common_yaml} not found")
    try:
        with open(values_afc_common_yaml, encoding="utf=8") as f:
            return \
                yaml.load(
                    f.read(),
                    Loader=getattr(yaml, "CFullLoader", yaml.FullLoader))
    except OSError as ex:
        error(f"Error reading '{values_afc_common_yaml}': {ex}")
    except yaml.YAMLError as ex:
        error(f"Error parsing '{values_afc_common_yaml}': {ex}")
