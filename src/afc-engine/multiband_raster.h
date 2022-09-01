/******************************************************************************************/
/**** FILE : multiband_raster.h                                                        ****/
/******************************************************************************************/

#ifndef MULTIBAND_RASTER_H
#define MULTIBAND_RASTER_H

#include "CachedGdal.h"
#include "cconst.h"
#include <boost/core/noncopyable.hpp>
#include "str_type.h"
#include <string>

/******************************************************************************************/
/**** CLASS: MultibandRasterClass                                                       ****/
/******************************************************************************************/
class MultibandRasterClass: private boost::noncopyable
{
	public:

		enum HeightResult
		{
			OUTSIDE_REGION,       // point outside region defined by rectangle "bounds"
			NO_DATA,              // point inside bounds that has no data.  terrainHeight set to nodataBE, bldgHeight undefined
			NO_BUILDING,          // point where there is no building, terrainHeight set to valid value, bldgHeight set to nodataBldg
			BUILDING              // point where there is a building, terrainHeight and bldgHeight valid values
		};

		MultibandRasterClass(const std::string& rasterFile, CConst::LidarFormatEnum formatVal);

		// Returns building height at a specified (lat/lon) point. If there are no buildings present at the given position then a quiet NaN value is returned
		void getHeight(const double& latDeg, const double& lonDeg, double& terrainHeight, double& bldgHeight, HeightResult& heightResult) const;
		bool contains(const double& latDeg, const double& lonDeg);

		static const StrTypeClass strHeightResultList[];

	private:

		CConst::LidarFormatEnum _format;
		mutable CachedGdal<float> _cgLidar;
};
/******************************************************************************************/

#endif
