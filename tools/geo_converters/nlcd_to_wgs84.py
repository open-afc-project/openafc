#!/usr/bin/env python3
# Converts NLCD file to WGS84 lat/lon format

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

import argparse
import math
import os
import re
import shlex
import struct
import subprocess
import sys
import tempfile
from typing import Any, Dict, List, NamedTuple, Optional, Tuple, Union

try:
    from osgeo import gdal
    HAS_GDAL = True
except ImportError:
    HAS_GDAL = False
    pass


_epilog = """This script expects that GDAL utilities are installed and in PATH.
Also it expects that as part of this installation proj/proj.db is somehow
installed as well (if it is, but gdalwarp still complains - set PROJ_LIB
environment variable to point to it).

Usage examples:
 - Convert CONUS NLCD file nlcd_2019_land_cover_l48_20210604.img that is in map
   projection (Albers conic) to nlcd_2019_land_cover_l48_20210604_resampled.tif
   that uses WGS84 (lat/lon) coordinates. Data resolution set to 1 second (3600
   pixels per degree) as it is comparable to 30 m pixel size in the source
   data, TIFF data compression set to PACKBITS (shrinks file 9 times, no
   performance penalty):
     $ ./nlcd_to_wgs84.py --pixels_per_degree 3600 \\
        --format_param COMPRESS=PACKBITS \\
        nlcd_2019_land_cover_l48_20210604.img \\
        nlcd_2019_land_cover_l48_20210604_resampled.tif
 - Same for Alaska NLCD:
     $ ./nlcd_to_wgs84.py --pixels_per_degree 3600 \\
         --format_param COMPRESS=PACKBITS \\
         NLCD_2016_Land_Cover_AK_20200724.img \\
         NLCD_2016_Land_Cover_AK_20200724_resampled.tif
 - Puerto Rico NLCD from NOAA source (NOAA uses different land usage codes):
     $ /nlcd_to_wgs84.py --pixels_per_degree 3600 --translate noaa \\
        --format_param COMPRESS=PACKBITS \\
        pr_2010_ccap_hr_land_cover20170214.img \\
        pr_2010_ccap_hr_land_cover20170214_resampled.tif
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


class Boundaries:
    """ Map boundaries

    Public attributes:
    top    -- Top latitude in degrees
    bottom -- Bottom latitude in degrees
    left   -- Left longitude in degrees
    right  -- Right longitude in degrees
    """
    def __init__(self, filename: str) -> None:
        """ Constructor

        Arguments:
        filename -- GDAL file name from which to gdalinfo boundaries
        """
        gi = execute(["gdalinfo", filename], return_output=True)
        assert isinstance(gi, str)
        r: Dict[str, Dict[str, float]] = {}
        for prefix, lat_kind, lon_kind in [("Upper Left", "max", "min"),
                                           ("Lower Left", "min", "min"),
                                           ("Upper Right", "max", "max"),
                                           ("Lower Right", "min", "max")]:
            m = re.search(
                prefix + r".+?\(\s*(?P<lon_deg>\d+)d\s*(?P<lon_min>\d+)'"
                r"\s*(?P<lon_sec>\d+\.\d+)\"(?P<lon_hem>[EW]),"
                r"\s*(?P<lat_deg>\d+)d\s*(?P<lat_min>\d+)'\s*"
                r"(?P<lat_sec>\d+\.\d+)\"(?P<lat_hem>[NS])\s*\)", gi)
            error_if(m is None, "Can't determine grid dimensions")
            assert m is not None
            lat = \
                (float(m.group("lat_deg")) +
                 float(m.group("lat_min")) / 60 +
                 float(m.group("lat_sec")) / 3600) * \
                (1 if m.group("lat_hem") == "N" else -1)
            lon = \
                (float(m.group("lon_deg")) +
                 float(m.group("lon_min")) / 60 +
                 float(m.group("lon_sec")) / 3600) * \
                (1 if m.group("lon_hem") == "E" else -1)
            for key, value, kind in [("lat", lat, lat_kind),
                                     ("lon", lon, lon_kind)]:
                r.setdefault(key, {})
                r[key][kind] = \
                    (min if kind == "min" else max)(r[key][kind], value) \
                    if kind in r[key] else value
        self.top = r["lat"]["max"]
        self.bottom = r["lat"]["min"]
        self.left = r["lon"]["min"]
        self.right = r["lon"]["max"]

    def adjust(self) -> None:
        """ Adjust for output NLCD.
        Make right boundaries greater than left, round to whole degrees
        """
        while self.right < self.left:
            self.right += 360
        while (self.right - self.left) >= 360:
            self.right -= 360
        self.top = math.ceil(self.top)
        self.bottom = math.floor(self.bottom)
        self.left = math.floor(self.left)
        self.right = math.ceil(self.right)

    def __str__(self) -> str:
        """ String representation for development purposes """
        return (f"[{abs(self.bottom)}{'N' if self.bottom >= 0 else 'S'} - "
                f"{abs(self.top)}{'N' if self.top >= 0 else 'S'}] X "
                f"[{abs(self.left)}{'E' if self.top >= 0 else 'W'} - "
                f"{abs(self.right)}{'E' if self.right >= 0 else 'W'}]")


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
                    input_format=length*struct_data_type,
                    output_format=length*"B", buffer=length*[0])
            self._contexts[key] = ret
        return ret


class Translation:
    """ Pixel row translator to NLCD encoding

    Private attributes:
    _translation -- Dictionary from some encoding to NLCD encoding
    """
    # Collection of known translations (filled on first call of get())
    _translations: Optional[Dict[str, "Translation"]] = None

    # Default value for NLCD data
    NLCD_DEF_VALUE = 0

    def __init__(self, translation: Dict[int, int]) -> None:
        """ Constructor

        Arguments:
        translation -- Translation from other to NLCD encoding
        """
        self._translation = translation

    def translate(self, input_bytes: bytes, data_type_code: int,
                  translation_contexts: TranslationContexts) -> bytes:
        """ Translate pixel row

        Arguments:
        input_bytes          -- Input raster line byte string
        data_type_code       -- GDAL pixel data type code 
        translation_contexts -- Collection of translation contexts
        Returns translated raster line
        """
        tc = translation_contexts.get_context(input_size=len(input_bytes),
                                              data_type_code=data_type_code)
        input_values = struct.unpack(tc.input_format, input_bytes)
        for idx, v in enumerate(input_values):
            tc.buffer[idx] = self._translation.get(v, self.NLCD_DEF_VALUE)
        return struct.pack(tc.output_format, *tc.buffer)

    @classmethod
    def get(cls, name: str) -> "Translation":
        """ Returns translation object of given name """
        if cls._translations is None:
            cls._translations = {}
            cls._translations["noaa"] = \
                Translation(
                    # NOAA codes to NLCD codes
                    {0: 127,   # Background value
                     1: 0,     # Unclassified
                     2: 24,    # Developed, High Intensity
                     3: 23,    # Developed, Medium Intensity
                     4: 22,    # Developed, Low Intensity
                     5: 21,    # Developed, Open Space
                     6: 82,    # Cultivated Crops
                     7: 81,    # Pasture/Hay
                     8: 71,    # Grassland/Herbaceous
                     9: 41,    # Deciduous Forest
                     10: 42,   # Evergreen Forest
                     11: 43,   # Mixed Forest
                     12: 52,   # Shrub/Scrub
                     13: 90,   # Woody Wetlands
                     14: 52,   # Shrub/Scrub
                     15: 95,   # Emergent Herbaceous Wetlands
                     16: 90,   # Woody Wetlands
                     17: 52,   # Shrub/Scrub
                     18: 95,   # Emergent Herbaceous Wetlands
                     19: 95,   # Emergent Herbaceous Wetlands
                     20: 31,   # Barren Land (Rock/Sand/Clay)
                     21: 11,   # Open Water
                     22: 11,   # Open Water
                     23: 11,   # Open Water
                     24: 12,   # Perennial Ice/Snow
                     25: 12,   # Perennial Ice/Snow
                     26: 51,   # Dwarf Scrub - Alaska
                     27: 72,   # Sedge/Herbaceous - Alaska
                     28: 74})  # Moss - Alaska
        ret = cls._translations.get(name)
        error_if(ret is None, f"Unknown translation name '{name}'")
        assert ret is not None
        return ret


class Progressor:
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
        new_fraction = line * 40 // self._total_lines
        if new_fraction == self._last_fraction:
            return
        self._last_fraction = new_fraction
        print("." if new_fraction % 4 else (new_fraction // 4 * 10), end="",
              flush=True)


def warp(src: str, dst: str, env: Optional[Dict[str, str]],
         boundaries: Optional[Boundaries] = None,
         pixel_size: Optional[float] = None,
         pixels_per_degree: Optional[float] = None,
         out_format: Optional[str] = None,
         format_params: Optional[List[str]] = None,
         center_lon_180: bool = False,
         overwrite: bool = False) -> None:
    """ Perform conversion with gdalwarp

    Arguments:
        src               -- Source file name
        dst               -- Resulting file name
        env               -- Optional environment to use for calling GDAL
                             utilities
        boundaries        -- Optional conversion boundaries
        pixel_size        -- Optional pixel size
        pixels_per_degree -- Optional resolution in pixels per degree
        out_format        -- Optional output format (for cases when it can't be
                             discerned from extension)
        format_params     -- Optional list of format parameters
        center_lon_180    -- True to center result around 180 longitude
        overwrite         -- True to overwrite resulting file
        """
    exec_args: List[str] = ["gdalwarp", "-t_srs", "+proj=longlat +datum=WGS84"]
    if boundaries is not None:
        exec_args += ["-te"] + \
            [str(a) for a in (boundaries.left, boundaries.bottom,
                              boundaries.right, boundaries.top)]
    if pixel_size is not None:
        error_if(pixels_per_degree is not None,
                 "--pixel_size and --pixels_per_degree may not be specified "
                 "together")
        exec_args += ["-tr", str(pixel_size), str(pixel_size)]
    elif pixels_per_degree is not None:
        assert boundaries is not None
        lat_degs = boundaries.top - boundaries.bottom
        lon_degs = boundaries.right - boundaries.left
        exec_args += ["-ts", str(int(lon_degs * pixels_per_degree)),
                      str(int(lat_degs * pixels_per_degree))]
    if out_format:
        exec_args += ["-of", out_format]
    for fp in (format_params or []):
        exec_args += ["-co", fp]
    if center_lon_180:
        exec_args += ["--config", "CENTER_LONG", "180"]
    if overwrite:
        exec_args += ["-overwrite"]
    exec_args += [src, dst]
    execute(exec_args, env=env)

# NLCD color table
NLCD_COLOR_TABLE: Dict[int, Tuple[int, int, int, int]] = \
    {0: (0, 0, 0, 0),
     1: (0, 0, 0, 255),
     2: (0, 0, 0, 255),
     3: (0, 0, 0, 255),
     4: (0, 0, 0, 255),
     5: (0, 0, 0, 255),
     6: (0, 0, 0, 255),
     7: (0, 0, 0, 255),
     8: (0, 0, 0, 255),
     9: (0, 0, 0, 255),
     10: (0, 0, 0, 255),
     11: (70, 107, 159, 255),
     12: (209, 222, 248, 255),
     13: (0, 0, 0, 255),
     14: (0, 0, 0, 255),
     15: (0, 0, 0, 255),
     16: (0, 0, 0, 255),
     17: (0, 0, 0, 255),
     18: (0, 0, 0, 255),
     19: (0, 0, 0, 255),
     20: (0, 0, 0, 255),
     21: (222, 197, 197, 255),
     22: (217, 146, 130, 255),
     23: (235, 0, 0, 255),
     24: (171, 0, 0, 255),
     25: (0, 0, 0, 255),
     26: (0, 0, 0, 255),
     27: (0, 0, 0, 255),
     28: (0, 0, 0, 255),
     29: (0, 0, 0, 255),
     30: (0, 0, 0, 255),
     31: (179, 172, 159, 255),
     32: (0, 0, 0, 255),
     33: (0, 0, 0, 255),
     34: (0, 0, 0, 255),
     35: (0, 0, 0, 255),
     36: (0, 0, 0, 255),
     37: (0, 0, 0, 255),
     38: (0, 0, 0, 255),
     39: (0, 0, 0, 255),
     40: (0, 0, 0, 255),
     41: (104, 171, 95, 255),
     42: (28, 95, 44, 255),
     43: (181, 197, 143, 255),
     44: (0, 0, 0, 255),
     45: (0, 0, 0, 255),
     46: (0, 0, 0, 255),
     47: (0, 0, 0, 255),
     48: (0, 0, 0, 255),
     49: (0, 0, 0, 255),
     50: (0, 0, 0, 255),
     51: (0, 0, 0, 255),
     52: (204, 184, 121, 255),
     53: (0, 0, 0, 255),
     54: (0, 0, 0, 255),
     55: (0, 0, 0, 255),
     56: (0, 0, 0, 255),
     57: (0, 0, 0, 255),
     58: (0, 0, 0, 255),
     59: (0, 0, 0, 255),
     60: (0, 0, 0, 255),
     61: (0, 0, 0, 255),
     62: (0, 0, 0, 255),
     63: (0, 0, 0, 255),
     64: (0, 0, 0, 255),
     65: (0, 0, 0, 255),
     66: (0, 0, 0, 255),
     67: (0, 0, 0, 255),
     68: (0, 0, 0, 255),
     69: (0, 0, 0, 255),
     70: (0, 0, 0, 255),
     71: (223, 223, 194, 255),
     72: (0, 0, 0, 255),
     73: (0, 0, 0, 255),
     74: (0, 0, 0, 255),
     75: (0, 0, 0, 255),
     76: (0, 0, 0, 255),
     77: (0, 0, 0, 255),
     78: (0, 0, 0, 255),
     79: (0, 0, 0, 255),
     80: (0, 0, 0, 255),
     81: (220, 217, 57, 255),
     82: (171, 108, 40, 255),
     83: (0, 0, 0, 255),
     84: (0, 0, 0, 255),
     85: (0, 0, 0, 255),
     86: (0, 0, 0, 255),
     87: (0, 0, 0, 255),
     88: (0, 0, 0, 255),
     89: (0, 0, 0, 255),
     90: (184, 217, 235, 255),
     91: (0, 0, 0, 255),
     92: (0, 0, 0, 255),
     93: (0, 0, 0, 255),
     94: (0, 0, 0, 255),
     95: (108, 159, 184, 255),
     96: (96, 96, 96, 255),
     97: (97, 97, 97, 255),
     98: (98, 98, 98, 255),
     99: (99, 99, 99, 255),
     100: (100, 100, 100, 255),
     101: (101, 101, 101, 255),
     102: (102, 102, 102, 255),
     103: (103, 103, 103, 255),
     104: (104, 104, 104, 255),
     105: (105, 105, 105, 255),
     106: (106, 106, 106, 255),
     107: (107, 107, 107, 255),
     108: (108, 108, 108, 255),
     109: (109, 109, 109, 255),
     110: (110, 110, 110, 255),
     111: (111, 111, 111, 255),
     112: (112, 112, 112, 255),
     113: (113, 113, 113, 255),
     114: (114, 114, 114, 255),
     115: (115, 115, 115, 255),
     116: (116, 116, 116, 255),
     117: (117, 117, 117, 255),
     118: (118, 118, 118, 255),
     119: (119, 119, 119, 255),
     120: (120, 120, 120, 255),
     121: (121, 121, 121, 255),
     122: (122, 122, 122, 255),
     123: (123, 123, 123, 255),
     124: (124, 124, 124, 255),
     125: (125, 125, 125, 255),
     126: (126, 126, 126, 255),
     127: (127, 127, 127, 255),
     128: (128, 128, 128, 255),
     129: (129, 129, 129, 255),
     130: (130, 130, 130, 255),
     131: (131, 131, 131, 255),
     132: (132, 132, 132, 255),
     133: (133, 133, 133, 255),
     134: (134, 134, 134, 255),
     135: (135, 135, 135, 255),
     136: (136, 136, 136, 255),
     137: (137, 137, 137, 255),
     138: (138, 138, 138, 255),
     139: (139, 139, 139, 255),
     140: (140, 140, 140, 255),
     141: (141, 141, 141, 255),
     142: (142, 142, 142, 255),
     143: (143, 143, 143, 255),
     144: (144, 144, 144, 255),
     145: (145, 145, 145, 255),
     146: (146, 146, 146, 255),
     147: (147, 147, 147, 255),
     148: (148, 148, 148, 255),
     149: (149, 149, 149, 255),
     150: (150, 150, 150, 255),
     151: (151, 151, 151, 255),
     152: (152, 152, 152, 255),
     153: (153, 153, 153, 255),
     154: (154, 154, 154, 255),
     155: (155, 155, 155, 255),
     156: (156, 156, 156, 255),
     157: (157, 157, 157, 255),
     158: (158, 158, 158, 255),
     159: (159, 159, 159, 255),
     160: (160, 160, 160, 255),
     161: (161, 161, 161, 255),
     162: (162, 162, 162, 255),
     163: (163, 163, 163, 255),
     164: (164, 164, 164, 255),
     165: (165, 165, 165, 255),
     166: (166, 166, 166, 255),
     167: (167, 167, 167, 255),
     168: (168, 168, 168, 255),
     169: (169, 169, 169, 255),
     170: (170, 170, 170, 255),
     171: (171, 171, 171, 255),
     172: (172, 172, 172, 255),
     173: (173, 173, 173, 255),
     174: (174, 174, 174, 255),
     175: (175, 175, 175, 255),
     176: (176, 176, 176, 255),
     177: (177, 177, 177, 255),
     178: (178, 178, 178, 255),
     179: (179, 179, 179, 255),
     180: (180, 180, 180, 255),
     181: (181, 181, 181, 255),
     182: (182, 182, 182, 255),
     183: (183, 183, 183, 255),
     184: (184, 184, 184, 255),
     185: (185, 185, 185, 255),
     186: (186, 186, 186, 255),
     187: (187, 187, 187, 255),
     188: (188, 188, 188, 255),
     189: (189, 189, 189, 255),
     190: (190, 190, 190, 255),
     191: (191, 191, 191, 255),
     192: (192, 192, 192, 255),
     193: (193, 193, 193, 255),
     194: (194, 194, 194, 255),
     195: (195, 195, 195, 255),
     196: (196, 196, 196, 255),
     197: (197, 197, 197, 255),
     198: (198, 198, 198, 255),
     199: (199, 199, 199, 255),
     200: (200, 200, 200, 255),
     201: (201, 201, 201, 255),
     202: (202, 202, 202, 255),
     203: (203, 203, 203, 255),
     204: (204, 204, 204, 255),
     205: (205, 205, 205, 255),
     206: (206, 206, 206, 255),
     207: (207, 207, 207, 255),
     208: (208, 208, 208, 255),
     209: (209, 209, 209, 255),
     210: (210, 210, 210, 255),
     211: (211, 211, 211, 255),
     212: (212, 212, 212, 255),
     213: (213, 213, 213, 255),
     214: (214, 214, 214, 255),
     215: (215, 215, 215, 255),
     216: (216, 216, 216, 255),
     217: (217, 217, 217, 255),
     218: (218, 218, 218, 255),
     219: (219, 219, 219, 255),
     220: (220, 220, 220, 255),
     221: (221, 221, 221, 255),
     222: (222, 222, 222, 255),
     223: (223, 223, 223, 255),
     224: (224, 224, 224, 255),
     225: (225, 225, 225, 255),
     226: (226, 226, 226, 255),
     227: (227, 227, 227, 255),
     228: (228, 228, 228, 255),
     229: (229, 229, 229, 255),
     230: (230, 230, 230, 255),
     231: (231, 231, 231, 255),
     232: (232, 232, 232, 255),
     233: (233, 233, 233, 255),
     234: (234, 234, 234, 255),
     235: (235, 235, 235, 255),
     236: (236, 236, 236, 255),
     237: (237, 237, 237, 255),
     238: (238, 238, 238, 255),
     239: (239, 239, 239, 255),
     240: (240, 240, 240, 255),
     241: (241, 241, 241, 255),
     242: (242, 242, 242, 255),
     243: (243, 243, 243, 255),
     244: (244, 244, 244, 255),
     245: (245, 245, 245, 255),
     246: (246, 246, 246, 255),
     247: (247, 247, 247, 255),
     248: (248, 248, 248, 255),
     249: (249, 249, 249, 255),
     250: (250, 250, 250, 255),
     251: (251, 251, 251, 255),
     252: (252, 252, 252, 255),
     253: (253, 253, 253, 255),
     254: (254, 254, 254, 255),
     255: (255, 255, 255, 255)}


def translate(src: str, dst: str, translation_name: str) -> None:
    """ Translate pixel codes to NLCD encoding

    Arguments:
    src              -- Source file name
    dst              -- Result file name (GeoTiff, no special parameters)
    translation_name -- Name of translation
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
    for index, color in NLCD_COLOR_TABLE.items():
        color_table_dst.SetColorEntry(index, color)
    band_dst.SetRasterColorTable(color_table_dst)
    band_dst.SetRasterColorInterpretation(gdal.GCI_PaletteIndex)
    band_dst.SetNoDataValue(Translation.NLCD_DEF_VALUE)
    translation = Translation.get(translation_name)
    contexts = TranslationContexts()
    progressor = Progressor(filename=src, total_lines=band_src.YSize)
    for y in range(band_src.YSize):
        scanline_src = \
            band_src.ReadRaster(xoff=0, yoff=y, xsize=band_src.XSize, ysize=1)
        scanline_dst = \
            translation.translate(
                input_bytes=scanline_src, data_type_code=band_src.DataType,
                translation_contexts=contexts)
        band_dst.WriteRaster(xoff=0, yoff=y, xsize=band_src.XSize, ysize=1,
                             buf_string=scanline_dst)
        progressor.progress(y)
    ds_dst.FlushCache()
    ds_dst = None


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = argparse.ArgumentParser(
        description="Converts NLCD file to WGS84 format",
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=_epilog)
    argument_parser.add_argument(
        "--pixel_size", metavar="DEGREES", type=float,
        help="Resulting file resolution in pixel size (expressed in degrees). "
        "May be omitted if source file is in geodetic (latitude/longitude) "
        "format and no resampling required")
    argument_parser.add_argument(
        "--pixels_per_degree", "-p", metavar="NUMBER", type=float,
        help="Resulting file resolution in pixels per degree. May be omitted "
        "if source file is in geodetic (latitude/longitude) format and no "
        "resampling required")
    argument_parser.add_argument(
        "--translate", "-t", choices=["noaa"],
        help="Translate land cover codes from given encoding to NLCD "
        "encoding. For now 'noaa' source encoding is supported")
    argument_parser.add_argument(
        "--format", "-f", metavar="GDAL_DRIVER_NAME",
        help="File format expressed as GDAL driver name (see "
        "https://gdal.org/drivers/raster/index.html ). By default derived "
        "from target file extension")
    argument_parser.add_argument(
        "--format_param", "-P", metavar="NAME=VALUE", action="append",
        default=[],
        help="Format option (e.g. COMPRESS=PACKBITS)")
    argument_parser.add_argument(
        "--overwrite", "-o", action="store_true",
        help="Overwrite target file if it exists")
    argument_parser.add_argument(
        "--temp_dir", "-T", metavar="TEMPDIR",
        help="Directory to create temporary files in. Might be useful if this "
        "script runs from container that has insufficient internal disk "
        "space. Default is to use default system temporary directory")
    argument_parser.add_argument("SRC", help="Source file name")
    argument_parser.add_argument("DST", help="Destination file name")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)
    args = argument_parser.parse_args(argv)

    error_if(not os.path.isfile(args.SRC),
             f"Can't find source file '{args.SRC}'")
    error_if(os.path.isfile(args.DST) and (not args.overwrite),
             f"'{args.DST}' already exists. Specify --overwrite to overwrite")
    error_if(
        (args.translate is not None) and (not HAS_GDAL),
        "Python GDAL support was not installed. Try to 'pip install gdal'")
    boundaries = Boundaries(args.SRC)
    env = gdalwarp_env()
    try:
        h, temp = tempfile.mkstemp(suffix=os.path.splitext(args.DST)[1],
                                   dir=args.temp_dir)
        os.close(h)
        print("DETERMINING RASTER SIZE")
        warp(src=args.SRC, dst=temp, env=env, out_format=args.format,
             format_params=args.format_param,
             center_lon_180=boundaries.right < boundaries.left, overwrite=True)
        src = args.SRC
        if args.translate is not None:
            print("TRANSLATING PIXELS TO NLCD ENCODING")
            translate(src=src, dst=temp, translation_name=args.translate)
            src = temp
        boundaries = Boundaries(temp)
        boundaries.adjust()
        print("TRANSLATING TO WGS84")
        warp(src=src, dst=args.DST, env=env, boundaries=boundaries,
             pixel_size=args.pixel_size,
             pixels_per_degree=args.pixels_per_degree, out_format=args.format,
             format_params=args.format_param, overwrite=args.overwrite)
    finally:
        if os.path.isfile(temp):
            os.unlink(temp)


if __name__ == "__main__":
    main(sys.argv[1:])
