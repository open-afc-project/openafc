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
# pylint: disable=too-many-nested-blocks

import argparse
import datetime
import glob
import logging
import os
import pydantic
import shlex
import shutil
import sqlalchemy as sa
import subprocess
import sys
import tempfile
import time
from typing import cast, Dict, List, Optional, Set

from uls_service_common import *
from rcache_client import RcacheClient
from rcache_models import LatLonRect, RcacheClientSettings

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


class Settings(pydantic.BaseSettings):
    """ Arguments from command lines - with their default values """
    download_script: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/daily_uls_parse/daily_uls_parse.py",
            env="ULS_DOWNLOAD_SCRIPT")
    download_script_args: Optional[str] = \
        pydantic.Field(None, env="ULS_DOWNLOAD_SCRIPT_ARGS")
    region: Optional[str] = \
        pydantic.Field(None, env="ULS_DOWNLOAD_REGION")
    result_dir: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/ULS_Database/", env="ULS_RESULT_DIR")
    temp_dir: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/daily_uls_parse/temp/", env="ULS_TEMP_DIR")
    ext_db_dir: str = pydantic.Field(..., env="ULS_EXT_DB_DIR")
    ext_db_symlink: str = pydantic.Field(..., env="ULS_CURRENT_DB_SYMLINK")
    fsid_file: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/daily_uls_parse/data_files/fsid_table.csv",
            env="ULS_FSID_FILE")
    ext_ras_database: str = pydantic.Field(..., env="ULS_EXT_RAS_DATABASE")
    ras_database: str = \
        pydantic.Field(
            "/mnt/nfs/rat_transfer/daily_uls_parse/data_files/RASdatabase.dat",
            env="ULS_RAS_DATABASE")
    status_dir: str = pydantic.Field(DEFAULT_STATUS_DIR, env="ULS_STATUS_DIR")
    check_ext_files: Optional[List[str]] = \
        pydantic.Field(
            "https://raw.githubusercontent.com/Wireless-Innovation-Forum/"
            "6-GHz-AFC/main/data/common_data"
            ":raw_wireless_innovation_forum_files"
            ":antenna_model_diameter_gain.csv,billboard_reflector.csv,"
            "category_b1_antennas.csv,high_performance_antennas.csv,"
            "fcc_fixed_service_channelization.csv,"
            "transmit_radio_unit_architecture.csv",
            env="ULS_CHECK_EXT_FILES")
    max_change_percent: Optional[float] = \
        pydantic.Field(10., env="ULS_MAX_CHANGE_PERCENT")
    afc_url: Optional[str] = pydantic.Field(None, env="ULS_AFC_URL")
    afc_parallel: Optional[int] = pydantic.Field(None, env="ULS_AFC_PARALLEL")
    rcache_url: Optional[str] = pydantic.Field(None, env="RCACHE_SERVICE_URL")
    rcache_enabled: bool = pydantic.Field(True, env="RCACHE_ENABLED")
    delay_hr: float = pydantic.Field(0., env="ULS_DELAY_HR")
    interval_hr: float = pydantic.Field(4, env="ULS_INTERVAL_HR")
    timeout_hr: float = pydantic.Field(1, env="ULS_TIMEOUT_HR")
    nice: bool = pydantic.Field(False, env="ULS_NICE")
    verbose: bool = pydantic.Field(False)
    run_once: bool = pydantic.Field(False, env="ULS_RUN_ONCE")
    force: bool = pydantic.Field(False)

    @pydantic.validator("check_ext_files", pre=True)
    @classmethod
    def check_ext_files_str_to_list(cls, v: Any) -> Any:
        """ Converts string value of 'check_ext_files' from environment from
        string to list """
        return [v] if isinstance(v, str) else v

    @pydantic.root_validator(pre=True)
    @classmethod
    def remove_empty(cls, v: Any) -> Any:
        """ Prevalidator that removes empty values (presumably from environment
        variables) to force use of defaults
        """
        if not isinstance(v, dict):
            return v
        for key, value in list(v.items()):
            if value is None:
                del v[key]
        return v


class ProcessingException(Exception):
    """ ULS processing exception """
    pass


