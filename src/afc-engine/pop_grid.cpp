/******************************************************************************************/
/**** FILE : pop_grid.cpp                                                              ****/
/******************************************************************************************/

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <cstring>
#include <math.h>
#include <stdexcept>
#include <iomanip>
#include <ogr_spatialref.h>

#include "gdal_priv.h"
#include "ogr_spatialref.h"

#include "cconst.h"
#include "pop_grid.h"
#include "global_defines.h"
#include "local_defines.h"
#include "global_fn.h"
#include "spline.h"
#include "polygon.h"
#include "list.h"
#include "uls.h"
#include "EcefModel.h"

#include "afclogging/Logging.h"
#include "afclogging/ErrStream.h"
#include "PopulationDatabase.h"
#include "AfcDefinitions.h"

namespace
{
// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "pop_grid")

} // end namespace

/******************************************************************************************/
/**** FUNCTION: PopGridClass::PopGridClass()                                           ****/
/******************************************************************************************/
PopGridClass::PopGridClass(double densityThrUrbanVal,
			   double densityThrSuburbanVal,
			   double densityThrRuralVal) :
	densityThrUrban(densityThrUrbanVal),
	densityThrSuburban(densityThrSuburbanVal),
	densityThrRural(densityThrRuralVal)
{
	minLonDeg = quietNaN;
	minLatDeg = quietNaN;
	deltaLonDeg = quietNaN;
	deltaLatDeg = quietNaN;

	numLon = 0;
	numLat = 0;

	pop = (double **)NULL;
	propEnv = (char **)NULL;
	region = (int **)NULL;
	numRegion = (int)regionNameList.size();

	isCumulative = false;
}
/******************************************************************************************/

