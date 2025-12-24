#!/usr/bin/env python3
""" Computes path difference between two FS (ULS) Databases """

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# pylint: disable=wrong-import-order, invalid-name, too-many-statements
# pylint: disable=too-many-branches, too-many-nested-blocks
# pylint: disable=too-many-instance-attributes, too-many-arguments
# pylint: disable=too-few-public-methods, too-many-locals, protected-access
# pylint: disable=too-many-positional-arguments

import argparse
import enum
import json
import logging
import math
import os
import sqlalchemy as sa
import sys
from typing import Any, cast, Dict, Iterator, List, NamedTuple, NoReturn, \
    Optional, Set, Tuple


# START OF DATABASE SCHEMA-DEPENDENT STUFF

# Name of table with FS Paths
FS_TABLE_NAME = "uls"

# Name of table with Passive Repeaters
PR_TABLE_NAME = "pr"

# Name of table with Restricted Areas
RAS_TABLE_NAME = "ras"


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

# RAS table fields to NOT include into hash
RAS_NO_HASH_FIELDS = ["rasid"]

# Primary key field in FS Path table
FS_FSID_FIELD = "fsid"

# Foreign key field in Passive Repeater table
PR_FSID_FIELD = "fsid"

# Field in FS Path table that denotes the presence of passive Repeater in the
# path
FS_PR_FIELD = "p_rp_num"

# Field in FS Path table containing optional TX latitude in north-positive
# degrees
FS_TX_LAT_DEG_FIELD = "tx_lat_deg"

# Field in FS Path table containing optional TX longitude in east-positive
# degrees
FS_TX_LON_DEG_FIELD = "tx_long_deg"

# Field in FS Path table containing optional azimuth from RX to TX in degrees
FS_TX_AZIMUTH_FIELD = "azimuth_angle_to_tx"

# Field in FS Path table containing TX latitude in north-positive degrees
FS_RX_LAT_DEG_FIELD = "rx_lat_deg"

# Field in FS Path table containing TX longitude in east-positive degrees
FS_RX_LON_DEG_FIELD = "rx_long_deg"

# Field in Passive Repeater table that sorts Repeater in due order
PR_ORDER_FIELD = "prSeq"

# Field in Passive Repeater table containing optional PR latitude in
# north-positive degrees
PR_LAT_DEG_FIELD = "pr_lat_deg"

# Field in Passive Repeater table containing optional PR longitude in
# east-positive degrees
PR_LON_DEG_FIELD = "pr_lon_deg"

# Field in Restricted Areas table containing restricted area region
RAS_REGION_FIELD = "region"

# Field in Restricted Areas table containing restricted area name
RAS_NAME_FIELD = "name"

# Field in Restricted Areas table containing restricted area location
RAS_LOCATION_FIELD = "location"

# Field in Restricted Areas table containing restricted area type name
RAS_TYPE_FIELD = "exclusionZone"

# Fields in Restricted Areas table containing latitudes/longitudes
# (north/east positive degrees) of angle oints of first/second bounding
# rectangles
RAS_RECT1_LAT1_FIELD = "rect1lat1"
RAS_RECT1_LON1_FIELD = "rect1lon1"
RAS_RECT1_LAT2_FIELD = "rect1lat2"
RAS_RECT1_LON2_FIELD = "rect1lon2"
RAS_RECT2_LAT1_FIELD = "rect2lat1"
RAS_RECT2_LON1_FIELD = "rect2lon1"
RAS_RECT2_LAT2_FIELD = "rect2lat2"
RAS_RECT2_LON2_FIELD = "rect2lon2"

# Fields in Restricted Areas table containing latitude/longitude
# (north/east positive degrees) of center point of restricted area
RAS_CENTER_LAT_FIELD = "centerLat"
RAS_CENTER_LON_FIELD = "centerLon"

# Field in Restricted Areas table containing radius of restricted area in km
RAS_RADIUS_KM_FIELD = "radiusKm"

# Field in Restricted Areas table containing height of restricted area in m
RAS_AGL_M_FIELD = "heightAGL"


# END OF DATABASE SCHEMA-DEPENDENT STUFF


def error(msg: str) -> NoReturn:
    """ Prints given msg as error message and exits abnormally """
    logging.error(msg)
    sys.exit(1)


def error_if(cond: Any, msg: str) -> None:
    """ If condition evaluates to true prints given msg as error message and
    exits abnormally """
    if cond:
        error(msg)


