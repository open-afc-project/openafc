#!/usr/bin/env python3
""" AFC Load Test Tool """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=too-many-lines, too-many-arguments, invalid-name
# pylint: disable=consider-using-f-string, wrong-import-order, too-many-locals
# pylint: disable=too-few-public-methods, logging-fstring-interpolation
# pylint: disable=too-many-instance-attributes, broad-exception-caught
# pylint: disable=too-many-branches, too-many-nested-blocks
# pylint: disable=too-many-statements, eval-used

import argparse
from collections.abc import Iterable, Iterator
import copy
import csv
import datetime
import enum
import hashlib
import http
import inspect
import json
import logging
import multiprocessing
import os
import random
try:
    import requests
except ImportError:
    pass
import re
import shlex
import signal
import sqlite3
import subprocess
import sys
import time
import traceback
from typing import Any, Callable, cast, List, Dict, NamedTuple, NoReturn, \
    Optional, Tuple, Union
import urllib.error
import urllib.parse
import urllib.request

has_yaml = True
try:
    import yaml
except ImportError:
    has_yaml = False


CURRENT_CLUSTER_NAME = "CURRENT"
K3D_PREFIX = "k3d-"

Protocol = enum.Enum("Protocol", ["http", "https"])


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
    print(f"DP {frameinfo.function}()@{frameinfo.lineno}: {msg}")


def error(msg: str) -> NoReturn:
    """ Prints given msg as error message and exit abnormally """
    logging.error(msg)
    sys.exit(1)


def error_if(cond: Any, msg: str) -> None:
    """ If condition evaluates to true prints given msg as error message and
    exits abnormally """
    if cond:
        error(msg)


def exc_if(predicate: Any, exc: Exception) -> None:
    """ Raise given exception if predicate evaluates to true """
    if predicate:
        raise exc


def expandpath(path: Optional[str]) -> Optional[str]:
    """ Expand ~ and {} in given path. For None/empty returns None/empty """
    return os.path.expandvars(os.path.expanduser(path)) if path else path


def unused_argument(arg: Any) -> None:  # pylint: disable=unused-argument
    """ Sink for all unused arguments """
    return


def yaml_load(yaml_s: str) -> Any:
    """ YAML/JSON dictionary for given YAML/JSON content """
    kwargs: Dict[str, Any] = {}
    if has_yaml:
        if hasattr(yaml, "CLoader"):
            kwargs["Loader"] = yaml.CLoader
        elif hasattr(yaml, "FullLoader"):
            kwargs["Loader"] = yaml.FullLoader
        return yaml.load(yaml_s, **kwargs)
    return json.loads(yaml_s)


class Config(Iterable):
    """ Node of config structure that provides dot-based access

    Private attributes:
    _data -- Data that corresponds to node (list or string-indexed dictionary)
    _path -- Path to node ("foo.bar.42.baz") for use in error messages
    """

    JSON_EXT = ".json"
    YAML_EXT = ".yaml"

    # Default config file name
    DEFAULT_CONFIG = \
        os.path.splitext(__file__)[0] + (YAML_EXT if has_yaml else JSON_EXT)

    def __init__(
            self, argv: Optional[List[str]] = None,
            arg_name: Optional[str] = None, config_env: Optional[str] = None,
            data: Optional[Union[List[Any], Dict[str, Any]]] = None,
            path: Optional[str] = None) -> None:
        """ Constructor (for both root and nonroot nodes)

        Arguments:
        argv       -- Argument for root node: Command line arguments
        arg_name   -- Argument for root node - name of command line parameter
                      with config
        env_config -- Optional argument for root node - name of environment
                      variable for main config
        data       -- Argument for nonroot node: data that corresponds to node
                      (list or string-indexed dictionary)
        path       -- Argument for nonroot node: path to node
                      ("foo.bar[42].baz") for use in error messages
        """
        self._data: Union[List[Any], Dict[str, Any]]
        self._path: str
        # Nonroot node
        if data is not None:
            assert path is not None
            assert (argv or arg_name or config_env) is None
            self._path = path
            self._data = data
            return

        # Root node
        assert path is None
        assert argv is not None
        assert arg_name is not None
        self._path = ""
        argument_parser = argparse.ArgumentParser(add_help=False)
        argument_parser.add_argument(
            "--" + arg_name, action="append", default=[])
        configs = getattr(argument_parser.parse_known_args(argv)[0], arg_name)
        # Finding root config
        if (not configs) or any(c.startswith("+") for c in configs):
            for config_file in \
                    ([expandpath(os.environ.get(config_env))]
                     if config_env else []) + [self.DEFAULT_CONFIG]:
                assert config_file is not None
                if os.path.isfile(config_file):
                    configs = [config_file] + configs
                    break
            else:
                error("This script's config file not found")
        self._data = {}
        # Merging all configs
        for config in configs:
            if config.startswith("+"):
                config = config[1:]
            error_if(not os.path.isfile(config),
                     f"Config file '{config}' not found")
            with open(config, encoding="utf-8") as f:
                yaml_s = f.read()
            try:
                config_yaml_dict = yaml_load(yaml_s)
            except (yaml.YAMLError if has_yaml else json.JSONDecodeError) \
                    as ex:
                error(f"Error reading '{config}': {repr(ex)}")
            error_if(not isinstance(config_yaml_dict, dict),
                     f"Content of config file '{config}' is not a dictionary")
            assert isinstance(config_yaml_dict, dict)
            self._data = {**self._data, **config_yaml_dict}

    def __getattr__(self, attr: str) -> Any:
        """ Returns given attribute of node """
        exc_if(not (isinstance(self._data, dict) and (attr in self._data)),
               AttributeError(f"Config item '{self._path}' does not have "
                              f"attribute '{attr}'"))
        assert isinstance(self._data, dict)
        return self._subitem(value=self._data[attr], attr=attr)

    def __getitem__(self, key: Union[int, str]) -> Any:
        """ Access by index - integer or string """
        if isinstance(self._data, list):
            exc_if(not isinstance(key, (int, slice)),
                   IndexError(f"Config item '{self._path}' is a list, it "
                              f"can't be subscribed with '{key}'"))
            assert isinstance(key, (int, slice))
            exc_if(isinstance(key, int) and (key >= len(self._data)),
                   IndexError(f"Index {key} is out of range for config item "
                              f"'{self._path}'"))
        else:
            assert isinstance(self._data, dict)
            exc_if(not isinstance(key, str),
                   KeyError(f"Config item '{self._path}' is a dictionary, it "
                            f"can't be subscribed with '{key}'"))
            assert isinstance(key, str)
            exc_if(key not in self._data,
                   KeyError(f"Config item '{self._path}' does not have "
                            f"attribute '{key}'"))
        return self._subitem(value=self._data[key], attr=key)

    def get(self, attr: str, default: Any = None) -> Any:
        """ Keys of dictionary-type node """
        exc_if(not isinstance(self._data, dict),
               AttributeError(f"Config item '{self._path}' is not a "
                              f"dictionary, can't be queried for '{attr}'"))
        assert isinstance(self._data, dict)
        exc_if(not isinstance(attr, str),
               AttributeError(f"Non-string attribute '{attr}' can't be looked "
                              f"up in config item '{self._path}'"))
        return self._subitem(value=self._data.get(attr, default), attr=attr)

    def __len__(self) -> int:
        """ Number of subitems in node """
        return len(self._data)

    def keys(self) -> Iterable:
        """ Keys of dictionary-type node """
        exc_if(not isinstance(self._data, dict),
               AttributeError(f"Config item '{self._path}' is not a "
                              f"dictionary, it doesn't have keys"))
        assert isinstance(self._data, dict)
        return self._data.keys()

    def values(self) -> Iterable:
        """ Values of dictionary-type node """
        exc_if(not isinstance(self._data, dict),
               AttributeError(f"Config item '{self._path}' is not a "
                              f"dictionary, it doesn't have values"))
        assert isinstance(self._data, dict)
        for key, value in self._data.items():
            yield self._subitem(value=value, attr=key)

    def items(self) -> Iterable:
        """ Items of dictionary-type node """
        exc_if(not isinstance(self._data, dict),
               AttributeError(f"Config item '{self._path}' is not a "
                              f"dictionary, it doesn't have items"))
        assert isinstance(self._data, dict)
        for key, value in self._data.items():
            yield (key, self._subitem(value=value, attr=key))

    def data(self) -> Union[List[Any], Dict[str, Any]]:
        """ Returns underlying data structure """
        return self._data

    def __iter__(self) -> Iterator:
        """ Iterator over node subitems """
        if isinstance(self._data, list):
            for idx, value in enumerate(self._data):
                yield self._subitem(value=value, attr=idx)
        else:
            assert isinstance(self._data, dict)
            for key in self._data.keys():
                yield key

    def __in__(self, attr: Any) -> bool:
        """ True if dictionary node contains given attribute """
        exc_if(not isinstance(self._data, dict),
               AttributeError(f"Config item '{self._path}' is not a "
                              f"dictionary, 'in' check can't be performed"))
        exc_if(not isinstance(attr, str),
               AttributeError(f"Non-string attribute '{attr}' can't be looked "
                              f"up in config item '{self._path}'"))
        return attr in self._data

    def _subitem(self, value: Any, attr: Union[int, str]) -> Any:
        """ Returns given subitem as attribute of given name """
        if not isinstance(value, (list, dict)):
            return value
        return \
            self.__class__(
                data=value,
                path=f"{self._path}{'.' if self._path else ''}{attr}"
                if isinstance(attr, str) else f"{self._path}[{attr}]")


def get_output(args: List[str]) -> str:
    """ Returns stdout content of given command's output """
    try:
        return subprocess.check_output(args, universal_newlines=True,
                                       encoding="utf-8")
    except (subprocess.CalledProcessError, OSError) as ex:
        error(f"Failed to run '{' '.join(shlex.quote(arg) for arg in args)}': "
              f"{repr(ex)}")
    return ""  # Unreachable code to make pylint happy