def print_args(settings: Settings) -> None:
    """ Print invocation parameters to log """
    logging.info("ULS downloader started with the following parameters")
    logging.info(f"ULS download script: {settings.download_script}")
    logging.info(f"Download regions: "
                 f"{settings.region if settings.region else 'All'}")
    logging.info(f"Additional ULS download script arguments: "
                 f"{settings.download_script_args}")
    logging.info(f"Directory where ULS download script puts downloaded file: "
                 f"{settings.result_dir}")
    logging.info(f"Temporary directory of ULS download script, cleaned before "
                 f"downloading: {settings.temp_dir}")
    logging.info(f"Ultimate downloaded file destination directory: "
                 f"{settings.ext_db_dir}")
    logging.info(f"Symlink pointing to current ULS file: "
                 f"{settings.ext_db_symlink}")
    logging.info(f"RAS database: {settings.ext_ras_database}")
    logging.info(f"Where from ULS script reads RAS database: "
                 f"{settings.ras_database}")
    logging.info(f"FSID file location expected by ULS download script: "
                 f"{settings.fsid_file}")
    logging.info(f"ULS download service state storage directory: "
                 f"{settings.status_dir}")
    mcp = "Unspecified" if settings.max_change_percent is None \
        else f"{settings.max_change_percent}%"
    logging.info(f"Limit on paths changed since previous: {mcp}")
    logging.info(
        f"AFC Service URL to use for database validity check: "
        f"{'Unspecified' if settings.afc_url is None else settings.afc_url}")
    logging.info(f"ULS download interval in hours: {settings.interval_hr}")
    logging.info(f"ULS download script maximum duration in hours: "
                 f"{settings.timeout_hr}")
    logging.info(f"Rcache spatial invalidation: "
                 f"{'Enabled' if settings.rcache_enabled else 'Disabled'}")
    logging.info(f"Rcache URL: {settings.rcache_url}")
    logging.info(f"Print debug info: {'Yes' if settings.verbose else 'No'}")
    logging.info(f"Run: {'Once' if settings.run_once else 'Indefinitely'}")
    logging.info(f"Update forced: {'Yes' if settings.force else 'No'}")


def extract_fsid_table(uls_file: str, fsid_file: str) -> None:
    """ Try to extract FSID table from ULS database:

    Arguments:
    uls_file  -- FS database filename
    fsid_file -- FSID table filename
    """
    # Clean previous FSID files...
    try:
        fsid_name_parts = os.path.splitext(fsid_file)
        logging.info("Extracting FSID table")
        subprocess.run(f"rm -f {fsid_name_parts[0]}*{fsid_name_parts[1]}",
                       shell=True, check=True)
    except OSError as ex:
        logging.warning(f"Strangely can't remove "
                        f"{fsid_name_parts[0]}*{fsid_name_parts[1]}. "
                        f"Proceeding nevertheless: {ex}")
    # ... and extracting latest one from previous FS database
    if os.path.isfile(uls_file):
        if subprocess.run(
                [FSID_TOOL, "check", uls_file], check=False).returncode == 0:
            subprocess.run([FSID_TOOL, "extract", uls_file, fsid_file],
                           check=True)


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


