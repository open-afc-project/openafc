""" Common routines/constants used by script in this folder """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

# pylint: disable=invalid-name

import json
import os
import re
import shlex
import subprocess
import sys
from typing import Any, cast, Dict, List, NamedTuple, NoReturn, Optional, \
    Set, Tuple, Union
import yaml

# Directory of scripts relative to sources' root
SCRIPTS_REL_DIR = "helm/bin"

# Directory of inner AFC cluster helmcharts relative to sources' root
INT_HELM_REL_DIR = "helm/afc-int"

# Absolute scripts' directory
SCRIPTS_DIR = os.path.abspath(os.path.dirname(__file__))

# Absolute sources' root directory
ROOT_DIR = \
    os.path.abspath(os.path.join(SCRIPTS_DIR,
                                 *([".."] * len(SCRIPTS_REL_DIR.split("/")))))

# Prefix for everything k3d
K3D_PREFIX = "k3d-"

# Auto-name
AUTO_NAME = "AUTO"


def error(message: str) -> NoReturn:
    """ Prints given message and exit abnormally """
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def error_if(cond: Any, message: str) -> None:
    """If condition is met - print given error message and exit.

    Arguments:
    cond    -- Condition to check
    message -- Message to print
    """
    if cond:
        error(message)


def warning(message: str) -> None:
    """ Prints given warning message """
    print(f"WARNING: {message}", file=sys.stderr)
    sys.exit(1)


def warning_if(cond: Any, message: str) -> None:
    """If condition is met - print given warning message.

    Arguments:
    cond       -- Condition to check
    message    -- Message to print
    """
    if cond:
        warning(message)


def execute(args: Union[List[str], str], cwd: Optional[str] = None,
            return_output: bool = False, echo=True) -> Optional[str]:
    """ Execute given command or commands

    Arguments:
    args          -- List of command parts or string with multiline series of
                     commands
    cwd           -- Optional directory to execute in
    return_output -- True to return output, False to print output and return
                     nothing
    echo          -- True to print command being executed
    Returns stdout if requested, otherwise None
    """
    ret: Optional[str] = None
    command: Union[str, List[str]]
    # No matter what, MyPy doesn't get it...
    for command in (args.splitlines() if isinstance(args, str) else [args]):
        if not command:
            continue

        print_parts: List[str] = []
        if cwd:
            print_parts.append(f"cd {cwd}")
        print_parts.append(
            re.sub(r" \s+", " ", command) if isinstance(command, str)
            else " ".join(shlex.quote(arg) for arg in command))
        if cwd:
            print_parts.append("cd -")

        if echo:
            print(" ; ".join(print_parts))
        try:
            p = subprocess.run(command, shell=isinstance(command, str),
                               text=True, check=False, cwd=cwd,
                               stdout=subprocess.PIPE if return_output
                               else None)
        except OSError as ex:
            ofwhat = "" if echo else f" of '{' ; '.join(print_parts)}'"
            error(f"Execution{ofwhat} failed: {ex}")
        error_if(p.returncode, "Execution failed")
        if return_output:
            ret = (ret or "") + ("\n" if ret else "") + p.stdout
    return ret


def parse_kubecontext(context_arg: Optional[str]) -> \
        Tuple[Optional[str], Optional[str]]:
    """ Parse optional contetx parameter [kubeconfig][:context]

    Returns (kubeconfig, kubecontext) tuple (both parts can be None
    """
    kubeconfig: Optional[str] = None
    kubecontext: Optional[str] = None
    m = re.match(r"^([^:]*)?(:([^:]*))?$", context_arg or "")
    error_if(not m, "Invalid --context parameter value format")
    assert m is not None
    if m.group(1):
        kubeconfig = os.path.expanduser(os.path.expandvars(m.group(1)))
        error_if(not os.path.isfile(kubeconfig),
                 f"Kubeconfig file `{kubeconfig}' not found")
    if m.group(3):
        config = \
            json.loads(
                cast(
                    str,
                    execute(
                        ["kubectl", "config", "view", "-o", "json"] +
                        (["--kubeconfig", kubeconfig] if kubeconfig
                         else []),
                        return_output=True)))
        for context in config.contexts:
            if context.name in (m.group(3), f"{K3D_PREFIX}{m.group(3)}"):
                kubecontext = context.name
                break
        else:
            error(
                f"Context {m.group(3)} not found"
                f"{(' in file ' + kubeconfig) if kubeconfig else ''}")
    return (kubeconfig, kubecontext)


