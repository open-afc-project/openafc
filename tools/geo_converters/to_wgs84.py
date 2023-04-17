#!/usr/bin/env python3
# Converts whatever geospatial file to WGS84 (wrapper around gdalwarp)

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=wildcard-import, unused-wildcard-import, invalid-name
# pylint: disable=too-many-statements, too-many-locals, too-many-arguments
# pylint: disable=too-many-branches

import argparse
from collections.abc import Sequence
import datetime
import enum
import glob
import multiprocessing.pool
import os
import signal
import sys
from typing import List, NamedTuple, Optional, Set

from geoutils import *

_epilog = """This script expects that GDAL utilities are installed and in PATH.
Also it expects that as part of this installation proj/proj.db is somehow
installed as well (if it is, but gdalwarp still complains - set PROJ_LIB
environment variable to point to this proj directory).

Usage examples:
 - Convert Canada DSM 2000 to WGS84
    $ ./to_wgs84.py --pixels_per_degree 3600 --resampling cubic \\
       --format_param BIGTIFF=IF_NEEDED --format_param COMPRESS=LZW \\
       cdsm-canada-dem.tif cdsm-canada-dem_wgs84_cgvd28.tif
"""

# Boundary detection status
BoundaryStatus = enum.Enum("BoundaryStatus",
                           ["Success", "NoSrcGeoid", "NoDstGeoid",
                            "OutOfBounds", "Error"])


class BoundaryResult(NamedTuple):
    """ Boundary detection result """

    # Filename
    filename: str

    # Boundary detection status
    status: BoundaryStatus

    # Boundaries (None in case of failure)
    boundaries: Optional[Boundaries] = None

    # Error message
    msg: Optional[str] = None

    # Data tpes (None in case of failure)
    data_types: Optional[Sequence[str]] = None


def boundary_worker(filename: str, pixel_size: Optional[float],
                    pixels_per_degree: Optional[float], top: Optional[float],
                    bottom: Optional[float], left: Optional[float],
                    right: Optional[float], round_boundaries_to_degree: bool,
                    round_pixels_to_degree: bool, src_geoids: Geoids,
                    dst_geoids: Geoids) -> BoundaryResult:
    """ Boundary detection worker

    Arguments:
    filename                   -- File name
    pixel_size                 -- Optional pixel size
    pixels_per_degree          -- Optional number of pixels per degree
    top                        -- Optional top boundary
    bottom                     -- Optional bottom boundary
    left                       -- Optional left boundary
    right                      -- Optional right boundary
    round_boundaries_to_degree -- True to round boundaries to outer degree
    round_pixels_to_degree     -- True to round number of pixels per degree to
                                  whole number
    src_geoids                 -- Source geoids
    dst_geoids                 -- Destination geoids
    Reurns BoundaryResult object
    """
    try:
        boundaries = \
            Boundaries(
                filename, pixel_size=pixel_size,
                pixels_per_degree=pixels_per_degree, top=top, bottom=bottom,
                left=left, right=right,
                round_boundaries_to_degree=round_boundaries_to_degree,
                round_pixels_to_degree=round_pixels_to_degree)
        gi = GdalInfo(filename)
        if not boundaries.intersects(
                Boundaries(top=gi.top, bottom=gi.bottom, left=gi.left,
                           right=gi.right)):
            return BoundaryResult(filename=filename,
                                  status=BoundaryStatus.OutOfBounds)
        if src_geoids and \
                (not src_geoids.geoid_for(boundaries,
                                          fail_if_not_found=False)):
            return BoundaryResult(filename=filename,
                                  status=BoundaryStatus.NoSrcGeoid,
                                  data_types=gi.data_types)
        if dst_geoids and \
                (not dst_geoids.geoid_for(boundaries,
                                          fail_if_not_found=False)):
            return BoundaryResult(filename=filename,
                                  status=BoundaryStatus.NoDstGeoid,
                                  data_types=gi.data_types)
        return BoundaryResult(filename=filename, status=BoundaryStatus.Success,
                              boundaries=boundaries, data_types=gi.data_types)
    except (Exception, KeyboardInterrupt, SystemExit) as ex:
        return BoundaryResult(filename=filename, status=BoundaryStatus.Error,
                              msg=repr(ex))