def update_uls_file(uls_dir: str, uls_file: str, symlink: str) -> None:
    """ Atomically retargeting symlink to new ULS file

    Arguments:
    uls_dir  -- Directory containing ULS files and symlink
    uls_file -- Base name of new ULS file (already in ULS directory)
    symlink  -- Base name of symlink pointing to current ULS file
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
    subprocess.check_call(["mv", "-fT", temp_symlink_name,
                           os.path.join(uls_dir, symlink)])
    logging.info(f"FS database symlink '{os.path.join(uls_dir, symlink)}' "
                 f"now points to '{uls_file}'")


def update_region_change_times(status_storage: StatusStorage,
                               all_regions: List[str],
                               updated_regions: Set[str]) -> None:
    """ Update region update times status

    Arguments:
    status_storage  -- Status storage facility
    all_regions     -- All regions contained in ULS file
    updated_regions -- Regions changed since previous ULS file
    """
    data_update_times = \
        status_storage.read_reg_data_changes(StatusStorage.S.RegionUpdate)
    for region in all_regions:
        if (region in updated_regions) or (region not in data_update_times):
            data_update_times[region] = datetime.datetime.now()
    status_storage.write_reg_data_changes(StatusStorage.S.RegionUpdate,
                                          data_update_times)


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
    def __init__(self, prev_filename: str, new_filename: str) \
            -> None:
        """ Constructor

        Arguments:
        prev_filename -- Previous file name
        new_filename  -- New filename
        """
        self.valid = False
        self.prev_len = 0
        self.new_len = 0
        self.diff_len = 0
        self.diff_tiles: List[LatLonRect] = []
        logging.info("Getting differences with previous database")
        try:
            p = subprocess.run(
                [FS_DB_DIFF, "--report_tiles", prev_filename, new_filename],
                stdout=subprocess.PIPE, check=True, timeout=10 * 60,
                encoding="ascii")
        except (subprocess.SubprocessError, OSError) as ex:
            logging.error(f"Database comparison failed: {ex}")
            return
        m = re.search(r"Paths in DB1:\s+(?P<db1>\d+)(.|\n)+"
                      r"Paths in DB2:\s+(?P<db2>\d+)(.|\n)+"
                      r"Different paths:\s+(?P<diff>\d+)(.|\n)+"
                      r"Different RAS entries:\s+(?P<ras_diff>\d+)(.|\n)+",
                      cast(str, p.stdout))
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
                r"(?P<lon_sign>[EW])", cast(str, p.stdout)):
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
    """
    def __init__(self, max_change_percent: Optional[float] = None,
                 afc_url: Optional[str] = None,
                 afc_parallel: Optional[int] = None,
                 regions: Optional[List[str]] = None) -> None:
        """ Constructor

        Arguments:
        max_change_percent -- Optional percent of maximum difference
        afc_url            -- Optional AFC Service URL to test ULS database on
        afc_parallel       -- Optional number of parallel AFC requests to make
                              during database verification
        regions            -- Optional list of regions to test database on. If
                              empty or None - test on all regions
        """
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
        return self._check_diff(db_diff) and self._check_afc(new_filename)

    def _check_diff(self, db_diff: Optional[DbDiff]) -> bool:
        """ Checks amount of difference since previous database """
        if db_diff is None:
            return True
        if not db_diff.valid:
            return False
        if ((db_diff.diff_len == 0) and (db_diff.ras_diff_len)) != \
                (len(db_diff.diff_tiles) == 0):
            logging.error(f"Inconsistent indication of database difference: "
                          f"difference is in {db_diff.diff_len} paths, "
                          f"but in {len(db_diff.diff_tiles)} tiles")
            return False
        if self._max_change_percent is None:
            return True
        diff_percent = \
            round(
                100 if db_diff.new_len == 0 else
                ((db_diff.diff_len * 100) / db_diff.new_len),
                3)
        if diff_percent > self._max_change_percent:
            logging.error(
                f"Database changed by {diff_percent}%, which exceeds "
                f"the limit of {self._max_change_percent}%")
            return False
        return True

    def _check_afc(self, new_filename: str) -> bool:
        """ Checks new database against AFC Service

        Arguments:
        new_filename -- Database being checked. Should have exactly same path
                        as required in AFC Config
        Returns True if test passed
        """
        if self._afc_url is None:
            return True
        logging.info("Testing new FS database on AFC Service")
        args = [FS_AFC, "--server_url", self._afc_url] + \
                (["--parallel", str(self._afc_parallel)]
                 if self._afc_parallel is not None else []) + \
               [f"--region={r}" for r in self._regions] + \
               [new_filename]
        logging.debug(" ".join(shlex.quote(arg) for arg in args))
        try:
            subprocess.run(args, check=True, timeout=30 * 60)
        except (subprocess.SubprocessError, OSError) as ex:
            logging.error(f"AFC Service test failed: {ex}")
            return False
        return True


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
        "--status_dir", metavar="STATUS_DIR",
        help=f"Directory where this (control) script places its status "
        f"information - for use by healthcheck script"
        f"{env_help(Settings, 'status_dir')}")
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

        print_args(settings)

        status_storage = StatusStorage(status_dir=settings.status_dir)
        if status_storage.read_milestone(StatusStorage.S.ServiceBirth) is None:
            status_storage.write_milestone(StatusStorage.S.ServiceBirth)

        error_if(not os.path.isfile(settings.download_script),
                 f"Download script '{settings.download_script}' not found")
        full_ext_db_dir = \
            os.path.join(settings.ext_db_dir,
                         os.path.dirname(settings.ext_db_symlink))
        error_if(
            not os.path.isdir(full_ext_db_dir),
            f"External database directory '{full_ext_db_dir}' not found")

        ext_params_file_checker = \
            ExtParamFilesChecker(
                ext_files_arg=settings.check_ext_files,
                script_dir=os.path.dirname(settings.download_script),
                status_storage=status_storage)

        current_uls_file = os.path.join(settings.ext_db_dir,
                                        settings.ext_db_symlink)

        uls_file_checker = \
            UlsFileChecker(
                max_change_percent=settings.max_change_percent,
                afc_url=settings.afc_url, afc_parallel=settings.afc_parallel,
                regions=None if settings.region is None
                else settings.region.split(":"))

        rcache_settings = \
            RcacheClientSettings(
                enabled=settings.rcache_enabled and bool(settings.rcache_url),
                service_url=settings.rcache_url)
        rcache_settings.validate_for(rcache=True)
        rcache: Optional[RcacheClient] = \
            RcacheClient(rcache_settings) if rcache_settings.enabled else None

        status_storage.write_milestone(StatusStorage.S.ServiceStart)

        if settings.delay_hr and (not settings.run_once):
            logging.info(f"Delaying by {settings.delay_hr} hours")
            time.sleep(settings.delay_hr * 3600)

        if settings.run_once:
            logging.info("Running healthcheck script")
            subprocess.run([sys.executable, HEALTHCHECK_SCRIPT,
                            "--force_success"],
                           timeout=200, check=True)

        # Temporary name of new ULS database in external directory
        temp_uls_file_name: Optional[str] = None

        while True:
            logging.info("Starting ULS download")
            download_start_time = datetime.datetime.now()
            status_storage.write_milestone(StatusStorage.S.DownloadStart)
            try:
                has_previous = os.path.islink(current_uls_file)
                if has_previous:
                    logging.info(
                        f"Current database: '{os.readlink(current_uls_file)}'")

                extract_fsid_table(uls_file=current_uls_file,
                                   fsid_file=settings.fsid_file)

                # Clear some directories from stuff left from previous
                # downloads
                for dir_to_clean in [settings.result_dir, settings.temp_dir]:
                    if dir_to_clean and os.path.isdir(dir_to_clean):
                        subprocess.run(f"rm -rf {dir_to_clean}/*", shell=True,
                                       timeout=100, check=True)

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
                subprocess.run(
                    args=" ".join(cmdline_args)
                    if settings.download_script_args else cmdline_args,
                    check=True, shell=bool(settings.download_script_args),
                    cwd=os.path.dirname(settings.download_script),
                    timeout=settings.timeout_hr * 3600)

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
                status_storage.write_milestone(StatusStorage.S.DownloadSuccess)

                updated_regions = set(new_uls_identity.keys())
                if has_previous:
                    for current_region, current_identity in \
                            (get_uls_identity(current_uls_file) or {}).items():
                        if current_identity and \
                                (new_uls_identity.get(current_region) ==
                                 current_identity):
                            updated_regions.remove(current_region)

                # If anything was updated - do the update routine
                if updated_regions or settings.force:
                    # Embed updated FSID table to the new database
                    logging.info("Embedding FSID table")
                    subprocess.run(
                        [FSID_TOOL, "embed", new_uls_file, settings.fsid_file],
                        check=True)

                    temp_uls_file_name = \
                        os.path.join(full_ext_db_dir,
                                     "temp_" + os.path.basename(new_uls_file))
                    # Copy new ULS file to external directory
                    logging.debug(
                        f"Copying '{new_uls_file}' to '{temp_uls_file_name}'")
                    shutil.copy2(new_uls_file, temp_uls_file_name)

                    db_diff = DbDiff(prev_filename=current_uls_file,
                                     new_filename=temp_uls_file_name) \
                        if has_previous else None
                    if settings.force or \
                            uls_file_checker.valid(
                                base_dir=settings.ext_db_dir,
                                new_filename=os.path.join(
                                    os.path.dirname(settings.ext_db_symlink),
                                    os.path.basename(temp_uls_file_name)),
                                db_diff=db_diff):
                        # Renaming database
                        permanent_uls_file_name = \
                            os.path.join(full_ext_db_dir,
                                         os.path.basename(new_uls_file))
                        os.rename(temp_uls_file_name, permanent_uls_file_name)
                        # Retargeting symlink
                        update_uls_file(
                            uls_dir=full_ext_db_dir,
                            uls_file=os.path.basename(new_uls_file),
                            symlink=os.path.basename(settings.ext_db_symlink))
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
                        update_region_change_times(
                            status_storage=status_storage,
                            all_regions=list(new_uls_identity.keys()),
                            updated_regions=updated_regions)

                        status_storage.write_milestone(
                            StatusStorage.S.SqliteUpdate)
                else:
                    logging.info("FS data is identical to previous. No update "
                                 "will be done")
            except (OSError, subprocess.SubprocessError, ProcessingException) \
                    as ex:
                logging.error(f"Download failed: {ex}")
            finally:
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
