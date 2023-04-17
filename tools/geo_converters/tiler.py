#!/usr/bin/env python3
# Tool for tiling geospatial image files (wrapper around gdal_translate)

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=unused-wildcard-import, invalid-name, too-few-public-methods
# pylint: disable=broad-exception-caught, too-many-arguments, wildcard-import
# pylint: disable=too-many-statements, too-many-branches, too-many-locals
# pylint: disable=too-many-return-statements

import argparse
import datetime
import enum
import glob
import multiprocessing
import multiprocessing.pool
import os
import shlex
import signal
import sys
from typing import List, NamedTuple, Optional, Set

from geoutils import *

_EPILOG = """This script expects that GDAL utilities are installed and in PATH.
Also it expects that as part of this installation proj/proj.db is somehow
installed as well (if it is, but gdalwarp still complains - set PROJ_LIB
environment variable to point to this proj directory).

Some examples:
- Tile NLCD files in nlcd/production to TILED_NLCD_PROD, using 8 CPUs.
  Tile names are like usa_lc_prd_n40w90.tif
   $ tiler.py --threads 8 --tile_pattern \
     TILED_NLCD_PROD/usa_lc_prd_{lat_hem}{lat_u:02}{lon_hem}{lon_l03}.tif \\
     nlcd/production/*.tif
"""

# Conversion result status
ConvStatus = enum.Enum("ConvStatus", ["Success", "Exists", "Dropped", "Error"])


class ConvResult(NamedTuple):
    """ Conversion result """

    # Name of tile
    tilename: str

    # Conversion status
    status: ConvStatus

    # Conversion duration
    duration: datetime.timedelta

    # Command lines
    command_lines: Optional[List[str]] = None

    # Optional error message
    msg: Optional[str] = None


