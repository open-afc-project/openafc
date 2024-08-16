""" Common routines/constants used by script in this folder """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

# pylint: disable=invalid-name, too-many-arguments, global-statement

import argparse
import datetime
import fnmatch
import json
import os
import re
import shlex
import socket
import subprocess
import sys
import time
from typing import Any, cast, Dict, List, NamedTuple, NoReturn, Optional, \
    Set, Union
import yaml

# Directory of scripts relative to sources' root
SCRIPTS_REL_DIR = "helm/bin"

# Directory of internal AFC cluster helmcharts relative to sources' root
INT_HELM_REL_DIR = "helm/afc-int"

# Directory of external AFC cluster helmcharts relative to sources' root
EXT_HELM_REL_DIR = "helm/afc-ext"

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

# --k3d_reg values that denote default registry
K3D_DEFAULT_REG = (None, "DEFAULT")

# True if execute() should print nothing
_silent_execute = False


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


def set_silent_execute(state: bool = True) -> None:
    """ Disables/enables silent execute() """
    global _silent_execute
    _silent_execute = state


def execute(args: Union[List[str], str], cwd: Optional[str] = None,
            return_output: bool = False, echo=True, hide_stderr: bool = False,
            print_prefix: str = "> ", fail_on_error: bool = True) \
            -> Union[str, bool]:
    """ Execute given command or commands

    Arguments:
    args          -- List of command parts or string with multiline series of
                     commands
    cwd           -- Optional directory to execute in
    return_output -- True to return output, False to print output and return
                     nothing
    echo          -- True to print command being executed (ignored if
                     _silent_execute is set)
    hide_stderr   -- True to not print stderr
    print_prefix  -- Prefix to print before each command
    fail_on_error -- True to fail on error
    Returns stdout if requested, otherwise success status (True on success)
    """
    ret: Union[str, bool] = "" if return_output else True
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

        echo &= (not _silent_execute)
        if echo:
            print(f"{print_prefix or ''}{' ; '.join(print_parts)}")
        try:
            p = subprocess.run(command, shell=isinstance(command, str),
                               encoding="utf-8", errors="backslashreplace",
                               check=False, cwd=cwd,
                               stdout=subprocess.PIPE
                               if return_output or _silent_execute else None,
                               stderr=subprocess.PIPE
                               if hide_stderr or _silent_execute else None)
        except OSError as ex:
            if not fail_on_error:
                continue
            ofwhat = "" if echo else f" of '{' ; '.join(print_parts)}'"
            error(f"Execution{ofwhat} failed: {ex}")
        error_if(p.returncode and fail_on_error, "Execution failed")
        if return_output:
            assert isinstance(ret, str)
            ret = ret + ("\n" if ret else "") + \
                ("" if p.returncode else p.stdout)
        else:
            assert isinstance(ret, bool)
            ret &= (p.returncode == 0)
    return ret


def parse_json_output(args: List[str], echo: bool = True) -> Any:
    """ Execute given command and return its output as parsed json """
    return json.loads(cast(str, execute(args, return_output=True, echo=echo)))


class ClusterContext(NamedTuple):
    """ Parameters that define Kubernetes cluster in command line """

    # Optional config file name
    config: Optional[str]

    # Optional contetx name
    context: Optional[str]

    def helm_args(self, namespace: Optional[str] = None) -> List[str]:
        """ Returns cluster/namespace-defining parameters for helm command line

        Arguments:
        nemespace -- If non-None, add namespace parameters
        Returns list of arguments for subprocess functions
        """
        return filter_args("--kubeconfig", self.config,
                           "--kube-context", self.context,
                           "--namespace", namespace)

    def kubectl_args(self, namespace: Optional[str] = None) -> List[str]:
        """ Returns cluster/namespace-defining parameters for kubectl command
        line

        Arguments:
        nemespace -- If non-None, add namespace parameters
        Returns list of arguments for subprocess functions
        """
        return filter_args("--kubeconfig", self.config,
                           "--context", self.context,
                           "--namespace", namespace)


