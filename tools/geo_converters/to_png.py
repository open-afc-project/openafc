#!/usr/bin/env python3
# Converts whatever geospatial file to PNG (wrapper around gdal_translate)

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=wildcard-import, unused-wildcard-import, invalid-name
# pylint: disable=too-many-statements, too-many-branches, too-many-arguments
# pylint: disable=too-many-locals

import argparse
from collections.abc import Sequence
import datetime
import enum
import glob
import multiprocessing.pool
import os
import shlex
import signal
import sys
from typing import List, NamedTuple, Optional, Set

from geoutils import *

# Default file extension
DEFAULT_EXT = ".png"

_EPILOG = """This script expects that GDAL utilities are installed and in PATH.
Also it expects that as part of this installation proj/proj.db is somehow
installed as well (if it is, but gdalwarp still complains - set PROJ_LIB
environment variable to point to this proj directory).

Some examples:
- Convert 3DEP files in 3dep/1_arcsec to png in 3DEP_PNG directory, using 8
  CPUs:
   $ tp_png.py --threads 8 --out_dir 3DEP_PNG 3dep/1_arcsec/*.tif
"""

DataTypeResult = NamedTuple("DataTypeResult",
                            [("filename", str),
                             ("data_types", Optional[Sequence[str]])])


def data_type_worker(filename: str) -> DataTypeResult:
    """ Data type worker. Returns per-band sequence of used data types """
    try:
        return DataTypeResult(filename=filename,
                              data_types=GdalInfo(filename).data_types)
    except (Exception, KeyboardInterrupt, SystemExit) as ex:
        return DataTypeResult(filename=filename, data_types=None)


# Conversion result status
ConvStatus = enum.Enum("ConvStatus", ["Success", "Exists", "Error"])


class ConvResult(NamedTuple):
    """Conversion result """

    # Name of converted file
    filename: str

    # Conversion status
    status: ConvStatus

    # Conversion duration
    duration: datetime.timedelta

    # Optional error message
    msg: Optional[str] = None

    # Optional command line
    command_line: Optional[str] = None


