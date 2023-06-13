#!/usr/bin/env python3
# Computes path difference between two FS (ULS) Databases

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=wrong-import-order, invalid-name, too-many-statements
# pylint: disable=too-many-branches, too-many-nested-blocks
# pylint: disable=too-many-instance-attributes, too-many-arguments
# pylint: disable=too-few-public-methods, too-many-locals, protected-access

import argparse
from collections.abc import Iterator
import logging
import math
import os
import sqlalchemy as sa
import sys
from typing import Any, cast, Dict, List, NamedTuple, Optional, Set, Tuple


# START OF DATABASE SCHEMA-DEPENDENT STUFF

# Name of table with FS Paths
FS_TABLE_NAME = "uls"

# Name of table with Passive Repeaters
PR_TABLE_NAME = "pr"


class PathIdent(NamedTuple):
    """ FS Path table columns that globally uniquely identify the path

    Not FSID as it is not necessarily unique across different databases) """

    region: str
    callsign: str
    path_number: int
    freq_assigned_start_mhz: float
    freq_assigned_end_mhz: float

    def __str__(self):
        """ String representation for difference report """
        return f"{self.region}:{self.callsign}/{self.path_number}" \
            f"[{self.freq_assigned_start_mhz}-" \
            f"{self.freq_assigned_end_mhz}]MHz"

# Fields of FS Path table to NOT include into FS path hash (besides those that
# included into PathIdent)
FS_NO_HASH_FIELDS = ["fsid", "p_rp_num", "name"]

# Fields of Passive Repeater table to NOT include into FS path hash
PR_NO_HASH_FIELDS = ["id", "fsid", "prSeq"]

# Coordinate field names of some receiver in some database record
RxCoordFields = NamedTuple("RxCoordinates", [("lat", str), ("lon", str)])

# Fields in FS Path table that comprise coordinates of path points that affect
# AFC Engine computations. Specified in (lat, lon) order
FS_COORD_FIELDS = [RxCoordFields("rx_lat_deg", "rx_long_deg")]

# Fields in Passive Repeater table that comprise coordinates of path points.
# Specified in (lat, lon) order
PR_COORD_FIELDS = [RxCoordFields("pr_lat_deg", "pr_lon_deg")]

# Primary key field in FS Path table
FS_FSID_FIELD = "fsid"

# Foreign key field in Passive Repeater table
PR_FSID_FIELD = "fsid"

# Field in FS Path table that denotes the presence of passive Repeater in the
# path
FS_PR_FIELD = "p_rp_num"

# Field in Passive Repeater table that sorts Repeater in due order
PR_ORDER_FIELD = "prSeq"

# END OF DATABASE SCHEMA-DEPENDENT STUFF


def error(msg: str) -> None:
    """ Prints given msg as error message and exits abnormally """
    logging.error(msg)
    sys.exit(1)


def error_if(cond: Any, msg: str) -> None:
    """ If condition evaluates to true prints given msg as error message and
    exits abnormally """
    if cond:
        error(msg)


