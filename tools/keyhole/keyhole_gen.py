#!/usr/bin/env python3
""" Keyhole shape converter/demonstrator """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

# pylint: disable=wrong-import-order, too-many-statements, too-many-locals
# pylint: disable=too-many-arguments, too-many-positional-arguments
# pylint: disable=consider-using-enumerate, too-many-branches
# pylint: disable=too-few-public-methods, invalid-name

import argparse
import copy
import enum
import json
import logging
import math
import matplotlib.pyplot
import operator
import os
import shapely
import shapely.wkb
import shapely.wkt
import sys
from typing import Any, Callable, List, NamedTuple, Optional, NoReturn, \
    Tuple, Union

# Default Shapely shape simplification tolerance in meters
DEFAULT_TOLERANCE = 100


def error(msg: str) -> NoReturn:
    """ Prints given msg as error message and exit abnormally """
    logging.error(msg)
    sys.exit(1)


def error_if(cond: Any, msg: str) -> None:
    """ If condition evaluates to true prints given msg as error message and
    exits abnormally """
    if cond:
        error(msg)


class FormatInfo(NamedTuple):
    """ Information about output format """
    # Description of format for help message
    dsc: str
    # Conversion function
    dumper: Callable[[shapely.Geometry], Union[bytes, str]]
    # True for default format
    is_default: bool = False


def postgis_dumps(polygon: shapely.Polygon) -> str:
    """ Returns string that defines keyhole image as a PostGIS geography
    polygon to use in WHERE ST_Covers() expression.
    
    Returns definition with these Jinja-style template inserts:
    {{ CENTER_LAT_DEG }} -- Center latitude in north-positive WGS84 degrees
    {{ CENTER_LON_DEG }} -- Center longitude in east-positive WGS84 degrees
    {{ ROTATION_RAD }}   -- Rotation angle in radians """
    # Point in polar coordinate system
    rotated_poly = \
        [f"ST_Project("
         f"ST_Point({{{{ CENTER_LON_DEG }}}}, {{{{ CENTER_LAT_DEG }}}}, "
         f"4329), "
         f"{math.sqrt(x**2 + y**2)}, "
         f"{math.atan2(y, x)} + {{{{ ROTATION_RAD }}}})::geometry"
         for y, x in polygon.boundary.coords]
    return f"ST_Polygon(ST_MakeLine(ARRAY[{', '.join(rotated_poly)}]), " \
        f"4326)::geography"


class OutputFormat(enum.Enum):
    """ Output format descriptor """
    wkb = FormatInfo(dsc="Well Known Binary geometry format",
                     dumper=shapely.wkb.dumps)
    wkt = FormatInfo(dsc="Well Known Text geometry format",
                     dumper=shapely.wkt.dumps)
    sql = FormatInfo(dsc="PostGIS definition of keyhole polygon with template "
                     "inserts ({{ CENTER_LAT_DEG }} for latitude in degrees, "
                     "{{ CENTER_LON_DEG }} for longitude in degrees, "
                     "{{ ROTATION_RAD }} for rotation in radians)",
                     dumper=postgis_dumps, is_default=True)


class SimplificationInfo(NamedTuple):
    """ Descriptor of simplification """
    # Description of format for help message
    dsc: str


class Simplification(enum.Enum):
    """ Information about source keyhole simplifications """
    convhull = SimplificationInfo("Convex Hull")
    unjag = SimplificationInfo(dsc="Ad-hoc fill jags")