def parse_kubecontext(context_arg: Optional[str]) -> ClusterContext:
    """ Parse optional contetx parameter [kubeconfig][:context]

    Returns ClusterContext, containing stuff, defined in the argument
    """
    kubeconfig: Optional[str] = None
    m = re.match(r"^([^:]*)?(:([^:]*))?$", context_arg or "")
    error_if(not m, "Invalid --context parameter value format")
    assert m is not None
    if m.group(1):
        kubeconfig = os.path.expanduser(os.path.expandvars(m.group(1)))
        error_if(not os.path.isfile(kubeconfig),
                 f"Kubeconfig file `{kubeconfig}' not found")
    kubecontext: Optional[str] = None
    if m.group(3):
        config = \
            parse_json_output(
                ["kubectl", "config", "view", "-o", "json"] +
                (["--kubeconfig", kubeconfig] if kubeconfig else []))
        kubecontexts: List[str] = []
        for context in config.get("contexts", []):
            if any(fnmatch.fnmatch(context.get("name", ""), pattern)
                   for pattern in (m.group(3), f"{K3D_PREFIX}{m.group(3)}")):
                kubecontexts.append(context["name"])
        error_if(
            len(kubecontexts) > 1,
            f"More than one kubecontexts"
            f"{(' in file ' + kubeconfig) if kubeconfig else ''} matches "
            f"'{m.group(3)}': {', '.join(kubecontexts)}")
        error_if(
            len(kubecontexts) < 1,
            f"No kubecontexts found"
            f"{(' in file ' + kubeconfig) if kubeconfig else ''} matching "
            f"'{m.group(3)}'")
        kubecontext = kubecontexts[0]
    return ClusterContext(config=kubeconfig, context=kubecontext)


# Information about image registry. may have diferent pull registry (registry
# # Kubernetes fetcher images from) and push registry (registry to push images
# # to)
ImageRegistryInfo = \
    NamedTuple("ImageRegistryInfo", [("pull", str), ("push", str)])


def parse_k3d_reg(k3d_reg_arg: Optional[str], required: bool = True) \
        -> Optional[ImageRegistryInfo]:
    """ Returns k3d registry information """
    if k3d_reg_arg in K3D_DEFAULT_REG:
        k3d_reg_arg = None
    reg_host = \
        cast(re.Match, re.match(r"^[^:]*", k3d_reg_arg or "")).group(0)
    m = re.search(r":(.*)$", k3d_reg_arg or "")
    reg_port = m.group(1) if m else ""
    pull: Optional[str] = None
    for reg_info in \
            parse_json_output(["k3d", "registry", "list", "-o", "json"]):
        reg_info_name = reg_info["name"]
        reg_info_port = \
            str(reg_info["portMappings"]["5000/tcp"][0]["HostPort"])
        if reg_host and \
                (reg_info_name not in (reg_host, f"{K3D_PREFIX}{reg_host}")):
            continue
        if reg_port and (reg_port != reg_info_port):
            continue
        if pull:
            if not required:
                return None
            error(f"More than one{' matching' if k3d_reg_arg else ''} k3d "
                  f"registry running. Please specify registry to "
                  f"use{' more' if k3d_reg_arg else ''} explicitly with "
                  f"--k3d_reg parameter")
        pull = f"{reg_info_name}:{reg_info_port}"
    if not pull:
        error("Matching k3d registry not found" if k3d_reg_arg
              else "No k3d registries is running")
    assert pull is not None
    reg_host, reg_port = pull.split(":")
    try:
        socket.gethostbyname(reg_host)
        push = pull
    except OSError:
        # CentOS 7 can't resolve k3d registry host name
        push = f"localhost:{reg_port}"
    return ImageRegistryInfo(pull=pull, push=push)


