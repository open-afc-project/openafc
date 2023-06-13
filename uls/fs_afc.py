#!/usr/bin/env python3
# Makes AFC Requests with custom FS database

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=wrong-import-order, too-many-statements, too-many-branches
# pylint: disable=invalid-name, too-many-locals, logging-fstring-interpolation

import argparse
import concurrent.futures
import copy
import datetime
import http
import json
import logging
import os
import random
import re
import requests
import string
import sys
from typing import Any, Dict, List, NamedTuple, Optional, Union
import yaml

# Name of the default config file
DEAFULT_CONFIG = os.path.realpath(os.path.splitext(__file__)[0] + ".yaml")


# Pydantic wouldn't be out of place here...

# Config key for default number of parallel requests
DEFAULT_PARALLEL_KEY = "default_parallel"

# Config key for default request timeout in minutes
DEFAULT_TIMEOUT_MIN_KEY = "default_timeout_min"

# Config key for request format
REQ_ID_FORMAT_KEY = "req_id_format"

# Config key for request pattern
REQ_PATTERN_KEY = "req_pattern"

# Config key for region definition patterns
REGION_PATTERNS_KEY = "region_patterns"

# Config key for path definitions
PATHS_KEY = "paths"

# Config subkey for path to Request ID in AFC Request JSON
REQ_ID_PATH_KEY = "request_id"

# Config subkey for path to region definition in AFC Request JSON
REGION_PATH_KEY = "region"

# Config subkey for path to coordinates in AFC Request JSON
COORD_PATH_KEY = "coordinates"

# Config subkey for path to FS Database in AFC Request JSON
FS_DATABASE_PATH_KEY = "fs_database"

# Config subkey for path to response code in AFC Response JSON
RESPONSE_CODE_PATH_KEY = "response_code"

# Config subkey in response description in AFC Response JSON
RESPONSE_DESC_PATH_KEY = "response_desc"

# Config subkey for descrition substring of statuses to ignore
SUCCESS_STATUSES_KEY = "success_statuses"

# Config subkey for code of ignored status
STATUS_CODE_KEY = "code"

# Config subkey for description of ignore dstatus
STATUS_DESC_KEY = "desc_substr"

# Config key for point definition patterns
POINT_PATTERNS_KEY = "point_patterns"

# Config subkey for point name
POINT_NAME_KEY = "name"

# Config subkey for point coordinates
POINT_COORD_DICT_KEY = "coordinates"

# Config subkey for point latitude
POINT_COORD_LAT_KEY = "latitude"

# Config subkey to point longitude
POINT_COORD_LON_KEY = "longitude"


def error(msg: str) -> None:
    """ Prints given msg as error message and exits abnormally """
    logging.error(msg)
    sys.exit(1)


def error_if(cond: Any, msg: str) -> None:
    """ If condition evaluates to true prints given msg as error message and
    exits abnormally """
    if cond:
        error(msg)


def json_substitute(json_obj: Union[List[Any], Dict[str, Any]],
                    path: List[Union[int, str]], value: Any) -> None:
    """ Substitute JSON element at given path with given value

    Element at given path must exist and its current value should be 'null'

    Arguments:
    json_obj -- JSON Dictionary. Updated inplace
    path     -- Path (list of indices) leading to desired element
    value    -- New value
    """
    error_if(not isinstance(json_obj, dict),
             "Attempt to JSON-substitute in non-JSON object")
    error_if(len(path) == 0, "JSON substitution path may not be empty")
    container: Optional[Union[List[Any], Dict[str, Any]]] = None
    original_obj = json_obj
    for idx, path_elem in enumerate(path):
        if isinstance(path_elem, int):
            error_if(not (isinstance(json_obj, list) and
                          (0 <= path_elem < len(json_obj))),
                     f"Path {path[:idx + 1]} is invalid for '{original_obj}'")
        elif isinstance(path_elem, str):
            error_if(not (isinstance(json_obj, dict) and
                          (path_elem in json_obj)),
                     f"Path {path[:idx + 1]} is invalid for '{original_obj}'")
        else:
            error(f"Path index '{path_elem}' has invalid type. Must be string "
                  f"or integer")
        container = json_obj
        json_obj = json_obj[path_elem]
    error_if(json_obj is not None,
             f"Path {path} is invalid for '{original_obj}' - must end on "
             f"'null' element, instead ends on '{json_obj}'")
    container[path[-1]] = value


