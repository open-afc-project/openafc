/******************************************************************************************/
/**** FILE : multiband_raster.h                                                        ****/
/******************************************************************************************/

#ifndef MULTIBAND_RASTER_H
#define MULTIBAND_RASTER_H

#include <QRectF>
#include <vector>
#include <utility>
#include "gdal/gdal_priv.h"
#include "cconst.h"
#include "str_type.h"

/******************************************************************************************/
/**** CLASS: MultibandRasterClass                                                       ****/
/******************************************************************************************/
class MultibandRasterClass
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
		~MultibandRasterClass();

		// Returns building height at a specified (lat/lon) point. If there are no buildings present at the given position then a quiet NaN value is returned
		void getHeight(const double& latDeg, const double& lonDeg, double& terrainHeight, double& bldgHeight, HeightResult& heightResult) const;
		bool contains(const double& latDeg, const double& lonDeg);

		static const StrTypeClass strHeightResultList[];

	private:

		QRectF bounds;            // in lon/lat, with starting point in top left
		double xres;              // degrees longitude per pixel
		double yres;              // degrees latitude per pixel
		float nodataBE;          // no data value for bare earth band
		float nodataBldg;        // no data value for building band
		CConst::LidarFormatEnum format;
		GDALDataset* gdalDataset;
};
/******************************************************************************************/

#endif