def yaml_load(filename: str, multidoc: bool = False) -> Any:
    """ Returns loaded content of given yaml file """
    try:
        with open(filename, encoding="utf-8") as f:
            return yaml_loads(f.read(), filename=filename, multidoc=multidoc)
    except OSError as ex:
        error(f"Error reading '{filename}': {ex}")


def yaml_loads(s: str, filename: Optional[str] = None,
               multidoc: bool = False) -> Any:
    """ Returns yaml-parsed value of given string """
    try:
        return \
            (yaml.load_all if multidoc else yaml.load)(
                s,
                yaml.CFullLoader if hasattr(yaml, "CFullLoader")
                else yaml.FullLoader)
    except yaml.YAMLError as ex:
        error(f"Invalid YAML syntax of '{filename if filename else s}': {ex}")


NodePortInfo = NamedTuple("NodePortInfo", [("component", str), ("port", int)])


def get_known_nodeports(helm_dir: str) -> Dict[str, NodePortInfo]:
    """ Returns dictionary of (component, nodeport) pairs indexed by nodeport
    names """
    values_file = os.path.join(ROOT_DIR, helm_dir, "values.yaml")
    values = yaml_load(values_file)
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


def rfc_1123(s: Optional[str], no_strip: bool = False) -> Optional[str]:
    """ Convert given string to RFC1123 ([a-z0-9-]+), kabob-casing case
    transitions

    Arguments:
    s        -- String to convert or None
    no_strip -- don't strip trailing dash
    Returns None for None, converted string otherwise
    """
    if s is None:
        return s
    s = re.sub(r"[_+]", "-", s)
    if not no_strip:
        s = s.strip("-")
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
    def kabober(s: str, no_strip: bool = False) -> str:
        """ Optional kabobcasing """
        return cast(str, rfc_1123(s, no_strip=no_strip)) if kabob else s

    return f"{kabober(os.getlogin())}{kabober('_', no_strip=True)}" \
        f"{kabober(os.path.basename(ROOT_DIR) or 'root')}"


class ArgumentParserFromFile(argparse.ArgumentParser):
    """ Performs proper splitting for arguments fetched from file """

    def convert_arg_line_to_args(self, arg_line: str) -> List[str]:
        """ Split multipart argument fetched from file """
        return shlex.split(arg_line, comments=True)


def filter_args(*args) -> List[str]:
    """ Makes command argument list from specified arguments

    Arguments:
    *args -- sequence of 'param' 'value' pairs ('param' may be None - then it
             is not used, 'value' may be nonstring - then it is evaluated but
             not used)
    Returns argument list, containing those pairs that have nontrivial values
    """
    assert (len(args) % 2) == 0
    ret: List[str] = []
    for idx in range(0, len(args), 2):
        if not args[idx + 1]:
            continue
        if args[idx] is not None:
            ret.append(args[idx])
        if isinstance(args[idx + 1], str):
            ret.append(args[idx + 1])
    return ret


def unused_argument(arg: Any) -> None:  # pylint: disable=unused-argument
    """ Sink for all unused arguments """
    return


def expand_filename(fn: str, root: Optional[str] = None) -> str:
    """ Do expamnduser/expandvars on given filename, optionally rebasing it
    from given root  """
    ret = os.path.expanduser(os.path.expandvars(fn))
    if root:
        ret = os.path.join(root, ret)
    return ret


