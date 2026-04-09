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
# pylint: disable=too-many-instance-attributes, too-many-positional-arguments

import argparse
import datetime
import glob
import hashlib
import hmac
import json
import logging
import os
import prometheus_client
import pydantic
from pydantic_settings import BaseSettings
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
from typing import Any, cast, Dict, Iterable, List, NamedTuple, Optional, \
    Tuple, Union
import urllib.error
import urllib.request
import wsgiref.simple_server

import als
from db_utils import safe_dsn
from rcache_client import RcacheClient
from rcache_models import Beam, LatLonRect, RcacheClientSettings
from pydantic_utils import env_help, merge_args
from uls_service_common import *
from uls_service_state_db import CheckType, DownloaderMilestone, LogType, \
    StateDb

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


class Settings(BaseSettings):
    """ Arguments from command lines - with their default values """
    download_script: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/daily_uls_parse/daily_uls_parse.py",
            validation_alias="ULS_DOWNLOAD_SCRIPT",
            description="FS download script")
    download_script_args: Optional[str] = \
        pydantic.Field(None, validation_alias="ULS_DOWNLOAD_SCRIPT_ARGS",
                       description="Additional download script parameters")
    region: Optional[str] = \
        pydantic.Field(None, validation_alias="ULS_DOWNLOAD_REGION",
                       description="Download regions", json_schema_extra={"no": "All"})
    result_dir: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/ULS_Database/", validation_alias="ULS_RESULT_DIR",
            description="Directory where download script puts downloaded file")
    temp_dir: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/daily_uls_parse/temp/", validation_alias="ULS_TEMP_DIR",
            description="Temporary directory of ULS download script, cleaned "
            "before downloading")
    save_dir: str = \
        pydantic.Field(
            "/rat_transfer/daily_uls_parse/country_history/", env="ULS_SAVE_DIR",
            description="Directory where download script puts downloaded file for country")
    num_saves: Optional[int] = \
        pydantic.Field(3, env="NUM_SAVE",
                       description="Number of backup saved for data")
    ext_db_dir: str = \
        pydantic.Field(
            ..., validation_alias="ULS_EXT_DB_DIR",
            description="Ultimate downloaded file destination directory")
    ext_db_symlink: str = \
        pydantic.Field(..., validation_alias="ULS_CURRENT_DB_SYMLINK",
                       description="Symlink pointing to current ULS file")
    fsid_file: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/daily_uls_parse/data_files/fsid_table.csv",
            validation_alias="ULS_FSID_FILE",
            description="FSID file location expected by ULS download script")
    ext_ras_database: str = \
        pydantic.Field(..., validation_alias="ULS_EXT_RAS_DATABASE",
                       description="RAS database")
    ras_database: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/daily_uls_parse/data_files/RASdatabase.dat",
            validation_alias="ULS_RAS_DATABASE",
            description="Where from ULS script reads RAS database")
    service_state_db_dsn: str = \
        pydantic.Field(
            ..., validation_alias="ULS_SERVICE_STATE_DB_DSN",
            description="Connection string to service state database",
            json_schema_extra={"convert": safe_dsn})
    service_state_db_password_file: Optional[str] = \
        pydantic.Field(
            None, validation_alias="ULS_SERVICE_STATE_DB_PASSWORD_FILE",
            description="Optional name of file with password for state "
            "database DSN")
    db_creator_url: Optional[str] = \
        pydantic.Field(
            None, validation_alias="AFC_DB_CREATOR_URL",
            description="Postgres database creator REST API URL")
    alembic_config: Optional[str] = \
        pydantic.Field(
            None, validation_alias="ULS_ALEMBIC_CONFIG",
            description="Optional name of Alembic config file")
    alembic_initial_version: Optional[str] = \
        pydantic.Field(
            None, validation_alias="ULS_ALEMBIC_INITIAL_VERSION",
            description="Version to stamp Alembic database with")
    alembic_head_version: Optional[str] = \
        pydantic.Field(
            None, validation_alias="ULS_ALEMBIC_HEAD_VERSION",
            description="Version to stamp newly-created database with "
            "(default is 'head')")
    prometheus_port: Optional[int] = \
        pydantic.Field(None, validation_alias="ULS_PROMETHEUS_PORT",
                       description="Port to serve Prometheus metrics on")
    statsd_server: Optional[str] = \
        pydantic.Field(None, validation_alias="ULS_STATSD_SERVER",
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
            validation_alias="ULS_CHECK_EXT_FILES",
            description="Verify that that files are the same as in internet",
            json_schema_extra={"no": "None"})
    max_change_percent: Optional[float] = \
        pydantic.Field(
            10., validation_alias="ULS_MAX_CHANGE_PERCENT",
            description="Limit on number of paths changed",
            json_schema_extra={"convert": lambda v: f"{v}%" if v else "Don't check"})
    hash_manifest_url: Optional[str] = \
        pydantic.Field(
            None, validation_alias="ULS_HASH_MANIFEST_URL",
            description="HTTPS URL of JSON manifest mapping DB filenames to "
            "expected SHA-256 hashes",
            json_schema_extra={"no": "Don't check"})
    hash_manifest_file: Optional[str] = \
        pydantic.Field(
            None, validation_alias="ULS_HASH_MANIFEST_FILE",
            description="Local path to JSON manifest mapping DB filenames to "
            "expected SHA-256 hashes",
            json_schema_extra={"no": "Don't check"})
    hash_manifest_hmac_key: Optional[str] = \
        pydantic.Field(
            None, validation_alias="ULS_HASH_MANIFEST_HMAC_KEY",
            description="HMAC-SHA256 key for authenticating the hash manifest. "
            "When set, a companion '.hmac' file (same URL "
            "with '.hmac' suffix) is fetched and verified before parsing. "
            "Reject manifest when HMAC is configured but absent or invalid.",
            json_schema_extra={
                "no": "No manifest authentication (insecure)",
                "convert": lambda v: "<redacted>" if v else None})
    afc_url: Optional[str] = \
        pydantic.Field(
            None, validation_alias="ULS_AFC_URL",
            description="AFC Service URL to use for database validity check",
            json_schema_extra={"no": "Don't check"})
    afc_parallel: Optional[int] = \
        pydantic.Field(
            None, validation_alias="ULS_AFC_PARALLEL",
            description="Number of parallel AFC Requests to use when doing "
            "validity check", json_schema_extra={"no": "fs_afc.py's default"})
    rcache_url: Optional[str] = \
        pydantic.Field(None, validation_alias="RCACHE_SERVICE_URL",
                       description="Rcache service url",
                       json_schema_extra={"no": "Don't do spatial invalidation"})
    rcache_enabled: bool = \
        pydantic.Field(True, validation_alias="RCACHE_ENABLED",
                       description="Rcache spatial invalidation",
                       json_schema_extra={"yes": "Enabled", "no": "Disabled"})
    rcache_directional_invalidate: bool = \
        pydantic.Field(
            True, validation_alias="RCACHE_DIRECTIONAL_INVALIDATE",
            description="True to use directional invalidation (default), "
            "False to use tiled invalidation)")
    delay_hr: float = \
        pydantic.Field(0., validation_alias="ULS_DELAY_HR",
                       description="Hours to delay first download by")
    interval_hr: float = \
        pydantic.Field(4, validation_alias="ULS_INTERVAL_HR",
                       description="Download interval in hours")
    timeout_hr: float = \
        pydantic.Field(1, validation_alias="ULS_TIMEOUT_HR",
                       description="Download maximum duration in hours")
    nice: bool = \
        pydantic.Field(False, validation_alias="ULS_NICE",
                       description="Run in lowered (nice) priority")
    verbose: bool = \
        pydantic.Field(False, description="Print debug info")
    run_once: bool = \
        pydantic.Field(False, validation_alias="ULS_RUN_ONCE",
                       description="Run", json_schema_extra={"yes": "Once", "no": "Indefinitely"})
    force: bool = \
        pydantic.Field(False, validation_alias="ULS_FORCE_UPDATE",
                       description="Force FS database update (even if not "
                       "changed or found invalid). "
                       "Local-dev override only — MUST NOT be set in "
                       "production deployments.")

    @pydantic.field_validator("check_ext_files", mode="before")
    @classmethod
    def check_ext_files_str_to_list(cls, v: Any) -> Any:
        """ Converts string value of 'check_ext_files' from environment from
        string to list (as it is list in argparse) """
        return [v] if v and isinstance(v, str) else v

    @pydantic.field_validator("statsd_server", mode="after")
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

    @pydantic.model_validator(mode="before")
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
    for name, field_info in Settings.model_fields.items():
        value = getattr(settings, name)
        extra = field_info.json_schema_extra or {}
        if not isinstance(extra, dict):
            extra = {}
        value_repr: str
        if "convert" in extra:
            value_repr = extra["convert"](value)
        elif value and ("yes" in extra):
            value_repr = extra["yes"]
        elif (not value) and ("no" in extra):
            value_repr = extra["no"]
        elif field_info.annotation is bool:
            value_repr = "Yes" if value else "No"
        else:
            value_repr = str(value)
        logging.info(f"{field_info.description}: {value_repr}")


