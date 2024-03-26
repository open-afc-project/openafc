#!/usr/bin/env python3
# Merges LiDAR files into one-file-per-agglomeration (gdal_merge.py phase)

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=wildcard-import, unused-wildcard-import, too-many-arguments
# pylint: disable=too-many-locals, invalid-name, too-many-return-statements
# pylint: disable=too-many-branches, too-many-statements

import argparse
import csv
import datetime
import enum
import multiprocessing.pool
import os
import shlex
import sys
from typing import List, Optional, Set, Tuple

from geoutils import *

# Pattern for .csv name that corresponds to locality name
CSV_PATTERN = "%s_info.csv"

# Default resampling to use in gdalbuildvrt
DEFAULT_RESAMPLING = "cubic"

# Conversion result status
ConvStatus = enum.Enum("ConvStatus", ["Success", "Exists", "Error"])

_EPILOG = """This script expects that GDAL utilities are installed and in PATH.
Also it expects that as part of this installation proj/proj.db is somehow
installed as well (if it is, but gdal_translate still complains - set PROJ_LIB
environment variable to point to this proj directory).
Some usage examples:

- Merge lidars in proj_lidar_2019, result in MERGED_LIDARS directrory,
  use 8 CPUs on lowered (nice) priority,
  use ZSTD compression in resulted files:
   $ lidar_merge.py --threads 8 --nice --proj_param BIGTIFF=YES \\
     --proj_param COMPRESS=ZSTD proj_lidar_2019 MERGED_LIDARS
  On 8 CPUs this takes ~5 hours.
"""


class ConvResult(NamedTuple):
    """ Conversion result """

    # Name of converted file
    locality: str

    # Conversion status
    status: ConvStatus

    # Conversion duration
    duration: datetime.timedelta

    # Optional error message
    msg: Optional[str] = None

    # gdal_merge command line used
    command_line: Optional[str] = None