/******************************************************************************************/
/**** CONSTRUCTOR: PopGridClass::PopGridClass()                                        ****/
/******************************************************************************************/
PopGridClass::PopGridClass(std::string worldPopulationFile,
			   const std::vector<PolygonClass *> &regionPolygonList,
			   double regionPolygonResolution,
			   double densityThrUrbanVal,
			   double densityThrSuburbanVal,
			   double densityThrRuralVal,
			   double minLat,
			   double minLon,
			   double maxLat,
			   double maxLon) :
	densityThrUrban(densityThrUrbanVal),
	densityThrSuburban(densityThrSuburbanVal),
	densityThrRural(densityThrRuralVal)
{
	// NOTE: Each point in the PopGridClass is taken to be in the first region in which the
	// point is contained.  So, for regions contained in another region, specify interior region
	// first.  For example, if regions are defined for india and asia, specifying india first
	// effectively defines two regions as india and "asia except india".

	std::ostringstream errStr;
	if (worldPopulationFile.empty()) {
		throw std::invalid_argument("worldPopulationFile is empty");
	}

	int regionIdx;

#if DEBUG_AFC
	FILE *fchk = (FILE *)NULL;

	#if 0
    if ( !(fchk = fopen("/tmp/test_pop.csv", "wb")) ) {
        throw std::runtime_error("ERROR");
    }
	#endif
	if (fchk) {
		fprintf(fchk, "Latitude (deg),Longitude (deg),Density (people/sq-km)\n");
	}
#endif

	numRegion = regionPolygonList.size();

	std::cout << "Loading world polulation file " << worldPopulationFile << std::endl;

	GDALDataset *gdalDataset = static_cast<GDALDataset *>(
		GDALOpen(worldPopulationFile.c_str(), GA_ReadOnly));

	int nXSize = gdalDataset->GetRasterXSize();
	int nYSize = gdalDataset->GetRasterYSize();
	int numRasterBand = gdalDataset->GetRasterCount();

	double adfGeoTransform[6];
	printf("Size is %dx%dx%d\n", nXSize, nYSize, numRasterBand);
	CPLErr readError = gdalDataset->GetGeoTransform(adfGeoTransform);
	if (readError != CPLErr::CE_None) {
		errStr << "ERROR: getting GEO Transform" << worldPopulationFile
		       << ", throwing CPLErr = " << readError;
		throw std::runtime_error(errStr.str());
	}

	// const char  *pszProjection = nullptr;
	printf("Origin = (%.6f,%.6f)\n", adfGeoTransform[0], adfGeoTransform[3]);
	printf("Pixel Size = (%.6f,%.6f)\n", adfGeoTransform[1], adfGeoTransform[5]);
	// pszProjection = GDALGetProjectionRef(gdalDataset);

	double pixelSize = adfGeoTransform[1];
	if (fabs(pixelSize + adfGeoTransform[5]) > 1.0e-8) {
		throw std::runtime_error("ERROR: X / Y pixel sizes not properly set");
	}

	double ULX = adfGeoTransform[0] + adfGeoTransform[1] * 0 + adfGeoTransform[2] * 0;
	double ULY = adfGeoTransform[3] + adfGeoTransform[4] * 0 + adfGeoTransform[5] * 0;

	double LLX = adfGeoTransform[0] + adfGeoTransform[1] * 0 + adfGeoTransform[2] * nYSize;
	double LLY = adfGeoTransform[3] + adfGeoTransform[4] * 0 + adfGeoTransform[5] * nYSize;

	double URX = adfGeoTransform[0] + adfGeoTransform[1] * nXSize + adfGeoTransform[2] * 0;
	double URY = adfGeoTransform[3] + adfGeoTransform[4] * nXSize + adfGeoTransform[5] * 0;

	double LRX = adfGeoTransform[0] + adfGeoTransform[1] * nXSize + adfGeoTransform[2] * nYSize;
	double LRY = adfGeoTransform[3] + adfGeoTransform[4] * nXSize + adfGeoTransform[5] * nYSize;

	if ((ULX != LLX) || (URX != LRX) || (LLY != LRY) || (ULY != URY)) {
		errStr << "ERROR: Inconsistent bounding box in world population file: "
		       << worldPopulationFile;
		throw std::runtime_error(errStr.str());
	}

	double worldMinLon = LLX;
	double worldMinLat = LLY;
	double worldMaxLon = URX;
	double worldMaxLat = URY;

	if ((fabs(worldMinLon + 180.0) > 1.0e-8) || (fabs(worldMaxLon - 180.0) > 1.0e-8) ||
	    (fabs(worldMinLat + 90.0) > 1.0e-8) || (fabs(worldMaxLat - 90.0) > 1.0e-8)) {
		errStr << "ERROR: world population file: " << worldPopulationFile
		       << " does not cover region LON: -180,180 LAT: -90,90";
		throw std::runtime_error(errStr.str());
	}

	double resLon = (worldMaxLon - worldMinLon) / nXSize;
	double resLat = (worldMaxLat - worldMinLat) / nYSize;
	double resLonLat = std::min(resLon, resLat);

	std::cout << "UL LONLAT: " << ULX << " " << ULY << std::endl;
	std::cout << "LL LONLAT: " << LLX << " " << LLY << std::endl;
	std::cout << "UR LONLAT: " << URX << " " << URY << std::endl;
	std::cout << "LR LONLAT: " << LRX << " " << LRY << std::endl;

	std::cout << "RES_LON = " << resLon << std::endl;
	std::cout << "RES_LAT = " << resLat << std::endl;
	std::cout << "RES_LONLAT = " << resLonLat << std::endl;

	std::cout << "NUMBER RASTER BANDS: " << numRasterBand << std::endl;

	if (numRasterBand != 1) {
		throw std::runtime_error("ERROR numRasterBand must be 1");
	}

	int nBlockXSize, nBlockYSize;
	GDALRasterBand *rasterBand = gdalDataset->GetRasterBand(1);
	char **rasterMetadata = rasterBand->GetMetadata();

	if (rasterMetadata) {
		std::cout << "RASTER METADATA: " << std::endl;
		char **chptr = rasterMetadata;
		while (*chptr) {
			std::cout << "    " << *chptr << std::endl;
			chptr++;
		}
	} else {
		std::cout << "NO RASTER METADATA: " << std::endl;
	}

	rasterBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
	printf("Block=%dx%d Type=%s, ColorInterp=%s\n",
	       nBlockXSize,
	       nBlockYSize,
	       GDALGetDataTypeName(rasterBand->GetRasterDataType()),
	       GDALGetColorInterpretationName(rasterBand->GetColorInterpretation()));

	int bGotMin, bGotMax;
	double adfMinMax[2];
	adfMinMax[0] = rasterBand->GetMinimum(&bGotMin);
	adfMinMax[1] = rasterBand->GetMaximum(&bGotMax);
	if (!(bGotMin && bGotMax)) {
		std::cout << "calling GDALComputeRasterMinMax()" << std::endl;
		GDALComputeRasterMinMax((GDALRasterBandH)rasterBand, TRUE, adfMinMax);
	}

	printf("Min=%.3f\nMax=%.3f\n", adfMinMax[0], adfMinMax[1]);
	if (rasterBand->GetOverviewCount() > 0) {
		printf("Band has %d overviews.\n", rasterBand->GetOverviewCount());
	}
	if (rasterBand->GetColorTable() != NULL) {
		printf("Band has a color table with %d entries.\n",
		       rasterBand->GetColorTable()->GetColorEntryCount());
	}

	int hasNoData;
	double origNodataValue = rasterBand->GetNoDataValue(&hasNoData);
	float origNodataValueFloat = (float)origNodataValue;
	if (hasNoData) {
		std::cout << "ORIG NODATA: " << origNodataValue << std::endl;
		std::cout << "ORIG NODATA (float): " << origNodataValueFloat << std::endl;
	} else {
		std::cout << "ORIG NODATA undefined" << std::endl;
	}

	int minx = (int)floor(minLon / regionPolygonResolution);
	int maxx = (int)floor(maxLon / regionPolygonResolution) + 1;
	int miny = (int)floor(minLat / regionPolygonResolution);
	int maxy = (int)floor(maxLat / regionPolygonResolution) + 1;

	minLonDeg = minx * regionPolygonResolution;
	minLatDeg = miny * regionPolygonResolution;
	double maxLonDeg = maxx * regionPolygonResolution;
	double maxLatDeg = maxy * regionPolygonResolution;

	deltaLonDeg = resLon;
	deltaLatDeg = resLat;

	int minLonIdx = (int)floor((minLonDeg - worldMinLon) / resLon + 0.5);
	int maxLonIdx = (int)floor((maxLonDeg - worldMinLon) / resLon + 0.5);
	int minLatIdx = (int)floor((minLatDeg - worldMinLat) / resLat + 0.5);
	int maxLatIdx = (int)floor((maxLatDeg - worldMinLat) / resLat + 0.5);

	std::cout << "REGION MIN LON DEG: " << minLonDeg << std::endl;
	std::cout << "REGION MIN LAT DEG: " << minLatDeg << std::endl;
	std::cout << "REGION MAX LON DEG: " << maxLonDeg << std::endl;
	std::cout << "REGION MAX LAT DEG: " << maxLatDeg << std::endl;

	std::cout << std::endl;

	std::cout << "REGION MIN LON IDX: " << minLonIdx << std::endl;
	std::cout << "REGION MIN LAT IDX: " << minLatIdx << std::endl;
	std::cout << "REGION MAX LON IDX: " << maxLonIdx << std::endl;
	std::cout << "REGION MAX LAT IDX: " << maxLatIdx << std::endl;

	std::cout << std::endl;

	bool wrapLonFlag;
	if (maxLonIdx > nXSize - 1) {
		wrapLonFlag = true;
	} else {
		wrapLonFlag = false;
	}

	std::cout << "Analysis region wraps around LON discontinuity at +/- 180 deg: "
		  << (wrapLonFlag ? "YES" : "NO") << std::endl;

	/**************************************************************************************/
	/**** Set parameters                                                               ****/
	/**************************************************************************************/
	numLon = maxLonIdx - minLonIdx;
	minLonDeg = worldMinLon + minLonIdx * resLon;

	numLat = maxLatIdx - minLatIdx;
	minLatDeg = worldMinLat + minLatIdx * resLat;
	/**************************************************************************************/

	/**************************************************************************************/
	/**** Allocate matrix and initialize to zeros                                      ****/
	/**************************************************************************************/
	pop = (double **)malloc(numLon * sizeof(double *));
	propEnv = (char **)malloc(numLon * sizeof(char *));
	region = (int **)malloc(numLon * sizeof(int *));
	for (int lonIdx = 0; lonIdx < numLon; lonIdx++) {
		pop[lonIdx] = (double *)malloc(numLat * sizeof(double));
		propEnv[lonIdx] = (char *)malloc(numLat * sizeof(char));
		region[lonIdx] = (int *)malloc(numLat * sizeof(int));
		for (int latIdx = 0; latIdx < numLat; latIdx++) {
			pop[lonIdx][latIdx] = 0.0;
			propEnv[lonIdx][latIdx] = 'X';
			region[lonIdx][latIdx] = -1;
		}
	}
	isCumulative = false;
	/**************************************************************************************/

	urbanPop.resize(numRegion);
	suburbanPop.resize(numRegion);
	ruralPop.resize(numRegion);
	barrenPop.resize(numRegion);

	std::vector<double> urbanArea(numRegion);
	std::vector<double> suburbanArea(numRegion);
	std::vector<double> ruralArea(numRegion);
	std::vector<double> barrenArea(numRegion);
	std::vector<double> zeroArea(numRegion);

	for (regionIdx = 0; regionIdx < numRegion; regionIdx++) {
		urbanPop[regionIdx] = 0.0;
		suburbanPop[regionIdx] = 0.0;
		ruralPop[regionIdx] = 0.0;
		barrenPop[regionIdx] = 0.0;

		urbanArea[regionIdx] = 0.0;
		suburbanArea[regionIdx] = 0.0;
		ruralArea[regionIdx] = 0.0;
		barrenArea[regionIdx] = 0.0;
	}

	double totalArea = 0.0;
	double totalPop = 0.0;

	int lonIdx, latIdx;
	std::cout << "numLon: " << numLon << std::endl;
	std::cout << "numLat: " << numLat << std::endl;
	// std::cout << "GDALGetDataTypeSizeBytes(GDT_Float32) = " <<
	// GDALGetDataTypeSizeBytes(GDT_Float32) << std::endl;
	std::cout << "sizeof(GDT_Float32) = " << sizeof(GDT_Float32) << std::endl;
	std::cout << "sizeof(GDT_Float64) = " << sizeof(GDT_Float64) << std::endl;
	std::cout << "sizeof(float) = " << sizeof(float) << std::endl;
	float *pafScanline = (float *)CPLMalloc(numLon * GDALGetDataTypeSize(GDT_Float32));

	double areaGridEquator = CConst::earthRadius * CConst::earthRadius *
				 (deltaLonDeg * M_PI / 180.0) * (deltaLatDeg * M_PI / 180.0);

	for (latIdx = 0; latIdx < numLat; latIdx++) {
		if (wrapLonFlag) {
			rasterBand->RasterIO(GF_Read,
					     minLonIdx,
					     nYSize - 1 - minLatIdx - latIdx,
					     nXSize - minLonIdx,
					     1,
					     pafScanline,
					     nXSize - minLonIdx,
					     1,
					     GDT_Float32,
					     0,
					     0);
			rasterBand->RasterIO(GF_Read,
					     0,
					     nYSize - 1 - minLatIdx - latIdx,
					     numLon - (nXSize - minLonIdx),
					     1,
					     pafScanline + nXSize - minLonIdx,
					     numLon - (nXSize - minLonIdx),
					     1,
					     GDT_Float32,
					     0,
					     0);
		} else {
			rasterBand->RasterIO(GF_Read,
					     minLonIdx,
					     nYSize - 1 - minLatIdx - latIdx,
					     numLon,
					     1,
					     pafScanline,
					     numLon,
					     1,
					     GDT_Float32,
					     0,
					     0);
		}
		double latitudeDeg = (maxLatDeg * (2 * latIdx + 1) +
				      minLatDeg * (2 * numLat - 2 * latIdx - 1)) /
				     (2 * numLat);
		int polygonY = (int)floor(latitudeDeg / regionPolygonResolution + 0.5);
		for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
			// if ((lonIdx == 1382) && (latIdx == 1425) ) {
			//     std::cout << "CHECK POINT" << std::endl;
			// }

			if (pafScanline[lonIdx] != origNodataValueFloat) {
				double longitudeDeg = (maxLonDeg * (2 * lonIdx + 1) +
						       minLonDeg * (2 * numLon - 2 * lonIdx - 1)) /
						      (2 * numLon);
				// std::cout << longitudeDeg << " " << latitudeDeg << std::endl;
				int polygonX = (int)floor(longitudeDeg / regionPolygonResolution +
							  0.5);
				bool foundRegion = false;
				for (regionIdx = 0;
				     (regionIdx < (int)regionPolygonList.size()) && (!foundRegion);
				     ++regionIdx) {
					PolygonClass *regionPolygon = regionPolygonList[regionIdx];
					if (regionPolygon->in_bdy_area(polygonX, polygonY)) {
						foundRegion = true;

						double density =
							pafScanline[lonIdx] *
							1.0e-6; // convert from people/sq-km to
								// people/sqm;
						double area = areaGridEquator *
							      cos(latitudeDeg * M_PI / 180.0);

						pop[lonIdx][latIdx] = density * area;
						region[lonIdx][latIdx] = regionIdx;

						if (density == 0.0) {
							zeroArea[regionIdx] += area;
						}

						if (density >= densityThrUrban) {
							urbanPop[regionIdx] += density * area;
							if (density != 0.0) {
								urbanArea[regionIdx] += area;
							}
							propEnv[lonIdx][latIdx] = 'U';
						} else if (density >= densityThrSuburban) {
							suburbanPop[regionIdx] += density * area;
							if (density != 0.0) {
								suburbanArea[regionIdx] += area;
							}
							propEnv[lonIdx][latIdx] = 'S';
						} else if (density >= densityThrRural) {
							ruralPop[regionIdx] += density * area;
							if (density != 0.0) {
								ruralArea[regionIdx] += area;
							}
							propEnv[lonIdx][latIdx] = 'R';
						} else {
							barrenPop[regionIdx] += density * area;
							if (density != 0.0) {
								barrenArea[regionIdx] += area;
							}
							propEnv[lonIdx][latIdx] = 'B';
						}

						totalArea += area;
						totalPop += pop[lonIdx][latIdx];

#if DEBUG_AFC
						if (fchk) {
							fprintf(fchk,
								"%.4f,%.4f,%.6f\n",
								latitudeDeg,
								longitudeDeg,
								density * 1.0e6);
						}
#endif
					}
				}
			}
		}
	}