class LoggingExecutor:
    """ Program executor that collects output

    Private attributes:
    _lines -- Output lines
    """

    def __init__(self) -> None:
        self._lines: List[str] = []

    def execute(self, cmd: List[str], fail_on_error: bool,
                return_output: bool = False, cwd: Optional[str] = None,
                timeout_sec: Optional[float] = None) -> \
            Union[bool, Optional[str]]:
        """ Execute command

        Arguments:
        cmd           -- Command as list of strings
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
            nonlocal timed_out
            try:
                os.killpg(pgid, signal.SIGTERM)
                timed_out = True
            except OSError:
                pass

        self._lines.append(
            "> " + ' '.join(shlex.quote(arg) for arg in cmd) + "\n")
        try:
            # start_new_session=True puts the child in its own process group so
            # the timeout watchdog's os.killpg() targets only the runaway child
            # and not the uls_service daemon's own group.
            with subprocess.Popen(cmd, shell=False, text=True,
                                  encoding="utf-8", stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, bufsize=0,
                                  cwd=cwd, start_new_session=True) as p:
                timer: Optional[threading.Timer] = \
                    threading.Timer(timeout_sec, killer_timer,
                                    kwargs={"pgid": os.getpgid(p.pid)}) \
                    if timeout_sec is not None else None
                if timer is not None:
                    timer.start()
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


def _start_token_gated_prometheus(port: int) -> None:
    """Start Prometheus metrics on *port* behind an X-AFC-Internal-Token gate
    (parity with als_siphon / afc_server / rat_server /metrics)."""
    file_path = os.getenv("AFC_INTERNAL_TOKEN_FILE")
    expected: Optional[str]
    if file_path:
        try:
            with open(file_path) as fh:
                expected = fh.read().strip()
        except OSError as exc:
            logging.critical(
                "FATAL: cannot read AFC_INTERNAL_TOKEN_FILE %s: %s",
                file_path, exc)
            sys.exit(1)
    else:
        expected = os.getenv("AFC_INTERNAL_TOKEN")

    metrics_app = prometheus_client.make_wsgi_app()

    def gated_app(environ: Any,
                  start_response: Any) -> Iterable[bytes]:
        supplied = environ.get("HTTP_X_AFC_INTERNAL_TOKEN", "")
        if (not expected) or \
                (not hmac.compare_digest(supplied, expected)):
            start_response("403 Forbidden",
                           [("Content-Type", "text/plain")])
            return [b"Forbidden"]
        return metrics_app(environ, start_response)

    httpd = wsgiref.simple_server.make_server("", port, gated_app)
    t = threading.Thread(target=httpd.serve_forever)
    t.daemon = True
    t.start()


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
                # String.format()-compatible pattern, containing placeholder
                # for label value
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
            try:
                _start_token_gated_prometheus(prometheus_port)
            except OSError as ex:
                logging.warning(f"Cant't start Prometheus client: {ex}. "
                                f"Proceeding nevertheless")
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
    fsid_glob = fsid_name_parts[0] + "*" + fsid_name_parts[1]
    for old_fsid in glob.glob(fsid_glob):
        try:
            os.remove(old_fsid)
        except OSError as ex:
            logging.warning(f"Could not remove old FSID file '{old_fsid}': {ex}. "
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
    engine = \
        sa.create_engine(f"sqlite:///{uls_file}?mode=ro",
                         connect_args={"uri": True})
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
        for row in conn.execute(sa.select(id_table)).mappings().fetchall():
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


def save_recent_download(regions, num_of_saves, recent_download_dir: str, saved_download_dir: str) -> None:
    """ Handles the saving and storage of the country downloads

    Arguments:
    regions  -- List of regions to saves
    num_of_saves -- Number of backups to save
    recent_download_dir  -- Location of where the most recent download took place
    saved_download_dir -- Location of where to save the backups
    """
    startTime = datetime.datetime.now()
    nameTime = startTime.isoformat(timespec='seconds').replace(":", '_')
    newSaveName = f"{nameTime}"

    for region in regions:
        regionDataDir = os.path.join(recent_download_dir, region)
        regionSaveParentDir = os.path.join(saved_download_dir, region)
        saveDir = os.path.join(regionSaveParentDir, newSaveName)
        shutil.copytree(regionDataDir, saveDir)

        existingBackups = [
            d for d in os.listdir(regionSaveParentDir)
            if os.path.isdir(os.path.join(regionSaveParentDir, d))
        ]

        existingBackups.sort()

        while len(existingBackups) > num_of_saves:
            oldestBackupName = existingBackups.pop(0)
            oldest_backup_path = os.path.join(regionSaveParentDir, oldestBackupName)

            shutil.rmtree(oldest_backup_path)

    logging.info(f"Successfully saved the recent downloads to backups for: "
                 f"{', '.join(sorted(regions))}")


class DbDiff:
    """ Computes and holds difference between two FS (aka ULS) databases

    Public attributes:
    valid        -- True if difference is valid
    prev_len     -- Number of paths in previous database
    new_len      -- Number of paths in new database
    diff_len     -- Number of different paths
    ras_diff_len -- Number of different RAS entries
    diff_tiles   -- Tiles to invalidate
    diff_beams   -- Beams to invalidate
    """

    def __init__(self, prev_filename: str, new_filename: str,
                 executor: LoggingExecutor,
                 allow_directional_invalidate: bool) -> None:
        """ Constructor

        Arguments:
        prev_filename                -- Previous file name
        new_filename                 -- New filename
        executor                     -- LoggingExecutor object
        allow_directional_invalidate -- True if directional invalidate is
                                        allowed, False if only tiled one is
                                        allowed
        """
        self.valid = False
        self.prev_len = 0
        self.new_len = 0
        self.diff_len = 0
        self.prev_filename = prev_filename
        self.new_filename = new_filename
        self.diff_tiles: List[LatLonRect] = []
        self.diff_beams: List[Beam] = []
        logging.info("Getting differences with previous database")

        invalidation_file: Optional[str] = None
        try:
            fd, invalidation_file = tempfile.mkstemp(suffix=".json",
                                                     prefix="invalidation_")
            os.close(fd)
            output = \
                executor.execute(
                    [FS_DB_DIFF, "--invalidation", invalidation_file,
                     prev_filename, new_filename],
                    timeout_sec=10 * 60, return_output=True,
                    fail_on_error=False)
            if output is None:
                logging.error("Database comparison failed")
                return
            with open(invalidation_file, encoding="utf-8") as f:
                invalidation_dict = json.load(f)
        finally:
            if invalidation_file:
                os.unlink(invalidation_file)
        m = re.search(r"Paths in DB1:\s+(?P<db1>\d+)(.|\n)+"
                      r"Paths in DB2:\s+(?P<db2>\d+)(.|\n)+"
                      r"Different paths:\s+(?P<diff>\d+)(.|\n)+"
                      r"Different RAS entries:\s+(?P<ras_diff>\d+)(.|\n)+",
                      cast(str, output))
        if m is None:
            logging.error(
                f"Output of '{FS_DB_DIFF}' has unrecognized structure")
            return
        self.prev_len = int(cast(str, m.group("db1")))
        self.new_len = int(cast(str, m.group("db2")))
        self.diff_len = int(cast(str, m.group("diff")))
        self.ras_diff_len = int(cast(str, m.group("ras_diff")))
        for beam_dict in invalidation_dict["beams"]:
            if not (allow_directional_invalidate and
                    any(dir_key in beam_dict for dir_key in
                        ("tx_lat_deg", "tx_lon_deg", "tx_azimuth_deg"))):
                self.diff_tiles.append(
                    LatLonRect(
                        min_lat=beam_dict["rx_lat_deg"],
                        max_lat=beam_dict["rx_lat_deg"],
                        min_lon=beam_dict["rx_lon_deg"],
                        max_lon=beam_dict["rx_lon_deg"]))
            else:
                self.diff_beams.append(
                    Beam(
                        rx_lat=beam_dict["rx_lat_deg"],
                        rx_lon=beam_dict["rx_lon_deg"],
                        tx_lat=beam_dict.get("tx_lat_deg"),
                        tx_lon=beam_dict.get("tx_lon_deg"),
                        azimuth_to_tx=beam_dict.get("tx_azimuth_deg")))
        for rect_dict in invalidation_dict["rectangles"]:
            self.diff_tiles.append(
                LatLonRect(
                    min_lat=rect_dict["min_lat_deg"],
                    max_lat=rect_dict["max_lat_deg"],
                    min_lon=rect_dict["min_lon_deg"],
                    max_lon=rect_dict["max_lon_deg"]))
        self.valid = True
        logging.info(str(self))

    def __str__(self) -> str:
        """ String representation (for log and ALS) """
        return \
            f"Database comparison succeeded: " \
            f"{os.path.basename(self.prev_filename)} has {self.prev_len} " \
            f"paths, {os.path.basename(self.new_filename)} has " \
            f"{self.new_len} paths, difference is in {self.diff_len} paths, " \
            f"{self.ras_diff_len} RAS entries, " \
            f"{len(self.diff_beams)} beams" \
            f"{len(self.diff_tiles)} tiles" \
            if self.valid else "Not computed"


class UlsFileChecker:
    """ Checker of ULS database validity

    Private attributes:
    _max_change_percent -- Optional percent of maximum difference
    _hash_manifest_url  -- Optional HTTPS URL of SHA-256 hash manifest JSON
    _hash_manifest_file -- Optional local path to SHA-256 hash manifest JSON
    _hash_manifest_hmac_key -- Optional HMAC-SHA256 key for manifest authentication
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
                 hash_manifest_url: Optional[str] = None,
                 hash_manifest_file: Optional[str] = None,
                 hash_manifest_hmac_key: Optional[str] = None,
                 afc_url: Optional[str] = None,
                 afc_parallel: Optional[int] = None,
                 regions: Optional[List[str]] = None) -> None:
        """ Constructor

        Arguments:
        executor               -- LoggingExecutor object
        status_updater         -- StatusUpdater object
        max_change_percent     -- Optional percent of maximum difference
        hash_manifest_url      -- Optional HTTPS URL to JSON manifest mapping DB
                                  filenames to expected SHA-256 hex digests
        hash_manifest_file     -- Optional local path to JSON manifest mapping DB
                                  filenames to expected SHA-256 hex digests
        hash_manifest_hmac_key -- Optional HMAC-SHA256 key for authenticating the
                                  manifest. When set, a companion
                                  '.hmac' file is required alongside the manifest.
        afc_url                -- Optional AFC Service URL to test ULS database on
        afc_parallel           -- Optional number of parallel AFC requests to make
                                  during database verification
        regions                -- Optional list of regions to test database on. If
                                  empty or None - test on all regions
        """
        self._executor = executor
        self._status_updater = status_updater
        self._max_change_percent = max_change_percent
        self._hash_manifest_url = hash_manifest_url
        self._hash_manifest_file = hash_manifest_file
        self._hash_manifest_hmac_key = hash_manifest_hmac_key
        self._afc_url = afc_url
        self._afc_parallel = afc_parallel
        self._regions: List[str] = regions or []

    def hash_valid(self, filename: str,
                   manifest_key: Optional[str] = None) -> bool:
        """Run only the SHA-256/HMAC manifest gate (no SQLite parsing).

        Use this before constructing DbDiff or calling get_uls_identity so
        that no FCC-derived SQLite content is parsed prior to cryptographic
        verification.  Returns True if the hash check passes; False otherwise.
        Updates the HashVerification status entry.
        """
        hash_results: Dict[str, Optional[str]] = {}
        hash_tested, hash_errmsg = self._check_hash(
            filename, manifest_key or os.path.basename(filename))
        if hash_tested:
            if hash_errmsg is not None:
                logging.error(hash_errmsg)
            hash_results["SHA-256 hash"] = hash_errmsg
        else:
            hash_errmsg = (
                "ULS_HASH_MANIFEST_URL/FILE not configured \u2014 refusing to "
                "activate unverified ULS database (fail-closed)")
            logging.error(hash_errmsg)
            hash_results["SHA-256 hash"] = hash_errmsg
        self._status_updater.status_check(CheckType.HashVerification,
                                          hash_results)
        return hash_errmsg is None

    @property
    def manifest_configured(self) -> bool:
        """ True if a hash manifest source (URL or file) is configured """
        return bool(self._hash_manifest_url or self._hash_manifest_file)

    def valid(self, base_dir: str, new_filename: str,
              db_diff: Optional[DbDiff],
              manifest_key: Optional[str] = None,
              expected_digest: Optional[str] = None,
              hash_only: bool = False,
              hash_filename: Optional[str] = None) -> bool:
        """ Checks validity of given database

        Arguments:
        base_dir     -- Directory, containing database being checked. This
                        argument is currently unused
        new_filename -- Database being checked, in the engine-relative form
                        required by _check_afc() (resolved against the AFC
                        service's own root). Should have exactly same path
                        as required in AFC Config
        db_diff      -- None or difference from previous database
        manifest_key -- Filename key to look up in the hash manifest (defaults
                        to basename of new_filename)
        expected_digest -- If set, verify SHA-256 against this digest (the
                        post-embed continuity baseline) instead of the
                        manifest entry
        hash_only    -- If True run only the cryptographic hash gate and
                        skip content (diff/AFC) checks
        hash_filename -- Absolute, locally-openable path to the exact file
                        object being activated. The hash gate must open this
                        file, not a re-derived engine-relative form that may
                        not resolve from the container's CWD. Defaults to
                        new_filename when not given.
        Returns True if check passed
        """
        hash_results: Dict[str, Optional[str]] = {}
        hash_tested, hash_errmsg = self._check_hash(
            hash_filename if hash_filename is not None else new_filename,
            manifest_key or os.path.basename(new_filename),
            expected_digest=expected_digest)
        if hash_tested:
            if hash_errmsg is not None:
                logging.error(hash_errmsg)
            hash_results["SHA-256 hash"] = hash_errmsg
        else:
            hash_errmsg = (
                "ULS_HASH_MANIFEST_URL/FILE not configured \u2014 refusing to "
                "activate unverified ULS database (fail-closed)")
            logging.error(hash_errmsg)
            hash_results["SHA-256 hash"] = hash_errmsg
        self._status_updater.status_check(CheckType.HashVerification,
                                          hash_results)
        if hash_errmsg is not None:
            # Cryptographic integrity gate failed — do NOT hand the
            # unverified sqlite to _check_afc / afc-engine.
            return False

        if hash_only:
            # ULS_FORCE_UPDATE: content (diff/AFC) checks may be skipped,
            # the cryptographic gate above may not
            return True

        check_results: Dict[str, Optional[str]] = {}
        for item, (tested, errmsg) in \
                [("difference from previous", self._check_diff(db_diff)),
                 ("usable by AFC Service", self._check_afc(new_filename))]:
            if not tested:
                continue
            if errmsg is not None:
                logging.error(errmsg)
            check_results[item] = errmsg
        self._status_updater.status_check(CheckType.FsDatabase, check_results)
        return all(errmsg is None
                   for errmsg in list(hash_results.values()) +
                   list(check_results.values()))

    def _check_hash(self, new_filename: str,
                    manifest_key: str,
                    expected_digest: Optional[str] = None) \
            -> Tuple[bool, Optional[str]]:
        """ Verify SHA-256 of new_filename against a JSON hash manifest.

        The manifest is a JSON object whose keys are bare database filenames
        and whose values are lowercase SHA-256 hex digests, e.g.:
          { "FS_2026-04-29T15_26_38_..._sorted_param.sqlite3": "abcd1234..." }

        If neither hash_manifest_url nor hash_manifest_file is configured the
        check fails closed (returns an error) so that valid() rejects the
        database — a cryptographic integrity anchor is mandatory.

        Returns (check_performed, error_message) tuple
        """
        if not (self._hash_manifest_url or self._hash_manifest_file):
            return (True,
                    "Neither ULS_HASH_MANIFEST_URL nor ULS_HASH_MANIFEST_FILE "
                    "is configured — refusing to activate database without a "
                    "cryptographic integrity anchor. Configure a hash manifest.")

        # Compute SHA-256 of the candidate file in streaming chunks
        sha256 = hashlib.sha256()
        try:
            with open(new_filename, "rb") as f:
                for chunk in iter(lambda: f.read(65536), b""):
                    sha256.update(chunk)
        except OSError as ex:
            return (True, f"Cannot read '{new_filename}' for hash check: {ex}")
        computed_hash = sha256.hexdigest()

        # Post-embed continuity check: the pre-embed file already passed
        # the manifest gate this cycle; the activation copy must match the
        # trusted local FSID embed's output byte-for-byte (the manifest
        # entry can only match the pre-embed bytes)
        if expected_digest is not None:
            if computed_hash.lower() != expected_digest.lower():
                return (True,
                        f"SHA-256 mismatch for '{manifest_key}': computed "
                        f"{computed_hash}, post-embed digest "
                        f"{expected_digest}")
            logging.info(f"SHA-256 verified against post-embed digest for "
                         f"'{manifest_key}': {computed_hash}")
            return (True, None)

        # Load manifest: local file takes precedence over URL
        manifest: Optional[Dict[str, str]] = None
        manifest_raw_bytes: Optional[bytes] = None
        try:
            if self._hash_manifest_file and \
                    os.path.isfile(self._hash_manifest_file):
                with open(self._hash_manifest_file, "rb") as f:
                    manifest_raw_bytes = f.read()
                manifest = json.loads(manifest_raw_bytes.decode("utf-8"))
            elif self._hash_manifest_url:
                logging.info(
                    f"Fetching hash manifest from '{self._hash_manifest_url}'")
                if not self._hash_manifest_url.startswith("https://"):
                    return (True, "Hash manifest URL must use https:// scheme (http:// is rejected to protect manifest integrity)")
                req = urllib.request.Request(
                    self._hash_manifest_url,
                    headers={"User-Agent": "uls_service/1.0"})
                with urllib.request.urlopen(req, timeout=60) as resp:
                    if not resp.geturl().startswith("https://"):
                        return (True,
                                "Hash manifest URL redirected away from "
                                "https:// — refusing downgraded transport")
                    manifest_raw_bytes = resp.read((1 << 20) + 1)
                if len(manifest_raw_bytes) > (1 << 20):
                    return (True,
                            "Hash manifest exceeds 1 MiB cap — refusing "
                            "oversized response (possible DoS)")
                manifest = json.loads(manifest_raw_bytes.decode("utf-8"))
        except (OSError, urllib.error.URLError,
                json.JSONDecodeError, ValueError) as ex:
            return (True, f"Cannot load hash manifest: {ex}")

        # Authenticate the manifest with HMAC-SHA256.
        # Whatever the manifest transport (URL or file), the HMAC key MUST be
        # set (fail closed): a file-mode manifest typically lives on the same
        # writable share as the databases it attests, so without independent
        # authentication an attacker who can replace a database can forge a
        # matching manifest entry too (and skip the _iat replay binding).
        if not self._hash_manifest_hmac_key:
            return (True,
                    "A hash manifest source is configured but "
                    "ULS_HASH_MANIFEST_HMAC_KEY is not — refusing to use an "
                    "unauthenticated manifest. Set ULS_HASH_MANIFEST_HMAC_KEY "
                    "to an independent secret and publish the .hmac companion "
                    "(update_uls_manifest.sh --hmac-key-file).")
        if self._hash_manifest_hmac_key:
            import hmac as _hmac_mod
            if manifest_raw_bytes is None:
                return (True, "Manifest bytes unavailable for HMAC verification")
            # Fetch or load the companion signature
            hmac_sig_bytes: Optional[bytes] = None
            try:
                if self._hash_manifest_file and \
                        os.path.isfile(self._hash_manifest_file + ".hmac"):
                    with open(self._hash_manifest_file + ".hmac", "rb") as f:
                        hmac_sig_bytes = f.read().strip()
                elif self._hash_manifest_url:
                    hmac_url = self._hash_manifest_url + ".hmac"
                    hmac_req = urllib.request.Request(
                        hmac_url,
                        headers={"User-Agent": "uls_service/1.0"})
                    with urllib.request.urlopen(hmac_req, timeout=30) as hr:
                        if not hr.geturl().startswith("https://"):
                            return (True,
                                    "Manifest HMAC URL redirected away from "
                                    "https:// — refusing downgraded transport")
                        hmac_sig_bytes = hr.read((1 << 20) + 1).strip()
            except (OSError, urllib.error.URLError) as ex:
                return (True,
                        f"Cannot fetch manifest HMAC signature: {ex}")
            if not hmac_sig_bytes:
                return (True,
                        "Manifest HMAC signature absent but "
                        "ULS_HASH_MANIFEST_HMAC_KEY is configured — "
                        "refusing to accept unauthenticated manifest")
            expected_hmac = _hmac_mod.new(
                self._hash_manifest_hmac_key.encode("utf-8"),
                manifest_raw_bytes,
                hashlib.sha256).hexdigest().encode("ascii")
            if not _hmac_mod.compare_digest(hmac_sig_bytes, expected_hmac):
                return (True,
                        "Manifest HMAC-SHA256 mismatch — "
                        "manifest integrity check failed; "
                        "refusing to use this manifest")
            logging.info("Manifest HMAC-SHA256 verified OK")

        if manifest is None:
            return (True, "Hash manifest not available")
        if not isinstance(manifest, dict):
            return (True, "Hash manifest is not a JSON object")

        # Replay defence (CWE-294): the manifest must carry a monotonically
        # non-decreasing '_iat' epoch bound under the HMAC. Reject any
        # manifest older than the last one accepted; persist the high-water
        # mark alongside the FS database directory.
        if self._hash_manifest_hmac_key:
            try:
                manifest_iat = int(manifest.get("_iat", ""))
            except (TypeError, ValueError):
                return (True,
                        "Hash manifest missing/invalid '_iat' freshness "
                        "field — refusing manifest without replay binding")
            iat_path = os.path.join(os.path.dirname(new_filename),
                                    ".uls_manifest_iat")
            last_iat = -1
            try:
                with open(iat_path, encoding="ascii") as iatf:
                    last_iat = int(iatf.read().strip())
            except (OSError, ValueError):
                pass
            if manifest_iat < last_iat:
                return (True,
                        f"Hash manifest _iat={manifest_iat} older than last "
                        f"accepted {last_iat} — replay rejected")
            if manifest_iat > last_iat:
                try:
                    with open(iat_path, "w", encoding="ascii") as iatf:
                        iatf.write(str(manifest_iat))
                except OSError as ex:
                    logging.warning(f"Cannot persist manifest _iat: {ex}")

        expected_hash = manifest.get(manifest_key)
        if expected_hash is None:
            return (True,
                    f"No hash entry for '{manifest_key}' in manifest "
                    f"(manifest contains {len(manifest)} entries)")
        if computed_hash.lower() != expected_hash.lower():
            return (True,
                    f"SHA-256 mismatch for '{manifest_key}': "
                    f"computed {computed_hash}, manifest says {expected_hash}")
        logging.info(f"SHA-256 verified for '{manifest_key}': {computed_hash}")
        return (True, None)

    def _check_diff(self, db_diff: Optional[DbDiff]) \
            -> Tuple[bool, Optional[str]]:
        """ Checks amount of difference since previous database.
        Returns (check_performed, error_message) tuple """
        if db_diff is None:
            return (False, None)
        if not db_diff.valid:
            return (True, "Database difference can't be obtained")
        if ((db_diff.diff_len == 0) and (db_diff.ras_diff_len == 0)) != \
                ((len(db_diff.diff_tiles) == 0) and
                 (len(db_diff.diff_beams) == 0)):
            return \
                (True,
                 f"Inconsistent indication of database difference: difference "
                 f"is in {db_diff.diff_len} paths and in "
                 f"{db_diff.ras_diff_len} RAS entries, but in "
                 f"{len(db_diff.diff_beams)} beams and "
                 f"{len(db_diff.diff_tiles)} tiles")
        if self._max_change_percent is None:
            return (False, None)
        # Use max(prev_len, new_len) as denominator so targeted deletion
        # (shrinking the DB) is not diluted by new_len becoming smaller.
        denominator = max(db_diff.prev_len, db_diff.new_len)
        diff_percent = \
            round(
                100 if denominator == 0 else
                ((db_diff.diff_len * 100) / denominator),
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
                       ("subdir", str),
                       # List of files
                       ("files", List[str])])

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
                        url = f"{epf.base_url}/{filename}"
                        if not os.path.isfile(internal_file_name):
                            errmsg = f"'{internal_file_name}' not foound in " \
                                f"container. It should be downloaded from " \
                                f"'{url}', verified and added to image"
                            continue
                        try:
                            external_file_name = os.path.join(temp_dir,
                                                              filename)
                            if not url.startswith("https://"):
                                raise ValueError(
                                    f"URL '{url}' must use https://")

                            def _cap(blocknum: int, blocksize: int,
                                     totalsize: int) -> None:
                                if blocknum * blocksize > (1 << 20):
                                    raise urllib.error.URLError(
                                        f"'{url}' exceeds 1 MiB cap — "
                                        f"refusing oversized response")
                            import socket
                            socket.setdefaulttimeout(10.0)
                            urllib.request.urlretrieve(
                                url, external_file_name, reporthook=_cap)
                        except urllib.error.URLError as ex:
                            logging.warning(f"Error downloading '{url}': {ex}")
                            errmsg = f"Error downloading '{url}': {ex}, so " \
                                f"it was not compared with " \
                                f"'{internal_file_name}'. Maybe next time..."
                            continue
                        contents: List[bytes] = []
                        try:
                            for fn in (internal_file_name, external_file_name):
                                with open(fn, "rb") as f:
                                    contents.append(f.read())
                        except OSError as ex:
                            logging.warning(f"Read failed: {ex}")
                            errmsg = f"Can't read '{fn}', so '{url}' was " \
                                f"not compared with '{internal_file_name}'"
                        if contents[0] != contents[1]:
                            errmsg = f"Content of '{url}' differs from " \
                                f"'{internal_file_name}'. Former should be " \
                                f"downloaded, verified and added to image " \
                                f"as latter"
                    finally:
                        check_results[os.path.join(epf.subdir, filename)] = \
                            errmsg
        finally:
            self._status_updater.status_check(check_type=CheckType.ExtParams,
                                              results=check_results)
            if temp_dir:
                shutil.rmtree(temp_dir, ignore_errors=True)


