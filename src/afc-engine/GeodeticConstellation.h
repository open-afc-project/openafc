
#ifndef GEODETIC_CONSTELLATION_H
#define GEODETIC_CONSTELLATION_H

#include <QVector>
#include "GeodeticCoord.h"

/** A constellation is a set of discrete earth-fixed points.
 * There is no implied meaning to the order of the points.
 */
struct GeodeticConstellation : public QVector<GeodeticCoord> {};

Q_DECLARE_METATYPE(GeodeticConstellation);

#endif /* GEODETIC_CONSTELLATION_H */