class HalfBoundary:
    """ Keyhole image half-boundary and related operations

    Private attributes:
    _half_boundary -- List of AD (angle-distance pair) objects
    _conv_hull     -- True to return convex hull polygon
    """
    class AD(NamedTuple):
        """ Angle-distance pair - element of keyhole boundary """
        deg: float  # Angle in degrees
        m: float    # Distance in meters

        def extend(self, extent: float) -> "HalfBoundary.AD":
            """ Return AD with distance changed by given value """
            return HalfBoundary.AD(deg=self.deg, m=self.m + extent)

        def sin(self) -> float:
            """ Angle sine """
            return math.sin(math.radians(self.deg))

        def cos(self) -> float:
            """ Angle cosine """
            return math.cos(math.radians(self.deg))

    def __init__(self, angles_deg: Optional[List[float]] = None,
                 distances_m: Optional[List[float]] = None,
                 half_boundary: Optional["HalfBoundary"] = None,
                 extent: float = 0.,
                 simplify: Optional[Simplification] = None) -> None:
        """ Constructor either from angle/pair list or from half-boundary

        Arguments:
        angle_deg     -- List of angles (in degrees) of source half-boundary or
                         None
        distances_m   -- List of distances (in meters) of source half-boundary
                         or None
        half_boundary -- Explicit source half-boundary or None
        extent        -- Extend all distance from source half boundary by given
                         extent
        simplify      -- How to simplify source half-boundary
        """
        self._half_boundary: List["HalfBoundary.AD"]
        if half_boundary is not None:
            assert (angles_deg is None) and (distances_m is None)
            self._half_boundary = copy.deepcopy(half_boundary._half_boundary)
        else:
            assert isinstance(angles_deg, list)
            assert isinstance(distances_m, list)
            error_if(
                len(angles_deg) != len(distances_m),
                f"Lengths of lists of angles and distances are different "
                f"({len(angles_deg)} and {len(distances_m)} respectively)")
            self._half_boundary = \
                list(
                    sorted([self.AD(deg=ad[0], m=ad[1])
                            for ad in zip(angles_deg, distances_m)
                            if 0 <= ad[0] <= 180],
                           key=operator.attrgetter("deg")))
            error_if(len(self._half_boundary) < 3,
                     "Not enough valid points in keyhole definition")
        if extent:
            for i in range(len(self._half_boundary)):
                self._half_boundary[i] = self._half_boundary[i].extend(extent)
        self._conv_hull = simplify == Simplification.convhull
        if simplify == Simplification.unjag:
            # Unjagging downslopes
            jag_start = 0
            for idx in range(1, len(self._half_boundary)):
                if (idx < (len(self._half_boundary) - 1)) and \
                        (self._half_boundary[idx].m ==
                         self._half_boundary[idx + 1].m):
                    continue
                jag_amount = self._half_boundary[idx].m - \
                    self._half_boundary[jag_start].m
                if (jag_start != (idx - 1)) and (jag_amount < 0):
                    jag_width = self._half_boundary[idx].deg - \
                        self._half_boundary[jag_start].deg
                    for i in range(jag_start + 1, idx):
                        angle_distance = self._half_boundary[i].deg - \
                            self._half_boundary[jag_start].deg
                        self._half_boundary[i] = \
                            self._half_boundary[i].extend(
                                jag_amount * (angle_distance / jag_width - 1))
                jag_start = idx
            # Unjagging upslopes
            for idx in range(len(self._half_boundary) - 1):
                if self._half_boundary[idx].m < self._half_boundary[idx + 1].m:
                    i = idx
                    while (i >= 0) and \
                            (self._half_boundary[i].m <
                             self._half_boundary[idx + 1].m):
                        self._half_boundary[i] = \
                            self.AD(deg=self._half_boundary[i].deg,
                                    m=self._half_boundary[idx + 1].m)
                        i -= 1

    def polygon(self) -> shapely.Polygon:
        """ Return correspondent shapely polygon """
        points: List[Tuple[float, float]] = []
        for ad in self._half_boundary:
            points.append((ad.m * (-ad.sin()), ad.m * ad.cos()))
        for idx in range(len(self._half_boundary) - 1, -1, -1):
            ad = self._half_boundary[idx]
            if ad.deg not in (0, 180):
                points.append((ad.m * ad.sin(), ad.m * ad.cos()))
        ret = shapely.Polygon(points)
        if self._conv_hull:
            ret = shapely.convex_hull(ret)
        return ret

    def radius(self) -> float:
        """ Distance from center to point in beam direction """
        return self._half_boundary[0].m


def draw_polygon(polygon: shapely.Polygon, **fill_kwargs) -> None:
    """ Draw polygon with matplotlib

    Known (but not found in documentation) possible arguments:
    color     -- Color name/hex/rgb. 'none' for no color
    edgecolor -- Edge color name/hex/rgb. 'none' for no color
    alpha     -- Transparency 0 (transparent) to 1 (opaque)
    linewidth -- Edge line thickness
    linestyle -- Line style name ('solid', 'dashed', 'dotted'...)
    hatch     -- Hatch pattern name ('/', '\', 'x', 'o', 'O', '.', '*'...)
    label     -- Label
    """
    matplotlib.pyplot.fill(*polygon.exterior.xy, **fill_kwargs)