class AlsRecord(pydantic.BaseModel):
    """ Stuff logged into ALS regarding download attempt """
    # Downloader settings
    settings: Dict[str, Any]
    # None or download start time in ISO format
    start_time: Optional[str] = None
    # None or external dependencies ccheck completion time in ISO format
    externals_checked_time: Optional[str] = None
    # None or download script completion time in ISO format
    downloaded_time: Optional[str] = None
    # None or list of updated regions
    updated_regions: Optional[List[str]] = None
    # None or DB diffrence report
    db_difference: Optional[str] = None
    # None or validation completion time in ISO format
    validated_time: Optional[str] = None
    # None or list of invalidated Rcache items
    invalidation: Optional[List[str]] = None
    # None new FS DB file name
    fs_db_name: Optional[str] = None
    # None or fs database update time in ISO format
    updated_time: Optional[str] = None

    def now(self, field_name: str) -> None:
        """ Sets given field to current datetime as ISO-formatted string """
        assert field_name.endswith("_time")
        setattr(self, field_name,
                datetime.datetime.now(datetime.timezone.utc).isoformat())


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
        "--save_dir", metavar="SAVE_DIR",
        help=f"Directory containing uls downloader's previously successful downloaded region files"
        f"per region){env_help(Settings, 'save_dir')}")
    argument_parser.add_argument(
        "--num_save", metavar="NUM_SAVE",
        help=f"Number of previously successful region files to save"
        f"expected ){env_help(Settings, 'num_saves')}")
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
        "--service_state_db_password_file", metavar="PASSWORD_FILE",
        help=f"Name of file with password to use in state database connection "
        f"string{env_help(Settings, 'service_state_db_password_file')}")
    argument_parser.add_argument(
        "--db_creator_url", metavar="URL",
        help=f"Postgres database creation REST API URL"
        f"{env_help(Settings, 'db_creator_url')}")
    argument_parser.add_argument(
        "--alembic_config", metavar="ALEMBIC_CONFIG_FILE",
        help=f"Alembic config file for state database. No Alembic "
        "manipulations performed if unspecified"
        f"{env_help(Settings, 'alembic_config')}")
    argument_parser.add_argument(
        "--alembic_initial_version", metavar="INITIAL_ALEMBIC_VERSION",
        help=f"Alembic version to stamp existing database without Alembic"
        f"{env_help(Settings, 'alembic_initial_version')}")
    argument_parser.add_argument(
        "--alembic_head_version", metavar="ALEMBIC_HEAD_VERSION",
        help=f"Alembic version to stamp newly created database. Default is "
        f"'head'{env_help(Settings, 'alembic_head_version')}")
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
        "--hash_manifest_url", metavar="URL",
        help=f"HTTPS URL of JSON manifest mapping DB filenames to expected "
        f"SHA-256 hashes. Manifest format: "
        f'{{ "FS_<timestamp>.sqlite3": "<sha256hex>", ... }}'
        f"{env_help(Settings, 'hash_manifest_url')}")
    argument_parser.add_argument(
        "--hash_manifest_file", metavar="FILENAME",
        help=f"Local path to JSON manifest mapping DB filenames to expected "
        f"SHA-256 hashes (same format as hash_manifest_url)"
        f"{env_help(Settings, 'hash_manifest_file')}")
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
        als.als_initialize(client_id="fs_downloader")

        if settings.run_once and (settings.prometheus_port is not None):
            logging.warning(
                "There is no point in using Prometheus in run-once mode. Use "
                "--statsd_server if metrics are necessary")
            settings.prometheus_port = None

        print_args(settings)

        error_if(settings.service_state_db_dsn is None,
                 "State database DSN not specified")
        state_db = \
            StateDb(db_dsn=cast(str, settings.service_state_db_dsn),
                    db_password_file=settings.service_state_db_password_file)
        state_db.create_db(
            db_creator_url=settings.db_creator_url,
            alembic_config=settings.alembic_config,
            alembic_initial_version=settings.alembic_initial_version,
            alembic_head_version=settings.alembic_head_version)

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
                hash_manifest_url=settings.hash_manifest_url,
                hash_manifest_file=settings.hash_manifest_file,
                hash_manifest_hmac_key=settings.hash_manifest_hmac_key,
                executor=executor, status_updater=status_updater)

        rcache_settings = \
            RcacheClientSettings(
                enabled=settings.rcache_enabled and bool(settings.rcache_url),
                service_url=settings.rcache_url, postgres_dsn=None,
                rmq_dsn=None)
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
            # settings.model_dump() returns raw values; pydantic does
            # NOT apply json_schema_extra["convert"] (safe_dsn) used by
            # print_args().  Build the ALS dict manually so DSN fields have
            # their passwords redacted before being published to Kafka/AFC_LOGS.
            _settings_dict = {}
            for _name, _field_info in Settings.model_fields.items():
                _value = getattr(settings, _name)
                _extra = _field_info.json_schema_extra or {}
                if not isinstance(_extra, dict):
                    _extra = {}
                if "convert" in _extra:
                    _value = _extra["convert"](_value)
                _settings_dict[_name] = _value
            als_record = AlsRecord(settings=_settings_dict)
            als_record.now(field_name="start_time")
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
                        for entry in os.listdir(dir_to_clean):
                            entry_path = os.path.join(dir_to_clean, entry)
                            try:
                                if os.path.isdir(entry_path) and \
                                        not os.path.islink(entry_path):
                                    shutil.rmtree(entry_path)
                                else:
                                    os.remove(entry_path)
                            except OSError as ex:
                                raise RuntimeError(
                                    f"Failed to clean '{entry_path}': {ex}"
                                ) from ex

                logging.info("Copying RAS database")
                shutil.copyfile(settings.ext_ras_database,
                                settings.ras_database)

                logging.info("Checking if external parameter files changed")
                ext_params_file_checker.check()
                als_record.now(field_name="externals_checked_time")

                # Issue download script
                cmdline_args: List[str] = []
                if settings.nice and (os.name == "posix"):
                    cmdline_args.append("nice")
                cmdline_args.append(settings.download_script)
                if settings.region:
                    cmdline_args += ["--region", settings.region]
                cmdline_args += ["--save_dir", settings.save_dir]
                if settings.download_script_args:
                    cmdline_args += shlex.split(settings.download_script_args)
                logging.info(f"Starting {' '.join(cmdline_args)}")
                executor.execute(
                    cmdline_args,
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

                # Run SHA-256/HMAC manifest gate BEFORE
                # constructing DbDiff or calling get_uls_identity so no
                # FCC-derived SQLite content is parsed prior to
                # cryptographic verification.
                temp_uls_file_name = \
                    os.path.join(full_ext_db_dir,
                                 "temp_" + os.path.basename(new_uls_file))
                logging.debug(
                    f"Copying '{new_uls_file}' to '{temp_uls_file_name}'")
                shutil.copy2(new_uls_file, temp_uls_file_name)
                # ULS_FORCE_UPDATE may bypass the hash gate only when no
                # manifest is configured (local dev): force must not
                # silently disable the cryptographic integrity anchor.
                # Open the absolute staged path (temp_uls_file_name), not a
                # re-derived engine-relative form: the latter is resolved
                # against the AFC service's root, not the container's CWD,
                # and does not exist there — the engine-relative form is
                # only correct for _check_afc() (see the valid() call below).
                if not (uls_file_checker.hash_valid(
                        filename=temp_uls_file_name,
                        manifest_key=os.path.basename(new_uls_file))
                        or (settings.force and
                            not uls_file_checker.manifest_configured)):
                    raise ProcessingException(
                        "Hash verification failed; aborting ULS swap")

                # Read the already-verified temp copy, not the original in
                # the shared NFS result_dir: after the hash gate passes,
                # every subsequent parse must target the exact attested
                # bytes, not a path a concurrent writer to result_dir could
                # still swap (TOCTOU).
                new_uls_identity = get_uls_identity(temp_uls_file_name)
                if new_uls_identity is None:
                    raise ProcessingException(
                        "Generated ULS file does not contain identity "
                        "information")
                status_updater.milestone(DownloaderMilestone.DownloadSuccess)
                als_record.now(field_name="downloaded_time")

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
                als_record.updated_regions = list(updated_regions)

                # If anything was updated - do the update routine
                if updated_regions or settings.force:
                    # Embed updated FSID table directly into the
                    # already-verified temp copy, not the original in the
                    # shared NFS result_dir: never touch the swappable
                    # original again after the hash gate above passes
                    # (closes the TOCTOU window between the copy at :1668
                    # and any later read of the original path).
                    logging.info("Embedding FSID table")
                    executor.execute(
                        [FSID_TOOL, "embed", temp_uls_file_name,
                         settings.fsid_file],
                        fail_on_error=True)

                    # Record the post-embed digest: the activation gate
                    # verifies the staged copy against the exact output of
                    # the trusted FSID embed of the manifest-verified file
                    # (one manifest entry cannot match both pre- and
                    # post-embed bytes)
                    post_embed_sha256 = hashlib.sha256()
                    with open(temp_uls_file_name, "rb") as f:
                        for chunk in iter(lambda: f.read(65536), b""):
                            post_embed_sha256.update(chunk)
                    post_embed_digest = post_embed_sha256.hexdigest()

                    db_diff = DbDiff(prev_filename=current_uls_file,
                                     new_filename=temp_uls_file_name,
                                     executor=executor,
                                     allow_directional_invalidate=settings.
                                     rcache_directional_invalidate) \
                        if has_previous else None
                    if not has_previous:
                        # Cold-start or deleted symlink — fail-closed: require
                        # the hash manifest to pass before first activation.
                        # A cryptographic manifest is required to activate
                        # a database when no prior baseline exists for diff checks.
                        if not (settings.hash_manifest_url or
                                settings.hash_manifest_file):
                            logging.error(
                                "Cold start: no previous ULS database and "
                                "ULS_HASH_MANIFEST_URL/FILE not configured — "
                                "refusing to activate unverified database. "
                                "Configure a hash manifest for cold-start "
                                "bootstrap integrity.")
                            raise ProcessingException(
                                "Cold-start activation requires hash manifest")
                    if db_diff:
                        als_record.db_difference = str(db_diff)
                    if uls_file_checker.valid(
                            base_dir=settings.ext_db_dir,
                            # Engine-relative form: correct for _check_afc(),
                            # which resolves it against the AFC service's
                            # own root, not this container's CWD.
                            new_filename=os.path.join(
                                os.path.dirname(settings.ext_db_symlink),
                                os.path.basename(temp_uls_file_name)),
                            db_diff=db_diff,
                            manifest_key=os.path.basename(
                                new_uls_file),
                            expected_digest=post_embed_digest,
                            hash_only=settings.force,
                            # The hash gate must open the exact absolute
                            # file object being activated, not the
                            # engine-relative form above (which does not
                            # resolve from this container's CWD).
                            hash_filename=temp_uls_file_name) or \
                            (settings.force and
                             not uls_file_checker.manifest_configured):
                        if not settings.force:
                            als_record.now(field_name="validated_time")
                        if settings.force:
                            status_updater.status_check(CheckType.FsDatabase,
                                                        None)
                        # Renaming database
                        permanent_uls_file_name = \
                            os.path.join(full_ext_db_dir,
                                         os.path.basename(new_uls_file))
                        os.rename(temp_uls_file_name, permanent_uls_file_name)
                        # Invalidate rcache BEFORE retargeting the symlink;
                        # if invalidation fails, abort the swap so stale
                        # entries are not served from the newly-swapped DB.
                        if rcache and (db_diff is not None):
                            if db_diff.diff_beams:
                                beam_list = \
                                    "<" + \
                                    ">, <".join(beam.short_str() for beam in
                                                db_diff.diff_beams[: 1000]) + \
                                    ">"
                                logging.info(f"Requesting invalidation of the "
                                             f"following beams: {beam_list}")
                                als_record.invalidation = \
                                    [beam.short_str() for beam in
                                     db_diff.diff_beams[: 1000]]
                                try:
                                    rcache.rcache_directional_invalidate(
                                        beams=db_diff.diff_beams)
                                except Exception as rcache_ex:  # noqa: BLE001
                                    raise ProcessingException(
                                        f"rcache directional invalidation "
                                        f"failed; aborting DB swap to prevent "
                                        f"stale entries: {rcache_ex}") from \
                                        rcache_ex
                            if db_diff.diff_tiles:
                                tile_list = \
                                    "<" + \
                                    ">, <".join(tile.short_str() for tile in
                                                db_diff.diff_tiles[: 1000]) + \
                                    ">"
                                logging.info(f"Requesting invalidation of the "
                                             f"following tiles: {tile_list}")
                                als_record.invalidation = \
                                    (als_record.invalidation or []) + \
                                    [tile.short_str() for tile in
                                     db_diff.diff_tiles[: 1000]]
                                try:
                                    rcache.rcache_spatial_invalidate(
                                        tiles=db_diff.diff_tiles)
                                except Exception as rcache_ex:  # noqa: BLE001
                                    raise ProcessingException(
                                        f"rcache spatial invalidation "
                                        f"failed; aborting DB swap to prevent "
                                        f"stale entries: {rcache_ex}") from \
                                        rcache_ex
                        # Retargeting symlink — only after rcache is clean
                        update_uls_file(
                            uls_dir=full_ext_db_dir,
                            uls_file=os.path.basename(new_uls_file),
                            symlink=os.path.basename(settings.ext_db_symlink),
                            executor=executor)
                        als_record.fs_db_name = os.path.basename(new_uls_file)
                        als_record.now(field_name="updated_time")

                        # drain in-flight afc-engine workers that
                        # opened the *previous* ULS sqlite via the old
                        # symlink target, then re-invalidate.  Without this
                        # a worker that read the old DB can INSERT a stale
                        # grant into rcache *after* the invalidation above
                        # (the cache-miss path is a fresh INSERT and so
                        # bypasses the on_conflict WHERE state != Invalid
                        # guard).  Drain bound must be >= the Celery
                        # soft_time_limit on afc.run (afc_worker.py) so
                        # every old-DB task has completed or been killed
                        # before re-invalidation.
                        if rcache and (db_diff is not None) and \
                                (db_diff.diff_beams or db_diff.diff_tiles):
                            _drain_s = int(os.getenv(
                                "ULS_WORKER_DRAIN_SECONDS", "1800"))
                            logging.info(
                                f"Draining in-flight workers for "
                                f"{_drain_s}s before post-swap rcache "
                                f"re-invalidation")
                            time.sleep(_drain_s)
                            # Post-swap re-invalidation is fail-closed:
                            # retry with backoff up to ULS_WORKER_DRAIN_SECONDS
                            # total, then raise ProcessingException so the
                            # healthcheck alarm fires (mirrors the pre-swap
                            # fail-closed behaviour above).
                            _reinv_deadline = time.time() + _drain_s
                            _reinv_delay = 5
                            _reinv_exc: Optional[Exception] = None
                            while True:
                                try:
                                    _reinv_exc = None
                                    if db_diff.diff_beams:
                                        rcache.rcache_directional_invalidate(
                                            beams=db_diff.diff_beams)
                                    if db_diff.diff_tiles:
                                        rcache.rcache_spatial_invalidate(
                                            tiles=db_diff.diff_tiles)
                                    break
                                except Exception as _ex:
                                    _reinv_exc = _ex
                                    if time.time() + _reinv_delay > \
                                            _reinv_deadline:
                                        break
                                    logging.warning(
                                        f"Post-swap rcache re-invalidation "
                                        f"failed ({_ex}); retrying in "
                                        f"{_reinv_delay}s")
                                    time.sleep(_reinv_delay)
                                    _reinv_delay = min(_reinv_delay * 2, 60)
                            if _reinv_exc is not None:
                                logging.error(
                                    f"Post-swap rcache re-invalidation "
                                    f"permanently failed: {_reinv_exc}")
                                status_updater.status_check(
                                    CheckType.FsDatabase,
                                    {"rcache_reinvalidate":
                                     str(_reinv_exc)})
                                raise ProcessingException(
                                    f"Post-swap rcache re-invalidation "
                                    f"failed after retries: {_reinv_exc}")

                        # Upon success, save the new region files
                        if settings.force:
                            # Saves regardless of new data
                            save_recent_download(set(new_uls_identity.keys()), settings.num_saves, settings.temp_dir, settings.save_dir)
                        else:
                            # Saves based on new data
                            save_recent_download(updated_regions, settings.num_saves, settings.temp_dir, settings.save_dir)

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
            except (OSError, subprocess.SubprocessError, ProcessingException,
                    sa.exc.SQLAlchemyError) as ex:
                err_msg = str(ex)
                logging.error(f"Download failed: {ex}")
            finally:
                als.als_json_log(topic="fs_download", record=als_record.model_dump())
                if settings.run_once:
                    flushed = als.als_flush()
                    if not flushed:
                        logging.warning("ALS flush failed")
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
