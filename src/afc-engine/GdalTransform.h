/*
 * Copyright (C) 2022 Broadcom. All rights reserved.
 * The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
 * that owns the software below.
 * This work is licensed under the OpenAFC Project License, a copy of which is
 * included with this software program.
 */

#ifndef GDAL_TRANSFORM_H
#define GDAL_TRANSFORM_H

/** @file
 * Utility classes handling latitude/longitude to pixel row/column computations
 */

/** Handles 6-parameter data transformation.
 * Made struct (not class) as there will be API for fine-tuning them by an
 * external function
 */

#include <string>

class GDALDataset; // Forward declaration

/** Parameters for coordinates to pixel index transformation.
 * This structure contains parameters for inverse GDAL 6-element transformation,
 * narrowed to the case of north-up geodetic coordinates.
 * Chosen format is based on original transformation, used for DEP terrain
 * (as of time of development the main source of terrain information)
 */
struct GdalTransform {
        /** Bounding rectangle */
        struct BoundRect {
                /** Constructor.
                 * @param latDegMin Minimum latitude in north-positive degrees
                 * @param lonDegMin Minimum longitude in east-positive degrees
                 * @param latDegMax Maximum latitude in north-positive degrees
                 * @param lonDegMax Maximum longitude in east-positive degrees
                 */
                BoundRect(double latDegMin, double lonDegMin, double latDegMax, double lonDegMax);

                /** Default constructor */
                BoundRect();

                /** True if rectangle contains given point.
                 * To facilitate unambiguous tiled inclusion detection, top and left
                 * boundaries included, bottom and right - not included) */
                bool contains(double latDeg, double lonDeg) const;

                /** Combine self with other */
                void combine(const BoundRect &other);

                /* Rectangle boundaries */
                double latDegMin; /*!< Minimum latitude in north-positive degrees */
                double lonDegMin; /*!< Minimum longitude in east-positive degrees */
                double latDegMax; /*!< Maximum latitude in north-positive degrees */
                double lonDegMax; /*!< Maximum longitude in east-positive degrees */

                /** Rebase longitude value to [base, base+360[ range */
                static double rebaseLon(double lon, double base);
        };

        /** Constructor from GDAL transformation.
         * @param gdalTransform 6-element GDAL transformation matrix
         * @param margin Number of pixels along boundary rectangle to ignore (only
         *	affects makeBoundRect() result, does not affect transformation itself)
         */
        GdalTransform(GDALDataset *gdalDataSet, const std::string &filename);

        /** Construct tile GDAL transformation from file GDAL transformation.
         * @param gdalXform Transformation for the whole GDAL file
         * @param latPixOffset Tile offset in number of pixels from first row
         * @param lonPixOffset Tile offset in number of pixels from first column
         * @param latSize Number of pixels in latitudinal direction
         * @param lonSize Number of pixels in longitudinal direction
         */
        GdalTransform(const GdalTransform &gdalXform, int latPixOffset, int lonPixOffset, int latSize, int lonSize);

        /** Default constructor */
        GdalTransform();

        /** Compute pixel indices for given point.
         * @param[in] latDeg Latitude in north-positive degrees to apply
         *	transformation to
         * @param[in] lonDeg Longitude in east-positive degrees to apply
         *	transformation to
         * @param[out] latIdx Resulted latitude index
         * @param[out] lonIdx Resulted longitude index
         */
        void computePixel(double latDeg, double lonDeg, int *latIdx, int *lonIdx) const;

        /** Returns GDAL data bounding rectangle.
         * It is guaranteed that latitudes are in [-90, 90] range,
         * latDegMin <= latDegMax, lonDegMin, <= lonDegMax. But it is not guaranteed
         * that longitudes lie in [-180, 180[ range (out of range case - NLCD for
         * Alaska)
         */
        BoundRect makeBoundRect() const;

        /** Round pixels per degree and pixel boundaries to multiple of given value.
         * @param pixelsPerDegree Value to make parameter component multiple of.
         * E.g. 1 means that all parameters become integer
         */
        void roundPpdToMultipleOf(double pixelsPerDegree);

        /** Treat everything outside given number of degrees as margin.
         * E.g. setMarginsOutsideDeg(1) treat as margin everything outside whole
         * number of degrees.
         * This function uses latitudinal parameters for margin computation
         * (assuming longitudinal margin has the same size).
         * This function ensures that resulting margin is a multiple of 0.5 (as
         * it is barely imaginably how can it be otherwise).
         * @param deg Number of degrees to round grid to
         */
        void setMarginsOutsideDeg(double deg);

        /** Number of pixels per degree in latitudinal direction */
        double latPixPerDeg;
        /** Number of pixels per degree in longitudinal direction */
        double lonPixPerDeg;
        /** Number of pixels from equator to top boundary (signed, north-positive) */
        double latPixMax;
        /** Number of pixels from Greenwich to left boundary (signed, east-positive) */
        double lonPixMin;
        /** Number of pixels in latitude direction */
        int latSize;
        /** Number of pixels in longitude direction */
        int lonSize;
        /** Number of (overlap) pixels along boundary to exclude from bounding rectangle */
        double margin;
};
#endif /* GDAL_TRANSFORM_H */
