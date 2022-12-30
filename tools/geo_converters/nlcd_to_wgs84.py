#!/usr/bin/env python3
# Converts NLCD file to WGS84 lat/lon format

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

import argparse
import os
import re
import shlex
import subprocess
import sys
from typing import Any, Dict, List, Optional, Union


_epilog = """This script expects that GDAL utilities are installed and in PATH.
Also it expects that as part of this installation proj/proj.db is somehow
installed as well (if it is, but gdalwarp still complains - set PROJ_LIB
environment variable to point to it).

Usage examples:
 - Convert CONUS NLCD file nlcd_2019_land_cover_l48_20210604.img that is in map
   projection (Albers conic) to nlcd_2019_land_cover_l48_20210604_resampled.tif
   that uses WGS84 (lat/lon) coordinates. Boundaries taken from Google Maps
   (don't use gdalinfo on source file for this!), data resolution set to
   1 second (3600 pixels per degree), TIFF data compression set to PACKBITS
   (shrinks file 9 times, no performance penalty):
     $ ./nlcd_to_wgs84.py --lat_lower 24 --lat_upper 50 \\
        --lon_left -125 --lon_right -66 --pixels_per_degree 3600 \\
        --format_param COMPRESS=PACKBITS \\
        nlcd_2019_land_cover_l48_20210604.img \\
        nlcd_2019_land_cover_l48_20210604_resampled.tif
 - Same for Alaska NLCD. Note that --lon_right is numerically less than
   --lon_left (as per Google Maps) - this is internally corrected, resulting
   file has right longitude of -129+360=231:
    $ ./nlcd_to_wgs84.py --lat_lower 51 --lat_upper 72 \\
        --lon_left 172 --lon_right -129 --pixels_per_degree 3600 \\
        --format_param COMPRESS=PACKBITS \\
        NLCD_2016_Land_Cover_AK_20200724.img \\
        NLCD_2016_Land_Cover_AK_20200724_resampled.tif
"""


def error(errmsg: str) -> None:
    """Print given error message and exit"""
    print(f"{os.path.basename(__file__)}: Error: {errmsg}",
          file=sys.stderr)
    sys.exit(1)


def error_if(condition: Any, errmsg: str) -> None:
    """If given condition met - print given error message and exit"""
    if condition:
        error(errmsg)


def warning(warnmsg: str) -> None:
    """Print given warning message and exit"""
    print(f"{os.path.basename(__file__)}: Warning: {warnmsg}",
          file=sys.stderr)


def execute(args: List[str], env: Optional[Dict[str, str]] = None,
            return_output: bool = False, fail_on_error: bool = True) \
        -> Union[bool, Optional[str]]:
    """ Execute command

    Arguments:
    args          -- List of command line parts
    env           -- Optional environment dictionary
    return_output -- True to return output and not print it, False to print
                     output and not return it
    fail_on_error -- True to fail on error, false to return None/False
    Returns True/stdout on success, False/None on error
    """
    if not return_output:
        print(" ".join(shlex.quote(a) for a in args))
    try:
        p = subprocess.run(
            args, text=True, env=env, check=False,
            stdout=subprocess.PIPE if return_output else None,
            stderr=subprocess.PIPE if return_output and (not fail_on_error)
            else None)
        if not p.returncode:
            return p.stdout if return_output else True
        if fail_on_error:
            sys.exit(p.returncode)
    except OSError as ex:
        if fail_on_error:
            error(f"{ex}")
    return None if return_output else False