def parse_k3d_reg(k3d_reg_arg: Optional[str]) -> str:
    """ Returns k3d registry address """
    reg_host = \
        cast(re.Match, re.match(r"^[^:]*", k3d_reg_arg or "")).group(0)
    m = re.search(r":(.*)$", k3d_reg_arg or "")
    reg_port = m.group(1) if m else ""
    ret: Optional[str] = None
    for reg_info in \
            json.loads(cast(str,
                            execute(["k3d", "registry", "list", "-o", "json"],
                                    return_output=True))):
        reg_info_name = reg_info["name"]
        reg_info_port = \
            str(reg_info["portMappings"]["5000/tcp"][0]["HostPort"])
        if reg_host and \
                (reg_info_name not in (reg_host, f"{K3D_PREFIX}{reg_host}")):
            continue
        if reg_port and (reg_port != reg_info_port):
            continue
        error_if(ret,
                 f"More than one{' matching' if k3d_reg_arg else ''} k3d "
                 f"registry running. Please specify registry to "
                 f"use{' more' if k3d_reg_arg else ''} explicitly with "
                 f"--k3d_reg parameter")
        ret = f"{reg_info_name}:{reg_info_port}"
    error_if(not ret,
             "Matching k3d registry not found" if k3d_reg_arg
             else "No k3d registries is running")
    assert ret is not None
    return ret


NodePortInfo = NamedTuple("NodePortInfo", [("component", str), ("port", int)])


def get_known_nodeports(helm_dir: str) -> Dict[str, NodePortInfo]:
    """ Returns dictionary of (component, nodeport) pairs indexed by nodeport
    names """
    values_file = os.path.join(ROOT_DIR, helm_dir, "values.yaml")
    try:
        with open(values_file, encoding="utf-8") as f:
            values = \
                yaml.load(f.read(),
                          yaml.CFullLoader if hasattr(yaml, "CFullLoader")
                          else yaml.FullLoader)
    except (OSError, yaml.YAMLError) as ex:
        error(f"Error reading '{values_file}': {ex}")
    if "components" not in values:
        warning(f"'{values_file}' has unknown structure (no 'components' "
                f"section)")
        return {}
    duplicates: Set[str] = set()
    ret: Dict[str, NodePortInfo] = {}
    for component_name, component_info in values["components"].items():
        for port_name, port_info in component_info.get("ports", {}).items():
            nodeport = port_info.get("nodePort")
            if nodeport is None:
                continue
            if port_name in ret:
                duplicates.add(port_name)
            else:
                ret[port_name] = \
                    NodePortInfo(component=component_name, port=nodeport)
    if duplicates:
        warning(f"These nodeports have duplicate names - they'll be excluded "
                f"from 'known': {', '.join(sorted(duplicates))}")
        for name in duplicates:
            del ret[name]
    warning_if(not ret,
               f"Strangely, no known nodeports found in '{values_file}'")
    return ret


def rfc_1123(s: Optional[str]) -> Optional[str]:
    """ Convert given string to RFC1123 ([a-z0-9-]+), kabob-casing case
    transitions

    Arguments:
    s -- String to convert or None
    Returns None for None, converted string otherwise
    """
    if s is None:
        return s
    s = re.sub(r"[_+]", "-", s).strip("-")
    while True:
        m = re.search("[A-Z]+", s)
        if m is None:
            break
        s = s[: m.start(0)] + \
            ("-" if m.start(0) and (s[m.start(0) - 1] != "-") else "") + \
            s[m.start(0): m.end(0)].lower() + \
            ("-" if ((m.end(0) - m.start(0)) > 1) and (m.end(0) < len(s)) and
             (not s[m.end(0):].startswith("-")) else "") + \
            s[m.end(0):]
    return s


def auto_name(kabob: bool) -> str:
    """ Returns auto-name, consisting of username and checkout directory name
    (kabobcased optionally) """
    def kabober(s: str) -> str:
        """ Optional kabobcasing """
        return cast(str, rfc_1123(s)) if kabob else s

    return f"{kabober(os.getlogin())}{kabober('_')}" \
        f"{kabober(os.path.basename(ROOT_DIR) or 'root')}"


def parse_json_output(args: List[str], echo: bool = True) -> Any:
    """ Execute given command and return its output as parsed json """
    return json.loads(cast(str, execute(args, return_output=True, echo=echo)))
