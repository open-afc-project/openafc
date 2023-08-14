# Common stuff for ULS Service related scripts

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=wrong-import-order, invalid-name,
# pylint: disable=logging-fstring-interpolation

import __main__
import argparse
import datetime
import enum
import inspect
import json
import logging
import os
import re
import shutil
import sys
import tempfile
import time
from typing import Any, Callable, Dict, List, NamedTuple, Optional
import urllib.error
import urllib.request

# Default directory for status filers
DEFAULT_STATUS_DIR = os.path.join(os.path.dirname(__file__), "status")


def error(msg: str) -> None:
    """ Prints given msg as error message and exit abnormally """
    logging.fatal(msg)
    sys.exit(1)


def error_if(cond: Any, msg: str) -> None:
    """ If condition evaluates to true prints given msg as error message and
    exits abnormally """
    if cond:
        error(msg)


def dp(*args, **kwargs) -> None:
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
    logging.info(f"DP {frameinfo.function}()@{frameinfo.lineno}: {msg}")


def docker_arg_type(final_type: Callable[[Any], Any], default: Any = None,
                    required: bool = False) -> Callable[[str], Any]:
    """ Generator of argument converter for Docker environment

    I Docker environment empty argument values should be treated as None and
    boolean arguments should be converted from string values

    Arguments:
    final_type -- Type converter for nonempty argument
    default    -- Default value for empty argument
    required   -- True if argument is required (can't be empty)
    Returns actual argument converter function
    """
    assert (not required) or (default is None)

    def arg_converter(arg: str) -> Any:
        if not arg:
            if required:
                raise ValueError("Parameter is required")
            return default
        if final_type == bool:
            if arg.lower() in ("yes", "true", "+", "1"):
                return True
            if arg.lower() in ("no", "false", "-", "0"):
                return False
            raise \
                argparse.ArgumentTypeError(
                    "Wrong representation for boolean argument")
        try:
            return final_type(arg)
        except Exception as ex:
            raise argparse.ArgumentTypeError(repr(ex))

    return arg_converter


def setup_logging(verbose: bool) -> None:
    """ Logging setup

    Arguments:
    verbose -- enable debug printout
    """
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(
        logging.Formatter(
            f"{os.path.basename(__main__.__file__)}. "
            f"%(levelname)s: %(asctime)s %(message)s"))
    logging.getLogger().addHandler(console_handler)
    logging.getLogger().setLevel(
        logging.DEBUG if verbose else logging.INFO)


class StatusStorage:
    """ Storage of health check status (times when events happen)

    For now implemented on files. Quite ACID (as there is just one writer)

    Private attributes:
    _status_dir -- Directory where status files stored
    """

    class S(enum.Enum):
        """ Status items """

        # (Filename for) service start time
        ServiceBirth = "service_birth_time.txt"

        # (Filename for) service start time
        ServiceStart = "service_start_time.txt"

        # (Filename for) last download start time
        DownloadStart = "download_start_time.txt"

        # (Filename for) last download success time
        DownloadSuccess = "download_success_time.txt"

        # (Filename for last) data update time
        SqliteUpdate = "sqlite_update_time.txt"

        # (Filename for) last email alarm
        AlarmSent = "alarm_time.txt"

        # (Filename for) last healthcheck time
        HealthCheck = "last_healthcheck.txt"

        # (Filename for) last email beacon
        BeaconSent = "beacon_time.txt"

        # (Filename for) last data update time
        RegionUpdate = "region_update_time.json"

        # (Filename for) external parameter files' sync status
        ExtParamSyncStatus = "ext_param_sync_status.json"

    # Number of IO retries
    _NUM_IO_ATTEMPTS = 4

    # Delay between IO retries
    _IO_ATTEMPT_DELAY_SEC = 1

    def __init__(self, status_dir: str) -> None:
        """ Constructor

        Arguments:
        status_dir -- Directory for status files
        """
        self._status_dir = status_dir
        os.makedirs(self._status_dir, exist_ok=True)

    def write_milestone(self, status: "StatusStorage.S") -> None:
        """ Write milestone (that given event took place """
        self._write_file(filename=status.value,
                         data=datetime.datetime.now().isoformat())

    def read_milestone(self, status: "StatusStorage.S",
                       default: Optional[datetime.datetime] = None) \
            -> Optional[datetime.datetime]:
        """ Read milestone

        Arguments:
        status  -- Milestone identifier
        default -- What to return if milestone not yet happen
        Returns Milestone datetime or value of 'default' parameter
        """
        file_content = self._read_file(status.value)
        return default if file_content is None \
            else datetime.datetime.fromisoformat(file_content)

    def write_reg_data_changes(
            self, status: "StatusStorage.S",
            reg_data_changes: Dict[str, datetime.datetime]) -> None:
        """ Write region data change time

        Arguments:
        status           -- Status identifier
        reg_data_changes -- Dictionary of change times, indexed by region names
        """
        self._write_file(
            filename=status.value,
            data=json.dumps({reg: d.isoformat()
                             for reg, d in reg_data_changes.items()},
                            indent=4))

    def read_reg_data_changes(self, status: "StatusStorage.S") \
            -> Dict[str, datetime.datetime]:
        """ Read region change times

        Arguments:
        status -- Status identifier
        Returns dictionary of change times, indexed by region names
        """
        file_content = self._read_file(status.value)
        return {} if file_content is None \
            else {reg: datetime.datetime.fromisoformat(d)
                  for reg, d in json.loads(file_content).items()}

    def write_ext_param_sync_status(
            self,  ext_param_sync_status: Dict[str, str]) -> None:
        """ Write synchronization status for external parameter files

        Arguments:
        ext_param_sync_status -- Per-file report of synchronization status
        """
        self._write_file(
            filename=StatusStorage.S.ExtParamSyncStatus.value,
            data=json.dumps(ext_param_sync_status, indent=4))

    def read_ext_param_sync_status(self) -> Dict[str, str]:
        """ Read synchronization status for external parameter files

        Arguments:
        Returns dictionary of per-file status report
        """
        file_content = \
            self._read_file(StatusStorage.S.ExtParamSyncStatus.value)
        return {} if file_content is None else json.loads(file_content)

    def _read_file(self, filename: str) -> Optional[str]:
        """ Reads file

        Arguments:
        filename - file name
        Returns file content
        """
        full_filename = os.path.join(self._status_dir, filename)
        if not os.path.isfile(full_filename):
            return None
        for remaining_attempts in range(self._NUM_IO_ATTEMPTS, 0, -1):
            try:
                with open(full_filename, mode="r", encoding="ascii") as f:
                    return f.read()
            except OSError:
                if remaining_attempts > 1:
                    time.sleep(self._IO_ATTEMPT_DELAY_SEC)
                    continue
                raise
        return None     # Appeasing MyPy. Will never happen

    def _write_file(self, filename: str, data: str) -> None:
        """ Atomically write file

        Arguments
        filename -- File name
        data     -- Data to write
        """
        fd, temp = tempfile.mkstemp(dir=self._status_dir,
                                    suffix="_" + filename)
        os.close(fd)
        with open(temp, mode="w", encoding="ascii") as f:
            f.write(data)
        for remaining_attempts in range(self._NUM_IO_ATTEMPTS, 0, -1):
            try:
                shutil.move(temp, os.path.join(self._status_dir, filename))
                break
            except OSError:
                if remaining_attempts > 1:
                    time.sleep(self._IO_ATTEMPT_DELAY_SEC)
                    continue
                raise