class HashFields:
    """ Fields of some table that participate in hash computation

    Private attributes:
    self._field_infos -- List of _FieldInfo object that specify indices (within
                         row) and names of fields
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
        error_if(not all(ef in table.c for ef in exclude),
                 f"Table '{table.name}' missing these fields: "
                 f"{', '.join([ef for ef in exclude if ef not in table.c])}")
        self._field_infos: List["HashFields._FieldInfo"] = []
        for idx, col in enumerate(table.c):
            if col.name not in exclude:
                self._field_infos.append(
                    self._FieldInfo(name=col.name, idx=idx))

    def get_hash_fields(self, row: sa.engine.Row) -> Iterator[Tuple[str, Any]]:
        """ Iterator over fields of given row

        Yields (filed_name, field_value) tuples
        """
        for fi in self._field_infos:
            yield (fi.name, row[fi.idx])


class Beam(NamedTuple):
    """ Beam information """
    # RX latitude in North positive degrees
    rx_lat_deg: float
    # RX longitude in east-positive degrees
    rx_lon_deg: float
    # TX latitude in north-positive degrees or None
    tx_lat_deg: Optional[float] = None
    # TX longitude in east-positive degrees or None
    tx_lon_deg: Optional[float] = None
    # Azimuth to TX or None
    tx_azimuth_deg: Optional[float] = None

    def as_dict(self) -> Dict[str, float]:
        """ Returns dictionary representation where only non-None fields
        present """
        ret = self._asdict()
        for f in [k for k, v in ret.items() if v is None]:
            del ret[f]
        return ret


class FsPath:
    """ Information about FS Path

    Public attributes:
    ident -- PathIdent - globally unique FS Path identifier
    fsid  -- Path primary key in DB

    Private attributes:
    _row_fields -- Dictionary of FS Path table row fields used in Path Hash
                   computation. None if fields computation was not requested
    _prs        -- List of per-repeater dictionaries of fields used in Path
                   Hash computation. None if fields computation was not
                   requested
    _beams      -- Liat of beams. None if its computation was not requested
    _path_hash  -- Path hash (hash of pertinent non-identification fields in
                   the path) if its computation was requested, None otherwise
    """
    def __init__(self, fs_row: sa.engine.Row, conn: sa.engine.Connection,
                 pr_table: sa.Table, fs_hash_fields: HashFields,
                 pr_hash_fields: HashFields, compute_hash: bool = False,
                 compute_fields: bool = False, compute_beams: bool = False) \
            -> None:
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
        compute_beams  -- True to compute path beams
        """
        self.ident = PathIdent(**{f: fs_row[f] for f in PathIdent._fields})
        self.fsid = fs_row[FS_FSID_FIELD]
        self._row_fields: Optional[Dict[str, Any]] = \
            {} if compute_fields else None
        self._prs: Optional[List[Dict[str, Any]]] = \
            [] if compute_fields else None
        self._beams: Optional[List[Beam]] = [] if compute_beams else None
        hash_fields: Optional[List[Any]] = [] if compute_hash else None
        for field_name, field_value in fs_hash_fields.get_hash_fields(fs_row):
            if hash_fields is not None:
                hash_fields.append(field_value)
            if self._row_fields is not None:
                self._row_fields[field_name] = field_value
        prev_tx_lat_deg: Optional[float] = \
            fs_row[FS_TX_LAT_DEG_FIELD] if self._beams is not None else None
        prev_tx_lon_deg: Optional[float] = \
            fs_row[FS_TX_LON_DEG_FIELD] if self._beams is not None else None
        if fs_row[FS_PR_FIELD]:
            sel = sa.select(pr_table).\
                where(pr_table.c[PR_FSID_FIELD] == self.fsid).\
                order_by(pr_table.c[PR_ORDER_FIELD])
            for pr_row in conn.execute(sel):
                pr: Optional[Dict[str, Any]] = {} if self._prs is not None \
                    else None
                if self._prs is not None:
                    assert pr is not None
                    self._prs.append(pr)
                for field_name, field_value in \
                        pr_hash_fields.get_hash_fields(pr_row):
                    if hash_fields is not None:
                        hash_fields.append(field_value)
                    if pr is not None:
                        pr[field_name] = field_value
                if self._beams is not None:
                    self._beams.append(
                        Beam(rx_lat_deg=pr_row[PR_LAT_DEG_FIELD],
                             rx_lon_deg=pr_row[PR_LON_DEG_FIELD],
                             tx_lat_deg=prev_tx_lat_deg,
                             tx_lon_deg=prev_tx_lon_deg))
                    prev_tx_lat_deg, prev_tx_lon_deg = \
                        self._beams[-1].rx_lat_deg, self._beams[-1].rx_lon_deg
        if self._beams is not None:
            self._beams.append(
                Beam(rx_lat_deg=fs_row[FS_RX_LAT_DEG_FIELD],
                     rx_lon_deg=fs_row[FS_RX_LON_DEG_FIELD],
                     tx_lat_deg=prev_tx_lat_deg, tx_lon_deg=prev_tx_lon_deg,
                     tx_azimuth_deg=fs_row[FS_TX_AZIMUTH_FIELD]
                     if (prev_tx_lat_deg is None) or (prev_tx_lon_deg is None)
                     else None))
        self._path_hash: Optional[int] = \
            hash(tuple(hash_fields)) if hash_fields is not None else None

    def path_hash(self) -> int:
        """ Returns path hash """
        assert self._path_hash is not None
        return self._path_hash

    def beams(self) -> List[Beam]:
        """ Returns list of Beams """
        assert self._beams is not None
        return self._beams

    def diff_report(self, other: "FsPath") -> str:
        """ Returns report on difference with other FsPath object """
        assert self._row_fields is not None
        assert self._prs is not None
        assert other._row_fields is not None
        assert other._prs is not None
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
            -> Tuple[HashFields, HashFields]:
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
            (HashFields(
                table=fs_table,
                exclude=list(PathIdent._fields) + FS_NO_HASH_FIELDS),
             HashFields(table=pr_table, exclude=PR_NO_HASH_FIELDS))