class Duration:
    """ Holds duration, convertible to seconds and go durations

    Private attributes:
    _seconds -- Optional floating point seconds
    """
    # Go duration units
    GO_UNITS: Dict[str, Union[float, int]] = {
        "ns": 1e-9,
        "us": 1e-6,
        "ms": 1e-3,
        "s": 1,
        "m": 60,
        "h": 3600,
        "d": 3600 * 24,
        "w": 3600 * 24 * 7,
        "y": 3600 * 24 * 365.25}

    def __init__(self, duration: Optional[Union[str, int, float]]) -> None:
        """ Constructs from seconds or Go duration """
        self._seconds: Optional[Union[float, int]]
        if duration is None:
            self._seconds = None
            return
        if isinstance(duration, (int, float)):
            self._seconds = duration
            return
        assert isinstance(duration, str)
        m = re.match(r"^[0-9]+$", duration)
        if m:
            try:
                self._seconds = float(duration)
                return
            except ValueError:
                error(f"Duration '{duration}' has invalid format")
        m = re.match("^([0-9.]+[a-z]+)+$", duration)
        error_if(not m, f"Duration '{duration}' has invalid format")
        self._seconds = 0
        for m in re.finditer("([0-9.]+)([a-z]+)", duration):
            try:
                self._seconds += float(m.group(1)) * self.GO_UNITS[m.group(2)]
            except ValueError:
                error(f"Duration '{duration}' has invalid duration "
                      f"'{m.group(1)}'")
            except LookupError:
                error(f"Duration '{duration}' has invalid unit '{m.group(2)}'")

    def __bool__(self) -> bool:
        """ True if duration is defined """
        return self._seconds is not None

    def helm_timeout(self) -> List[str]:
        """ Timeout wait arguments for helm command line """
        if self._seconds is None:
            return []
        go_duration: str
        if self._seconds == 0:
            go_duration = "0s"
        else:
            go_duration = ""
            remaining = self._seconds
            while True:
                best_unit: Optional[str] = None
                for unit, duration in self.GO_UNITS.items():
                    if duration > remaining:
                        continue
                    if (best_unit is None) or \
                            (duration > self.GO_UNITS[best_unit]):
                        best_unit = unit
                if best_unit is None:
                    break
                n = int(remaining // self.GO_UNITS[best_unit])
                go_duration += f"{n}{best_unit}"
                remaining -= n * self.GO_UNITS[best_unit]
        return ["--wait", "--timeout", go_duration]

    def kubectl_timeout(self) -> List[str]:
        """ Timeout wait arguments for kubectl command line """
        return self.helm_timeout()

    def seconds(self) -> Optional[Union[int, float]]:
        """ Duration in seconds, None if undefined """
        return self._seconds


def get_helm_values(helm_args: List[str]) -> Dict[str, Any]:
    """ Retrieves resulting helm values for given helm command """
    dry_run = cast(str,
                   execute(helm_args + ["--dry-run", "--debug"],
                           return_output=True, fail_on_error=False))
    if dry_run is None:
        return {}
    m = re.search(r"\nCOMPUTED VALUES:\n((.|\n)+?)\n---", dry_run)
    error_if(not m,
             "Can't obtain helm values: unrecognized helm output structure")
    assert m is not None
    return yaml_loads(m.group(1))


def print_helm_values(helm_args: List[str], print_format: str) -> None:
    """ Print helm values for given helm command line in given format """
    values = get_helm_values(helm_args=helm_args)
    if print_format == "yaml":
        print(yaml.dump(values, indent=2))
    elif print_format == "json":
        print(json.dumps(values, indent=2))
    else:
        error(f"Internal error: unsupported values output format "
              f"'{print_format}'")


def wait_termination(what: str, cluster_context: ClusterContext,
                     namespace: Optional[str] = None,
                     uninstall_duration: Duration = Duration(None)) -> None:
    """ Wait until all 'Terminating' extinguished in given context

    Arguments:
    what               -- What is being uininstalled (for use in error
                          messages)
    cluster_context    -- Cluster context
    namespace          -- Optional namespace name
    uninstall_duration -- Maximum duration
    """
    start = datetime.datetime.now()
    time.sleep(1)
    while "Terminating" in \
            cast(
                str,
                execute(
                    ["kubectl", "get", "all"] +
                    cluster_context.kubectl_args(namespace=namespace),
                    return_output=True)):
        error_if(
            uninstall_duration and
            ((datetime.datetime.now() - start).total_seconds() >
             cast(float, uninstall_duration.seconds())),
            f"Uninstall timeout expired for {what}")
        time.sleep(5)
