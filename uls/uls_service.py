#!/usr/bin/env python3
""" ULS data download control service """

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=unused-wildcard-import, wrong-import-order, wildcard-import
# pylint: disable=too-many-statements, too-many-branches, unnecessary-pass
# pylint: disable=logging-fstring-interpolation, invalid-name, too-many-locals
# pylint: disable=too-few-public-methods, too-many-arguments
# pylint: disable=too-many-nested-blocks, too-many-lines
# pylint: disable=too-many-instance-attributes

import argparse
import datetime
import glob
import logging
import os
import prometheus_client
import pydantic
import re
import shlex
import shutil
import signal
import sqlalchemy as sa
import statsd
import subprocess
import sys
import tempfile
import threading
import time
from typing import cast, Dict, Iterable, List, NamedTuple, Optional, Tuple, \
    Union
import urllib.error
import urllib.request

from rcache_client import RcacheClient
from rcache_models import LatLonRect, RcacheClientSettings
from uls_service_common import *
from uls_service_state_db import CheckType, DownloaderMilestone, LogType, \
    safe_dsn, StateDb

# Filemask for ULS databases
ULS_FILEMASK = "*.sqlite3"

# ULS database identity table
DATA_IDS_TABLE = "data_ids"

# Identity table region column
DATA_IDS_REG_COLUMN = "region"

# Identity table region ID column
DATA_IDS_ID_COLUMN = "identity"

# Name of FSID tool script
FSID_TOOL = os.path.join(os.path.dirname(__file__), "fsid_tool.py")

# Name of FS DB Diff script
FS_DB_DIFF = os.path.join(os.path.dirname(__file__), "fs_db_diff.py")

# Name of FS AFC test script
FS_AFC = os.path.join(os.path.dirname(__file__), "fs_afc.py")

# Healthcheck script
HEALTHCHECK_SCRIPT = os.path.join(os.path.dirname(__file__),
                                  "uls_service_healthcheck.py")

# Default StatsD port
DEFAULT_STATSD_PORT = 8125


class Settings(pydantic.BaseSettings):
    """ Arguments from command lines - with their default values """
    download_script: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/daily_uls_parse/daily_uls_parse.py",
            env="ULS_DOWNLOAD_SCRIPT",
            description="FS download script")
    download_script_args: Optional[str] = \
        pydantic.Field(None, env="ULS_DOWNLOAD_SCRIPT_ARGS",
                       description="Additional download script parameters")
    region: Optional[str] = \
        pydantic.Field(None, env="ULS_DOWNLOAD_REGION",
                       description="Download regions", no="All")
    result_dir: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/ULS_Database/", env="ULS_RESULT_DIR",
            description="Directory where download script puts downloaded file")
    temp_dir: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/daily_uls_parse/temp/", env="ULS_TEMP_DIR",
            description="Temporary directory of ULS download script, cleaned "
            "before downloading")
    ext_db_dir: str = \
        pydantic.Field(
            ..., env="ULS_EXT_DB_DIR",
            description="Ultimate downloaded file destination directory")
    ext_db_symlink: str = \
        pydantic.Field(..., env="ULS_CURRENT_DB_SYMLINK",
                       description="Symlink pointing to current ULS file")
    fsid_file: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/daily_uls_parse/data_files/fsid_table.csv",
            env="ULS_FSID_FILE",
            description="FSID file location expected by ULS download script")
    ext_ras_database: str = \
        pydantic.Field(..., env="ULS_EXT_RAS_DATABASE",
                       description="RAS database")
    ras_database: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/daily_uls_parse/data_files/RASdatabase.dat",
            env="ULS_RAS_DATABASE",
            description="Where from ULS script reads RAS database")
    service_state_db_dsn: str = \
        pydantic.Field(
            ..., env="ULS_SERVICE_STATE_DB_DSN",
            description="Connection string to service state database",
            convert=safe_dsn)
    service_state_db_create_if_absent: bool = \
        pydantic.Field(
            True, env="ULS_SERVICE_STATE_DB_CREATE_IF_ABSENT",
            description="Create service state database if it is absent")
    service_state_db_recreate: bool = \
        pydantic.Field(
            False, env="ULS_SERVICE_STATE_DB_RECREATE",
            description="Recreate service state database if it exists")
    prometheus_port: Optional[int] = \
        pydantic.Field(None, env="ULS_PROMETHEUS_PORT",
                       description="Port to serve Prometheus metrics on")
    statsd_server: Optional[str] = \
        pydantic.Field(None, env="ULS_STATSD_SERVER",
                       description="StatsD server to send metrics to")
    check_ext_files: Optional[List[str]] = \
        pydantic.Field(
            "https://raw.githubusercontent.com/Wireless-Innovation-Forum/"
            "6-GHz-AFC/main/data/common_data"
            ":raw_wireless_innovation_forum_files"
            ":antenna_model_diameter_gain.csv,billboard_reflector.csv,"
            "category_b1_antennas.csv,high_performance_antennas.csv,"
            "fcc_fixed_service_channelization.csv,"
            "transmit_radio_unit_architecture.csv",
            env="ULS_CHECK_EXT_FILES",
            description="Verify that that files are the same as in internet",
            no="None")
    max_change_percent: Optional[float] = \
        pydantic.Field(
            10., env="ULS_MAX_CHANGE_PERCENT",
            description="Limit on number of paths changed",
            convert=lambda v: f"{v}%" if v else "Don't check")
    afc_url: Optional[str] = \
        pydantic.Field(
            None, env="ULS_AFC_URL",
            description="AFC Service URL to use for database validity check",
            no="Don't check")
    afc_parallel: Optional[int] = \
        pydantic.Field(
            None, env="ULS_AFC_PARALLEL",
            description="Number of parallel AFC Requests to use when doing "
            "validity check", no="fs_afc.py's default")
    rcache_url: Optional[str] = \
        pydantic.Field(None, env="RCACHE_SERVICE_URL",
                       description="Rcache service url",
                       no="Don't do spatial invalidation")
    rcache_enabled: bool = \
        pydantic.Field(True, env="RCACHE_ENABLED",
                       description="Rcache spatial invalidation",
                       yes="Enabled", no="Disabled")
    delay_hr: float = \
        pydantic.Field(0., env="ULS_DELAY_HR",
                       description="Hours to delay first download by")
    interval_hr: float = \
        pydantic.Field(4, env="ULS_INTERVAL_HR",
                       description="Download interval in hours")
    timeout_hr: float = \
        pydantic.Field(1, env="ULS_TIMEOUT_HR",
                       description="Download maximum duration in hours")
    nice: bool = \
        pydantic.Field(False, env="ULS_NICE",
                       description="Run in lowered (nice) priority")
    verbose: bool = \
        pydantic.Field(False, description="Print debug info")
    run_once: bool = \
        pydantic.Field(False, env="ULS_RUN_ONCE",
                       description="Run", yes="Once", no="Indefinitely")
    force: bool = \
        pydantic.Field(False,
                       description="Force FS database update (even if not "
                       "changed or found invalid)")

    @pydantic.validator("check_ext_files", pre=True)
    @classmethod
    def check_ext_files_str_to_list(cls, v: Any) -> Any:
        """ Converts string value of 'check_ext_files' from environment from
        string to list (as it is list in argparse) """
        return [v] if v and isinstance(v, str) else v

    @pydantic.validator("statsd_server", pre=False)
    @classmethod
    def check_statsd_server(cls, v: Any) -> Any:
        """ Applies default StatsD port """
        if v:
            if ":" not in v:
                v = f"{v}:{DEFAULT_STATSD_PORT}"
            else:
                host, port = v.split(":", 1)
                int(port)
                assert host
        return v

    @pydantic.root_validator(pre=True)
    @classmethod
    def remove_empty(cls, v: Any) -> Any:
        """ Prevalidator that removes empty values (presumably from environment
        variables) to force use of defaults
        """
        if not isinstance(v, dict):
            return v
        for key, value in list(v.items()):
            if value in (None, "", []):
                del v[key]
        return v