# Bounding rectangle ib north/east positive latitudes/longitudes
class BoundingRect(NamedTuple):
    """ Geographic bounding rectangle """
    # Minimum latitude in north-positive degrees
    min_lat_deg: float
    # Maximum latitude in north-positive degrees
    max_lat_deg: float
    # Minimum longitude in east-positive degrees
    min_lon_deg: float
    # Maximum longitude in east-positive degrees
    max_lon_deg: float

    def as_dict(self) -> Dict[str, float]:
        """ Self in dictionary format """
        return self._asdict()


class Ras:
    """ Information about single restricted area:

    Public attributes:
    location -- Location name

    Private attributes:
    _boundary    -- List of BoundingRect objects containing restricted area
    _hash_fields -- Hash fields (represent restricted area identity)
    """
    # Earth radius, same as cconst.cpp CConst::earthRadius
    EARTH_RADIUS_KM = 6378.137
    # Maximum imaginable SP AP height above ground in kilometers
    MAX_AP_AGL_KM = 1

    class RasType(enum.Enum):
        """ RAS definition types. Values are DB names of these types """
        Rect = "One Rectangle"
        TwoRect = "Two Rectangles"
        Circle = "Circle"
        Horizon = "Horizon Distance"

    def __init__(self, fs_row: sa.engine.Row, hash_fields: HashFields) -> None:
        """ Constructor

        Arguments:
        fs_row      -- Database row to construct from
        hash_fields -- Retriever of identity fields
        """
        try:
            self.location = f"{fs_row[RAS_REGION_FIELD]}: " \
                f"{fs_row[RAS_NAME_FIELD]} @ {fs_row[RAS_LOCATION_FIELD]}"
            self._boundary: List[BoundingRect] = []
            ras_type = [rt for rt in self.RasType
                        if rt.value == fs_row[RAS_TYPE_FIELD]][0]
            if ras_type in (self.RasType.Rect, self.RasType.TwoRect):
                for lat1_f, lon1_f, lat2_f, lon2_f in \
                        [(RAS_RECT1_LAT1_FIELD, RAS_RECT1_LON1_FIELD,
                          RAS_RECT1_LAT2_FIELD, RAS_RECT1_LON2_FIELD)] + \
                        ([(RAS_RECT2_LAT1_FIELD, RAS_RECT2_LON1_FIELD,
                           RAS_RECT2_LAT2_FIELD, RAS_RECT2_LON2_FIELD)]
                         if ras_type == self.RasType.TwoRect else []):
                    lon1 = fs_row[lon1_f]
                    lon2 = fs_row[lon2_f]
                    center_lon = (lon1 + lon2) / 2
                    if (lon2 - center_lon) > 180:
                        lon2 -= 360
                    elif (lon2 - center_lon) < -180:
                        lon2 += 360
                    self._boundary.append(
                        BoundingRect(
                            min_lat_deg=min(fs_row[lat1_f], fs_row[lat2_f]),
                            max_lat_deg=max(fs_row[lat1_f], fs_row[lat2_f]),
                            min_lon_deg=min(lon1, lon2),
                            max_lon_deg=max(lon1, lon2)))
            elif ras_type in (self.RasType.Circle, self.RasType.Horizon):
                center_lat_deg = fs_row[RAS_CENTER_LAT_FIELD]
                center_lon_deg = fs_row[RAS_CENTER_LON_FIELD]
                if ras_type == self.RasType.Circle:
                    radius_km = fs_row[RAS_RADIUS_KM_FIELD]
                else:
                    radius_km = \
                        math.sqrt(2 * Ras.EARTH_RADIUS_KM * 4 / 3) * \
                        (math.sqrt(fs_row[RAS_AGL_M_FIELD] / 1000) +
                         math.sqrt(Ras.MAX_AP_AGL_KM))
                radius_rad = radius_km / Ras.EARTH_RADIUS_KM
                lat_radius_deg = math.degrees(radius_rad)
                lon_radius_deg = \
                    math.degrees(
                        radius_rad / math.cos(math.radians(center_lat_deg)))
                self._boundary.append(
                    BoundingRect(
                        min_lat_deg=center_lat_deg - lat_radius_deg,
                        max_lat_deg=center_lat_deg + lat_radius_deg,
                        min_lon_deg=center_lon_deg - lon_radius_deg,
                        max_lon_deg=center_lon_deg + lon_radius_deg))
            else:
                raise ValueError(f"Unsupported RAS type: {ras_type.name}")
        except (LookupError, ValueError, sa.exc.SQLAlchemyError) as ex:
            error(
                f"RAS record '{fs_row}' has unsupported structure: {repr(ex)}")
        self._hash_fields = \
            tuple(v for _, v in hash_fields.get_hash_fields(fs_row))

    def boundary(self) -> List[BoundingRect]:
        """ Returns sequence of coveriing bounding rectangles """
        return self._boundary

    def __eq__(self, other: Any) -> bool:
        """ Equality comparator """
        return isinstance(other, self.__class__) and \
            (self._hash_fields == other._hash_fields)

    def __hash__(self) -> int:
        """ Hash computer """
        return hash(self._hash_fields)