def gdalwarp_env() -> Optional[Dict[str, str]]:
    """ Returns environment required to run gdalwarp. None if not needed """
    if os.name != "nt":
        return None
    if "PROJ_LIB" in os.environ:
        return None
    gw = execute(["where", "gdalwarp"], return_output=True,
                 fail_on_error=False)
    error_if(gw is None, "'gdalwarp' not found")
    assert isinstance(gw, str)
    ret = dict(os.environ)
    for path_to_proj in ("projlib", r"..\share\proj"):
        proj_path = os.path.abspath(os.path.join(os.path.dirname(gw.strip()),
                                                 path_to_proj))
        if os.path.isfile(os.path.join(proj_path, "proj.db")):
            ret["PROJ_LIB"] = proj_path
            break
    else:
        warning("Projection library directory not found. gdalwarp may refuse "
                "to work properly")
    return ret


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = argparse.ArgumentParser(
        description="Converts NLCD file to WGS84 format",
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=_epilog)
    argument_parser.add_argument(
        "--lat_lower", metavar="LAT_DEG", type=float,
        help="Lower latitude of resulting grid in north-positive degrees. "
        "Better be specified if source file in map projection format, may be "
        "omitted if source is in geodetic (latitude/longitude) format and no "
        "crop required")
    argument_parser.add_argument(
        "--lat_upper", metavar="LAT_DEG", type=float,
        help="Upper latitude of resulting grid in north-positive degrees. "
        "Better be specified if source file in map projection format, may be "
        "omitted if source is in geodetic (latitude/longitude) format and no "
        "crop required")
    argument_parser.add_argument(
        "--lon_left", metavar="LON_DEG", type=float,
        help="Left longitude of resulting grid in east-positive degrees. "
        "Better be specified if source file in map projection format, may be "
        "omitted if source is in geodetic (latitude/longitude) format and no "
        "crop required")
    argument_parser.add_argument(
        "--lon_right", metavar="LON_DEG", type=float,
        help="Right longitude of resulting grid in east-positive degrees. May "
        "be less than 'lon_left' if grid crosses 180E/W. Better be specified "
        "if source file in map projection format, may be omitted if source is "
        "in geodetic (latitude/longitude) format and no crop required")
    argument_parser.add_argument(
        "--pixel_size", metavar="DEGREES", type=float,
        help="Resulting file resolution in pixel size (expressed in degrees). "
        "May be omitted if source file is in geodetic (latitude/longitude) "
        "format and no resampling required")
    argument_parser.add_argument(
        "--pixels_per_degree", metavar="NUMBER", type=float,
        help="Resulting file resolution in pixels per degree. May be omitted "
        "if source file is in geodetic (latitude/longitude) format and no "
        "resampling required")
    argument_parser.add_argument(
        "--format", metavar="GDAL_DRIVER_NAME",
        help="File format expressed as GDAL driver name (see "
        "https://gdal.org/drivers/raster/index.html ). By default derived "
        "from target file extension")
    argument_parser.add_argument(
        "--format_param", metavar="NAME=VALUE", action="append", default=[],
        help="Format option (e.g. COMPRESS=PACKBITS)")
    argument_parser.add_argument(
        "--overwrite", action="store_true",
        help="Overwrite target file if it exists")
    argument_parser.add_argument("SRC", help="Source file name")
    argument_parser.add_argument("DST", help="Destination file name")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)
    args = argument_parser.parse_args(argv)

    error_if(not os.path.isfile(args.SRC),
             f"Can't find source file '{args.SRC}'")
    error_if(args.DST is None, "Destination file name not specified")
    error_if(os.path.isfile(args.DST) and (not args.overwrite),
             f"'{args.DST}' already exists. Specify --overwrite to overwrite")
    error_if(
        sum(0 if a is None else 1 for a in
            (args.lat_lower, args.lat_upper, args.lon_left, args.lon_right))
        not in (0, 4),
        "Minimum/maximum latitude/longitude should be specified all together "
        "or none at all")
    exec_args: List[str] = ["gdalwarp", "-t_srs", "+proj=longlat +datum=WGS84"]
    if args.lat_lower is not None:
        error_if((args.lat_lower is not None) and
                 (not (-90 <= args.lat_lower <= args.lat_upper <= 90)),
                 "Invalid latitude range")
        while args.lon_right < args.lon_left:
            args.lon_right += 360
        while (args.lon_right - 360) > args.lon_left:
            args.lon_right -= 360
        exec_args += ["-te"] + \
            [str(a) for a in (args.lon_left, args.lat_lower,
                              args.lon_right, args.lat_upper)]
    if args.pixel_size is not None:
        error_if(args.pixels_per_degree is not None,
                 "--pixel_size and --pixels_per_degree may not be specified "
                 "together")
        exec_args += ["-tr", str(args.pixel_size), str(args.pixel_size)]
    elif args.pixels_per_degree is not None:
        if args.lat_lower is not None:
            lat_degs: float = args.lat_upper - args.lat_lower
            lon_degs: float = args.lon_right - args.lon_left
        else:
            gi = execute(["gdalinfo", args.SRC], return_output=True)
            assert isinstance(gi, str)
            r: Dict[str, Dict[str, float]] = {}
            for prefix, kind in [("Lower Left", "min"),
                                 ("Upper Right", "max")]:
                r[kind] = {}
                m = re.search(
                    prefix + r".+?\(\s*(?P<lon_deg>\d+)d\s*(?P<lon_min>\d+)'"
                    r"\s*(?P<lon_sec>\d+\.\d+)\"(?P<lon_hem>[EW]),"
                    r"\s*(?P<lat_deg>\d+)d\s*(?P<lat_min>\d+)'\s*"
                    r"(?P<lat_sec>\d+\.\d+)\"(?P<lat_hem>[NS])\s*\)", gi)
                error_if(m is None,
                         "Can't determine grid dimensions. Please specify "
                         "min/max lat/lon explicitly")
                assert m is not None
                r[kind]["lat"] = \
                    (float(m.group("lat_deg")) +
                     float(m.group("lat_min")) / 60 +
                     float(m.group("lat_sec")) / 3600) * \
                    (1 if m.group("lat_hem") == "N" else -1)
                r[kind]["lon"] = \
                    (float(m.group("lon_deg")) +
                     float(m.group("lon_min")) / 60 +
                     float(m.group("lon_sec")) / 3600) * \
                    (1 if m.group("lon_hem") == "E" else -1)
            lat_degs = r["max"]["lat"] - r["min"]["lat"]
            lon_degs = r["max"]["lon"] - r["min"]["lon"]
            while lon_degs < 0:
                lon_degs += 360
            while lon_degs > 0:
                lon_degs -= 360
        exec_args += ["-ts", str(int(lon_degs * args.pixels_per_degree)),
                      str(int(lat_degs * args.pixels_per_degree))]
    if args.format:
        exec_args += ["-of", args.format]
    for fp in args.format_param:
        exec_args += ["-co", fp]
    if args.overwrite:
        exec_args += ["-overwrite"]
    exec_args += [args.SRC, args.DST]
    execute(exec_args, env=gdalwarp_env())


if __name__ == "__main__":
    main(sys.argv[1:])