def conversion_worker(
        src: str, dst: str, resampling: str, top: Optional[float],
        bottom: Optional[float], left: Optional[float], right: Optional[float],
        pixel_size: Optional[float], pixels_per_degree: Optional[float],
        scale: Optional[List[float]], no_data: Optional[str],
        data_type: Optional[str], wld: bool, round_boundaries_to_degree: bool,
        round_pixels_to_degree: bool, format_params: List[str],
        overwrite: bool, quiet: bool) -> ConvResult:
    """ Worker function performing the conversion

    Arguments:
    src                        -- Source file name
    dst                        -- Destination file name
    resampling                 -- Resampling method
    top                        -- Optional top boundary of resulting file
    bottom                     -- Optional bottom boundary of resulting file
    left                       -- Optional left boundary of resulting file
    right                      -- Optional right boundary of resulting file
    pixel_size                 -- Optional pixel size in degrees
    pixels_per_degree          -- Optional number of pixels per degree
    scale                      -- Optional scale as [src_min,src_max,dst_min,
                                  dst_max]
    no_data                    -- Optional NoData value
    data_type                  -- Optional pixel data type
    wld                        -- Generate .wld file (containing translation
                                  parameters)
    round_boundaries_to_degree -- True to round not explicitly specified
                                  boundaries outward to next degree
    round_pixels_to_degree     -- True to round pixel sizes to whole number per
                                  degree
    format_params              -- Output format options
    overwrite                  -- True to overwrite file if it exists
    quiet                      -- True to not print anything, returning message
                                  instead
    Returns ConvResult object
    """
    stem, ext = os.path.splitext(dst)
    dst_wld = stem + ".wld"
    temp_file = stem + ".incomplete" + ext
    temp_file_wld = stem + ".incomplete.wld"
    temp_file_xml = temp_file + ".aux.xml"
    start_time = datetime.datetime.now()
    try:
        if os.path.isfile(dst) and (not overwrite):
            return ConvResult(filename=src, status=ConvStatus.Exists,
                              duration=datetime.datetime.now() - start_time)
        boundaries = \
            Boundaries(
                src, pixel_size=pixel_size,
                pixels_per_degree=pixels_per_degree, top=top, bottom=bottom,
                left=left, right=right,
                round_boundaries_to_degree=round_boundaries_to_degree,
                round_pixels_to_degree=round_pixels_to_degree)
        gi = GdalInfo(src, fail_on_error=False)
        if not (gi and boundaries):
            return ConvResult(filename=src, status=ConvStatus.Error,
                              duration=datetime.datetime.now() - start_time,
                              msg="gdalinfo inspection failed")
        warn: Optional[str] = None
        if (not scale) and \
                any(dt in ("Float32", "Float64") for dt in gi.data_types):
            warn = "Float source data converted to integer data without " + \
                "scaling. Loss of precision may ensue"

        trans_args = \
            ["gdal_translate", "-strict", "-r", resampling, "-of", PNG_FORMAT]
        for fp in format_params:
            trans_args += ["-co", fp]
        if wld:
            trans_args += ["-co", "WORLDFILE=YES"]
        if boundaries.edges_overridden:
            trans_args += \
                ["-projwin", str(boundaries.left), str(boundaries.top),
                 str(boundaries.right), str(boundaries.bottom)]
        if scale:
            trans_args += ["-scale"] + [str(s) for s in scale]
        if no_data:
            trans_args += ["-a_nodata", no_data]
        if data_type:
            trans_args += ["-ot", data_type]
        trans_args += [src, temp_file]
        command_line = " ".join(shlex.quote(arg) for arg in trans_args)
        exec_result = execute(trans_args, env=gdal_env(),
                              disable_output=quiet, return_error=quiet,
                              fail_on_error=not quiet)
        if quiet:
            if exec_result is not None:
                assert isinstance(exec_result, str)
                return ConvResult(
                    filename=src,
                    status=ConvStatus.Error,
                    duration=datetime.datetime.now() -
                    start_time,
                    msg=exec_result,
                    command_line=command_line)
        else:
            assert isinstance(exec_result, bool) and exec_result
        if os.path.isfile(dst):
            os.unlink(dst)
        if os.path.isfile(dst_wld):
            os.unlink(dst_wld)
        os.rename(temp_file, dst)
        if wld:
            os.rename(temp_file_wld, dst_wld)
        return ConvResult(filename=src, status=ConvStatus.Success,
                          duration=datetime.datetime.now() - start_time,
                          msg=warn)
    except (Exception, KeyboardInterrupt, SystemExit) as ex:
        return ConvResult(filename=src, status=ConvStatus.Error,
                          duration=datetime.datetime.now() - start_time,
                          msg=repr(ex))
    finally:
        for filename in (temp_file_xml, temp_file_wld, temp_file):
            try:
                if os.path.isfile(filename):
                    os.unlink(filename)
            except OSError:
                pass


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = argparse.ArgumentParser(
        description="Converts geospatial file to PNG (wrapper around "
        "gdal_translate)",
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=_EPILOG)
    argument_parser.add_argument(
        "--pixel_size", metavar="DEGREES", type=float,
        help="Resulting file resolution in pixel size (expressed in degrees). "
        "Default is to use one from 'gdalinfo'")
    argument_parser.add_argument(
        "--pixels_per_degree", metavar="NUMBER", type=float,
        help="Resulting file resolution in pixels per degree. Default is to "
        "use one from 'gdalinfo'")
    argument_parser.add_argument(
        "--top", metavar="MAX_LATITUDE", type=float,
        help="Maximum latitude. Default is to use one from 'gdalinfo'")
    argument_parser.add_argument(
        "--bottom", metavar="MIN_LATITUDE", type=float,
        help="Minimum latitude. Default is to use one from 'gdalinfo'")
    argument_parser.add_argument(
        "--left", metavar="MIN_LONGITUDE", type=float,
        help="Minimum longitude. Default is to use one from 'gdalinfo'")
    argument_parser.add_argument(
        "--right", metavar="MAX_LONGITUDE", type=float,
        help="Maximum longitude. Default is to use one from 'gdalinfo'")
    argument_parser.add_argument(
        "--round_boundaries_to_degree", action="store_true",
        help="Round boundaries (that were not explicitly specified) to next "
        "degree in outward direction. Default is to keep boundaries")
    argument_parser.add_argument(
        "--round_pixels_to_degree", action="store_true",
        help="Round pixel sizes to whole number of pixels per degree")
    argument_parser.add_argument(
        "--scale", metavar="V", nargs=4, type=float,
        help=f"Scale/offset output data. Parameter order is: SRC_MIN SRC_MAX "
        f"DST_MIN DST_MAX where [SRC_MIN, SRC_MAX] interval "
        f"maps to [DST_MIN, DST_MAX]. For Int16/Int32 sources default is "
        f"'{' '.join(str(v) for v in DEFAULT_INT_PNG_SCALE)}', for "
        f"Float32/Float64 sources default is "
        f"'{' '.join(str(v) for v in DEFAULT_FLOAT_PNG_SCALE)}")
    argument_parser.add_argument(
        "--no_data", metavar="NODATA",
        help=f"No Data value for target files. If target is UInt16 PNG and "
        f"source is not Byte/UInt16, default is {DEFAULT_PNG_NO_DATA}")
    argument_parser.add_argument(
        "--data_type", metavar="DATA_TYPE", choices=["Byte", "UInt16"],
        help="Pixel data type for output file. Possible values: Byte, UInt16")
    argument_parser.add_argument(
        "--wld", action="store_true",
        help="Generate .wld file, containing translation parameters")
    argument_parser.add_argument(
        "--resampling", metavar="METHOD",
        choices=warp_resamplings(),
        help="Resampling method. See "
        "https://gdal.org/programs/gdalwarp.html#cmdoption-gdalwarp-r for "
        "explanations. Default is `near' for byte data, 'cubic' for other "
        "data")
    argument_parser.add_argument(
        "--format_param", metavar="NAME=VALUE", action="append",
        default=[],
        help="Format option (e.g. COMPRESS=PACKBITS or BIGTIFF=IF_NEEDED)")
    argument_parser.add_argument(
        "--out_dir", metavar="OUTPUT_DIRECTORY",
        help="If present - mass conversion is performed and this is the "
        "target directory")
    argument_parser.add_argument(
        "--out_ext", metavar=".EXT", default=DEFAULT_EXT,
        help=f"Extension for output files to use in case of mass conversion. "
        f"Default is '{DEFAULT_EXT}'")
    argument_parser.add_argument(
        "--overwrite", action="store_true",
        help="Overwrite target file if exists. By default if mass conversion "
        "is performed (--out_dir provided), already existing files are "
        "skipped (making possible a restartable conversion), otherwise "
        "already existing file reported as an error")
    argument_parser.add_argument(
        "--threads", metavar="COUNT_OR_PERCENT%",
        help="Number of threads to use. If positive - number of threads, if "
        "negative - number of CPU cores NOT to use, if followed by `%%` - "
        "percent of CPU cores. Default is total number of CPU cores")
    argument_parser.add_argument(
        "--nice", action="store_true",
        help="Lower priority of this process and its subprocesses")
    argument_parser.add_argument(
        "FILES", nargs="+",
        help="In case of mass conversion (--out_dir is provided) - source "
        "file names. Otherwise it should be 'SRC DST' pair")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)
    args = argument_parser.parse_args(argv)

    setup_logging()

    if args.nice:
        nice()

    start_time = datetime.datetime.now()

    sources: List[str] = []
    if args.out_dir is None:
        error_if(len(args.FILES) < 2,
                 "Destination file name not provided")
        error_if(len(args.FILES) > 2,
                 "Exactly two file names (source and destination) should be "
                 "provided")
        sources.append(args.FILES[0])
    else:
        for files_arg in args.FILES:
            files = glob.glob(files_arg)
            error_if(not files,
                     f"No source files matching '{files_arg}' found")
            sources += list(files)
    source_data_types: Set[str] = set()

    def data_type_completer(dtr: DataTypeResult) -> None:
        """ Callback processing results of data_type_worker """
        error_if(dtr.data_types is None,
                 f"'{dtr.filename}' failed gdalinfo inspection")
        source_data_types.update(dtr.data_types)

    try:
        original_sigint_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
        with multiprocessing.pool.ThreadPool(
                processes=threads_arg(args.threads)) as pool:
            signal.signal(signal.SIGINT, original_sigint_handler)
            for filename in sources:
                pool.apply_async(data_type_worker, kwds={"filename": filename},
                                 callback=data_type_completer)
            pool.close()
            pool.join()
    except KeyboardInterrupt:
        sys.exit(1)

    data_type = args.data_type
    if data_type is None:
        if any(dt.startswith("Float") for dt in source_data_types):
            data_type = "UInt16"
        else:
            error_if(not all(dt == "Byte" for dt in source_data_types),
                     "Data type must be specified explicitly with --data_type")

    scale = get_scale(arg_scale=args.scale, src_data_types=source_data_types,
                      dst_format=PNG_FORMAT, dst_data_type=data_type,
                      dst_ext=None)
    no_data = get_no_data(arg_no_data=args.no_data,
                          src_data_types=source_data_types,
                          dst_format=PNG_FORMAT, dst_data_type=data_type,
                          dst_ext=None)
    resampling = get_resampling(arg_resampling=args.resampling,
                                src_data_types=source_data_types,
                                dst_data_type=data_type)

    common_kwargs = {
        "resampling": resampling,
        "top": args.top,
        "bottom": args.bottom,
        "left": args.left,
        "right": args.right,
        "pixel_size": args.pixel_size,
        "pixels_per_degree": args.pixels_per_degree,
        "scale": scale,
        "no_data": no_data,
        "data_type": data_type,
        "wld": args.wld,
        "round_boundaries_to_degree": args.round_boundaries_to_degree,
        "round_pixels_to_degree": args.round_pixels_to_degree,
        "format_params": args.format_param,
        "overwrite": args.overwrite}

    if args.out_dir is None:
        cr = conversion_worker(src=args.FILES[0], dst=args.FILES[1],
                               quiet=False, **common_kwargs)
        if cr.status == ConvStatus.Error:
            assert cr.msg is not None
            error(cr.msg)
        error_if(cr.status == ConvStatus.Exists,
                 f"File '{args.FILES[1]}' already exists. Specify --overwrite "
                 f"to overwrite it")
        assert cr.status == ConvStatus.Success
    else:
        if not os.path.isdir(args.out_dir):
            os.makedirs(args.out_dir)
        total_count = len(sources)
        completed_count = [0]  # Made list to facilitate closure in completer()
        skipped_files: List[str] = []
        failed_files: List[str] = []

        def completer(cr: ConvResult) -> None:
            """ Processes completion of a single file """
            completed_count[0] += 1
            msg = f"{completed_count[0]} of {total_count} " \
                f"({completed_count[0] * 100 // total_count}%) {cr.filename}: "
            if cr.status == ConvStatus.Exists:
                msg += "Destination file exists. Skipped"
                skipped_files.append(cr.filename)
            elif cr.status == ConvStatus.Error:
                failed_files.append(cr.filename)
                msg += f"Conversion failed: {cr.msg}"
            else:
                assert cr.status == ConvStatus.Success
                msg += f"Converted in {Durator.duration_to_hms(cr.duration)}"
                if cr.msg:
                    msg += "\n" + cr.msg
            print(msg)

        try:
            original_sigint_handler = signal.signal(signal.SIGINT,
                                                    signal.SIG_IGN)
            with multiprocessing.pool.ThreadPool(
                    processes=threads_arg(args.threads)) as pool:
                signal.signal(signal.SIGINT, original_sigint_handler)
                for filename in sources:
                    kwargs = common_kwargs.copy()
                    kwargs["src"] = filename
                    kwargs["dst"] = \
                        os.path.join(args.out_dir, os.path.basename(filename))
                    if args.out_ext is not None:
                        kwargs["dst"] = \
                            os.path.splitext(kwargs["dst"])[0] + args.out_ext
                    kwargs["quiet"] = True
                    pool.apply_async(conversion_worker, kwds=kwargs,
                                     callback=completer)
                pool.close()
                pool.join()
            if skipped_files:
                print(
                    f"{len(skipped_files)} previously existing files skipped")
            if failed_files:
                print("Following files were not converted due to errors:")
                for filename in sorted(failed_files):
                    print(f"  {filename}")
        except KeyboardInterrupt:
            sys.exit(1)

    implicit_conversion_params: List[str] = []
    if scale != args.scale:
        implicit_conversion_params.append(f"scale of {scale}")
    if no_data != args.no_data:
        implicit_conversion_params.append(f"NoData of {no_data}")
    if resampling != args.resampling:
        implicit_conversion_params.append(f"resampling of {resampling}")
    if data_type != args.data_type:
        implicit_conversion_params.append(f"data type of {data_type}")
    if implicit_conversion_params:
        print(f"Implicitly chosen conversion parameters: "
              f"{', '.join(implicit_conversion_params)}")

    print(
        f"Total duration: "
        f"{Durator.duration_to_hms(datetime.datetime.now() - start_time)}")


if __name__ == "__main__":
    main(sys.argv[1:])
