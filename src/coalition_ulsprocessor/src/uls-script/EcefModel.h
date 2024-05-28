#ifndef ECEF_MODEL_H
#define ECEF_MODEL_H

#include <armadillo>
#include "GeodeticCoord.h"
#include "Vector3.h"

/** Convert between geodetic coordinates and WGS84 Earth-centered Earth-fixed
 * (ECEF) coordinates.
 */
class EcefModel
{
    public:
        static Vector3 geodeticToEcef(double lat, double lon, double alt);
        static GeodeticCoord ecefToGeodetic(const Vector3 &ecef);

        /** Convert from geodetic coordinates to ECEF point.
         * @param in The geodetic coordinates to convert.
         * @return The ECEF coordinates for the same location (in units kilometers).
         */
        static Vector3 fromGeodetic(const GeodeticCoord &in);

        /** Convert from ECEF point to geodetic coordinates.
         * @param in The ECEF coordiantes to convert (in units kilometers).
         * @return The geodetic coordinates for the same location.
         */
        static GeodeticCoord toGeodetic(const Vector3 &in);

        /** Determine the local ellipsoid normal "up" direction at a given location.
         * @param in The geodetic coordinates of the location.
         * @return A unit vector in ECEF coordinates in the direction "up".
         */
        static Vector3 localVertical(const GeodeticCoord &in);
};

#endif