class ProcessingException(Exception):
    """ ULS processing exception """
    pass


def print_args(settings: Settings) -> None:
    """ Print invocation parameters to log """
    logging.info("FS downloader started with the following parameters")
    for name, model_field in settings.__fields__.items():
        value = getattr(settings, name)
        extra = getattr(model_field.field_info, "extra", {})
        value_repr: str
        if "convert" in extra:
            value_repr = extra["convert"](value)
        elif value and ("yes" in extra):
            value_repr = extra["yes"]
        elif (not value) and ("no" in extra):
            value_repr = extra["no"]
        elif model_field.type_ == bool:
            value_repr = "Yes" if value else "No"
        else:
            value_repr = str(value)
        logging.info(f"{model_field.field_info.description}: {value_repr}")


class LoggingExecutor:
    """ Program executor that collects output

    Private attributes:
    _lines -- Output lines
    """
    def __init__(self) -> None:
        self._lines: List[str] = []

    def execute(self, cmd: Union[str, List[str]], fail_on_error: bool,
                return_output: bool = False, cwd: Optional[str] = None,
                timeout_sec: Optional[float] = None) -> \
            Union[bool, Optional[str]]:
        """ Execute command

        Arguments:
        cmd           -- Command as string or list of strings
        fail_on_error -- True to raise exception on error, False to return
                         failure code on error
        return_output -- True to return output (None on failure), False to
                         return boolean success status
        cwd           -- Directory to execute in or None
        timeout_sec   -- Timeout in seconds or None
        Returns If 'return_output' - returns output/None on success/failure,
        otherwise returns boolean success status
        """
        ret_lines: Optional[List[str]] = [] if return_output else None
        success = True
        timed_out = False

        def killer_timer(pgid: int) -> None:
            """ Kills given process by process group id (rumors are that
            os.kill() only adequate if shell=False)
            """
            try:
                os.killpg(pgid, signal.SIGTERM)
                timed_out = True
            except OSError:
                pass

        self._lines.append(
            "> " +
            (cmd if isinstance(cmd, str)
             else ''.join(shlex.quote(arg) for arg in cmd)) +
            "\n")
        try:
            with subprocess.Popen(cmd, shell=isinstance(cmd, str), text=True,
                                  encoding="utf-8", stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, bufsize=0,
                                  cwd=cwd) as p:
                timer: Optional[threading.Timer] = \
                    threading.Timer(timeout_sec, killer_timer,
                                    kwargs={"pgid": os.getpgid(p.pid)}) \
                    if timeout_sec is not None else None
                assert p.stdout is not None
                for line in p.stdout:
                    print(line, end="", flush=True)
                    self._lines.append(line)
                    if ret_lines is not None:
                        ret_lines.append(line)
                p.wait()
                if timer is not None:
                    timer.cancel()
                if timed_out:
                    raise subprocess.TimeoutExpired(cmd,
                                                    cast(float, timeout_sec))
                if p.returncode:
                    raise subprocess.CalledProcessError(p.returncode, p.args)
        except (OSError, subprocess.SubprocessError) as ex:
            success = False
            self._lines.append(f"{ex}\n")
            if fail_on_error:
                raise
        if return_output:
            assert ret_lines is not None
            return "".join(ret_lines) if success else None
        return success

    def get_output(self) -> str:
        """ Returns and resets accumulated output """
        ret = "".join(self._lines)
        self._lines = []
        return ret


