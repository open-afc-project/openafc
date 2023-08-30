#!/usr/bin/env python3
# ULS data download control service

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

# Default value for --download_script
DEFAULT_DOWNLOAD_SCRIPT = \
    "/mnt/nfs/rat_transfer/daily_uls_parse/daily_uls_parse.py"

# Default value for --result_dir
DEFAULT_RESULT_DIR = "/mnt/nfs/rat_transfer/ULS_Database/"

# Default value for --interval_hr
DEFAULT_INTERVAL_HR = 4.

# Default value for --timeout_hr
DEFAULT_TIMEOUT_HR = 1.

# Default value for --ext_db_dir
DEFAULT_EXTERNAL_DB_DIR = "/ULS_Database"

# Default value for --ext_db_symlink
DEFAULT_CURRENT_DB_SYMLINK = "FS_LATEST.sqlite3"

# Default value for --fsid_file
DEFAULT_FSID_INTERNAL_FILE = \
    "/mnt/nfs/rat_transfer/daily_uls_parse/data_files/fsid_table.csv"

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


class ProcessingException(Exception):
    """ ULS processing exception """
    pass


def print_args(args: Any) -> None:
    """ Print invocation parameters to log """
    logging.info("ULS downloader started with the following parameters")
    logging.info(f"ULS download script: {args.download_script}")
    logging.info(f"Download regions: {args.region if args.region else 'All'}")
    logging.info(f"Additional ULS download script arguments: "
                 f"{args.download_script_args}")
    logging.info(f"Directory where ULS download script puts downloaded file: "
                 f"{args.result_dir}")
    logging.info(f"Temporary directory of ULS download script, cleaned before "
                 f"downloading: {args.temp_dir}")
    logging.info(f"Ultimate downloaded file destination directory: "
                 f"{args.ext_db_dir}")
    logging.info(f"Symlink pointing to current ULS file: "
                 f"{args.ext_db_symlink}")
    logging.info(f"FSID file location expected by ULS download script: "
                 f"{args.fsid_file}")
    logging.info(f"ULS download service state storage directory: "
                 f"{args.status_dir}")
    mcp = "Unspecified" if args.max_change_percent is None \
        else f"{args.max_change_percent}%"
    logging.info(f"Limit on paths changed since previous: {mcp}")
    logging.info(f"AFC Service URL to use for database validity check: "
                 f"{'Unspecified' if args.afc_url is None else args.afc_url}")
    logging.info(f"ULS download interval in hours: {args.interval_hr}")
    logging.info(f"ULS download script maximum duration in hours: "
                 f"{args.timeout_hr}")
    logging.info(f"Rcache spatial invalidation: "
                 f"{'enabled' if args.rcache_enabled else 'disabled'}")
    logging.info(f"Rcache URL: {args.rcache_url}")
    logging.info(f"Run: {'Once' if args.run_once else 'Indefinitely'}")
    logging.info(f"Print debug info: {'Yes' if args.verbose else 'No'}")


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
    has_previous -- True if previous database exists
    prev_len     -- Number of paths in previous database
    new_len      -- Number of paths in new database
    diff_len     -- Number of different paths
    diff_tiles   -- Tiles containing receivers of different paths
    """
    def __init__(self, prev_filename: Optional[str], new_filename: str) \
            -> None:
        self.has_previous = (prev_filename is not None) and \
            os.path.isfile(prev_filename)
        self.valid = False
        self.prev_len = 0
        self.new_len = 0
        self.diff_len = 0
        self.diff_tiles: List[LatLonRect] = []
        if not self.has_previous:
            return
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
                      r"Different paths:\s+(?P<diff>\d+)(.|\n)+",
                      cast(str, p.stdout))
        if m is None:
            logging.error(
                f"Output of '{FS_DB_DIFF}' has unrecognized structure")
            return
        self.prev_len = int(cast(str, m.group("db2")))
        self.new_len = int(cast(str, m.group("db2")))
        self.diff_len = int(cast(str, m.group("diff")))
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

    def valid(self, new_filename: str, db_diff: DbDiff) -> bool:
        """ Checks validity of given database """
        return self._check_diff(db_diff) and self._check_afc(new_filename)

    def _check_diff(self, db_diff: DbDiff) -> bool:
        """ Checks amount of difference since previous database """
        if not db_diff.has_previous:
            return True
        if not db_diff.valid:
            return False
        if (db_diff.diff_len == 0) != (len(db_diff.diff_tiles) == 0):
            logging.error(f"Inconsistent indication of database difference: "
                          f"difference is in {db_diff.diff_len} paths, "
                          f"but in {len(db_diff.diff_tiles)} tiles")
            return False
        if self._max_change_percent is None:
            return True
        diff_percent = \
            round(
                100 if db_diff.diff_len == 0 else
                ((db_diff.diff_len * 100) / db_diff.new_len),
                3)
        if diff_percent > self._max_change_percent:
            logging.error(
                f"Database changed by {diff_percent}%, which exceeds "
                f"the limit of {self._max_change_percent}%")
            return False
        return True

    def _check_afc(self, new_filename: str) -> bool:
        """ Checks new database against AFC Service """
        if self._afc_url is None:
            return True
        logging.info("Testing new FS database on AFC Service")
        try:
            subprocess.run(
                [FS_AFC, "--server_url", self._afc_url] +
                (["--parallel", str(self._afc_parallel)]
                 if self._afc_parallel is not None else []) +
                [f"--region={r}" for r in self._regions] +
                [os.path.basename(new_filename)], check=True, timeout=30 * 60)
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
        default=DEFAULT_DOWNLOAD_SCRIPT,
        type=docker_arg_type(str, default=DEFAULT_DOWNLOAD_SCRIPT),
        help=f"ULS download script. Default is '{DEFAULT_DOWNLOAD_SCRIPT}'")
    argument_parser.add_argument(
        "--download_script_args", metavar="ARGS", type=docker_arg_type(str),
        help="Optional additional arguments to pass to ULS download script")
    argument_parser.add_argument(
        "--region", metavar="REG1[:REG2[:REG3...]]", type=docker_arg_type(str),
        help="Colon-separated list of regions to download. Default is all "
        "regions ('US:CA' as of time of this writing")
    argument_parser.add_argument(
        "--result_dir", metavar="RESULT_DIR", default=DEFAULT_RESULT_DIR,
        type=docker_arg_type(str, default=DEFAULT_RESULT_DIR),
        help=f"Directory where ULS download script puts resulting database. "
        f"Default is '{DEFAULT_RESULT_DIR}'")
    argument_parser.add_argument(
        "--temp_dir", metavar="TEMP_DIR",
        type=docker_arg_type(str),
        help="Directory containing downloader's temporary files (cleared "
        "before downloading)")
    argument_parser.add_argument(
        "--ext_db_dir", metavar="EXTERNAL_DATABASE_DIR",
        default=DEFAULT_EXTERNAL_DB_DIR,
        type=docker_arg_type(str, default=DEFAULT_EXTERNAL_DB_DIR),
        help=f"Directory where new ULS databases should be copied. Default is "
        f"'{DEFAULT_EXTERNAL_DB_DIR}'")
    argument_parser.add_argument(
        "--ext_db_symlink", metavar="CURRENT_DATABASE_SYMLINK",
        default=DEFAULT_CURRENT_DB_SYMLINK,
        type=docker_arg_type(str, default=DEFAULT_CURRENT_DB_SYMLINK),
        help=f"Symlink in database directory that points to current database. "
        f"Default is '{DEFAULT_CURRENT_DB_SYMLINK}'")
    argument_parser.add_argument(
        "--fsid_file", metavar="FSID_FILE",
        default=DEFAULT_FSID_INTERNAL_FILE,
        type=docker_arg_type(str, default=DEFAULT_FSID_INTERNAL_FILE),
        help=f"FSID file where ULS downloader is expected to read/update it. "
        f"Default is '{DEFAULT_FSID_INTERNAL_FILE}'")
    argument_parser.add_argument(
        "--status_dir", metavar="STATUS_DIR", default=DEFAULT_STATUS_DIR,
        type=docker_arg_type(str, default=DEFAULT_STATUS_DIR),
        help=f"Directory where this (control) script places its status "
        f"information - for use by healthcheck script. Default is "
        f"'{DEFAULT_STATUS_DIR}'")
    argument_parser.add_argument(
        "--check_ext_files", metavar="BASE_URL:SUBDIR:FILENAME[,FILENAME...]",
        action="append", default=[],
        help="Verify that given files at given location match files at given "
        "subdirectory of ULS downloader script. This parameter may be "
        "specified several times")
    argument_parser.add_argument(
        "--max_change_percent", metavar="MAX_CHANGE_PERCENT",
        type=docker_arg_type(float),
        help="Maximum allowed change since previous database in percents")
    argument_parser.add_argument(
        "--afc_url", metavar="URL", type=docker_arg_type(str),
        help="URL for making trial AFC Requests with new database")
    argument_parser.add_argument(
        "--afc_parallel", metavar="NUMBER", type=docker_arg_type(int),
        help=f"Number of parallel AFC Request to make during verifying new "
        f"database against AFC Engine. Default see in config file of "
        f"'{FS_AFC}'")
    argument_parser.add_argument(
        "--rcache_url", metavar="URL", type=docker_arg_type(str),
        help="URL for spatial invalidation")
    argument_parser.add_argument(
        "--rcache_enabled", metavar="TRUE/FALSE",
        type=docker_arg_type(bool, default=True), default=True,
        help="FALSE to disable spatial invalidation (even if URL specified). "
        "Default is TRUE")
    argument_parser.add_argument(
        "--delay_hr", metavar="DELAY_HR", type=docker_arg_type(float),
        help="Delay before invocation in hours. Default is no delay")
    argument_parser.add_argument(
        "--interval_hr", metavar="INTERVAL_HR", default=DEFAULT_INTERVAL_HR,
        type=docker_arg_type(float, default=DEFAULT_INTERVAL_HR),
        help=f"Download interval in hours. Default is {DEFAULT_INTERVAL_HR}")
    argument_parser.add_argument(
        "--timeout_hr", metavar="TIMEOUT_HR", default=DEFAULT_TIMEOUT_HR,
        type=docker_arg_type(float, default=DEFAULT_TIMEOUT_HR),
        help=f"Download script execution timeout in hours. Default is "
        f"{DEFAULT_INTERVAL_HR}")
    argument_parser.add_argument(
        "--nice", nargs="?", metavar="[YES/NO]", const=True,
        type=docker_arg_type(bool, default=False),
        help="Run download script on nice (low) priority")
    argument_parser.add_argument(
        "--run_once", nargs="?", metavar="[YES/NO]", const=True,
        type=docker_arg_type(bool, default=False),
        help="Run download once and exit")
    argument_parser.add_argument(
        "--verbose", nargs="?", metavar="[YES/NO]", const=True,
        type=docker_arg_type(bool, default=False),
        help="Print detailed log information")

    args = argument_parser.parse_args(argv)

    setup_logging(verbose=args.verbose)

    print_args(args)

    status_storage = StatusStorage(status_dir=args.status_dir)
    if status_storage.read_milestone(StatusStorage.S.ServiceBirth) is None:
        status_storage.write_milestone(StatusStorage.S.ServiceBirth)

    error_if(not os.path.isfile(args.download_script),
             f"Download script '{args.download_script}' not found")
    error_if(not os.path.isdir(args.ext_db_dir),
             f"External database directory '{args.ext_db_dir}' not found")

    ext_params_file_checker = \
        ExtParamFilesChecker(
            ext_files_arg=args.check_ext_files,
            script_dir=os.path.dirname(args.download_script),
            status_storage=status_storage)

    current_uls_file = os.path.join(args.ext_db_dir, args.ext_db_symlink)

    uls_file_checker = \
        UlsFileChecker(
            max_change_percent=args.max_change_percent, afc_url=args.afc_url,
            afc_parallel=args.afc_parallel,
            regions=None if args.region is None else args.region.split(":"))

    rcache_settings = \
        RcacheClientSettings(
            enabled=args.rcache_enabled and bool(args.rcache_url),
            service_url=args.rcache_url)
    rcache_settings.validate_for(rcache=True)
    rcache: Optional[RcacheClient] = \
        RcacheClient(rcache_settings) if rcache_settings.enabled else None

    status_storage.write_milestone(StatusStorage.S.ServiceStart)

    if args.delay_hr:
        logging.info(f"Delaying by {args.delay_hr} hours")
        time.sleep(args.delay_hr * 3600)

    # Temporary name of new ULS database in external directory
    temp_uls_file_name: Optional[str] = None

    while True:
        logging.info("Starting ULS download")
        download_start_time = datetime.datetime.now()
        status_storage.write_milestone(StatusStorage.S.DownloadStart)
        try:
            if os.path.islink(current_uls_file):
                logging.info(
                    f"Current database: '{os.readlink(current_uls_file)}'")

            extract_fsid_table(uls_file=current_uls_file,
                               fsid_file=args.fsid_file)

            # Clear some directories from stuff left from previous downloads
            for dir_to_clean in [args.result_dir, args.temp_dir]:
                if dir_to_clean and os.path.isdir(dir_to_clean):
                    subprocess.run(f"rm -rf {dir_to_clean}/*", shell=True,
                                   timeout=100, check=True)

            logging.info("Checking if external parameter files changed")
            ext_params_file_checker.check()

            # Issue download script
            cmdline_args: List[str] = []
            if args.nice and (os.name == "posix"):
                cmdline_args.append("nice")
            cmdline_args.append(args.download_script)
            if args.region:
                cmdline_args += ["--region", args.region]
            if args.download_script_args:
                cmdline_args.append(args.download_script_args)
            logging.info(f"Starting {' '.join(cmdline_args)}")
            subprocess.run(
                args=" ".join(cmdline_args) if args.download_script_args
                else cmdline_args,
                check=True, shell=bool(args.download_script_args),
                cwd=os.path.dirname(args.download_script),
                timeout=args.timeout_hr * 3600)

            # Find out name of new ULS file
            uls_files = glob.glob(os.path.join(args.result_dir, ULS_FILEMASK))
            if len(uls_files) < 1:
                raise ProcessingException(
                    "ULS file not generated by ULS downloader")
            if len(uls_files) > 1:
                raise ProcessingException(
                    f"More than one {ULS_FILEMASK} file generated by ULS "
                    f"downloader. What gives?")

            # Check what regions were updated
            new_uls_file = uls_files[0]
            logging.info(f"ULS file '{new_uls_file}' created. It will undergo "
                         f"some inspection")
            new_uls_identity = get_uls_identity(new_uls_file)
            if new_uls_identity is None:
                raise ProcessingException(
                    "Generated ULS file does not contain identity information")
            status_storage.write_milestone(StatusStorage.S.DownloadSuccess)

            updated_regions = set(new_uls_identity.keys())
            if os.path.isfile(current_uls_file):
                for current_region, current_identity in \
                        (get_uls_identity(current_uls_file) or {}).items():
                    if current_identity and \
                            (new_uls_identity.get(current_region) ==
                             current_identity):
                        updated_regions.remove(current_region)

            # If anything was updated - do the update routine
            if updated_regions:
                # Embed updated FSID table to the new database
                logging.info("Embedding FSID table")
                subprocess.run(
                    [FSID_TOOL, "embed", new_uls_file, args.fsid_file],
                    check=True)

                temp_uls_file_name = \
                    os.path.join(args.ext_db_dir,
                                 "temp_" + os.path.basename(new_uls_file))
                # Copy new ULS file to external directory
                shutil.copy2(new_uls_file, temp_uls_file_name)

                db_diff = DbDiff(prev_filename=current_uls_file,
                                 new_filename=temp_uls_file_name)
                if uls_file_checker.valid(new_filename=temp_uls_file_name,
                                          db_diff=db_diff):
                    # Renaming database
                    permanent_uls_file_name = \
                        os.path.join(args.ext_db_dir,
                                     os.path.basename(new_uls_file))
                    os.rename(temp_uls_file_name, permanent_uls_file_name)
                    # Retargeting symlink
                    update_uls_file(uls_dir=args.ext_db_dir,
                                    uls_file=os.path.basename(new_uls_file),
                                    symlink=args.ext_db_symlink)
                    if rcache:
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
                if temp_uls_file_name and os.path.isfile(temp_uls_file_name):
                    os.unlink(temp_uls_file_name)
            except OSError as ex:
                logging.error(f"Attempt to remove temporary ULS database "
                              f"'{temp_uls_file_name}' failed: {ex}")

        # Prepare to sleep
        download_duration_sec = \
            (datetime.datetime.now() - download_start_time).total_seconds()
        logging.info(f"Download took {download_duration_sec // 60} minutes")
        if args.run_once:
            break
        remaining_seconds = \
            max(0, args.interval_hr * 3600 - download_duration_sec)
        if remaining_seconds:
            next_attempt_at = \
                (datetime.datetime.now() +
                 datetime.timedelta(seconds=remaining_seconds)).isoformat()
            logging.info(f"Next download at {next_attempt_at}")
            time.sleep(remaining_seconds)


if __name__ == "__main__":
    main(sys.argv[1:])
