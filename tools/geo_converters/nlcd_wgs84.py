#!/usr/bin/env python3
# Converts land cover file to NLCD/WGS84 format

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

import argparse
from collections.abc import Callable, Iterable, Iterator
import datetime
import math
import multiprocessing
import os
import re
import shlex
import signal
import struct
import subprocess
import sys
import tempfile
from typing import Any, Dict, Generic, List, NamedTuple, Optional, Set, \
    Tuple, TypeVar, Union
import yaml

try:
    from osgeo import gdal
    HAS_GDAL = True
except ImportError:
    HAS_GDAL = False

# Parameter YAML file name
PARAM_FILE_NAME = os.path.splitext(__file__)[0] + ".yaml"

# Length of color table in translated file
TARGET_COLOR_TABLE_LENGTH = 256


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


# Source data type for WindowedProcessPool processor
WppSrc = TypeVar("WppSrc")
# Result data type for WindowedProcessPool processor
WppResult = TypeVar("WppResult")

# Data type for task queue
ProcQueueTask = NamedTuple("ProcQueueTask", [("idx", int), ("data", Any)])

# Data type for results queue
ProcQueueResult = NamedTuple("ProcQueueResult", [("idx", int), ("data", Any)])


class WindowedProcessPool(Iterable[WppResult], Generic[WppSrc, WppResult]):
    """ Kind of multiprocessing pool that yields results in the same order as
    they were produced and maintain a limited number of data items in flight
    (when already processed but not yet consumed).
    Yields data via iteration
    Must be used as context manager (with 'with')

    Priovate attributes:
    _producer        -- Iterable that produces source data items
    _processor       -- Function that produces result from the source data
    _max_in_flight   -- Maximum number of produced but not yet consumed data
                        items
    _task_queue      -- Queue with source data for processing. Contains
                        ProcQueueTask objects or None (to signal worker to
                        stop)
    _results_queue   -- Queue with processing results. Contains ProcQueueResult
                        objects
    _result_dict     -- Dictionary that stores received processed results,
                        indexed by their sequential indices
    _next_src_idx    -- Sequential index of next produced data item
    _next_result_idx -- Sequential index of next result to be consumed
    _processes       -- List of worker processes
    _running         -- True if worker processes running (not yet terminated)
    """

    def __init__(self, producer: Iterable[WppSrc],
                 processor: Callable[[WppSrc], WppResult],
                 max_in_flight: int) -> None:
        """ Constructor

        Arguments:
        producer      -- Iterable that produces source data items
        processor     -- Function that produces result from the source data
        max_in_flight -- Maximum number of produced but not yet consumed data
                         items
        """
        self._producer = producer
        self._processor = processor
        self._max_in_flight = max_in_flight
        self._task_queue: multiprocessing.Queue[Optional[ProcQueueTask]] = \
            multiprocessing.Queue()
        self._results_queue: multiprocessing.Queue[ProcQueueResult] = \
            multiprocessing.Queue()
        self._result_dict: Dict[int, WppResult] = {}
        self._next_src_idx = 0
        self._next_result_idx = 0
        original_sigint_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
        self._processes: List[multiprocessing.Process] = []
        for _ in range(multiprocessing.cpu_count()):
            self._processes.append(
                multiprocessing.Process(
                    target=self.__class__._processor_wrapper,
                    name="WindowedProcessPoolWorker",
                    kwargs={"task_queue": self._task_queue,
                            "results_queue": self._results_queue,
                            "processor": self._processor}))
            self._processes[-1].start()
        signal.signal(signal.SIGINT, original_sigint_handler)
        self._running = True

    def __enter__(self) -> "WindowedProcessPool":
        """ Context entry """
        return self

    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
        """ Context exit """
        self._terminate()

    def __iter__(self) -> Iterator[WppResult]:
        """ Iteration over processed results """
        prod_next = self._producer.__iter__().__next__
        exhausted = False
        while (not exhausted) or (self._next_src_idx > self._next_result_idx):
            # Filling task queue
            while (not exhausted) and \
                ((self._next_src_idx - self._next_result_idx) <
                 self._max_in_flight):
                try:
                    src: WppSrc = prod_next()
                    self._task_queue.put(
                        ProcQueueTask(idx=self._next_src_idx, data=src))
                    self._next_src_idx += 1
                except StopIteration:
                    exhausted = True
            # Draining result queue
            while (self._next_result_idx not in self._result_dict) or \
                    (not self._results_queue.empty()):
                queue_result: ProcQueueResult = \
                    self._results_queue.get(block=True)
                self._result_dict[queue_result.idx] = queue_result.data
            # Popping next processed line
            result: WppResult = self._result_dict[self._next_result_idx]
            del self._result_dict[self._next_result_idx]
            self._next_result_idx += 1
            yield result
        self._terminate()

    def _terminate(self) -> None:
        """ Terminate worker processes """
        if not self._running:
            return
        self._running = False
        for _ in range(len(self._processes)):
            self._task_queue.put(None)
        for proc in self._processes:
            proc.join()

    @classmethod
    def _processor_wrapper(cls, task_queue: multiprocessing.Queue,
                           results_queue: multiprocessing.Queue,
                           processor: Callable[[WppSrc], WppResult]) -> None:
        """ Worker process

        Arguments:
        task_queue    -- Queue for data to process (ProcQueueTask objects or
                         Nones to stop)
        results_queue -- Queue for processing results
        processor     -- Processing function
        """
        while True:
            task: Optional[ProcQueueTask] = task_queue.get(block=True)
            if task is None:
                return
            results_queue.put(
                ProcQueueResult(idx=task.idx, data=processor(task.data)))