class FsPath:
    """ Information about FS Path

    Public attributes:
    ident -- Globally unique FS Path identifier
    fsid  -- Path primary key in DB

    Private attributes:
    _row_fields -- Dictionary of FS Path table row fields used in Path Hash
                   computation. None if fields computation was not requested
    _prs        -- List of per-repeater dictionaries of fields used in Path
                   Hash computation. None if fields computation was not
                   requested
    _tiles      -- Set of tiles containing receivers. None if its computation
                   was not requested
    _path_hash  -- Path hash (hash of pertinent non-identification fields in
                   the path) if its computation was requested, None otherwise
    """
    class Tile(NamedTuple):
        """ 1x1 degree tile containing some receiver """
        # Minimum latitude in north-positive degrees
        min_lat: int

        # Minimum longitude in east-positive degrees
        min_lon: int

        def __str__(self) -> str:
            """ String representation for difference report """
            return f"[{abs(self.min_lat)}-{abs(self.min_lat + 1)}]" \
                f"{'N' if self.min_lat >= 0 else 'S'}, " \
                f"[{abs(self.min_lon)}-{abs(self.min_lon + 1)}]" \
                f"{'E' if self.min_lon >= 0 else 'W'}"

        @classmethod
        def from_record(cls, row: "sa.engine.RowProxy",
                        rx_fields: List[RxCoordFields]) -> Set["FsPath.Tile"]:
            """ Make set of tiles according to given list of receiver
            coordinates """
            ret: Set["FsPath.Tile"] = set()
            for rf in rx_fields:
                ret.add(cls(min_lat=math.floor(row[rf.lat]),
                            min_lon=math.floor(row[rf.lon])))
            return ret

    class HashFields:
        """ Fields of some table that participate in FS Path hash computation

        Private attributes:
        self._field_infos -- List of _FieldInfo object that specify indices
                             (within row) and names of fields
        """
        # Field descriptor (index in DB row and name)
        _FieldInfo = NamedTuple("_FieldInfo", [("name", str), ("idx", int)])

        def __init__(self, table: sa.Table, exclude: List[str]) -> None:
            """ Constructor

            Arguments:
            table   -- SqlAlchemy table object (contains information about all
                       columns)
            exclude -- List of names of fields not to include
            """
            error_if(
                not all(ef in table.c for ef in exclude),
                f"Table '{table.name}' missing these fields: "
                f"{', '.join([ef for ef in exclude if ef not in table.c])}")
            self._field_infos: List["FsPath.HashFields._FieldInfo"] = []
            for idx, col in enumerate(table.c):
                if col.name not in exclude:
                    self._field_infos.append(
                        self._FieldInfo(name=col.name, idx=idx))

        def get_hash_fields(self, row: "sa.engine.RowProxy") \
                -> "Iterator[Tuple[str, Any]]":
            """ Iterator over fields of given row

            Yields (filed_name, field_value) tuples
            """
            for fi in self._field_infos:
                yield (fi.name, row[fi.idx])

    class _CoordFieldMapping(NamedTuple):
        """ Latitude/longitude field names """
        lat: str
        lon: str

        def make_tile(self, row: "sa.engine.RowProxy") -> "FsPath.Tile":
            """ Makes tile from latitude/longitude fields in given DB row """
            return FsPath.Tile(min_lat=math.floor(row[self.lat]),
                               min_lon=math.floor(row[self.lon]))

    def __init__(self, fs_row: "sa.engine.RowProxy",
                 conn: sa.engine.Connection,
                 pr_table: sa.Table, fs_hash_fields: "FsPath.HashFields",
                 pr_hash_fields: "FsPath.HashFields",
                 compute_hash: bool = False, compute_fields: bool = False,
                 compute_tiles: bool = False) -> None:
        """ Constructor

        Arguments:
        fs_row         -- Row from FS table
        conn           -- Connection (to use for reading Passive Repeater
                          table)
        pr_table       -- Passive Repeater SqlAlchemy table
        fs_hash_fields -- Hash fields iterator for FS table
        pr_hash_fields -- Hash fields iterator for Passive Repeater table
        compute_hash   -- True to compute path hash
        compute_fields -- True to compute fields' dictionary
        compute_tiles  -- True to compute receiver tiles
        """
        self.ident = PathIdent(**{f: fs_row[f] for f in PathIdent._fields})
        self.fsid = fs_row[FS_FSID_FIELD]
        self._row_fields: Optional[Dict[str, Any]] = \
            {} if compute_fields else None
        self._prs: Optional[List[Dict[str, Any]]] = \
            [] if compute_fields else None
        self._tiles: Optional[Set[FsPath.Tile]] = \
            set() if compute_tiles else None
        hash_fields: Optional[List[Any]] = [] if compute_hash else None
        for field_name, field_value in fs_hash_fields.get_hash_fields(fs_row):
            if hash_fields is not None:
                hash_fields.append(field_value)
            if self._row_fields is not None:
                self._row_fields[field_name] = field_value
        if self._tiles is not None:
            self._tiles |= self.Tile.from_record(fs_row, FS_COORD_FIELDS)
        if fs_row[FS_PR_FIELD]:
            sel = sa.select([pr_table]).\
                where(pr_table.c[PR_FSID_FIELD] == self.fsid).\
                order_by(pr_table.c[PR_ORDER_FIELD])
            for pr_row in conn.execute(sel):
                pr: Optional[Dict[str, Any]] = {} if self._prs is not None \
                    else None
                if self._prs is not None:
                    self._prs.append(pr)
                for field_name, field_value in \
                        pr_hash_fields.get_hash_fields(pr_row):
                    if hash_fields is not None:
                        hash_fields.append(field_value)
                    if pr is not None:
                        pr[field_name] = field_value
                if self._tiles is not None:
                    self._tiles |= self.Tile.from_record(pr_row,
                                                         PR_COORD_FIELDS)
        self._path_hash: Optional[int] = \
            hash(tuple(hash_fields)) if hash_fields is not None else None

    def path_hash(self) -> int:
        """ Returns path hash """
        assert self._path_hash is not None
        return self._path_hash

    def tiles(self) -> Set["FsPath.Tile"]:
        """ Returns receiver tiles """
        assert self._tiles is not None
        return self._tiles

    def diff_report(self, other: "FsPath") -> str:
        """ Returns report on difference with other FsPath object """
        assert self._row_fields is not None
        assert self._prs is not None
        diffs: List[str] = []
        for field, this_value in self._row_fields.items():
            other_value = other._row_fields[field]
            if this_value != other_value:
                diffs.append(f"'{field}': '{this_value}' vs '{other_value}'")
        if len(self._prs) != len(other._prs):
            diffs.append(f"Different number of repeaters: "
                         f"{len(self._prs)} vs {len(other._prs)}")
        else:
            for pr_idx, this_pr_dict in enumerate(self._prs):
                other_pr_dict = other._prs[pr_idx]
                for field, this_value in this_pr_dict.items():
                    other_value = other_pr_dict[field]
                    if this_value != other_value:
                        diffs.append(f"Repeater #{pr_idx + 1}, '{field}': "
                                     f"'{this_value}' vs '{other_value}'")
        return ", ".join(diffs)

    @classmethod
    def compute_hash_fields(cls, fs_table: sa.Table, pr_table: sa.Table) \
            -> Tuple["FsPath.HashFields", "FsPath.HashFields"]:
        """ Computes hash fields' iterators

        Arguments:
        fs_table -- SqlAlchemy table object for FS
        pr_table -- SqlAlchemy table object  Passive Repeater
        Returns (FS_iterator, PR_Iterator) tuple
        """
        missing_ident_fields = \
            [f for f in PathIdent._fields if f not in fs_table.c]
        error_if(
            missing_ident_fields,
            f"Table '{fs_table.name}' does not contain following path "
            f"identification fields: {', '.join(missing_ident_fields)}")
        return \
            (cls.HashFields(
                table=fs_table,
                exclude=list(PathIdent._fields) + FS_NO_HASH_FIELDS),
             cls.HashFields(table=pr_table, exclude=PR_NO_HASH_FIELDS))


