/******************************************************************************************/
/**** FILE : channel_pair.cpp                                                          ****/
/******************************************************************************************/

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <cstring>
#include <math.h>
#include <stdexcept>

#include <QPointF>

#include "lidar_blacklist_entry.h"
#include "EcefModel.h"

/******************************************************************************************/
/**** FUNCTION: LidarBlacklistEntryClass::LidarBlacklistEntryClass()                   ****/
/******************************************************************************************/
LidarBlacklistEntryClass::LidarBlacklistEntryClass(double lonDegVal,
						   double latDegVal,
						   double radiusMeterVal) :
	lonDeg(lonDegVal), latDeg(latDegVal), radiusMeter(radiusMeterVal)
{
	centerPosition = EcefModel::geodeticToEcef(latDeg, lonDeg, 0.0);
	radiusSqKm = radiusMeter * radiusMeter * 1.0e-6;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: LidarBlacklistEntryClass::~LidarBlacklistEntryClass()                  ****/
/******************************************************************************************/
LidarBlacklistEntryClass::~LidarBlacklistEntryClass()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: LidarBlacklistEntryClass::contains()                                   ****/
/******************************************************************************************/
bool LidarBlacklistEntryClass::contains(double ptLonDeg, double ptLatDeg)
{
	bool cFlag;

	Vector3 ptPosition = EcefModel::geodeticToEcef(ptLatDeg, ptLonDeg, 0.0);
	Vector3 u = ptPosition - centerPosition;

	if (u.dot(u) < radiusSqKm) {
		cFlag = true;
		//      std::cout << "PT: " << ptLatDeg << " " << ptLonDeg << "  BLACKLIST_CENTER: "
		//      << latDeg << " " << lonDeg << " DIST = " << u.len()*1000 << " BLACKLIST_RAD
		//      = " << radiusMeter << std::endl;
	} else {
		cFlag = false;
	}

	return (cFlag);
}
/******************************************************************************************/