def json_retrieve(json_obj: Optional[Union[List[Any], Dict[str, Any]]],
                  path=List[Union[int, str]]) -> Any:
    """ Try to read value at given path in given JSON

    Arguments:
    json_obj -- JSON object to read from
    path     -- Path (sequence of indices) leading to desired element
    Returns element value or None
    """
    original_obj = json_obj
    for idx, path_elem in enumerate(path):
        if json_obj is None:
            return None
        if isinstance(path_elem, int):
            error_if(not isinstance(json_obj, list),
                     f"Path {path[:idx + 1]} is invalid for '{original_obj}'")
            if not (0 <= path_elem < len(json_obj)):
                return None
        elif isinstance(path_elem, str):
            error_if(not isinstance(json_obj, dict),
                     f"Path {path[:idx + 1]} is invalid for '{original_obj}'")
            if path_elem not in json_obj:
                return None
        else:
            error(f"Path index '{path_elem}' has invalid type. Must be string "
                  f"or integer")
        json_obj = json_obj[path_elem]
    return json_obj


# Response from request worker function
ResponseInfo = \
    NamedTuple(
        "ResponseInfo",
        [
         # AFC Response object or None
         ("afc_response", Optional[Dict[str, Any]]),
         # True if there was a timeout
         ("timeout", bool),
         # HTTP Status code
         ("status_code", Optional[int]),
         # Request duration in seconds
         ("duration_sec", int)])