class Db:
    """ Database access (most part of it)

    Public attributes
    filename       -- SQLite database filename
    conn           -- Database connection
    fs_table       -- SqlAlchemy table object for FS
    pr_table       -- SqlAlchemy table object  Passive Repeater
    fs_hash_fields -- Iterator over hash fields in FS record
    pr_hash_fields -- Iterator over hash fields in Passive Repeater record

    """
    def __init__(self, filename: str) -> None:
        """ Constructor

        Arguments:
        filename       -- SQLite database file
        """
        error_if(not os.path.isfile(filename),
                 f"FS SQLite database '{filename}' not found")
        self.filename = filename
        try:
            self._engine = sa.create_engine("sqlite:///" + self.filename)
            self._metadata = sa.MetaData()
            self._metadata.reflect(bind=self._engine)
            self.conn = self._engine.connect()
        except sa.exc.SQLAlchemyError as ex:
            error(f"Error opening FS database '{self.filename}': {ex}")
        error_if(FS_TABLE_NAME not in self._metadata.tables,
                 f"'{self.filename}' does not contain table '{FS_TABLE_NAME}'")
        self.fs_table = self._metadata.tables[FS_TABLE_NAME]
        error_if(PR_TABLE_NAME not in self._metadata.tables,
                 f"'{self.filename}' does not contain table '{PR_TABLE_NAME}'")
        self.pr_table = self._metadata.tables[PR_TABLE_NAME]
        self.fs_hash_fields, self.pr_hash_fields = \
            FsPath.compute_hash_fields(fs_table=self.fs_table,
                                       pr_table=self.pr_table)

    def close(self):
        """ Close connection """
        self.conn.close()

    def all_fs_rows(self) -> "Iterator[sa.engine.RowProxy]":
        """ Iterator that reads all FS Path rows """
        try:
            for fs_row in self.conn.execute(sa.select([self.fs_table])).\
                    fetchall():
                yield fs_row
        except sa.exc.SQLAlchemyError as ex:
            error(f"Error reading '{self.fs_table.name}' table from FS "
                  f"database '{self.filename}': {ex}")

    def fs_row_by_fsid(self, fsid: int) -> "sa.engine.RowProxy":
        """ Fetch FS row by FSID """
        try:
            ret = self.conn.execute(sa.select([self.fs_table]).\
                where(self.fs_table.c[FS_FSID_FIELD] == fsid)).first()
            error_if(
                ret is None,
                f"Record with FSID of {fsid} not found in table "
                f"'{self.fs_table.name}' of FS database '{self.filename}'")
            assert ret is not None
            return ret
        except sa.exc.SQLAlchemyError as ex:
            error(f"Error reading '{self.fs_table.name}' table from FS "
                  f"database '{self.filename}': {ex}")
            raise  # Will never happen, just to pacify pylint


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = \
        argparse.ArgumentParser(
            description="Computes path difference between two FS (aka ULS) "
            "Databases")
    argument_parser.add_argument(
        "--report_tiles", action="store_true",
        help="Print 1x1 degree tiles affected by difference")
    argument_parser.add_argument(
        "--report_paths", action="store_true",
        help="Print paths that constitute difference")
    argument_parser.add_argument(
        "DB1",
        help="First FS (aka ULS) sqlite3 database file to compare")
    argument_parser.add_argument(
        "DB2",
        help="Second FS (aka ULS) sqlite3 database file to compare")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)

    args = argument_parser.parse_args(argv)

    console_handler = logging.StreamHandler()
    console_handler.setFormatter(
        logging.Formatter(
            f"{os.path.basename(__file__)}. "
            f"%(levelname)s: %(asctime)s %(message)s"))
    logging.getLogger().addHandler(console_handler)
    logging.getLogger().setLevel(logging.INFO)

    dbs: List[Optional[Db]] = [None, None]
    try:
        dbs = [Db(args.DB1), Db(args.DB2)]

        IdentHash = NamedTuple("IdentHash", [("ident", PathIdent),
                                             ("hash", int)])
        IdentHashToFsid = Dict[IdentHash, int]

        # Per-database dictionaries that map (FS Ident, FS Hash) pairs to FSIDs
        # Building them...
        ident_hash_to_fsid_by_db: List[IdentHashToFsid] = []
        for db in dbs:
            ident_hash_to_fsid: Dict[Tuple[PathIdent, int], int] = {}
            ident_hash_to_fsid_by_db.append(ident_hash_to_fsid)
            try:
                for fs_row in db.all_fs_rows():
                    fs_path = \
                        FsPath(
                            fs_row=fs_row, conn=db.conn, pr_table=db.pr_table,
                            fs_hash_fields=db.fs_hash_fields,
                            pr_hash_fields=db.pr_hash_fields,
                            compute_hash=True)
                    ident_hash_to_fsid[
                        IdentHash(ident=fs_path.ident,
                                  hash=cast(int, fs_path.path_hash()))] = \
                        fs_path.fsid
            except sa.exc.SQLAlchemyError as ex:
                error(f"Error reading FS database '{db.filename}': {ex}")

        # Idents of FS Paths with differences mapped to FSID dictionaries
        # (dictionaries indexed by DB index (0 or 1) to correspondent FSIDs).
        # Building it
        ident_to_fsids_diff: Dict[PathIdent, Dict[int, int]] = {}
        # Loop by unique (FS Ident, FS Hash) pairs
        for unique_ident_hash in (set(ident_hash_to_fsid_by_db[0].keys()) ^
                                  set(ident_hash_to_fsid_by_db[1].keys())):
            db_idx = 0 if unique_ident_hash in ident_hash_to_fsid_by_db[0] \
                else 1
            ident_to_fsids_diff.setdefault(unique_ident_hash.ident,
                                           {})[db_idx] = \
                ident_hash_to_fsid_by_db[db_idx][unique_ident_hash]

        # Making brief report
        print(f"Paths in DB1: {len(ident_hash_to_fsid_by_db[0])}")
        print(f"Paths in DB2: {len(ident_hash_to_fsid_by_db[1])}")
        print(f"Different paths: {len(ident_to_fsids_diff)}")

        # Detailed reports
        if args.report_paths or args.report_tiles:
            # RX tiles belonging to unique paths
            tiles_of_difference: Set[FsPath.Tile] = set()
            # Loop by unique FS identifiers
            for path_ident in sorted(ident_to_fsids_diff.keys()):
                # Per-database FsPoath objects
                paths: List[Optional[FsPath]] = []
                # Filling it (and adding tiles)
                for db_idx, db in enumerate(dbs):
                    fsid = ident_to_fsids_diff[path_ident].get(db_idx)
                    if fsid is None:
                        paths.append(None)
                        continue
                    try:
                        fs_row = db.fs_row_by_fsid(fsid)
                        fs_path = \
                            FsPath(
                                fs_row=fs_row, conn=db.conn,
                                pr_table=db.pr_table,
                                fs_hash_fields=db.fs_hash_fields,
                                pr_hash_fields=db.pr_hash_fields,
                                compute_fields=args.report_paths,
                                compute_tiles=args.report_tiles)
                    except sa.exc.SQLAlchemyError as ex:
                        error(
                            f"Error reading FS database '{db.filename}': {ex}")
                    if args.report_tiles:
                        tiles_of_difference |= fs_path.tiles()
                    paths.append(fs_path)
                if args.report_paths:
                    if paths[0] is not None:
                        if paths[1] is not None:
                            diff_report = paths[0].diff_report(paths[1])
                        else:
                            diff_report = "Only present in DB1"
                    else:
                        assert paths[1] is not None
                        diff_report = "Only present in DB2"
                    print(f"Difference in path {path_ident}: {diff_report}")
            if args.report_tiles:
                for tile in sorted(tiles_of_difference):
                    print(f"Difference in tile {tile}")
    except KeyboardInterrupt:
        pass
    finally:
        for db in dbs:
            if db:
                db.close()


if __name__ == "__main__":
    main(sys.argv[1:])
