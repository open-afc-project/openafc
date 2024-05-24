
#include <cmath>
#include <limits>
#include <QDebug>
#include "GeodeticCoord.h"

namespace
{
const int metaTypeId = qRegisterMetaType<GeodeticCoord>("GeodeticCoord");
}

const qreal GeodeticCoord::nan = std::numeric_limits<qreal>::quiet_NaN();

bool GeodeticCoord::isNull() const
{
	return (std::isnan(longitudeDeg) || std::isnan(latitudeDeg) || std::isnan(heightKm));
}

void GeodeticCoord::normalize()
{
	// This is the number of (positive or negative) wraps occurring
	const int over = std::floor((longitudeDeg + 180.0) / 360.0);
	// Remove the number of wraps from longitude
	longitudeDeg -= 360.0 * over;
	// Clamp latitude
	latitudeDeg = std::max(-90.0, std::min(+90.0, latitudeDeg));
}

bool GeodeticCoord::isIdenticalTo(const GeodeticCoord &other, qreal accuracy) const
{
	const qreal diffLon = longitudeDeg - other.longitudeDeg;
	const qreal diffLat = latitudeDeg - other.latitudeDeg;
	return ((std::abs(diffLon) <= accuracy) && (std::abs(diffLat) <= accuracy));
}

QDebug operator<<(QDebug stream, const GeodeticCoord &pt)
{
	stream.nospace() << "(lon: " << pt.longitudeDeg << ", lat: " << pt.latitudeDeg
					 << ", height: " << pt.heightKm << ")";
	return stream.space();
}
