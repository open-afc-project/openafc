#!/usr/bin/env python3
# Computes MD5 hash over files in directory

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=unused-wildcard-import, invalid-name, too-few-public-methods
# pylint: disable=too-many-statements, too-many-locals, too-many-arguments
# pylint: disable=wildcard-import
import argparse
import datetime
import fnmatch
import hashlib
import json
try:
    import jsonschema
except ImportError:
    pass
import multiprocessing
import os
import queue
import re
import signal
import sys
import threading
from typing import Any, Dict, List, NamedTuple, Optional, Tuple, Union

from geoutils import *

# For printing statistics
GIGA = 1024 * 1024 * 1024

# Name of schema file for version information JSON files
SCHEMA_FILE_NAME = "g8l_info_schema.json"

# Root of geospatial data definitions in JSON file
FILESETS_KEY = "data_releases"

# Structure of dataset key
FILESET_KEY_PATTERN = r"^\w{3}_\w{2}_\w{3}$"

# Key of filemask list witin dataset
FILESET_MASKS_KEY = "files"

# Key of MD5 list witin dataset
FILESET_MD5_KEY = "md5"

_EPILOG = """ This script benefits from `jsonschema` Python module installed,
but may leave without it. Hence it may easily be used outside of container.
Some examples:
- List subcommands:
    $ dir_md5.py
- Print help on `compute` subcommand:
    $ dir_md5.py help compute
- List LiDAR files (in proc_lidar_2019 directory) to be included into MD5
  computation:
    $ dir_md5.py list --mask '*.csv' --mask '*.tif' --recursive proc_lidar_2019
- Compute MD5 over SRTM files in srtm3arcsecondv003 directory:
    $ dir_md5.py compute --mask '*.hgt'  srtm3arcsecondv003
- Update MD5 for USA geoids, per by usa_ge_prd/usa_ge_prd_version_info.json
    $ dir_md5.py update --json usa_ge_prd/usa_ge_prd_version_info.json
"""


class FileInfo:
    """ Information about file to hash

    Public attributes:
    full_path -- Full file path (to use for file reading)
    rel_path  -- Root-relative path to use as sort key if computation is
                 recursive
    basename  -- Basename to use as sort key if computation is nonrecursive
    size      -- File size in bytes
    """
    def __init__(self, filename: str, root: str, size: int) -> None:
        """ Constructor

        Arguments:
        filename -- File name
        root     -- Root relative to which relative path should be computed
        size     -- File size in bytes
        """
        self.full_path = os.path.abspath(filename)
        self.rel_path = os.path.relpath(filename, root)
        self.basename = os.path.basename(filename)
        self.size = size

    def sort_key(self, with_path: bool) -> Union[str, Tuple[str, str]]:
        """ Sort key

        Arguments:
        with_path -- True to return relative path as sort key, False to return
                     basename
        Returns sort key
        """
        return os.path.split(self.rel_path) if with_path else self.basename


class ChunkInfo:
    """ Information about file chunk

    Public attributes:
    seq      -- Chunk sequence number in overall chunk sequence
    fileinfo -- Information about file to read chunk from
    offset   -- Chunk offset
    length   -- Chunk length
    data     -- Chunk content (filled in worker, initially None)
    """
    def __init__(self, seq: int, fileinfo: FileInfo, offset: int,
                 length: int) -> None:
        """ Constructor

        Arguments:
        seq      -- Chunk sequence number in overall chunk sequence
        fileinfo -- Information about file to read chunk from
        offset   -- Chunk offset
        length   -- Chunk length
        """
        self.seq = seq
        self.fileinfo = fileinfo
        self.offset = offset
        self.length = length
        self.data: Optional[bytes] = None


