/******************************************************************************************/
/**** FILE : terrain.cpp                                                               ****/
/******************************************************************************************/

#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <QtCore>

#include "GdalDataDir.h"
#include "WorldData.h"
#include "global_fn.h"
#include "terrain.h"
#include "cconst.h"

#include "rkflogging/ErrStream.h"
#include "rkflogging/Logging.h"
#include "rkflogging/LoggingConfig.h"


std::atomic_llong TerrainClass::numLidar;
std::atomic_llong TerrainClass::numSRTM;
std::atomic_llong TerrainClass::numGlobal;
std::atomic_llong TerrainClass::numDEP;

namespace {
	// Logger for all instances of class
	LOGGER_DEFINE_GLOBAL(logger, "terrain")
}

/******************************************************************************************/
/**** FUNCTION: TerrainClass::TerrainClass()                                           ****/
/******************************************************************************************/
TerrainClass::TerrainClass(QString lidarDir, std::string srtmDir, std::string depDir, QString globeDir,
		double terrainMinLat, double terrainMinLon, double terrainMaxLat, double terrainMaxLon,
		double terrainMinLatBldg, double terrainMinLonBldg, double terrainMaxLatBldg, double terrainMaxLonBldg,
		int maxLidarRegionLoadVal) :
	maxLidarRegionLoad(maxLidarRegionLoadVal)
{
	if (!lidarDir.isEmpty())
	{
		LOGGER_INFO(logger) << "Loading building+terrain data from " << lidarDir;
		readLidarInfo(lidarDir);
		readLidarData(terrainMinLatBldg, terrainMinLonBldg, terrainMaxLatBldg, terrainMaxLonBldg);
		minLidarLongitude = terrainMinLonBldg;
		maxLidarLongitude = terrainMaxLonBldg;
		minLidarLatitude  = terrainMinLatBldg;
		maxLidarLatitude  = terrainMaxLatBldg;
	}

	if (!depDir.empty())
	{
		LOGGER_INFO(logger) << "Loading DEP terrain data from " << depDir;
		time_t t1 = time(NULL);
		int pointsPerDegree = 3600;  // 1 arcsec => pointsPerDegree = 60*60
		// int pointsPerDegree = 10800; // 1/3 arcsec => pointsPerDegree = 60*60*3
		depDataset = new DEPDatasetClass(depDir, pointsPerDegree, false);
		depDataset->readRegion(terrainMinLat, terrainMinLon, terrainMaxLat, terrainMaxLon);
		time_t t2 = time(NULL);
		LOGGER_DEBUG(logger) << "Elapsed Time: " << (t2-t1);
	}
	else
	{
		depDataset = nullptr;
	}

	// STRM data is always loaded as fallback
	LOGGER_INFO(logger) << "Loading SRTM terrain data from " << srtmDir;
	gdalDir = new GdalDataDir(srtmDir.c_str(), terrainMinLat, terrainMinLon, terrainMaxLat, terrainMaxLon);

	// GLOBE data is always loaded as final fallback
	LOGGER_INFO(logger) << "Loading GLOBE terrain data from " << globeDir.toStdString();
	globeModel = new WorldData(QDir(globeDir), terrainMinLat, terrainMinLon, terrainMaxLat, terrainMaxLon); 

	numLidar = (long long) 0;
	numSRTM = (long long) 0;
	numGlobal = (long long) 0;

}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: TerrainClass::~TerrainClass()                                           ****/
/******************************************************************************************/
TerrainClass::~TerrainClass()
{
	if (gdalDir) {
		delete gdalDir;
	}
	if (globeModel) {
		delete globeModel;
	}

	if (depDataset) {
		delete depDataset;
	}

	while(activeLidarRegionList.size()) {
		int deleteLidarRegionIdx = activeLidarRegionList.back();
		activeLidarRegionList.pop_back();
		delete lidarRegionList[deleteLidarRegionIdx].multibandRaster;
		lidarRegionList[deleteLidarRegionIdx].multibandRaster = (MultibandRasterClass *) NULL;
	}
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: TerrainClass::getNumLidarRegion()                                      ****/
/******************************************************************************************/
int TerrainClass::getNumLidarRegion()
{
	return((int) lidarRegionList.size());
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: TerrainClass::getLidarRegion()                                         ****/
/******************************************************************************************/
LidarRegionStruct& TerrainClass::getLidarRegion(int lidarRegionIdx)
{
	if(!lidarRegionList[lidarRegionIdx].multibandRaster) {
		std::string file = lidarRegionList[lidarRegionIdx].topPath  + "/" + lidarRegionList[lidarRegionIdx].multibandFile;
		lidarRegionList[lidarRegionIdx].multibandRaster = new MultibandRasterClass(file, lidarRegionList[lidarRegionIdx].format);
	}
	return(lidarRegionList[lidarRegionIdx]);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: TerrainClass::getTerrainHeight()                                       ****/
/******************************************************************************************/
void TerrainClass::getTerrainHeight(double longitudeDeg, double latitudeDeg, double& terrainHeight, double& bldgHeight,
		MultibandRasterClass::HeightResult& lidarHeightResult, CConst::HeightSourceEnum& heightSource) const
{
	int lidarRegionIdx = -1;

	if (    (longitudeDeg >= minLidarLongitude) 
			&& (longitudeDeg <= maxLidarLongitude) 
			&& (latitudeDeg  >= minLidarLatitude) 
			&& (latitudeDeg  <= maxLidarLatitude) ) {
		lidarRegionIdx = getLidarRegion(longitudeDeg, latitudeDeg);
	}

	if (lidarRegionIdx != -1) {
		// loadLidarRegion(lidarRegionIdx);
		lidarRegionList[lidarRegionIdx].multibandRaster->getHeight(latitudeDeg, longitudeDeg,
				terrainHeight, bldgHeight, lidarHeightResult);

		switch(lidarHeightResult) {
			case MultibandRasterClass::OUTSIDE_REGION:
				// point outside region defined by rectangle "bounds"
				// This should be impossible because lidarRegion has already been checked.
				throw std::logic_error("point outside region defined by rectangle 'bounds' for lat: "
						+ std::to_string(latitudeDeg)
						+ ", lon: " + std::to_string(longitudeDeg)
						+ " in lidarRegionIdx: " + std::to_string(lidarRegionIdx));
				break;
			case MultibandRasterClass::NO_DATA:
				// point inside bounds that has no data.  terrainHeight set to nodataBE, bldgHeight undefined
				heightSource = CConst::unknownHeightSource;
				break;
			case MultibandRasterClass::NO_BUILDING:
				// point where there is no building, terrainHeight set to valid value, bldgHeight set to nodataBldg
			case MultibandRasterClass::BUILDING:
				// point where there is a building, terrainHeight and bldgHeight valid values
				heightSource = CConst::lidarHeightSource;
				numLidar++;
				break;
		}
	} else {
		lidarHeightResult  = MultibandRasterClass::OUTSIDE_REGION;
		bldgHeight    = std::numeric_limits<double>::quiet_NaN();
		heightSource = CConst::unknownHeightSource;
	}

	if (heightSource == CConst::unknownHeightSource && depDataset) {
		float ht = depDataset->getHeight(latitudeDeg, longitudeDeg);
		if(ht == depDataset->INVALID_HEIGHT){
			// Do nothing
		} else {
			heightSource = CConst::depHeightSource;
			terrainHeight = (double) ht;
			numDEP++;
		}
	}
	if (heightSource == CConst::unknownHeightSource) {
		qint16 ht;
		gdalDir->getHeight(ht,latitudeDeg, longitudeDeg);
		// printf("longitudeDeg = %.15e\n", longitudeDeg);
		// printf("latitudeDeg = %.15e\n", latitudeDeg);
		// printf("ht = %d\n", (int) ht);
		// printf("invalidHeight = %d\n", (int) gdalDir->getInvalidHeight());
		// fflush(stdout);
		if(ht == gdalDir->getInvalidHeight()){
			// Do nothing
		} else {
			heightSource = CConst::srtmHeightSource;
			terrainHeight = (double) ht;
			numSRTM++;
		}

	}

	if (heightSource == CConst::unknownHeightSource) {
		heightSource = CConst::globalHeightSource;
		terrainHeight = (double) globeModel->valueAtLatLon(latitudeDeg, longitudeDeg);
		numGlobal++;
	}
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: TerrainClass::loadLidarRegion()                                        ****/
/******************************************************************************************/
void TerrainClass::loadLidarRegion(int lidarRegionIdx)
{
	std::string command;

	if (lidarRegionList[lidarRegionIdx].multibandRaster) {
		// region already loaded.
		return;
	}

	if (((int) activeLidarRegionList.size()) == maxLidarRegionLoad) {
		// Close Lidar region before opening new one.
		int deleteLidarRegionIdx = activeLidarRegionList.back();
		activeLidarRegionList.pop_back();
		delete lidarRegionList[deleteLidarRegionIdx].multibandRaster;
		lidarRegionList[deleteLidarRegionIdx].multibandRaster = (MultibandRasterClass *) NULL;

		LOGGER_WARN(logger) << "REMOVING LIDAR REGION: " << deleteLidarRegionIdx;
	}

	LOGGER_DEBUG(logger) << "LOADING LIDAR REGION: " << lidarRegionIdx;
	std::string file = lidarRegionList[lidarRegionIdx].topPath  + "/" + lidarRegionList[lidarRegionIdx].multibandFile;
	lidarRegionList[lidarRegionIdx].multibandRaster = new MultibandRasterClass(file, lidarRegionList[lidarRegionIdx].format);

	activeLidarRegionList.insert(activeLidarRegionList.begin(), lidarRegionIdx);

	LOGGER_DEBUG(logger) << "NUM LIDAR REGIONS LOADED = " << activeLidarRegionList.size() << "    MAX = " << maxLidarRegionLoad;
}
/******************************************************************************************/

/******************************************************************************************/
/**** TerrainClass::getLidarRegion()                                                   ****/
/******************************************************************************************/
int TerrainClass::getLidarRegion(double lonDeg, double latDeg) const
{

	int lidarRegionIdx, retval;
	bool found = false;

	for(lidarRegionIdx=0; (lidarRegionIdx<((int) lidarRegionList.size()))&&(!found); ++lidarRegionIdx) {
		const LidarRegionStruct& lidarRegion = lidarRegionList[lidarRegionIdx];

		if (lidarRegion.multibandRaster) {
			if (lidarRegion.multibandRaster->contains(lonDeg, latDeg)) {
				found = true;
				retval = lidarRegionIdx;
			}
		}
		//else {
		//    if (    (lonDeg >= lidarRegion.minLonDeg) && (lonDeg <= lidarRegion.maxLonDeg)
		//         && (latDeg >= lidarRegion.minLatDeg) && (latDeg <= lidarRegion.maxLatDeg) ) {
		//        found = true;
		//        retval = lidarRegionIdx;
		//    }
		//}
	}

	if (!found) {
		retval = -1;
	}

	return(retval);
}
/******************************************************************************************/

/******************************************************************************************/
/**** TerrainClass::readLidarData()                                                    ****/
/******************************************************************************************/
void TerrainClass::readLidarData(double terrainMinLat, double terrainMinLon, double terrainMaxLat, double terrainMaxLon)
{
	int lidarRegionIdx;
	std::ostringstream errStr;

	int numRegionWithOverlap = 0;
	for(lidarRegionIdx=0; (lidarRegionIdx<((int) lidarRegionList.size())); ++lidarRegionIdx) {
		const LidarRegionStruct& lidarRegion = lidarRegionList[lidarRegionIdx];

		if (!(    (terrainMaxLon < lidarRegion.minLonDeg) || (terrainMinLon > lidarRegion.maxLonDeg)
					|| (terrainMaxLat < lidarRegion.minLatDeg) || (terrainMinLat > lidarRegion.maxLatDeg) )) {
			numRegionWithOverlap++;
		}
	}

	if (numRegionWithOverlap > maxLidarRegionLoad) {
		errStr << "ERROR: Terrain region specified requires " << numRegionWithOverlap
			<< " LIDAR tiles which exceeds maxLidarRegionLoad = " << maxLidarRegionLoad << std::endl;
		throw std::runtime_error(errStr.str());
	}

	for(lidarRegionIdx=0; (lidarRegionIdx<((int) lidarRegionList.size())); ++lidarRegionIdx) {
		const LidarRegionStruct& lidarRegion = lidarRegionList[lidarRegionIdx];

		if (!(    (terrainMaxLon < lidarRegion.minLonDeg) || (terrainMinLon > lidarRegion.maxLonDeg)
					|| (terrainMaxLat < lidarRegion.minLatDeg) || (terrainMinLat > lidarRegion.maxLatDeg) )) {
			loadLidarRegion(lidarRegionIdx);
		}
	}

#if 0
	if (numRegionWithOverlap == 0)
		throw std::runtime_error("Building data was requested, but none was found within the analysis area.");
#endif
	LOGGER_INFO(logger) << numRegionWithOverlap << " LiDAR tiles loaded";

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** TerrainClass::readLidarInfo()                                                    ****/
/******************************************************************************************/
void TerrainClass::readLidarInfo(QDir lidarDir)
{

	QDir lDir(lidarDir);
	auto lidarCityNames = lDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	for (int i = 0; i < lidarCityNames.size(); i++)
	{
		auto file = lDir.filePath(lidarCityNames[i]);
		lidarCityNames[i] = file;
	}


	int cityIdx;

	for(cityIdx=0; cityIdx<lidarCityNames.size(); ++cityIdx) {
		std::string topPath  = lidarCityNames[cityIdx].toStdString();
		std::string infoFile = lidarCityNames[cityIdx].toStdString() + "_info.csv";
		std::string cityName;

		std::size_t posn = topPath.find_last_of("/\\");
		if (posn == std::string::npos) {
			cityName = topPath;
		} else {
			cityName = topPath.substr(posn+1);
		}

		int linenum, fIdx;
		std::string line, strval;
		char *chptr;
		FILE *fp = (FILE *) NULL;
		std::string str;
		std::string reasonIgnored;
		std::ostringstream errStr;

		int multibandFieldIdx           = -1;
		int minLonFieldIdx              = -1;
		int maxLonFieldIdx              = -1;
		int minLatFieldIdx              = -1;
		int maxLatFieldIdx              = -1;
		int formatFieldIdx              = -1;

		std::vector<int *> fieldIdxList;            std::vector<std::string> fieldLabelList;
		fieldIdxList.push_back(&multibandFieldIdx); fieldLabelList.push_back("FILE");
		fieldIdxList.push_back(&minLonFieldIdx);    fieldLabelList.push_back("MIN_LON_DEG");
		fieldIdxList.push_back(&maxLonFieldIdx);    fieldLabelList.push_back("MAX_LON_DEG");
		fieldIdxList.push_back(&minLatFieldIdx);    fieldLabelList.push_back("MIN_LAT_DEG");
		fieldIdxList.push_back(&maxLatFieldIdx);    fieldLabelList.push_back("MAX_LAT_DEG");
		fieldIdxList.push_back(&formatFieldIdx);    fieldLabelList.push_back("FORMAT");

		int fieldIdx;

		if (infoFile.empty()) {
			str = std::string("ERROR: Lidar Info file specified\n");
			throw std::runtime_error(str);
		}

		if ( !(fp = fopen(infoFile.c_str(), "rb")) ) {
			str = std::string("ERROR: Unable to open Lidar Info file \"") + infoFile + std::string("\"\n");
			throw std::runtime_error(str);
		}

		enum LineTypeEnum {
			labelLineType,
			dataLineType,
			ignoreLineType,
			unknownLineType
		};

		LineTypeEnum lineType;

		linenum = 0;
		bool foundLabelLine = false;
		while (fgetline(fp, line, false)) {
			linenum++;
			std::vector<std::string> fieldList = splitCSV(line);

			lineType = unknownLineType;
			/**************************************************************************/
			/**** Determine line type                                              ****/
			/**************************************************************************/
			if (fieldList.size() == 0) {
				lineType = ignoreLineType;
			} else {
				fIdx = fieldList[0].find_first_not_of(' ');
				if (fIdx == (int) std::string::npos) {
					if (fieldList.size() == 1) {
						lineType = ignoreLineType;
					}
				} else {
					if (fieldList[0].at(fIdx) == '#') {
						lineType = ignoreLineType;
					}
				}
			}

			if ((lineType == unknownLineType)&&(!foundLabelLine)) {
				lineType = labelLineType;
				foundLabelLine = 1;
			}
			if ((lineType == unknownLineType)&&(foundLabelLine)) {
				lineType = dataLineType;
			}
			/**************************************************************************/

			/**************************************************************************/
			/**** Process Line                                                     ****/
			/**************************************************************************/
			bool found;
			std::string field;
			LidarRegionStruct lidarRegion;
			lidarRegion.topPath = topPath;
			lidarRegion.cityName = cityName;
			lidarRegion.multibandRaster = (MultibandRasterClass *) NULL;
			switch(lineType) {
				case   labelLineType:
					for(fieldIdx=0; fieldIdx<(int) fieldList.size(); fieldIdx++) {
						field = fieldList.at(fieldIdx);

						// std::cout << "FIELD: \"" << field << "\"" << std::endl;

						found = false;
						for(fIdx=0; (fIdx < (int) fieldLabelList.size())&&(!found); fIdx++) {
							if (field == fieldLabelList.at(fIdx)) {
								*fieldIdxList.at(fIdx) = fieldIdx;
								found = true;
							}
						}
					}

					for(fIdx=0; fIdx < (int) fieldIdxList.size(); fIdx++) {
						if (*fieldIdxList.at(fIdx) == -1) {
							if (fieldLabelList.at(fIdx) == "FORMAT") {
							} else {
								errStr << "ERROR: Invalid Lidar Info file \"" << infoFile << "\" label line missing \"" << fieldLabelList.at(fIdx) << "\"" << std::endl;
								throw std::runtime_error(errStr.str());
							}
						}
					}

					break;
				case    dataLineType:
					lidarRegion.multibandFile = fieldList.at(multibandFieldIdx);

					strval = fieldList.at(minLonFieldIdx);
					lidarRegion.minLonDeg = std::strtod(strval.c_str(), &chptr);

					strval = fieldList.at(maxLonFieldIdx);
					lidarRegion.maxLonDeg = std::strtod(strval.c_str(), &chptr);

					strval = fieldList.at(minLatFieldIdx);
					lidarRegion.minLatDeg = std::strtod(strval.c_str(), &chptr);

					strval = fieldList.at(maxLatFieldIdx);
					lidarRegion.maxLatDeg = std::strtod(strval.c_str(), &chptr);

					if (formatFieldIdx == -1) {
						strval = "from_vector";
					} else {
						strval = fieldList.at(formatFieldIdx);
					}

					if (strval == "from_vector") {
						lidarRegion.format = CConst::fromVectorLidarFormat;
					} else if (strval == "from_raster") {
						lidarRegion.format = CConst::fromRasterLidarFormat;
					} else {
						errStr << "lidarRegion.format not a valid value. Got " << lidarRegion.format << std::endl;
						throw std::logic_error(errStr.str());
					}

					lidarRegionList.push_back(lidarRegion);

#if 0
					LOGGER_DEBUG(logger) << "TOP_PATH = " << lidarRegion.topPath
						<< "    MULTIBAND_FILE = " << lidarRegion.multibandFile
						<< "    MIN_LON (deg) = " << lidarRegion.minLonDeg
						<< "    MAX_LON (deg) = " << lidarRegion.maxLonDeg
						<< "    MIN_LAT (deg) = " << lidarRegion.minLatDeg
						<< "    MAX_LAT (deg) = " << lidarRegion.maxLatDeg;
#endif
					break;
				case  ignoreLineType:
				case unknownLineType:
					// do nothing
					break;
				default:
					throw std::runtime_error("Impossible case statement reached in terrain.cpp");
					break;
			}
		}

		if (fp) { fclose(fp); }
	}

	return;
}
/******************************************************************************************/

std::vector<QRectF> TerrainClass::getBounds() const
{
	std::vector<QRectF> bounds = std::vector<QRectF>();
	for (LidarRegionStruct m : this->lidarRegionList)
	{
		if (!m.multibandRaster) continue;
		QPointF topLeft(m.maxLonDeg,m.minLatDeg);
		QPointF bottomRight(m.minLonDeg,m.maxLatDeg);
		QRectF b(topLeft, bottomRight);
		bounds.push_back(b);
	}
	return bounds;
}

/**
 * Register a label with a height source value
 */
void TerrainClass::setSourceName(const CConst::HeightSourceEnum& sourceVal, const std::string& sourceName)
{
	sourceNames[sourceVal] = sourceName;
}

/**
 * Convert a HeightSourceEnum to correct string representation for the Terrain model
 */
const std::string& TerrainClass::getSourceName(const CConst::HeightSourceEnum& sourceVal) const
{
	return sourceNames.at(sourceVal);
}


/******************************************************************************************/
/**** TerrainClass::printStats()                                                       ****/
/******************************************************************************************/
void TerrainClass::printStats()
{
	long long totalNumTerrain = numLidar + numSRTM + numDEP + numGlobal;

	LOGGER_INFO(logger) << "TOTAL_NUM_TERRAIN = " << totalNumTerrain;
	LOGGER_INFO(logger) << "NUM_LIDAR = " << numLidar << "  ("
		<< (double) (totalNumTerrain ? numLidar*100.0/totalNumTerrain : 0.0)
		<< " %)";
	LOGGER_INFO(logger) << "NUM_DEP = " << numDEP << "  ("
		<< (double) (totalNumTerrain ? numDEP*100.0/totalNumTerrain : 0.0)
		<< " %)";
	LOGGER_INFO(logger) << "NUM_SRTM = " << numSRTM << "  ("
		<< (double) (totalNumTerrain ? numSRTM*100.0/totalNumTerrain : 0.0)
		<< " %)";
	LOGGER_INFO(logger) << "NUM_GLOBAL = " << numGlobal << "  ("
		<< (double) (totalNumTerrain ? numGlobal*100.0/totalNumTerrain : 0.0)
		<< " %)";
}
/******************************************************************************************/