# Conversion result status
ConvStatus = enum.Enum("ConvStatus", ["Success", "Exists", "Error"])

# Conversion result
ConvResult = \
    NamedTuple(
        "ConvResult",
        [
            # Name of converted file
            ("filename", str),
            # Conversion status
            ("status", ConvStatus),
            # Conversion duration
            ("duration", datetime.timedelta),
            # Optional error message
            ("msg", Optional[str])])


def conversion_worker(
        src: str, dst: str, boundaries: Boundaries, resampling: str,
        src_geoids: Geoids, dst_geoids: Geoids, out_format: Optional[str],
        format_params: List[str], overwrite: bool, quiet: bool,
        remove_src: bool) -> ConvResult:
    """ Worker function performing the conversion

    Arguments:
    src                        -- Source file name
    dst                        -- Destination file name
    boundaries                 -- Destination file boundaries
    resampling                 -- Resampling method
    src_geoids                 -- Geoid(s) of source file
    dst_geoids                 -- Geoid(s) for destination file
    out_format                 -- Optional output file format
    format_params              -- Output format options
    overwrite                  -- True to overwrite file if it exists
    quiet                      -- True to not print anything, returning message
                                  instead
    remove_src                 -- Remove source file
    Returns ConvResult object
    """
    stem, ext = os.path.splitext(dst)
    temp_file = stem + ".incomplete" + ext
    start_time = datetime.datetime.now()
    try:
        if os.path.isfile(dst) and (not overwrite):
            return ConvResult(filename=src, status=ConvStatus.Exists,
                              duration=datetime.datetime.now()-start_time,
                              msg=None)
        src_geoid: Optional[str] = None
        if src_geoids:
            src_geoid = \
                src_geoids.geoid_for(boundaries, fail_if_not_found=False)
            assert src_geoid is not None

        dst_geoid: Optional[str] = None
        if dst_geoids:
            dst_geoid = \
                dst_geoids.geoid_for(boundaries, fail_if_not_found=False)
            assert src_geoid is not None

        success, msg = \
            warp(src=src, dst=temp_file, resampling=resampling,
                 top=boundaries.top if boundaries.edges_overridden else None,
                 bottom=boundaries.bottom
                 if boundaries.edges_overridden else None,
                 left=boundaries.left if boundaries.edges_overridden else None,
                 right=boundaries.right
                 if boundaries.edges_overridden else None,
                 pixel_size_lat=boundaries.pixel_size_lat
                 if boundaries.pixel_size_overridden else None,
                 pixel_size_lon=boundaries.pixel_size_lon
                 if boundaries.pixel_size_overridden else None,
                 src_geoid=src_geoid, dst_geoid=dst_geoid,
                 out_format=out_format, format_params=format_params,
                 overwrite=True, quiet=quiet)
        if not success:
            return ConvResult(filename=src, status=ConvStatus.Error,
                              duration=datetime.datetime.now()-start_time,
                              msg=msg)
        if os.path.isfile(dst):
            if not quiet:
                print(f"Removing '{dst}'")
            os.unlink(dst)
        if not quiet:
            print(f"Renaming '{temp_file}' to '{dst}'")
        os.rename(temp_file, dst)
        if remove_src:
            os.unlink(src)
        return ConvResult(filename=src, status=ConvStatus.Success,
                          duration=datetime.datetime.now()-start_time, msg=msg)
    except (Exception, KeyboardInterrupt, SystemExit) as ex:
        return ConvResult(filename=src, status=ConvStatus.Error,
                          duration=datetime.datetime.now()-start_time,
                          msg=repr(ex))
    finally:
        try:
            if os.path.isfile(temp_file):
                os.unlink(temp_file)
        except OSError:
            pass


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = argparse.ArgumentParser(
        description="Converts whatever geospatial file to WGS84",
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=_epilog)
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
        "--resampling", metavar="METHOD",
        choices=warp_resamplings(),
        help="Resampling method. See "
        "https://gdal.org/programs/gdalwarp.html#cmdoption-gdalwarp-r for "
        "explanations. Default is `near' for byte data, 'cubic' for other "
        "data")
    argument_parser.add_argument(
        "--src_geoid", metavar="GEOID_FILE", action="append",
        help="Geoid source file heights specified relative to. Name may "
        "include wildcards (to handle multifile geoids). This argument may be "
        "specified several times (in order of preference decrease). If "
        "directory not specified geoids also looked up in this script's "
        "directory. Default is to assume ellipsoidal source heights")
    argument_parser.add_argument(
        "--dst_geoid", metavar="GEOID_FILE", action="append",
        help="Geoid resulting file heights specified relative to. Name may "
        "include wildcards (to handle multifile geoids). This argument may be "
        "specified several times (in order of preference decrease). If "
        "directory not specified geoids also looked up in this script's "
        "directory. Default is to assume ellipsoidal resulting heights")
    argument_parser.add_argument(
        "--extend_geoid_coverage", metavar="DEGREES", type=float, default=0.,
        help="Artificially extend geoid boundaries when checking for "
        "coverage. Useful when geoid and converted file cut at same "
        "latitude/longitude degree, but converted file has margin that "
        "extends it beyond geoid (this making it not fully covered) - as it "
        "is the case for Canada")
    argument_parser.add_argument(
        "--format", metavar="GDAL_DRIVER_NAME",
        help="File format expressed as GDAL driver name (see "
        "https://gdal.org/drivers/raster/index.html ). By default derived "
        "from target file extension")
    argument_parser.add_argument(
        "--format_param", metavar="NAME=VALUE", action="append",
        default=[],
        help="Format option (e.g. COMPRESS=PACKBITS or BIGTIFF=IF_NEEDED)")
    argument_parser.add_argument(
        "--out_dir", metavar="OUTPUT_DIRECTORY",
        help="If present - mass conversion is performed and this is the "
        "target directory")
    argument_parser.add_argument(
        "--out_ext", metavar=".EXT",
        help="Extension for output files to use in case of mass conversion. "
        "Default is to keep original extension")
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
        "--remove_src", action="store_true",
        help="Remove source file after successful conversion (e.g. to save "
        "space)")
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

    filenames: List[str] = []
    if args.out_dir is None:
        error_if(len(args.FILES) < 2,
                 "Destination file name not provided")
        error_if(len(args.FILES) > 2,
                 "Exactly two file names (source and destination) should be "
                 "provided")
        error_if(not os.path.isfile(args.FILES[0]),
                 f"File '{args.FILES[0]}' not found")
        filenames.append(args.FILES[0])
    else:
        for files_arg in args.FILES:
            files = glob.glob(files_arg)
            error_if(not files,
                     f"No source files matching '{files_arg}' found")
            filenames += list(files)

    try:
        SrcInfo = NamedTuple("SrcInfo",
                             [("filename", str), ("boundaries", Boundaries)])
        src_infos: List[SrcInfo] = []
        oob_files: List[str] = []
        no_geoid_files: List[str] = []
        src_geoids = \
            Geoids(args.src_geoid, extension=args.extend_geoid_coverage)
        dst_geoids = \
            Geoids(args.dst_geoid, extension=args.extend_geoid_coverage)
        source_data_types: Set[str] = set()
        failed_files: List[str] = []

        def boundariesCompleter(br: BoundaryResult) -> None:
            if br.data_types is not None:
                source_data_types.update(br.data_types)
            if br.status == BoundaryStatus.Success:
                assert br.boundaries is not None
                src_infos.append(SrcInfo(filename=br.filename,
                                         boundaries=br.boundaries))
                return
            if br.status == BoundaryStatus.OutOfBounds:
                warning(f"'{br.filename}' lies outside of given boundaries. "
                        f"It will not be processed")
                oob_files.append(br.filename)
                return
            if br.status == BoundaryStatus.NoSrcGeoid:
                warning(f"'{br.filename}' not covered by any source geoid. It "
                        f"will not be processed")
                no_geoid_files.append(br.filename)
                return
            if br.status == BoundaryStatus.NoDstGeoid:
                warning(f"'{br.filename}' not covered by any target geoid. It "
                        f"will not be processed")
                no_geoid_files.append(br.filename)
                return
            if br.status == BoundaryStatus.Error:
                warning(f"'{br.filename}' has a problem: {br.msg}")
                failed_files.append(br.filename)
                return

        original_sigint_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
        with multiprocessing.pool.ThreadPool(
                processes=threads_arg(args.threads)) as pool:
            signal.signal(signal.SIGINT, original_sigint_handler)
            for filename in filenames:
                pool.apply_async(
                    boundary_worker,
                    kwds={"filename": filename, "pixel_size": args.pixel_size,
                          "pixels_per_degree": args.pixels_per_degree,
                          "top": args.top, "bottom": args.bottom,
                          "left": args.left, "right": args.right,
                          "round_boundaries_to_degree":
                          args.round_boundaries_to_degree,
                          "round_pixels_to_degree":
                          args.round_pixels_to_degree,
                          "src_geoids": src_geoids, "dst_geoids": dst_geoids},
                    callback=boundariesCompleter)
            pool.close()
            pool.join()
        src_infos.sort()
        resampling = get_resampling(arg_resampling=args.resampling,
                                    src_data_types=source_data_types,
                                    dst_data_type=None)
        common_kwargs = {
            "resampling": resampling,
            "src_geoids": src_geoids,
            "dst_geoids": dst_geoids,
            "out_format": args.format,
            "format_params": args.format_param,
            "overwrite": args.overwrite,
            "remove_src": args.remove_src}
        if args.out_dir is None:
            error_if(not src_infos, "Nothing to convert")
            cr = conversion_worker(src=args.FILES[0], dst=args.FILES[1],
                                   boundaries=src_infos[0].boundaries,
                                   quiet=False, **common_kwargs)
            if cr.status == ConvStatus.Error:
                assert cr.msg is not None
                error(cr.msg)
            error_if(cr.status == ConvStatus.Exists,
                     f"File '{args.FILES[1]}' already exists. Specify "
                     f"--overwrite to overwrite it")
            assert cr.status == ConvStatus.Success
            print(f"Conversion took {Durator.duration_to_hms(cr.duration)}")
            return

        start_time = datetime.datetime.now()

        if not os.path.isdir(args.out_dir):
            os.makedirs(args.out_dir)

        total_count = len(src_infos)
        completed_count = [0]  # Made list to facilitate closure in completer()
        skipped_files: List[str] = []

        def conversionCompleter(cr: ConvResult) -> None:
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
                msg += f"\n{cr.msg}\nConverted in " \
                    f"{Durator.duration_to_hms(cr.duration)}"
            print(msg)

        original_sigint_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
        with multiprocessing.pool.ThreadPool(
                processes=threads_arg(args.threads)) as pool:
            signal.signal(signal.SIGINT, original_sigint_handler)
            for src_info in src_infos:
                kwargs = common_kwargs.copy()
                kwargs["src"] = src_info.filename
                kwargs["dst"] = \
                    os.path.join(args.out_dir,
                                 os.path.basename(src_info.filename))
                if args.out_ext is not None:
                    kwargs["dst"] = \
                        os.path.splitext(kwargs["dst"])[0] + args.out_ext
                kwargs["boundaries"] = src_info.boundaries
                kwargs["quiet"] = True
                pool.apply_async(conversion_worker, kwds=kwargs,
                                 callback=conversionCompleter)
            pool.close()
            pool.join()

        if skipped_files:
            print(f"{len(skipped_files)} previously existing files skipped")
        if no_geoid_files:
            print(f"{len(no_geoid_files)} files are out of geoid coverage")
        if failed_files:
            print("Following files were not converted due to errors:")
            for filename in sorted(failed_files):
                print(f"  {filename}")

        implicit_conversion_params: List[str] = []
        if resampling != args.resampling:
            implicit_conversion_params.append(f"resampling of {resampling}")
        if implicit_conversion_params:
            print(f"Implicitly chosen conversion parameters: "
                  f"{', '.join(implicit_conversion_params)}")

        print(
            f"Total duration: "
            f"{Durator.duration_to_hms(datetime.datetime.now() - start_time)}")
    except KeyboardInterrupt:
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1:])
