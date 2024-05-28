

#ifndef GEODETIC_COORD_H
#define GEODETIC_COORD_H

#include <QMetaType>
#include <QVariant>
//#include <qtglobal.h>

/** The base structure contains a 3D Earth-fixed geodetic coordinate.
 * This is in the WGS84 ellipsoid, so any conversion functions must follow
 * the WGS84 conventions. The height is an optional constructor parameter
 * because it is unused in many cases, but it is still more consistent to
 * have a single geodetic coordinate type than to have a 2D type and a 3D type
 * separate from each other.
 */
struct GeodeticCoord {
        /// Convenience definition for NaN value
        static const qreal nan;

        /// Static helper function for lat/lon coordinate order.
        static inline GeodeticCoord fromLatLon(qreal latDeg, qreal lonDeg, qreal htKm = 0)
        {
            return GeodeticCoord(lonDeg, latDeg, htKm);
        }

        /// Static helper function for lon/lat coordinate order.
        static inline GeodeticCoord fromLonLat(qreal lonDeg, qreal latDeg, qreal htKm = 0)
        {
            return GeodeticCoord(lonDeg, latDeg, htKm);
        }

        /** Default constructor has NaN horizontal values to distinguish an
         * invalid coordinate, but zero height value.
         */
        GeodeticCoord() : longitudeDeg(nan), latitudeDeg(nan), heightKm(0)
        {
        }

        /** Construct a new geodetic coordinate, the height is optional.
         */
        GeodeticCoord(qreal inLongitudeDeg, qreal inLatitudeDeg, qreal inHeightKm = 0) :
            longitudeDeg(inLongitudeDeg), latitudeDeg(inLatitudeDeg), heightKm(inHeightKm)
        {
        }

        /** Implicit conversion to QVariant.
         * @return The variant containing this GeodeticCoord value.
         */
        operator QVariant() const
        {
            return QVariant::fromValue(*this);
        }

        /** Determine if this location is the default NaN-valued.
         * @return True if any coordinate is NaN.
         */
        bool isNull() const;

        /** Normalize the coordinates in-place.
         * Longitude is limited to the range [-180, +180) by wrapping.
         * Latitude is limited to the range [-90, +90] by clamping.
         */
        void normalize();

        /** Get a normalized copy of the coordinates.
         * @return A copy of these coordinates after normalize() is called on it.
         */
        GeodeticCoord normalized() const
        {
            GeodeticCoord oth(*this);
            oth.normalize();
            return oth;
        }

        /** Compare two points to some required accuracy of sameness.
         * @param other The point to compare against.
         * @param accuracy This applies to difference between each of the
         * longitudes and latitude independently.
         */
        bool isIdenticalTo(const GeodeticCoord &other, qreal accuracy) const;

        /// Longitude referenced to WGS84 zero meridian; units of degrees.
        qreal longitudeDeg;
        /// Latitude referenced to WGS84 equator; units of degrees.
        qreal latitudeDeg;
        /// Height referenced to WGS84 ellipsoid; units of kilometers.
        qreal heightKm;
};
Q_DECLARE_METATYPE(GeodeticCoord);

/// Allow debugging prints
QDebug operator<<(QDebug stream, const GeodeticCoord &pt);

#endif /* GEODETIC_COORD_H */