class StatusUpdater:
    """ Updater of State DB and Prometheus/StatsD metrics

    Private attributes:
    _state_db                  -- StateDb object to update
    _prometheus_metrics        -- By-milestone dictionary of region-inspecific
                                  Prometheus metrics. Metrics are gauges,
                                  containing seconds since epoch of recent
                                  milestone occurrence. Empty if Prometheus
                                  metrics are not served
    _prometheus_region_metrics -- By-milestone dictionary of region-specific
                                  Prometheus metrics. Metrics are gauges,
                                  containing seconds since epoch of recent
                                  milestone occurrence. Each metric has
                                  'region' label. Empty if Prometheus metrics\
                                  are not served
    _prometheus_check_metric   -- None or Prometheus gauge, with 'check_type'
                                  label (containing name of CheckType item),
                                  containing 1 if check passed, 0 if not
    _statsd_connection         -- statsd.Connection object or None
    _statsd_metrics            -- By-milestone dictionary of region-inspecific
                                  StatsD metrics. Metrics are gauges,
                                  containing seconds since epoch of recent
                                  milestone occurrence. Empty if StatsD metrics
                                  are not served
    _statsd_region_metrics     -- By-milestone dictionary of region-specific
                                  StatsD metric descriptors. Empty if StatsD
                                  metrics are not served
    _statsd_check_metrics      -- None or check result StatsD gauge descriptor
    """
    # StatsD metric descriptor (working around the absence of labels in StatsD
    # by putting them into name)
    _StatsdLabeledMetricInfo = \
        NamedTuple(
            "_StatsdLabeledMetricInfo",
            [
             # String.format()-compatible pattern, containing placeholder for
             # label value
             ("pattern", str),
             # Dictionary of StatsD metrics, indexed by label value name
             ("metrics", Dict[str, statsd.Gauge])])

    def __init__(self, state_db: StateDb, prometheus_port: Optional[int],
                 statsd_server: Optional[str]) -> None:
        """ Constructor

        Arguments:
        state_db        -- StateDb object
        prometheus_port -- Port to serve Prometheus metrics on or None
        statsd_server   -- Address of StatsD server to send metrics to or None
        """
        self._state_db = state_db
        self._prometheus_metrics: Dict[DownloaderMilestone, Any] = {}
        self._prometheus_region_metrics: Dict[DownloaderMilestone, Any] = {}
        self._prometheus_check_metric: Any = None
        self._statsd_connection: Optional[statsd.Connection] = None
        self._statsd_metrics: Dict[DownloaderMilestone, Any] = {}
        self._statsd_region_metrics: \
            Dict[DownloaderMilestone,
                 "StatusUpdater._StatsdLabeledMetricInfo"] = {}
        self._statsd_check_metrics: \
            Optional["StatusUpdater._StatsdLabeledMetricInfo"] = None
        if prometheus_port is not None:
            prometheus_client.start_http_server(prometheus_port)
            self._prometheus_metrics[DownloaderMilestone.DownloadStart] = \
                prometheus_client.Gauge(
                    "fs_download_started",
                    "Seconds since epoch of last downloader script run")
            self._prometheus_metrics[DownloaderMilestone.DownloadSuccess] = \
                prometheus_client.Gauge(
                    "fs_download_succeeded",
                    "Seconds since epoch of last downloader script success")
            self._prometheus_metrics[DownloaderMilestone.DbUpdated] = \
                prometheus_client.Gauge(
                    "fs_download_database_updated",
                    "Seconds since epoch of last FS database file update")
            self._prometheus_region_metrics[
                DownloaderMilestone.RegionChanged] = \
                prometheus_client.Gauge(
                    "fs_download_region_changed",
                    "Seconds since epoch of last region changed", ["region"])
            self._prometheus_check_metric = \
                prometheus_client.Gauge(
                    "fs_download_check_passed",
                    "Recent check state (1 - success, 0 - failure)",
                    ["check_type"])
        if statsd_server:
            host, port = statsd_server.split(":", 1)
            self._statsd_connection = \
                statsd.Connection(host=host, port=int(port))
            self._statsd_metrics[DownloaderMilestone.DownloadStart] = \
                statsd.Gauge("fs_download_started")
            self._statsd_metrics[DownloaderMilestone.DownloadSuccess] = \
                statsd.Gauge("fs_download_succeeded")
            self._statsd_metrics[DownloaderMilestone.DbUpdated] = \
                statsd.Gauge("fs_download_database_updated")
            self._statsd_region_metrics[DownloaderMilestone.RegionChanged] = \
                self._StatsdLabeledMetricInfo(
                    pattern="fs_download_region_changed_{region}", metrics={})
            self._statsd_check_metrics = \
                self._StatsdLabeledMetricInfo(
                    pattern="fs_download_check_passed_{check_type}",
                    metrics={})

    def milestone(self, milestone: DownloaderMilestone,
                  updated_regions: Optional[Iterable[str]] = None,
                  all_regions: Optional[List[str]] = None) -> None:
        """ Update milestone

        Arguments:
        milestone        -- Milestone to write
        updated_regions  -- List of region strings of updated regions, None for
                            region-inspecific milestones
        all_regions      -- List of all region strings,  None for
                            region-inspecific milestones
        """
        self._state_db.write_milestone(
            milestone=milestone, updated_regions=updated_regions,
            all_regions=all_regions)
        seconds_since_epoch = int(time.time())
        if milestone in self._prometheus_metrics:
            self._prometheus_metrics[milestone].set(seconds_since_epoch)
        if (milestone in self._prometheus_region_metrics) and updated_regions:
            for region in updated_regions:
                self._prometheus_region_metrics[milestone].\
                    labels(region=region).set(seconds_since_epoch)
        if milestone in self._statsd_metrics:
            self._statsd_metrics[milestone].send(seconds_since_epoch)
        if (milestone in self._statsd_region_metrics) and updated_regions:
            mi = self._statsd_region_metrics[milestone]
            for region in updated_regions:
                metric: Optional[statsd.Gauge] = mi.metrics.get(region)
                if not metric:
                    metric = statsd.Gauge(mi.pattern.format(region=region))
                    mi.metrics[region] = metric
                metric.send(seconds_since_epoch)

    def status_check(self, check_type: CheckType,
                     results: Optional[Dict[str, Optional[str]]] = None) \
            -> None:
        """ Update status check results

        Arguments:
        check_type -- Type of check
        results    -- Itemized results: dictionary contained error message for
                      failed checks, None for succeeded ones
        """
        self._state_db.write_check_results(check_type=check_type,
                                           results=results)
        success = all(result is None for result in (results or {}).values())
        if self._prometheus_check_metric:
            self._prometheus_check_metric.labels(check_type=check_type.name).\
                set(1 if success else 0)
        if self._statsd_check_metrics:
            metric: Optional[statsd.Gauge] = \
                self._statsd_check_metrics.metrics.get(check_type.name)
            if not metric:
                metric = \
                    statsd.Gauge(
                        self._statsd_check_metrics.pattern.format(
                            check_type=check_type.name))
                self._statsd_check_metrics.metrics[check_type.name] = metric
            metric.send(1 if success else 0)