def merge_worker(
        src_dir: str, dst_dir: str, locality: str, out_ext: Optional[str],
        out_format: Optional[str], resampling: str, format_params: List[str],
        overwrite: bool, verbose: bool) -> ConvResult:
    """ Merge worker

    Arguments:
    src_dir       -- Source directory
    dst_dir       -- Destination directory
    locality      -- Locality (subdirectory with .tif files)
    out_ext       -- Optional output file extension
    out_format    -- Optional output format (GDAL driver name)
    resampling    -- Resampling for gdalvrt
    format_params -- Optional output format parameters
    overwrite     -- True to overwrite existing files
    verbose       -- Print gdal_merge output directly and verbosely
    Return Conversion result
    """
    temp_filename_vrt = os.path.join(dst_dir, locality + ".vrt")
    temp_filename_xml: Optional[str] = None
    temp_filename: Optional[str] = None
    try:
        start_time = datetime.datetime.now()
        warn_msg: str = ""
        srcs: List[str] = []
        src_extensions: Set[str] = set()
        src_pixel_sizes: Set[Tuple[Optional[float], Optional[float]]] = set()
        csv_file_nmame = os.path.join(src_dir, CSV_PATTERN % locality)
        with open(csv_file_nmame, newline='', encoding="utf-8") as csv_f:
            for row in csv.DictReader(csv_f):
                if "FILE" not in row:
                    return \
                        ConvResult(
                            locality=locality, status=ConvStatus.Error,
                            duration=datetime.datetime.now() - start_time,
                            msg=f"Invalid '{csv_file_nmame}' file structure")
                src_filename = os.path.join(src_dir, locality, row["FILE"])
                if not os.path.isfile(src_filename):
                    return \
                        ConvResult(
                            locality=locality, status=ConvStatus.Error,
                            duration=datetime.datetime.now() - start_time,
                            msg=f"Source file '{src_filename}' of locality "
                            f"'{locality}' not found")
                gi = GdalInfo(src_filename, fail_on_error=False)
                if not gi:
                    return \
                        ConvResult(
                            locality=locality, status=ConvStatus.Error,
                            duration=datetime.datetime.now() - start_time,
                            msg=f"Unable to inspect '{src_filename}' of "
                            f"locality '{locality}' with gdalinfo")
                src_extensions.add(os.path.splitext(src_filename)[1])
                src_pixel_sizes.add((gi.pixel_size_lat, gi.pixel_size_lon))
                srcs.append(src_filename)
        if not srcs:
            return \
                ConvResult(
                    locality=locality, status=ConvStatus.Error,
                    duration=datetime.datetime.now() - start_time,
                    msg=f"Locality '{locality}' have no source files")
        srcs.reverse()
        if len(src_pixel_sizes) != 1:
            if warn_msg:
                warn_msg += "\n"
            warn_msg += f"Locality '{locality}' has different pixel sizes " \
                f"in different source files. Pixel sizes of the last file " \
                f"({srcs[0]}) will be used"
        if out_ext is None:
            if len(src_extensions) != 1:
                return \
                    ConvResult(
                        locality=locality, status=ConvStatus.Error,
                        duration=datetime.datetime.now() - start_time,
                        msg=f"Source files for locality '{locality}' have "
                        f"have different extensions. Extension for output "
                        f"file must be explicitly specified")
            out_ext = list(src_extensions)[0]
        dst_filename = os.path.join(dst_dir, locality) + out_ext
        temp_filename = \
            os.path.join(dst_dir, locality) + ".incomplete" + out_ext
        temp_filename_xml = temp_filename + ".aux.xml"
        if os.path.isfile(dst_filename):
            if not overwrite:
                return ConvResult(
                    locality=locality,
                    status=ConvStatus.Exists,
                    duration=datetime.datetime.now() -
                    start_time)

        # Building VRT for filelist
        vrt_args = ["gdalbuildvrt", "-ignore_srcmaskband"]
        if resampling:
            vrt_args += ["-r", resampling]
        vrt_args.append(temp_filename_vrt)
        vrt_args += srcs
        command_line = " ".join(shlex.quote(arg) for arg in vrt_args)
        exec_result = execute(vrt_args, env=gdal_env(),
                              disable_output=not verbose,
                              return_error=not verbose, fail_on_error=verbose)
        if verbose:
            assert exec_result is True
        elif exec_result is not None:
            assert isinstance(exec_result, str)
            return ConvResult(locality=locality, status=ConvStatus.Error,
                              duration=datetime.datetime.now() - start_time,
                              msg=exec_result, command_line=command_line)

        # Translating VRT to single file
        trans_args = ["gdal_translate", "-strict"]
        if out_format:
            trans_args += ["-of", out_format]
        for fp in format_params:
            trans_args += ["-co", fp]
        trans_args += [temp_filename_vrt, temp_filename]
        command_line = " ".join(shlex.quote(arg) for arg in trans_args)
        exec_result = execute(trans_args, env=gdal_env(),
                              disable_output=not verbose,
                              return_error=not verbose, fail_on_error=verbose)
        if verbose:
            assert exec_result is True
        elif exec_result is not None:
            assert isinstance(exec_result, str)
            return ConvResult(locality=locality, status=ConvStatus.Error,
                              duration=datetime.datetime.now() - start_time,
                              msg=exec_result, command_line=command_line)
        if os.path.isfile(dst_filename):
            if verbose:
                print(f"Removing '{dst_filename}'")
            os.unlink(dst_filename)
        if verbose:
            print(f"Renaming '{temp_filename}' to '{dst_filename}'")
        os.rename(temp_filename, dst_filename)
        return ConvResult(locality=locality, status=ConvStatus.Success,
                          duration=datetime.datetime.now() - start_time,
                          msg=warn_msg, command_line=command_line)
    except (Exception, KeyboardInterrupt, SystemExit) as ex:
        return ConvResult(locality=locality, status=ConvStatus.Error,
                          duration=datetime.datetime.now() - start_time,
                          msg=repr(ex))
    finally:
        for filename in (temp_filename, temp_filename_xml, temp_filename_vrt):
            try:
                if filename and os.path.isfile(filename):
                    os.unlink(filename)
            except OSError:
                pass


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = argparse.ArgumentParser(
        description="Merges LiDAR files into one-file-per-agglomeration",
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=_EPILOG)
    argument_parser.add_argument(
        "--overwrite", action="store_true",
        help="Overwrite existing files. By default they are skipped (to "
        "achieve easy resumption of the process)")
    argument_parser.add_argument(
        "--format", metavar="GDAL_DRIVER_NAME",
        help="File format expressed as GDAL driver name (see "
        "https://gdal.org/drivers/raster/index.html ). By default derived "
        "from target file extension")
    argument_parser.add_argument(
        "--format_param", metavar="NAME=VALUE", action="append",
        default=[],
        help="Format option. May be specified several times")
    argument_parser.add_argument(
        "--threads", metavar="COUNT_OR_PERCENT%",
        help="Number of threads to use. If positive - number of threads, if "
        "negative - number of CPU cores NOT to use, if followed by `%%` - "
        "percent of CPU cores. Default is total number of CPU cores")
    argument_parser.add_argument(
        "--nice", action="store_true",
        help="Lower priority of this process and its subprocesses")
    argument_parser.add_argument(
        "--locality", metavar="LOCALITY_NAME", action="append",
        help="Do the merge only for given locality (name of subdirectory with "
        "LiDAR files, initial part of .csv file name). This parameter may be "
        "specified more than once. Default is to do the merge for all "
        "localities")
    argument_parser.add_argument(
        "--resampling", metavar="METHOD", default=DEFAULT_RESAMPLING,
        choices=warp_resamplings(),
        help=f"Resampling method to use. See "
        f"https://gdal.org/programs/gdalwarp.html#cmdoption-gdalwarp-r for "
        f"explanations. Default is '{DEFAULT_RESAMPLING}'")
    argument_parser.add_argument(
        "--out_ext", metavar=".EXT",
        help="Extension for output files. Default is to keep original "
        "extension")
    argument_parser.add_argument(
        "--verbose", action="store_true",
        help="Process localities sequentially, print verbose gdal_translate "
        "output in real time, fail on first error. For debug purposes")
    argument_parser.add_argument(
        "SRC_DIR",
        help="Source directory - root of all LiDAR files, where .csv files "
        "are")
    argument_parser.add_argument(
        "DST_DIR", help="Target directory")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)
    args = argument_parser.parse_args(argv)

    setup_logging()

    if args.nice:
        nice()

    error_if(not os.path.isdir(args.SRC_DIR),
             f"Source directory '{args.SRC_DIR}' not found")

    start_time = datetime.datetime.now()

    if not os.path.isdir(args.DST_DIR):
        os.makedirs(args.DST_DIR)
    localities: List[str] = []
    if args.locality:
        for locality in args.locality:
            dir_name = os.path.join(args.SRC_DIR, locality)
            csv_name = os.path.join(args.SRC_DIR, CSV_PATTERN % locality)
            error_if(
                not (os.path.isdir(dir_name) and os.path.isfile(csv_name)),
                f"Source data for locality '{locality}' not found in "
                f"'{args.SRC_DIR}'")
            localities.append(locality)
    else:
        for locality in os.listdir(args.SRC_DIR):
            dir_name = os.path.join(args.SRC_DIR, locality)
            csv_name = os.path.join(args.SRC_DIR, CSV_PATTERN % locality)
            if os.path.isdir(dir_name) and os.path.isfile(csv_name):
                localities.append(locality)
        error_if(not localities,
                 f"No LiDAR localities found in '{args.SRC_DIR}'")

    completed_count = [0]  # List to facilitate closure in completer()
    total_count = len(localities)
    skipped_localities: List[str] = []
    failed_localities: List[str] = []

    def completer(cr: ConvResult) -> None:
        """ Processes completion of a single file """
        completed_count[0] += 1
        msg = f"{completed_count[0]} of {total_count} " \
            f"({completed_count[0] * 100 // total_count}%) {cr.locality}: "
        if (not args.verbose) and (cr.command_line is not None):
            msg += "\n" + cr.command_line + "\n"
        if cr.status == ConvStatus.Exists:
            msg += "Destination file exists. Skipped"
            skipped_localities.append(cr.locality)
        elif cr.status == ConvStatus.Error:
            error_if(args.verbose, cr.msg)
            if cr.msg is not None:
                msg += "\n" + cr.msg
            failed_localities.append(cr.locality)
        else:
            assert cr.status == ConvStatus.Success
            if cr.msg is not None:
                msg += "\n" + cr.msg
            msg += f"Converted in {Durator.duration_to_hms(cr.duration)}"
        print(msg)

    common_kwargs = {
        "src_dir": args.SRC_DIR,
        "dst_dir": args.DST_DIR,
        "out_ext": args.out_ext,
        "out_format": args.format,
        "resampling": args.resampling,
        "format_params": args.format_param,
        "overwrite": args.overwrite,
        "verbose": args.verbose}
    try:
        if args.verbose:
            for locality in localities:
                completer(merge_worker(locality=locality, **common_kwargs))
        else:
            original_sigint_handler = \
                signal.signal(signal.SIGINT, signal.SIG_IGN)
            with multiprocessing.pool.ThreadPool(
                    processes=threads_arg(args.threads)) as pool:
                signal.signal(signal.SIGINT, original_sigint_handler)
                for locality in localities:
                    kwds = common_kwargs.copy()
                    kwds["locality"] = locality
                    pool.apply_async(merge_worker, kwds=kwds,
                                     callback=completer)
                pool.close()
                pool.join()
        if skipped_localities:
            print(f"{len(skipped_localities)} previously processed localities "
                  "skipped")
        if failed_localities:
            print("Following localities were not converted due to errors:")
            for locality in sorted(failed_localities):
                print(f"  {locality}")
        print(
            f"Total duration: "
            f"{Durator.duration_to_hms(datetime.datetime.now() - start_time)}")
    except KeyboardInterrupt:
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1:])
