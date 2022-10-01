#include "multiband_raster.h"
#include <limits>
#include <sstream>
#include <stdexcept>
#include <rkflogging/Logging.h>

namespace 
{
	// Logger for all instances of class
	LOGGER_DEFINE_GLOBAL(logger, "multiband_raster")

} // end namespace

const StrTypeClass MultibandRasterClass::strHeightResultList[] = {
	{   MultibandRasterClass::OUTSIDE_REGION, "OUTSIDE_REGION" },
	{          MultibandRasterClass::NO_DATA, "NO_DATA"        },
	{      MultibandRasterClass::NO_BUILDING, "NO_BUILDING"    },
	{         MultibandRasterClass::BUILDING, "BUILDING"       },

	{                                     -1,  (char *) 0      }

};

MultibandRasterClass::MultibandRasterClass(const std::string& rasterFile, CConst::LidarFormatEnum formatVal)
	: _format(formatVal), _cgLidar(rasterFile, "lidar", nullptr, 2)
{
	_cgLidar.setNoData(std::numeric_limits<float>::quiet_NaN(), 1);
	_cgLidar.setNoData(std::numeric_limits<float>::quiet_NaN(), 2);
}

bool MultibandRasterClass::contains(const double& lonDeg, const double& latDeg)
{
	return _cgLidar.covers(latDeg, lonDeg);
}

void MultibandRasterClass::getHeight(const double& latDeg, const double& lonDeg, double& terrainHeight, double& bldgHeight, HeightResult& heightResult, bool directGdalMode) const
{
	float terrainHeightF = std::numeric_limits<float>::quiet_NaN();
	float bldgHeightF = std::numeric_limits<float>::quiet_NaN();
	if (_cgLidar.covers(latDeg, lonDeg)) {
		if (!_cgLidar.getValueAt(latDeg, lonDeg, &terrainHeightF, 1, directGdalMode)) {
			heightResult = NO_DATA;
		} else if (!_cgLidar.getValueAt(latDeg, lonDeg, &bldgHeightF, 2, directGdalMode)) {
			heightResult = (_format == CConst::fromVectorLidarFormat) ? NO_BUILDING : NO_DATA;
		} else if ((_format == CConst::fromRasterLidarFormat) &&
			(bldgHeightF <= (terrainHeightF + 1)))
		{
			heightResult = NO_BUILDING;
			bldgHeightF = std::numeric_limits<float>::quiet_NaN();
		} else {
			heightResult = BUILDING;
		}
	} else {
		heightResult = OUTSIDE_REGION;
	}
	terrainHeight = terrainHeightF;
	bldgHeight = bldgHeightF;
}
/******************************************************************************************/
