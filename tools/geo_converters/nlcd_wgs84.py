#!/usr/bin/env python3
# Converts land cover file to NLCD/WGS84 format

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=wildcard-import, invalid-name, too-few-public-methods
# pylint: disable=too-many-locals, unused-wildcard-import

import argparse
from collections.abc import Iterable
import os
import struct
import sys
import tempfile
from typing import Dict, List, NamedTuple, Set, Tuple
import yaml

from geoutils import *

try:
    from osgeo import gdal
    HAS_GDAL = True
except ImportError:
    HAS_GDAL = False

# Parameter YAML file name
PARAM_FILE_NAME = os.path.splitext(__file__)[0] + ".yaml"

# Length of color table in translated file
TARGET_COLOR_TABLE_LENGTH = 256

DEFAULT_RESAMPLING = "near"


_EPILOG = """This script expects that GDAL utilities are installed and in PATH.
Also it expects that as part of this installation proj/proj.db is somehow
installed as well (if it is, but gdalwarp still complains - set PROJ_LIB
environment variable to point to this proj directory).

Usage examples:
 - Convert CONUS NLCD file nlcd_2019_land_cover_l48_20210604.img that is in map
   projection (Albers conic) to nlcd_2019_land_cover_l48_20210604_resampled.tif
   that uses WGS84 (lat/lon) coordinates. Data resolution set to 1 second (3600
   pixels per degree) as it is comparable to 30 m pixel size in the source
   data, TIFF data compression set to PACKBITS (shrinks file 9 times, no
   performance penalty):
     $ ./nlcd_wgs84.py --pixels_per_degree 3600 \\
        --format_param COMPRESS=PACKBITS \\
        nlcd_2019_land_cover_l48_20210604.img \\
        nlcd_2019_land_cover_l48_20210604_resampled.tif
 - Same for Alaska NLCD:
     $ ./nlcd_wgs84.py --pixels_per_degree 3600 \\
         --format_param COMPRESS=PACKBITS \\
         NLCD_2016_Land_Cover_AK_20200724.img \\
         NLCD_2016_Land_Cover_AK_20200724_resampled.tif
 - Puerto Rico NLCD from NOAA source (NOAA uses different land usage codes):
     $ /nlcd_wgs84.py --pixels_per_degree 3600 --encoding noaa \\
        --format_param COMPRESS=PACKBITS \\
        pr_2010_ccap_hr_land_cover20170214.img \\
        pr_2010_ccap_hr_land_cover20170214_resampled.tif
Note that --threads only affect recoding part of this script (that is
relatively fast). Coordinate system change (second phase) uses single CPU and
takes a long time (e.g. 12 hours for Canada)
"""