class Md5Computer:
    """ Threaded MD5 computer

    Private attributes:
    _num_threads   -- Number of threads
    _chunk_size    -- Size of chunk in bytes
    _max_in_flight -- Maximum number of retrieved but not processed chunks
    _progress      -- Print progress information
    """
    def __init__(self, num_threads: Optional[int], chunk_size: int,
                 max_in_flight: int, progress: bool, stats: bool) -> None:
        """ Constructor

        Arguments:
        num_threads   -- Number of threads (None for 2*CPUs)
        chunk_size    -- Size of chunk in bytes
        max_in_flight -- Maximum number of retrieved but not processed chunks
        progress      -- Print progress information
        stats         -- Print performance statistics
        """
        self._num_threads: int = \
            num_threads or (2 * multiprocessing.cpu_count())
        self._chunk_size = chunk_size
        self._max_in_flight = max_in_flight
        self._progress = progress
        self._stats = stats

    def calculate(self, fileinfos: List[FileInfo]) -> str:
        """ Calculate MD5

        Arguments:
        fileinfos -- List of FileInfo objects on files to use
        Return MD5 in string representation
        """
        start_time = datetime.datetime.now()
        md5 = hashlib.md5()
        # Worker threads that read chunks' data
        threads: List[threading.Thread] = []
        try:
            # Queue to worker threads, contains unfilled chunks, Nones to stop
            chunk_task_queue: queue.Queue[Optional[ChunkInfo]] = queue.Queue()
            # Queue from worker threads, contains filled chunks, Nones on error
            chunk_result_queue: queue.Queue[Optional[ChunkInfo]] = \
                queue.Queue()
            # Filled chunks, indexed by sequence number
            chunk_map: Dict[int, ChunkInfo] = {}
            # Sequence number of next chunk to be put to task queue
            next_chunk_seq = 0
            # Sequence number of next chunk to MD5
            next_md5_seq = 0
            # Creating worker threads
            original_sigint_handler = \
                signal.signal(signal.SIGINT, signal.SIG_IGN)
            for _ in range(self._num_threads):
                threads.append(
                    threading.Thread(
                        target=self._chunk_worker,
                        kwargs={"task_queue": chunk_task_queue,
                                "result_queue": chunk_result_queue}))
                threads[-1].start()
            signal.signal(signal.SIGINT, original_sigint_handler)

            task_queue_lens = 0
            task_queue_samples = 0
            last_name = ""

            # List of all nonfilled chunks
            chunks = self._get_chunks(fileinfos)
            # While not all chunks were MD%-ed
            while next_md5_seq != len(chunks):
                # MD5 chunks from map that are in order
                while next_md5_seq in chunk_map:
                    data = chunk_map[next_md5_seq].data
                    assert data is not None
                    md5.update(data)
                    chunk_map[next_md5_seq].data = None
                    del chunk_map[next_md5_seq]
                    next_md5_seq += 1

                task_queue_lens += chunk_task_queue.qsize()
                task_queue_samples += 1

                # Give workers some chunks to fill
                while (next_chunk_seq != len(chunks)) and \
                        ((next_chunk_seq - next_md5_seq) <
                         self._max_in_flight):
                    chunk = chunks[next_chunk_seq]

                    new_name = chunk.fileinfo.rel_path
                    if self._progress and (last_name != new_name):
                        print(new_name +
                              (max(0, len(last_name) - len(new_name))) * " ",
                              end="\r", flush=True)
                        last_name = new_name

                    chunk_task_queue.put(chunk)
                    next_chunk_seq += 1
                # Retrieve filled chunks to chunk map
                while (not chunk_result_queue.empty()) or \
                        (next_md5_seq != len(chunks)) and \
                        (next_md5_seq not in chunk_map):
                    chunk1: Optional[ChunkInfo] = chunk_result_queue.get()
                    error_if(chunk1 is None,
                             "Undefined file reading problem")
                    assert chunk1 is not None
                    error_if(
                        chunk1.data is None,
                        f"Error reading {chunk1.length} bytes at offset "
                        f"{chunk1.offset} from '{chunk.fileinfo.full_path}'")
                    chunk_map[chunk1.seq] = chunk1
        finally:
            # Close all threads
            for _ in range(len(threads)):
                chunk_task_queue.put(None)
            for thread in threads:
                thread.join()

        if self._progress:
            print(len(last_name) * " ", end="\r")

        if self._stats:
            seconds = (datetime.datetime.now() - start_time).total_seconds()
            total_size = sum(fi.size for fi in fileinfos)
            cpu_bound = \
                (task_queue_lens / (task_queue_samples or 1)) < \
                (self._max_in_flight / 2)
            print(f"Duration: {int(seconds) // 3600}h "
                  f"{(int(seconds) // 60) % 60}m {seconds % 60:.2f}s")
            print(f"Data length: {total_size / GIGA:.3f} GB")
            print(f"Performance: {total_size / seconds / GIGA:.2f} GB/s")
            print(
                f"Operation was {'CPU(MD5)' if cpu_bound else 'IO'}-bound")

        return "".join(f"{b:02X}" for b in md5.digest())

    def _get_chunks(self, fileinfos: List[FileInfo]) -> List[ChunkInfo]:
        """ Create list of chunks """
        ret: List[ChunkInfo] = []
        for fi in fileinfos:
            offset = 0
            while offset < fi.size:
                length = min(self._chunk_size, fi.size - offset)
                ret.append(
                    ChunkInfo(seq=len(ret), fileinfo=fi, offset=offset,
                              length=length))
                offset += length
        return ret

    def _chunk_worker(self, task_queue: queue.Queue,
                      result_queue: queue.Queue) -> None:
        """ Worker thread function that fills chunks

        Arguments:
        task_queue   -- Queue with chunks to fill. None to exit
        result_queue -- Queue with filled chunks
        """
        while True:
            chunk: Optional[ChunkInfo] = None
            try:
                chunk = task_queue.get()
                if chunk is None:
                    return
                with open(chunk.fileinfo.full_path, mode="rb") as f:
                    f.seek(chunk.offset)
                    chunk.data = f.read(chunk.length)
            except Exception:
                pass
            finally:
                result_queue.put(chunk)