#if DEBUG_AFC
	if (fchk) {
		fclose(fchk);
	}
#endif

	std::cout << "TOTAL INTEGRATED POPULATION: " << totalPop << std::endl;
	std::cout << "TOTAL INTEGRATED AREA: " << totalArea << std::endl;
	std::cout << std::endl;
	if ((totalPop > 0.0) && (totalArea > 0.0)) {
		for (regionIdx = 0; regionIdx < numRegion; regionIdx++) {
			PolygonClass *regionPolygon = regionPolygonList[regionIdx];
			double regionPop = urbanPop[regionIdx] + suburbanPop[regionIdx] +
					   ruralPop[regionIdx] + barrenPop[regionIdx];
			std::cout << "REGION " << regionPolygon->name
				  << " URBAN    POPULATION: " << urbanPop[regionIdx] << " "
				  << 100 * urbanPop[regionIdx] / totalPop << " % (total)"
				  << " " << 100 * urbanPop[regionIdx] / regionPop << " % (region)"
				  << std::endl;
			std::cout << "REGION " << regionPolygon->name
				  << " SUBURBAN POPULATION: " << suburbanPop[regionIdx] << " "
				  << 100 * suburbanPop[regionIdx] / totalPop << " % (total)"
				  << " " << 100 * suburbanPop[regionIdx] / regionPop
				  << " % (region)" << std::endl;
			std::cout << "REGION " << regionPolygon->name
				  << " RURAL    POPULATION: " << ruralPop[regionIdx] << " "
				  << 100 * ruralPop[regionIdx] / totalPop << " % (total)"
				  << " " << 100 * ruralPop[regionIdx] / regionPop << " % (region)"
				  << std::endl;
			std::cout << "REGION " << regionPolygon->name
				  << " BARREN   POPULATION: " << barrenPop[regionIdx] << " "
				  << 100 * barrenPop[regionIdx] / totalPop << " % (total)"
				  << " " << 100 * barrenPop[regionIdx] / regionPop << " % (region)"
				  << std::endl;
			std::cout << std::endl;

			double regionArea = urbanArea[regionIdx] + suburbanArea[regionIdx] +
					    ruralArea[regionIdx] + barrenArea[regionIdx] +
					    zeroArea[regionIdx];
			std::cout << "REGION " << regionPolygon->name
				  << " URBAN_NZ    AREA: " << urbanArea[regionIdx] << " "
				  << 100 * urbanArea[regionIdx] / totalArea << " % (total)"
				  << " " << 100 * urbanArea[regionIdx] / regionArea << " % (region)"
				  << std::endl;
			std::cout << "REGION " << regionPolygon->name
				  << " SUBURBAN_NZ AREA: " << suburbanArea[regionIdx] << " "
				  << 100 * suburbanArea[regionIdx] / totalArea << " % (total)"
				  << " " << 100 * suburbanArea[regionIdx] / regionArea
				  << " % (region)" << std::endl;
			std::cout << "REGION " << regionPolygon->name
				  << " RURAL_NZ    AREA: " << ruralArea[regionIdx] << " "
				  << 100 * ruralArea[regionIdx] / totalArea << " % (total)"
				  << " " << 100 * ruralArea[regionIdx] / regionArea << " % (region)"
				  << std::endl;
			std::cout << "REGION " << regionPolygon->name
				  << " BARREN_NZ   AREA: " << barrenArea[regionIdx] << " "
				  << 100 * barrenArea[regionIdx] / totalArea << " % (total)"
				  << " " << 100 * barrenArea[regionIdx] / regionArea
				  << " % (region)" << std::endl;
			std::cout << "REGION " << regionPolygon->name
				  << " ZERO-POP    AREA: " << zeroArea[regionIdx] << " "
				  << 100 * zeroArea[regionIdx] / totalArea << " % (total)"
				  << " " << 100 * zeroArea[regionIdx] / regionArea << " % (region)"
				  << std::endl;
			std::cout << std::endl;
		}
	}
	std::cout << std::flush;
	/**************************************************************************************/

	CPLFree(pafScanline);
}
/******************************************************************************************/

