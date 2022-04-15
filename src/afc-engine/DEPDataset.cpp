/******************************************************************************************/
/**** FILE : DEPDataset.cpp                                                            ****/
/******************************************************************************************/

// Note that for DEP filenames, coordinates in the filename correspond to the upper left corner of the tile.
// This is diffenent from SRTM filenames where coordinates in the filename correspond to the lower left corner of the tile.
#include <math.h>
#include <stdexcept>
#include <iostream>

#include <QtConcurrentMap>
#include "DEPDataset.h"
//#include "MemoryMap.h"
#define SEQUENTIAL

inline int DEPDatasetClass::latlonToHashDirect(int tileLat, int tileLon){
	return tileLat * 10000 + tileLon;
}


#if 0
#if 1
// 90 m SRTM data - 3 seconds
qint16 GdalDataDir::INVALID_HEIGHT = -32768;
static const int SIZE = 1201;
static const double STEP = 1.0/1200.0; // only valid for DTED Level 2
static const double INV_STEP = 1200.0;
static const int INV_STEP_INT = 1200;
static const int MAX_CACHED = 1000;
#else
// 30 m SRTM data - 1 second
qint16 GdalDataDir::INVALID_HEIGHT = -32768;
static const int SIZE = 3601;
static const double STEP = 1.0/3600.0; // only valid for DTED Level 2
static const double INV_STEP = 3600.0;
static const int INV_STEP_INT = 3600;
static const int MAX_CACHED = 1000;
int GdalDataDir::MAX_CACHED = 600;
#endif
#endif

namespace {
	// Logger for all instances of class
	LOGGER_DEFINE_GLOBAL(logger, "AfcManager")
}

DEPDatasetClass::DEPDatasetClass(std::string directoryVal, int pointsPerDegreeVal, bool fastFlagVal)
{
	std::string errMsg;
	std::string str;
	GDALAllRegister();

	directory = directoryVal;
	pointsPerDegree = pointsPerDegreeVal;
	fastFlag = fastFlagVal;

	switch(pointsPerDegree) {
		case 3600:
			str = "DEP 1 arcsec data";
			size = 3612;
			break;
		case 10800:
			str = "DEP 1/3 arcsec data";
			size = 10812;
			break;
		default:
			errMsg = "ERROR: Unable to create DEPDatasetClass, Invalid POINTS_PER_DEGREE value of " + std::to_string(pointsPerDegree);
			throw std::runtime_error(errMsg);
	}

	LOGGER_INFO(logger) << str;

	overlap = (size - pointsPerDegree)/2;
	resolution = 1.0/pointsPerDegree;

	INVALID_HEIGHT = -32768.0f;
}


