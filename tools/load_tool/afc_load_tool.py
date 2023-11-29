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
# pylint: disable=too-many-statements

import argparse
from collections.abc import Iterable, Iterator
import copy
import datetime
import hashlib
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
import signal
import subprocess
import sys
import time
import traceback
from typing import Any, cast, List, Dict, NamedTuple, Optional, Tuple, Union
import urllib.error
import urllib.parse
import urllib.request
import yaml


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


def error(msg: str) -> None:
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
    """ YAML dictionary for given YAML content """
    kwargs: Dict[str, Any] = {}
    if hasattr(yaml, "CLoader"):
        kwargs["Loader"] = yaml.CLoader
    elif hasattr(yaml, "FullLoader"):
        kwargs["Loader"] = yaml.FullLoader
    return yaml.load(yaml_s, **kwargs)


class Config(Iterable):
    """ Node of config structure that provides dot-based access

    Private attributes:
    _data -- Data that corresponds to node (list or string-indexed dictionary)
    _path -- Path to node ("foo.bar.42.baz") for use in error messages
    """

    # Default config file name
    DEFAULT_CONFIG = os.path.splitext(__file__)[0] + ".yaml"

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
        argument_parser = argparse.ArgumentParser()
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
            except yaml.YAMLError as ex:
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


def run_docker(args: List[str]) -> str:
    """ Runs docker with given parameters, returns stdout content """
    try:
        return subprocess.check_output(["docker"] + args, text=True)
    except subprocess.CalledProcessError as ex:
        error(f"Failed to run 'docker {' '.join(args)}': {repr(ex)}. Please "
              "specify all hosts explicitly")
    return ""  # Unreachable code to make pylint happy