/******************************************************************************************/
/**** COPY CONSTRUCTOR: PopGridClass::PopGridClass(const PopGridClass &obj)            ****/
/******************************************************************************************/
PopGridClass::PopGridClass(const PopGridClass &obj) : regionNameList(obj.regionNameList)
{
	densityThrUrban = obj.densityThrUrban;
	densityThrSuburban = obj.densityThrSuburban;
	densityThrRural = obj.densityThrRural;
	minLonDeg = obj.minLonDeg;
	minLatDeg = obj.minLatDeg;
	deltaLonDeg = obj.deltaLonDeg;
	deltaLatDeg = obj.deltaLatDeg;
	isCumulative = obj.isCumulative;
	numLon = obj.numLon;
	numLat = obj.numLat;
	numRegion = obj.numRegion;

	/**************************************************************************************/
	/**** Allocate matrix and copy values                                              ****/
	/**************************************************************************************/
	int lonIdx, latIdx;
	pop = (double **)malloc(numLon * sizeof(double *));
	propEnv = (char **)malloc(numLon * sizeof(char *));
	region = (int **)malloc(numLon * sizeof(int *));
	for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
		pop[lonIdx] = (double *)malloc(numLat * sizeof(double));
		propEnv[lonIdx] = (char *)malloc(numLat * sizeof(char));
		region[lonIdx] = (int *)malloc(numLat * sizeof(int));
		for (latIdx = 0; latIdx < numLat; latIdx++) {
			pop[lonIdx][latIdx] = obj.pop[lonIdx][latIdx];
			propEnv[lonIdx][latIdx] = obj.propEnv[lonIdx][latIdx];
			region[lonIdx][latIdx] = obj.region[lonIdx][latIdx];
		}
	}
	/**************************************************************************************/

	urbanPop = obj.urbanPop;
	suburbanPop = obj.suburbanPop;
	ruralPop = obj.ruralPop;
	barrenPop = obj.barrenPop;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::~PopGridClass()                                          ****/
/******************************************************************************************/
PopGridClass::~PopGridClass()
{
	int lonIdx;

	if (pop) {
		for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
			free(pop[lonIdx]);
		}
		free(pop);
	}

	if (propEnv) {
		for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
			free(propEnv[lonIdx]);
		}
		free(propEnv);
	}

	if (region) {
		for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
			free(region[lonIdx]);
		}
		free(region);
	}
}
/******************************************************************************************/

/******************************************************************************************/
/**** GET/SET functions                                                                ****/
/******************************************************************************************/
int PopGridClass::getNumLon()
{
	return (numLon);
}
int PopGridClass::getNumLat()
{
	return (numLat);
}
double PopGridClass::getDensityThrUrban()
{
	return (densityThrUrban);
}
double PopGridClass::getDensityThrSuburban()
{
	return (densityThrSuburban);
}
double PopGridClass::getDensityThrRural()
{
	return (densityThrRural);
}

void PopGridClass::setPop(int lonIdx, int latIdx, double popVal)
{
	pop[lonIdx][latIdx] = popVal;
	return;
}
void PopGridClass::setPropEnv(int lonIdx, int latIdx, char propEnvVal)
{
	propEnv[lonIdx][latIdx] = propEnvVal;
	return;
}