def tile_creator(tile_pattern: str, sources: List[str], top: int, left: int,
                 margin: int, pixel_size_lat: float, pixel_size_lon: float,
                 scale: Optional[List[float]], no_data: Optional[str],
                 data_type: Optional[str], resampling: str, overwrite: bool,
                 remove_values: Optional[List[float]],
                 out_format: Optional[str], format_params: List[str],
                 verbose: bool) -> ConvResult:
    """ Worker function that creates tile

    Arguments:
    tile_pattern   -- Tile file name pattern
    sources        -- List of source files (maybe empty)
    top            -- Top latitude
    left           -- Left longitude
    margin         -- Number of margin pixels
    pixel_size_lat -- Pixel size in latitudinal direction
    pixel_size_lon -- Pixel size in longitudinal direction
    scale          -- Optional scale as [src_min,src_max,dst_min, dst_max]
    no_data        -- Optional NoData value
    data_type      -- Optional pixel data type
    pixel_size_lat -- Pixel size in latitudinal direction
    pixel_size_lon -- Pixel size in longitudinal direction
    resampling     -- Resampling method
    overwrite      -- True to overwrite existing files, False to skip
    remove_values  -- Optional list of values values in monochrome tiles to
                      drop
    out_format     -- Optional output format
    format_params  -- Optional output format parameters
    verbose        -- Print all output, fail on first failure
    Returns ConvResult object
    """
    while left < -180:
        left += 360
    while left >= 180:
        left -= 360

    temp_filename: Optional[str] = None
    temp_filename_vrt: Optional[str] = None
    temp_filename_xml: Optional[str] = None
    try:
        tile_filename = \
            tile_pattern.format(lat_u=abs(top), lat_d=abs(top-1),
                                lon_l=abs(left), lon_r=abs(left + 1),
                                lat_hem='n' if top > 0 else 's',
                                LAT_HEM='N' if top > 0 else 'S',
                                lon_hem='e' if left >= 0 else 'w',
                                LON_HEM='E' if left >= 0 else 'W')
        temp_filename = os.path.splitext(tile_filename)[0] + ".incomplete" + \
            os.path.splitext(tile_filename)[1]
        temp_filename_vrt = \
            os.path.splitext(tile_filename)[0] + ".incomplete.vrt"
        temp_filename_xml = temp_filename + ".aux.xml"
        start_time = datetime.datetime.now()
        if os.path.isfile(tile_filename) and (not overwrite):
            return ConvResult(tilename=tile_filename, status=ConvStatus.Exists,
                              duration=datetime.datetime.now()-start_time)

        # Preparing source - only source or vrt of all sources
        command_lines: List[str] = []
        if len(sources) > 1:
            vrt_args = ["gdalbuildvrt", "-ignore_srcmaskband",
                        "-r", resampling, temp_filename_vrt] + sources[::-1]
            command_lines.append(
                " ".join(shlex.quote(arg) for arg in vrt_args))
            exec_result = execute(vrt_args, env=gdal_env(),
                                  disable_output=True, return_error=True,
                                  fail_on_error=False)
            if exec_result is not None:
                assert isinstance(exec_result, str)
                return ConvResult(tilename=tile_filename,
                                  status=ConvStatus.Error,
                                  duration=datetime.datetime.now()-start_time,
                                  msg=exec_result, command_lines=command_lines)
            src = temp_filename_vrt
        else:
            src = sources[0]

        # Translate from source to tile
        trans_args = \
            ["gdal_translate", "-strict", "-r", resampling, "-projwin",
             str(left - pixel_size_lon * margin),
             str(top + pixel_size_lat * margin),
             str(left + 1 + pixel_size_lon * margin),
             str(top - 1 - pixel_size_lat * margin)]
        if out_format:
            trans_args += ["-of", out_format]
        for fp in format_params:
            trans_args += ["-co", fp]
        if scale:
            trans_args += ["-scale"] + [str(s) for s in scale]
        if no_data:
            trans_args += ["-a_nodata", no_data]
        if data_type:
            trans_args += ["-ot", data_type]
        trans_args += [src, temp_filename]
        command_lines.append(" ".join(shlex.quote(arg) for arg in trans_args))
        exec_result = execute(trans_args, env=gdal_env(),
                              disable_output=not verbose,
                              return_error=not verbose, fail_on_error=verbose)

        if verbose:
            assert exec_result is True
        elif exec_result is not None:
            assert isinstance(exec_result, str)
            return ConvResult(tilename=tile_filename, status=ConvStatus.Error,
                              duration=datetime.datetime.now()-start_time,
                              msg=exec_result, command_lines=command_lines)
        gi = GdalInfo(temp_filename, options=["-stats"], fail_on_error=False)
        if not gi:
            return ConvResult(tilename=tile_filename, status=ConvStatus.Error,
                              duration=datetime.datetime.now()-start_time,
                              msg="gdalinfo inspection failed",
                              command_lines=command_lines)
        if os.path.isfile(tile_filename):
            if verbose:
                print(f"Removing '{tile_filename}'")
            os.unlink(tile_filename)
        if gi.valid_percent == 0:
            return \
                ConvResult(tilename=tile_filename, status=ConvStatus.Dropped,
                           duration=datetime.datetime.now()-start_time,
                           msg="All pixels are NoData",
                           command_lines=command_lines)
        if (gi.min_value is not None) and (gi.min_value == gi.max_value) and \
                (gi.min_value in (remove_values or [])):
            return \
                ConvResult(
                    tilename=tile_filename, status=ConvStatus.Dropped,
                    duration=datetime.datetime.now()-start_time,
                    msg=f"All valid pixels are equal to {gi.min_value}",
                    command_lines=command_lines)
        if verbose:
            print(f"Renaming '{temp_filename}' to '{tile_filename}'")
        os.rename(temp_filename, tile_filename)

        return ConvResult(tilename=tile_filename, status=ConvStatus.Success,
                          duration=datetime.datetime.now()-start_time,
                          command_lines=command_lines)
    except (Exception, KeyboardInterrupt, SystemExit) as ex:
        return ConvResult(tilename=tile_filename, status=ConvStatus.Error,
                          duration=datetime.datetime.now()-start_time,
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
        description="Cut geospatial image to 1x1 tiles (wrapper around "
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
        help="Maximum latitude")
    argument_parser.add_argument(
        "--bottom", metavar="MIN_LATITUDE", type=float,
        help="Minimum latitude")
    argument_parser.add_argument(
        "--left", metavar="MIN_LONGITUDE", type=float,
        help="Minimum longitude. Should be provided with --right or not at "
        "all")
    argument_parser.add_argument(
        "--right", metavar="MAX_LONGITUDE", type=float,
        help="Maximum longitude. Should be provided with --left or not at all")
    argument_parser.add_argument(
        "--margin", metavar="MARGIN_PIXELS", type=int, default=0,
        help="Size of outer margin in pixels. Default is 0 (no margin)")
    argument_parser.add_argument(
        "--scale", metavar="V", nargs=4, type=float,
        help=f"Scale/offset output data. Parameter order is: SRC_MIN SRC_MAX "
        f"DST_MIN DST_MAX where [SRC_MIN, SRC_MAX] interval "
        f"maps to [DST_MIN, DST_MAX]. For Int16/Int32 -> PNG default is "
        f"'{' '.join(str(v) for v in DEFAULT_INT_PNG_SCALE)}', for "
        f"Float32/Float64 -> PNG default is "
        f"'{' '.join(str(v) for v in DEFAULT_FLOAT_PNG_SCALE)}")
    argument_parser.add_argument(
        "--no_data", metavar="NODATA",
        help=f"No Data value for target files. If target is UInt16 PNG and "
        f"source is not Byte/UInt16, default is {DEFAULT_PNG_NO_DATA}")
    data_types = translate_datatypes()
    argument_parser.add_argument(
        "--data_type", metavar="DATA_TYPE", choices=data_types,
        help=f"Pixel data type for output file. Possible values: "
        f"{', '.join(data_types)}")
    resampling_methods = warp_resamplings()
    argument_parser.add_argument(
        "--resampling", metavar="METHOD",
        choices=resampling_methods,
        help=f"Resampling method. Possible values: "
        f"{', '.join(resampling_methods)}. See "
        f"https://gdal.org/programs/gdalwarp.html#cmdoption-gdalwarp-r for "
        f"explanations. Default is `near' for byte data, 'cubic' for other "
        f"data")
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
        "--tile_pattern", metavar="PATTERN", required=True,
        help="Pattern for tile filenames. May include path. Final (filename) "
        "part may include '{VALUE[:FORMAT]}' format specifiers. Here VALUE is "
        "one of 'lat_u', 'lat_d', 'lon_l', lon_r', 'lat_hem', 'LAT_HEM', "
        "'lon_hem', 'LON_HEM'. Where lat/lon for absolute integer part of "
        "latitude/longitude in degrees, '_u/_d' are for lower/upper latitude "
        "of tile, '_l/_r' for left/right longitude of tile, '_hem' for "
        "lowercase hemisphere (n/s/e/w), '_HEM' for uppercase hemisphere "
        "(N/S/E/W). FORMAT is for f-string format (e.g. 03 for 3 digits with "
        "leading zeros). This parameter is required")
    argument_parser.add_argument(
        "--remove_value", metavar="PIXEL_VALUE", action="append", type=float,
        help="Remove tiles all valid points of which consists only of given "
        "value (e.g. artificial NLCD in far sea. This parameter may be "
        "specified more than once")
    argument_parser.add_argument(
        "--verbose", action="store_true",
        help="Create tiles sequentially, printing gdal_translate output in "
        "real time, failing on first fail. For debug purposes")
    argument_parser.add_argument(
        "SRC", metavar="FILENAMES", nargs="+",
        help="Source filenames. May contain wildcards")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)
    args = argument_parser.parse_args(argv)

    setup_logging()

    if args.nice:
        nice()

    start_time = datetime.datetime.now()

    error_if((args.left is None) != (args.right is None),
             "--left and --right should be specified together or not at all")
    try:
        args.tile_pattern.format(lat_u=0, lat_d=0, lon_l=0, lon_r=0,
                                 lat_hem="n", LAT_HEM="N",
                                 lon_hem="e", LON_HEM="E")
    except (KeyError, ValueError) as ex:
        error(f"Invalid filename pattern syntax: {repr(ex)}")

    if os.path.dirname(args.tile_pattern):
        os.makedirs(os.path.dirname(args.tile_pattern), exist_ok=True)

    PixelSizes = NamedTuple("PixelSizes",
                            [("pixel_size_lat", Optional[float]),
                             ("pixel_size_lon", Optional[float])])
    source_pixel_sizes: Set[PixelSizes] = set()
    source_data_types: Set[str] = set()
    SourceBoundary = NamedTuple("SourceBoundary",
                                [("filename", str),
                                 ("boundaries", Boundaries)])
    source_boundaries: List[SourceBoundary] = []
    global_boundaries: Optional[Boundaries] = None
    for src in args.SRC:
        found = False
        for filename in glob.glob(src):
            found = True
            file_boundaries = Boundaries(filename)
            source_boundaries.append(
                SourceBoundary(filename=filename, boundaries=file_boundaries))
            source_pixel_sizes.add(
                PixelSizes(pixel_size_lat=file_boundaries.pixel_size_lat,
                           pixel_size_lon=file_boundaries.pixel_size_lon))
            source_data_types.update(GdalInfo(filename).data_types)

            global_boundaries = \
                (global_boundaries if global_boundaries is not None
                 else file_boundaries).combine(file_boundaries,
                                               round_boundaries_to_degree=True)
        error_if(not found,
                 f"No files matching '{src}' were found")
    assert global_boundaries is not None
    global_boundaries = \
        global_boundaries.crop(
            Boundaries(top=args.top, bottom=args.bottom,
                       left=args.left, right=args.right),
            round_boundaries_to_degree=True)
    error_if(global_boundaries is None,
             "Given source files lie completely outside of given boundaries")
    if args.pixel_size is not None:
        error_if(args.pixels_per_degree is not None,
                 "--pixel_size and --pixels_per_degree may not be specified "
                 "together")
        pixel_size_lat = pixel_size_lon = args.pixel_size
    elif args.pixels_per_degree is not None:
        pixel_size_lat = pixel_size_lon = 1 / args.pixels_per_degree
    elif len(source_pixel_sizes) == 1:
        pixel_size_lat = list(source_pixel_sizes)[0].pixel_size_lat
        pixel_size_lon = list(source_pixel_sizes)[0].pixel_size_lon
        error_if((pixel_size_lat is None) or (pixel_size_lon is None),
                 "Unable to derive pixel size from source files. It should be "
                 "specified explicitly with --pixel_size or "
                 "--pixels_per_degree")
    else:
        error("Source files have different pixel sizes. It should be "
              "specified explicitly with --pixel_size or --pixels_per_degree")

    scale = get_scale(arg_scale=args.scale, src_data_types=source_data_types,
                      dst_format=args.format, dst_data_type=args.data_type,
                      dst_ext=os.path.splitext(args.tile_pattern)[1])
    no_data = get_no_data(arg_no_data=args.no_data,
                          src_data_types=source_data_types,
                          dst_format=args.format, dst_data_type=args.data_type,
                          dst_ext=os.path.splitext(args.tile_pattern)[1])
    resampling = get_resampling(arg_resampling=args.resampling,
                                src_data_types=source_data_types,
                                dst_data_type=args.data_type)

    assert (global_boundaries is not None) and \
        (global_boundaries.top is not None) and \
        (global_boundaries.bottom is not None) and \
        (global_boundaries.right is not None) and \
        (global_boundaries.left is not None)

    Tile = NamedTuple("Tile",
                      [("top", int), ("left", int), ("sources", List[str])])
    tiles: List[Tile] = []
    for top in range(round(global_boundaries.top),
                     round(global_boundaries.bottom), -1):
        for left in range(round(global_boundaries.left),
                          round(global_boundaries.right)):
            tile_boundaries = \
                Boundaries(top=top, bottom=top - 1, left=left, right=left + 1)
            sources = [sb.filename for sb in source_boundaries
                       if sb.boundaries.intersects(tile_boundaries)]
            if not sources:
                continue
            tiles.append(Tile(top=top, left=left, sources=sources))

    total_tiles = len(tiles)

    completed_tiles = [0]  # List to facilitate closure in completer()
    skipped_tiles: List[str] = []
    failed_tiles: List[str] = []

    def completer(cr: ConvResult) -> None:
        """ Processes completion of a single tile """
        completed_tiles[0] += 1
        msg = f"{completed_tiles[0]} of {total_tiles} " \
            f"({completed_tiles[0] * 100 // total_tiles}%) {cr.tilename}: "
        if cr.status == ConvStatus.Exists:
            msg += "Tile exists. Skipped"
            skipped_tiles.append(cr.tilename)
        elif cr.status == ConvStatus.Error:
            assert cr.msg is not None
            error_if(args.verbose, cr.msg)
            if cr.command_lines:
                msg += "\n" + "\n".join(cr.command_lines)
            msg += "\n" + cr.msg
        elif cr.status == ConvStatus.Dropped:
            assert cr.msg is not None
            msg += f"{cr.msg}. Tile dropped"
        else:
            assert cr.status == ConvStatus.Success
            if cr.msg is not None:
                msg += "\n" + cr.msg
            msg += f"Converted in {Durator.duration_to_hms(cr.duration)}"
        print(msg)

    print(f"{len(tiles)} tiles in {global_boundaries}")

    original_sigint_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
    pool = None if args.verbose else \
        multiprocessing.pool.ThreadPool(
            processes=threads_arg(args.threads))
    signal.signal(signal.SIGINT, original_sigint_handler)
    try:
        for tile in tiles:
            kwargs = {
                "tile_pattern": args.tile_pattern,
                "sources": tile.sources,
                "top": tile.top,
                "left": tile.left,
                "margin": args.margin,
                "pixel_size_lat": pixel_size_lat,
                "pixel_size_lon": pixel_size_lon,
                "scale": scale,
                "no_data": no_data,
                "data_type": args.data_type,
                "resampling": resampling,
                "overwrite": args.overwrite,
                "remove_values": args.remove_value,
                "out_format": args.format,
                "format_params": args.format_param,
                "verbose": args.verbose}
            if args.verbose:
                completer(tile_creator(**kwargs))
            else:
                assert pool is not None
                pool.apply_async(tile_creator, kwds=kwargs,
                                 callback=completer)
        if pool:
            pool.close()
            pool.join()
            pool = None

        if skipped_tiles:
            print(f"{len(skipped_tiles)} previously existing tiles skipped")
        if failed_tiles:
            print("Following tiles were not created due to errors:")
            for tilename in sorted(failed_tiles):
                print(f"  {tilename}")

        implicit_conversion_params: List[str] = []
        if scale != args.scale:
            implicit_conversion_params.append(f"scale of {scale}")
        if no_data != args.no_data:
            implicit_conversion_params.append(f"NoData of {no_data}")
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
    finally:
        if pool is not None:
            pool.terminate()


if __name__ == "__main__":
    main(sys.argv[1:])