class VersionJson:
    """ Handler of version information JSON

    Private attributes:
    _filename  -- JSON file name
    _json_dict -- JSON file content as dictionary

    Public attributes:
    directory -- Directory where JSON file is located
    """
    # Information about fileset, stored in JSON
    FilesetInfo = \
        NamedTuple(
            "FilesetInfo", [
                # Fileset type (region_filetype)
                ("fileset_type", str),
                # List of masks
                ("masks", List[str])])

    def __init__(self, filename: str) -> None:
        """ Constructor

        Arguments:
        filename -- JSON file name
        """
        self._filename = filename
        error_if(not os.path.isfile(self._filename),
                 f"File '{self._filename}' not found")
        try:
            with open(self._filename, mode="rb") as f:
                self._json_dict = json.load(f)
        except json.JSONDecodeError as ex:
            error(f"Invalid JSON syntax in '{self._filename}': {ex}")
        self.directory = os.path.dirname(self._filename) or "."

        schema_filename = \
            os.path.join(os.path.dirname(__file__) or ".", SCHEMA_FILE_NAME)
        if os.path.isfile(schema_filename):
            if "jsonschema" in sys.modules:
                try:
                    with open(schema_filename, mode="rb") as f:
                        json_schema_dict = json.load(f)
                    try:
                        jsonschema.validate(instance=self._json_dict,
                                            schema=json_schema_dict)
                    except jsonschema.ValidationError as ex:
                        warning(f"File '{self._filename}' doesn't match "
                                f"schema: {ex}")
                except json.JSONDecodeError as ex:
                    warning(f"Invalid JSON syntax in '{schema_filename}': "
                            f"{ex}, hence correctness of '{self._filename}' "
                            f"schema not checked")
            else:
                warning(f"'jsonschema' module not installed in Python, hence "
                        f"correctness of '{self._filename}' schema not "
                        f"checked")
        else:
            warning(f"Version info schema file ({schema_filename}) was not "
                    f"found, hence correctness of '{self._filename}' schema "
                    f"not checked")
        error_if(not (isinstance(self._json_dict, dict) and
                 (FILESETS_KEY in self._json_dict) and
                 isinstance(self._json_dict[FILESETS_KEY], dict) and
                 all(re.match(FILESET_KEY_PATTERN, k)
                     for k in self._json_dict[FILESETS_KEY])),
                 f"Unsupported structure of '{self._filename}'")

    def get_filesets(self) -> List["VersionJson.FilesetInfo"]:
        """ Returns information about filesets contained in the file """
        ret: List["VersionJson.FilesetInfo"] = []
        for fs_type, fs_dict in self._json_dict[FILESETS_KEY].items():
            error_if(not(isinstance(fs_dict, dict) and
                         (FILESET_MASKS_KEY in fs_dict) and
                         isinstance(fs_dict[FILESET_MASKS_KEY], list) and
                         all(isinstance(v, str)
                             for v in fs_dict[FILESET_MASKS_KEY])),
                     f"'{fs_type}' fileset descriptor in "
                     f"'{FILESET_KEY_PATTERN}' of '{self._filename}' has "
                     f"invalid structure")
            ret.append(
                self.__class__.FilesetInfo(
                    fileset_type=fs_type, masks=fs_dict[FILESET_MASKS_KEY]))
        return ret

    def get_md5(self, fs_type: str) -> Optional[str]:
        """ Returns MD5 stored for given fileset type. None if there is none
        """
        return self._json_dict[FILESETS_KEY][fs_type].get(FILESET_MD5_KEY)

    def update_md5(self, fs_type: str, md5_str: str) -> None:
        """ Updates MD5 for given fileset type """
        self._json_dict[FILESETS_KEY][fs_type][FILESET_MD5_KEY] = md5_str
        try:
            with open(self._filename, mode="w", encoding="utf-8") as f:
                json.dump(self._json_dict, f, indent=2)
        except OSError as ex:
            error(f"Error writing '{self._filename}': {ex}")