class ServiceDiscovery:
    """ Base class for service discovery

    Protected members:
    cfg -- Config object
    """

    def __init__(self, cfg: Config) -> None:
        """ Constructor

        Arguments:
        cfg -- Config object
        """
        self.cfg = cfg

    def get_exec(self, service: str) -> List[str]:
        """ Initial part of command to execute something in container of given
        service. Should be overloaded """
        error("Cluster type/name not specified")
        unused_argument(service)
        return []  # Unreachable code

    def get_url(self, base_url: str, param_host: Optional[str],
                protocol: Optional[Protocol] = None) -> str:
        """ Construct URL from base URL (taken from config) and optional
        command line host

        Arguments:
        base_url   -- Base URL from config
        param_host -- Optional [protocol://][host:port][/path] from command
                      line
        protocol   -- Protocol (scheme) to use. None to use protocol from base
                      URL
        Returns actionable URL
        """
        base_parts = urllib.parse.urlparse(base_url)
        if param_host:
            replacement_url = param_host
            if "://" not in replacement_url:
                replacement_url = f"{base_parts.scheme}://{replacement_url}"
        else:
            base_netloc_parts = base_parts.netloc.split(":")
            host = base_netloc_parts[0]
            port = \
                int(base_netloc_parts[1]) if len(base_netloc_parts) > 1 else 80
            replacement_url = \
                self.get_cluster_url(
                    protocol=protocol if protocol is not None
                    else getattr(Protocol, base_parts.scheme),
                    service=host, port=port)
        replacement_parts = urllib.parse.urlparse(replacement_url)
        replacements = {field: getattr(replacement_parts, field)
                        for field in base_parts._fields
                        if getattr(replacement_parts, field)}
        return urllib.parse.urlunparse(base_parts._replace(**replacements))

    def get_cluster_url(self, protocol: Protocol, service: str, port: int) \
            -> str:
        """ Returns URL inside cluster that corresponds to given parameters
        (should be overloaded)

        Arguments:
        protocol -- Protocol (http or https)
        service  -- Service name from
        port     -- Port number
        Returns SCHEME://HOST:PORT
        """
        error("Cluster type/name not specified")
        unused_argument(protocol)  # Unreachable code
        unused_argument(service)
        unused_argument(port)
        return ""

    @classmethod
    def create(cls, cfg: Config, compose_project: Optional[str] = None,
               k3d_cluster: Optional[str] = None) -> "ServiceDiscovery":
        """ Factory method

        Arguments:
        cfg             -- Config object
        compose_project -- Compose project name or None
        k3d_cluster     -- K3d cluster name (may be CURRENT) or None
        Returns instance of class derived from ServiceDiscovery
        """
        error_if(compose_project and k3d_cluster,
                 "--comp_proj and --k3d parameters are mutually exclusive")
        if compose_project:
            return ServiceDiscoveryCompose(cfg=cfg,
                                           compose_project=compose_project)
        if k3d_cluster:
            return ServiceDiscoveryK3d(cfg=cfg, k3d_cluster=k3d_cluster)
        return ServiceDiscovery(cfg=cfg)

    def _get_json_output(self, args: List[str]) -> Any:
        """ Execute command and return parsed JSON output """
        try:
            return json.loads(get_output(args))
        except json.JSONDecodeError as ex:
            error(f"Error parsing as JSON output of "
                  f"'{' '.join(shlex.quote(arg) for arg in args)}': {ex}")
        return None  # Unreachable code


class ServiceDiscoveryCompose(ServiceDiscovery):
    """ Implements service discovery for Compose cluster

    Private attributes:
    _compose_project  -- Compose project name
    _containers       -- Dictionary of _ContainerInfo object, indexed by
                         service name. None before first access
    """
    class _ContainerInfo:
        """ Information about single service

        Public attributes:
        name      -- Container/pod name
        ip        -- IP address. None if not yet found
        ext_ports -- Dictionary of external IPv4 ports, indexed by internal
                     ports
        """

        def __init__(self, name: str) -> None:
            """ Constructor

            Arguments:
            name -- Container name
            """
            self.name = name
            self.ip = None
            self.ext_ports: Dict[int, int] = {}

    def __init__(self, cfg: Config, compose_project: str) -> None:
        """ Constructor

        Arguments:
        cfg             -- Config object
        compose_project -- Docker compose project name
        """
        super().__init__(cfg=cfg)
        self._compose_project = compose_project
        self._containers: \
            Optional[Dict[str, "ServiceDiscoveryCompose._ContainerInfo"]] = \
            None

    def get_exec(self, service: str) -> List[str]:
        """ Initial part of command to execute something in container of given
        service """
        return ["docker", "exec", self._get_cont_info(service=service).name]

    def get_cluster_url(self, protocol: Protocol, service: str, port: int) \
            -> str:
        """ Returns URL inside cluster that corresponds to given parameters

        Arguments:
        protocol -- Protocol
        service  -- Service name from
        port     -- Port number
        Returns SCHEME://HOST:PORT
        """
        ci = self._get_cont_info(service)
        if service == "dispatcher":
            ext_port = \
                ci.ext_ports.get(80 if protocol == Protocol.http else 443)
            error_if(not ext_port,
                     f"Dispatcher {protocol.name} port not exposed")
            return f"{protocol.name}://localhost:{ext_port}"
        if not ci.ip:
            inspect_dict = \
                self._get_json_output(["docker", "inspect", ci.name])
            try:
                for net_name, net_info in \
                        inspect_dict[0]["NetworkSettings"]["Networks"].items():
                    if net_name.endswith("_default"):
                        ci.ip = net_info["IPAddress"]
                        break
                else:
                    error(
                        f"Default network not found in container '{ci.name}'")
            except (AttributeError, LookupError) as ex:
                error(f"Unsupported structure of  'docker inspect {ci.name}' "
                      f"output: {repr(ex)}")
            assert ci.ip is not None
        return f"{protocol.name}://{ci.ip}:{port}"

    def _get_cont_info(self, service: str) \
            -> "ServiceDiscoveryCompose._ContainerInfo":
        """ Returns _ContainerInfo object for given service name """
        if self._containers is None:
            self._containers = {}
            name_offset: Optional[int] = None
            for line in get_output(["docker", "ps"]).splitlines():
                if name_offset is None:
                    name_offset = line.find("NAMES")
                    error_if(name_offset < 0,
                             "Unsupported structure of 'docker ps' output")
                    continue
                assert name_offset is not None
                container: Optional[str] = None
                for names in re.split(r",\s*", line[name_offset:]):
                    m = re.search(r"(%s_(.+)_\d+)" %
                                  re.escape(self._compose_project),
                                  names)
                    if m:
                        container = m.group(2)
                        self._containers[container] = \
                            self._ContainerInfo(name=m.group(1))
                if container:
                    for m in \
                            re.finditer(r"\d+\.\d+\.\d+\.\d+:(\d+)->(\d+)/tcp",
                                        line):
                        self._containers[container].\
                            ext_ports[int(m.group(2))] = int(m.group(1))
        error_if(not self._containers,
                 f"No running containers found for compose project "
                 f"'{self._compose_project}'")
        ret = self._containers.get(service)
        error_if(ret is None,
                 f"Service name '{service}' not found among containers of "
                 f"compose project '{self._compose_project}'")
        assert ret is not None
        return ret


class ServiceDiscoveryK3d(ServiceDiscovery):
    """ Implements service discovery for k3d cluster

    Private attributes:
    _k3d_cluster_arg  -- Cluster name from command line
    _k3d_cluster_name -- Actual cluster name (without k3d- prefix). Initially
                         None
    _context_name     -- Kubeconfig context name. Initially None
    _pod_list         -- List of pod names. Initially None
    _port_mapping     -- External ports indexed by internal nodeports.
                         Initially None
    """

    def __init__(self, cfg: Config, k3d_cluster: str) -> None:
        """ Constructor

        Arguments:
        cfg         -- Config object
        k3d_cluster -- K3d cluster name of CURRENT
        """
        super().__init__(cfg=cfg)
        self._k3d_cluster_arg = k3d_cluster
        self._k3d_cluster_name: Optional[str] = None
        self._context_name: Optional[str] = None
        self._pod_list: Optional[List[str]] = None
        self._port_mapping: Optional[Dict[int, int]] = None

    def get_exec(self, service: str) -> List[str]:
        """ Initial part of command to execute something in container of given
        service """
        self._compute_cluster_name()
        if self._pod_list is None:
            self._pod_list = []
            kubeconfig = self._get_json_output(["kubectl", "config", "view",
                                                "-o", "json"])
            for context in (kubeconfig or {}).get("contexts", []):
                if context.get("context" or {}).get("cluster", "") == \
                        f"{K3D_PREFIX}{self._k3d_cluster_name}":
                    self._context_name = context["name"]
                    break
            else:
                error(f"Cluster '{self._k3d_cluster_name}' (aka "
                      f"'{K3D_PREFIX}{self._k3d_cluster_name}') not found in "
                      f"current kubeconfig (without being in kubeconfig its "
                      f"pods can't be obtained)")
            assert self._context_name is not None
            self._pod_list = \
                [cast(re.Match, re.match(r"^(pod/)?(.*)$", line)).group(2)
                 for line in get_output(["kubectl", "get", "pods",
                                         "-o", "name",
                                         "--context", self._context_name,
                                         "-n", "default"]).splitlines()]
        assert self._context_name is not None
        service = service.replace("_", "-")
        for pod_name in self._pod_list:
            if pod_name.startswith(service):
                return ["kubectl", "exec", "--context", self._context_name,
                        pod_name, "--"]
        error(
            f"Pod '{service}' not found in cluster '{self._k3d_cluster_name}'")
        return []  # Unreachable code

    def get_cluster_url(self, protocol: Protocol, service: str, port: int) \
            -> str:
        """ Returns URL inside cluster that corresponds to given parameters

        Arguments:
        protocol -- Protocol
        service  -- Service name from
        port     -- Port number
        Returns SCHEME://HOST:PORT
        """
        self._compute_cluster_name()
        if self._port_mapping is None:
            self._port_mapping = {}
            for cluster_info in \
                    self._get_json_output(["k3d", "cluster", "list",
                                           "-o", "json"]):
                if cluster_info.get("name") == self._k3d_cluster_name:
                    for node_info in cluster_info.get("nodes", []):
                        for int_port, mappings in \
                                node_info.get("portMappings", {}).items():
                            m = re.match(r"^(\d+)/tcp$", int_port)
                            if not m:
                                continue
                            int_port_num = int(m.group(1))
                            for mapping_info in (mappings or []):
                                if mapping_info.get("HostIp"):
                                    continue
                                ext_port = mapping_info.get("HostPort")
                                if ext_port:
                                    self._port_mapping[int_port_num] = \
                                        int(ext_port)
                    break
            else:
                error(f"Information about k3d cluster "
                      f"{self._k3d_cluster_name} not found")
        assert self._port_mapping is not None
        nodeport = self.cfg.k3d_nodeports.get(service, {}).get(protocol.name)
        error_if(nodeport is None,
                 f"Can't find {protocol.name} nodeport name for {service} in "
                 f"the config file")
        error_if(nodeport not in self._port_mapping,
                 f"{protocol.name} port of {service} not exposed by k3d "
                 f"cluster {self._k3d_cluster_name}")
        return f"{protocol.name}://localhost:{self._port_mapping[nodeport]}"

    def _compute_cluster_name(self) -> None:
        """ Make sure _k3d_cluster_name is computed """
        if self._k3d_cluster_name is not None:
            return
        k3d_cluster_arg = self._k3d_cluster_arg
        if k3d_cluster_arg == CURRENT_CLUSTER_NAME:
            current_kubeconfig = \
                self._get_json_output(["kubectl", "config", "view", "--minify",
                                       "-o", "json"])
            clusters = current_kubeconfig.get("clusters", [])
            error_if(not clusters, "Current kubeconfig has no current cluster")
            k3d_cluster_arg = clusters[0].get("name", "")
            error_if(not k3d_cluster_arg.startswith(K3D_PREFIX),
                     "Current cluster is not k3d")
        self._k3d_cluster_name = \
            k3d_cluster_arg[len(K3D_PREFIX):] \
            if k3d_cluster_arg.startswith(K3D_PREFIX) else k3d_cluster_arg