def extract_fsid_table(uls_file: str, fsid_file: str,
                       executor: LoggingExecutor) -> None:
    """ Try to extract FSID table from ULS database:

    Arguments:
    uls_file  -- FS database filename
    fsid_file -- FSID table filename
    executor  -- LoggingExecutor object
    """
    # Clean previous FSID files...
    fsid_name_parts = os.path.splitext(fsid_file)
    logging.info("Extracting FSID table")
    if not executor.execute(f"rm -f {fsid_name_parts[0]}*{fsid_name_parts[1]}",
                            fail_on_error=False):
        logging.warning(f"Strangely can't remove "
                        f"{fsid_name_parts[0]}*{fsid_name_parts[1]}. "
                        f"Proceeding nevertheless")
    # ... and extracting latest one from previous FS database
    if os.path.isfile(uls_file):
        if executor.execute([FSID_TOOL, "check", uls_file],
                            fail_on_error=False):
            executor.execute([FSID_TOOL, "extract", uls_file, fsid_file],
                             fail_on_error=True)


def get_uls_identity(uls_file: str) -> Optional[Dict[str, str]]:
    """ Read regions' identity from ULS database

    Arguments:
    uls_file -- ULS database
    Returns dictionary of region identities indexed by region name
    """
    if not os.path.isfile(uls_file):
        return None
    engine = sa.create_engine("sqlite:///" + uls_file)
    conn = engine.connect()
    try:
        metadata = sa.MetaData()
        metadata.reflect(bind=engine)
        id_table = metadata.tables.get(DATA_IDS_TABLE)
        if id_table is None:
            return None
        if not all(col in id_table.c for col in (DATA_IDS_REG_COLUMN,
                                                 DATA_IDS_ID_COLUMN)):
            return None
        ret: Dict[str, str] = {}
        for row in conn.execute(sa.select(id_table)).fetchall():
            ret[row[DATA_IDS_REG_COLUMN]] = row[DATA_IDS_ID_COLUMN]
        return ret
    finally:
        conn.close()


def update_uls_file(uls_dir: str, uls_file: str, symlink: str,
                    executor: LoggingExecutor) -> None:
    """ Atomically retargeting symlink to new ULS file

    Arguments:
    uls_dir  -- Directory containing ULS files and symlink
    uls_file -- Base name of new ULS file (already in ULS directory)
    symlink  -- Base name of symlink pointing to current ULS file
    executor -- LoggingExecutor object
    """
    # Getting random name for temporary symlink
    fd, temp_symlink_name = tempfile.mkstemp(dir=uls_dir, suffix="_" + symlink)
    os.close(fd)
    os.unlink(temp_symlink_name)
    # Name race condition is astronomically improbable, as name is random and
    # downloader is one (except for development environment)

    assert uls_file == os.path.basename(uls_file)
    assert os.path.isfile(os.path.join(uls_dir, uls_file))
    os.symlink(uls_file, temp_symlink_name)

    executor.execute(
        ["mv", "-fT", temp_symlink_name, os.path.join(uls_dir, symlink)],
        fail_on_error=True)
    logging.info(f"FS database symlink '{os.path.join(uls_dir, symlink)}' "
                 f"now points to '{uls_file}'")


class DbDiff:
    """ Computes and holds difference between two FS (aka ULS) databases

    Public attributes:
    valid        -- True if difference is valid
    prev_len     -- Number of paths in previous database
    new_len      -- Number of paths in new database
    diff_len     -- Number of different paths
    ras_diff_len -- Number of different RAS entries
    diff_tiles   -- Tiles containing receivers of different paths
    """
    def __init__(self, prev_filename: str, new_filename: str,
                 executor: LoggingExecutor) -> None:
        """ Constructor

        Arguments:
        prev_filename -- Previous file name
        new_filename  -- New filename
        executor      -- LoggingExecutor object
        """
        self.valid = False
        self.prev_len = 0
        self.new_len = 0
        self.diff_len = 0
        self.diff_tiles: List[LatLonRect] = []
        logging.info("Getting differences with previous database")
        output = \
            executor.execute(
                [FS_DB_DIFF, "--report_tiles", prev_filename, new_filename],
                timeout_sec=10 * 60, return_output=True, fail_on_error=False)
        if output is None:
            logging.error("Database comparison failed")
            return
        m = re.search(r"Paths in DB1:\s+(?P<db1>\d+)(.|\n)+"
                      r"Paths in DB2:\s+(?P<db2>\d+)(.|\n)+"
                      r"Different paths:\s+(?P<diff>\d+)(.|\n)+"
                      r"Different RAS entries:\s+(?P<ras_diff>\d+)(.|\n)+",
                      cast(str, output))
        if m is None:
            logging.error(
                f"Output of '{FS_DB_DIFF}' has unrecognized structure")
            return
        self.prev_len = int(cast(str, m.group("db2")))
        self.new_len = int(cast(str, m.group("db2")))
        self.diff_len = int(cast(str, m.group("diff")))
        self.ras_diff_len = int(cast(str, m.group("ras_diff")))
        for m in re.finditer(
                r"Difference in tile "
                r"\[(?P<min_lat>[0-9.]+)-(?P<max_lat>[0-9.]+)\]"
                r"(?P<lat_sign>[NS]), "
                r"\[(?P<min_lon>[0-9.]+)-(?P<max_lon>[0-9.]+)\]"
                r"(?P<lon_sign>[EW])", cast(str, output)):
            lat_sign = 1 if m.group("lat_sign") == "N" else -1
            lon_sign = 1 if m.group("lon_sign") == "E" else -1
            self.diff_tiles.append(
                LatLonRect(
                    min_lat=float(m.group("min_lat")) * lat_sign,
                    max_lat=float(m.group("max_lat")) * lat_sign,
                    min_lon=float(m.group("min_lon")) * lon_sign,
                    max_lon=float(m.group("max_lon")) * lon_sign))
        logging.info(
            f"Database comparison succeeded: "
            f"{os.path.basename(prev_filename)} has {self.prev_len} paths, "
            f"{os.path.basename(new_filename)} has {self.new_len} paths, "
            f"difference is in {self.diff_len} paths, "
            f"{self.ras_diff_len} RAS entries, "
            f"{len(self.diff_tiles)} tiles")
        self.valid = True