def get_files(file_dir_name: str, masks: Optional[List[str]],
              recursive: bool) -> List[FileInfo]:
    """ Creates list of FileInfo object from given file/directory name

    Arguments:
    file_or_dir -- Name of file or directory
    masks       -- None or list of fnmatch-compatible wildcard masks
    recursive   -- True to recurse into subdirectories
    Returns list of FileInfo objects
    """
    ret: List[FileInfo] = []

    def process_file(filename: str, root: str) -> None:
        """ Adds file to resulting list if it is eligible

        Arguments:
        filename -- File name
        root     -- Root directory to use for FileInfo creation
        """
        if masks:
            for mask in masks:
                if fnmatch.fnmatch(os.path.basename(filename), mask):
                    break
            else:
                return
        ret.append(FileInfo(filename=filename, root=root,
                            size=os.path.getsize(filename)))
    if os.path.isfile(file_dir_name):
        process_file(file_dir_name, os.path.dirname(file_dir_name))
    elif os.path.isdir(file_dir_name):
        if recursive:
            for dirpath, _, basenames in os.walk(file_dir_name):
                for basename in basenames:
                    process_file(os.path.join(dirpath, basename),
                                 file_dir_name)
        else:
            for basename in os.listdir(file_dir_name):
                full_name = os.path.join(file_dir_name, basename)
                if os.path.isfile(full_name) and \
                        (not os.path.islink(full_name)):
                    process_file(full_name, file_dir_name)
    else:
        error(f"'{file_dir_name}' is neither file nor directory name")
    return ret


def get_filelists(
        json_filename: Optional[str], json_required: bool = False,
        files_and_dirs: Optional[List[str]] = None, recursive: bool = False,
        masks: Optional[List[str]] = None) \
        -> Tuple[Optional[VersionJson],
                 List[Tuple[Optional[str], List[FileInfo]]]]:
    """ Returns filelists, indexed by fileset types

    Arguments:
    json_filename  -- Optional name of version info JSAON file
    json_required  -- True if version info JSAON file must be specified
    files_and_dirs -- List of files and directories for which to compute MD5.
                      If empty and JSOIN file not specified - all files in the\
                      current directory
    recursive      -- True to recurse into subdirectories
    masks          -- fnmatch masks of files to include into MD5 computation
    """
    error_if(json_required and (json_filename is None),
             "Version info JSON file (--json switch) not provided")
    error_if(
        (json_filename is not None) and files_and_dirs,
        "Files/directories should not be provided if --json is provided")
    ret: List[Tuple[Optional[str], List[FileInfo]]] = []
    vj: Optional[VersionJson] = None
    if json_filename is not None:
        vj = VersionJson(json_filename)
        for fs in vj.get_filesets():
            ret.append(
                (fs.fileset_type, get_files(vj.directory, masks=fs.masks,
                                            recursive=False)))
        recursive = False
    else:
        fileinfos: List[FileInfo] = []
        for file_or_dir in files_and_dirs or ["."]:
            fileinfos += get_files(file_dir_name=file_or_dir, masks=masks,
                                   recursive=recursive)
        ret.append((None, fileinfos))
        if (len(fileinfos) == 1) and \
                (os.path.splitext(fileinfos[0].basename)[1] == ".json"):
            warning("Didn't you forget to provide '--json' switch? Without it "
                    "MD5 will be computed only over JSON file itself")

    def sort_key(fi: FileInfo) -> Union[str, Tuple[str, str]]:
        return fi.sort_key(with_path=recursive)

    for _, fileinfos in ret:
        fileinfos.sort(key=sort_key)
        for i in range(len(fileinfos) - 1):
            error_if(
                sort_key(fileinfos[i]) == sort_key(fileinfos[i + 1]),
                f"Files '{fileinfos[i].full_path}' and "
                f"'{fileinfos[i + 1].full_path}' have same sort name of "
                f"'sort_key(fileinfos[i])', which makes MD5 undefined (as it "
                f"is file order dependent)")
    return (vj, ret)