def get_url(base_url: str, param_host: Optional[str],
            param_comp_prj: Optional[str], purpose: str,
            service_cache: Optional[Dict[str, str]] = None) -> str:
    """ Construct URL from base URL in config and command line host or compose
    project name

    Arguments:
    base_url       -- Base URL from config
    param_host     -- Optional host name from command line parameter
    param_comp_prj -- Optional name of running compose
    service_cache  -- Optional dictionary for storing results of service IP
                      lookups
    Returns actionable URL with host either specified or retrieved from compose
    container inspection and tail from base URL
    """
    base_parts = urllib.parse.urlparse(base_url)
    if param_host:
        if "://" not in param_host:
            param_host = f"{base_parts.scheme}://{param_host}"
        param_parts = urllib.parse.urlparse(param_host)
        replacements = {field: getattr(param_parts, field)
                        for field in base_parts._fields
                        if getattr(param_parts, field)}
        return urllib.parse.urlunparse(base_parts._replace(**replacements))
    if param_comp_prj is None:
        return base_url

    service = base_parts.netloc.split(":")[0]
    service_ip = (service_cache or {}).get(service)
    if service_ip is None:
        name_offset: Optional[int] = None
        container: Optional[str] = None
        for line in run_docker(["ps"]).splitlines():
            if name_offset is None:
                name_offset = line.find("NAMES")
                error_if(name_offset < 0,
                         "Unsupported structure of 'docker ps' output. Please "
                         "specify all hosts explicitly")
                continue
            assert name_offset is not None
            for names in re.split(r",\s*", line[name_offset:]):
                m = re.search(r"(%s_(.+)_\d+)" % re.escape(param_comp_prj),
                              names)
                if m and (m.group(2) == service):
                    container = m.group(1)
                    break
            if container:
                break
        else:
            error(f"Service name '{param_comp_prj}_{service}_*' not found. "
                  f"Please specify host/URL for {purpose} explicitly")
        assert container is not None

        try:
            inspect_dict = json.loads(run_docker(["inspect", container]))
        except json.JSONDecodeError as ex:
            error(f"Error parsing 'docker inspect {container}' output: "
                  f"{repr(ex)}. Please specify host/URL for {purpose} "
                  f"explicitly")
        try:
            for net_name, net_info in \
                    inspect_dict[0]["NetworkSettings"]["Networks"].items():
                if net_name.endswith("_default"):
                    service_ip = net_info["IPAddress"]
                    break
            else:
                error(f"Default network not found in {container}. Please "
                      f"specify host/URL for {purpose} explicitly")
        except (AttributeError, LookupError) as ex:
            error(f"Unsupported structure of  'docker inspect {container}' "
                  f"output: {repr(ex)}. Please specify host/URL for {purpose} "
                  f"explicitly")
    assert service_ip is not None
    if service_cache is not None:
        service_cache[service] = service_ip
    return \
        urllib.parse.urlunparse(
            base_parts._replace(
                netloc=service_ip + base_parts.netloc[len(service):]))


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
    _grid_size        -- Copy of cfg.region.grid_dize
    _min_lat          -- Copy of cfg.region.min_lat
    _max_lat          -- Copy of cfg.region.max_lat
    _min_lon          -- Copy of cfg.region.min_lon
    _max_lon          -- Copy of cfg.region.max_lon
    _channels_20mhz   -- Copy of cfg._channels_20mhz
    """
    def __init__(self, cfg: Config) -> None:
        """ Constructor

        Arguments:
        cfg -- Config object
        """
        self._paths = cfg.paths
        self._req_msg_pattern = json.loads(cfg.req_msg_pattern)
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
        self._channels_20mhz = cfg.channels_20mhz

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
            path_set(
                req, self._paths.lat_in_req,
                self._min_lat +
                (req_idx // self._grid_size) *
                (self._max_lat - self._min_lat) / self._grid_size)
            path_set(
                req, self._paths.lon_in_req,
                self._min_lon +
                (req_idx % self._grid_size) *
                (self._max_lon - self._min_lon) / self._grid_size)
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


class RestDataHandlerBase:
    """ Base class for generate/process REST API request/response data payloads
    """
    def __init__(self, cfg: Config) -> None:
        """ Constructor

        Arguments:
        cfg -- Config object
        """
        self.cfg = cfg
        self.afc_req_resp_gen = AfcReqRespGenerator(cfg)

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

    def dry_result_data(self, batch: int) -> bytes:
        """ Virtual method returning bytes string to pass to make_error_map on
        dry run. This default implementation returns empty string """
        unused_argument(batch)
        return b''


class PreloadRestDataHandler(RestDataHandlerBase):
    """ REST API data handler for 'preload' operation - i.e. Rcache update
    messages (no response)

    Private attributes:
    _hash_base -- MD5 hash computed over AFC Config, awaiting AFC Request tail
    """
    def __init__(self, cfg: Config, afc_config: Dict[str, Any]) -> None:
        self._hash_base = hashlib.md5()
        self._hash_base.update(json.dumps(afc_config,
                                          sort_keys=True).encode("utf-8"))
        super().__init__(cfg)

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

    def dry_result_data(self, batch: int) -> bytes:
        """ Returns byte string, containing AFC Response to parse on dry run
        """
        resp_msg = json.loads(self.cfg.resp_msg_pattern)
        path_set(resp_msg, self.cfg.paths.resps_in_msg,
                 path_get(resp_msg, self.cfg.paths.resps_in_msg) * batch)
        return json.dumps(resp_msg).encode("utf-8")


# REST API Request data and supplementary information
WorkerReqInfo = \
    NamedTuple("WorkerReqInfo",
               [
                # Request indices, contained in REST API request data
                ("req_indices", List[int]),
                # REST API Request data
                ("req_data", bytes)])


# REST API request results
WorkerResultInfo = \
    NamedTuple("WorkerResultInfo",
               [
                # Request indices
                ("req_indices", List[int]),
                # Retries made (0 - from first attempt)
                ("retries", int),
                # REST API Response data (None if error or N/A)
                ("result_data", Optional[bytes]),
                # Error message for failed requests, None for succeeded
                ("error_msg", Optional[str])])


# Message from Tick worker for EMA rate computation
TickInfo = NamedTuple("TickInfo", [("tick", int)])

# Type for result queue items
ResultQueueDataType = Optional[Union[WorkerResultInfo, TickInfo]]


class RateEma:
    """ Exponential Moving Average for rate of change

    Public attributes:
    rate_ema -- Rate Average computed on last tick

    Private attributes:
    _worker     -- Tick worker process (generates TickInfo once per second)
    _weight     -- Weight for EMA computation
    _prev_value -- Value on previous tick
    """
    def __init__(self,
                 result_queue: "multiprocessing.Queue[ResultQueueDataType]",
                 win_size: float = 20) \
            -> None:
        """ Constructor

        Arguments:
        result_queue -- Queue for tick worker to put TickInfo objects
        win_size     -- Averaging window size in seconds
        """
        self._worker = \
            multiprocessing.Process(target=RateEma._tick_worker,
                                    kwargs={"result_queue": result_queue})
        self._worker.start()
        self._weight = 2 / (win_size + 1)
        self.rate_ema: float = 0
        self._prev_value = 0

    def on_tick(self, new_value: int) -> None:
        """ Call on arrival of TickInfo message

        Arguments:
        new_value - Measured data value on this tick
        """
        increment = new_value - self._prev_value
        self._prev_value = new_value
        self.rate_ema += self._weight * (increment - self.rate_ema)

    def stop(self) -> None:
        """ Stops tick worker """
        self._worker.terminate()

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
    _total_requests    -- Total number of individual requests (not batches)
                          that will be executed
    _result_queue      -- Queue with execution results, second ticks, Nones for
                          completed workers
    _status_period     -- Period of status printing (in terms of request
                          count), e.g. 1000 for once in 1000 requests, 0 to not
                          print status (except in the end)
    _rest_data_handler -- Generator/interpreter of REST request/response data
    _num_workers       -- Number of worker processes
    _rate_ema          -- Rate averager
    _status_printer    -- StatusPrinter
    """
    def __init__(
            self, total_requests: int,
            result_queue: "multiprocessing.Queue[ResultQueueDataType]",
            num_workers: int, status_period: int,
            rest_data_handler: RestDataHandlerBase,
            rate_ema: RateEma) -> None:
        """ Constructor

        Arguments:
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
        """
        self._total_requests = total_requests
        self._result_queue = result_queue
        self._status_period = status_period
        self._num_workers = num_workers
        self._rest_data_handler = rest_data_handler
        self._rate_ema = rate_ema
        self._status_printer = StatusPrinter()

    def process(self) -> None:
        """ Keep processing results until all worker will stop """
        start_time = datetime.datetime.now()
        requests_sent = 0
        requests_failed = 0
        retries = 0

        def status_message(eta: bool) -> str:
            """ Returns status message (with or without ETA) """
            now = datetime.datetime.now()
            elapsed = now - start_time
            elapsed_sec = elapsed.total_seconds()
            elapsed_str = re.sub(r"\.\d*$", "", str(elapsed))
            rate = self._rate_ema.rate_ema if eta \
                else (requests_sent / (elapsed_sec or 1E-3))
            ret = \
                f"{'Progress: ' if eta else ''}" \
                f"{requests_sent} requests sent " \
                f"({requests_sent * 100 / self._total_requests:.1f}%), " \
                f"{requests_failed} failed " \
                f"({requests_failed * 100 / (requests_sent or 1):.1f}%), " \
                f"{retries} retries made. " \
                f"{elapsed_str} elapsed, rate is {rate:.1f} req/sec"
            if eta and elapsed_sec and requests_sent:
                total_duration = \
                    datetime.timedelta(
                        seconds=self._total_requests * elapsed_sec /
                        requests_sent)
                eta_dt = start_time + total_duration
                tta_sec = int((eta_dt - now).total_seconds())
                tta = f"{tta_sec // 60} minutes" if tta_sec >= 60 else \
                    f"{tta_sec} seconds"
                ret += f". ETA: {eta_dt.strftime('%X')} (in {tta})"
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
                    self._rate_ema.on_tick(requests_sent)
                    continue
                error_msg = result_info.error_msg
                error_map: Dict[int, str] = {}
                if error_msg is None:
                    try:
                        error_map = \
                            self._rest_data_handler.make_error_map(
                                result_data=result_info.result_data)
                    except Exception as ex:
                        error_msg = f"Error decoding message " \
                            f"{result_info.result_data!r}: {repr(ex)}"
                if error_msg:
                    self._status_printer.pr()
                    logging.error(
                        f"Request with indices "
                        f"({', '.join(str(i) for i in result_info.req_indices)}) "
                        f"failed: {error_msg}")

                for idx, error_msg in error_map.items():
                    self._status_printer.pr()
                    logging.error(f"Request {idx} failed: {error_msg}")

                prev_sent = requests_sent
                requests_sent += len(result_info.req_indices)
                retries += result_info.retries
                if result_info.error_msg:
                    requests_failed += len(result_info.req_indices)
                else:
                    requests_failed += len(error_map)

                if self._status_period and \
                        ((prev_sent // self._status_period) !=
                         (requests_sent // self._status_period)):
                    self._status_printer.pr(status_message(eta=True))
        finally:
            self._status_printer.pr()
            logging.info(status_message(eta=False))

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


def rest_api_worker(
        url: str, retries: int, backoff: float, dry: bool,
        req_queue: "multiprocessing.Queue[Optional[WorkerReqInfo]]",
        result_queue: "multiprocessing.Queue[Optional[WorkerResultInfo]]",
        dry_result_data: Optional[bytes], use_requests: bool) -> None:
    """ REST API process worker function

    Arguments:
    url          -- REST API URL
    retries      -- Number of retries
    backoff      -- Initial backoff window in seconds
    dry          -- True to dry run
    req_queue    -- Request queue. Elements are WorkerReqInfo objects
                    corresponding to single REST API post or None to stop
                    operation
    result_queue -- Result queue. Elements are WorkerResultInfo for
                    operation results, None to signal that worker finished
    use_requests -- True to use requests, False to use urllib.request
    """
    try:
        session: Optional[requests.Session] = \
            requests.Session() if use_requests and (not dry) else None
        while True:
            req_info = req_queue.get()
            if req_info is None:
                result_queue.put(None)
                return

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
                                    headers={"Content-Type":
                                             "application/json; charset=utf-8",
                                             "Content-Length":
                                             str(len(req_info.req_data))},
                                    timeout=30)
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
                            with urllib.request.urlopen(req,
                                                        req_info.req_data,
                                                        timeout=30) as f:
                                result_data = f.read()
                            break
                        except (urllib.error.HTTPError, urllib.error.URLError,
                                urllib.error.ContentTooShortError) as ex:
                            last_error = repr(ex)
                    time.sleep(
                        random.uniform(0, (1 << attempt)) * backoff)
                else:
                    error_msg = last_error
                attempts = attempt + 1
            result_queue.put(
                WorkerResultInfo(
                    req_indices=req_info.req_indices, retries=attempts - 1,
                    result_data=result_data, error_msg=error_msg))
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
        min_idx: int, max_idx: int, count: Optional[int], batch: int,
        parallel: int, dry: bool, rest_data_handler: RestDataHandlerBase,
        req_queue: "multiprocessing.Queue[Optional[WorkerReqInfo]]") -> None:
    """ Producer (request queue filler), moved into separate process

    Arguments:
    batch             -- Batch size (number of requests per POST)
    min_idx           -- Minimum request index
    max_idx           -- Aftremaximum request index
    count             -- Optional count of requests to send. None means that
                         requests will be sent sequentially according to range.
                         If specified - request indices will be randomized
    parallel          -- Number of worker processes to use
    dry               -- Dry run
    rest_data_handler -- Generator/interpreter of REST request/response data
    req_queue         -- Requests queue to fill
    """
    try:
        if count is not None:
            for min_count in range(0, count, batch):
                req_indices = [random.randrange(min_idx, max_idx)
                               for _ in range(min(count - min_count, batch))]
                req_queue.put(
                    WorkerReqInfo(
                        req_indices=req_indices,
                        req_data=b'' if dry else
                        rest_data_handler.make_req_data(req_indices)))
        else:
            for min_req_idx in range(min_idx, max_idx, batch):
                req_indices = list(range(min_req_idx,
                                         min(min_req_idx + batch, max_idx)))
                req_queue.put(
                    WorkerReqInfo(
                        req_indices=req_indices,
                        req_data=b'' if dry else
                        rest_data_handler.make_req_data(req_indices)))
    except Exception as ex:
        error(f"Producer terminated: {repr(ex)}")
    finally:
        for _ in range(parallel):
            req_queue.put(None)


def run(rest_data_handler: RestDataHandlerBase, url: str, parallel: int,
        backoff: int, retries: int, dry: bool, batch: int, min_idx: int,
        max_idx: int, status_period: int, count: Optional[int] = None,
        use_requests: bool = False) -> None:
    """ Run the operation

    Arguments:
    rest_data_handler -- REST API payload data generator/interpreter
    url               -- REST API URL to send POSTs to
    parallel          -- Number of worker processes to use
    backoff           -- Initial size of backoff windows in seconds
    retries           -- Number of retries
    dry               -- True to dry run
    batch             -- Batch size (number of requests per POST)
    min_idx           -- Minimum request index
    max_idx           -- Aftermaximum request index
    status_period     -- Period (in terms of count) of status message prints (0
                         to not at all)
    count             -- Optional count of requests to send. None means that
                         requests will be sent sequentially according to range.
                         If specified - request indices will be randomized
    use_requests      -- Use requests to send requests (default is to use
                         urllib.request)
    """
    error_if(use_requests and ("requests" not in sys.modules),
             "'requests' Python3 module have to be installed to use "
             "'--no_reconnect' option")
    logging.info(f"URL: {'N/A' if dry else url}")
    logging.info(f"Streams: {parallel}")
    logging.info(f"Backoff: {backoff} sec, {retries} retries")
    logging.info(f"Index range: {min_idx:_} - {max_idx:_}")
    logging.info(f"Batch size: {batch}")
    logging.info(f"Requests to send: "
                 f"{(max_idx - min_idx) if count is None else count:_}")
    logging.info(f"Nonreconnect mode: {use_requests}")
    if status_period:
        logging.info(f"Intermediate status is printed every "
                     f"{status_period} requests sent")
    else:
        logging.info("Intermediate status is not printed")
    req_queue: "multiprocessing.Queue[Optional[WorkerReqInfo]]" = \
        multiprocessing.Queue()
    result_queue: "multiprocessing.Queue[ResultQueueDataType]" = \
        multiprocessing.Queue()
    workers: List[multiprocessing.Process] = []
    original_sigint_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
    rate_ema: Optional[RateEma] = None
    try:
        for _ in range(parallel):
            workers.append(
                multiprocessing.Process(
                    target=rest_api_worker,
                    kwargs={"url": url, "backoff": backoff, "retries": retries,
                            "dry": dry, "req_queue": req_queue,
                            "result_queue": result_queue,
                            "dry_result_data":
                            rest_data_handler.dry_result_data(batch),
                            "use_requests": use_requests}))
            workers[-1].start()
        workers.append(
            multiprocessing.Process(
                target=producer_worker,
                kwargs={"min_idx": min_idx, "max_idx": max_idx, "count": count,
                        "batch": batch, "parallel": parallel, "dry": dry,
                        "rest_data_handler": rest_data_handler,
                        "req_queue": req_queue}))
        workers[-1].start()
        rate_ema = RateEma(result_queue)
        signal.signal(signal.SIGINT, original_sigint_handler)
        results_processor = \
            ResultsProcessor(
                total_requests=count if count is not None
                else (max_idx - min_idx),
                result_queue=result_queue, num_workers=parallel,
                status_period=status_period,
                rest_data_handler=rest_data_handler, rate_ema=rate_ema)
        results_processor.process()
        for worker in workers:
            worker.join()
    finally:
        for worker in workers:
            if worker.is_alive():
                worker.terminate()
        if rate_ema:
            rate_ema.stop()


def do_preload(cfg: Config, args: Any) -> None:
    """ Execute "preload" command.

    Arguments:
    cfg  -- Config object
    args -- Parsed command line arguments
    """
    if args.dry:
        afc_config = {}
        update_rcache_url = ""
    else:
        service_cache: Dict[str, str] = {}
        if args.protect_cache:
            try:
                with urllib.request.urlopen(
                        urllib.request.Request(
                            get_url(base_url=cfg.rest_api.protect_rcache.url,
                                    param_host=args.rcache,
                                    param_comp_prj=args.comp_proj,
                                    purpose="Rcache invalidate",
                                    service_cache=service_cache),
                            method="POST"),
                        timeout=30):
                    pass
            except (urllib.error.HTTPError, urllib.error.URLError) as ex:
                error(f"Error attempting to protect Rcache from invalidation "
                      f"using protect_rcache.url: {repr(ex)}")

        get_config_url = \
            get_url(base_url=cfg.rest_api.get_config.url,
                    param_host=args.rat_server,
                    param_comp_prj=args.comp_proj,
                    purpose="AFC Config retrieval",
                    service_cache=service_cache)
        update_rcache_url = \
            get_url(base_url=cfg.rest_api.update_rcache.url,
                    param_host=args.rcache,
                    param_comp_prj=args.comp_proj, purpose="Rcache preload",
                    service_cache=service_cache)
        try:
            get_config_url += cfg.region.rulesest_id
            with urllib.request.urlopen(get_config_url, timeout=30) as f:
                afc_config_str = f.read().decode('utf-8')
        except (urllib.error.HTTPError, urllib.error.URLError) as ex:
            error(f"Error retrieving AFC Config for Ruleset ID "
                  f"'{cfg.region.rulesest_id}' using URL get_config.url: "
                  f"{repr(ex)}")
        try:
            afc_config = json.loads(afc_config_str)
        except json.JSONDecodeError as ex:
            error(f"Error decoding AFC Config JSON: {repr(ex)}")

    min_idx, max_idx = get_idx_range(args.idx_range)

    run(rest_data_handler=PreloadRestDataHandler(cfg=cfg,
                                                 afc_config=afc_config),
        url=update_rcache_url, parallel=args.parallel, backoff=args.backoff,
        retries=args.retries, dry=args.dry, batch=args.batch, min_idx=min_idx,
        max_idx=max_idx, status_period=args.status_period,
        use_requests=args.no_reconnect)

    logging.info("Waiting for updates to flush to DB")
    start_time = datetime.datetime.now()
    start_queue_len: Optional[int] = None
    prev_queue_len: Optional[int] = None
    status_printer = StatusPrinter()
    rcache_status_url = \
        get_url(base_url=cfg.rest_api.rcache_status.url,
                param_host=args.rcache, param_comp_prj=args.comp_proj,
                purpose="Rcache status retrieval", service_cache=service_cache)
    while True:
        try:
            with urllib.request.urlopen(rcache_status_url, timeout=30) as f:
                rcache_status = json.loads(f.read())
        except (urllib.error.HTTPError, urllib.error.URLError) as ex:
            error(f"Error retrieving Rcache status "
                  f"'{cfg.region.rulesest_id}' using URL get_config.url : "
                  f"{repr(ex)}")
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


def do_load(cfg: Config, args: Any) -> None:
    """ Execute "load" command.

    Arguments:
    cfg  -- Config object
    args -- Parsed command line arguments
    """
    if args.dry:
        afc_url = ""
    elif args.localhost:
        error_if(not args.comp_proj, "--comp_proj parameter must be specified")
        m = re.search(r"0\.0\.0\.0:(\d+)->%d/tcp.*%s_dispatcher_\d+" %
                      (80 if args.localhost == "http" else 443,
                       re.escape(args.comp_proj)),
                      run_docker(["ps"]))
        error_if(
            not m,
            "AFC port not found. Please specify AFC server address explicitly")
        assert m is not None
        afc_url = \
            get_url(base_url=cfg.rest_api.afc_req.url,
                    param_host=f"localhost:{m.group(1)}",
                    param_comp_prj=args.comp_proj, purpose="AFC Server")
    else:
        afc_url = \
            get_url(base_url=cfg.rest_api.afc_req.url, param_host=args.afc,
                    param_comp_prj=args.comp_proj, purpose="AFC Server")
    if args.no_cache:
        parsed_afc_url = urllib.parse.urlparse(afc_url)
        afc_url = \
            parsed_afc_url._replace(
                query="&".join(
                    p for p in [parsed_afc_url.query, "nocache=True"] if p)).\
            geturl()

    min_idx, max_idx = get_idx_range(args.idx_range)

    run(rest_data_handler=LoadRestDataHandler(cfg=cfg), url=afc_url,
        parallel=args.parallel, backoff=args.backoff, retries=args.retries,
        dry=args.dry, batch=args.batch, min_idx=min_idx, max_idx=max_idx,
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
    error_if(not(args.protect or args.unprotect or args.invalidate),
             "Nothing to do")
    service_cache: Dict[str, str] = {}
    json_data: Any
    for arg, attr, json_data, purpose in \
            [("protect", "protect_rcache", None, "Rcache protect"),
             ("invalidate", "invalidate_rcache", {}, "Rcache invalidate"),
             ("unprotect", "unprotect_rcache", None, "Rcache unprotect")]:
        try:
            if not getattr(args, arg):
                continue
            data: Optional[bytes] = None
            url = get_url(base_url=getattr(cfg.rest_api, attr).url,
                          param_host=args.rcache,
                          param_comp_prj=args.comp_proj,
                          purpose=purpose, service_cache=service_cache)
            req = urllib.request.Request(url, method="POST")
            if json_data is not None:
                data = json.dumps(json_data).encode(encoding="ascii")
                req.add_header("Content-Type", "application/json")
            urllib.request.urlopen(req, data, timeout=30)
        except (urllib.error.HTTPError, urllib.error.URLError) as ex:
            error(f"Error attempting to perform {purpose} using "
                  f"rest_api.{attr}.url: {repr(ex)}")


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
        "--idx_range", metavar="[FROM-]TO", default=cfg.defaults.idx_range,
        help=f"Range of AP indices. FROM is initial index (0 if omitted), TO "
        f"is 'afterlast' index. Default is '{cfg.defaults.idx_range}'")
    switches_common.add_argument(
        "--parallel", metavar="N", type=int, default=cfg.defaults.parallel,
        help=f"Number of requests to execute in parallel. Default is "
        f"{cfg.defaults.parallel}")
    switches_common.add_argument(
        "--batch", metavar="N", type=int, default=cfg.defaults.batch,
        help=f"Number of requests in one REST API call. Default is "
        f"{cfg.defaults.batch}")
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
        help="Dry run to estimate overhead")
    switches_common.add_argument(
        "--status_period", metavar="N", type=int,
        default=cfg.defaults.status_period,
        help=f"How often to print status information. Default is once in "
        f"{cfg.defaults.status_period} requests. 0 means no status print")
    switches_common.add_argument(
        "--no_reconnect", action="store_true",
        help="Do not reconnect on each request (requires 'requests' Python3 "
        "library to be installed: 'pip install requests'")

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
        f"script, but has '.yaml' extension (i.e. {Config.DEFAULT_CONFIG}). "
        f"May be specified several times (in which case values are merged "
        f"together). If prefixed with '+' - joined to the default config")

    subparsers = argument_parser.add_subparsers(dest="subcommand",
                                                metavar="SUBCOMMAND")

    parser_preload = subparsers.add_parser(
        "preload",
        parents=[switches_common, switches_compose, switches_rcache],
        help="Fill rcache with (fake) responses")
    parser_preload.add_argument(
        "--rat_server", metavar="[PROTOCOL://]HOST[:port][path][params]",
        help="Server to request config from. Unspecified parts are taken "
        "from 'rest_api.get_config' of config file. By default determined by "
        "means of '--comp_proj'")
    parser_preload.add_argument(
        "--protect_cache", action="store_true",
        help="Protect Rcache from invalidation (e.g. by ULS downloader). "
        "Protection persists in rcache database, see 'rcache' subcommand on "
        "how to unprotect")
    parser_preload.set_defaults(func=do_preload)

    parser_load = subparsers.add_parser(
        "load", parents=[switches_common, switches_compose],
        help="Do load test")
    parser_load.add_argument(
        "--afc", metavar="[PROTOCOL://]HOST[:port][path][params]",
        help="AFC Server to send requests to. Unspecified parts are taken "
        "from 'rest_api.afc_req' of config file. By default determined by "
        "means of '--comp_proj'")
    parser_load.add_argument(
        "--localhost", nargs="?", choices=["http", "https"], const="http",
        help="If --afc not specified, default is to send requests to msghnd "
        "container (bypassing Nginx container). This flag causes requests to "
        "be sent to external http/https AFC port on localhost. If protocol "
        "not specified http is assumed")
    parser_load.add_argument(
        "--count", metavar="NUM_REQS", type=int, default=cfg.defaults.count,
        help=f"Number of requests to send. Default is {cfg.defaults.count}")
    parser_load.add_argument(
        "--no_cache", action="store_true",
        help="Don't use rcache, force each request to be computed")
    parser_load.set_defaults(func=do_load)

    parser_cache = subparsers.add_parser(
        "cache", parents=[switches_compose, switches_rcache],
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

    # Subparser for 'help' command
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