class UlsFileChecker:
    """ Checker of ULS database validity

    Private attributes:
    _max_change_percent -- Optional percent of maximum difference
    _afc_url            -- Optional AFC Service URL to test ULS database on
    _afc_parallel       -- Optional number of parallel AFC requests to make
                           during database verification
    _regions            -- List of regions to test database on. Empty if on all
                           regions
    _executor           -- LoggingExecutor object
    _status_updater      -- StatusUpdater object
    """
    def __init__(self, executor: LoggingExecutor,
                 status_updater: StatusUpdater,
                 max_change_percent: Optional[float] = None,
                 afc_url: Optional[str] = None,
                 afc_parallel: Optional[int] = None,
                 regions: Optional[List[str]] = None) -> None:
        """ Constructor

        Arguments:
        executor           -- LoggingExecutor object
        status_updater     -- StatusUpdater object
        max_change_percent -- Optional percent of maximum difference
        afc_url            -- Optional AFC Service URL to test ULS database on
        afc_parallel       -- Optional number of parallel AFC requests to make
                              during database verification
        regions            -- Optional list of regions to test database on. If
                              empty or None - test on all regions
        """
        self._executor = executor
        self._status_updater = status_updater
        self._max_change_percent = max_change_percent
        self._afc_url = afc_url
        self._afc_parallel = afc_parallel
        self._regions: List[str] = regions or []

    def valid(self, base_dir: str, new_filename: str,
              db_diff: Optional[DbDiff]) -> bool:
        """ Checks validity of given database

        Arguments:
        base_dir     -- Directory, containing database being checked. This
                        argument is currently unused
        new_filename -- Database being checked. Should have exactly same path
                        as required in AFC Config
        db_diff      -- None or difference from previous database
        Returns True if check passed
        """
        check_results: Dict[str, Optional[str]] = {}
        for item, (tested, errmsg) in \
                [("difference from previous",  self._check_diff(db_diff)),
                 ("usable by AFC Service", self._check_afc(new_filename))]:
            if not tested:
                continue
            if errmsg is not None:
                logging.error(errmsg)
            check_results[item] = errmsg
        self._status_updater.status_check(CheckType.FsDatabase, check_results)
        return all(errmsg is None for errmsg in check_results.values())

    def _check_diff(self, db_diff: Optional[DbDiff]) \
            -> Tuple[bool, Optional[str]]:
        """ Checks amount of difference since previous database.
        Returns (check_performed, error_message) tuple """
        if db_diff is None:
            return (False, None)
        if not db_diff.valid:
            return (True, "Database difference can't be obtained")
        if ((db_diff.diff_len == 0) and (db_diff.ras_diff_len == 0)) != \
                (len(db_diff.diff_tiles) == 0):
            return \
                (True,
                 f"Inconsistent indication of database difference: difference "
                 f"is in {db_diff.diff_len} paths and in "
                 f"{db_diff.ras_diff_len} RAS entries, but in "
                 f"{len(db_diff.diff_tiles)} tiles")
        if self._max_change_percent is None:
            return (False, None)
        diff_percent = \
            round(
                100 if db_diff.new_len == 0 else
                ((db_diff.diff_len * 100) / db_diff.new_len),
                3)
        if diff_percent > self._max_change_percent:
            return \
                (True,
                 f"Database changed by {diff_percent}%, which exceeds the "
                 f"limit of {self._max_change_percent}%")
        return (True, None)

    def _check_afc(self, new_filename: str) -> Tuple[bool, Optional[str]]:
        """ Checks new database against AFC Service

        Arguments:
        new_filename -- Database being checked. Should have exactly same path
                        as required in AFC Config
        Returns (check_performed, error_message) tuple
        """
        if self._afc_url is None:
            return (False, None)
        logging.info("Testing new FS database on AFC Service")
        args = [FS_AFC, "--server_url", self._afc_url] + \
            (["--parallel", str(self._afc_parallel)]
             if self._afc_parallel is not None else []) + \
            [f"--region={r}" for r in self._regions] + [new_filename]
        logging.debug(" ".join(shlex.quote(arg) for arg in args))
        if not self._executor.execute(args, fail_on_error=False,
                                      timeout_sec=30 * 60):
            return (True, "AFC Service test failed")
        return (True, None)