def do_list(args: Any) -> None:
    """Execute "list" command.

    Arguments:
    args -- Parsed command line arguments
    """
    _, filelists = \
        get_filelists(
            json_filename=args.json, files_and_dirs=args.FILES_AND_DIRS,
            recursive=args.recursive, masks=args.mask)
    for fs_type, fileinfos in filelists:
        if len(filelists) != 1:
            print(f"Fileset: {fs_type}")
        for fileinfo in fileinfos:
            print(fileinfo.rel_path if (args.recursive and not args.json)
                  else fileinfo.basename)
        if args.stats:
            print("")
            total_size = sum(fi.size for fi in fileinfos)
            print(f"Data length: {total_size / GIGA:.3f} GB")
        if len(filelists) != 1:
            print("")


def do_compute(args: Any) -> None:
    """Execute "compute" command.

    Arguments:
    args -- Parsed command line arguments
    """
    if args.nice:
        nice()
    _, filelists = \
        get_filelists(
            json_filename=args.json, files_and_dirs=args.FILES_AND_DIRS,
            recursive=args.recursive, masks=args.mask)
    for fs_type, fileinfos in filelists:
        if len(filelists) != 1:
            print(f"Fileset: {fs_type}")
        md5_computer = Md5Computer(num_threads=threads_arg(args.threads),
                                   chunk_size=args.chunk,
                                   max_in_flight=args.max_in_flight,
                                   progress=args.progress, stats=args.stats)
        print(md5_computer.calculate(fileinfos))
        if len(filelists) != 1:
            print("")


def do_verify(args: Any) -> None:
    """Execute "verify" command.

    Arguments:
    args -- Parsed command line arguments
    """
    if args.nice:
        nice()
    version_json, filelists = \
        get_filelists(json_filename=args.json, json_required=True)
    assert version_json is not None
    mismatches: List[str] = []
    for fs_type, fileinfos in filelists:
        assert fs_type is not None
        if len(filelists) != 1:
            print(f"Fileset: {fs_type}")
        md5_computer = Md5Computer(num_threads=threads_arg(args.threads),
                                   chunk_size=args.chunk,
                                   max_in_flight=args.max_in_flight,
                                   progress=args.progress, stats=args.stats)
        computed_md5 = md5_computer.calculate(fileinfos)
        json_md5 = version_json.get_md5(fs_type)
        if computed_md5 != json_md5:
            print(f"MD5 mismatch. Computed: {computed_md5}, in '{args.json}': "
                  f"{json_md5}'")
            print("")
            mismatches.append(fs_type)
    error_if(mismatches, f"MD5 mismatches found in: {', '.join(mismatches)}")


def do_update(args: Any) -> None:
    """Execute "update" command.

    Arguments:
    args -- Parsed command line arguments
    """
    if args.nice:
        nice()
    version_json, filelists = \
        get_filelists(json_filename=args.json, json_required=True)
    assert version_json is not None
    for fs_type, fileinfos in filelists:
        assert fs_type is not None
        if len(filelists) != 1:
            print(f"Fileset: {fs_type}")
        md5_computer = Md5Computer(num_threads=threads_arg(args.threads),
                                   chunk_size=args.chunk,
                                   max_in_flight=args.max_in_flight,
                                   progress=args.progress, stats=args.stats)
        computed_md5 = md5_computer.calculate(fileinfos)
        json_md5 = version_json.get_md5(fs_type)
        if computed_md5 != json_md5:
            version_json.update_md5(fs_type, computed_md5)