class ExtParamFilesChecker:
    """ Checks that external parameter file match those in image

    Private attributes:
    _epf_list       -- List of external parameter file sets descriptors
                       (_ExtParamFiles objects) or None
    _script_dir     -- Downloader script directory or None
    _status_storage -- Status information persistent storage
    """

    # Descriptor of external parameter files
    _ExtParamFiles = \
        NamedTuple("_ExtParamFiles",
                   [
                    # Location in the internet
                    ("base_url", str),
                    # Downloader script subdirectory
                    ("subdir", str), ("files", List[str])])

    def __init__(self, status_storage: StatusStorage,
                 ext_files_arg: Optional[List[str]] = None,
                 script_dir: Optional[str] = None) -> None:
        """ Constructor

        Arguments:
        ext_files_arg  -- List of 'BASE_URL:SUBDIR:FILES,FILE... external
                          parameter file descriptors from command line
        status_storage -- Status information persistent storage
        """
        self._epf_list: \
            Optional[List["ExtParamFilesChecker._ExtParamFiles"]] = None
        if ext_files_arg is not None:
            self._epf_list = []
            for ef in ext_files_arg:
                m = re.match(r"^(.*):(.+):(.+)$", ef)
                error_if(
                    m is None,
                    f"Invalid format of --check_ext_files parameter: '{ef}'")
                assert m is not None
                self._epf_list.append(
                    self._ExtParamFiles(base_url=m.group(1).rstrip("/"),
                                        subdir=m.group(2),
                                        files=m.group(3).split(",")))
        self._script_dir = script_dir
        self._status_storage = status_storage

    def check(self) -> None:
        """ Check that external files matches those in image """
        assert (self._epf_list is not None) and (self._script_dir is not None)
        temp_dir: Optional[str] = None
        status_dict: Dict[str, str] = {}
        try:
            temp_dir = tempfile.mkdtemp()
            for epf in self._epf_list:
                for filename in epf.files:
                    key = os.path.join(epf.subdir, filename)
                    internal_file_name = \
                        os.path.join(self._script_dir, epf.subdir, filename)
                    if not os.path.isfile(internal_file_name):
                        status_dict[key] = "Absent in container"
                        continue
                    try:
                        url = f"{epf.base_url}/{filename}"
                        external_file_name = os.path.join(temp_dir, filename)
                        urllib.request.urlretrieve(url, external_file_name)
                    except urllib.error.URLError as ex:
                        logging.warning(f"Error downloading '{url}': {ex}")
                        status_dict[key] = "Download failed"
                        continue
                    contents: List[bytes] = []
                    try:
                        for fn in (internal_file_name, external_file_name):
                            with open(fn, "rb") as f:
                                contents.append(f.read())
                    except OSError as ex:
                        logging.warning(f"Read failed: {ex}")
                        status_dict[key] = "Read failed"
                        continue
                    status_dict[key] = \
                        "External content changed" \
                        if contents[0] != contents[1] else "OK"
        finally:
            if temp_dir:
                shutil.rmtree(temp_dir, ignore_errors=True)
        self._status_storage.write_ext_param_sync_status(status_dict)

    def get_problems(self) -> List[str]:
        """ Return list of lines that describe problematic files """
        status_dict = self._status_storage.read_ext_param_sync_status()
        return [f"{key}: {value}" for key, value in status_dict.items()
                if value != "OK"]