def do_request(req: Dict[str, Any], url: str, timeout_sec: float) \
        -> ResponseInfo:
    """ Thread worker function that performs AFC Request

    Arguments
    req         -- AFC Request JSON dictionary
    url         -- Request URL
    timeout_sec -- Timeout in seconds
    Returns ResponseInfo object
    """
    timeout = False
    start_time = datetime.datetime.now()
    result: Optional[requests.Response] = None
    try:
        result = requests.post(url=url, json=req, timeout=timeout_sec)
    except requests.Timeout:
        timeout = True
    return \
        ResponseInfo(
            afc_response=result.json()
            if (result is not None) and
            (result.status_code == http.HTTPStatus.OK)
            else None,
            timeout=timeout,
            status_code=result.status_code if result is not None else None,
            duration_sec=int((datetime.datetime.now() -
                              start_time).total_seconds()))


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = \
        argparse.ArgumentParser(
            description="Makes AFC Requests with custom FS database")
    argument_parser.add_argument(
        "--config", metavar="CONGIG", default=DEAFULT_CONFIG,
        help=f"Config file name. Default is '{DEAFULT_CONFIG}'")
    argument_parser.add_argument(
        "--server_url", metavar="URL", required=True,
        help="URL of AFC Service to use (should accept "
        "OpenAfcOverrideAfcConfig Vendor Extension)")
    argument_parser.add_argument(
        "--region", metavar="REGION", action="append", default=[],
        help="Region to run tests for. This parameter may be specified "
        "several times. Default is to run for all regions")
    argument_parser.add_argument(
        "--parallel", metavar="NUM_PARALLEL_REQS", type=int,
        help="Number of parallel requests. Default see in the config file")
    argument_parser.add_argument(
        "--timeout_min", metavar="MINUTES", type=float,
        help="Request timeout in minutes. Default see in the config file")
    argument_parser.add_argument(
        "--point", metavar="LAT_DEG,LON_DEG OR NAME", action="append",
        help="Run request for given point. Point specified either by "
        "coordinates (north/east positive degrees) or by name in config file. "
        "In former case exactly one --region should be specified, in latter "
        "case - only if it is required for disambiguation. This parameter may "
        "be specified several times")
    argument_parser.add_argument(
        "--failed_json", metavar="FILENAME",
        help="Write json that caused fail to file")
    argument_parser.add_argument(
        "FS_DATABASE", help="FS (aka ULS) database (.sqlite3 file) to use")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)

    args = argument_parser.parse_args(argv)

    console_handler = logging.StreamHandler()
    console_handler.setFormatter(
        logging.Formatter(
            f"{os.path.basename(__file__)}. "
            f"%(levelname)s: %(asctime)s %(message)s"))
    logging.getLogger().addHandler(console_handler)
    logging.getLogger().setLevel(logging.INFO)

    error_if(not os.path.isfile(args.config),
             f"Config file '{args.config}' not found")
    with open(args.config, mode="r", encoding="utf-8") as f:
        config = \
            yaml.load(
                f.read(),
                Loader=yaml.CLoader if hasattr(yaml, "CLoader")
                else yaml.Loader)

    if args.region:
        error_if(
            not all(r in config[REGION_PATTERNS_KEY] for r in args.region),
            f"Unknown region code '{args.region[0]}'. Known regions are: "
            f"{', '.join(sorted(config[REGION_PATTERNS_KEY].keys()))}")

    start_time = datetime.datetime.now()

    req_pattern_s = config[REQ_PATTERN_KEY]
    try:
        req_pattern = json.loads(req_pattern_s)
    except json.JSONDecodeError as ex:
        error(f"Syntax error in config's request pattern: {ex}")

    # Building per-region lists of points to send requests for
    region_points: Dict[str, List[Dict[str, Any]]] = {}
    if args.point:
        for point in args.point:
            m = re.match(r"^([0-9+-.]+),([0-9+-.]+)$", point)
            point_dict: Dict[str, Any]
            if m:
                error_if(len(args.region) != 1,
                         "One '--region' parameter must be specified along "
                         "with coordinate-based '--point' parameter")
                point_dict = {POINT_NAME_KEY: point}
                for field, value_s in [(POINT_COORD_LAT_KEY, m.group(1)),
                                       (POINT_COORD_LON_KEY, m.group(2))]:
                    try:
                        point_dict.setdefault(POINT_COORD_DICT_KEY,
                                              {})[field] = float(value_s)
                    except ValueError:
                        error(
                            f"Wrong structure of point coordinates '{point}'")
                region_points.setdefault(args.region[0], []).append(point_dict)
            else:
                found = False
                for region in config[POINT_PATTERNS_KEY]:
                    if args.region and region not in args.region:
                        continue
                    for point_dict in config[POINT_PATTERNS_KEY][region]:
                        if point_dict[POINT_NAME_KEY] == point:
                            region_points.setdefault(region,
                                                     []).append(point_dict)
                            found = True
                error_if(not found,
                         f"Point '{point}' not found in config file")
    else:
        for region in config[POINT_PATTERNS_KEY]:
            error_if(region not in config[REGION_PATTERNS_KEY],
                     f"Config file's '{POINT_PATTERNS_KEY}' section contains "
                     f"region code of '{region}', not found in config's "
                     f"'{REGION_PATTERNS_KEY}' section")
            if args.region and (region not in args.region):
                continue
            region_points[region] = config[POINT_PATTERNS_KEY][region]

    request_timeout_sec = \
        60 * (args.timeout_min if args.timeout_min is not None
              else config[DEFAULT_TIMEOUT_MIN_KEY])
    max_workers = args.parallel if args.parallel is not None \
        else config[DEFAULT_PARALLEL_KEY]

    ReqInfo = NamedTuple("ReqInfo", [("name", str), ("region", str),
                                     ("req", Dict[str, Any])])
    success = False
    with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) \
            as executor:
        try:
            paths: Dict[str, List[Union[str, int]]] = config[PATHS_KEY]
            future_to_req_info: Dict[concurrent.futures.Future, ReqInfo] = {}
            # Starting requests
            for region, point_list in region_points.items():
                for point in point_list:
                    req = copy.deepcopy(req_pattern)
                    json_substitute(
                        json_obj=req, path=paths[REQ_ID_PATH_KEY],
                        value=config[REQ_ID_FORMAT_KEY].format(
                            req_idx=len(future_to_req_info), region=region,
                            random_str="".join(
                                random.choices(
                                    string.ascii_uppercase + string.digits,
                                    k=10))))
                    json_substitute(json_obj=req, path=paths[REGION_PATH_KEY],
                                    value=config[REGION_PATTERNS_KEY][region])
                    json_substitute(json_obj=req, path=paths[COORD_PATH_KEY],
                                    value=point[POINT_COORD_DICT_KEY])
                    json_substitute(json_obj=req,
                                    path=paths[FS_DATABASE_PATH_KEY],
                                    value=args.FS_DATABASE)
                    future_to_req_info[
                        executor.submit(
                            do_request, req=req, url=args.server_url,
                            timeout_sec=request_timeout_sec
                            )] = ReqInfo(name=point[POINT_NAME_KEY],
                                         region=region, req=req)
            # Processing finished requests
            for future in concurrent.futures.as_completed(future_to_req_info):
                req_info = future_to_req_info[future]
                try:
                    exception = future.exception()
                    error_if(exception is not None,
                             f"Request '{req_info.name}' ended with "
                             f"exception: {exception}")
                    response_info: Optional[ResponseInfo] = future.result()
                    assert response_info is not None
                    error_if(response_info.timeout,
                             f"Request '{req_info.name}' timed out")
                    error_if(response_info.status_code != http.HTTPStatus.OK,
                             f"Request '{req_info.name}' timed out ended with "
                             f"status code {response_info.status_code}")
                    response_code = \
                        json_retrieve(json_obj=response_info.afc_response,
                                      path=paths[RESPONSE_CODE_PATH_KEY])
                    response_desc = \
                        json_retrieve(json_obj=response_info.afc_response,
                                      path=paths[RESPONSE_DESC_PATH_KEY])
                    for ss in config[SUCCESS_STATUSES_KEY]:
                        if (STATUS_CODE_KEY in ss) and \
                                (ss[STATUS_CODE_KEY] != response_code):
                            continue
                        if (STATUS_DESC_KEY in ss) and \
                                (ss[STATUS_DESC_KEY] not in
                                 (response_desc or "")):
                            continue
                        break
                    else:
                        response_desc_msg = \
                            "" if response_desc is None \
                            else f" ({response_desc})"
                        error(f"Request '{req_info.name}' ended with "
                                 f"AFC response code "
                                 f"{response_code}{response_desc_msg}")
                    logging.info(f"Request '{req_info.name}'"
                                 f"({req_info.region}) successfully completed "
                                 f"in {response_info.duration_sec} seconds")
                except KeyboardInterrupt:
                    break
                except:  # noqa
                    if args.failed_json:
                        with open(args.failed_json, mode="w",
                                  encoding="utf-8") as f:
                            json.dump(req_info.req, f, indent=4,
                                      sort_keys=False)
                        logging.info(
                            f"Offending request JSON written to "
                            f"'{os.path.realpath(args.failed_json)}'. Happy "
                            f"curling")
                    raise
            success = True
        except KeyboardInterrupt:
            pass
        finally:
            if not success:
                logging.info("Waiting for completion of in-progress requests")
            executor.shutdown(wait=True, cancel_futures=True)
        elapsed_seconds = \
            int((datetime.datetime.now() - start_time).total_seconds())
        logging.info(f"{len(future_to_req_info)} requests were processed in "
                     f"{elapsed_seconds // 60} min {elapsed_seconds % 60} sec")


if __name__ == "__main__":
    main(sys.argv[1:])