def ratdb(cfg: Config, command: str, service_discovery: ServiceDiscovery) \
        -> Union[int, List[Dict[str, Any]]]:
    """ Executes SQL statement in ratdb

    Arguments:
    cfg               -- Config object
    command           -- SQL command to execute
    service_discovery -- ServiceDiscovery object
    Returns number of affected records for INSERT/UPDATE/DELETE, list of row
    dictionaries on SELECT
    """
    result = \
        get_output(
            service_discovery.get_exec(cfg.ratdb.service) +
            ["psql", "-U", cfg.ratdb.username, "-d", cfg.ratdb.dbname, "--csv",
             "-c", command])
    if command.upper().startswith("SELECT "):
        try:
            return list(csv.DictReader(result.splitlines()))
        except csv.Error as ex:
            error(f"CSV parse error on output of '{command}': {repr(ex)}")
    for begin, pattern in [("INSERT ", r"INSERT\s+\d+\s+(\d+)"),
                           ("UPDATE ", r"UPDATE\s+(\d+)"),
                           ("DELETE ", r"DELETE\s+(\d+)")]:
        if command.upper().startswith(begin):
            m = re.search(pattern, result)
            error_if(not m,
                     f"Can't fetch result of '{command}'")
            assert m is not None
            return int(m.group(1))
    error(f"Unknown SQL command '{command}'")
    return 0    # Unreachable code, appeasing pylint


def path_get(obj: Any, path: List[Union[str, int]]) -> Any:
    """ Read value from multilevel dictionary/list structure

    Arguments:
    obj  -- Multilevel dictionary/list structure
    path -- Sequence of indices
    Returns value at the end of sequence
    """
    for idx in path:
        obj = obj[idx]
    return obj


def path_set(obj: Any, path: List[Union[str, int]], value: Any) -> None:
    """ Write value to multilevel dictionary/list structure

    Arguments:
    obj   -- Multilevel dictionary/list structure
    path  -- Sequence of indices
    value -- Value to insert to point at the end of sequence
    """
    for idx in path[: -1]:
        obj = obj[idx]
    obj[path[-1]] = value


def path_del(obj: Any, path: List[Union[str, int]]) -> Any:
    """ Delete value from multilevel dictionary/list structure

    Arguments:
    obj  -- Multilevel dictionary/list structure
    path -- Sequence of indices, value at end of it is deleted
    """
    ret = obj
    for idx in path[: -1]:
        obj = obj[idx]
    if isinstance(path[-1], int):
        obj.pop(path[-1])
    else:
        del obj[path[-1]]
    return ret


