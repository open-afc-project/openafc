/******************************************************************************************/
/**** FILE : lidar_blacklist_entry.h                                                   ****/
/******************************************************************************************/

#ifndef LIDAR_BLACKLIST_ENTRY_H
#define LIDAR_BLACKLIST_ENTRY_H

#include "Vector3.h"

/******************************************************************************************/
/**** CLASS: LidarBlacklistEntryClass                                                  ****/
/******************************************************************************************/
class LidarBlacklistEntryClass
{
	public:
		LidarBlacklistEntryClass(double lonDegVal, double latDegVal, double radiusMeterVal);
		~LidarBlacklistEntryClass();

		bool contains(double ptLonDeg, double ptLatDeg);

	private:
		double lonDeg, latDeg, radiusMeter, radiusSqKm;
		Vector3 centerPosition;
};
/******************************************************************************************/

#endif
