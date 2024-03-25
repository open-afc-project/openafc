#!/usr/bin/env python3
""" Preparing SQLite DB with population density data  """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wildcard-import, too-many-locals, invalid-name
# pylint: disable=too-many-branches, too-many-statements, wrong-import-order

import argparse
import datetime
import math
import os
from osgeo import gdal
import sqlite3
import struct
import sys
import tempfile
from typing import Dict, List, Optional

from geoutils import error, error_if, setup_logging, warp

# Default resolution (tile size) of population database in seconds
DEFAULT_RESOLUTION_SEC = 60

# Population database table name
TABLE_NAME = "population_density"
# Name of field with cumulative population density (ultimately - normed to 1)
CUMULATIVE_DENSITY_FIELD = "cumulative_density"

# Names of tile boundary fields
MIN_LAT_FIELD = "min_lat"
MAX_LAT_FIELD = "max_lat"
MIN_LON_FIELD = "min_lon"
MAX_LON_FIELD = "max_lon"

# Database bulk write saize (in number of records)
BULK_WRITE_THRESHOLD = 1000


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = argparse.ArgumentParser(
        description="Make Population density DB for als_load_tool.py")
    argument_parser.add_argument(
        "--resolution", metavar="SECONDS", default=DEFAULT_RESOLUTION_SEC,
        type=float,
        help=f"Resolution (tile size) of resulting database in seconds. "
        f"Default is {DEFAULT_RESOLUTION_SEC}")
    argument_parser.add_argument(
        "--overwrite", action="store_true",
        help="Overwrite target database if it is already exists")
    argument_parser.add_argument(
        "--center_lon_180", action="store_true",
        help="Better be set for countries crossing 180 longitude (e.g for "
        "USA with Alaska)")
    argument_parser.add_argument(
        "FROM", help="Source GDAL-compatible population density file")
    argument_parser.add_argument(
        "TO", help="Resulting sqlite3 file")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)
    args = argument_parser.parse_args(argv)

    setup_logging()

    error_if(not os.path.isfile(args.FROM),
             f"Source file '{args.FROM}' not found")
    if os.path.isfile(args.TO):
        error_if(not args.overwrite,
                 f"Database file '{args.TO}' already exists. Specify "
                 f"'--overwrite' to overwrite it")
        try:
            os.unlink(args.TO)
        except OSError as ex:
            error(f"Error deleting '{args.TO}': {repr(ex)}")
    else:
        target_dir = os.path.dirname(args.TO)
        if target_dir and (not os.path.isdir(target_dir)):
            try:
                os.makedirs(target_dir)
            except OSError as ex:
                error(f"Error creating directory '{target_dir}' for target "
                      f"database: {repr(ex)}'")

    gdal_dataset: Optional[gdal.Dataset] = None
    gdal_band: Optional[gdal.Band] = None
    scaled_file: Optional[str] = None
    db_conn: Optional[sqlite3.Connection] = None
    success = False
    try:
        fd, scaled_file = \
            tempfile.mkstemp(
                prefix=os.path.basename(os.path.splitext(__file__)[0]),
                suffix=".tif")
        os.close(fd)
        warp(src=args.FROM, dst=scaled_file, resampling="sum",
             pixel_size_lat=args.resolution / 3600,
             pixel_size_lon=args.resolution / 3600,
             format_params=["BIGTIFF=YES", "COMPRESS=PACKBITS"],
             data_type="Float64", center_lon_180=args.center_lon_180,
             overwrite=True, quiet=False)
        db_conn = sqlite3.connect(args.TO)
        db_cur = db_conn.cursor()
        db_cur.execute(
            f"CREATE TABLE {TABLE_NAME}({CUMULATIVE_DENSITY_FIELD} real, "
            f"{MIN_LAT_FIELD} real, {MAX_LAT_FIELD} real, "
            f"{MIN_LON_FIELD} real, {MAX_LON_FIELD} real)")
        db_cur.execute(
            f"CREATE INDEX {CUMULATIVE_DENSITY_FIELD}_idx ON {TABLE_NAME}"
            f"({CUMULATIVE_DENSITY_FIELD} ASC)")
        gdal_dataset = gdal.Open(scaled_file, gdal.GA_ReadOnly)
        line_len = gdal_dataset.RasterXSize
        num_lines = gdal_dataset.RasterYSize
        lon0, lon_res, _, lat0, _, lat_res = gdal_dataset.GetGeoTransform()
        unpack_format = "d" * line_len

        gdal_band = gdal_dataset.GetRasterBand(1)
        nodata = gdal_band.GetNoDataValue()

        total_population: float = 0
        bulk: List[Dict[str, float]] = []
        start_time = datetime.datetime.now()
        print("Fetching density data from file")
        for lat_idx in range(num_lines):
            print(f"Line {lat_idx} of {num_lines} "
                  f"({lat_idx * 100 / num_lines:.1f}%)",
                  end="\r", flush=True)
            raster_bytes = gdal_band.ReadRaster(xoff=0, yoff=lat_idx,
                                                xsize=line_len, ysize=1)
            raster_values = struct.unpack(unpack_format, raster_bytes)
            for lon_idx in range(line_len):
                v = raster_values[lon_idx]
                if math.isnan(v) or (v in (0, nodata)):
                    continue
                total_population += v
                lat1 = lat0 + lat_idx * lat_res
                lat2 = lat0 + (lat_idx + 1) * lat_res
                lon1 = lon0 + lon_idx * lon_res
                lon2 = lon0 + (lon_idx + 1) * lon_res
                min_lon = min(lon1, lon2)
                max_lon = max(lon1, lon2)
                while min_lon < -180:
                    min_lon += 360
                    max_lon += 360
                while max_lon > 180:
                    min_lon -= 360
                    max_lon -= 360
                bulk.append({CUMULATIVE_DENSITY_FIELD: total_population,
                             MIN_LAT_FIELD: min(lat1, lat2),
                             MAX_LAT_FIELD: max(lat1, lat2),
                             MIN_LON_FIELD: min_lon,
                             MAX_LON_FIELD: max_lon})
                if len(bulk) < BULK_WRITE_THRESHOLD:
                    continue
                db_cur.executemany(
                    f"INSERT INTO {TABLE_NAME} VALUES("
                    f":{CUMULATIVE_DENSITY_FIELD}, "
                    f":{MIN_LAT_FIELD}, :{MAX_LAT_FIELD},"
                    f":{MIN_LON_FIELD}, :{MAX_LON_FIELD})",
                    bulk)
                db_conn.commit()
                bulk = []
        if bulk:
            db_cur.executemany(
                f"INSERT INTO {TABLE_NAME} VALUES("
                f":{CUMULATIVE_DENSITY_FIELD}, "
                f":{MIN_LAT_FIELD}, :{MAX_LAT_FIELD},"
                f":{MIN_LON_FIELD}, :{MAX_LON_FIELD})",
                bulk)
            db_conn.commit()
            bulk = []
        print(f"\nPopulation total: {total_population}")
        print("Normalizing density")
        db_cur.execute(f"UPDATE {TABLE_NAME} SET {CUMULATIVE_DENSITY_FIELD} = "
                       f"{CUMULATIVE_DENSITY_FIELD} / {total_population}")
        db_conn.commit()
        success = True
    except KeyboardInterrupt:
        pass
    finally:
        gdal_band = None
        gdal_dataset = None
        if scaled_file is not None:
            try:
                os.unlink(scaled_file)
            except OSError:
                pass
        if db_conn is not None:
            db_conn.close()
        if not success:
            try:
                os.unlink(args.TO)
            except OSError:
                pass


if __name__ == "__main__":
    main(sys.argv[1:])