class ExtParamFilesChecker:
    """ Checks that external parameter file match those in image

    Private attributes:
    _epf_list       -- List of external parameter file sets descriptors
                       (_ExtParamFiles objects) or None
    _script_dir     -- Downloader script directory or None
    _status_updater -- StatusUpdater object
    """

    # Descriptor of external parameter files
    _ExtParamFiles = \
        NamedTuple("_ExtParamFiles",
                   [
                    # Location in the internet
                    ("base_url", str),
                    # Downloader script subdirectory
                    ("subdir", str), ("files", List[str])])

    def __init__(self, status_updater: StatusUpdater,
                 ext_files_arg: Optional[List[str]] = None,
                 script_dir: Optional[str] = None) -> None:
        """ Constructor

        Arguments:
        status_updater -- StatusUpdater object
        ext_files_arg  -- List of 'BASE_URL:SUBDIR:FILES,FILE...' groups,
                          separated with semicolon: external parameter file
                          descriptors from command line
        script_dir     -- Downloader script directory
        """
        self._epf_list: \
            Optional[List["ExtParamFilesChecker._ExtParamFiles"]] = None
        if ext_files_arg is not None:
            self._epf_list = []
            for efg in ext_files_arg:
                for ef in efg.split(";"):
                    m = re.match(r"^(.*):(.+):(.+)$", ef)
                    error_if(m is None,
                             f"Invalid format of --check_ext_files parameter: "
                             f"'{ef}'")
                    assert m is not None
                    self._epf_list.append(
                        self._ExtParamFiles(base_url=m.group(1).rstrip("/"),
                                            subdir=m.group(2),
                                            files=m.group(3).split(",")))
        self._script_dir = script_dir
        self._status_updater = status_updater

    def check(self) -> None:
        """ Check that external files matches those in image """
        assert (self._epf_list is not None) and (self._script_dir is not None)
        temp_dir: Optional[str] = None
        check_results: Dict[str, Optional[str]] = {}
        try:
            temp_dir = tempfile.mkdtemp()
            for epf in self._epf_list:
                for filename in epf.files:
                    errmsg: Optional[str] = None
                    try:
                        internal_file_name = \
                            os.path.join(self._script_dir, epf.subdir,
                                         filename)
                        if not os.path.isfile(internal_file_name):
                            errmsg = "Absent in container"
                            continue
                        try:
                            url = f"{epf.base_url}/{filename}"
                            external_file_name = os.path.join(temp_dir,
                                                              filename)
                            urllib.request.urlretrieve(url, external_file_name)
                        except urllib.error.URLError as ex:
                            logging.warning(f"Error downloading '{url}': {ex}")
                            errmsg = "Download failed"
                            continue
                        contents: List[bytes] = []
                        try:
                            for fn in (internal_file_name, external_file_name):
                                with open(fn, "rb") as f:
                                    contents.append(f.read())
                        except OSError as ex:
                            logging.warning(f"Read failed: {ex}")
                            errmsg = "Read failed"
                            continue
                        if contents[0] != contents[1]:
                            errmsg = "External content changed"
                    finally:
                        check_results[os.path.join(epf.subdir, filename)] = \
                            errmsg
        finally:
            self._status_updater.status_check(check_type=CheckType.ExtParams,
                                              results=check_results)
            if temp_dir:
                shutil.rmtree(temp_dir, ignore_errors=True)


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = argparse.ArgumentParser(
        description="ULS data download control service")
    argument_parser.add_argument(
        "--download_script", metavar="ULS_PARSER_SCRIPT",
        help=f"ULS download script{env_help(Settings, 'download_script')}")
    argument_parser.add_argument(
        "--download_script_args", metavar="ARGS",
        help=f"Optional additional arguments to pass to ULS download script"
        f"{env_help(Settings, 'download_script_args')}")
    argument_parser.add_argument(
        "--region", metavar="REG1[:REG2[:REG3...]]",
        help=f"Colon-separated list of regions to download. Default is all "
        f"regions{env_help(Settings, 'region')}")
    argument_parser.add_argument(
        "--result_dir", metavar="RESULT_DIR",
        help=f"Directory where ULS download script puts resulting database"
        f"{env_help(Settings, 'result_dir')}")
    argument_parser.add_argument(
        "--temp_dir", metavar="TEMP_DIR",
        help=f"Directory containing downloader's temporary files (cleared "
        f"before downloading){env_help(Settings, 'temp_dir')}")
    argument_parser.add_argument(
        "--ext_db_dir", metavar="EXTERNAL_DATABASE_DIR",
        help=f"Directory where new ULS databases should be copied. If "
        f"--ext_db_symlink contains path, this parameter is root directory "
        f"for this path{env_help(Settings, 'ext_db_dir')}")
    argument_parser.add_argument(
        "--ext_db_symlink", metavar="CURRENT_DATABASE_SYMLINK",
        help=f"Symlink in database directory (specified with --ext_db_dir) "
        f"that points to current database. May contain path - if so, this "
        f"path is used for AFC Config override during database validity check"
        f"{env_help(Settings, 'ext_db_symlink')}")
    argument_parser.add_argument(
        "--ext_ras_database", metavar="FILENAME",
        help=f"Externally maintained RAS 'database' file"
        f"{env_help(Settings, 'ext_ras_database')}")
    argument_parser.add_argument(
        "--ras_database", metavar="FILENAME",
        help=f"Where from downloader scripts reads RAS 'database'"
        f"{env_help(Settings, 'ras_database')}")
    argument_parser.add_argument(
        "--fsid_file", metavar="FSID_FILE",
        help=f"FSID file where ULS downloader is expected to read/update it"
        f"{env_help(Settings, 'fsid_file')}")
    argument_parser.add_argument(
        "--service_state_db_dsn", metavar="STATE_DB_DSN",
        help=f"Connection string to database containing FS service state "
        f"(that is used by healthcheck script)"
        f"{env_help(Settings, 'service_state_db_dsn')}")
    argument_parser.add_argument(
        "--service_state_db_create_if_absent", action="store_true",
        help=f"Create state database if absent"
        f"{env_help(Settings, 'service_state_db_create_if_absent')}")
    argument_parser.add_argument(
        "--service_state_db_recreate", action="store_true",
        help=f"Recreate state DB if it exists"
        f"{env_help(Settings, 'service_state_db_recreate')}")
    argument_parser.add_argument(
        "--prometheus_port", metavar="PORT_NUMBER",
        help=f"Port to serve Prometheus metrics on. Default is to not serve "
        f"Prometheus metrics. Ignored (as irrelevant) if specified with "
        f"'--run_once'{env_help(Settings, 'prometheus_port')}")
    argument_parser.add_argument(
        "--statsd_server", metavar="HOST[:PORT]",
        help=f"Send metrics to given StatsD host. Default is not to"
        f"{env_help(Settings, 'prometheus_port')}")
    argument_parser.add_argument(
        "--check_ext_files", metavar="BASE_URL:SUBDIR:FILENAME[,...][;...]",
        action="append", default=[],
        help=f"Verify that given files at given location match files at given "
        f"subdirectory of ULS downloader script, several such "
        f"semicolon-separated groups may be specified (e.g. in environment "
        f"variable){env_help(Settings, 'check_ext_files')}")
    argument_parser.add_argument(
        "--max_change_percent", metavar="MAX_CHANGE_PERCENT",
        help=f"Maximum allowed change since previous database in percents"
        f"{env_help(Settings, 'max_change_percent')}")
    argument_parser.add_argument(
        "--afc_url", metavar="URL",
        help=f"URL for making trial AFC Requests with new database"
        f"{env_help(Settings, 'afc_url')}")
    argument_parser.add_argument(
        "--afc_parallel", metavar="NUMBER",
        help=f"Number of parallel AFC Request to make during verifying new "
        f"database against AFC Engine{env_help(Settings, 'afc_parallel')}")
    argument_parser.add_argument(
        "--rcache_url", metavar="URL",
        help=f"URL for spatial invalidation{env_help(Settings, 'rcache_url')}")
    argument_parser.add_argument(
        "--rcache_enabled", metavar="TRUE/FALSE",
        help=f"FALSE to disable spatial invalidation (even if URL specified)"
        f"{env_help(Settings, 'rcache_enabled')}")
    argument_parser.add_argument(
        "--delay_hr", metavar="DELAY_HR",
        help=f"Delay before invocation in hours"
        f"{env_help(Settings, 'delay_hr')}")
    argument_parser.add_argument(
        "--interval_hr", metavar="INTERVAL_HR",
        help=f"Download interval in hours{env_help(Settings, 'interval_hr')}")
    argument_parser.add_argument(
        "--timeout_hr", metavar="TIMEOUT_HR",
        help=f"Download script execution timeout in hours"
        f"{env_help(Settings, 'timeout_hr')}")
    argument_parser.add_argument(
        "--nice", action="store_true",
        help=f"Run download script on nice (low) priority"
        f"{env_help(Settings, 'nice')}")
    argument_parser.add_argument(
        "--verbose", action="store_true",
        help=f"Print detailed log information{env_help(Settings, 'verbose')}")
    argument_parser.add_argument(
        "--run_once", action="store_true",
        help=f"Run download once and exit{env_help(Settings, 'run_once')}")
    argument_parser.add_argument(
        "--force", action="store_true",
        help=f"Force database update even if it is not noticeably changed or "
        f"not passed validity check{env_help(Settings, 'force')}")

    settings: Settings = \
        cast(Settings, merge_args(settings_class=Settings,
                                  args=argument_parser.parse_args(argv)))

    try:
        setup_logging(verbose=settings.verbose)

        if settings.run_once and (settings.prometheus_port is not None):
            logging.warning(
                "There is no point in using Prometheus in run-once mode. Use "
                "--statsd_server if metrics are necessary")
            settings.prometheus_port = None

        print_args(settings)

        state_db = StateDb(db_dsn=settings.service_state_db_dsn)
        state_db.check_server()
        if settings.service_state_db_create_if_absent:
            state_db.create_db(
                recreate_tables=settings.service_state_db_recreate)

        status_updater = \
            StatusUpdater(state_db=state_db,
                          prometheus_port=settings.prometheus_port,
                          statsd_server=settings.statsd_server)

        if not state_db.read_milestone(DownloaderMilestone.ServiceBirth):
            status_updater.milestone(DownloaderMilestone.ServiceBirth)

        error_if(not os.path.isfile(settings.download_script),
                 f"Download script '{settings.download_script}' not found")
        full_ext_db_dir = \
            os.path.join(settings.ext_db_dir,
                         os.path.dirname(settings.ext_db_symlink))
        error_if(
            not os.path.isdir(full_ext_db_dir),
            f"External database directory '{full_ext_db_dir}' not found")

        executor = LoggingExecutor()

        ext_params_file_checker = \
            ExtParamFilesChecker(
                ext_files_arg=settings.check_ext_files,
                script_dir=os.path.dirname(settings.download_script),
                status_updater=status_updater)

        current_uls_file = os.path.join(settings.ext_db_dir,
                                        settings.ext_db_symlink)

        uls_file_checker = \
            UlsFileChecker(
                max_change_percent=settings.max_change_percent,
                afc_url=settings.afc_url, afc_parallel=settings.afc_parallel,
                regions=None if settings.region is None
                else settings.region.split(":"),
                executor=executor, status_updater=status_updater)

        rcache_settings = \
            RcacheClientSettings(
                enabled=settings.rcache_enabled and bool(settings.rcache_url),
                service_url=settings.rcache_url)
        rcache_settings.validate_for(rcache=True)
        rcache: Optional[RcacheClient] = \
            RcacheClient(rcache_settings) if rcache_settings.enabled else None

        status_updater.milestone(DownloaderMilestone.ServiceStart)

        if settings.delay_hr and (not settings.run_once):
            logging.info(f"Delaying by {settings.delay_hr} hours")
            time.sleep(settings.delay_hr * 3600)

        if settings.run_once:
            logging.info("Running healthcheck script")
            executor.execute(
                [sys.executable, HEALTHCHECK_SCRIPT, "--force_success"],
                timeout_sec=200, fail_on_error=True)

        # Temporary name of new ULS database in external directory
        temp_uls_file_name: Optional[str] = None

        while True:
            err_msg: Optional[str] = None
            completed = False
            logging.info("Starting ULS download")
            download_start_time = datetime.datetime.now()
            status_updater.milestone(DownloaderMilestone.DownloadStart)
            try:
                has_previous = os.path.islink(current_uls_file)
                if has_previous:
                    logging.info(
                        f"Current database: '{os.readlink(current_uls_file)}'")

                extract_fsid_table(uls_file=current_uls_file,
                                   fsid_file=settings.fsid_file,
                                   executor=executor)

                # Clear some directories from stuff left from previous
                # downloads
                for dir_to_clean in [settings.result_dir, settings.temp_dir]:
                    if dir_to_clean and os.path.isdir(dir_to_clean):
                        executor.execute(f"rm -rf {dir_to_clean}/*",
                                         timeout_sec=100, fail_on_error=True)

                logging.info("Copying RAS database")
                shutil.copyfile(settings.ext_ras_database,
                                settings.ras_database)

                logging.info("Checking if external parameter files changed")
                ext_params_file_checker.check()

                # Issue download script
                cmdline_args: List[str] = []
                if settings.nice and (os.name == "posix"):
                    cmdline_args.append("nice")
                cmdline_args.append(settings.download_script)
                if settings.region:
                    cmdline_args += ["--region", settings.region]
                if settings.download_script_args:
                    cmdline_args.append(settings.download_script_args)
                logging.info(f"Starting {' '.join(cmdline_args)}")
                executor.execute(
                    " ".join(cmdline_args) if settings.download_script_args
                    else cmdline_args,
                    cwd=os.path.dirname(settings.download_script),
                    timeout_sec=settings.timeout_hr * 3600,
                    fail_on_error=True)

                # Find out name of new ULS file
                uls_files = glob.glob(os.path.join(settings.result_dir,
                                                   ULS_FILEMASK))
                if len(uls_files) < 1:
                    raise ProcessingException(
                        "ULS file not generated by ULS downloader")
                if len(uls_files) > 1:
                    raise ProcessingException(
                        f"More than one {ULS_FILEMASK} file generated by ULS "
                        f"downloader. What gives?")

                # Check what regions were updated
                new_uls_file = uls_files[0]
                logging.info(f"ULS file '{new_uls_file}' created. It will "
                             f"undergo some inspection")
                new_uls_identity = get_uls_identity(new_uls_file)
                if new_uls_identity is None:
                    raise ProcessingException(
                        "Generated ULS file does not contain identity "
                        "information")
                status_updater.milestone(DownloaderMilestone.DownloadSuccess)

                updated_regions = set(new_uls_identity.keys())
                if has_previous:
                    for current_region, current_identity in \
                            (get_uls_identity(current_uls_file) or {}).items():
                        if current_identity and \
                                (new_uls_identity.get(current_region) ==
                                 current_identity):
                            updated_regions.remove(current_region)
                if updated_regions:
                    logging.info(f"Updated regions: "
                                 f"{', '.join(sorted(updated_regions))}")

                # If anything was updated - do the update routine
                if updated_regions or settings.force:
                    # Embed updated FSID table to the new database
                    logging.info("Embedding FSID table")
                    executor.execute(
                        [FSID_TOOL, "embed", new_uls_file, settings.fsid_file],
                        fail_on_error=True)

                    temp_uls_file_name = \
                        os.path.join(full_ext_db_dir,
                                     "temp_" + os.path.basename(new_uls_file))
                    # Copy new ULS file to external directory
                    logging.debug(
                        f"Copying '{new_uls_file}' to '{temp_uls_file_name}'")
                    shutil.copy2(new_uls_file, temp_uls_file_name)

                    db_diff = DbDiff(prev_filename=current_uls_file,
                                     new_filename=temp_uls_file_name,
                                     executor=executor) \
                        if has_previous else None
                    if settings.force or \
                            uls_file_checker.valid(
                                base_dir=settings.ext_db_dir,
                                new_filename=os.path.join(
                                    os.path.dirname(settings.ext_db_symlink),
                                    os.path.basename(temp_uls_file_name)),
                                db_diff=db_diff):
                        if settings.force:
                            status_updater.status_check(CheckType.FsDatabase,
                                                        None)
                        # Renaming database
                        permanent_uls_file_name = \
                            os.path.join(full_ext_db_dir,
                                         os.path.basename(new_uls_file))
                        os.rename(temp_uls_file_name, permanent_uls_file_name)
                        # Retargeting symlink
                        update_uls_file(
                            uls_dir=full_ext_db_dir,
                            uls_file=os.path.basename(new_uls_file),
                            symlink=os.path.basename(settings.ext_db_symlink),
                            executor=executor)
                        if rcache and (db_diff is not None) and \
                                db_diff.diff_tiles:
                            tile_list = \
                                "<" + \
                                ">, <".join(tile.short_str() for tile in
                                            db_diff.diff_tiles[: 1000]) + \
                                ">"
                            logging.info(f"Requesting invalidation of the "
                                         f"following tiles: {tile_list}")
                            rcache.rcache_spatial_invalidate(
                                tiles=db_diff.diff_tiles)

                        # Update data change times (for health checker)
                        status_updater.milestone(
                            milestone=DownloaderMilestone.RegionChanged,
                            updated_regions=updated_regions,
                            all_regions=list(new_uls_identity.keys()))

                        status_updater.milestone(DownloaderMilestone.DbUpdated)
                        completed = True
                else:
                    logging.info("FS data is identical to previous. No update "
                                 "will be done")
            except (OSError, subprocess.SubprocessError, ProcessingException) \
                    as ex:
                err_msg = str(ex)
                logging.error(f"Download failed: {ex}")
            finally:
                exec_output = executor.get_output()
                if err_msg:
                    exec_output = f"{exec_output.rstrip()}\n{err_msg}\n"
                state_db.write_log(log_type=LogType.Last, log=exec_output)
                if completed:
                    state_db.write_log(log_type=LogType.LastCompleted,
                                       log=exec_output)
                if err_msg:
                    state_db.write_log(log_type=LogType.LastFailed,
                                       log=exec_output)
                try:
                    if temp_uls_file_name and \
                            os.path.isfile(temp_uls_file_name):
                        logging.debug(f"Removing '{temp_uls_file_name}'")
                        os.unlink(temp_uls_file_name)
                except OSError as ex:
                    logging.error(f"Attempt to remove temporary ULS database "
                                  f"'{temp_uls_file_name}' failed: {ex}")

            # Prepare to sleep
            download_duration_sec = \
                (datetime.datetime.now() - download_start_time).total_seconds()
            logging.info(
                f"Download took {download_duration_sec // 60} minutes")
            if settings.run_once:
                break
            remaining_seconds = \
                max(0, settings.interval_hr * 3600 - download_duration_sec)
            if remaining_seconds:
                next_attempt_at = \
                    (datetime.datetime.now() +
                     datetime.timedelta(seconds=remaining_seconds)).isoformat()
                logging.info(f"Next download at {next_attempt_at}")
                time.sleep(remaining_seconds)
    except KeyboardInterrupt:
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1:])
