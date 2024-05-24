/******************************************************************************************/
/**** FILE : denied_region.cpp                                                         ****/
/******************************************************************************************/

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <cstring>
#include <math.h>
#include <stdexcept>

#include <QPointF>

#include "denied_region.h"

#include "afclogging/ErrStream.h"
#include "afclogging/Logging.h"
#include "afclogging/LoggingConfig.h"

namespace
{
// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "DeniedRegionClass")
}

/******************************************************************************************/
/**** FUNCTION: DeniedRegionClass::DeniedRegionClass()                                 ****/
/******************************************************************************************/
DeniedRegionClass::DeniedRegionClass(int idVal) : id(idVal)
{
	type = nullType;
	startFreq = -1.0;
	stopFreq = -1.0;
	heightAGL = -1.0;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: DeniedRegionClass::~DeniedRegionClass()                                ****/
/******************************************************************************************/
DeniedRegionClass::~DeniedRegionClass()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: RectDeniedRegionClass::RectDeniedRegionClass()                         ****/
/******************************************************************************************/
RectDeniedRegionClass::RectDeniedRegionClass(int idVal) : DeniedRegionClass(idVal)
{
	rectList = std::vector<std::tuple<double, double, double, double>>(0);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: RectDeniedRegionClass::~RectDeniedRegionClass()                        ****/
/******************************************************************************************/
RectDeniedRegionClass::~RectDeniedRegionClass()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: RectDeniedRegionClass::RectDeniedRegionClass()                         ****/
/******************************************************************************************/
void RectDeniedRegionClass::addRect(double lon1, double lon2, double lat1, double lat2)
{
	double lonStart = std::min(lon1, lon2);
	double lonStop = std::max(lon1, lon2);
	double latStart = std::min(lat1, lat2);
	double latStop = std::max(lat1, lat2);
	rectList.push_back(std::make_tuple(lonStart, lonStop, latStart, latStop));
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: RectDeniedRegionClass::intersect()                                   ****/
/******************************************************************************************/
bool RectDeniedRegionClass::intersect(
	double longitude, double latitude, double maxDist, double txHeightAGL) const
{
	int rectIdx;
	double deltaLon, deltaLat, dist;
	bool intersectFlag = false;
	for (rectIdx = 0; (rectIdx < (int)rectList.size()) && (!intersectFlag); ++rectIdx) {
		double rectLonStart, rectLonStop, rectLatStart, rectLatStop;
		std::tie(rectLonStart, rectLonStop, rectLatStart, rectLatStop) = rectList[rectIdx];

		int sx = (longitude < rectLonStart ? -1 : longitude > rectLonStop ? 1 : 0);

		int sy = (latitude < rectLatStart ? -1 : latitude > rectLatStop ? 1 : 0);

		if ((sx == 0) && (sy == 0)) {
			intersectFlag = true;
		} else if (sx == 0) {
			deltaLat = (sy == -1 ? rectLatStart - latitude : latitude - rectLatStop);
			dist = deltaLat * CConst::earthRadius * M_PI / 180.0;
			intersectFlag = (dist <= maxDist);
		} else if (sy == 0) {
			double cosVal = cos(latitude * M_PI / 180.0);
			deltaLon = (sx == -1 ? rectLonStart - longitude : longitude - rectLonStop);
			dist = deltaLon * CConst::earthRadius * M_PI / 180.0 * cosVal;
			intersectFlag = (dist <= maxDist);
		} else {
			double cosVal = cos(latitude * M_PI / 180.0);
			double cosSq = cosVal * cosVal;
			deltaLat = (sy == -1 ? rectLatStart - latitude : latitude - rectLatStop);
			deltaLon = (sx == -1 ? rectLonStart - longitude : longitude - rectLonStop);
			dist = CConst::earthRadius * M_PI / 180.0 *
				sqrt(deltaLat * deltaLat + deltaLon * deltaLon * cosSq);
			intersectFlag = (dist <= maxDist);
		}
	}

	return (intersectFlag);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: CircleDeniedRegionClass::CircleDeniedRegionClass()                     ****/
/******************************************************************************************/
CircleDeniedRegionClass::CircleDeniedRegionClass(int idVal, bool horizonDistFlagVal) :
	DeniedRegionClass(idVal), horizonDistFlag(horizonDistFlagVal)
{
	longitudeCenter = 0.0;
	latitudeCenter = 0.0;
	radius = 0.0;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: CircleDeniedRegionClass::~CircleDeniedRegionClass()                    ****/
/******************************************************************************************/
CircleDeniedRegionClass::~CircleDeniedRegionClass()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: CircleDeniedRegionClass::computeRadius()                               ****/
/******************************************************************************************/
double CircleDeniedRegionClass::computeRadius(double txHeightAGL) const
{
	double returnVal;

	if (!horizonDistFlag) {
		returnVal = radius;
	} else {
		returnVal = sqrt(2 * CConst::earthRadius * 4.0 / 3) * (sqrt(heightAGL) + sqrt(txHeightAGL));
	}

	return returnVal;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: CircleDeniedRegionClass::intersect()                                   ****/
/******************************************************************************************/
bool CircleDeniedRegionClass::intersect(
	double longitude, double latitude, double maxDist, double txHeightAGL) const
{
	double drRadius = computeRadius(txHeightAGL);

	double deltaLon = longitudeCenter - longitude;
	double deltaLat = latitudeCenter - latitude;
	double cosVal = cos(latitude * M_PI / 180.0);
	double cosSq = cosVal * cosVal;

	double dist = CConst::earthRadius * M_PI / 180.0 *
		(sqrt(deltaLat * deltaLat + deltaLon * deltaLon * cosSq));

	bool retval = (dist <= drRadius + maxDist);

	return (retval);
}
/******************************************************************************************/
