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
from typing import Dict, List, Optional, Set

from uls_service_common import *

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
    logging.info(f"ULS download interval in hours: {args.interval_hr}")
    logging.info(f"ULS download script maximum duration in hours: "
                 f"{args.timeout_hr}")
    logging.info(f"Run: {'Once' if args.run_once else 'Indefinitely'}")
    logging.info(f"Print debug info: {'Yes' if args.verbose else 'No'}")


def extract_fsid_table(uls_file: str, fsid_file: str) -> None:
    """ Try to extract FSID table from ULS database:

    Arguments:
    uls_file  -- FS database filename
    fsid_file -- FSID table filename
    """
    # Clean previous fsid files...
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
        "--interval_hr", metavar="INTERVAL_HR", default=DEFAULT_INTERVAL_HR,
        type=docker_arg_type(float, default=DEFAULT_INTERVAL_HR),
        help=f"Download interval in hours. Default is {DEFAULT_INTERVAL_HR}")
    argument_parser.add_argument(
        "--timeout_hr", metavar="TIMEOUT_HR", default=DEFAULT_TIMEOUT_HR,
        type=docker_arg_type(float, default=DEFAULT_TIMEOUT_HR),
        help=f"Download script execution timeout in hours. Default is "
        f"{DEFAULT_INTERVAL_HR}")
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

    status_storage.write_milestone(StatusStorage.S.ServiceStart)

    while True:
        logging.info("Starting ULS download")
        download_start_time = datetime.datetime.now()
        status_storage.write_milestone(StatusStorage.S.DownloadStart)
        try:
            current_uls_file = \
                os.path.join(args.ext_db_dir, args.ext_db_symlink)

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
            cmdline_args = [args.download_script]
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

                # Copy new ULS file to external directory
                shutil.copy2(new_uls_file, args.ext_db_dir)

                # Retargeting symlink
                update_uls_file(uls_dir=args.ext_db_dir,
                                uls_file=os.path.basename(new_uls_file),
                                symlink=args.ext_db_symlink)

                # Update data change times (for health checker)
                update_region_change_times(
                    status_storage=status_storage,
                    all_regions=list(new_uls_identity.keys()),
                    updated_regions=updated_regions)

                status_storage.write_milestone(StatusStorage.S.SqliteUpdate)
            else:
                logging.info("FS data is identical to previous. No update "
                             "will be done")
        except (OSError, subprocess.SubprocessError, ProcessingException) \
                as ex:
            logging.error(f"Download failed: {ex}")

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