int DEPDatasetClass::readRegion(double minlat, double minlon, double maxlat, double maxlon)
{
	if (directory.empty()) {
		return(0);
	}

	QDir dataDir(QString(directory.c_str()));

	QStringList entries = dataDir.entryList();

	int lonStart = (int) floor(minlon);
	int lonStop  = (int) floor(maxlon);

	int latStart = (int) ceil(minlat);
	int latStop  = (int) ceil(maxlat);

	int numRead = 0;

	foreach(QString entry, entries) {
		int lat, lon;
		bool useFileFlag = true;

		static QRegExp fileRx("(n|s)(\\d\\d)(e|w)(\\d\\d\\d).tif$");
		if (entry.contains(fileRx)) {
			if (fileRx.cap(1) == "n") {
				lat = fileRx.cap(2).toInt();
			}
			else if (fileRx.cap(1) == "s") {
				lat = -fileRx.cap(2).toInt();
			} else {
				useFileFlag = false;
			}

			if (fileRx.cap(3) == "e") {
				lon = fileRx.cap(4).toInt();
			}
			else if (fileRx.cap(3) == "w") {
				lon = -fileRx.cap(4).toInt();
			} else {
				useFileFlag = false;
			}

			if ((lon < lonStart) || (lon > lonStop) || (lat < latStart) || (lat > latStop)) {
				useFileFlag = false;
			}
		} else {
			useFileFlag = false;
		}

		if (useFileFlag) {
			QString filename = dataDir.filePath(entry);
			readFile(filename.toStdString(), lon, lat);

#if 0
			/// Read the tile.
			uint key = latlonToHashDirect(lat, lon);
			_mutexes.insert(key, new QReadWriteLock());

			if (fastFlag) {
				_availableData.insert(key, filename);
				float *hgtData = readFile(_availableData.value(key).toStdString(), lon, lat);

				GDALDataset *hgtDataset = readFile(filename.toStdString(), lon, lat);

				LOGGER_DEBUG(logger) << "[" << numRead << "] read file: " << entry << " " << filename;

				_cachedData.insert(key, hgtDataset);
#endif
				numRead++;
			}        
		}

		LOGGER_INFO(logger) << "Read DEP DataFiles [" << numRead << "]"; 
		return numRead;
	}


	void DEPDatasetClass::readFile(std::string filename, int lon, int lat)
	{
		GDALDataset *poDataset;

		poDataset = static_cast<GDALDataset *>(GDALOpen(filename.c_str(), GA_ReadOnly));

		int xsize = poDataset->GetRasterXSize();
		int ysize = poDataset->GetRasterYSize();
		GDALRasterBand *rb = poDataset->GetRasterBand(1);

		int hasNoData;
		double nodataValue = rb->GetNoDataValue(&hasNoData);

		if (rb->GetRasterDataType() != GDT_Float32) {
			std::string errMsg = "ERROR: FILE \"" + filename + "\" contains raster data that is not of type GDT_Float32";
			throw std::runtime_error(errMsg);
		}

		if (xsize != size || ysize != size) {
			std::string errMsg = "ERROR: FILE \"" + filename + "\" has size " + std::to_string(xsize) + "x" + std::to_string(ysize)
				+ ", expected size " + std::to_string(size) + "x" + std::to_string(size);
			throw std::runtime_error(errMsg);
		}


		double adfGeoTransform[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
		if( poDataset->GetGeoTransform( adfGeoTransform ) != CE_None ) {
			std::string errMsg = "ERROR: FILE \"" + filename + "\" error getting GEO transform";
			throw std::runtime_error(errMsg);
		}

		double originX = adfGeoTransform[0];
		double originY = adfGeoTransform[3];
		double resolutionX = adfGeoTransform[1];
		double resolutionY = adfGeoTransform[5];

		double expectedOriginX = ((double) lon) - overlap*resolution;
		double expectedOriginY = ((double) lat) + overlap*resolution;

		double eps = 1.0e-6;

		if (fabs(originX - expectedOriginX) > eps) {
			std::string errMsg = "ERROR: FILE \"" + filename + "\" originX not as expected";
			throw std::runtime_error(errMsg);
		}

		if (fabs(originY - expectedOriginY) > eps) {
			std::string errMsg = "ERROR: FILE \"" + filename + "\" originY not as expected";
			throw std::runtime_error(errMsg);
		}

		if (fabs(resolutionX - resolution) > eps) {
			std::string errMsg = "ERROR: FILE \"" + filename + "\" resolutionX not as expected";
			throw std::runtime_error(errMsg);
		}

		// resolutionY should be -resolution
		if (fabs(resolutionY + resolution) > eps) {
			std::string errMsg = "ERROR: FILE \"" + filename + "\" resolutionX not as expected";
			throw std::runtime_error(errMsg);
		}

		uint key = latlonToHashDirect(lat, lon);
		_mutexes.insert(key, new QReadWriteLock());

		if (fastFlag) {
			float *buffer = new float[xsize * ysize];
			rb->RasterIO(GF_Read, 0, 0, xsize, ysize, buffer, xsize, ysize, GDT_Float32, 0, 0);

			GDALClose(poDataset);

			int numNoData = 0;
			if (hasNoData) {
				int buffIdx;
				for(buffIdx=0; buffIdx<xsize*ysize; ++buffIdx) {
					if (buffer[buffIdx] == nodataValue) {
						buffer[buffIdx] = INVALID_HEIGHT;
						numNoData++;
					}
				}
				LOGGER_DEBUG(logger) << "DEP FILE " << filename << " NUM_NO_DATA = " << numNoData;
			} else {
				LOGGER_DEBUG(logger) << "DEP FILE " << filename << " NO_DATA UNDEFINED ";
			}

			LOGGER_DEBUG(logger) << " read file: " << filename;

			_cachedDataFast.insert(key, buffer);
		} else {
			_cachedDataGDAL.insert(key, poDataset);
		}

		return;
	}

	float DEPDatasetClass::getHeight(double lat, double lon, int *hashKey, float **hashData) const
	{
		const int tileLon = (int) floor(lon);
		const int tileLat = (int) ceil(lat);
		const int originLonPixel = (tileLon * pointsPerDegree) - overlap;
		const int originLatPixel = (tileLat * pointsPerDegree) + overlap;

		const int lonIndex = (int) floor(lon*pointsPerDegree - originLonPixel);
		const int latIndex = (int) floor(originLatPixel - lat*pointsPerDegree);

		float height;

		int key = latlonToHashDirect(tileLat, tileLon);

		if (fastFlag) {
			if (hashKey) {
				if(key == *hashKey){
					return (*hashData)[latIndex * size + lonIndex];
				}
			}
			QHash <int, float *>::const_iterator cacheIt = _cachedDataFast.constFind(key);
			if (cacheIt == _cachedDataFast.constEnd()) {
				height = INVALID_HEIGHT;
			} else {
				float *hgtData = cacheIt.value();
				if (hashKey) {
					*hashKey = key;
					*hashData = hgtData;
				}
				height = hgtData[latIndex * size + lonIndex];
			}
		} else {
			QHash <int, GDALDataset *>::const_iterator dataSet = _cachedDataGDAL.constFind(key);
			if (dataSet == _cachedDataGDAL.constEnd()) {
				height = INVALID_HEIGHT;
			} else {
				CPLErr readError = dataSet.value()->GetRasterBand(1)->RasterIO(GF_Read, lonIndex, latIndex, 1, 1, &height, 1, 1, GDT_Float32, 0, 0);
				if (readError != CPLErr::CE_None) {
					throw std::runtime_error("GdalDataDir::readFile(): error");
				} else {
					int hasNoData;
					float nodataValue = dataSet.value()->GetRasterBand(1)->GetNoDataValue(&hasNoData);
					if (hasNoData && (height == nodataValue)) {
						height = INVALID_HEIGHT;
					}
				}
			}
		}

		return height;
	}

	DEPDatasetClass::~DEPDatasetClass()
	{
		if (fastFlag) {
			foreach(float *data, _cachedDataFast.values()) {
				delete [] data;
			}
		} else {
			foreach(GDALDataset *poDataset, _cachedDataGDAL.values()) {
				GDALClose(poDataset);
			}
		}

		qDeleteAll(_mutexes.values());
	}