class Db:
    """ Database access (most part of it)

    Public attributes
    filename        -- SQLite database filename
    conn            -- Database connection
    fs_table        -- SqlAlchemy table object for FS
    pr_table        -- SqlAlchemy table object for Passive Repeater
    ras_table       -- Optional SqlAlchemy table object for RAS
    fs_hash_fields  -- Iterator over hash fields in FS record
    pr_hash_fields  -- Iterator over hash fields in Passive Repeater record
    ras_hash_fields -- Optional iterator over hash fields in RAS record
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
            self._engine = \
                sa.create_engine(f"sqlite:///{self.filename}?mode=ro",
                                 connect_args={"uri": True})
            self._metadata = sa.MetaData()
            self._metadata.reflect(bind=self._engine)
            self.conn = cast(sa.engine.Connection, self._engine.connect())
        except sa.exc.SQLAlchemyError as ex:
            error(f"Error opening FS database '{self.filename}': {ex}")
        assert self._metadata.tables is not None
        error_if(FS_TABLE_NAME not in self._metadata.tables,
                 f"'{self.filename}' does not contain table '{FS_TABLE_NAME}'")
        self.fs_table = self._metadata.tables[FS_TABLE_NAME]
        error_if(PR_TABLE_NAME not in self._metadata.tables,
                 f"'{self.filename}' does not contain table '{PR_TABLE_NAME}'")
        self.pr_table = self._metadata.tables[PR_TABLE_NAME]
        self.ras_table = self._metadata.tables.get(RAS_TABLE_NAME)
        self.fs_hash_fields, self.pr_hash_fields = \
            FsPath.compute_hash_fields(fs_table=self.fs_table,
                                       pr_table=self.pr_table)
        self.ras_hash_fields = \
            None if self.ras_table is None \
            else HashFields(table=self.ras_table, exclude=RAS_NO_HASH_FIELDS)

    def close(self):
        """ Close connection """
        self.conn.close()

    def all_fs_rows(self) -> Iterator[sa.engine.Row]:
        """ Iterator that reads all FS Path rows """
        try:
            yield from self.conn.execute(sa.select(self.fs_table)).fetchall()
        except sa.exc.SQLAlchemyError as ex:
            error(f"Error reading '{self.fs_table.name}' table from FS "
                  f"database '{self.filename}': {ex}")

    def fs_row_by_fsid(self, fsid: int) -> sa.engine.Row:
        """ Fetch FS row by FSID """
        try:
            ret = \
                self.conn.execute(
                    sa.select(self.fs_table).
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

    def all_ras_rows(self) -> Iterator[sa.engine.Row]:
        """ Iterator that reads all RAS rows """
        if self.ras_table is None:
            return
        try:
            yield from self.conn.execute(sa.select(self.ras_table)).fetchall()
        except sa.exc.SQLAlchemyError as ex:
            error(f"Error reading '{self.ras_table.name}' table from FS "
                  f"database '{self.filename}': {ex}")


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
        "--paths", action="store_true",
        help="Print what paths/RASs were changed")
    argument_parser.add_argument(
        "--invalidation", metavar="FILENAME",
        help="Write invalidation details into JSON file")
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

    dbs: List[Db] = []
    try:
        dbs.append(Db(args.DB1))
        dbs.append(Db(args.DB2))

        IdentHash = NamedTuple("IdentHash", [("ident", PathIdent),
                                             ("hash", int)])
        # Per-database dictionaries that map (FS Ident, FS Hash) pairs to FSIDs
        # Building them...
        ident_hash_to_fsid_by_db: List[Dict[IdentHash, int]] = []
        for db in dbs:
            ident_hash_to_fsid: Dict[IdentHash, int] = {}
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

        # Reading RAS tables from databases
        ras_sets: List[Set[Ras]] = []
        for db in dbs:
            ras_sets.append(set())
            for fs_row in db.all_ras_rows():
                assert db.ras_hash_fields is not None
                ras_sets[-1].add(Ras(fs_row=fs_row,
                                 hash_fields=db.ras_hash_fields))
        ras_difference = ras_sets[0] ^ ras_sets[1]

        # Making brief report
        print(f"Paths in DB1: {len(ident_hash_to_fsid_by_db[0])}")
        print(f"Paths in DB2: {len(ident_hash_to_fsid_by_db[1])}")
        print(f"Different paths: {len(ident_to_fsids_diff)}")
        print(f"Different RAS entries: {len(ras_difference)}")

        # Detailed reports
        if args.paths or args.invalidation:
            invalidation_dict: Dict[str, Any] = {"beams": [], "rectangles": []}
            # Loop by unique FS identifiers
            for path_ident in sorted(ident_to_fsids_diff.keys()):
                # Per-database FsPoath objects
                paths: List[Optional[FsPath]] = []
                # Filling it (and adding beams)
                for db_idx, db in enumerate(dbs):
                    fsid = ident_to_fsids_diff[path_ident].get(db_idx)
                    if fsid is None:
                        paths.append(None)
                        continue
                    try:
                        fs_path = \
                            FsPath(
                                fs_row=db.fs_row_by_fsid(fsid), conn=db.conn,
                                pr_table=db.pr_table,
                                fs_hash_fields=db.fs_hash_fields,
                                pr_hash_fields=db.pr_hash_fields,
                                compute_fields=args.paths,
                                compute_beams=bool(args.invalidation))
                    except sa.exc.SQLAlchemyError as ex:
                        error(
                            f"Error reading FS database '{db.filename}': {ex}")
                    if args.invalidation:
                        invalidation_dict["beams"] += \
                            [beam.as_dict() for beam in fs_path.beams()]
                    paths.append(fs_path)
                if args.paths:
                    if paths[0] is not None:
                        if paths[1] is not None:
                            diff_report = paths[0].diff_report(paths[1])
                        else:
                            diff_report = "Only present in DB1"
                    else:
                        assert paths[1] is not None
                        diff_report = "Only present in DB2"
                    print(f"Difference in path {path_ident}: {diff_report}")
            if args.paths:
                for db_idx, ras_set in enumerate(ras_sets):
                    for ras in (ras_set - ras_sets[1 - db_idx]):
                        print(f"RAS '{ras.location}' from DB{db_idx + 1} not "
                              f"present in or different from "
                              f"DB{1 - db_idx + 1}")
            if args.invalidation:
                for ras in ras_difference:
                    invalidation_dict["rectangles"] += \
                        [br.as_dict() for br in ras.boundary()]
                try:
                    with open(args.invalidation, "w", encoding="utf-8") as f:
                        json.dump(invalidation_dict, f, indent=4)
                except OSError as ex:
                    error(f"Error writing invalidation report to "
                          f"'{args.invalidation}': {ex}")
    except KeyboardInterrupt:
        pass
    finally:
        for db in dbs:
            if db:
                db.close()


if __name__ == "__main__":
    main(sys.argv[1:])
