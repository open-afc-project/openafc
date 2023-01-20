#!/usr/bin/env python3
# Computes MD5 hash over files in directory

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

import argparse
import datetime
import fnmatch
import hashlib
import json
import multiprocessing
import os
import queue
import re
import signal
import sys
import threading
from typing import Any, Dict, List, NamedTuple, Optional, Tuple, Union

# For printing statistics
GIGA = 1024 * 1024 * 1024


def error(errmsg: str) -> None:
    """Print given error message and exit"""
    print(f"{os.path.basename(__file__)}: Error: {errmsg}",
          file=sys.stderr)
    sys.exit(1)


def error_if(condition: Any, errmsg: str) -> None:
    """If given condition met - print given error message and exit"""
    if condition:
        error(errmsg)


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

    Public attributes:
    cpu_bound     -- True if last operation was CPU-bound, False if IO-bound
    """
    def __init__(self, num_threads: Optional[int], chunk_size: int,
                 max_in_flight: int, progress: bool) -> None:
        """ Constructor

        Arguments:
        num_threads   -- Number of threads (None to 2XCPUs)
        chunk_size    -- Size of chunk in bytes
        max_in_flight -- Maximum number of retrieved but not processed chunks
        progress      -- Print progress information
        """
        self._num_threads: int = \
            num_threads or (2 * multiprocessing.cpu_count())
        self._chunk_size = chunk_size
        self._max_in_flight = max_in_flight
        self._progress = progress
        self.cpu_bound = True

    def calculate(self, fileinfos: List[FileInfo]) -> str:
        """ Calculate MD5

        Arguments:
        fileinfos -- List of FileInfo objects on files to use
        Return MD5 in string representation
        """
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
        self.cpu_bound = \
            (task_queue_lens / (task_queue_samples or 1)) < \
            (self._max_in_flight / 2)

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


class JsonUpdater:
    """ Json file(s) updater

    Private attributes:
    _json_dscs -- List of _JsonDsc objects
    """

    _JsonDsc = \
        NamedTuple(
            "_JsonDsc",
            [
             # JSON file name
             ("filename", str),
             # Sequence of string/integer (object/list) indices in container
             ("keys", List[Union[int, str]]),
             # Prefix to prepend to MD5 value
             ("prefix", str),
             # Suffix to append to MD5 value
             ("suffix", str)])

    def __init__(self, json_file_keys: List[str]) -> None:
        """ Constructor

        Arguments:
        json_file_keys -- List of JSON_FILE:KEY_SEQUENCE[:[PREFIX][:SUFFIX]]
                          descriptors
        """
        self._json_dscs: List["JsonUpdater._JsonDsc"] = []
        for jfk in json_file_keys:
            error_if(":" not in jfk,
                     f"--json parameter '{jfk}' has no key part")
            parts = jfk.split(":")
            error_if(not (2 <= len(parts) <= 4),
                     "Invalid structure --json parameter structure (should "
                     "contain 2 to 4 parts)")
            parts += ["", ""]
            filename = parts[0]
            keys_str = parts[1]
            prefix = parts[2]
            suffix = parts[3]
            keys: List[Union[int, str]] = []
            while keys_str:
                if keys_str.startswith("/"):
                    keys_str = keys_str[1:]
                elif keys_str.startswith("["):
                    m = re.match(r"^\[(\d+)\](.*)$", keys_str)
                    error_if(m is None,
                             "Invalid list index syntax in JSON key")
                    assert m is not None
                    keys.append(int(m.group(1)))
                    keys_str = m.group(2)
                else:
                    m = re.match(r"^([^\[/]+)(.*)$", keys_str)
                    assert m is not None
                    keys.append(m.group(1))
                    keys_str = m.group(2)
            error_if(not keys, f"--json parameter '{jfk}' has no keys")
            self._json_dscs.append(
                self._JsonDsc(filename=filename, keys=keys, prefix=prefix,
                              suffix=suffix))

    def update(self, md5_str: str) -> None:
        """ Update JSON with MD5

        Arguments:
        md5_str -- MD5 value to put to JSONs
        """
        for jd in self._json_dscs:
            md5_str = jd.prefix + md5_str + jd.suffix
            json_data: Union[Dict[str, Any], List[Any]]
            if os.path.isfile(jd.filename):
                with open(jd.filename, "rb") as f:
                    try:
                        json_data = json.load(f)
                    except json.JSONDecodeError as ex:
                        error(f"File '{jd.filename}' is an invalid JSON: {ex}")
            elif isinstance(jd.keys[0], int):
                json_data = []
            else:
                assert isinstance(jd.keys[0], str)
                json_data = {}
            # Container into which is next key
            container: Union[Dict[str, Any], List[Any]] = json_data
            # Going deeper and deeper into container hierarchy, on the route
            # creating missing containers if required and possible
            for i, key in enumerate(jd.keys):
                last = i == (len(jd.keys) - 1)
                # If we'll need to insert something into current container at
                # current key - what would it be?
                insertable: Union[str, Dict[str, Any], List[Any]] = \
                    md5_str if last else \
                    ([] if isinstance(jd.keys[i + 1], int) else {})
                if isinstance(key, int):
                    # Integer key - expecting container to be list
                    error_if(not isinstance(container, list),
                             "Attempt to list-index of nonlist subconbtainer "
                             "in JSON")
                    assert isinstance(container, list)
                    error_if(len(container) < (key - 1),
                             "JSON list index out of range")
                    if key == len(container):
                        container.append(insertable)
                        if not last:
                            assert not isinstance(insertable, str)
                            container = insertable
                    elif last:
                        container[key] = md5_str
                    else:
                        container = container[key]
                else:
                    # String key - expecting container to be dictionary
                    assert isinstance(key, str)
                    error_if(not isinstance(container, dict),
                             "Attempt to object-index of nonobject "
                             "subconbtainer in JSON")
                    assert isinstance(container, dict)
                    if (key not in container) or last:
                        container[key] = insertable
                        if not last:
                            assert not isinstance(insertable, str)
                            container = insertable
                    else:
                        container = container[key]
            with open(jd.filename, "w", encoding="utf-8") as f:
                json.dump(json_data, f, indent=4)


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


def sort_files(fileinfos: List[FileInfo], with_path: bool) -> None:
    """ Sorts given FileInfo list, checks for duplicates

    Arguments:
    fileinfos -- List to sort
    recursive -- True to use (rel_path, basename) as keys, False to use
                 basenames
    """
    def sort_key(fi: FileInfo) -> Union[str, Tuple[str, str]]:
        return fi.sort_key(with_path=with_path)

    fileinfos.sort(key=sort_key)
    for i in range(len(fileinfos) - 1):
        error_if(
            sort_key(fileinfos[i]) == sort_key(fileinfos[i + 1]),
            f"Files '{fileinfos[i].full_path}' and "
            f"'{fileinfos[i + 1].full_path}' have same sort name of "
            f"'sort_key(fileinfos[i])', which makes MD5 undefined (as it is "
            f"file order dependent)")


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = argparse.ArgumentParser(
        description="Computes MD5 hash over files in directory",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    argument_parser.add_argument(
        "--recursive", "-r", action="store_true",
        help="Process files in subdirectories")
    argument_parser.add_argument(
        "--mask", "-m", metavar="WILDCARD", action="append",
        help="Only use given filename pattern (should not include directory). "
        "Wildcard format is fnmatch compatible (?, *, [...]). This switch may "
        "be specified several times. By default all files are used. Don't "
        "forget to quote it on Linux")
    argument_parser.add_argument(
        "--list", "-l", action="store_true",
        help="Print list of files - sorted in the same order as MD5 will be "
        "computed. Sorting is lexicographical, first by directory "
        "(if --recursive specified) then by filename. If this switch "
        "specified MD5 not computed")
    argument_parser.add_argument(
        "--json", "-j", metavar="FILE:KEY_PATH[:[PREFIX][:SUFFIX]]",
        action="append",
        help="Puts resulting MD5 to given JSON file at given key path, "
        "optionally appending given prefix and suffix to it. Key path is a "
        "sequence of key name list indices - e.g. a/b[1][2]c/d. If file not "
        "existed - it is created. This switch may be specified several times")
    argument_parser.add_argument(
        "--progress", "-p", action="store_true",
        help="Print progress information")
    argument_parser.add_argument(
        "--stats", "-s", action="store_true", help="Print statistics")
    argument_parser.add_argument(
        "--threads", type=int, help=argparse.SUPPRESS)
    argument_parser.add_argument(
        "--chunk", type=int, default=16*1024*1024, help=argparse.SUPPRESS)
    argument_parser.add_argument(
        "--max_in_flight", type=int, default=50, help=argparse.SUPPRESS)
    argument_parser.add_argument(
        "FILES_AND_DIRS", nargs="*",
        help="Files and directories. Default is current directory. Yet if no "
        "arguments specified - help will be printed, so, say, '.' or '*' "
        "might be used")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)
    args = argument_parser.parse_args(argv)

    json_updater = JsonUpdater(args.json) if args.json else None

    start = datetime.datetime.now()
    fileinfos: List[FileInfo] = []
    for file_or_dir in (args.FILES_AND_DIRS or "."):
        fileinfos += get_files(file_dir_name=file_or_dir, masks=args.mask,
                               recursive=args.recursive)
    sort_files(fileinfos, with_path=args.recursive)
    total_size = sum(fi.size for fi in fileinfos)

    if args.list:
        for fileinfo in fileinfos:
            print(fileinfo.rel_path if args.recursive else fileinfo.basename)
        if args.stats:
            print("")
            print(f"Data length: {total_size / GIGA:.3f} GB")
        return
    md5_computer = Md5Computer(num_threads=args.threads, chunk_size=args.chunk,
                               max_in_flight=args.max_in_flight,
                               progress=args.progress)
    md5_str = md5_computer.calculate(fileinfos)
    duration = datetime.datetime.now() - start
    if json_updater is not None:
        json_updater.update(md5_str)
    else:
        print(md5_str)
    if args.stats:
        seconds = duration.total_seconds()
        print(f"Duration: {int(seconds) // 3600}h "
              f"{(int(seconds) // 60) % 60}m {seconds % 60:.2f}s")
        print(f"Data length: {total_size / GIGA:.3f} GB")
        print(f"Performance: {total_size / seconds / GIGA:.2f} GB/s")
        print(
            f"Operation was "
            f"{'CPU(MD5)' if md5_computer.cpu_bound else 'IO'}-bound")


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except KeyboardInterrupt:
        sys.exit(1)
