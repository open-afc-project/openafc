# Utility stuff for geospatial scripts

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=invalid-name, wrong-import-order
# pylint: disable=too-many-instance-attributes, too-few-public-methods
# pylint: disable=too-many-arguments, unnecessary-pass, too-many-locals
# pylint: disable=too-many-statements, too-many-branches, too-many-lines
import __main__
from collections.abc import Callable, Iterable, Iterator, Sequence
import datetime
import glob
import inspect
import math
import logging
import multiprocessing
import os
import re
import shlex
import signal
import subprocess
import sys
import tempfile
from typing import Any, Dict, Generic, List, NamedTuple, Optional, Tuple, \
    TypeVar, Union

try:
    import psutil
    HAS_PSUTIL = True
except ImportError:
    HAS_PSUTIL = False


# Default scale for Int16/Int32 -> PNG UInt16
DEFAULT_INT_PNG_SCALE = [-1000., 0., 0., 1000.]

# Default scale for Int16/Int32 -> PNG UInt16
DEFAULT_FLOAT_PNG_SCALE = [-1000., 0., 0., 5000.]

# Default UInt16 PNG NoData value
DEFAULT_PNG_NO_DATA = 65535

# PNG driver name
PNG_FORMAT = "PNG"

# PNG file extension
PNG_EXT = ".png"


def dp(*args, **kwargs) -> None:  # pylint: disable=invalid-name
    """Print debug message

    Arguments:
    args   -- Format and positional arguments. If latter present - formatted
              with %
    kwargs -- Keyword arguments. If present formatted with format()
    """
    msg = args[0] if args else ""
    if len(args) > 1:
        msg = msg % args[1:]
    if args and kwargs:
        msg = msg.format(**kwargs)
    cur_frame = inspect.currentframe()
    assert (cur_frame is not None) and (cur_frame.f_back is not None)
    frameinfo = inspect.getframeinfo(cur_frame.f_back)
    print(f"DP {frameinfo.function}()@{frameinfo.lineno}: {msg}")


def setup_logging() -> None:
    """ Set up logging """
    # Set up logging
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(
        logging.Formatter(
            f"{os.path.basename(__main__.__file__)}. "
            f"%(levelname)s: %(message)s"))
    logging.getLogger().addHandler(console_handler)
    logging.getLogger().setLevel(logging.INFO)


def error(msg: str) -> None:
    """ Prints given msg as error message and exit abnormally """
    logging.error(msg)
    sys.exit(1)


def error_if(cond: Any, msg: str) -> None:
    """ If condition evaluates to true prints given msg as error message and
    exits abnormally """
    if cond:
        error(msg)


def warning(warnmsg: str) -> None:
    """ Print given warning message """
    logging.warning(warnmsg)


def execute(args: List[str], env: Optional[Dict[str, str]] = None,
            return_output: bool = False, fail_on_error: bool = True,
            return_error: bool = False, disable_output: bool = False,
            ignore_exit_code: bool = False) \
        -> Union[bool, Optional[str]]:
    """ Execute command

    Arguments:
    args             -- List of command line parts
    env              -- Optional environment dictionary
    return_output    -- True to return output and not print it, False to print
                        output and not return it
    fail_on_error    -- True to fail on error, false to return None/False
    return_error     -- Return stderr on failure, None on success
    disable_output   -- True to disable all output
    ignore_exit_code -- Assume success even though exit code is nonzero
    Returns True/stdout on success, False/None on error
    """
    if not (return_output or disable_output):
        print(" ".join(shlex.quote(a) for a in args))
    try:
        p = subprocess.run(
            args, text=True, env=env, check=False,
            stdout=subprocess.PIPE if (return_output or disable_output)
            else None,
            stderr=subprocess.PIPE if (return_output and (not fail_on_error))
            or return_error or disable_output
            else None)
        if (not p.returncode) or ignore_exit_code:
            if return_output:
                return p.stdout
            if return_error:
                return None
            return True
        if fail_on_error:
            if p.stderr is not None:
                print(p.stderr)
            sys.exit(p.returncode)
        errmsg = p.stderr
    except OSError as ex:
        if fail_on_error:
            error(f"{ex}")
        errmsg = str(ex)
    if return_output:
        return None
    if return_error:
        return errmsg
    return False