def main(argv: List[str]) -> None:
    """Do the deed.

    Arguments:
    argv -- Program arguments
    """
    argument_parser = argparse.ArgumentParser(
        description="Keyhole shape converter/demonstrator")

    simplifications = \
        ", ".join(
            f"'{f.name}' ({f.value.dsc})"
            for f in Simplification.__members__.values())
    argument_parser.add_argument(
        "--simplify", choices=Simplification.__members__.keys(),
        help=f"Simplify source image. One of: {simplifications}")

    argument_parser.add_argument(
        "--tolerance", metavar="METERS", default=DEFAULT_TOLERANCE, type=float,
        help=f"Tolerance (in meters) when doing edge reduction of original "
        f"keyhole image Default is {DEFAULT_TOLERANCE}")
    argument_parser.add_argument(
        "--show", action="store_true",
        help="Show original (raw) and resulting shapes")

    default_format = \
        [f.name for f in OutputFormat.__members__.values()
         if f.value.is_default][0]
    formats = \
        ", ".join(
            f"'{f.name}' ({f.value.dsc})"
            for f in OutputFormat.__members__.values())
    argument_parser.add_argument(
        "--format", metavar="OUTPUT_FILE_FORMAT",
        choices=OutputFormat.__members__.keys(),
        default=default_format,
        help=f"Output file format. One of: {formats}. "
        f"Default is '{default_format}'")
    argument_parser.add_argument(
        "INPUT",
        help="Keyhole JSON file generated by AFC engine")
    argument_parser.add_argument(
        "OUTPUT", nargs="?",
        help="Output geometry file. may be absent if only 'show' is required")

    if not argv:
        argument_parser.print_help()
        sys.exit(1)

    args = argument_parser.parse_args(argv)

    # Set up logging
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(
        logging.Formatter(
            f"{os.path.basename(__file__)}. %(levelname)s: %(message)s"))
    logging.getLogger().addHandler(console_handler)
    logging.getLogger().setLevel(logging.INFO)

    error_if(not (args.show or args.OUTPUT), "Nothing to do!")
    error_if(not os.path.isfile(args.INPUT),
             f"Input keyhole JSON file '{args.INPUT}' not found")
    try:
        with open(args.INPUT, "r", encoding="utf-8") as f:
            json_data = json.load(f)
    except json.JSONDecodeError as ex:
        error(f"Input file '{args.INPUT}' is not a valid JSON: {ex}")
    except OSError as ex:
        error(f"Error reading input file '{args.INPUT}': {ex}")
    angles: List[float] = json_data.get("anglesDeg")
    distances: List[float] = json_data.get("distancesM")

    error_if(not isinstance(angles, list),
             f"'anglesDeg' (list of angles) not found in '{args.INPUT}'")
    error_if(not isinstance(distances, list),
             f"'distancesM' (list of distances) not found in '{args.INPUT}'")

    try:
        simplification: Optional[Simplification] = \
            Simplification[args.simplify] if args.simplify is not None \
            else None

        # Half boundary from file
        raw_half_boundary = \
            HalfBoundary(angles_deg=angles, distances_m=distances)
        raw_polygon = raw_half_boundary.polygon()

        interim_half_boundary = HalfBoundary(half_boundary=raw_half_boundary,
                                             simplify=simplification)

        # Choosing extent so that reduced polygon just covers intermediate one
        max_extent = 1.
        while True:
            extended_simplified_polygon = \
                HalfBoundary(
                    half_boundary=interim_half_boundary,
                    extent=max_extent, simplify=simplification).polygon().\
                simplify(args.tolerance)
            if extended_simplified_polygon.covers(raw_polygon):
                break
            max_extent *= 2
        min_extent = max_extent / 2
        while (max_extent - min_extent) > 1:
            mid_extent = (max_extent + min_extent) / 2
            extended_simplified_polygon = \
                HalfBoundary(
                    half_boundary=interim_half_boundary,
                    extent=mid_extent, simplify=simplification).polygon().\
                simplify(args.tolerance)
            if extended_simplified_polygon.covers(raw_polygon):
                max_extent = mid_extent
            else:
                min_extent = mid_extent

        # Final half-boundary
        final_half_boundary = \
            HalfBoundary(
                    half_boundary=interim_half_boundary,
                    extent=max_extent, simplify=simplification)
        final_polygon = final_half_boundary.polygon().simplify(args.tolerance)

        print(f"Resulting polygon has {len(final_polygon.exterior.coords)} "
              f"vertices")
        print(f"Simplification loss: "
              f"{(final_polygon.area / raw_polygon.area - 1) * 100:.1f}%")
        area_fraction = \
            final_polygon.area / (math.pi * raw_half_boundary.radius() ** 2)
        print(f"Keyhole covers {area_fraction * 100:.2f}% "
              f"(1/{1 / area_fraction:.2f} part) of antenna coverage circle")

        # Drawing
        if args.show:
            draw_polygon(final_polygon, color="black")
            draw_polygon(raw_half_boundary.polygon(), color="orange")
            draw_polygon(final_polygon, color="none", edgecolor="red",
                         linewidth=2)
            matplotlib.pyplot.axis("equal")
            print("Close drawing window to continue")
            matplotlib.pyplot.show()


        # Writing to file
        if args.OUTPUT:
            s = OutputFormat[args.format].value.dumper(final_polygon)
            assert isinstance(s, (str, bytes))
            try:
                with open(args.OUTPUT, "w" if isinstance(s, str) else "wb",
                          encoding="utf-8" if isinstance(s, str) else None) \
                        as f:
                    f.write(s)
            except OSError as ex:
                error(f"Error writing '{args.OUTPUT}': {ex}")
    except KeyboardInterrupt:
        print("^C")


if __name__ == "__main__":
    main(sys.argv[1:])