def do_help(args: Any) -> None:
    """Execute "help" command.

    Arguments:
    args -- Parsed command line arguments (also contains 'argument_parser' and
            'subparsers' fields)
    """
    if args.subcommand is None:
        args.argument_parser.print_help()
    else:
        args.subparsers.choices[args.subcommand].print_help()


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    # Switches for explicitly specified sets of files
    switches_explicit = argparse.ArgumentParser(add_help=False)
    switches_explicit.add_argument(
        "--recursive", action="store_true",
        help="Process files in subdirectories")
    switches_explicit.add_argument(
        "--mask", metavar="WILDCARD", action="append",
        help="Only use given filename pattern (should not include directory). "
        "Wildcard format is fnmatch compatible (?, *, [...]). This switch may "
        "be specified several times. By default all files are used. Don't "
        "forget to quote it on Linux")
    switches_explicit.add_argument(
        "FILES_AND_DIRS", nargs="*",
        help="Files and directories. Default is current directory. Yet if no "
        "arguments specified - help will be printed, so, say, '.' or '*' "
        "might be used")

    # Switches for MD5 computation
    switches_computation = argparse.ArgumentParser(add_help=False)
    switches_computation.add_argument(
        "--progress", action="store_true",
        help="Print progress information")
    switches_computation.add_argument(
        "--threads", metavar="COUNT_OR_PERCENT%",
        help="Number of threads to use. If positive - number of threads, if "
        "negative - number of CPU cores NOT to use, if followed by `%%` - "
        "percent of CPU cores. Default is total number of CPU cores")
    switches_computation.add_argument(
        "--nice", action="store_true",
        help="Lower priority of this process and its subprocesses")
    switches_computation.add_argument(
        "--chunk", type=int, default=16*1024*1024, help=argparse.SUPPRESS)
    switches_computation.add_argument(
        "--max_in_flight", type=int, default=50, help=argparse.SUPPRESS)

    # Switches for statistics
    switches_stats = argparse.ArgumentParser(add_help=False)
    switches_stats.add_argument(
        "--stats", action="store_true", help="Print statistics")

    # Switches for version info JSON
    switches_optional_json = argparse.ArgumentParser(add_help=False)
    switches_optional_json.add_argument(
        "--json", metavar="VERSION_INFO_JSON",
        help="Name of ...version_info.json file that describes fileset(s) to "
        "compute MD5 for")

    # Switches for version info JSON
    switches_required_json = argparse.ArgumentParser(add_help=False)
    switches_required_json.add_argument(
        "--json", metavar="VERSION_INFO_JSON", required=True,
        help="Name of ...version_info.json file that describes fileset(s) to "
        "compute MD5 for. This parameter is mandatory")

    argument_parser = argparse.ArgumentParser(
        description="Computes MD5 hash over files in directory",
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=_EPILOG)
    subparsers = argument_parser.add_subparsers(dest="subcommand",
                                                metavar="SUBCOMMAND")

    # Subparser for "list" subcommand
    parser_list = subparsers.add_parser(
        "list",
        parents=[switches_explicit, switches_optional_json, switches_stats],
        help="Print list of files (no MD5 computed) - sorted in the same "
        "order as MD5 would have been be computed. Sorting is first by "
        "directory  (if --recursive specified) then by filename")
    parser_list.set_defaults(func=do_list)

    # Subparser for "compute" subcommand
    parser_compute = subparsers.add_parser(
        "compute",
        parents=[switches_explicit, switches_optional_json,
                 switches_computation, switches_stats],
        help="Compute MD5")
    parser_compute.set_defaults(func=do_compute)

    # Subparser for "verify" subcommand
    parser_verify = subparsers.add_parser(
        "verify",
        parents=[switches_required_json, switches_computation, switches_stats],
        help="Computes MD5 and checks if it matches one in "
        "...version_info.json file")
    parser_verify.set_defaults(func=do_verify)

    # Subparser for "update" subcommand
    parser_update = subparsers.add_parser(
        "update",
        parents=[switches_required_json, switches_computation, switches_stats],
        help="Computes MD5 and updates ...version_info.json file with it")
    parser_update.set_defaults(func=do_update)

    # Subparser for 'help' command
    parser_help = subparsers.add_parser(
        "help", add_help=False,
        help="Prints help on given subcommand")
    parser_help.add_argument(
        "subcommand", metavar="SUBCOMMAND", nargs="?",
        choices=subparsers.choices,
        help="Name of subcommand to print help about (use " +
        "\"%(prog)s --help\" to get list of all subcommands)")
    parser_help.set_defaults(func=do_help, subparsers=subparsers,
                             argument_parser=argument_parser)
    parser_help.set_defaults(supports_unknown_args=False)

    if not argv:
        argument_parser.print_help()
        sys.exit(1)
    args = argument_parser.parse_args(argv)

    setup_logging()

    args.func(args)


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except KeyboardInterrupt:
        sys.exit(1)