def gdal_env() -> Optional[Dict[str, str]]:
    """ Returns environment required to run GDAL utilities. None if not
    needed """
    if os.name != "nt":
        return None
    if "PROJ_LIB" in os.environ:
        return None
    gw = execute(["where", "gdalwarp"], return_output=True,
                 fail_on_error=False)
    error_if(gw is None, "'gdalwarp' not found")
    assert isinstance(gw, str)
    gw = gw.strip().split("\n")[0]
    ret = dict(os.environ)
    for path_to_proj in ("projlib", r"..\share\proj"):
        proj_path = os.path.abspath(os.path.join(os.path.dirname(gw.strip()),
                                                 path_to_proj))
        if os.path.isfile(os.path.join(proj_path, "proj.db")):
            ret["PROJ_LIB"] = proj_path
            break
    else:
        warning("Projection library directory not found. GDAL utilities may "
                "refuse to work properly")
    return ret


class GdalInfo:
    """ gdalinfo information from file

    Public attributes:
    raw            -- Raw 'gdalinfo' output. None on gdalinfo fail
    top            -- Top latitude in north-positive degrees or None
    bottom         -- Bottom latitude in north-positive degrees or None
    left           -- Left longitude in east-positive degrees or None
    right          -- Right longitude in north-positive degrees or None
    pixel_size_lat -- Pixel size in latitudinal direction in degrees or None
    pixel_size_lon -- Pixel size in longitudinal direction in degrees or None
    is_geodetic    -- True for geodetic coordinate system, False for map
                      projection coordinate system, None if unknown
    data_types     -- List of per-band data type names
    proj_string    -- Proj-4 coordinate system definition or None
    valid_percent  -- Maximum valid (not 'no data') percent among bands or None
    min_value      -- Minimum value among all bands or None
    max_value      -- Maximum value among all bands or None
    """

    def __init__(self, filename: str,
                 options: Optional[Union[str, List[str]]] = None,
                 fail_on_error: bool = True, remove_aux: bool = False) -> None:
        """ Constructor
        filename      -- Name of file to inspect
        options       -- None or additional option or list of additional
                         options, list of additional gdalinfo options
        fail_on_error -- True to fail on error
        remove_aux    -- True to remove created .aux.xml file
        """
        self.raw: Optional[str] = None
        self.top: Optional[float] = None
        self.bottom: Optional[float] = None
        self.left: Optional[float] = None
        self.right: Optional[float] = None
        self.pixel_size_lat: Optional[float] = None
        self.pixel_size_lon: Optional[float] = None
        self.is_geodetic: Optional[bool] = None
        self.proj_string: Optional[str] = None
        self.valid_percent: Optional[float] = None
        self.min_value: Optional[float] = None
        self.max_value: Optional[float] = None
        self.data_types: List[str] = []
        if isinstance(options, str):
            options = [options]
        ret = execute(["gdalinfo"] + (options or []) + [filename],
                      env=gdal_env(), fail_on_error=fail_on_error,
                      return_output=True, disable_output=not fail_on_error)
        if remove_aux and os.path.isfile(filename + ".aux.xml"):
            try:
                os.unlink(filename + ".aux.xml")
            except OSError:
                pass
        if ret is None:
            return
        assert isinstance(ret, str)
        self.raw = ret
        r: Dict[str, Dict[str, float]] = {}
        for prefix, lat_kind, lon_kind in [("Upper Left", "max", "min"),
                                           ("Lower Left", "min", "min"),
                                           ("Upper Right", "max", "max"),
                                           ("Lower Right", "min", "max")]:
            m = re.search(
                prefix + r".+?\(\s*(?P<lon_deg>\d+)d\s*(?P<lon_min>\d+)'"
                r"\s*(?P<lon_sec>\d+\.\d+)\"(?P<lon_hem>[EW]),"
                r"\s*(?P<lat_deg>\d+)d\s*(?P<lat_min>\d+)'\s*"
                r"(?P<lat_sec>\d+\.\d+)\"(?P<lat_hem>[NS])\s*\)", self.raw)
            if m is None:
                continue
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
        for attr, latlon, minmax in \
                [("top", "lat", "max"), ("bottom", "lat", "min"),
                 ("left", "lon", "min"), ("right", "lon", "max")]:
            setattr(self, attr, r.get(latlon, {}).get(minmax))

        if "CS[ellipsoidal" in self.raw:
            self.is_geodetic = True
        elif "CS[Cartesian" in self.raw:
            self.is_geodetic = False

        m = re.search(r"Pixel Size\s*=\s*\(\s*([0-9.-]+),\s*([0-9.-]+)\)",
                      self.raw)
        if m is not None:
            self.pixel_size_lat = abs(float(m.group(2)))
            self.pixel_size_lon = abs(float(m.group(1)))

        m = re.search(r"PROJ\.4 string is:\s*'(.+?)'", self.raw)
        if m is not None:
            self.proj_string = m.group(1)

        for row, attr, use_max in \
                [("STATISTICS_VALID_PERCENT", "valid_percent", True),
                 ("STATISTICS_MINIMUM", "min_value", False),
                 ("STATISTICS_MAXIMUM", "max_value", True)]:
            for vps in re.findall(r"(?<=%s=)([0-9.eE]+)" % row, self.raw):
                try:
                    read_value = float(vps)
                    current_value = getattr(self, attr)
                    if (current_value is None) or \
                            (use_max and (read_value > current_value)) or \
                            ((not use_max) and (read_value < current_value)):
                        setattr(self, attr, read_value)
                except ValueError:
                    pass
        self.data_types = re.findall(r"Band\s+\d+\s+.*Type=(\w+)", self.raw)

    def __bool__(self):
        return self.raw is not None


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

    Private attributes:
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
                 max_in_flight: int, threads: Optional[int] = None) -> None:
        """ Constructor

        Arguments:
        producer      -- Iterable that produces source data items
        processor     -- Function that produces result from the source data
        max_in_flight -- Maximum number of produced but not yet consumed data
                         items
        threads       -- None or number of threads to use
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
        for _ in range(threads or multiprocessing.cpu_count()):
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
        if exc_val is not None:
            return
        duration = datetime.datetime.now() - self._start_time
        print(f"{self._heading} {self.__class__.duration_to_hms(duration)}")

    @classmethod
    def duration_to_hms(cls, duration: datetime.timedelta) -> str:
        """ Converts duration to printable representation """
        total_seconds = int(duration.total_seconds())
        seconds = total_seconds % 60
        total_seconds //= 60
        minutes = total_seconds % 60
        total_seconds //= 60
        hours = total_seconds
        ret = ""
        if hours:
            ret += f" {hours}h"
        if hours or minutes:
            ret += f" {minutes}m"
        ret += f" {seconds}s"
        return ret[1:]


def warp_resamplings() -> List[str]:
    """ List of supported resampling methods """
    warp_help = \
        execute(["gdalwarp", "--help"], env=gdal_env(), return_output=True,
                ignore_exit_code=True)
    assert isinstance(warp_help, str)
    m = re.search(r"Available resampling methods:\s*((\n\s+\S+.*)+)",
                  warp_help)
    error_if(not m, "Can't fetch list of available resampling methods")
    assert m is not None
    methods = m.group(1)
    for remove in (r"\s", r"\(.*?\)", r"\."):
        methods = re.sub(remove, "", methods)
    return methods.split(",")


def translate_datatypes() -> List[str]:
    """ List of data types, supported by gdal_translate """
    trans_help = \
        execute(["gdal_translate", "--help"], env=gdal_env(),
                return_output=True, ignore_exit_code=True)
    assert isinstance(trans_help, str)
    m = re.search(r"-ot\s.*?{((.|\n)+?)\}", trans_help)
    error_if(not m, "Can't fetch list of available data types")
    assert m is not None
    return re.split(r"\W+", m.group(1))


def warp(src: str, dst: str, resampling: str,
         top: Optional[float] = None,
         bottom: Optional[float] = None,
         left: Optional[float] = None,
         right: Optional[float] = None,
         pixel_size_lat: Optional[float] = None,
         pixel_size_lon: Optional[float] = None,
         src_geoid: Optional[str] = None,
         dst_geoid: Optional[str] = None,
         out_format: Optional[str] = None,
         format_params: Optional[List[str]] = None,
         data_type: Optional[str] = None,
         center_lon_180: bool = False,
         overwrite: bool = False,
         quiet: bool = False) -> Tuple[bool, Optional[str]]:
    """ Perform conversion with gdalwarp

    Arguments:
    src            -- Source file name
    dst            -- Resulting file name
    resampling     -- Resampling method (value for gdalwarp's -r)
    top            -- Optional maximum latitude (for cropping)
    bottom         -- Optional minimum latitude (for cropping)
    left           -- Optional minimum longitude (for cropping)
    right          -- Optional maximum longitude (for cropping)
    pixel_size_lat -- Optional pixel size in latitudinal direction
    pixel_size_lon -- Optional pixel size in longitudinal direction
    src_geoid      -- Optional source geoid file name
    dst_geoid      -- Optional destination geoid file name
    out_format     -- Optional output format (for cases when it can't be
                      discerned from extension)
    format_params  -- Optional list of format parameters
    data_type      -- Optional name of data type in resulting file
    center_lon_180 -- True to center result around 180 longitude
    overwrite      -- True to overwrite resulting file
    quiet          -- True to print nothing but return messages as part of
                      return tuple and also to return negative success status
                      on failure. False to print everything that needs to be
                      and fail on failure
    Returns (success, msg) tuple. 'msg' is None if quiet is False
    """
    exec_args: List[str] = ["gdalwarp", "-r", resampling]

    if src_geoid:
        gi = GdalInfo(src, options="-proj4", fail_on_error=not quiet)
        if not gi:
            return (False, f"{src}: gdalinfo inspection failed")
        if gi.proj_string is None:
            err_msg = f"{src}: can't retrieve projection information"
            error_if(not quiet, err_msg)
            return (False, err_msg)
        exec_args += ["-s_srs", f"{gi.proj_string} +geoidgrids={src_geoid}"]

    exec_args += ["-t_srs",
                  f"+proj=longlat +datum=WGS84"
                  f"{(' +geoidgrids=' + dst_geoid) if dst_geoid else ''}"]

    error_if(not ((top is None) == (bottom is None) == (left is None) ==
                  (right is None)),
             "Target boundaries defined partially. They should either be "
             "defined fully or not at all")
    if left is not None:
        assert right is not None
        while right <= left:
            right += 360
        while (right - left) > 360:
            right -= 360
        exec_args += ["-te", str(left), str(bottom),
                      str(right), str(top)]

    error_if(not ((pixel_size_lat is None) == (pixel_size_lon is None)),
             "Pixel sizes defined partially. They should either be defined "
             "fully or not at all")
    if pixel_size_lat is not None:
        exec_args += ["-tr", str(pixel_size_lon), str(pixel_size_lat)]

    if out_format:
        exec_args += ["-of", out_format]
    for fp in (format_params or []):
        exec_args += ["-co", fp]
    if data_type:
        exec_args += ["-ot", data_type]
    if center_lon_180:
        exec_args += ["--config", "CENTER_LONG", "180"]
    if overwrite:
        exec_args += ["-overwrite"]
    exec_args += [src, dst]

    ret = execute(exec_args, env=gdal_env(), disable_output=quiet,
                  return_error=quiet)
    if quiet:
        assert not isinstance(ret, bool)
        return \
            (ret is None,
             " ".join(shlex.quote(a) for a in exec_args) +
             ("" if ret is None else ("\n" + ret)))
    return (True, None)


def threads_arg(arg: Optional[str]) -> int:
    """ Returns thread count for given threads parameter """
    total = multiprocessing.cpu_count()
    if arg is None:
        return total
    try:
        if arg.endswith("%"):
            ret = int(float(arg[:-1]) * total / 100)
        else:
            ret = int(arg)
            if ret < 0:
                ret = total + ret
        return max(1, min(total, ret))
    except ValueError as ex:
        error(f"Invalid thread count format '{arg}': {ex}")
        return 0    # Will never happen, pacifying linters


class Boundaries:
    """ Map boundaries

    Public attributes:
    is_geodetic           -- True for geodetic (lat/lon) coordinate system,
                             False for map projection, None if not known
    top                   -- Top latitude in degrees
    bottom                -- Bottom latitude in degrees
    left                  -- Left longitude in degrees
    right                 -- Right longitude in degrees
    edges_overridden      -- True if edges overridden - with command line
                             parameters, map->geodetic conversion or adjustment
    pixel_size_lat        -- Pixel size in latitudinal direction
    pixel_size_lon        -- Pixel size in longitudinal direction
    pixel_size_overridden -- True if pixel size overridden - with command line
                             parameters, map->geodetic conversion or adjustment
    cross_180             -- Crosses 180 longitude. None if not known
    """

    def __init__(self, filename: Optional[str] = None,
                 top: Optional[float] = None, bottom: Optional[float] = None,
                 left: Optional[float] = None, right: Optional[float] = None,
                 pixel_size: Optional[float] = None,
                 pixels_per_degree: Optional[float] = None,
                 round_boundaries_to_degree: bool = False,
                 round_all_boundaries_to_degree: bool = False,
                 round_pixels_to_degree: bool = False) -> None:
        """ Constructor

        Arguments:
        filename                       -- Optional GDAL file name from which to
                                          gdalinfo boundaries
        top                            -- Optional top boundary (north-positive
                                          degrees)
        bottom                         -- Optional bottom boundary
                                          (north-positive degrees)
        left                           -- Optional left boundary (east-positive
                                          degrees)
        right                          -- Optional right boundary
                                          (east-positive degrees)
        pixel_size                     -- Optional pixel size in degrees
        pixels_per_degree              -- Optional number of pixels per degree
        round_boundaries_to_degree     -- Round not explicitly specified
                                          boundaries to next whole degree in
                                          outward direction
        round_all_boundaries_to_degree -- Round all boundaries to next whole
                                          degree in outward direction
        round_pixels_to_degree         -- Round not explicitly specified pixel
                                          sizes to whole number of pixels per
                                          degree
        """
        self.top = top
        self.bottom = bottom
        self.left = left
        self.right = right
        self.pixel_size_lat: Optional[float]
        self.pixel_size_lon: Optional[float]
        self.cross_180: Optional[bool] = None
        if pixel_size is not None:
            self.pixel_size_lat = self.pixel_size_lon = pixel_size
        elif pixels_per_degree is not None:
            self.pixel_size_lat = self.pixel_size_lon = 1 / pixels_per_degree
        else:
            self.pixel_size_lat = self.pixel_size_lon = None
        self.edges_overridden = \
            any(v is not None for v in
                (self.top, self.bottom, self.left, self.right))
        self.pixel_size_overridden = \
            any(v is not None for v in
                (self.pixel_size_lat, self.pixel_size_lon))
        if not all(v is not None for v in
                   (self.top, self.bottom, self.left, self.right,
                    self.pixel_size_lat, self.pixel_size_lon)) and \
                (filename is not None):
            gi = GdalInfo(filename)
            resample = not gi.is_geodetic
            if resample:
                temp_file: Optional[str] = None
                try:
                    error_if((gi.right is None) or (gi.left is None),
                             f"Can't determine geodetic boundaries of "
                             f"'{filename}' from its 'gdalinfo' information")
                    assert (gi.right is not None) and (gi.left is not None)
                    h, temp_file = tempfile.mkstemp(suffix=".tif")
                    os.close(h)
                    warp(src=filename, dst=temp_file, resampling="near",
                         pixel_size_lat=0.01, pixel_size_lon=0.01,
                         center_lon_180=gi.right < gi.left, overwrite=True,
                         quiet=True)
                    gi = GdalInfo(temp_file)
                    self.edges_overridden = True
                finally:
                    if temp_file:
                        os.unlink(temp_file)
            for attr in ["top", "bottom", "left", "right",
                         "pixel_size_lat", "pixel_size_lon"]:
                if getattr(self, attr) is None:
                    setattr(self, attr, getattr(gi, attr))
        if round_boundaries_to_degree or round_all_boundaries_to_degree:
            for attr, arg, ceil_floor in \
                    [("top", top, math.ceil), ("bottom", bottom, math.floor),
                     ("left", left, math.floor), ("right", right, math.ceil)]:
                if (arg is not None) and (not round_all_boundaries_to_degree):
                    continue
                v = getattr(self, attr)
                if v is None:
                    continue
                self.edges_overridden = True
                setattr(self, attr, ceil_floor(v))
        if round_pixels_to_degree and \
                (pixel_size is None) and (pixels_per_degree is None):
            for attr in ("pixel_size_lat", "pixel_size_lon"):
                v = getattr(self, attr)
                if v is None:
                    continue
                self.pixel_size_overridden = True
                setattr(self, attr, 1 / round(1 / v))
        if (self.left is not None) and (self.right is not None):
            left, right = self.left, self.right
            while left > 180:
                left -= 360
                right -= 360
            while left <= -180:
                left += 360
                right += 360
            assert -180 < left <= 180
            self.cross_180 = right >= 180

    def __eq__(self, other: Any) -> bool:
        """ Equality comparison """
        return isinstance(other, self.__class__) and \
            (self._values_tuple() == other._values_tuple())

    def __hash__(self) -> int:
        """ Hash """
        return hash(self._values_tuple())

    def contains(self, other: "Boundaries", extension: float = 0) -> bool:
        """ True if given boundaries lie inside this boundaries. Artificially
        extends self by given amount """
        assert (self.top is not None) and (self.bottom is not None)
        assert (self.left is not None) and (self.right is not None)
        assert (other.top is not None) and (other.bottom is not None)
        assert (other.left is not None) and (other.right is not None)
        if (other.top > (self.top + extension)) or \
                (other.bottom < (self.bottom - extension)):
            return False

        self_left, self_right, other_left, other_right, \
            self_circular, other_circular = \
            self.__class__.align_longitude_ranges(
                self.left, self.right, other.left, other.right)
        if not self_circular:
            self_left -= extension
            self_right += extension
        return self_circular or \
            ((not other_circular) and
             ((self_left <= other_left) and (self_right >= other_right)))

    def intersects(self, other: "Boundaries") -> bool:
        """ True if given boundaries intersect with this boundaries """
        assert (self.top is not None) and (self.bottom is not None)
        assert (self.left is not None) and (self.right is not None)
        assert (other.top is not None) and (other.bottom is not None)
        assert (other.left is not None) and (other.right is not None)
        if (other.bottom > self.top) or (other.top < self.bottom):
            return False

        self_left, self_right, other_left, other_right, \
            self_circular, other_circular = \
            self.__class__.align_longitude_ranges(
                self.left, self.right, other.left, other.right)
        return self_circular or other_circular or \
            ((other_left <= self_right) and (other_right >= self_left))

    def combine(self, other: "Boundaries",
                round_boundaries_to_degree: bool = False) -> "Boundaries":
        """ Returns boundaries that contains both this and other boundaries
        (optionally rounding boundaries to next outward degree. Pixel sizes are
        not set
        """
        assert (self.top is not None) and (self.bottom is not None)
        assert (self.left is not None) and (self.right is not None)
        assert (other.top is not None) and (other.bottom is not None)
        assert (other.left is not None) and (other.right is not None)

        self_left, self_right, other_left, other_right, \
            self_circular, other_circular = \
            self.__class__.align_longitude_ranges(
                self.left, self.right, other.left, other.right)

        return Boundaries(
            top=max(self.top, other.top),
            bottom=min(self.bottom, other.bottom),
            left=self_left if self_circular else
            (other_left if other_circular else min(self_left, other_left)),
            right=self_right if self_circular else
            (other_right if other_circular else max(self_right, other_right)),
            round_all_boundaries_to_degree=round_boundaries_to_degree and
            (not (self_circular or other_circular)))

    def crop(self, other: "Boundaries",
             round_boundaries_to_degree: bool = False) \
            -> Optional["Boundaries"]:
        """ Returns self, cropped by other boundaries """
        assert (self.top is not None) and (self.bottom is not None)
        assert (self.left is not None) and (self.right is not None)
        ret_top = self.top if other.top is None else min(self.top, other.top)
        ret_bottom = self.bottom if other.bottom is None \
            else max(self.bottom, other.bottom)
        if self.top < self.bottom:
            return None

        assert (other.left is None) == (other.right is None)
        if other.left is not None:
            assert other.right is not None
            self_left, self_right, other_left, other_right, \
                self_circular, other_circular = \
                self.__class__.align_longitude_ranges(
                    self.left, self.right, other.left, other.right)
            ret_left = other_left if self_circular else \
                (self_left if other_circular else max(self_left, other_left))
            ret_right = other_right if self_circular else \
                (self_right if other_circular
                 else min(self_right, other_right))
            if ret_left > ret_right:
                return None
        else:
            ret_left = self.left
            ret_right = self.right
        return Boundaries(
            top=ret_top, bottom=ret_bottom, left=ret_left, right=ret_right,
            round_all_boundaries_to_degree=round_boundaries_to_degree)

    def _values_tuple(self) -> Tuple[Optional[Union[float, bool]], ...]:
        """ Tuple containing all fields """
        return (self.top, self.bottom, self.left, self.right,
                self.pixel_size_lat, self.pixel_size_lon,
                self.edges_overridden, self.pixel_size_overridden)

    def __str__(self) -> str:
        """ String representation (for development purposes) """
        assert (self.top is not None) and (self.bottom is not None)
        assert (self.left is not None) and (self.right is not None)
        return (f"[{abs(self.bottom)}{'N' if self.bottom >= 0 else 'S'} - "
                f"{abs(self.top)}{'N' if self.top >= 0 else 'S'}] X "
                f"[{abs(self.left)}{'E' if self.left >= 0 else 'W'} - "
                f"{abs(self.right)}{'E' if self.right >= 0 else 'W'}]")

    @classmethod
    def align_longitude_ranges(cls, left1: float, right1: float, left2: float,
                               right2: float) \
            -> Tuple[float, float, float, float, bool, bool]:
        """ For given pair of longitude ranges return another pair that are
        properly ordered and lie in same 360 degree range, also returns if
        source or destination are circular
        """
        circular1 = \
            (left1 != right1) and (abs(math.fmod(right1 - left1, 360)) < 1e-6)
        circular2 = \
            (left2 != right2) and (abs(math.fmod(right2 - left2, 360)) < 1e-6)
        while right1 <= left1:
            right1 += 360
        while (right1 - left1) > 360:
            right1 -= 360

        while right2 <= left2:
            right2 += 360
        while (right2 - left2) > 360:
            right2 -= 360

        if not (circular1 or circular2):
            while (right1 - left2) >= 360:
                left1 -= 360
                right1 -= 360
            while (left1 - right2) <= -360:
                left1 += 360
                right1 += 360
        return (left1, right1, left2, right2, circular1, circular2)


class Geoids:
    """ Collection of geoid files

    Private attributes:
    _geoids    -- List of (boundary, file) tuples for geoid files
    _extension -- Amount of geoid coverage extension when coverage is tested
    """

    def __init__(self, geoids: Optional[Union[str, List[str]]],
                 extension: float = 0) -> None:
        """ Constructor

        Arguments:
        geoids    -- None or name of geoid files or list of names of geoid
                     files. Geoid file names may include wildcard symbols. List
                     items expected to follow in order of preference decrease
        extension -- Artificially extend geoid boundaries by this amount when
                     checking coverage - to accommodate for margins
        """
        self._extension = extension
        if isinstance(geoids, str):
            geoids = [geoids]
        elif geoids is None:
            geoids = []
        self._geoids: List[Tuple[Boundaries, str]] = []
        for geoid_str in geoids:
            files = glob.glob(geoid_str)
            if (not files) and (not os.path.dirname(geoid_str)):
                files = glob.glob(os.path.join(os.path.dirname(__file__),
                                               geoid_str))
            error_if(not files,
                     f"No geoid files matching '{geoid_str}' found")
            for filename in files:
                self._geoids.append((Boundaries(filename), filename))

    def __bool__(self):
        """ True if collection is not empty """
        return bool(self._geoids)

    def geoid_for(self, boundaries: Boundaries,
                  fail_if_not_found: bool = True) -> Optional[str]:
        """ Finds geoid containing given boundaries

        Arguments:
        boundaries        -- Boundaries to look geoid for. First fit
                             (ostensibly - preferable) geoid is expected to be
                             found
        fail_if_not_found -- True to fail if nothing found, False to return
                             None
        Returns name of geoid file, None if not found
        """
        for gb, gn in self._geoids:
            if gb.contains(boundaries, extension=self._extension):
                return gn
        error_if(fail_if_not_found, f"Geoid for {boundaries} not found")
        return None


def get_scale(arg_scale: Optional[List[float]], src_data_types: Sequence[str],
              dst_format: Optional[str], dst_data_type: Optional[str],
              dst_ext: Optional[str]) -> Optional[List[float]]:
    """ Apply default to scale parameter if destination is PNG and has
    different data type

    Arguments:
    arg_scale      -- Optional --scale parameter value
    src_data_types -- Collection of data types in source files
    dst_format     -- Optional GDAL file type format for destination file
    dst_data_type  -- Optional resulting data type
    dst_ext        -- Optional extension of destination file
    Returns either what was passed with --scale or proper PNG-related default
    """
    if (arg_scale is not None) or (not src_data_types) or \
            (not _is_png(gdal_format=dst_format, file_ext=dst_ext)) or \
            all(v in ("Byte", "UInt16") for v in src_data_types) or \
            (dst_data_type not in (None, "UInt16")):
        return arg_scale
    if any("Float" for v in src_data_types):
        return DEFAULT_FLOAT_PNG_SCALE
    return DEFAULT_INT_PNG_SCALE


def get_no_data(arg_no_data: Optional[str], src_data_types: Sequence[str],
                dst_format: Optional[str], dst_data_type: Optional[str],
                dst_ext: Optional[str]) -> Optional[str]:
    """ Apply default to NoData parameter if destination is PNG and has
    different data type

    Arguments:
    arg_no_data    -- Optional --no_data parameter value
    src_data_types -- Collection of data types in source files
    dst_format     -- Optional GDAL file type format for destination file
    dst_data_type  -- Optional resulting data type
    dst_ext        -- Optional extension of destination file
    Returns either what was passed with --no_data or proper PNG-related default
    """
    if (arg_no_data is not None) or (not src_data_types) or \
            (not _is_png(gdal_format=dst_format, file_ext=dst_ext)) or \
            all(v in ("Byte", "UInt16") for v in src_data_types) or\
            (dst_data_type not in (None, "UInt16")):
        return arg_no_data
    return str(DEFAULT_PNG_NO_DATA)


def _is_png(gdal_format: Optional[str], file_ext: Optional[str]) -> bool:
    """ Checksd if given file information corresponds to PNG file

    Argument:
    gdal_format -- Optional value of --format switch
    file_ext    -- File extenmsion
    True if at least ond of given arguments corresponds to PNG
    """
    return (gdal_format == PNG_FORMAT) or (file_ext == PNG_EXT)


def get_resampling(
        arg_resampling: Optional[str], src_data_types: Sequence[str],
        dst_data_type: Optional[str]) -> str:
    """ Provides resulting resampling method

    Arguments:
    arg_resampling -- Optional resampling algorithm from command line
    src_data_types -- Collection of data types in source files
    dst_data_type  -- Optional resulting data type
    """
    if arg_resampling:
        return arg_resampling
    error_if(not (src_data_types or dst_data_type),
             "--resampling can't be derived and must be specified explicitly")
    return \
        "near" if "Byte" in list(src_data_types) + [dst_data_type] else "cubic"


def nice() -> None:
    """ Lower priority of current process """
    if os.name == "nt":
        if HAS_PSUTIL:
            psutil.Process().nice(psutil.BELOW_NORMAL_PRIORITY_CLASS)
        else:
            warning("Process can't be niced because 'psutil' Python module is "
                    "not installed")
    elif os.name == "posix":
        os.nice(1)
    else:
        warning("No idea how to nice in this environment")