double PopGridClass::getPropEnvPop(CConst::PropEnvEnum propEnvVal, int regionIdx) const
{
	double popVal;
	switch (propEnvVal) {
		case CConst::urbanPropEnv:
			popVal = urbanPop[regionIdx];
			break;
		case CConst::suburbanPropEnv:
			popVal = suburbanPop[regionIdx];
			break;
		case CConst::ruralPropEnv:
			popVal = ruralPop[regionIdx];
			break;
		case CConst::barrenPropEnv:
			popVal = barrenPop[regionIdx];
			break;
		default:
			throw std::runtime_error(
				ErrStream() << "ERROR in PopGridClass::getPropEnvPop: propEnvVal = "
					    << propEnvVal << " INVALID value");
			break;
	}

	return (popVal);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::getPropEnv()                                             ****/
/******************************************************************************************/
char PopGridClass::getPropEnv(int lonIdx, int latIdx) const
{
	return propEnv[lonIdx][latIdx];
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::getLonLatDeg()                                           ****/
/******************************************************************************************/
void PopGridClass::getLonLatDeg(int lonIdx, int latIdx, double &longitudeDeg, double &latitudeDeg)
{
	longitudeDeg = minLonDeg + (lonIdx + 0.5) * deltaLonDeg;
	latitudeDeg = minLatDeg + (latIdx + 0.5) * deltaLatDeg;

	if (longitudeDeg > 180.0) {
		longitudeDeg -= 360.0;
	}
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::getPop()                                                 ****/
/******************************************************************************************/
double PopGridClass::getPop(int lonIdx, int latIdx) const
{
	if (isCumulative) {
		throw("ERROR in PopGridClass::getPop(), pop grid is cumulative\n");
	}

	return pop[lonIdx][latIdx];
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::getPopFromCDF()                                          ****/
/******************************************************************************************/
double PopGridClass::getPopFromCDF(int lonIdx, int latIdx) const
{
	double population;

	if (!isCumulative) {
		throw("ERROR in PopGridClass::getPopFromCDF(), pop grid not cumulative\n");
	}

	if ((lonIdx == 0) && (latIdx == 0)) {
		population = pop[0][0];
	} else if (latIdx == 0) {
		population = pop[lonIdx][latIdx] - pop[lonIdx - 1][numLat - 1];
	} else {
		population = pop[lonIdx][latIdx] - pop[lonIdx][latIdx - 1];
	}

	return population;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::getProbFromCDF()                                         ****/
/******************************************************************************************/
double PopGridClass::getProbFromCDF(int lonIdx, int latIdx) const
{
	double population = getPopFromCDF(lonIdx, latIdx);

	double prob = population / pop[numLon - 1][numLat - 1];

	return prob;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::readData()                                               ****/
/******************************************************************************************/
void PopGridClass::readData(std::string filename,
			    const std::vector<std::string> &regionNameListVal,
			    const std::vector<int> &regionIDListVal,
			    int numLonVal,
			    double deltaLonDegVal,
			    double minLonDegVal,
			    int numLatVal,
			    double deltaLatDegVal,
			    double minLatDegVal)
{
	LOGGER_INFO(logger) << "Reading population density file: " << filename << " ...";

	int lonIdx, latIdx;

	if (regionIDListVal.size() != regionNameListVal.size()) {
		throw("ERROR creating PopGridClass, inconsistent region name and ID lists\n");
	}

	regionNameList = regionNameListVal;

	numRegion = regionIDListVal.size();

	/**************************************************************************************/
	/**** Set parameters                                                               ****/
	/**************************************************************************************/
	numLon = numLonVal;
	deltaLonDeg = deltaLonDegVal;
	minLonDeg = minLonDegVal;

	numLat = numLatVal;
	deltaLatDeg = deltaLatDegVal;
	minLatDeg = minLatDegVal;

	double maxLonDeg = minLonDeg + numLon * deltaLonDeg;
	double maxLatDeg = minLatDeg + numLat * deltaLatDeg;
	/**************************************************************************************/

	/**************************************************************************************/
	/**** Allocate matrix and initialize to zeros                                      ****/
	/**************************************************************************************/
	pop = (double **)malloc(numLon * sizeof(double *));
	propEnv = (char **)malloc(numLon * sizeof(char *));
	region = (int **)malloc(numLon * sizeof(int *));
	for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
		pop[lonIdx] = (double *)malloc(numLat * sizeof(double));
		propEnv[lonIdx] = (char *)malloc(numLat * sizeof(char));
		region[lonIdx] = (int *)malloc(numLat * sizeof(int));
		for (latIdx = 0; latIdx < numLat; latIdx++) {
			pop[lonIdx][latIdx] = 0.0; // default to no population
			propEnv[lonIdx][latIdx] = 'B'; // Default to barren outside defined area
			region[lonIdx][latIdx] = 0; // default to first region
		}
	}
	/**************************************************************************************/

	// variables updated for each row in database
	double longitudeDeg, latitudeDeg;
	double density;
	int regionIdx, regionVal;
	double area;
	double lonIdxDbl;
	double latIdxDbl;

	urbanPop.resize(numRegion);
	suburbanPop.resize(numRegion);
	ruralPop.resize(numRegion);
	barrenPop.resize(numRegion);

	std::vector<double> urbanArea(numRegion);
	std::vector<double> suburbanArea(numRegion);
	std::vector<double> ruralArea(numRegion);
	std::vector<double> barrenArea(numRegion);

	for (regionIdx = 0; regionIdx < numRegion; regionIdx++) {
		urbanPop[regionIdx] = 0.0;
		suburbanPop[regionIdx] = 0.0;
		ruralPop[regionIdx] = 0.0;
		barrenPop[regionIdx] = 0.0;

		urbanArea[regionIdx] = 0.0;
		suburbanArea[regionIdx] = 0.0;
		ruralArea[regionIdx] = 0.0;
		barrenArea[regionIdx] = 0.0;
	}

	double totalArea = 0.0;
	double totalPop = 0.0;
	// bool foundLabelLine = false;
	// bool hasRegion = false;

	std::vector<PopulationRecord> rows;
	PopulationDatabase::loadPopulationData(QString::fromStdString(filename),
					       rows,
					       minLatDeg,
					       maxLatDeg,
					       minLonDeg,
					       maxLonDeg); // add buffer *1.5

	// iterate through returned rows in population db and add them to members
	for (int r = 0; r < (int)rows.size(); r++) {
		// Grabs the longitude, latitude, and density from a row
		longitudeDeg = rows.at(r).longitude;
		latitudeDeg = rows.at(r).latitude;
		density = rows.at(r).density * 1.0e-6; // convert from

		regionVal = 0; // only support 1 region

		if (longitudeDeg < minLonDeg) {
			longitudeDeg += 360.0;
		}

		lonIdxDbl = (longitudeDeg - minLonDeg) / deltaLonDeg;
		latIdxDbl = (latitudeDeg - minLatDeg) / deltaLatDeg;

		lonIdx = (int)floor(lonIdxDbl);
		latIdx = (int)floor(latIdxDbl);

		// When we store in db we only check that lat/lon values are valid coordinates, but
		// not that they are on grid
		if (fabs(lonIdxDbl - lonIdx - 0.5) > 0.05) {
			throw std::runtime_error(ErrStream()
						 << "ERROR: Invalid population density data file \""
						 << filename << "(" << r
						 << ")\" longitude value not on grid, lonIdxDbl = "
						 << lonIdxDbl);
		}

		if (fabs(latIdxDbl - latIdx - 0.5) > 0.05) {
			throw std::runtime_error(ErrStream()
						 << "ERROR: Invalid population density data file \""
						 << filename << "(" << r
						 << ")\" latitude value not on grid, latIdxDbl = "
						 << latIdxDbl);
		}

		// area of population tile
		area = CConst::earthRadius * CConst::earthRadius * cos(latitudeDeg * M_PI / 180.0) *
		       deltaLonDeg * deltaLatDeg * (M_PI / 180.0) * (M_PI / 180.0);

		// assign data value to class members
		double populationVal = density * area;
		pop[lonIdx][latIdx] = populationVal;

		region[lonIdx][latIdx] = regionVal;

		if (density >=
		    densityThrUrban) { // Check density against different thresholds to determine
				       // what the population environment is (urban, suburban, etc.)
			urbanPop[regionVal] += populationVal;
			urbanArea[regionVal] += area;
			propEnv[lonIdx][latIdx] = 'U'; // Urban
		} else if (density >= densityThrSuburban) {
			suburbanPop[regionVal] += populationVal;
			suburbanArea[regionVal] += area;
			propEnv[lonIdx][latIdx] = 'S'; // Suburban
		} else if (density >= densityThrRural) {
			ruralPop[regionVal] += populationVal;
			ruralArea[regionVal] += area;
			propEnv[lonIdx][latIdx] = 'R'; // Ruran
		} else {
			barrenPop[regionVal] += populationVal;
			barrenArea[regionVal] += area;
			propEnv[lonIdx][latIdx] = 'B'; // Barren
		} // Char is used since working with big files, so 8 bit char is better than 32 bit
		  // int

		// add to totals
		totalArea += area;
		totalPop += pop[lonIdx][latIdx];
	}

	LOGGER_INFO(logger) << "Lines processed: " << rows.size();
	LOGGER_INFO(logger) << "TOTAL INTEGRATED POPULATION: " << totalPop;
	LOGGER_INFO(logger) << "TOTAL INTEGRATED AREA: " << totalArea;
	for (regionIdx = 0; regionIdx < numRegion; regionIdx++) {
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " URBAN    POPULATION: " << urbanPop[regionIdx] << " "
				    << 100 * urbanPop[regionIdx] / totalPop << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " SUBURBAN POPULATION: " << suburbanPop[regionIdx] << " "
				    << 100 * suburbanPop[regionIdx] / totalPop << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " RURAL    POPULATION: " << ruralPop[regionIdx] << " "
				    << 100 * ruralPop[regionIdx] / totalPop << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " BARREN   POPULATION: " << barrenPop[regionIdx] << " "
				    << 100 * barrenPop[regionIdx] / totalPop << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " URBAN    AREA: " << urbanArea[regionIdx] << " "
				    << 100 * urbanArea[regionIdx] / totalArea << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " SUBURBAN AREA: " << suburbanArea[regionIdx] << " "
				    << 100 * suburbanArea[regionIdx] / totalArea << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " RURAL    AREA: " << ruralArea[regionIdx] << " "
				    << 100 * ruralArea[regionIdx] / totalArea << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " BARREN   AREA: " << barrenArea[regionIdx] << " "
				    << 100 * barrenArea[regionIdx] / totalArea << " %";
	}
	/**************************************************************************************/

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::setDimensions()                                          ****/
/******************************************************************************************/
void PopGridClass::setDimensions(int numLonVal,
				 double deltaLonDegVal,
				 double minLonDegVal,
				 int numLatVal,
				 double deltaLatDegVal,
				 double minLatDegVal)
{
	int lonIdx, latIdx;

	/**************************************************************************************/
	/**** Set parameters                                                               ****/
	/**************************************************************************************/
	numLon = numLonVal;
	deltaLonDeg = deltaLonDegVal;
	minLonDeg = minLonDegVal;

	numLat = numLatVal;
	deltaLatDeg = deltaLatDegVal;
	minLatDeg = minLatDegVal;

	isCumulative = false;
	/**************************************************************************************/

	/**************************************************************************************/
	/**** Allocate matrix and initialize to zeros                                      ****/
	/**************************************************************************************/
	pop = (double **)malloc(numLon * sizeof(double *));
	propEnv = (char **)malloc(numLon * sizeof(char *));
	region = (int **)malloc(numLon * sizeof(int *));
	for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
		pop[lonIdx] = (double *)malloc(numLat * sizeof(double));
		propEnv[lonIdx] = (char *)malloc(numLat * sizeof(char));
		region[lonIdx] = (int *)malloc(numLat * sizeof(int));
		for (latIdx = 0; latIdx < numLat; latIdx++) {
			pop[lonIdx][latIdx] = 0.0;
			propEnv[lonIdx][latIdx] = 'X';
			region[lonIdx][latIdx] = -1;
		}
	}
	/**************************************************************************************/

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::scale()                                               ****/
/******************************************************************************************/
void PopGridClass::scale(std::vector<double> urbanPopVal,
			 std::vector<double> suburbanPopVal,
			 std::vector<double> ruralPopVal,
			 std::vector<double> barrenPopVal)
{
	int regionIdx;

	if (isCumulative) {
		throw("ERROR in PopGridClass::scale(), pop grid cumulative\n");
	}

	std::vector<double> scaleUrban(numRegion);
	std::vector<double> scaleSuburban(numRegion);
	std::vector<double> scaleRural(numRegion);
	std::vector<double> scaleBarren(numRegion);

	for (regionIdx = 0; regionIdx < numRegion; regionIdx++) {
		scaleUrban[regionIdx] = urbanPopVal[regionIdx] / urbanPop[regionIdx];
		scaleSuburban[regionIdx] = suburbanPopVal[regionIdx] / suburbanPop[regionIdx];
		scaleRural[regionIdx] = ruralPopVal[regionIdx] / ruralPop[regionIdx];
		scaleBarren[regionIdx] = ((barrenPopVal[regionIdx] == 0.0) ?
						  0.0 :
						  barrenPopVal[regionIdx] / barrenPop[regionIdx]);

		urbanPop[regionIdx] = 0.0;
		suburbanPop[regionIdx] = 0.0;
		ruralPop[regionIdx] = 0.0;
		barrenPop[regionIdx] = 0.0;
	}

	int lonIdx, latIdx;

	double totalPop = 0.0;
	for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
		for (latIdx = 0; latIdx < numLat; latIdx++) {
			regionIdx = region[lonIdx][latIdx];
			switch (propEnv[lonIdx][latIdx]) {
				case 'U':
					pop[lonIdx][latIdx] *= scaleUrban[regionIdx];
					urbanPop[regionIdx] += pop[lonIdx][latIdx];
					break;
				case 'S':
					pop[lonIdx][latIdx] *= scaleSuburban[regionIdx];
					suburbanPop[regionIdx] += pop[lonIdx][latIdx];
					break;
				case 'R':
					pop[lonIdx][latIdx] *= scaleRural[regionIdx];
					ruralPop[regionIdx] += pop[lonIdx][latIdx];
					break;
				case 'B':
					pop[lonIdx][latIdx] *= scaleBarren[regionIdx];
					barrenPop[regionIdx] += pop[lonIdx][latIdx];
					break;
			}
			totalPop += pop[lonIdx][latIdx];
		}
	}

	int totalScaledPopulation = 0;
	for (regionIdx = 0; regionIdx < numRegion; regionIdx++) {
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " RLAN DEVICE URBAN    POPULATION: " << urbanPop[regionIdx]
				    << " " << 100 * urbanPop[regionIdx] / totalPop << " %";
		LOGGER_INFO(logger)
			<< "REGION " << regionNameList[regionIdx]
			<< " RLAN DEVICE SUBURBAN POPULATION: " << suburbanPop[regionIdx] << " "
			<< 100 * suburbanPop[regionIdx] / totalPop << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " RLAN DEVICE RURAL    POPULATION: " << ruralPop[regionIdx]
				    << " " << 100 * ruralPop[regionIdx] / totalPop << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " RLAN DEVICE BARREN   POPULATION: " << barrenPop[regionIdx]
				    << " " << 100 * barrenPop[regionIdx] / totalPop << " %";
		totalScaledPopulation += urbanPop[regionIdx] + suburbanPop[regionIdx] +
					 ruralPop[regionIdx] + barrenPop[regionIdx];
	}
	LOGGER_INFO(logger) << "TOTAL_RLAN_DEVICE_POPULATION: " << totalScaledPopulation;

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::writeDensity()                                        ****/
/******************************************************************************************/
void PopGridClass::writeDensity(std::string filename, bool dumpPopGrid)
{
	FILE *fp;
	std::ostringstream errStr;

	if (isCumulative) {
		throw("ERROR in PopGridClass::writeDensity(), pop grid cumulative\n");
	}

	if (!(fp = fopen(filename.c_str(), "wb"))) {
		throw std::runtime_error(ErrStream() << "ERROR: Unable to write to file \""
						     << filename << "\"");
	}

	int lonIdx, latIdx;

	if (dumpPopGrid) {
		double popSum = 0.0;
		fprintf(fp, "lonIdx,latIdx,pop,popSum\n");
		for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
			for (latIdx = 0; latIdx < numLat; latIdx++) {
				popSum += pop[lonIdx][latIdx];
				fprintf(fp,
					"%d,%d,%.5f,%.5f\n",
					lonIdx,
					latIdx,
					pop[lonIdx][latIdx],
					popSum);
			}
		}
	} else {
		fprintf(fp,
			"Longitude (deg),Latitude (deg),Device density (#/sqkm),Propagation "
			"Environment\n");
		for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
			double longitudeDeg = minLonDeg + (lonIdx + 0.5) * deltaLonDeg;
			for (latIdx = 0; latIdx < numLat; latIdx++) {
				if ((propEnv[lonIdx][latIdx] != 'X') &&
				    (propEnv[lonIdx][latIdx] != 'B')) {
					double latitudeDeg = minLatDeg +
							     (latIdx + 0.5) * deltaLatDeg;
					double area = computeArea(lonIdx, latIdx);

					fprintf(fp,
						"%.5f,%.5f,%.3f,%c\n",
						longitudeDeg,
						latitudeDeg,
						(pop[lonIdx][latIdx] / area) * 1.0e6,
						propEnv[lonIdx][latIdx]);
				}
			}
		}
	}

	fclose(fp);

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::adjustRegion                                          ****/
/******************************************************************************************/
int PopGridClass::adjustRegion(double centerLongitudeDeg, double centerLatitudeDeg, double radius)
{
	int regionIdx;

	if (isCumulative) {
		throw("ERROR in PopGridClass::adjustRegion(), pop grid cumulative\n");
	}

	Vector3 centerPosition = EcefModel::geodeticToEcef(centerLatitudeDeg,
							   centerLongitudeDeg,
							   0.0);

	// Solve: radius = 2*EarthRadius*cos(centerLatitude)*sin(maxLonOffset/2);
	double maxLonOffset = 2 * asin(radius / (2 * CConst::earthRadius *
						 cos(centerLatitudeDeg * M_PI / 180.0)));

	// Solve: radius = 2*EarthRadius*sin(maxLatOffset/2);
	double maxLatOffset = 2 * asin(radius / (2 * CConst::earthRadius)) * 180.0 / M_PI;

	double newMinLon = centerLongitudeDeg - maxLonOffset;
	double newMaxLon = centerLongitudeDeg + maxLonOffset;
	double newMinLat = centerLatitudeDeg - maxLatOffset;
	double newMaxLat = centerLatitudeDeg + maxLatOffset;

	int minLonIdx = (int)floor((newMinLon - minLonDeg) / deltaLonDeg);
	int maxLonIdx = (int)floor((newMaxLon - minLonDeg) / deltaLonDeg) + 1;
	int minLatIdx = (int)floor((newMinLat - minLatDeg) / deltaLatDeg);
	int maxLatIdx = (int)floor((newMaxLat - minLatDeg) / deltaLatDeg) + 1;

	if (minLonIdx < 0) {
		minLonIdx = 0;
	}
	if (minLatIdx < 0) {
		minLatIdx = 0;
	}
	if (maxLonIdx > numLon - 1) {
		maxLonIdx = numLon - 1;
	}
	if (maxLatIdx > numLat - 1) {
		maxLatIdx = numLat - 1;
	}

	int newNumLon = maxLonIdx - minLonIdx + 1;
	int newNumLat = maxLatIdx - minLatIdx + 1;

	newMinLon = minLonDeg + minLonIdx * deltaLonDeg;
	newMinLat = minLatDeg + minLatIdx * deltaLatDeg;

	for (regionIdx = 0; regionIdx < numRegion; regionIdx++) {
		urbanPop[regionIdx] = 0.0;
		suburbanPop[regionIdx] = 0.0;
		ruralPop[regionIdx] = 0.0;
		barrenPop[regionIdx] = 0.0;
	}

	int totalPop = 0;
	int lonIdx, latIdx;
	/**************************************************************************************/
	/**** Allocate matrix                                                              ****/
	/**************************************************************************************/
	double **newPop = (double **)malloc(newNumLon * sizeof(double *));
	char **newPropEnv = (char **)malloc(newNumLon * sizeof(char *));
	int **newRegion = (int **)malloc(newNumLon * sizeof(int *));
	for (lonIdx = 0; lonIdx < newNumLon; lonIdx++) {
		newPop[lonIdx] = (double *)malloc(newNumLat * sizeof(double));
		newPropEnv[lonIdx] = (char *)malloc(newNumLat * sizeof(char));
		newRegion[lonIdx] = (int *)malloc(newNumLat * sizeof(int));
		double lonDeg = (newMinLon + lonIdx * deltaLonDeg);
		for (latIdx = 0; latIdx < newNumLat; latIdx++) {
			double latDeg = (newMinLat + latIdx * deltaLatDeg);
			Vector3 posn = EcefModel::geodeticToEcef(latDeg, lonDeg, 0.0);
			if ((posn - centerPosition).len() * 1000.0 <= radius) {
				newPop[lonIdx][latIdx] =
					pop[minLonIdx + lonIdx][minLatIdx + latIdx];
				newPropEnv[lonIdx][latIdx] =
					propEnv[minLonIdx + lonIdx][minLatIdx + latIdx];
				newRegion[lonIdx][latIdx] =
					region[minLonIdx + lonIdx][minLatIdx + latIdx];
			} else {
				newPop[lonIdx][latIdx] = 0.0;
				newPropEnv[lonIdx][latIdx] = 'X';
				newRegion[lonIdx][latIdx] = -1;
			}
			regionIdx = newRegion[lonIdx][latIdx];
			switch (newPropEnv[lonIdx][latIdx]) {
				case 'U':
					urbanPop[regionIdx] += newPop[lonIdx][latIdx];
					break;
				case 'S':
					suburbanPop[regionIdx] += newPop[lonIdx][latIdx];
					break;
				case 'R':
					ruralPop[regionIdx] += newPop[lonIdx][latIdx];
					break;
				case 'B':
					barrenPop[regionIdx] += newPop[lonIdx][latIdx];
					break;
			}
			totalPop += newPop[lonIdx][latIdx];
		}
	}
	/**************************************************************************************/

	if (pop) {
		for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
			free(pop[lonIdx]);
		}
		free(pop);
	}

	if (propEnv) {
		for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
			free(propEnv[lonIdx]);
		}
		free(propEnv);
	}
	if (region) {
		for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
			free(region[lonIdx]);
		}
		free(region);
	}

	pop = newPop;
	propEnv = newPropEnv;
	region = newRegion;
	minLonDeg = newMinLon;
	minLatDeg = newMinLat;
	numLon = newNumLon;
	numLat = newNumLat;

	return (totalPop);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::adjustRegion                                          ****/
/******************************************************************************************/
double PopGridClass::adjustRegion(ListClass<ULSClass *> *ulsList, double maxRadius)
{
	int regionIdx;
	time_t t1;
	char *tstr;

	t1 = time(NULL);
	tstr = strdup(ctime(&t1));
	strtok(tstr, "\n");
	std::cout << tstr << " : Beginning ADJUSTING SIMULATION REGION." << std::endl;
	free(tstr);
	std::cout << std::flush;

	double maxDistGridCenterToEdge = CConst::earthRadius *
					 sqrt((deltaLonDeg * deltaLonDeg +
					       deltaLatDeg * deltaLatDeg) *
					      (M_PI / 180.0) * (M_PI / 180.0)) /
					 2;
	double maxDistKMSQ = (maxRadius + maxDistGridCenterToEdge) *
			     (maxRadius + maxDistGridCenterToEdge) * 1.0e-6;

	int lonIdx, latIdx;
	int **possible = (int **)malloc(numLon * sizeof(int *));
	for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
		possible[lonIdx] = (int *)malloc(numLat * sizeof(int));
		for (latIdx = 0; latIdx < numLat; latIdx++) {
			possible[lonIdx][latIdx] = 0;
		}
	}

	double minDeltaY = CConst::earthRadius * deltaLatDeg * (M_PI / 180.0);
	double cosa = cos(minLatDeg * M_PI / 180.0);
	double cosb = cos((minLatDeg + (numLat - 1) * deltaLatDeg) * (M_PI / 180.0));
	double mincos = std::min(cosa, cosb);
	double minDeltaX = CConst::earthRadius * deltaLonDeg * mincos * (M_PI / 180.0);
	int offsetLonIdx = ceil((maxRadius + maxDistGridCenterToEdge) / minDeltaX) + 1;
	int offsetLatIdx = ceil((maxRadius + maxDistGridCenterToEdge) / minDeltaY) + 1;

	for (int ulsIdx = 0; ulsIdx < ulsList->getSize(); ulsIdx++) {
		ULSClass *uls = (*ulsList)[ulsIdx];
		int ulsLonIdx = (int)floor((uls->getRxLongitudeDeg() - minLonDeg) / deltaLonDeg);
		int ulsLatIdx = (int)floor((uls->getRxLatitudeDeg() - minLatDeg) / deltaLatDeg);
		int lonIdxStart = ulsLonIdx - offsetLonIdx;
		if (lonIdxStart < 0) {
			lonIdxStart = 0;
		}
		int lonIdxStop = ulsLonIdx + offsetLonIdx;
		if (lonIdxStop > numLon - 1) {
			lonIdxStop = numLon - 1;
		}

		int latIdxStart = ulsLatIdx - offsetLatIdx;
		if (latIdxStart < 0) {
			latIdxStart = 0;
		}
		int latIdxStop = ulsLatIdx + offsetLatIdx;
		if (latIdxStop > numLat - 1) {
			latIdxStop = numLat - 1;
		}

		for (lonIdx = lonIdxStart; lonIdx <= lonIdxStop; lonIdx++) {
			for (latIdx = latIdxStart; latIdx <= latIdxStop; latIdx++) {
				possible[lonIdx][latIdx] = 1;
			}
		}
	}

	bool initFlag = true;
	int minLonIdx = -1;
	int maxLonIdx = -1;
	int minLatIdx = -1;
	int maxLatIdx = -1;
	for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
		double longitudeDeg = minLonDeg + lonIdx * deltaLonDeg;
		for (latIdx = 0; latIdx < numLat; latIdx++) {
			double latitudeDeg = minLatDeg + latIdx * deltaLatDeg;

			bool useFlag = false;

			if (possible[lonIdx][latIdx]) {
				Vector3 gridPosition = EcefModel::geodeticToEcef(latitudeDeg,
										 longitudeDeg,
										 0.0);

				for (int ulsIdx = 0; (ulsIdx < ulsList->getSize()) && (!useFlag);
				     ulsIdx++) {
					ULSClass *uls = (*ulsList)[ulsIdx];
					Vector3 losPath = uls->getRxPosition() - gridPosition;
					double pathDistKMSQ = losPath.dot(losPath);
					if (pathDistKMSQ < maxDistKMSQ) {
						useFlag = true;
					}
				}
			}

			if (useFlag) {
				if (initFlag || (lonIdx < minLonIdx)) {
					minLonIdx = lonIdx;
				}
				if (initFlag || (lonIdx > maxLonIdx)) {
					maxLonIdx = lonIdx;
				}
				if (initFlag || (latIdx < minLatIdx)) {
					minLatIdx = latIdx;
				}
				if (initFlag || (latIdx > maxLatIdx)) {
					maxLatIdx = latIdx;
				}
				initFlag = false;
			} else {
				pop[lonIdx][latIdx] = 0.0;
				propEnv[lonIdx][latIdx] = 'X';
				region[lonIdx][latIdx] = -1;
			}
		}

		if ((lonIdx % 100) == 99) {
			std::cout << "ADJUSTED " << (double)(lonIdx + 1.0) * 100 / numLon << " %"
				  << std::endl;
			std::cout << std::flush;
		}
	}

	int newNumLon = maxLonIdx - minLonIdx + 1;
	int newNumLat = maxLatIdx - minLatIdx + 1;

	double newMinLon = minLonDeg + minLonIdx * deltaLonDeg;
	double newMinLat = minLatDeg + minLatIdx * deltaLatDeg;

	std::vector<double> urbanArea(numRegion);
	std::vector<double> suburbanArea(numRegion);
	std::vector<double> ruralArea(numRegion);
	std::vector<double> barrenArea(numRegion);

	for (regionIdx = 0; regionIdx < numRegion; regionIdx++) {
		urbanPop[regionIdx] = 0.0;
		suburbanPop[regionIdx] = 0.0;
		ruralPop[regionIdx] = 0.0;
		barrenPop[regionIdx] = 0.0;

		urbanArea[regionIdx] = 0.0;
		suburbanArea[regionIdx] = 0.0;
		ruralArea[regionIdx] = 0.0;
		barrenArea[regionIdx] = 0.0;
	}

	double totalArea = 0.0;
	double totalPop = 0.0;
	/**************************************************************************************/
	/**** Allocate matrix                                                              ****/
	/**************************************************************************************/
	double **newPop = (double **)malloc(newNumLon * sizeof(double *));
	char **newPropEnv = (char **)malloc(newNumLon * sizeof(char *));
	int **newRegion = (int **)malloc(newNumLon * sizeof(int *));
	for (lonIdx = 0; lonIdx < newNumLon; lonIdx++) {
		newPop[lonIdx] = (double *)malloc(newNumLat * sizeof(double));
		newPropEnv[lonIdx] = (char *)malloc(newNumLat * sizeof(char));
		newRegion[lonIdx] = (int *)malloc(newNumLat * sizeof(int));
		for (latIdx = 0; latIdx < newNumLat; latIdx++) {
			double latitudeDeg = minLatDeg + latIdx * deltaLatDeg;
			double area = CConst::earthRadius * CConst::earthRadius *
				      cos(latitudeDeg * M_PI / 180.0) * deltaLonDeg * deltaLatDeg *
				      (M_PI / 180.0) * (M_PI / 180.0);

			newPop[lonIdx][latIdx] = pop[minLonIdx + lonIdx][minLatIdx + latIdx];
			newPropEnv[lonIdx][latIdx] =
				propEnv[minLonIdx + lonIdx][minLatIdx + latIdx];
			newRegion[lonIdx][latIdx] = region[minLonIdx + lonIdx][minLatIdx + latIdx];

			regionIdx = newRegion[lonIdx][latIdx];
			switch (newPropEnv[lonIdx][latIdx]) {
				case 'U':
					urbanPop[regionIdx] += newPop[lonIdx][latIdx];
					urbanArea[regionIdx] += area;
					totalArea += area;
					break;
				case 'S':
					suburbanPop[regionIdx] += newPop[lonIdx][latIdx];
					suburbanArea[regionIdx] += area;
					totalArea += area;
					break;
				case 'R':
					ruralPop[regionIdx] += newPop[lonIdx][latIdx];
					ruralArea[regionIdx] += area;
					totalArea += area;
					break;
				case 'B':
					barrenPop[regionIdx] += newPop[lonIdx][latIdx];
					barrenArea[regionIdx] += area;
					totalArea += area;
					break;
			}
			totalPop += newPop[lonIdx][latIdx];
		}
	}
	/**************************************************************************************/

	if (pop) {
		for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
			free(pop[lonIdx]);
		}
		free(pop);
	}

	if (propEnv) {
		for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
			free(propEnv[lonIdx]);
		}
		free(propEnv);
	}
	if (region) {
		for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
			free(region[lonIdx]);
		}
		free(region);
	}

	if (possible) {
		for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
			free(possible[lonIdx]);
		}
		free(possible);
	}

	pop = newPop;
	propEnv = newPropEnv;
	region = newRegion;
	minLonDeg = newMinLon;
	minLatDeg = newMinLat;
	numLon = newNumLon;
	numLat = newNumLat;

	for (regionIdx = 0; regionIdx < numRegion; regionIdx++) {
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " ADJUSTED URBAN    POPULATION: " << urbanPop[regionIdx]
				    << " " << 100 * urbanPop[regionIdx] / totalPop << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " ADJUSTED SUBURBAN POPULATION: " << suburbanPop[regionIdx]
				    << " " << 100 * suburbanPop[regionIdx] / totalPop << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " ADJUSTED RURAL    POPULATION: " << ruralPop[regionIdx]
				    << " " << 100 * ruralPop[regionIdx] / totalPop << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " ADJUSTED BARREN   POPULATION: " << barrenPop[regionIdx]
				    << " " << 100 * barrenPop[regionIdx] / totalPop << " %";

		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " ADJUSTED URBAN    AREA: " << urbanArea[regionIdx] << " "
				    << 100 * urbanArea[regionIdx] / totalArea << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " ADJUSTED SUBURBAN AREA: " << suburbanArea[regionIdx] << " "
				    << 100 * suburbanArea[regionIdx] / totalArea << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " ADJUSTED RURAL    AREA: " << ruralArea[regionIdx] << " "
				    << 100 * ruralArea[regionIdx] / totalArea << " %";
		LOGGER_INFO(logger) << "REGION " << regionNameList[regionIdx]
				    << " ADJUSTED BARREN   AREA: " << barrenArea[regionIdx] << " "
				    << 100 * barrenArea[regionIdx] / totalArea << " %";
	}
	LOGGER_INFO(logger) << "TOTAL_ADJUSTED_POPULATION: " << totalPop;
	LOGGER_INFO(logger) << "TOTAL_ADJUSTED_AREA: " << totalArea;

	t1 = time(NULL);
	tstr = strdup(ctime(&t1));
	strtok(tstr, "\n");
	std::cout << tstr << " : DONE ADJUSTING SIMULATION REGION." << std::endl;
	free(tstr);
	std::cout << std::flush;

	return (totalPop);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::makeCDF                                               ****/