class YamlParams:
    """ Parameters stored in accompanying YAML file (mostly encoding-related)

    Public attrinutes:
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
            return
        new_fraction = line * 40 // self._total_lines
        if new_fraction == self._last_fraction:
            return
        self._last_fraction = new_fraction
        print("." if new_fraction % 4 else (new_fraction // 4 * 10), end="",
              flush=True)


class Durator:
    """ Prints operation duration.
    This class intended to be used as context manager (with 'with')

    Private attributes:
    _heading    -- End message heading
    _start_time -- Operation start time

    """
    def __init__(self, heading: str) -> None:
        """ Constructor

        Arguments:
        heading -- End message heading
        """
        self._start_time = datetime.datetime.now()
        self._heading = heading

    def __enter__(self) -> None:
        """ Context entry """
        pass

    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
        """ Context exit """
        duration = \
            int((datetime.datetime.now() - self._start_time).total_seconds())
        seconds = duration % 60
        duration //= 60
        minutes = duration % 60
        duration //= 60
        hours = duration
        msg = self._heading
        if hours:
            msg += f" {hours} h"
        if hours or minutes:
            msg += f" {minutes} m"
        msg += f" {seconds}s"
        print(msg)


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


def translate(src: str, dst: str, yaml_params: YamlParams,
              translation_name: str) -> None:
    """ Translate pixel codes to NLCD encoding

    Arguments:
    src              -- Source file name
    dst              -- Result file name (GeoTiff, no special parameters)
    yaml_params      -- Parameters from accompanying YAML file
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
    progressor = Progressor(filename=src, total_lines=band_src.YSize)

    def scan_line_producer() -> Iterable[bytes]:
        """ Produces scan lines from the source file """
        for y in range(band_src.YSize):
            yield band_src.ReadRaster(xoff=0, yoff=y,
                                      xsize=band_src.XSize, ysize=1)

    unknown_codes: Set[int] = set()
    with WindowedProcessPool(producer=scan_line_producer(),
                             processor=translation.translate,
                             max_in_flight=100) as wpp:
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
    encoding_names = sorted(yaml_params.encodings.keys())
    argument_parser.add_argument(
        "--encoding", "-t", choices=encoding_names,
        help=f"Translate land cover codes from given encoding to NLCD "
        f"encoding. Supported encodings are: {', '.join(encoding_names)}")
    argument_parser.add_argument(
        "--format", "-f", metavar="GDAL_DRIVER_NAME",
        help="File format expressed as GDAL driver name (see "
        "https://gdal.org/drivers/raster/index.html ). By default derived "
        "from target file extension")
    argument_parser.add_argument(
        "--format_param", "-P", metavar="NAME=VALUE", action="append",
        default=[],
        help="Format option (e.g. COMPRESS=PACKBITS or BIGTIFF=YES)")
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
        (args.encoding is not None) and (not HAS_GDAL),
        "Python GDAL support was not installed. Try to 'pip install gdal'")
    boundaries = Boundaries(args.SRC)
    env = gdalwarp_env()
    try:
        h, temp = tempfile.mkstemp(suffix=os.path.splitext(args.DST)[1],
                                   dir=args.temp_dir)
        os.close(h)
        with Durator("Total duration:"):
            with Durator("This stage took"):
                print("DETERMINING RASTER BOUNDARIES")
                warp(src=args.SRC, dst=temp, env=env, out_format=args.format,
                     format_params=args.format_param,
                     center_lon_180=boundaries.right < boundaries.left,
                     overwrite=True)
                src = args.SRC
            boundaries = Boundaries(temp)
            boundaries.adjust()
            if args.encoding is not None:
                with Durator("This stage took"):
                    print("TRANSLATING PIXELS TO NLCD ENCODING")
                    translate(src=src, dst=temp, yaml_params=yaml_params,
                              translation_name=args.encoding)
                    src = temp
            with Durator("This stage took"):
                print("TRANSLATING TO WGS84")
                warp(src=src, dst=args.DST, env=env, boundaries=boundaries,
                     pixel_size=args.pixel_size,
                     pixels_per_degree=args.pixels_per_degree,
                     out_format=args.format, format_params=args.format_param,
                     overwrite=args.overwrite)
    finally:
        if os.path.isfile(temp):
            os.unlink(temp)


if __name__ == "__main__":
    main(sys.argv[1:])