class AfcReqRespGenerator:
    """ Generator of AFC Request/Response messages for given request indices

    Private attributes
    _paths            -- Copy of cfg.paths
    _req_msg_pattern  -- AFC Request message pattern as JSON dictionary
    _resp_msg_pattern -- AFC Response message pattern as JSON dictionary
    _grid_size        -- Copy of cfg.region.grid_size
    _min_lat          -- Copy of cfg.region.min_lat
    _max_lat          -- Copy of cfg.region.max_lat
    _min_lon          -- Copy of cfg.region.min_lon
    _max_lon          -- Copy of cfg.region.max_lon
    _default_height   -- AP height to use when there is no randomization
    _channels_20mhz   -- Copy of cfg._channels_20mhz
    _randomize        -- True to choose AP positions randomly (uniformly or
                         according to population density). False to use request
                         index (to make caching possible)
    _random_height    -- Formula for random height in string form. Evaluated
                         with 'r' local value, randomly distributed in [0, 1]
    _population_db    -- Population density database name. None for uniform
    _db_conn          -- None or SQLite3 connection
    _db_cur           -- None or SQLite3 cursor

    """

    def __init__(self, cfg: Config, randomize: bool,
                 population_db: Optional[str],
                 req_msg_pattern: Optional[Dict[str, Any]]) -> None:
        """ Constructor

        Arguments:
        cfg             -- Config object
        randomize       -- True to choose AP positions randomly (uniformly or
                           according to population density). False to use
                           request index (to make caching possible)
        population_db   -- Population density database name. None for uniform
        req_msg_pattern -- Optional Request message pattern to use instead of
                           default
        """
        self._paths = cfg.paths
        self._req_msg_pattern = \
            req_msg_pattern or json.loads(cfg.req_msg_pattern)
        self._resp_msg_pattern = json.loads(cfg.resp_msg_pattern)
        path_set(path_get(self._req_msg_pattern, self._paths.reqs_in_msg)[0],
                 self._paths.ruleset_in_req, cfg.region.rulesest_id)
        path_set(path_get(self._req_msg_pattern, self._paths.reqs_in_msg)[0],
                 self._paths.cert_in_req, cfg.region.cert_id)
        path_set(path_get(self._resp_msg_pattern, self._paths.resps_in_msg)[0],
                 self._paths.ruleset_in_resp, cfg.region.rulesest_id)
        self._grid_size = cfg.region.grid_size
        self._min_lat = cfg.region.min_lat
        self._max_lat = cfg.region.max_lat
        self._min_lon = cfg.region.min_lon
        self._max_lon = cfg.region.max_lon
        self._default_height = \
            path_get(
                path_get(self._req_msg_pattern, self._paths.reqs_in_msg)[0],
                self._paths.height_in_req)
        self._channels_20mhz = cfg.channels_20mhz
        self._randomize = randomize
        self._random_height = cfg.region.random_height
        self._population_db = population_db
        self._db_conn: Optional[sqlite3.Connection] = None
        self._db_cur: Optional[sqlite3.Cursor] = None

    def request_msg(self, req_indices=Union[int, List[int]]) -> Dict[str, Any]:
        """ Generate AFC Request message for given request index range

        Arguments:
        req_indices -- Request index or list of request indices
        Returns Request message
        """
        if isinstance(req_indices, int):
            req_indices = [req_indices]
        assert not isinstance(req_indices, int)

        # Whole message to be
        msg = copy.deepcopy(self._req_msg_pattern)
        pattern_req = path_get(msg, self._paths.reqs_in_msg)[0]
        # List of requests
        reqs = []
        for idx_in_msg, req_idx in enumerate(req_indices):
            # Individual request
            req = copy.deepcopy(pattern_req) if len(req_indices) > 1 \
                else pattern_req

            path_set(req, self._paths.id_in_req, self._req_id(req_idx,
                                                              idx_in_msg))
            path_set(req, self._paths.serial_in_req, self._serial(req_idx))
            lat, lon, height = self._get_position(req_idx=req_idx)
            path_set(req, self._paths.lat_in_req, lat)
            path_set(req, self._paths.lon_in_req, lon)
            path_set(req, self._paths.height_in_req, height)
            reqs.append(req)
        path_set(msg, self._paths.reqs_in_msg, reqs)
        return msg

    def response_msg(self, req_idx: int) -> Dict[str, Any]:
        """ Generate (fake) AFC Response message (for rcache - hence
        single-item)

        Arguments:
        req_idx -- Request index
        Returns Fake AFC Response message
        """
        # AFC Response message to be
        msg = copy.deepcopy(self._resp_msg_pattern)
        # Response inside it
        resp = path_get(msg, self._paths.resps_in_msg)[0]
        path_set(resp, self._paths.id_in_resp, self._req_id(req_idx, 0))
        # To add uniqueness to message - set 20MHz channels according to bits
        # in request index binary representation
        channels = []
        powers = []
        chan_idx = 0
        while req_idx:
            if req_idx & 1:
                channels.append(self._channels_20mhz[chan_idx])
                powers.append(30.)
            chan_idx += 1
            req_idx //= 2
        path_set(resp, self._paths.var_chans_in_resp, channels)
        path_set(resp, self._paths.var_pwrs_in_resp, powers)
        return msg

    def _req_id(self, req_idx: int, idx_in_msg: int) -> str:
        """ Request ID in message for given request index

        Arguments:
        req_idx    -- Request index
        idx_in_msg -- Index of request in AFC message
        Returns Request ID to use
        """
        unused_argument(idx_in_msg)
        return str(req_idx)

    def _serial(self, req_idx: int) -> str:
        """ AP Serial Number for given request index """
        return f"AFC_LOAD_{req_idx:08}"

    def _get_position(self, req_idx: int) -> Tuple[float, float, float]:
        """ Returns (lat_deg, lon_deg, height_m) position for given request
        index """
        height = \
            eval(self._random_height, None, {"r": random.uniform(0, 1)}) \
            if self._randomize else self._default_height
        if self._population_db is None:
            if self._randomize:
                return (random.uniform(self._min_lat, self._max_lat),
                        random.uniform(self._min_lon, self._max_lon),
                        height)
            return (self._min_lat +
                    (req_idx // self._grid_size) *
                    (self._max_lat - self._min_lat) / self._grid_size,
                    self._min_lon +
                    (req_idx % self._grid_size) *
                    (self._max_lon - self._min_lon) / self._grid_size,
                    height)
        if self._db_conn is None:
            self._db_conn = \
                sqlite3.connect(f"file:{self._population_db}?mode=ro",
                                uri=True)
            self._db_cur = self._db_conn.cursor()
        assert self._db_cur is not None
        cumulative_density = random.uniform(0, 1) if self._randomize \
            else req_idx / (self._grid_size * self._grid_size)
        rows = \
            self._db_cur.execute(
                f"SELECT min_lat, max_lat, min_lon, max_lon "
                f"FROM population_density "
                f"WHERE cumulative_density >= {cumulative_density} "
                f"ORDER BY cumulative_density "
                f"LIMIT 1")
        min_lat, max_lat, min_lon, max_lon = rows.fetchall()[0]
        if self._randomize:
            return (random.uniform(min_lat, max_lat),
                    random.uniform(min_lon, max_lon), height)
        return ((min_lat + max_lat) / 2, (min_lon + max_lon) / 2, height)


class RestDataHandlerBase:
    """ Base class for generate/process REST API request/response data payloads
    """

    def __init__(self, cfg: Config, randomize: bool = False,
                 population_db: Optional[str] = None,
                 req_msg_pattern: Optional[Dict[str, Any]] = None) -> None:
        """ Constructor

        Arguments:
        cfg             -- Config object
        randomize       -- True to choose AP positions randomly (uniformly or
                           according to population density). False to use
                           request index (to make caching possible)
        population_db   -- Population density database name. None for uniform
        req_msg_pattern -- Optional Request message pattern to use instead of
                           default
        """
        self.cfg = cfg
        self.afc_req_resp_gen = \
            AfcReqRespGenerator(
                cfg=cfg, randomize=randomize, population_db=population_db,
                req_msg_pattern=req_msg_pattern)

    def make_req_data(self, req_indices: List[int]) -> bytes:
        """ Abstract method that generates REST API POST data payload

        Arguments:
        req_indices -- List of request indices to generate payload for
        Returns POST payload as byte string
        """
        unused_argument(req_indices)
        raise \
            NotImplementedError(
                f"{self.__class__}.make_req_data() must be implemented")

    def make_error_map(self, result_data: Optional[bytes]) -> Dict[int, str]:
        """ Virtual method for error map generation

        Arguments:
        result_data -- Response in form of optional bytes string
        Returns error map - dictionary of error messages, indexed by request
        indices. This default implementation returns empty dictionary
        """
        unused_argument(result_data)
        return {}

    def dry_result_data(self, batch: Optional[int]) -> bytes:
        """ Virtual method returning bytes string to pass to make_error_map on
        dry run. This default implementation returns empty string """
        unused_argument(batch)
        return b""


class PreloadRestDataHandler(RestDataHandlerBase):
    """ REST API data handler for 'preload' operation - i.e. Rcache update
    messages (no response)

    Private attributes:
    _hash_base -- MD5 hash computed over AFC Config, awaiting AFC Request tail
    """

    def __init__(self, cfg: Config, afc_config: Dict[str, Any],
                 req_msg_pattern: Optional[Dict[str, Any]] = None) -> None:
        """ Constructor

        Arguments:
        cfg             -- Config object
        afc_config      -- AFC Config that will be used in request hash
                           computation
        req_msg_pattern -- Optional Request message pattern to use instead of
                           default
        """
        self._hash_base = hashlib.md5()
        self._hash_base.update(json.dumps(afc_config,
                                          sort_keys=True).encode("utf-8"))
        super().__init__(cfg=cfg, req_msg_pattern=req_msg_pattern)

    def make_req_data(self, req_indices: List[int]) -> bytes:
        """ Generates REST API POST payload (RcacheUpdateReq - see
        rcache_models.py, not included here)

        Arguments:
        req_indices -- List of request indices to generate payload for
        Returns POST payload as byte string
        """
        rrks = []
        for req_idx in req_indices:
            req_msg = self.afc_req_resp_gen.request_msg(req_indices=[req_idx])
            resp_msg = self.afc_req_resp_gen.response_msg(req_idx=req_idx)
            hash_req = \
                copy.deepcopy(path_get(req_msg, self.cfg.paths.reqs_in_msg)[0])
            path_del(hash_req, self.cfg.paths.id_in_req)
            h = self._hash_base.copy()
            h.update(json.dumps(hash_req, sort_keys=True).encode("utf-8"))
            rrks.append({"afc_req": json.dumps(req_msg),
                         "afc_resp": json.dumps(resp_msg),
                         "req_cfg_digest": h.hexdigest()})
        return json.dumps({"req_resp_keys": rrks}).encode("utf-8")


class LoadRestDataHandler(RestDataHandlerBase):
    """ REST API data handler for 'load' operation - i.e. AFC Request/Response
    messages """

    def __init__(self, cfg: Config, randomize: bool,
                 population_db: Optional[str],
                 req_msg_pattern: Optional[Dict[str, Any]] = None) -> None:
        """ Constructor

        Arguments:
        cfg             -- Config object
        randomize       -- True to choose AP positions randomly (uniformly or
                           according to population density). False to use
                           request index (to make caching possible)
        population_db   -- Population density database name. None for uniform
        req_msg_pattern -- Optional Request message pattern to use instead of
                           default
        """
        super().__init__(cfg=cfg, randomize=randomize,
                         population_db=population_db,
                         req_msg_pattern=req_msg_pattern)

    def make_req_data(self, req_indices: List[int]) -> bytes:
        """ Generates REST API POST payload (AFC Request message)

        Arguments:
        req_indices -- Request indices to generate payload for
        Returns POST payload as byte string
        """
        return \
            json.dumps(
                self.afc_req_resp_gen.request_msg(req_indices=req_indices)).\
            encode("utf-8")

    def make_error_map(self, result_data: Optional[bytes]) -> Dict[int, str]:
        """ Generate error map for given AFC Response message

        Arguments:
        result_data -- AFC Response as byte string
        Returns error map - dictionary of error messages, indexed by request
        indices
        """
        paths = self.cfg.paths
        ret: Dict[int, str] = {}
        for resp in path_get(json.loads(result_data or b""),
                             paths.resps_in_msg):
            if path_get(resp, paths.code_in_resp):
                ret[int(path_get(resp, paths.id_in_resp))] = \
                    str(path_get(resp, paths.response_in_resp))
        return ret

    def dry_result_data(self, batch: Optional[int]) -> bytes:
        """ Returns byte string, containing AFC Response to parse on dry run
        """
        assert batch is not None
        resp_msg = json.loads(self.cfg.resp_msg_pattern)
        path_set(resp_msg, self.cfg.paths.resps_in_msg,
                 path_get(resp_msg, self.cfg.paths.resps_in_msg) * batch)
        return json.dumps(resp_msg).encode("utf-8")


# POST Worker request data and supplementary information
PostWorkerReqInfo = \
    NamedTuple("PostWorkerReqInfo",
               [
                # Request indices, contained in REST API request data
                ("req_indices", List[int]),
                # REST API Request data
                ("req_data", bytes)])


# GET Worker request data and supplementary information
GetWorkerReqInfo = \
    NamedTuple("GetWorkerReqInfo",
               [
                # Number of GET requests to send
                ("num_gets", int)])


# REST API request results
class WorkerResultInfo(NamedTuple):
    """ Data, returned by REST API workers in result queue """
    # Retries made (0 - from first attempt)
    retries: int
    # CPU time consumed by worker in nanoseconds. Negative if not available
    worker_cpu_consumed_ns: int
    # Time consumed in processing of current request in second
    req_time_spent_sec: float
    # Request indices. None for netload
    req_indices: Optional[List[int]] = None
    # REST API Response data (None if error or N/A)
    result_data: Optional[bytes] = None
    # Error message for failed requests, None for succeeded
    error_msg: Optional[str] = None
    # Optional request data
    req_data: Optional[bytes] = None


# Message from Tick worker for EMA rate computation
TickInfo = NamedTuple("TickInfo", [("tick", int)])

# Type for result queue items
ResultQueueDataType = Optional[Union[WorkerResultInfo, TickInfo]]


class Ticker:
    """ Second ticker (used for EMA computations). Puts TickInfo to result
    queue

    Private attributes:
    _worker -- Tick worker process (generates TickInfo once per second)
    """

    def __init__(self,
                 result_queue: "multiprocessing.Queue[ResultQueueDataType]") \
            -> None:
        """ Constructor

        Arguments:
        result_queue -- Queue for tick worker to put TickInfo objects
        """
        self._worker = \
            multiprocessing.Process(target=Ticker._tick_worker,
                                    kwargs={"result_queue": result_queue})
        self._worker.start()

    @classmethod
    def _tick_worker(
            cls, result_queue: "multiprocessing.Queue[ResultQueueDataType]") \
            -> None:
        """ Tick worker process

        Arguments:
        result_queue -- Queue to put TickInfo to
        """
        count = 0
        while True:
            time.sleep(1)
            count += 1
            result_queue.put(TickInfo(tick=count))

    def stop(self) -> None:
        """ Stops tick worker """
        self._worker.terminate()


class RateEma:
    """ Exponential Moving Average for rate of change

    Public attributes:
    rate_ema -- Rate Average computed on last tick

    Private attributes:
    _weight     -- Weight for EMA computation
    _prev_value -- Value on previous tick
    """

    def __init__(self, win_size_sec: float = 20) -> None:
        """ Constructor

        Arguments:
        result_queue -- Queue for tick worker to put TickInfo objects
        win_size_sec -- Averaging window size in seconds
        """
        self._weight = 2 / (win_size_sec + 1)
        self.rate_ema: float = 0
        self._prev_value: float = 0

    def on_tick(self, new_value: float) -> None:
        """ Call on arrival of TickInfo message

        Arguments:
        new_value - Measured data value on this tick
        """
        increment = new_value - self._prev_value
        self._prev_value = new_value
        self.rate_ema += self._weight * (increment - self.rate_ema)


class StatusPrinter:
    """ Prints status on a single line:

    Private attributes:
    _prev_len -- Length of previously printed line
    """

    def __init__(self) -> None:
        """ Constructor

        Arguments:
        enabled -- True if status print is enabled
        """
        self._prev_len = 0

    def pr(self, s: Optional[str] = None) -> None:
        """ Print string (if given) or cleans up after previous print """
        if s is None:
            if self._prev_len:
                print(" " * self._prev_len, flush=True)
                self._prev_len = 0
        else:
            print(s + " " * (max(0, self._prev_len - len(s))), end="\r",
                  flush=True)
            self._prev_len = len(s)


class ResultsProcessor:
    """ Prints important summary of request execution results

    Private attributes:
    _netload             -- True for netload testing, False for preload/load
                            testing
    _total_requests      -- Total number of individual requests (not batches)
                            that will be executed
    _result_queue        -- Queue with execution results, second ticks, Nones
                            for completed workers
    _status_period       -- Period of status printing (in terms of request
                            count), e.g. 1000 for once in 1000 requests, 0 to
                            not print status (except in the end)
    _rest_data_handler   -- Generator/interpreter of REST request/response
                            data. None for netload test
    _num_workers         -- Number of worker processes
    _err_dir             -- None or directory for failed requests
    _status_printer      -- StatusPrinter
    """

    def __init__(
            self, netload: bool, total_requests: int,
            result_queue: "multiprocessing.Queue[ResultQueueDataType]",
            num_workers: int, status_period: int,
            rest_data_handler: Optional[RestDataHandlerBase],
            err_dir: Optional[str]) -> None:
        """ Constructor

        Arguments:
        netload           -- True for netload testing, False for preload/load
                             testing
        total_requests    -- Total number of individual requests (not batches)
                             that will be executed
        result_queue      -- Queue with execution results, Nones for completed
                             workers
        num_workers       -- Number of worker processes
        status_period     -- Period of status printing (in terms of request
                             count), e.g. 1000 for once in 1000 requests, 0 to
                             not print status (except in the end)
        rest_data_handler -- Generator/interpreter of REST request/response
                             data
        err_dir           -- None or directory for failed requests
        """
        self._netload = netload
        self._total_requests = total_requests
        self._result_queue = result_queue
        self._status_period = status_period
        self._num_workers = num_workers
        self._rest_data_handler = rest_data_handler
        self._err_dir = err_dir
        self._status_printer = StatusPrinter()

    def process(self) -> None:
        """ Keep processing results until all worker will stop """
        start_time = datetime.datetime.now()
        requests_sent: int = 0
        requests_failed: int = 0
        retries: int = 0
        cpu_consumed_ns: int = 0
        time_spent_sec: float = 0
        req_rate_ema = RateEma()
        cpu_consumption_ema = RateEma()

        def status_message(intermediate: bool) -> str:
            """ Returns status message (intermediate or final) """
            now = datetime.datetime.now()
            elapsed = now - start_time
            elapsed_sec = elapsed.total_seconds()
            elapsed_str = re.sub(r"\.\d*$", "", str(elapsed))
            global_req_rate = requests_sent / elapsed_sec if elapsed_sec else 0

            cpu_consumption: float
            if cpu_consumed_ns < 0:
                cpu_consumption = -1
            elif intermediate:
                cpu_consumption = cpu_consumption_ema.rate_ema
            elif elapsed_sec:
                cpu_consumption = cpu_consumed_ns * 1e-9 / elapsed_sec
            else:
                cpu_consumption = 0

            if requests_sent:
                req_duration = time_spent_sec / requests_sent
                req_duration_str = \
                    f"{req_duration:.3g} sec" if req_duration > 0.1 \
                    else f"{req_duration * 1000:.3g} ms"
            else:
                req_duration_str = "unknown"
            ret = \
                f"{'Progress: ' if intermediate else ''}" \
                f"{requests_sent} requests completed " \
                f"({requests_sent * 100 / self._total_requests:.1f}%), " \
                f"{requests_failed} failed " \
                f"({requests_failed * 100 / (requests_sent or 1):.3f}%), " \
                f"{retries} retries made. " \
                f"{elapsed_str} elapsed, " \
                f"rate is {global_req_rate:.3f} req/sec " \
                f"(avg req proc time {req_duration_str})"
            if cpu_consumption >= 0:
                ret += f", CPU consumption is {cpu_consumption:.3f}"
            if intermediate and elapsed_sec and requests_sent:
                total_duration = \
                    datetime.timedelta(
                        seconds=self._total_requests * elapsed_sec /
                        requests_sent)
                eta_dt = start_time + total_duration
                tta_sec = int((eta_dt - now).total_seconds())
                tta: str
                if tta_sec < 60:
                    tta = f"{tta_sec} seconds"
                elif tta_sec < 3600:
                    tta = f"{tta_sec // 60} minutes"
                else:
                    tta_minutes = tta_sec // 60
                    tta = \
                        f"{tta_minutes // 60} hours {tta_minutes % 60} minutes"
                ret += f", current rate is " \
                    f"{req_rate_ema.rate_ema:.3f} req/sec. " \
                    f"ETA: {eta_dt.strftime('%X')} (in {tta})"
            return ret

        try:
            while True:
                result_info = self._result_queue.get()
                if result_info is None:
                    self._num_workers -= 1
                    if self._num_workers == 0:
                        break
                    continue
                if isinstance(result_info, TickInfo):
                    req_rate_ema.on_tick(requests_sent)
                    cpu_consumption_ema.on_tick(cpu_consumed_ns * 1e-9)
                    continue
                error_msg = result_info.error_msg
                if error_msg:
                    self._status_printer.pr()
                    if self._netload:
                        indices_clause = ""
                    else:
                        assert result_info.req_indices is not None
                        indices = \
                            ", ".join(str(i) for i in result_info.req_indices)
                        indices_clause = f" with indices ({indices})"
                    logging.error(
                        f"Request{indices_clause} failed: {error_msg}")

                error_map: Dict[int, str] = {}
                if self._rest_data_handler is not None:
                    if error_msg is None:
                        try:
                            error_map = \
                                self._rest_data_handler.make_error_map(
                                    result_data=result_info.result_data)
                        except Exception as ex:
                            error_msg = f"Error decoding message " \
                                f"{result_info.result_data!r}: {repr(ex)}"
                    for idx, error_msg in error_map.items():
                        self._status_printer.pr()
                        logging.error(f"Request {idx} failed: {error_msg}")

                prev_sent = requests_sent
                num_requests = \
                    1 if self._netload \
                    else len(cast(List[int], result_info.req_indices))
                requests_sent += num_requests
                retries += result_info.retries
                if result_info.error_msg:
                    requests_failed += num_requests
                else:
                    requests_failed += len(error_map)
                if self._err_dir and result_info.req_data and \
                        (result_info.error_msg or error_map):
                    try:
                        filename = \
                            os.path.join(
                                self._err_dir,
                                datetime.datetime.now().strftime(
                                    "err_req_%y%m%d_%H%M%S_%f.json"))
                        with open(filename, "wb") as f:
                            f.write(result_info.req_data)
                    except OSError as ex:
                        error(f"Failed to write failed request file "
                              f"'{filename}': {repr(ex)}")
                cpu_consumed_ns += result_info.worker_cpu_consumed_ns
                time_spent_sec += result_info.req_time_spent_sec

                if self._status_period and \
                        ((prev_sent // self._status_period) !=
                         (requests_sent // self._status_period)):
                    self._status_printer.pr(status_message(intermediate=True))
        finally:
            self._status_printer.pr()
            logging.info(status_message(intermediate=False))

    def _print(self, s: str, newline: bool, is_error: bool) -> None:
        """ Print message

        Arguments:
        s       -- Message to print
        newline -- True to go to new line, False to remain on same line
        """
        if newline:
            self._status_printer.pr()
            (logging.error if is_error else logging.info)(s)
        else:
            self._status_printer.pr(s)


def post_req_worker(
        url: str, retries: int, backoff: float, dry: bool,
        post_req_queue: multiprocessing.Queue,
        result_queue: "multiprocessing.Queue[ResultQueueDataType]",
        dry_result_data: Optional[bytes], use_requests: bool,
        return_requests: bool = False, delay_sec: float = 0) -> None:
    """ REST API POST worker

    Arguments:
    url             -- REST API URL to send POSTs to
    retries         -- Number of retries
    backoff         -- Initial backoff window in seconds
    dry             -- True to dry run
    post_req_queue  -- Request queue. Elements are PostWorkerReqInfo objects
                       corresponding to single REST API post or None to stop
                       operation
    result_queue    -- Result queue. Elements added are WorkerResultInfo for
                       operation results, None to signal that worker finished
    use_requests    -- True to use requests, False to use urllib.request
    return_requests -- True to return requests in WorkerResultInfo
    delay_sec       -- Delay start by this number of seconds
    """
    try:
        time.sleep(delay_sec)
        session: Optional[requests.Session] = \
            requests.Session() if use_requests and (not dry) else None
        has_proc_time = hasattr(time, "process_time_ns")
        prev_proc_time_ns = time.process_time_ns() if has_proc_time else 0
        while True:
            req_info: PostWorkerReqInfo = post_req_queue.get()
            if req_info is None:
                result_queue.put(None)
                return

            start_time = datetime.datetime.now()
            error_msg = None
            if dry:
                result_data = dry_result_data
                attempts = 1
            else:
                result_data = None
                last_error: Optional[str] = None
                for attempt in range(retries + 1):
                    if use_requests:
                        assert session is not None
                        try:
                            resp = \
                                session.post(
                                    url=url, data=req_info.req_data,
                                    headers={
                                        "Content-Type":
                                        "application/json; charset=utf-8",
                                        "Content-Length":
                                        str(len(req_info.req_data))},
                                    timeout=180)
                            if not resp.ok:
                                last_error = \
                                    f"{resp.status_code}: {resp.reason}"
                                continue
                            result_data = resp.content
                            break
                        except requests.RequestException as ex:
                            last_error = repr(ex)
                    else:
                        req = urllib.request.Request(url)
                        req.add_header("Content-Type",
                                       "application/json; charset=utf-8")
                        req.add_header("Content-Length",
                                       str(len(req_info.req_data)))
                        try:
                            with urllib.request.urlopen(
                                    req, req_info.req_data, timeout=180) as f:
                                result_data = f.read()
                            break
                        except (urllib.error.HTTPError, urllib.error.URLError,
                                urllib.error.ContentTooShortError, OSError) \
                                as ex:
                            last_error = repr(ex)
                    time.sleep(
                        random.uniform(0, (1 << attempt)) * backoff)
                else:
                    error_msg = last_error
                attempts = attempt + 1
            new_proc_time_ns = time.process_time_ns() if has_proc_time else 0
            result_queue.put(
                WorkerResultInfo(
                    req_indices=req_info.req_indices, retries=attempts - 1,
                    result_data=result_data, error_msg=error_msg,
                    worker_cpu_consumed_ns=(new_proc_time_ns -
                                            prev_proc_time_ns)
                    if has_proc_time else -1,
                    req_time_spent_sec=(datetime.datetime.now() - start_time).
                    total_seconds(),
                    req_data=req_info.req_data if return_requests and (not dry)
                    else None))
            prev_proc_time_ns = new_proc_time_ns
    except Exception as ex:
        logging.error(f"Worker failed: {repr(ex)}\n"
                      f"{traceback.format_exc()}")
        result_queue.put(None)


def get_req_worker(
        url: str, expected_code: Optional[int], retries: int, backoff: float,
        dry: bool, get_req_queue: multiprocessing.Queue,
        result_queue: "multiprocessing.Queue[ResultQueueDataType]",
        use_requests: bool) -> None:
    """ REST API GET worker

    Arguments:
    url           -- REST API URL to send POSTs to
    expected_code -- None or expected non-200 status code
    retries       -- Number of retries
    backoff       -- Initial backoff window in seconds
    dry           -- True to dry run
    get_req_queue -- Request queue. Elements are GetWorkerReqInfo objects
                     corresponding to bunch of single REST API GETs or None to
                     stop operation
    result_queue  -- Result queue. Elements added are WorkerResultInfo for
                     operation results, None to signal that worker finished
    use_requests  -- True to use requests, False to use urllib.request
    """
    try:
        if expected_code is None:
            expected_code = http.HTTPStatus.OK.value
        has_proc_time = hasattr(time, "process_time_ns")
        prev_proc_time_ns = time.process_time_ns() if has_proc_time else 0
        session: Optional[requests.Session] = \
            requests.Session() if use_requests and (not dry) else None
        while True:
            req_info: GetWorkerReqInfo = get_req_queue.get()
            if req_info is None:
                result_queue.put(None)
                return
            start_time = datetime.datetime.now()
            for _ in range(req_info.num_gets):
                error_msg = None
                if dry:
                    attempts = 1
                else:
                    last_error: Optional[str] = None
                    for attempt in range(retries + 1):
                        if use_requests:
                            assert session is not None
                            try:
                                resp = session.get(url=url, timeout=30)
                                if resp.status_code != expected_code:
                                    last_error = \
                                        f"{resp.status_code}: {resp.reason}"
                                    continue
                                break
                            except requests.RequestException as ex:
                                last_error = repr(ex)
                        else:
                            req = urllib.request.Request(url)
                            status_code: Optional[int] = None
                            status_reason: str = ""
                            try:
                                with urllib.request.urlopen(req, timeout=30):
                                    pass
                                status_code = http.HTTPStatus.OK.value
                                status_reason = http.HTTPStatus.OK.name
                            except urllib.error.HTTPError as http_ex:
                                status_code = http_ex.code
                                status_reason = http_ex.reason
                            except (urllib.error.URLError,
                                    urllib.error.ContentTooShortError) as ex:
                                last_error = repr(ex)
                            if status_code is not None:
                                if status_code == expected_code:
                                    break
                                last_error = f"{status_code}: {status_reason}"
                        time.sleep(
                            random.uniform(0, (1 << attempt)) * backoff)
                    else:
                        error_msg = last_error
                    attempts = attempt + 1
                new_proc_time_ns = \
                    time.process_time_ns() if has_proc_time else 0
                result_queue.put(
                    WorkerResultInfo(
                        retries=attempts - 1, error_msg=error_msg,
                        worker_cpu_consumed_ns=(new_proc_time_ns -
                                                prev_proc_time_ns)
                        if has_proc_time else -1,
                        req_time_spent_sec=(datetime.datetime.now() -
                                            start_time).
                        total_seconds(),))
                prev_proc_time_ns = new_proc_time_ns
    except Exception as ex:
        logging.error(f"Worker failed: {repr(ex)}\n"
                      f"{traceback.format_exc()}")
        result_queue.put(None)


def get_idx_range(idx_range_arg: str) -> Tuple[int, int]:
    """ Parses --idx_range command line parameter to index range tuple """
    parts = idx_range_arg.split("-", maxsplit=1)
    try:
        ret = (0, int(parts[0])) if len(parts) == 1 \
            else (int(parts[0]), int(parts[1]))
        error_if(ret[0] >= ret[1], f"Invalid index range: {idx_range_arg}")
        return ret
    except ValueError as ex:
        error(f"Invalid index range syntax: {repr(ex)}")
    return (0, 0)  # Appeasing pylint, will never happen


def producer_worker(
        count: Optional[int], batch: int, parallel: int,
        req_queue: multiprocessing.Queue, netload: bool = False,
        min_idx: Optional[int] = None, max_idx: Optional[int] = None,
        rest_data_handler: Optional[RestDataHandlerBase] = None) -> None:
    """ POST Producer (request queue filler)

    Arguments:
    batch             -- Batch size (number of requests per queue element)
    min_idx           -- Minimum request index. None for netload
    max_idx           -- Aftremaximum request index. None for netload
    count             -- Optional count of requests to send. None means that
                         requests will be sent sequentially according to range.
                         If specified - request indices will be randomized
    parallel          -- Number of worker processes to use
    dry               -- Dry run
    rest_data_handler -- Generator/interpreter of REST request/response data
    req_queue         -- Requests queue to fill
    """
    try:
        if netload:
            assert count is not None
            for min_count in range(0, count, batch):
                req_queue.put(
                    GetWorkerReqInfo(num_gets=min(count - min_count, batch)))
        elif count is not None:
            assert (min_idx is not None) and (max_idx is not None) and \
                (rest_data_handler is not None)
            for min_count in range(0, count, batch):
                req_indices = \
                    [random.randrange(min_idx, max_idx)
                     for _ in range(min(count - min_count, batch))]
                req_queue.put(
                    PostWorkerReqInfo(
                        req_indices=req_indices,
                        req_data=rest_data_handler.make_req_data(req_indices)))
        else:
            assert (min_idx is not None) and (max_idx is not None) and \
                (rest_data_handler is not None)
            for min_req_idx in range(min_idx, max_idx, batch):
                req_indices = list(range(min_req_idx,
                                         min(min_req_idx + batch, max_idx)))
                req_queue.put(
                    PostWorkerReqInfo(
                        req_indices=req_indices,
                        req_data=rest_data_handler.make_req_data(req_indices)))
    except Exception as ex:
        error(f"Producer terminated: {repr(ex)}")
    finally:
        for _ in range(parallel):
            req_queue.put(None)


def run(url: str, parallel: int, backoff: int, retries: int, dry: bool,
        status_period: int, batch: int,
        rest_data_handler: Optional[RestDataHandlerBase] = None,
        netload_target: Optional[str] = None,
        expected_code: Optional[int] = None, min_idx: Optional[int] = None,
        max_idx: Optional[int] = None, count: Optional[int] = None,
        use_requests: bool = False, err_dir: Optional[str] = None,
        ramp_up: Optional[float] = None, randomize: Optional[bool] = None,
        population_db: Optional[str] = None) -> None:
    """ Run the POST operation

    Arguments:
    rest_data_handler -- REST API payload data generator/interpreter. None for
                         netload
    url               -- REST API URL to send POSTs to (GET in case of netload)
    parallel          -- Number of worker processes to use
    backoff           -- Initial size of backoff windows in seconds
    retries           -- Number of retries
    dry               -- True to dry run
    status_period     -- Period (in terms of count) of status message prints (0
                         to not at all)
    batch             -- Batch size (number of requests per element of request
                         queue)
    netload_target    -- None for POST test, tested destination for netload
                         test
    expected_code     -- None or non-200 netload test HTTP status code
    min_idx           -- Minimum request index. None for netload test
    max_idx           -- Aftermaximum request index. None for netload test
    count             -- Optional count of requests to send. None means that
                         requests will be sent sequentially according to range.
                         If specified - request indices will be randomized
    use_requests      -- Use requests to send requests (default is to use
                         urllib.request)
    err_dir           -- None or directory for failed requests
    ramp_up           -- Ramp up parallel streams for this number of seconds
    randomize         -- True to random points, False for predefined points,
                         None if irrelevant. Only used in banner printing
    population_db     -- Population database file name or None. Only used for
                         banner printing
    """
    error_if(use_requests and ("requests" not in sys.modules),
             "'requests' Python3 module have to be installed to use "
             "'--no_reconnect' option")
    total_requests = count if count is not None \
        else (cast(int, max_idx) - cast(int, min_idx))
    if netload_target:
        logging.info(f"Netload test of {netload_target}")
    logging.info(f"URL: {'N/A' if dry else url}")
    logging.info(f"Streams: {parallel}")
    logging.info(f"Backoff: {backoff} sec, {retries} retries")
    if not netload_target:
        logging.info(f"Index range: {min_idx:_} - {max_idx:_}")
        logging.info(f"Batch size: {batch}")
    logging.info(f"Requests to send: {total_requests:_}")
    logging.info(f"Nonreconnect mode: {use_requests}")
    if ramp_up is not None:
        logging.info(f"Ramp up for: {ramp_up} seconds")
    if status_period:
        logging.info(f"Intermediate status is printed every "
                     f"{status_period} requests completed")
    else:
        logging.info("Intermediate status is not printed")
    if err_dir:
        logging.info(f"Directory for failed requests: {err_dir}")
    if randomize is not None:
        logging.info(
            f"Point selection is {'random' if randomize else 'predefined'}")
    if population_db:
        logging.info(f"Point density chosen according to population database: "
                     f"{population_db}")
    if dry:
        logging.info("Dry mode")

    req_queue: multiprocessing.Queue = multiprocessing.Queue()
    result_queue: "multiprocessing.Queue[ResultQueueDataType]" = \
        multiprocessing.Queue()
    workers: List[multiprocessing.Process] = []
    original_sigint_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
    ticker: Optional[Ticker] = None
    if err_dir:
        try:
            os.makedirs(err_dir, exist_ok=True)
        except OSError as ex:
            error(f"Failed to create directory '{err_dir}': {repr(ex)}")
    try:
        req_worker_kwargs: Dict[str, Any] = {
            "url": url, "backoff": backoff, "retries": retries,
            "dry": dry, "result_queue": result_queue,
            "use_requests": use_requests}
        req_worker: Callable
        if netload_target:
            req_worker = get_req_worker
            req_worker_kwargs["get_req_queue"] = req_queue
            req_worker_kwargs["expected_code"] = expected_code
        else:
            assert rest_data_handler is not None
            req_worker = post_req_worker
            req_worker_kwargs["post_req_queue"] = req_queue
            req_worker_kwargs["dry_result_data"] = \
                rest_data_handler.dry_result_data(batch)
            req_worker_kwargs["return_requests"] = err_dir is not None
        for idx in range(parallel):
            kwargs = dict(req_worker_kwargs)
            if ramp_up is not None:
                kwargs["delay_sec"] = \
                    (idx * ramp_up / (parallel - 1)) if idx else 0

            workers.append(
                multiprocessing.Process(target=req_worker, kwargs=kwargs))
            workers[-1].start()
        workers.append(
            multiprocessing.Process(
                target=producer_worker,
                kwargs={"netload": netload_target is not None,
                        "min_idx": min_idx, "max_idx": max_idx,
                        "count": count, "batch": batch,
                        "parallel": parallel,
                        "rest_data_handler": rest_data_handler,
                        "req_queue": req_queue}))
        workers[-1].start()
        ticker = Ticker(result_queue)
        signal.signal(signal.SIGINT, original_sigint_handler)
        results_processor = \
            ResultsProcessor(
                netload=netload_target is not None,
                total_requests=total_requests, result_queue=result_queue,
                num_workers=parallel, status_period=status_period,
                rest_data_handler=rest_data_handler, err_dir=err_dir)
        results_processor.process()
        for worker in workers:
            worker.join()
    finally:
        for worker in workers:
            if worker.is_alive():
                worker.terminate()
        if ticker:
            ticker.stop()


def wait_rcache_flush(cfg: Config, args: Any,
                      service_discovery: ServiceDiscovery) -> None:
    """ Waiting for rcache preload stuff flushing to DB

    Arguments:
    cfg               -- Config object
    args              -- Parsed command line arguments
    service_discovery -- ServiceDiscovery object
    """
    logging.info("Waiting for updates to flush to DB")
    start_time = datetime.datetime.now()
    start_queue_len: Optional[int] = None
    prev_queue_len: Optional[int] = None
    status_printer = StatusPrinter()
    rcache_status_url = \
        service_discovery.get_url(base_url=cfg.rest_api.rcache_status.url,
                                  param_host=args.rcache)
    while True:
        try:
            with urllib.request.urlopen(rcache_status_url, timeout=30) as f:
                rcache_status = json.loads(f.read())
        except (urllib.error.HTTPError, urllib.error.URLError) as ex:
            error(f"Error retrieving Rcache status '{cfg.region.rulesest_id}' "
                  f"using URL get_config.url : {repr(ex)}")
        queue_len: int = rcache_status["update_queue_len"]
        if not rcache_status["update_queue_len"]:
            status_printer.pr()
            break
        if start_queue_len is None:
            start_queue_len = prev_queue_len = queue_len
        elif (args.status_period is not None) and \
                ((cast(int, prev_queue_len) - queue_len) >=
                 args.status_period):
            now = datetime.datetime.now()
            elapsed_sec = (now - start_time).total_seconds()
            written = start_queue_len - queue_len
            total_duration = \
                datetime.timedelta(
                    seconds=start_queue_len * elapsed_sec / written)
            eta_dt = start_time + total_duration
            status_printer.pr(
                f"{queue_len} records not yet written. "
                f"Write rate {written / elapsed_sec:.2f} rec/sec. "
                f"ETA {eta_dt.strftime('%X')} (in "
                f"{int((eta_dt - now).total_seconds()) // 60} minutes)")
        time.sleep(1)
    status_printer.pr()
    now = datetime.datetime.now()
    elapsed_sec = (now - start_time).total_seconds()
    elapsed_str = re.sub(r"\.\d*$", "", str((now - start_time)))
    msg = f"Flushing took {elapsed_str}"
    if start_queue_len is not None:
        msg += f". Flush rate {start_queue_len / elapsed_sec: .2f} rec/sec"
    logging.info(msg)


def get_afc_config(cfg: Config, args: Any,
                   service_discovery: ServiceDiscovery) -> Dict[str, Any]:
    """ Retrieve AFC Config for configured Ruleset ID

    Arguments:
    cfg                -- Config object
    args               -- Parsed command line arguments
    service_discovery  -- ServiceDiscovery object
    Returns AFC Config as dictionary
    """
    get_config_url = \
        service_discovery.get_url(base_url=cfg.rest_api.get_config.url,
                                  param_host=getattr(args, "rat_server", None))
    try:
        get_config_url += cfg.region.rulesest_id
        with urllib.request.urlopen(get_config_url, timeout=30) as f:
            afc_config_str = f.read().decode('utf-8')
    except (urllib.error.HTTPError, urllib.error.URLError) as ex:
        error(f"Error retrieving AFC Config for Ruleset ID "
              f"'{cfg.region.rulesest_id}' using URL get_config.url: "
              f"{repr(ex)}")
    try:
        return json.loads(afc_config_str)
    except json.JSONDecodeError as ex:
        error(f"Error decoding AFC Config JSON: {repr(ex)}")
        return {}  # Unreachable code to appease pylint


def patch_json(patch_arg: Optional[List[str]], json_dict: Dict[str, Any],
               data_type: str, new_type: Optional[str] = None) \
        -> Dict[str, Any]:
    """ Modify JSON object with patches from command line

    Arguments:
    patch_arg -- Optional list of FIELD1=VALUE1[,FIELD2=VALUE2...] patches
    json_dict -- JSON dictionary to modify
    data_type -- Patch of what - to be used in error messages
    new_type  -- None or type of new values for previously nonexistent keys
    returns modified dictionary
    """
    if not patch_arg:
        return json_dict
    ret = copy.deepcopy(json_dict)
    for patches in patch_arg:
        for patch in patches.split(";"):
            error_if("=" not in patch,
                     f"Invalid syntax: {data_type} setting '{patch}' doesn't "
                     f"have '='")
            field, value = patch.split("=", 1)
            new_key = False
            super_obj: Any = None
            obj: Any = ret
            last_idx: Any = None
            idx: Any
            for idx in field.split("."):
                super_obj = obj
                if re.match(r"^\d+$", idx):
                    int_idx = int(idx)
                    error_if(not isinstance(obj, list),
                             f"Integer index '{idx}' in {data_type} setting "
                             f"'{patch}' is applied to nonlist entity")
                    error_if(not (0 <= int_idx < len(obj)),
                             f"Integer index '{idx}' in {data_type} setting "
                             f"'{patch}' is outside of [0, {len(obj)}[ valid "
                             f"range")
                    idx = int_idx
                else:
                    error_if(not isinstance(obj, dict),
                             f"Key '{idx}' in {data_type} setting '{patch}' "
                             f"can't be applied to nondictionary entity")
                    if idx not in obj:
                        error_if(not new_type,
                                 f"Key '{idx}' of setting '{patch}' not found "
                                 f"in {data_type}")
                        obj[idx] = {}
                        new_key = True
                obj = obj[idx]
                last_idx = idx
            error_if(
                (isinstance(obj, dict) and (not new_key)) or
                (isinstance(obj, list) and (not value.startswith("["))),
                f"'{field}' of {data_type} setting '{patch}' does not address "
                f"scalar value")
            try:
                if isinstance(obj, int) or (new_key and (new_type == "int")):
                    try:
                        super_obj[last_idx] = int(value)
                    except ValueError:
                        if new_key and (new_type == "int"):
                            raise
                        super_obj[last_idx] = float(value)
                elif isinstance(obj, float) or \
                        (new_key and (new_type == "float")):
                    super_obj[last_idx] = float(value)
                elif isinstance(obj, bool) or \
                        (new_key and (new_type == "bool")):
                    if value.lower() in ("1", "y", "t", "yes", "true", "+"):
                        super_obj[last_idx] = True
                    elif value.lower() in ("0", "n", "f", "no", "false", "-"):
                        super_obj[last_idx] = False
                    else:
                        raise TypeError(f"'{value}' is bot a valid boolean "
                                        "representation")
                elif isinstance(obj, list) or \
                        (new_key and (new_type == "list")):
                    super_obj[last_idx] = json.loads(value)
                else:
                    super_obj[last_idx] = value
            except (TypeError, ValueError, json.JSONDecodeError) as ex:
                error(f"'{value}' of {data_type} setting '{patch}' has "
                      f"invalid type/formatting: {repr(ex)}")
    return ret


def patch_req(cfg: Config, args_req: Optional[List[str]]) -> Dict[str, Any]:
    """ Get AFC Request pattern, patched according to --req switch

    Arguments:
    cfg      -- Config object
    args_req -- Optional --req switch value
    Returns patched AFC Request pattern
    """
    req_msg_dict = json.loads(cfg.req_msg_pattern)
    req_dict = path_get(obj=req_msg_dict, path=cfg.paths.reqs_in_msg)[0]
    req_dict = patch_json(patch_arg=args_req, json_dict=req_dict,
                          data_type="AFC Request")
    path_set(obj=req_msg_dict, path=cfg.paths.reqs_in_msg, value=[req_dict])
    return req_msg_dict


def do_preload(cfg: Config, args: Any) -> None:
    """ Execute "preload" command.

    Arguments:
    cfg  -- Config object
    args -- Parsed command line arguments
    """
    if args.dry:
        afc_config = {}
        worker_url = ""
    else:
        service_discovery = \
            ServiceDiscovery.create(cfg=cfg, compose_project=args.comp_proj,
                                    k3d_cluster=args.k3d)
        if args.protect_cache:
            try:
                with urllib.request.urlopen(
                        urllib.request.Request(
                            service_discovery.get_url(
                                base_url=cfg.rest_api.protect_rcache.url,
                                param_host=args.rcache),
                            method="POST"),
                        timeout=30):
                    pass
            except (urllib.error.HTTPError, urllib.error.URLError) as ex:
                error(f"Error attempting to protect Rcache from invalidation "
                      f"using protect_rcache.url: {repr(ex)}")

        worker_url = \
            service_discovery.get_url(
                base_url=cfg.rest_api.update_rcache.url,
                param_host=args.rcache)

        afc_config = get_afc_config(cfg=cfg, args=args,
                                    service_discovery=service_discovery)

    min_idx, max_idx = get_idx_range(args.idx_range)

    run(rest_data_handler=PreloadRestDataHandler(
            cfg=cfg, afc_config=afc_config,
            req_msg_pattern=patch_req(cfg=cfg, args_req=args.req)),
        url=worker_url, parallel=args.parallel, backoff=args.backoff,
        retries=args.retries, dry=args.dry, batch=args.batch, min_idx=min_idx,
        max_idx=max_idx, status_period=args.status_period,
        use_requests=args.no_reconnect)

    if not args.dry:
        wait_rcache_flush(cfg=cfg, args=args,
                          service_discovery=service_discovery)


def do_load(cfg: Config, args: Any) -> None:
    """ Execute "load" command

    Arguments:
    cfg  -- Config object
    args -- Parsed command line arguments
    """
    if args.dry:
        worker_url = ""
    else:
        service_discovery = \
            ServiceDiscovery.create(cfg=cfg, compose_project=args.comp_proj,
                                    k3d_cluster=args.k3d)
        worker_url = \
            service_discovery.get_url(
                base_url=cfg.rest_api.afc_req_dispatcher.url if args.dispatcher
                else cfg.rest_api.afc_req.url, param_host=args.afc,
                protocol=getattr(Protocol, args.dispatcher) if args.dispatcher
                else None)
        parsed_url = urllib.parse.urlparse(worker_url)
        query = \
            "&".join(
                q for q, pred in [(parsed_url.query, parsed_url.query),
                                  ("nocache=True", args.no_cache),
                                  ("debug=True", args.debug)]
                if pred)
        worker_url = parsed_url._replace(query=query).geturl()
    min_idx, max_idx = get_idx_range(args.idx_range)
    run(rest_data_handler=LoadRestDataHandler(
            cfg=cfg, randomize=args.random, population_db=args.population,
            req_msg_pattern=patch_req(cfg=cfg, args_req=args.req)),
        url=worker_url, parallel=args.parallel, backoff=args.backoff,
        retries=args.retries, dry=args.dry, batch=args.batch, min_idx=min_idx,
        max_idx=max_idx, status_period=args.status_period, count=args.count,
        use_requests=args.no_reconnect, err_dir=args.err_dir,
        ramp_up=args.ramp_up, randomize=args.random,
        population_db=args.population)


def do_netload(cfg: Config, args: Any) -> None:
    """ Execute "netload" command.

    Arguments:
    cfg  -- Config object
    args -- Parsed command line arguments
    """
    expected_code: Optional[int] = None
    worker_url = ""
    if not args.dry:
        service_discovery = \
            ServiceDiscovery.create(cfg=cfg, compose_project=args.comp_proj,
                                    k3d_cluster=args.k3d)
        if args.target is None:
            if args.dispatcher and (not args.rcache):
                args.target = "dispatcher"
            elif args.afc and (not (args.dispatcher or args.rcache)):
                args.target = "msghnd"
            elif args.rcache and (not (args.dispatcher or args.afc)):
                args.target = "rcache"
            else:
                error("'--target' argument must be explicitly specified")
        if args.target == "rcache":
            worker_url = \
                service_discovery.get_url(base_url=cfg.rest_api.rcache_get.url,
                                          param_host=args.rcache)
            expected_code = cfg.rest_api.rcache_get.get("code")
        elif args.target == "msghnd":
            worker_url = \
                service_discovery.get_url(base_url=cfg.rest_api.msghnd_get.url,
                                          param_host=args.afc)
            expected_code = cfg.rest_api.msghnd_get.get("code")
        else:
            assert args.target == "dispatcher"
            worker_url = \
                service_discovery.get_url(
                    base_url=cfg.rest_api.afc_req_dispatcher.url
                    if args.dispatcher else cfg.rest_api.afc_req.url,
                    param_host=args.afc,
                    protocol=getattr(Protocol, args.dispatcher)
                    if args.dispatcher else None)
            expected_code = cfg.rest_api.dispatcher_get.get("code")
    run(netload_target=args.target, url=worker_url,
        parallel=args.parallel, backoff=args.backoff, retries=args.retries,
        dry=args.dry, batch=1000, expected_code=expected_code,
        status_period=args.status_period, count=args.count,
        use_requests=args.no_reconnect)


def do_cache(cfg: Config, args: Any) -> None:
    """ Execute "cache" command.

    Arguments:
    cfg  -- Config object
    args -- Parsed command line arguments
    """
    error_if(args.protect and args.unprotect,
             "--protect and --unprotect are mutually exclusive")
    error_if(not (args.protect or args.unprotect or args.invalidate),
             "Nothing to do")
    service_discovery = \
        ServiceDiscovery.create(cfg=cfg, compose_project=args.comp_proj,
                                k3d_cluster=args.k3d)
    json_data: Any
    for arg, attr, json_data in \
            [("protect", "protect_rcache", None),
             ("invalidate", "invalidate_rcache", {}),
             ("unprotect", "unprotect_rcache", None)]:
        try:
            if not getattr(args, arg):
                continue
            data: Optional[bytes] = None
            url = \
                service_discovery.get_url(
                    base_url=getattr(cfg.rest_api, attr).url,
                    param_host=args.rcache)
            req = urllib.request.Request(url, method="POST")
            if json_data is not None:
                data = json.dumps(json_data).encode(encoding="ascii")
                req.add_header("Content-Type", "application/json")
            urllib.request.urlopen(req, data, timeout=30)
        except (urllib.error.HTTPError, urllib.error.URLError) as ex:
            error(f"Error attempting to perform cache {arg} using "
                  f"rest_api.{attr}.url: {repr(ex)}")


def do_afc_config(cfg: Config, args: Any) -> None:
    """ Execute "afc_config" command.

    Arguments:
    cfg  -- Config object
    args -- Parsed command line arguments
    """
    service_discovery = \
        ServiceDiscovery.create(cfg=cfg, compose_project=args.comp_proj,
                                k3d_cluster=args.k3d)
    afc_config = get_afc_config(cfg=cfg, args=args,
                                service_discovery=service_discovery)
    afc_config_str = \
        json.dumps(
            patch_json(patch_arg=args.FIELD_VALUE, json_dict=afc_config,
                       new_type=args.new, data_type="AFC Config"))
    result = \
        ratdb(
            cfg=cfg,
            command=cfg.ratdb.update_config_by_id.format(
                afc_config=afc_config_str, region_str=afc_config["regionStr"]),
            service_discovery=service_discovery)
    error_if(not (isinstance(result, int) and result > 0),
             "AFC Config update failed")


def do_json_config(cfg: Config, args: Any) -> None:
    """ Execute "afc_config" command.

    Arguments:
    cfg  -- Config object
    args -- Parsed command line arguments
    """
    s = json.dumps(cfg.data(), indent=2)
    filename = args.JSON_CONFIG if args.JSON_CONFIG else \
        (os.path.splitext(Config.DEFAULT_CONFIG)[0] + Config.JSON_EXT)
    with open(filename, "w", encoding="utf-8") as f:
        f.write(s)


def do_help(cfg: Config, args: Any) -> None:
    """ Execute "help" command.

    Arguments:
    cfg  -- Config object (not used)
    args -- Parsed command line arguments (also contains 'argument_parser' and
            'subparsers' fields)
    """
    unused_argument(cfg)
    if args.subcommand is None:
        args.argument_parser.print_help()
    else:
        args.subparsers.choices[args.subcommand].print_help()


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """

    cfg = Config(argv=argv, arg_name="config")

    switches_common = argparse.ArgumentParser(add_help=False)
    switches_common.add_argument(
        "--parallel", metavar="N", type=int, default=cfg.defaults.parallel,
        help=f"Number of requests to execute in parallel. Default is "
        f"{cfg.defaults.parallel}")
    switches_common.add_argument(
        "--backoff", metavar="SECONDS", type=float,
        default=cfg.defaults.backoff,
        help=f"Initial backoff window (in seconds) to use on request failure. "
        f"It is doubled on each retry. Default is {cfg.defaults.backoff} "
        f"seconds")
    switches_common.add_argument(
        "--retries", metavar="N", type=int, default=cfg.defaults.retries,
        help=f"Maximum number of retries before giving up. Default is "
        f"{cfg.defaults.retries}")
    switches_common.add_argument(
        "--dry", action="store_true",
        help="Dry run (no communication with server) to estimate overhead")
    switches_common.add_argument(
        "--status_period", metavar="N", type=int,
        default=cfg.defaults.status_period,
        help=f"How often to print status information. Default is once in "
        f"{cfg.defaults.status_period} requests. 0 means no status print")
    switches_common.add_argument(
        "--no_reconnect", action="store_true",
        help="Do not reconnect on each request (requires 'requests' Python3 "
        "library to be installed: 'pip install requests'")

    switches_req = argparse.ArgumentParser(add_help=False)
    switches_req.add_argument(
        "--idx_range", metavar="[FROM-]TO", default=cfg.defaults.idx_range,
        help=f"Range of AP indices. FROM is initial index (0 if omitted), TO "
        f"is 'afterlast' index. Default is '{cfg.defaults.idx_range}'")
    switches_req.add_argument(
        "--batch", metavar="N", type=int, default=cfg.defaults.batch,
        help=f"Number of requests in one REST API call. Default is "
        f"{cfg.defaults.batch}")
    switches_req.add_argument(
        "--req", metavar="FIELD1=VALUE1[;FIELD2=VALUE2...]", action="append",
        default=[],
        help="Change field(s) in request body (compared to req_msg_pattern "
        "in config file). FIELD is dot-separated path to field inside request "
        "(e.g. 'location.ellipse.majorAxis'), VALUE is a field value, if "
        "field value is list - it should be surrounded by [] and formatted as "
        "in JSON. Several semicolon-separated settings may be specified, also "
        "this parameter may be specified several times")

    switches_count = argparse.ArgumentParser(add_help=False)
    switches_count.add_argument(
        "--count", metavar="NUM_REQS", type=int, default=cfg.defaults.count,
        help=f"Number of requests to send. Default is {cfg.defaults.count}")

    switches_afc = argparse.ArgumentParser(add_help=False)
    switches_afc.add_argument(
        "--afc", metavar="[PROTOCOL://]HOST[:port][path][params]",
        help="AFC Server to send requests to. Unspecified parts are taken "
        "from 'rest_api.afc_req' of config file. By default determined by "
        "means of '--comp_proj' or '--k3d'")
    switches_afc.add_argument(
        "--dispatcher", nargs="?", choices=["http", "https"], const="http",
        help="If --afc is not specified - send AFC Requests to http (default) "
        "or https port of the dispatcher. If neither --afc nor --dispatcher "
        "specified, AFC Requests are sent directly to msghnd (to avoid https "
        "issues on dispatcher")

    switches_k3d = argparse.ArgumentParser(add_help=False)
    switches_k3d.add_argument(
        "--k3d", metavar="CLUSTER",
        help=f"K3d cluster to send requests to. '{CURRENT_CLUSTER_NAME}' for "
        f"current cluster.  Should have necessary ports exposed (at a "
        f"minimum - dispatcher's http or https)")

    switches_rat = argparse.ArgumentParser(add_help=False)
    switches_rat.add_argument(
        "--rat_server", metavar="[PROTOCOL://]HOST[:port][path][params]",
        help="Server to request config from. Unspecified parts are taken "
        "from 'rest_api.get_config' of config file. By default determined by "
        "means of '--comp_proj'")

    switches_compose = argparse.ArgumentParser(add_help=False)
    switches_compose.add_argument(
        "--comp_proj", metavar="PROJ_NAME",
        help="Docker compose project name. Used to determine hosts to send "
        "API calls to. If not specified hostnames should be specified "
        "explicitly")

    switches_rcache = argparse.ArgumentParser(add_help=False)
    switches_rcache.add_argument(
        "--rcache", metavar="[PROTOCOL://]HOST[:port]",
        help="Rcache server. May also be determined by means of '--comp_proj'")

    # Top level parser
    argument_parser = argparse.ArgumentParser(
        description="AFC Load Test Tool")
    argument_parser.add_argument(
        "--config", metavar="[+]CONFIG_FILE", action="append", default=[],
        help=f"Config file. Default has same name and directory as this "
        f"script, but has "
        f"'{Config.YAML_EXT if has_yaml else Config.JSON_EXT}' extension "
        f"(i.e. {Config.DEFAULT_CONFIG}). May be specified several times (in "
        f"which case values are merged together). If prefixed with '+' - "
        "joined to the default config. Note that this script is accompanied "
        f"with default YAML config. On YAML-less Python it should be "
        f"converted to JSON (with 'json_config' subcommand) on some YAML-ed "
        f"system and copied to YAML-less one")

    subparsers = argument_parser.add_subparsers(dest="subcommand",
                                                metavar="SUBCOMMAND")

    parser_preload = subparsers.add_parser(
        "preload",
        parents=[switches_common, switches_rat, switches_req, switches_compose,
                 switches_rcache, switches_k3d],
        help="Fill rcache with (fake) responses")
    parser_preload.add_argument(
        "--protect_cache", action="store_true",
        help="Protect Rcache from invalidation (e.g. by ULS downloader). "
        "Protection persists in rcache database, see 'rcache' subcommand on "
        "how to unprotect")
    parser_preload.set_defaults(func=do_preload)

    parser_load = subparsers.add_parser(
        "load",
        parents=[switches_common, switches_req, switches_count,
                 switches_compose, switches_afc, switches_k3d],
        help="Do load test")
    parser_load.add_argument(
        "--no_cache", action="store_true",
        help="Don't use rcache, force each request to be computed")
    parser_load.add_argument(
        "--debug", action="store_true",
        help="Run AFC engine in debug mode")
    parser_load.add_argument(
        "--population", metavar="POPULATION_DB_FILE",
        help="Select AP positions proportionally to population density from "
        "given database (prepared with "
        "tools/geo_converters/make_population_db.py). Positions are random, "
        "so no rcache will help")
    parser_load.add_argument(
        "--err_dir", metavar="DIRECTORY",
        help="Directory for offending JSON AFC Requests")
    parser_load.add_argument(
        "--random", action="store_true",
        help="Choose AP positions randomly (makes sense only in noncached "
        "mode)")
    parser_load.add_argument(
        "--ramp_up", metavar="SECONDS", type=float, default=0,
        help="Ramp up streams for this number of seconds. Default is to start "
        "all at once")
    parser_load.set_defaults(func=do_load)

    parser_network = subparsers.add_parser(
        "netload",
        parents=[switches_common, switches_count, switches_compose,
                 switches_afc, switches_rcache, switches_k3d],
        help="Network load test by repeatedly querying health endpoints")
    parser_network.add_argument(
        "--target", choices=["dispatcher", "msghnd", "rcache"],
        help="What to test. If omitted then guess is attempted: 'dispatcher' "
        "if --dispatcher specified, 'msghnd' if --afc without --dispatcher is "
        "specified, 'rcache' if --rcache is specified")
    parser_network.set_defaults(func=do_netload)

    parser_cache = subparsers.add_parser(
        "cache", parents=[switches_compose, switches_rcache, switches_k3d],
        help="Do something with response cache")
    parser_cache.add_argument(
        "--protect", action="store_true",
        help="Protect rcache from invalidation (e.g. by background ULS "
        "downloader). This action persists in rcache database, it need to be "
        "explicitly undone with --unprotect")
    parser_cache.add_argument(
        "--unprotect", action="store_true",
        help="Allows rcache invalidation")
    parser_cache.add_argument(
        "--invalidate", action="store_true",
        help="Invalidate cache (invalidation must be enabled)")
    parser_cache.set_defaults(func=do_cache)

    parser_afc_config = subparsers.add_parser(
        "afc_config", parents=[switches_rat, switches_compose, switches_k3d],
        help="Modify AFC Config")
    parser_afc_config.add_argument(
        "--new", metavar="VALUE_TYPE",
        choices=["str", "int", "float", "bool", "list"],
        help="Allow creation of new AFC Config keys (requires respective "
        "changes in AFC Engine). Created keys will be of given type")
    parser_afc_config.add_argument(
        "FIELD_VALUE", nargs="+",
        help="One or more FIELD=VALUE clauses, where FIELD is a field name in "
        "AFC Config, deep field may be specified in dot-separated for m (e.g. "
        "'freqBands.0.startFreqMHz'). VALUE is new field value, if field "
        "value is list - it should be surrounded by [] and formatted as in "
        "JSON")
    parser_afc_config.set_defaults(func=do_afc_config)

    parser_json_config = subparsers.add_parser(
        "json_config",
        help="Convert config file from YAML to JSON for use on YAML-less "
        "systems")
    parser_json_config.add_argument(
        "JSON_CONFIG", nargs="?",
        help=f"JSON file to create. By default - same as source YAML file, "
        f"but with {Config.JSON_EXT} extension")
    parser_json_config.set_defaults(func=do_json_config)

    parser_help = subparsers.add_parser(
        "help", add_help=False,
        help="Prints help on given subcommand")
    parser_help.add_argument(
        "subcommand", metavar="SUBCOMMAND", nargs="?",
        choices=subparsers.choices,
        help="Name of subcommand to print help about (use " +
        "\"%(prog)s --help\" to get list of all subcommands)")
    parser_help.set_defaults(func=do_help, subparsers=subparsers,
                             argument_parser=argument_parser,
                             supports_unknown_args=True)

    if not argv:
        argument_parser.print_help()
        sys.exit(1)
    args = argument_parser.parse_known_args(argv)[0]
    if not getattr(args, "supports_unknown_args", False):
        args = argument_parser.parse_args(argv)

    # Set up logging
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(
        logging.Formatter(
            f"{os.path.basename(__file__)}. %(levelname)s: %(message)s"))
    logging.getLogger().addHandler(console_handler)
    logging.getLogger().setLevel(logging.INFO)

    # Do the needful
    try:
        args.func(cfg, args)
    except KeyboardInterrupt:
        print("^C")
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1:])