/******************************************************************************************/
void PopGridClass::makeCDF()
{
	int lonIdx, latIdx;

	if (isCumulative) {
		throw("ERROR in PopGridClass::makeCDF(), pop grid already cumulative\n");
	}

	double sum = 0.0;
	for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
		for (latIdx = 0; latIdx < numLat; latIdx++) {
			sum += pop[lonIdx][latIdx];
			pop[lonIdx][latIdx] = sum;
		}
	}

	isCumulative = true;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::check                                                 ****/
/******************************************************************************************/
void PopGridClass::check(std::string s)
{
	int lonIdx, latIdx;

	for (lonIdx = 0; lonIdx < numLon; lonIdx++) {
		for (latIdx = 0; latIdx < numLat; latIdx++) {
			if ((propEnv[lonIdx][latIdx] == 'X') && (pop[lonIdx][latIdx] != 0.0)) {
				std::cout << "CHECK GRID: " << s << " " << lonIdx << " " << latIdx
					  << " POP = " << pop[lonIdx][latIdx] << std::endl;
			}
		}
	}

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::findDeg()                                                ****/
/**** Find given longitude/latitude coordinates in PopGridClass, if coordinates are    ****/
/**** outside the grid, set lonIdx = -1, latIdx = -1, provEnv = (char) NULL            ****/
/******************************************************************************************/
void PopGridClass::findDeg(double longitudeDeg,
			   double latitudeDeg,
			   int &lonIdx,
			   int &latIdx,
			   char &propEnvVal,
			   int &regionIdx) const
{
	if (longitudeDeg < minLonDeg) {
		longitudeDeg += 360.0;
	}
	lonIdx = (int)floor((longitudeDeg - minLonDeg) / deltaLonDeg);
	latIdx = (int)floor((latitudeDeg - minLatDeg) / deltaLatDeg);

	if ((lonIdx < 0) || (lonIdx > numLon - 1) || (latIdx < 0) || (latIdx > numLat - 1)) {
		lonIdx = -1;
		latIdx = -1;
		propEnvVal = 0;
		regionIdx = -1;
	} else {
		propEnvVal = propEnv[lonIdx][latIdx];
		regionIdx = region[lonIdx][latIdx];
	}

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PopGridClass::computeArea()                                            ****/
/**** Compute area of deltaLon x deltaLat rectangle.                                   ****/
/******************************************************************************************/
double PopGridClass::computeArea(int /* lonIdx */, int latIdx) const
{
	double latitudeDeg = minLatDeg + (latIdx + 0.5) * deltaLatDeg;
	double area = CConst::earthRadius * CConst::earthRadius * cos(latitudeDeg * M_PI / 180.0) *
		      deltaLonDeg * deltaLatDeg * (M_PI / 180.0) * (M_PI / 180.0);

	return (area);
}
/******************************************************************************************/