class GdalProgressor:
    """ GDAL-style context printer

    Private attributes:
    _total_lines   -- Total number of raster lines
    _last_fraction -- Recently printed 1/40-th fraction
    """

    def __init__(self, filename: str, total_lines: int) -> None:
        """ Constructor

        Arguments:
        filename    -- Name of file being processed
        total_lines -- Total number of raster lines
        """
        self._total_lines = total_lines
        self._last_fraction: int = -1
        print(f"Processing:{filename}: ", end="", flush=True)

    def progress(self, line: int) -> None:
        """ Print progress

        Arguments:
        line -- 0-based raster line number
        """
        if line == (self._total_lines - 1):
            print("100 - done.")
            return
        new_fraction = line * 40 // self._total_lines
        if new_fraction == self._last_fraction:
            return
        self._last_fraction = new_fraction
        print("." if new_fraction % 4 else (new_fraction // 4 * 10), end="",
              flush=True)


class YamlParams:
    """ Parameters stored in accompanying YAML file (mostly encoding-related)

    Public attributes:
    default_code  -- Default target code
    default_color -- Default map color
    target_codes  -- By-code dictionary of TargetCodeInfo objects
    encodings     -- By name dictionary of by-code dictionaries of SrcCodeInfo
                     objects
    """
    # Information about target (NLCD as of time of this writing) code
    TargetCodeInfo = \
        NamedTuple("TargetCodeInfo",
                   [
                       # Land usage description
                       ("description", str),
                       # Color to use on generated map
                       ("color", List[int])])
    # Information about source code
    SrcCodeInfo = \
        NamedTuple("SrcCodeInfo",
                   [
                       # Land usage description
                       ("description", str),
                       # Correspondent target code
                       ("target_code", int)])

    def __init__(self):
        """ Constructor. Reads all from YAML file """
        error_if(not os.path.isfile(PARAM_FILE_NAME),
                 f"Parameter file '{PARAM_FILE_NAME}' not found")
        if hasattr(yaml, "CLoader"):
            loader = yaml.CLoader
        elif hasattr(yaml, "FullLoader"):
            loader = yaml.FullLoader
        else:
            loader = None
        with open(PARAM_FILE_NAME, encoding="utf-8") as f:
            yaml_content = f.read()
        # pylint: disable=no-value-for-parameter
        yaml_dict = yaml.load(yaml_content, loader) if loader \
            else yaml.load(yaml_content)
        try:
            self.default_code: int = yaml_dict["target"]["default_code"]
            self.default_color: List[int] = \
                yaml_dict["target"]["default_color"]
            self.target_codes: Dict[int, "YamlParams.TargetCodeInfo"] = {}
            for code, tgt_code_info in yaml_dict["target"]["codes"].items():
                error_if(
                    code in self.target_codes,
                    f"Target encoding code {code} specified more than once")
                self.target_codes[code] = \
                    self.TargetCodeInfo(
                        description=tgt_code_info["description"],
                        color=tgt_code_info["color"])
            self.encodings: Dict[str, Dict[int, "YamlParams.SrcCodeInfo"]] = {}
            for name, enc_info in yaml_dict["encodings"].items():
                self.encodings[name] = {}
                for code, src_code_info in enc_info["codes"].items():
                    error_if(
                        code in self.encodings[name],
                        f"Encoding '{name}' has information for code {code} "
                        f"specified more than once")
                    error_if(
                        src_code_info["target_code"] not in self.target_codes,
                        f"Encoding '{name}' code {code} refers target code "
                        f"{src_code_info['target_code']} not defined in "
                        f"target encoding")
                    self.encodings[name][code] = \
                        self.SrcCodeInfo(
                            description=src_code_info["description"],
                            target_code=src_code_info["target_code"])
        except LookupError as ex:
            error(f"Invalid '{os.path.basename(PARAM_FILE_NAME)}' structure: "
                  f"{ex}")

    def get_color(self, code: int) -> Tuple[int, ...]:
        """ Returns color for given code """
        return tuple(self.target_codes[code].color if code in self.target_codes
                     else self.default_color)

    def get_translation_dict(self, name: str) -> Dict[int, int]:
        """ Returns translation dictionary for given encoding """
        error_if(name not in self.encodings,
                 f"Encoding '{name}' not found")
        return {k: i.target_code for k, i in self.encodings[name].items()}


class TranslationContexts:
    """ Collection of translation contexts.

    Private attributes:
    self._contexts -- Contexts, ordered by row length and data type
    """
    # Translation context: preallocated buffers and constants used in
    # translation
    TranslationContext = \
        NamedTuple("TranslationContext",
                   [
                       # Format of input data for estruct.unpack()
                       ("input_format", str),
                       # Format of output data for struct.pack()
                       ("output_format", str),
                       # Buffer for output data
                       ("buffer", List[int])])

    # Translation of GDAL raster data types to struct data types
    _DATA_SIZES: Dict[int, str] = \
        {gdal.GDT_Byte: "B",
         gdal.GDT_Int8: "b",
         gdal.GDT_Int16: "h",
         gdal.GDT_Int32: "i",
         gdal.GDT_UInt16: "H",
         gdal.GDT_UInt32: "I"} if HAS_GDAL else {}

    def __init__(self) -> None:
        """ Constructor """
        self._contexts: \
            Dict[Tuple[int, int],
                 "TranslationContexts.TranslationContext"] = {}

    def get_context(self, input_size: int, data_type_code: int) \
            -> "TranslationContexts.TranslationContext":
        """ Context for given data length and type

        Arguments:
        input_size     -- Input data size in bytes
        data_type_code -- GDAL input data type code
        Returns Context for given size and data type
        """
        key = (input_size, data_type_code)
        ret = self._contexts.get(key)
        if ret is None:
            struct_data_type = self._DATA_SIZES.get(data_type_code)
            error_if(struct_data_type is None,
                     f"Unsupported data type code of {data_type_code} in "
                     f"source file")
            assert struct_data_type is not None
            length = input_size // struct.calcsize(struct_data_type)
            ret = \
                self.TranslationContext(
                    input_format=length * struct_data_type,
                    output_format=length * "B", buffer=length * [0])
            self._contexts[key] = ret
        return ret


class Translation:
    """ Pixel row translator to NLCD encoding.
    This class intended to be used as context manager (with 'with`)

    Private attributes:
    _translation_dict     -- Dictionary from source to target (NLCD as of time
                             of this writing) encoding
    _default_code         -- Code to use when source code missing in
                             translation dictionary
    _data_type_code       -- GDAL pixel data type code
    _translation_contexts -- Translation context factory
    """

    def __init__(self, translation_dict: Dict[int, int], default_code: int,
                 data_type_code: int) -> None:
        """ Constructor

        Arguments:
        translation _dict -- Translation dictionary to target encoding
        default_code      -- Default code in target encoding
        data_type_code    -- GDAL pixel data type code
        """
        self._translation_dict = translation_dict
        self._default_code = default_code
        self._data_type_code = data_type_code
        self._translation_contexts = TranslationContexts()

    def translate(self, input_bytes: bytes) -> Tuple[bytes, Set[int]]:
        """ Translate pixel row

        Arguments:
        input_bytes -- Input raster line byte string
        Returns tuple with translated raster line and set of codes that were
        not in translation dictionary
        """
        tc = \
            self._translation_contexts.get_context(
                input_size=len(input_bytes),
                data_type_code=self._data_type_code)
        input_values = struct.unpack(tc.input_format, input_bytes)
        unknown_codes: Set[int] = set()
        end_idx = len(input_values)
        idx = 0
        while idx < end_idx:
            try:
                while idx < end_idx:
                    tc.buffer[idx] = self._translation_dict[input_values[idx]]
                    idx += 1
            except KeyError:
                unknown_codes.add(input_values[idx])
                self._translation_dict[input_values[idx]] = self._default_code
        return (struct.pack(tc.output_format, *tc.buffer), unknown_codes)


def translate(src: str, dst: str, yaml_params: YamlParams,
              translation_name: str, threads: int) -> None:
    """ Translate pixel codes to NLCD encoding

    Arguments:
    src              -- Source file name
    dst              -- Result file name (GeoTiff, no special parameters)
    yaml_params      -- Parameters from accompanying YAML file
    translation_name -- Name of translation
    threads          -- Number of threads to use
    """
    ds_src = gdal.Open(src, gdal.GA_ReadOnly)
    band_src = ds_src.GetRasterBand(1)
    driver_gtiff = gdal.GetDriverByName('GTiff')
    ds_dst = \
        driver_gtiff.Create(dst, xsize=band_src.XSize, ysize=band_src.YSize,
                            eType=gdal.GDT_Byte)
    error_if(ds_dst is None, f"Can't create '{dst}'")
    ds_dst.SetProjection(ds_src.GetProjection())
    ds_dst.SetGeoTransform(ds_src.GetGeoTransform())
    band_dst = ds_dst.GetRasterBand(1)
    color_table_dst = gdal.ColorTable()
    for idx in range(TARGET_COLOR_TABLE_LENGTH):
        color_table_dst.SetColorEntry(idx, yaml_params.get_color(idx))
    band_dst.SetRasterColorTable(color_table_dst)
    band_dst.SetRasterColorInterpretation(gdal.GCI_PaletteIndex)
    band_dst.SetNoDataValue(yaml_params.default_code)
    translation = \
        Translation(
            translation_dict=yaml_params.get_translation_dict(
                translation_name),
            default_code=yaml_params.default_code,
            data_type_code=band_src.DataType)
    progressor = GdalProgressor(filename=src, total_lines=band_src.YSize)

    def scan_line_producer() -> Iterable[bytes]:
        """ Produces scan lines from the source file """
        for y in range(band_src.YSize):
            yield band_src.ReadRaster(xoff=0, yoff=y,
                                      xsize=band_src.XSize, ysize=1)

    unknown_codes: Set[int] = set()
    with WindowedProcessPool(producer=scan_line_producer(),
                             processor=translation.translate,
                             max_in_flight=100, threads=threads) as wpp:
        y = 0
        for scanline_dst, uc in wpp:
            band_dst.WriteRaster(xoff=0, yoff=y, xsize=band_src.XSize, ysize=1,
                                 buf_string=scanline_dst)
            progressor.progress(y)
            y += 1
            unknown_codes |= uc
    ds_dst.FlushCache()
    ds_dst = None
    if unknown_codes:
        warning(
            f"Source file contains following unknown land usage codes: "
            f"{', '.join(str(c) for c in sorted(unknown_codes))}. They were "
            f"translated to {yaml_params.default_code}. Consider adding these "
            f"codes to '{os.path.basename(PARAM_FILE_NAME)}'")


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    yaml_params = YamlParams()
    argument_parser = argparse.ArgumentParser(
        description="Converts land cover file to NLCD/WGS84 format",
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
    encoding_names = sorted(yaml_params.encodings.keys())
    argument_parser.add_argument(
        "--encoding", choices=encoding_names,
        help=f"Translate land cover codes from given encoding to NLCD "
        f"encoding. Supported encodings are: {', '.join(encoding_names)}")
    argument_parser.add_argument(
        "--format", metavar="GDAL_DRIVER_NAME",
        help="File format expressed as GDAL driver name (see "
        "https://gdal.org/drivers/raster/index.html ). By default derived "
        "from target file extension")
    argument_parser.add_argument(
        "--format_param", metavar="NAME=VALUE", action="append",
        default=[],
        help="Format option (e.g. COMPRESS=PACKBITS or BIGTIFF=IF_NEEDED). "
        "May be specified several times")
    resampling_methods = warp_resamplings()
    argument_parser.add_argument(
        "--resampling", metavar="METHOD", default=DEFAULT_RESAMPLING,
        choices=resampling_methods,
        help=f"Resampling method. Possible values: {resampling_methods}. "
        f"Default is `{DEFAULT_RESAMPLING}'")
    argument_parser.add_argument(
        "--threads", metavar="COUNT_OR_PERCENT%",
        help="Number of threads to use. If positive - number of threads, if "
        "negative - number of CPU cores NOT to use, if followed by `%%` - "
        "percent of CPU cores. Default is total number of CPU cores")
    argument_parser.add_argument(
        "--nice", action="store_true",
        help="Lower priority of this process and its subprocesses")
    argument_parser.add_argument(
        "--overwrite", action="store_true",
        help="Overwrite target file if it exists")
    argument_parser.add_argument("SRC", help="Source file name")
    argument_parser.add_argument("DST", help="Destination file name")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)
    args = argument_parser.parse_args(argv)

    setup_logging()

    if args.nice:
        nice()

    error_if(not os.path.isfile(args.SRC),
             f"Can't find source file '{args.SRC}'")
    error_if(os.path.isfile(args.DST) and (not args.overwrite),
             f"'{args.DST}' already exists. Specify --overwrite to overwrite")
    error_if(
        (args.encoding is not None) and (not HAS_GDAL),
        "Python GDAL support was not installed. Try to  'pip install gdal' or "
        "run containerized version of this script")
    os.makedirs(
        os.path.dirname(os.path.abspath(
            os.path.expanduser(os.path.expandvars(args.DST)))),
        exist_ok=True)
    boundaries = \
        Boundaries(
            args.SRC, pixel_size=args.pixel_size,
            pixels_per_degree=args.pixels_per_degree,
            top=args.top, bottom=args.bottom, left=args.left, right=args.right,
            round_boundaries_to_degree=True, round_pixels_to_degree=True)
    try:
        temp: Optional[str] = None
        src = args.SRC
        with Durator("Total duration:"):
            if args.encoding is not None:
                with Durator("This stage took"):
                    print("TRANSLATING PIXELS TO NLCD ENCODING")
                    h, temp = \
                        tempfile.mkstemp(suffix=os.path.splitext(args.DST)[1],
                                         dir=os.path.dirname(args.DST) or ".")
                    os.close(h)
                    translate(src=src, dst=temp, yaml_params=yaml_params,
                              translation_name=args.encoding,
                              threads=threads_arg(args.threads))
                    src = temp
            with Durator("This stage took"):
                print("TRANSLATING TO WGS84")
                warp(src=src, dst=args.DST, resampling=args.resampling,
                     top=boundaries.top, bottom=boundaries.bottom,
                     left=boundaries.left, right=boundaries.right,
                     pixel_size_lat=boundaries.pixel_size_lat,
                     pixel_size_lon=boundaries.pixel_size_lon,
                     out_format=args.format, format_params=args.format_param,
                     overwrite=args.overwrite)
    finally:
        if temp:
            os.unlink(temp)


if __name__ == "__main__":
    main(sys.argv[1:])
