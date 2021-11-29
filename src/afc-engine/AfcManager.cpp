// AfcManager.cpp -- Manages I/O and top-level operations for the AFC Engine
// Authors: Vlad Martin, Andrew Winter, Michael Mandell, Sam Smucny
#include "AfcManager.h"
#include "RlanRegion.h"

extern double qerfi(double q);

QJsonArray jsonChannelData(const std::vector<ChannelStruct> &channelList);
QJsonObject jsonSpectrumData(const std::vector<ChannelStruct> &channelList, const QJsonObject &deviceDesc, const double &startFreq);



std::string padStringFront(const std::string& s, const char& padder, const int& amount)
{
	std::string r = s;
	while ((int) (r.length()) < amount)
	{
		r = padder + r;
	}
	return r;
}

/**
 * Generate a UTC ISO8601-formatted timestamp
 * and return as QString
 */
QString ISO8601TimeUTC(const int &dayStep = 0)
{
	time_t rawtime;
	struct tm *ptm;

	std::time(&rawtime);
	rawtime += dayStep*24*3600;

	ptm = gmtime(&rawtime);

	// "yyyy-mm-ddThh:mm:ssZ"
	std::string result = 
		padStringFront(std::to_string(ptm->tm_year+1900), '0', 4) + "-" +
		padStringFront(std::to_string(1 + ptm->tm_mon), '0', 2) + "-" +
		padStringFront(std::to_string(ptm->tm_mday), '0', 2) + "T" +
		padStringFront(std::to_string(ptm->tm_hour), '0', 2) + ":" +
		padStringFront(std::to_string(ptm->tm_min), '0', 2) + ":" +
		padStringFront(std::to_string(ptm->tm_sec), '0', 2) + "Z";

	return QString::fromStdString(result);
}

namespace
{

	// Logger for all instances of class
	LOGGER_DEFINE_GLOBAL(logger, "AfcManager")
	
		const double fixedEpsDielect = 15;
	const double fixedSgmConductivity = 0.005;
	const double fixedEsNoSurfRef = 301;
	const int fixedRadioClimate = 6;
	const int fixedPolarization = 1;
	const double fixedConfidence = 0.5;
	const double fixedRelevance = 0.5;
	/**
	 * Encapsulates a CSV writer to a file gzip'd
	 * All fields are NULL if filename parameter in constructor is not a valid path.
	 */
	class GzipCsvWriter
	{
		public:
			std::unique_ptr<CsvWriter> csv_writer;
			std::unique_ptr<QFile> file_writer;
			std::unique_ptr<GzipStream> gzip_writer;

			GzipCsvWriter(std::string &filename);
	};

	GzipCsvWriter::GzipCsvWriter(std::string &filename)
	{
		if (!filename.empty())
		{
			file_writer = FileHelpers::open(QString::fromStdString(filename), QIODevice::WriteOnly);
			gzip_writer.reset(new GzipStream(file_writer.get()));
			if (!gzip_writer->open(QIODevice::WriteOnly))
			{
				throw std::runtime_error("Gzip failed to open.");
			}
			csv_writer.reset(new CsvWriter(*gzip_writer));
		}
	}

	/**
	 * Encapsulates a XML writer to a file 
	 * All fields are NULL if filename parameter in constructor is not a valid path.
	 */
	class ZXmlWriter
	{
		public:
			std::unique_ptr<QXmlStreamWriter> xml_writer;
			std::unique_ptr<QIODevice> _file;
			std::unique_ptr<ZipWriter> zip_writer;

			ZXmlWriter(const std::string &filename);
			~ZXmlWriter();
	};

	ZXmlWriter::ZXmlWriter(const std::string &filename)
	{
		if (!filename.empty())
		{
			zip_writer.reset(new ZipWriter(QString::fromStdString(filename)));
			_file = zip_writer->openFile("doc.kml");
			xml_writer.reset(new QXmlStreamWriter(_file.get()));
		}
	}

	ZXmlWriter::~ZXmlWriter()
	{
		xml_writer.reset();
		_file.reset();
		zip_writer.reset();	
	}
}; // end namespace

AfcManager::AfcManager()
{
	_terrainDataModel = (TerrainClass *) NULL;

	_ulsList = new ListClass<ULSClass *>(0);

	_rasList = new ListClass<RASClass *>(0);

	_popGrid = (PopGridClass *) NULL;

        _rlanRegion = (RlanRegionClass *) NULL;

	_heatmapIToNDB = (double **) NULL;
	_heatmapIsIndoor = (bool **) NULL;
	_heatmapNumPtsLon = 0;
	_heatmapNumPtsLat = 0;
}

/******************************************************************************************/
/**** FUNCTION: AfcManager::~AfcManager()                                              ****/
/******************************************************************************************/
AfcManager::~AfcManager()
{
	clearData();
	delete _ulsList;
	delete _rasList;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::initializeDatabases()                                      ****/
/******************************************************************************************/
void AfcManager::initializeDatabases()
{
	// Following lines are finding the minimum and maximum longitudes and latitudes with the 150km rlan range

	double minLon, maxLon, minLat, maxLat;
	double minLonBldg, maxLonBldg, minLatBldg, maxLatBldg;

        int chanIdx;
        double maxBandwidth = 0.0;
        for (chanIdx = 0; chanIdx < (int) _channelList.size(); ++chanIdx) {
            ChannelStruct *channel = &(_channelList[chanIdx]);
            double chanStartFreq = channel->startFreqMHz * 1.0e6;
            double chanStopFreq = channel->stopFreqMHz * 1.0e6;
            double bandwidth = chanStopFreq - chanStartFreq;
            if (bandwidth > maxBandwidth) { maxBandwidth = bandwidth; }
        }

        double ulsMinFreq = _wlanMinFreq - (_aciFlag ? maxBandwidth : 0.0);
        double ulsMaxFreq = _wlanMaxFreq + (_aciFlag ? maxBandwidth : 0.0);

	/**************************************************************************************/
	/* Set Path Loss Model Parameters                                                     */
	/**************************************************************************************/
	if (_pathLossModelStr == "ITM_BLDG") {
		// Building Polygons

		// _gdalDataModel->printDebugInfo();
		// exit(1);
		_closeInDist = 0.0;              // Radius in which close in path loss model is used
            if (_terrainDataModel->getNumLidarRegion() == 0) {
                throw std::runtime_error(ErrStream() << "Path loss model set to ITM_BLDG, but no building data found within the analysis region.");
            }
	} else if (_pathLossModelStr == "COALITION_OPT_6") {
		_closeInDist = 1.0e3;              // Radius in which close in path loss model is used
	} else if (_pathLossModelStr == "FCC_6GHZ_REPORT_AND_ORDER") {
		_closeInDist = 1.0e3;              // Radius in which close in path loss model is used
	} else if (_pathLossModelStr == "FSPL") {
		_closeInDist = 0.0;              // Radius in which close in path loss model is used
	} else {
		throw std::runtime_error(ErrStream() << "ERROR: Path Loss Model set to invalid value \"" + _pathLossModelStr + "\"");
	}

	// Set pathLossModel
	int validFlag;
	_pathLossModel = (CConst::PathLossModelEnum)CConst::strPathLossModelList->str_to_type(_pathLossModelStr, validFlag, 0);
	ULSClass::pathLossModel = _pathLossModel;

	if (!validFlag)
	{
		throw std::runtime_error(ErrStream() << "ERROR: Path Loss Model set to invalid value \"" + _pathLossModelStr + "\"");
	}

	/**************************************************************************************/

	if (_analysisType == "PointAnalysis" || _analysisType == "APAnalysis" || _analysisType == "AP-AFC") {
		double centerLat, centerLon;

                /**************************************************************************************/
                /* Create Rlan Uncertainty Region                                                     */
                /**************************************************************************************/
                switch(_rlanUncertaintyRegionType) {
                    case ELLIPSE:
                        _rlanRegion = (RlanRegionClass *) new EllipseRlanRegionClass(_rlanLLA, _rlanUncerts_m, _rlanOrientation_deg);
                        break;
                    case LINEAR_POLY:
                        _rlanRegion = (RlanRegionClass *) new PolygonRlanRegionClass(_rlanLLA, _rlanUncerts_m, _rlanLinearPolygon, LINEAR_POLY);
                        break;
                    case RADIAL_POLY:
                        _rlanRegion = (RlanRegionClass *) new PolygonRlanRegionClass(_rlanLLA, _rlanUncerts_m, _rlanRadialPolygon, RADIAL_POLY);
                        break;
                    default:
		        throw std::runtime_error(ErrStream() << "ERROR: INVALID _rlanUncertaintyRegionType = " << _rlanUncertaintyRegionType);
                        break;
                }
                /**************************************************************************************/

                double rlanRegionSize = _rlanRegion->getMaxDist();
                centerLon = _rlanRegion->getCenterLongitude();
                centerLat = _rlanRegion->getCenterLatitude();

		minLat = centerLat - ((_maxRadius + rlanRegionSize) / CConst::earthRadius) * 180.0 / M_PI;
		maxLat = centerLat + ((_maxRadius + rlanRegionSize) / CConst::earthRadius) * 180.0 / M_PI;

		double maxAbsLat = std::max(fabs(minLat), fabs(maxLat));
		minLon = centerLon - ((_maxRadius + rlanRegionSize) / (CConst::earthRadius * cos(maxAbsLat * M_PI / 180.0))) * 180.0 / M_PI;
		maxLon = centerLon + ((_maxRadius + rlanRegionSize) / (CConst::earthRadius * cos(maxAbsLat * M_PI / 180.0))) * 180.0 / M_PI;

	        if (_pathLossModel == CConst::FCC6GHzReportAndOrderPathLossModel) {
		    minLatBldg = centerLat - ((_closeInDist + rlanRegionSize) / CConst::earthRadius) * 180.0 / M_PI;
		    maxLatBldg = centerLat + ((_closeInDist + rlanRegionSize) / CConst::earthRadius) * 180.0 / M_PI;

		    double maxAbsLatBldg = std::max(fabs(minLatBldg), fabs(maxLatBldg));
		    minLonBldg = centerLon - ((_closeInDist + rlanRegionSize) / (CConst::earthRadius * cos(maxAbsLatBldg * M_PI / 180.0))) * 180.0 / M_PI;
		    maxLonBldg = centerLon + ((_closeInDist + rlanRegionSize) / (CConst::earthRadius * cos(maxAbsLatBldg * M_PI / 180.0))) * 180.0 / M_PI;
                } else {
                    minLatBldg = minLat;
                    maxLatBldg = maxLat;
                    minLonBldg = minLon;
                    maxLonBldg = maxLon;
                }
	} else if (_analysisType == "ExclusionZoneAnalysis") {
		readULSData(_ulsDataFile, (PopGridClass *) NULL, 0, ulsMinFreq, ulsMaxFreq, _removeMobile, CConst::FixedSimulation, 0.0, 0.0, 0.0, 0.0);
		readRASData(_rasDataFile);
		if (_ulsList->getSize() == 0) {
		} else if (_ulsList->getSize() > 1) {
		}
		double centerLat = (*_ulsList)[0]->getRxLatitudeDeg();
		double centerLon = (*_ulsList)[0]->getRxLongitudeDeg();

		minLat = centerLat - ((_maxRadius) / CConst::earthRadius) * 180.0 / M_PI;
		maxLat = centerLat + ((_maxRadius) / CConst::earthRadius) * 180.0 / M_PI;

		double maxAbsLat = std::max(fabs(minLat), fabs(maxLat));
		minLon = centerLon - ((_maxRadius) / (CConst::earthRadius * cos(maxAbsLat * M_PI / 180.0))) * 180.0 / M_PI;
		maxLon = centerLon + ((_maxRadius) / (CConst::earthRadius * cos(maxAbsLat * M_PI / 180.0))) * 180.0 / M_PI;

	        if (_pathLossModel == CConst::FCC6GHzReportAndOrderPathLossModel) {
		    minLatBldg = centerLat - ((_closeInDist) / CConst::earthRadius) * 180.0 / M_PI;
		    maxLatBldg = centerLat + ((_closeInDist) / CConst::earthRadius) * 180.0 / M_PI;

		    double maxAbsLatBldg = std::max(fabs(minLatBldg), fabs(maxLatBldg));
		    minLonBldg = centerLon - ((_closeInDist) / (CConst::earthRadius * cos(maxAbsLatBldg * M_PI / 180.0))) * 180.0 / M_PI;
		    maxLonBldg = centerLon + ((_closeInDist) / (CConst::earthRadius * cos(maxAbsLatBldg * M_PI / 180.0))) * 180.0 / M_PI;
                } else {
                    minLatBldg = minLat;
                    maxLatBldg = maxLat;
                    minLonBldg = minLon;
                    maxLonBldg = maxLon;
                }
	} else if (_analysisType == "HeatmapAnalysis") {
		minLat = _heatmapMinLat - (_maxRadius / CConst::earthRadius) * 180.0 / M_PI;
		maxLat = _heatmapMaxLat + (_maxRadius / CConst::earthRadius) * 180.0 / M_PI;

		double maxAbsLat = std::max(fabs(minLat), fabs(maxLat));
		minLon = _heatmapMinLon - (_maxRadius / (CConst::earthRadius * cos(maxAbsLat * M_PI / 180.0))) * 180.0 / M_PI;
		maxLon = _heatmapMaxLon + (_maxRadius / (CConst::earthRadius * cos(maxAbsLat * M_PI / 180.0))) * 180.0 / M_PI;

	        if (_pathLossModel == CConst::FCC6GHzReportAndOrderPathLossModel) {
		    minLatBldg = _heatmapMinLat - (_closeInDist / CConst::earthRadius) * 180.0 / M_PI;
		    maxLatBldg = _heatmapMaxLat + (_closeInDist / CConst::earthRadius) * 180.0 / M_PI;

		    double maxAbsLatBldg = std::max(fabs(minLatBldg), fabs(maxLatBldg));
		    minLonBldg = _heatmapMinLon - (_closeInDist / (CConst::earthRadius * cos(maxAbsLatBldg * M_PI / 180.0))) * 180.0 / M_PI;
		    maxLonBldg = _heatmapMaxLon + (_closeInDist / (CConst::earthRadius * cos(maxAbsLatBldg * M_PI / 180.0))) * 180.0 / M_PI;
                } else {
                    minLatBldg = minLat;
                    maxLatBldg = maxLat;
                    minLonBldg = minLon;
                    maxLonBldg = maxLon;
                }
	}

	/**************************************************************************************/
	/* Setup Terrain data                                                                    */
	/**************************************************************************************/
	// The following counters are used to understand the percentage of holes in terrain data that exist (mostly for debugging)
	UlsMeasurementAnalysis::numInvalidSRTM = 0; // Define counters for invalid heights returned by SRTM
	UlsMeasurementAnalysis::numSRTM = 0;        // Define counters for all SRTM calls

	_maxLidarRegionLoadVal = 70;

	if (_useBDesignFlag) {
		_lidarDir = SearchPaths::forReading("data", "fbrat/rat_transfer/Multiband-BDesign3D", true);
	}
	else if (_useLiDAR)
	{
		_lidarDir = SearchPaths::forReading("data", "fbrat/rat_transfer/proc_lidar_2019", true);
	}
	else
	{
		_lidarDir = "";
	}

	if (_use3DEP)
	{
		// 3DEP directory is currently not present so this will fail if called
		// LOGGER_WARN(logger) << "3DEP loading request is being ignored";
		_depDir = SearchPaths::forReading("data", "fbrat/rat_transfer/3dep/1_arcsec", true).toStdString();
	}
	else
	{
		_depDir = "";
	}

	_terrainDataModel = new TerrainClass(_lidarDir, _srtmDir.c_str(), _depDir.c_str(), _globeDir,
                                             minLat, minLon, maxLat, maxLon,
                                             minLatBldg, minLonBldg, maxLatBldg, maxLonBldg,
                                              _maxLidarRegionLoadVal);
	
	_terrainDataModel->setSourceName(CConst::HeightSourceEnum::unknownHeightSource, "UNKNOWN");
	_terrainDataModel->setSourceName(CConst::HeightSourceEnum::globalHeightSource, "GLOBE");
	_terrainDataModel->setSourceName(CConst::HeightSourceEnum::depHeightSource, "3DEP 1 arcsec");
	_terrainDataModel->setSourceName(CConst::HeightSourceEnum::srtmHeightSource, "SRTM");
	if (_useBDesignFlag)
	{
		_terrainDataModel->setSourceName(CConst::HeightSourceEnum::lidarHeightSource, "B3D-3DEP");
	}
	else if (_useLiDAR)
	{
		_terrainDataModel->setSourceName(CConst::HeightSourceEnum::lidarHeightSource, "LiDAR");
	}
	
	// Check to make sure that the inputs are valid (e.g. such that antenna height does not go underground)
	try {
		isValid();
	}
	catch (std::exception &err) {
		throw std::runtime_error(
				ErrStream() << "AfcManager::initializeDatabases(): User provided invalid input parameters: " << err.what()
				);
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Read Antenna Pattern Data if provided // This will be used in phase 2              */
	/**************************************************************************************/
	if (!_ulsAntennaPatternFile.empty())
	{
		_ulsAntennaList = AntennaClass::readMultipleBoresightAntennas(_ulsAntennaPatternFile);
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Read Population data                                                               */
	/**************************************************************************************/
	readPopulationGrid(); // Reads population density in grid of lat/lon, multiply by area to get population of specified regions
	/**************************************************************************************/

	/**************************************************************************************/
	/* Read ULS data                                                                      */
	/**************************************************************************************/
	//_ulsDataFile = "/home/ssmucny/ULS_23Jan2019_perlink_fixedwithbas.sqlite3";

	if (_analysisType == "PointAnalysis" || _analysisType == "APAnalysis" || _analysisType == "HeatmapAnalysis" || _analysisType == "AP-AFC") {
		readULSData(_ulsDataFile, _popGrid, 0, ulsMinFreq, ulsMaxFreq, _removeMobile, CConst::FixedSimulation, minLat, maxLat, minLon, maxLon);
		readRASData(_rasDataFile);
	} else if (_analysisType == "ExclusionZoneAnalysis") {
		fixFSTerrain();
	}
	/**************************************************************************************/
}
/**************************************************************************************/


/******************************************************************************************/
/**** FUNCTION: AfcManager::clearData                                                  ****/
/******************************************************************************************/
void AfcManager::clearData()
{
	clearULSList();

	clearRASList();

	for (int antIdx = 0; antIdx < (int)_ulsAntennaList.size(); antIdx++)
	{
		delete _ulsAntennaList[antIdx];
	}

	if (_popGrid) {
		delete _popGrid;
		_popGrid = (PopGridClass *) NULL;
	}

	if (_heatmapIsIndoor) {
		int lonIdx;
		for (lonIdx = 0; lonIdx < _heatmapNumPtsLon; ++lonIdx) {
			free(_heatmapIsIndoor[lonIdx]);
		}
		free(_heatmapIsIndoor);
	}

	if (_heatmapIToNDB) {
		int lonIdx;
		for (lonIdx = 0; lonIdx < _heatmapNumPtsLon; ++lonIdx) {
			free(_heatmapIToNDB[lonIdx]);
		}
		free(_heatmapIToNDB);

		_heatmapIToNDB = (double **) NULL;
		_heatmapNumPtsLon = 0;
		_heatmapNumPtsLat = 0;
	}
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::clearULSList                                               ****/
/******************************************************************************************/
void AfcManager::clearULSList()
{
	int ulsIdx;
	ULSClass *uls;

	for (ulsIdx = 0; ulsIdx <= _ulsList->getSize() - 1; ulsIdx++)
	{
		uls = (*_ulsList)[ulsIdx];
		delete uls;
	}
	_ulsList->reset();
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::clearRASList                                               ****/
/******************************************************************************************/
void AfcManager::clearRASList()
{
	int rasIdx;
	RASClass *ras;

	for (rasIdx = 0; rasIdx <= _rasList->getSize() - 1; rasIdx++)
	{
		ras = (*_rasList)[rasIdx];
		delete ras;
	}
	_rasList->reset();
}
/******************************************************************************************/

// Start assigning the values from json object into the vars in struct
void AfcManager::importGUIjson(const std::string &inputJSONpath)
{
	// Read input parameters from GUI in JSON file
	QJsonDocument jsonDoc;
	QFile inFile;
	// Use SearchPaths::forReading("data", ..., true) to ensure that the input file exists before reading it in
	inFile.setFileName(inputJSONpath.c_str());
	if (inFile.open(QFile::ReadOnly | QFile::Text))
	{
		jsonDoc = QJsonDocument::fromJson(inFile.readAll());
		inFile.close();
	}
	else
	{
		throw std::runtime_error("AfcManager::importGUIjson(): Failed to open JSON file specifying user's input parameters.");
	}

	// Print entirety of imported JSON file to debug log
	LOGGER_DEBUG(logger) << "Contents of imported JSON file: " << std::endl << 
		jsonDoc.toJson().toStdString() << std::endl;

	// Read JSON from file
	QJsonObject jsonObj = jsonDoc.object();


	// Properly set _aciFlag from GUI.  When set to true, ACI is considered in interference calculations, when set to false, there is no ACI.
	_aciFlag = true;

	if (_analysisType == "AP-AFC")
	{

		_requestId = jsonObj["requestId"].toString();
		_deviceDesc = jsonObj["deviceDescriptor"].toObject();
		_serialNumber = _deviceDesc["serialNumber"].toString();

		QJsonObject location = jsonObj["location"].toObject();
		_rlanHeightType = "AGL";
		if (location["indoorDeployment"].toInt() ==1){
			_rlanType =  RLANType::RLAN_INDOOR;
		}
		else {
			_rlanType = RLANType::RLAN_OUTDOOR;
		}

		if (location.contains("ellipse") && !jsonObj["ellipse"].isUndefined())
		{
			_rlanUncertaintyRegionType = RLANBoundary::ELLIPSE;
			QJsonObject ellipse = location["ellipse"].toObject();
			QJsonObject center = ellipse["center"].toObject();
			_rlanLLA = std::make_tuple(center["latitude"].toDouble(),
				center["longitude"].toDouble(),
				location["height"].toDouble());
			_rlanUncerts_m = std::make_tuple(ellipse["minorAxis"].toDouble(),
				ellipse["majorAxis"].toDouble(),
				location["verticalUncertainty"].toDouble());
			_rlanOrientation_deg = ellipse["orientation"].toDouble();
		}
		else if (location.contains("linearPolygon") && !jsonObj["linearPolygon"].isUndefined())
		{
			_rlanUncertaintyRegionType = RLANBoundary::LINEAR_POLY;
			QJsonArray boundary = location["linearPolygon"].toObject()["outerBoundary"].toArray();
			for (QJsonValue vertexVal : boundary)
			{
				QJsonObject vertex = vertexVal.toObject();
				_rlanLinearPolygon.push_back(std::make_pair(vertex["latitude"].toDouble(), vertex["longitude"].toDouble()));
			}
                        double sumX = 0.0;
                        double sumY = 0.0;
                        double sumA = 0.0;
                        int i;
                        for(i=0; i< (int) _rlanLinearPolygon.size(); i++) {
                            double xval0 = _rlanLinearPolygon[i].second;
                            double yval0 = _rlanLinearPolygon[i].first;
                            double xval1 = _rlanLinearPolygon[(i+1)%_rlanLinearPolygon.size()].second;
                            double yval1 = _rlanLinearPolygon[(i+1)%_rlanLinearPolygon.size()].first;
                            sumX += (xval0 + xval1)*(xval0*yval1-xval1*yval0);
                            sumY += (yval0 + yval1)*(xval0*yval1-xval1*yval0);
                            sumA += (xval0*yval1-xval1*yval0);
                        }
                        double centerLongitude = sumX / (3*sumA);
                        double centerLatitude  = sumY / (3*sumA);

			_rlanLLA = std::make_tuple(centerLatitude,
				centerLongitude,
				location["height"].toDouble());
			_rlanUncerts_m = std::make_tuple(std::numeric_limits<double>::quiet_NaN(),
				std::numeric_limits<double>::quiet_NaN(),
				location["verticalUncertainty"].toDouble());
		}
		else if (location.contains("radialPolygon") && !jsonObj["radialPolygon"].isUndefined())
		{
			_rlanUncertaintyRegionType = RLANBoundary::RADIAL_POLY;
			QJsonArray boundary = location["radialPolygon"].toObject()["outerBoundary"].toArray();
			QJsonObject center = location["radialPolygon"].toObject()["center"].toObject();
			for (QJsonValue vectorVal : boundary)
			{
				QJsonObject vector = vectorVal.toObject();
				_rlanRadialPolygon.push_back(std::make_pair(vector["angle"].toDouble(), vector["length"].toDouble()));
			}
			_rlanLLA = std::make_tuple(center["latitude"].toDouble(),
				center["longitude"].toDouble(),
				location["height"].toDouble());
			_rlanUncerts_m = std::make_tuple(std::numeric_limits<double>::quiet_NaN(),
				std::numeric_limits<double>::quiet_NaN(),
				location["verticalUncertainty"].toDouble());
		}
		else
		{
			throw std::runtime_error("Unsupported uncertainty region. Only 'ellipse', 'linearPolygon', and 'radialPolygon' are supported.");
		}

		if (jsonObj.contains("minDesiredPower") && !jsonObj["minDesiredPower"].isUndefined())
			_minEIRP_dBm = jsonObj["minDesiredPower"].toDouble();

		bool validRequestType = false;
		if (jsonObj.contains("inquiredChannels") && !jsonObj["inquiredChannels"].isUndefined())
		{
			for (const QJsonValue& channelsVal : jsonObj["inquiredChannels"].toArray())
			{
				QJsonObject channels = channelsVal.toObject();
				auto chanClass = std::make_pair<int, std::vector<int>>(channels["globalOperatingClass"].toInt(), std::vector<int>());
				if (channels.contains("channelCfi"))
				{
					for (const QJsonValue& chanIdx : channels["channelCfi"].toArray())
						chanClass.second.push_back(chanIdx.toInt());
						
				}
				LOGGER_INFO(logger) 
				<< (chanClass.second.empty() ? "ALL" : std::to_string(chanClass.second.size()))
				<< " channels requested in operating class " 
				<< chanClass.first;
				// TODO: are we handling the case where they want all?

				_inquiredChannels.push_back(chanClass);
			}
			LOGGER_INFO(logger) << _inquiredChannels.size() << " operating class(es) requested";
			validRequestType = true;
		} 

		if (jsonObj.contains("inquiredFrequencyRange") && !jsonObj["inquiredFrequencyRange"].isUndefined())
		{
			for (const QJsonValue& freqRangeVal : jsonObj["inquiredFrequencyRange"].toArray())
			{
				auto freqRange = freqRangeVal.toObject();
				_inquiredFrquencyRangesMHz.push_back(std::make_pair(
					freqRange["lowFrequency"].toInt(), 
					freqRange["highFrequency"].toInt()));
			}
			LOGGER_INFO(logger) << _inquiredFrquencyRangesMHz.size() << " frequency range(s) requested";
			validRequestType = true;
		}
		
		if (!validRequestType)
		{
			throw std::invalid_argument("must specify either inquiredChannels or inquiredFrequencies");
		}

		createChannelList();
		
	}
	else if (_analysisType == "PointAnalysis" || _analysisType == "APAnalysis")
	{
		// Adjacent Channel only for Point Analysis
		if (_analysisType == "PointAnalysis") {
			_aciFlag = jsonObj["useAdjacentChannel"].toBool();
		}
		// These are created for ease of copying
		QJsonObject ellipsePoint = jsonObj["location"].toObject()["point"].toObject();
		QJsonObject antenna = jsonObj["antenna"].toObject();

		// Assign the read values to the parts of the UserInputs struct
		_rlanUncertaintyRegionType = RLANBoundary::ELLIPSE;
		_deviceDesc = jsonObj["deviceDesc"].toObject();
		_serialNumber = _deviceDesc["serialNumber"].toString();
		_rlanLLA = std::make_tuple(ellipsePoint["center"].toObject()["latitude"].toDouble(),
				ellipsePoint["center"].toObject()["longitude"].toDouble(),
				antenna["height"].toDouble());
		_rlanUncerts_m = std::make_tuple(ellipsePoint["semiMinorAxis"].toDouble(),
				ellipsePoint["semiMajorAxis"].toDouble(),
				antenna["heightUncertainty"].toDouble());
		_rlanOrientation_deg = ellipsePoint["orientation"].toDouble();
		_rlanHeightType = antenna["heightType"].toString();

		if (jsonObj["capabilities"].toObject()["indoorOutdoor"].toString() == "Outdoor")
		{
			_rlanType = RLANType::RLAN_OUTDOOR;
		}
		else
		{ // Defaults to RLAN_INDOOR
			_rlanType = RLANType::RLAN_INDOOR;
		}

		auto numChannels = std::vector<int> { 59, 29, 14, 7 };
		auto bwList = std::vector<int> { 20, 40, 80, 160 };
		int startFreq = 5945; // MHz
        for(int bwIdx = 0; bwIdx < (int) bwList.size(); bwIdx++) 
		{
			for (int chanIdx = 0; chanIdx < numChannels.at(bwIdx); chanIdx++)
			{
				ChannelStruct channel;
				channel.startFreqMHz = startFreq + chanIdx * bwList.at(bwIdx);
				channel.stopFreqMHz = startFreq + (chanIdx + 1) * bwList.at(bwIdx);
				channel.availability = GREEN; // Everything initialized to GREEN
				channel.eirpLimit_dBm = 0;
				channel.type = ChannelType::INQUIRED_CHANNEL;
				_channelList.push_back(channel);
			}
        }
	}
	else if (_analysisType == "ExclusionZoneAnalysis")
	{
		_exclusionZoneFSID = jsonObj["FSID"].toInt();

		_exclusionZoneRLANBWHz = jsonObj["bandwidth"].toDouble() * 1.0e6;
		ChannelStruct channel;
		channel.availability = ChannelColor::GREEN;
		channel.eirpLimit_dBm = 0;
		channel.startFreqMHz = jsonObj["centerFrequency"].toInt() - jsonObj["bandwidth"].toInt()/2;
		channel.stopFreqMHz = jsonObj["centerFrequency"].toInt() + jsonObj["bandwidth"].toInt()/2;
		_channelList.push_back(channel);

		_exclusionZoneRLANChanIdx = 0;//round((centerFreq - _wlanMinFreq) / _exclusionZoneRLANBWHz - 0.5);
		_exclusionZoneRLANEIRPDBm = jsonObj["EIRP"].toDouble();

		_rlanHeightType = jsonObj["heightType"].toString();
		_rlanLLA = std::make_tuple(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), jsonObj["height"].toDouble());
		_rlanUncerts_m = std::make_tuple(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), jsonObj["heightUncertainty"].toDouble());
		_rlanType = jsonObj["indoorOutdoor"].toString() == "Outdoor" ? RLANType::RLAN_OUTDOOR : RLANType::RLAN_INDOOR;

		// set defaults for unused values
		_rlanOrientation_deg = 0.0; // Orientation (deg) of ellipse clockwise from North
	} 
	else if (_analysisType == "HeatmapAnalysis")
	{
		_heatmapRLANBWHz = jsonObj["bandwidth"].toDouble() * 1.0e6;

		QJsonObject bounds = jsonObj["bounds"].toObject();
		_heatmapMinLat = bounds["south"].toDouble();
		_heatmapMaxLat = bounds["north"].toDouble();
		_heatmapMinLon = bounds["west"].toDouble();
		_heatmapMaxLon = bounds["east"].toDouble();
		_heatmapRLANSpacing = jsonObj["spacing"].toDouble();

		// lat (deg), lon (deg), height (m) [height is specified differently]
		_rlanLLA = std::make_tuple(
				(_heatmapMaxLat + _heatmapMinLat) / 2, 
				(_heatmapMaxLon + _heatmapMinLon) / 2, 
				std::numeric_limits<double>::quiet_NaN()); 

		_heatmapRLANBWHz = jsonObj["bandwidth"].toDouble() * 1.0e6;
		_heatmapRLANChanIdx = 0;

		ChannelStruct channel;
		channel.availability = ChannelColor::GREEN;
		channel.eirpLimit_dBm = 0;
		channel.startFreqMHz = jsonObj["centerFrequency"].toInt() - jsonObj["bandwidth"].toInt()/2;
		channel.stopFreqMHz = jsonObj["centerFrequency"].toInt() + jsonObj["bandwidth"].toInt()/2;
		_channelList.push_back(channel);

		QJsonObject inOutdoor = jsonObj["indoorOutdoor"].toObject();
		if (inOutdoor["kind"].toString() == "Selected per Building Data")
		{
			// set both sets of values in here
			_heatmapIndoorOutdoorStr = "Database";   // Can be: "Indoor", "Outdoor", "Database"

			QJsonObject in = inOutdoor["in"].toObject();
			_heatmapRLANIndoorEIRPDBm = in["EIRP"].toDouble();
			_heatmapRLANIndoorHeight = in["height"].toDouble();
			_heatmapRLANIndoorHeightUncertainty = in["heightUncertainty"].toDouble();
			_heatmapRLANIndoorHeightType = in["heightType"].toString();

			QJsonObject out = inOutdoor["out"].toObject();
			_heatmapRLANOutdoorEIRPDBm = out["EIRP"].toDouble();
			_heatmapRLANOutdoorHeight = out["height"].toDouble();
			_heatmapRLANOutdoorHeightUncertainty = out["heightUncertainty"].toDouble();
			_heatmapRLANOutdoorHeightType = out["heightType"].toString();
		}
		else if (inOutdoor["kind"].toString() == "Outdoor")
		{
			// only set outdoor set
			_heatmapIndoorOutdoorStr = "Outdoor";   // Can be: "Indoor", "Outdoor", "Database"

			_heatmapRLANOutdoorEIRPDBm = inOutdoor["EIRP"].toDouble();
			_heatmapRLANOutdoorHeight = inOutdoor["height"].toDouble();
			_heatmapRLANOutdoorHeightUncertainty = inOutdoor["heightUncertainty"].toDouble();
			_heatmapRLANOutdoorHeightType = inOutdoor["heightType"].toString();
		}
		else
		{
			// default to indoor
			// only set indoor set
			_heatmapIndoorOutdoorStr = "Indoor";   // Can be: "Indoor", "Outdoor", "Database"

			_heatmapRLANIndoorEIRPDBm = inOutdoor["EIRP"].toDouble();
			_heatmapRLANIndoorHeight = inOutdoor["height"].toDouble();
			_heatmapRLANIndoorHeightUncertainty = inOutdoor["heightUncertainty"].toDouble();
			_heatmapRLANIndoorHeightType = inOutdoor["heightType"].toString();
		}

		// set defaults for unused values
		_rlanOrientation_deg = 0.0; // Orientation (deg) of ellipse clockwise from North
		_rlanUncerts_m = std::make_tuple(0, 0, 0);
	}
	else
	{
		throw std::runtime_error(QString("Invalid analysis type: %1").arg(QString::fromStdString(_analysisType)).toStdString());
	}

}

// Support command line interface with AFC Engine
void AfcManager::setCmdLineParams(std::string &inputFilePath, std::string &configFilePath, std::string &outputFilePath, std::string &tempDir, std::string &logLevel, int argc, char **argv)
{
	// Declare the supported options
	po::options_description optDescript{"Allowed options"};
	// Create command line options
	optDescript.add_options()
		("help,h", "Use input-file-path, config-file-path, or output-file-path.")
		("request-type,r", po::value<std::string>()->default_value("PointAnalysis"), "set request-type (PointAnalysis, APAnalysis, HeatmapAnalysis, ExclusionZoneAnalysis)")
		("state-root,s", po::value<std::string>()->default_value("/var/lib/fbrat"), "set fbrat state root directory")
		("input-file-path,i", po::value<std::string>()->default_value("inputFile.json"), "set input-file-path level")
		("config-file-path,c", po::value<std::string>()->default_value("configFile.json"), "set config-file-path level")
		("output-file-path,o", po::value<std::string>()->default_value("outputFile.json"), "set output-file-path level")
		("temp-dir,t", po::value<std::string>()->default_value(""), "set temp-dir level")
		("log-level,l", po::value<std::string>()->default_value("debug"), "set log-level");

	po::variables_map cmdLineArgs;
	po::store(po::parse_command_line(argc, argv, optDescript), cmdLineArgs); // ac and av are parameters passed into main
	po::notify(cmdLineArgs);

	// Check whether "help" argument was specified
	if (cmdLineArgs.count("help"))
	{
		std::cout << optDescript << std::endl;
		exit(0); // Terminate program, indicating it completed successfully
	}
	// Check whether "request-type(r)" was specified
	if (cmdLineArgs.count("request-type"))
	{
		_analysisType = cmdLineArgs["request-type"].as<std::string>();
	}
	else // Must be specified
	{
		throw std::runtime_error("AfcManager::setCmdLineParams(): request-type(r) command line argument was not set.");
	}
	// Check whether "state-root(s)" was specified
	if (cmdLineArgs.count("state-root"))
	{
		_stateRoot = cmdLineArgs["state-root"].as<std::string>();
	}
	else // Must be specified
	{
		throw std::runtime_error("AfcManager::setCmdLineParams(): state-root(s) command line argument was not set.");
	}
	// Check whether "input-file-path(i)" was specified
	if (cmdLineArgs.count("input-file-path"))
	{
		inputFilePath = cmdLineArgs["input-file-path"].as<std::string>();
	}
	else
	{
		throw std::runtime_error("AfcManager::setCmdLineParams(): input-file-path(i) command line argument was not set.");
	}
	// Check whether "config-file-path(c)" was specified
	if (cmdLineArgs.count("config-file-path"))
	{ // This is a work in progress
		configFilePath = cmdLineArgs["config-file-path"].as<std::string>();
	}
	else
	{
		throw std::runtime_error("AfcManager::setCmdLineParams(): config-file-path(c) command line argument was not set.");
	}
	// Check whether "output-file-path(o)" was specified
	if (cmdLineArgs.count("output-file-path"))
	{
		outputFilePath = cmdLineArgs["output-file-path"].as<std::string>();
	}
	else
	{
		throw std::runtime_error("AfcManager::setCmdLineParams(): output-file-path(o) command line argument was not set.");
	}
	// Check whether "temp-dir(t)" was specified
	if (cmdLineArgs.count("temp-dir"))
	{
		tempDir = cmdLineArgs["temp-dir"].as<std::string>();
	}
	else
	{
		throw std::runtime_error("AfcManager::setCmdLineParams(): temp-dir command line argument was not set.");
	}
	// Check whether "log-level(l)" was specified
	if (cmdLineArgs.count("log-level"))
	{
		logLevel = cmdLineArgs["log-level"].as<std::string>();
	}
	else
	{
		throw std::runtime_error("AfcManager::setCmdLineParams(): log-level command line argument was not set.");
	}
}

void AfcManager::importConfigAFCjson(const std::string &inputJSONpath)
{
	// read json file in
	QJsonDocument jsonDoc;
	QFile inFile;
	QString fName = QString(inputJSONpath.c_str());
	inFile.setFileName(fName);
	//inFile.open(inputJSONpath, std::ifstream::in);
	if (inFile.open(QFile::ReadOnly | QFile::Text))
	{
		jsonDoc = QJsonDocument::fromJson(inFile.readAll());
		inFile.close();
	}
	else
	{
		throw std::runtime_error("AfcManager::importConfigAFCjson(): Failed to open JSON file specifying configuration parameters.");
	}

	// Print raw contents of input JSON file
	LOGGER_DEBUG(logger) << "Raw contents of input JSON file provided by the GUI: " << jsonDoc.toJson().toStdString();

	// Read JSON from file
	QJsonObject jsonObj = jsonDoc.object();

	// These are created for ease of copying
	QJsonObject ellipsePoint = jsonObj["location"].toObject()["point"].toObject();
	QJsonObject antenna = jsonObj["antenna"].toObject();
	QJsonObject buildingLoss = jsonObj["buildingPenetrationLoss"].toObject();
	QJsonObject propModel = jsonObj["propagationModel"].toObject();

	// Input parameters stored in the AfcManager object
	_regionStr = jsonObj["regionStr"].toString().toStdString();

        if (_regionStr == "CONUS") {
            _regionPolygonFileList = SearchPaths::forReading("data", "fbrat/rat_transfer/population/conus.kml", true).toStdString();
        } else if (_regionStr == "Canada") {
            _regionPolygonFileList = SearchPaths::forReading("data", "fbrat/rat_transfer/population/Canada.kml", true).toStdString();
        } else {
	    throw std::runtime_error("AfcManager::importConfigAFCjson(): Invalid regionStr specified.");
        }

	_ulsDataFile = _stateRoot + "/ULS_Database/" + jsonObj["ulsDatabase"].toString().toStdString();
	_inputULSDatabaseStr = jsonObj["ulsDatabase"].toString();
	_rasDataFile = _stateRoot + "/RAS_Database/" + jsonObj["rasDatabase"].toString().toStdString();
	if (std::isnan(_minEIRP_dBm)) // only use config min eirp if not specified in request object
		_minEIRP_dBm = jsonObj["minEIRP"].toDouble();
	_maxEIRP_dBm = jsonObj["maxEIRP"].toDouble();
	_IoverN_threshold_dB = jsonObj["threshold"].toDouble();
	_maxRadius = jsonObj["maxLinkDistance"].toDouble() * 1000.0; // convert from km to m
	_bodyLossIndoorDB = jsonObj["bodyLoss"].toObject()["valueIndoor"].toDouble();
	_bodyLossOutdoorDB = jsonObj["bodyLoss"].toObject()["valueOutdoor"].toDouble();
	_polarizationLossDB = jsonObj["polarizationMismatchLoss"].toObject()["value"].toDouble();
	_buildingLossModel = (buildingLoss["kind"]).toString();
	_propagationEnviro = jsonObj["propagationEnv"].toString();
	_receiverFeedLoss_dB = jsonObj["receiverFeederLoss"].toDouble();
	_antennaPattern = jsonObj["antennaPattern"].toObject()["kind"].toString();

	if (_antennaPattern == "User Upload") {
		//_ulsAntennaPatternFile = SearchPaths::forReading("data", "fbrat/AntennaPatterns/" + jsonObj["antennaPattern"].toObject()["value"].toString(), true).toStdString();
		_ulsAntennaPatternFile = QDir(QString::fromStdString(_stateRoot))
			.absoluteFilePath(QString::fromStdString("/AntennaPatterns/") + jsonObj["antennaPattern"].toObject()["value"].toString())
			.toStdString();
		LOGGER_INFO(logger) << "Antenna pattern file set to: " << _ulsAntennaPatternFile;
	} else {
		_ulsAntennaPatternFile = "";
	}

	if (_rlanType == RLANType::RLAN_INDOOR) {
		// Check what the building penetration type is (right ehre it is ITU-R P.2109)
		if (buildingLoss["kind"].toString() == "ITU-R Rec. P.2109") {
			_fixedBuildingLossFlag = false;

			if (buildingLoss["buildingType"].toString() == "Traditional") {
				_buildingType = CConst::traditionalBuildingType;
			} else if (buildingLoss["buildingType"].toString() == "Efficient") {
				_buildingType = CConst::thermallyEfficientBuildingType;
			}
			_confidenceBldg2109 = buildingLoss["confidence"].toDouble() / 100.0;

			// User uses a fixed value for building loss
		} else if (buildingLoss["kind"].toString() == "Fixed Value") {
			_fixedBuildingLossFlag = true;
			_fixedBuildingLossValue = buildingLoss["value"].toDouble();
		} else {
			throw std::runtime_error("ERROR: Invalid buildingLoss[\"kind\"]");
		}
		_bodyLossDB = _bodyLossIndoorDB;
	} else {
		_buildingType = CConst::noBuildingType;
		_confidenceBldg2109 = 0.0;
		_fixedBuildingLossFlag = false;
		_fixedBuildingLossValue = 0.0;
		_bodyLossDB = _bodyLossOutdoorDB;
	}

	// **** Determine which propagation model to use ****
    _winner2CombineFlag = false;
    _pathLossClampFSPL = false;

	// FSPL
	if (propModel["kind"].toString() == "FSPL")
	{
		_pathLossModelStr = "FSPL";
                _winner2BldgLOSFlag = false;
	}

	// No buildings, Winner II, ITM, P.456 Clutter
	else if (propModel["kind"].toString() == "ITM with no building data")
	{
		// GUI gets these values as percentiles from 0-100, convert to probabilities 0-1
		_winner2ProbLOSThr = propModel["win2ProbLosThreshold"].toDouble()/100.0;
		_confidenceWinner2 = propModel["win2Confidence"].toDouble()/100.0;
		_confidenceITM = propModel["itmConfidence"].toDouble()/100.0;
		_confidenceClutter2108 = propModel["p2108Confidence"].toDouble()/100.0;
		
		_use3DEP = propModel["terrainSource"].toString() == "3DEP (30m)";

		_pathLossModelStr = "COALITION_OPT_6";
                _winner2BldgLOSFlag = false;
	}

	// FCC_6GHZ_REPORT_AND_ORDER path loss model
	else if (propModel["kind"].toString() == "FCC 6GHz Report & Order")
	{
		// GUI gets these values as percentiles from 0-100, convert to probabilities 0-1
		_winner2ProbLOSThr = std::numeric_limits<double>::quiet_NaN();
		_confidenceWinner2 = propModel["win2Confidence"].toDouble()/100.0;
		_confidenceITM = propModel["itmConfidence"].toDouble()/100.0;
		_confidenceClutter2108 = propModel["p2108Confidence"].toDouble()/100.0;

		_useBDesignFlag = propModel["buildingSource"].toString() == "B-Design3D";
		_useLiDAR = propModel["buildingSource"].toString() == "LiDAR";
		// for now, we always use 3DEP for FCC 6Ghz
		_use3DEP = true;//propModel["terrainSource"].toString() == "3DEP (30m)";

		_pathLossModelStr = "FCC_6GHZ_REPORT_AND_ORDER";
        _winner2CombineFlag = true;
        _pathLossClampFSPL = true;
        _winner2BldgLOSFlag = (_useBDesignFlag || _useLiDAR);
	}

	// Buildings involved in analysis
	else if (propModel["kind"].toString() == "ITM with building data")
	{
		// GUI gets these values as percentiles from 0-100, convert to probabilities 0-1
		_winner2ProbLOSThr = propModel["win2ProbLosThreshold"].toDouble()/100.0;
		_confidenceWinner2 = propModel["win2Confidence"].toDouble()/100.0;
		_confidenceITM = propModel["itmConfidence"].toDouble()/100.0;
		_confidenceClutter2108 = propModel["p2108Confidence"].toDouble()/100.0;
		_useBDesignFlag = propModel["buildingSource"].toString() == "B-Design3D";
		_useLiDAR = propModel["buildingSource"].toString() == "LiDAR";
		_use3DEP = true;
		_pathLossModelStr = "ITM_BLDG";
                _winner2BldgLOSFlag = false;
	}

	// Ray tracing
	else if (propModel["kind"].toString() == "Ray Tracing")
	{
		throw std::invalid_argument("AfcManager::importConfigAFCjson(): This propagation model (" + propModel["kind"].toString().toStdString() + ") is not supported");
	}
	else
	{
		throw std::invalid_argument("AfcManager::importConfigAFCjson(): This propagation model (" + propModel["kind"].toString().toStdString() + ") is not supported");
	}
}

QJsonArray generateStatusMessages(const std::vector<std::string>& messages)
{
	auto array = QJsonArray();
	for (auto m : messages)
		array.append(QJsonValue(QString::fromStdString(m)));
	return array;
}

void AfcManager::addBuildingDatabaseTiles(OGRLayer *layer)
{
	// add building database raster boundary if loaded
	if (_pathLossModelStr == "ITM_BLDG")
	{
		LOGGER_DEBUG(logger) << "adding raster bounds";
		for (QRectF b : _terrainDataModel->getBounds())
		{
			LOGGER_DEBUG(logger) << "adding tile";
			// Instantiate cone object
			std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> boxFeature(OGRFeature::CreateFeature(layer->GetLayerDefn()));

			// Intantiate unique-pointers to OGRPolygon and OGRLinearRing for storing the beam coverage
			GdalHelpers::GeomUniquePtr<OGRPolygon> boundBox(GdalHelpers::createGeometry<OGRPolygon>()); // Use GdalHelpers.h templates to have unique pointers create these on the heap
			GdalHelpers::GeomUniquePtr<OGRLinearRing> boxPoints(GdalHelpers::createGeometry<OGRLinearRing>());

			// Create OGRPoints to store the coordinates of the beam triangle
			// ***IMPORTANT NOTE: Coordinates stored as (Lon,Lat) here, required by geoJSON specifications
			GdalHelpers::GeomUniquePtr<OGRPoint> topLeft(GdalHelpers::createGeometry<OGRPoint>());
			topLeft->setY(b.top());
			topLeft->setX(b.left()); // Must set points manually since cannot construct while pointing at with unique pointer
			GdalHelpers::GeomUniquePtr<OGRPoint> topRight(GdalHelpers::createGeometry<OGRPoint>());
			topRight->setY(b.top());
			topRight->setX(b.right());
			GdalHelpers::GeomUniquePtr<OGRPoint> botLeft(GdalHelpers::createGeometry<OGRPoint>());
			botLeft->setY(b.bottom());
			botLeft->setX(b.left());
			GdalHelpers::GeomUniquePtr<OGRPoint> botRight(GdalHelpers::createGeometry<OGRPoint>());
			botRight->setY(b.bottom());
			botRight->setX(b.right());

			// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
			boxPoints->addPoint(topLeft.get()); // Using .get() gives access to the pointer without giving up ownership
			boxPoints->addPoint(topRight.get());
			boxPoints->addPoint(botRight.get());
			boxPoints->addPoint(botLeft.get());
			boxPoints->addPoint(topLeft.get());
			boundBox->addRingDirectly(boxPoints.release()); // Points unique-pointer to null and gives up ownership of exteriorOfCone to beamCone

			// Add properties to the geoJSON features
			boxFeature->SetField("kind", "BLDB"); // BLDB = building bounds

			// Add geometry to feature
			boxFeature->SetGeometryDirectly(boundBox.release());

			if (layer->CreateFeature(boxFeature.release()) != OGRERR_NONE)
			{
				throw std::runtime_error("Could not add bound feature in layer of output data source");
			}
		}
	}
}

QJsonDocument AfcManager::generateRatAfcJson()
{
	std::vector<psdFreqRangeClass> psdFreqRangeList;
	computeInquiredFreqRangesPSD(psdFreqRangeList);

	// compute availableSpectrumInfo
	QJsonArray spectrumInfos = QJsonArray();
	for (auto freqRange : psdFreqRangeList)
	{
		for (int i = 0; i < (int) freqRange.psd_dBm_MHzList.size(); i++)
		{
			spectrumInfos.append(QJsonObject 
			{
				{ "frequencyRange", QJsonObject
					{
						{ "lowFrequency", freqRange.freqMHzList.at(i) },
						{ "highFrequency", freqRange.freqMHzList.at(i+1) }
					}
				},
				{ "maxPSD", freqRange.psd_dBm_MHzList.at(i) }
			});
		}
	}

	QJsonArray channelInfos = QJsonArray();
	for (const auto &group : _inquiredChannels)
	{
		auto operatingClass = group.first;
		auto indexArray = QJsonArray();
		auto eirpArray = QJsonArray();

		for (const auto &chan : _channelList)
		{
			if (chan.type == ChannelType::INQUIRED_CHANNEL && chan.operatingClass == operatingClass)
			{
				indexArray.append(chan.index);
				eirpArray.append(chan.eirpLimit_dBm);
			}
		}

		auto channelGroup = QJsonObject
		{
			{ "globalOperatingClass", QJsonValue(operatingClass) },
			{ "channelCfi", indexArray },
			{ "maxEirp", eirpArray }
		};
		channelInfos.append(channelGroup);
	}
	
	// for (auto chan : _channelList)
	// {
	// 	if (chan.type == ChannelType::INQUIRED_CHANNEL)
	// 	{
	// 		if (channelInfos.size() == 0)
	// 		{
	// 			channelInfos.push_back(QJsonObject
	// 			{
	// 				{ "globalOperatingClass", chan.operatingClass },
	// 				{ "channelCfi", QJsonArray { chan.index } },
	// 				{ "maxEirp", QJsonArray { chan.eirpLimit_dBm } }
	// 			});
	// 			continue;
	// 		}
	// 		for (int infoIdx = 0; infoIdx < channelInfos.size(); infoIdx++)
	// 		{
	// 			QJsonObject chanClass = channelInfos[infoIdx].toObject();
	// 			if (chan.operatingClass == chanClass["globalOperatingClass"].toInt())
	// 			{
	// 				auto indexArray = chanClass["channelCfi"].toArray();
	// 				auto eirpArray = chanClass["maxEirp"].toArray();
	// 				indexArray.push_back(QJsonValue(chan.index));
	// 				eirpArray.push_back(QJsonValue(chan.eirpLimit_dBm));
	// 				chanClass["channelCfi"] = indexArray;
	// 				chanClass["maxEirp"] = eirpArray;
	// 				continue;
	// 			}
	// 			channelInfos.push_back(QJsonObject
	// 			{
	// 				{ "globalOperatingClass", chan.operatingClass },
	// 				{ "channelCfi", QJsonArray { chan.index } },
	// 				{ "maxEirp", QJsonArray { chan.eirpLimit_dBm } }
	// 			});
	// 		}
	// 	}
	// }

	QJsonObject responses { 
		{ "version", "0.6" },
		{ "availableSpectrumInquiryResponses", 
			QJsonArray 
			{
				QJsonObject 
				{
					{ "requestId", _requestId },
					{ "availableChannelInfo", channelInfos },
					{ "availableSpectrumInfo", spectrumInfos }, 
					{ "availabilityExpireTime", ISO8601TimeUTC(1) },
					{ "response", QJsonObject
						{
							{ "responseCode", 0 },
							{ "shortDescription", "success" }
						} 
					}
				}
			}
		}
	};

	return QJsonDocument(responses);
}

QJsonDocument AfcManager::generateExclusionZoneJson()
{
	// Temporarily export layer so that we can re-import it as a geoJSON-formatted string
	const std::string gdalDriverName = "GeoJSON";
	OGRRegisterAll();

	OGRSFDriver* ptrDriver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(gdalDriverName.c_str());
	// This is the same as !(unique_ptr.get() != nullptr)
	if (ptrDriver == nullptr)
	{
		throw std::runtime_error("AfcManager::generateExclusionZone(): " + gdalDriverName + " driver was not found");
	}

	QTemporaryDir tempDir;
	if (!tempDir.isValid())
	{
		throw std::runtime_error("AfcManager::generateExclusionZone(): Failed to create a temporary directory to store output of GeoJSON driver");
	}

	const QString tempOutFileName = "output.tmp";
	const QString tempOutFilePath = tempDir.filePath(tempOutFileName);

	std::unique_ptr<OGRDataSource> ptrOutputDS(ptrDriver->CreateDataSource(tempOutFilePath.toStdString().c_str(), NULL));
	if (ptrOutputDS == nullptr)
	{
		throw std::runtime_error("AfcManager::generateExclusionZone(): Could not create a data source at " + tempOutFilePath.toStdString());
	}

	OGRSpatialReference spatialRefWGS84; // Set the desired spatial reference (WGS84 in this case)
	spatialRefWGS84.SetWellKnownGeogCS("WGS84");
	OGRLayer *exclusionLayer = ptrOutputDS->CreateLayer("Temp_Output", &spatialRefWGS84, wkbPolygon, NULL);
	if (exclusionLayer == nullptr)
	{
		throw std::runtime_error("AfcManager::generateExclusionZone(): Could not create a layer in output data source");
	}

	OGRFieldDefn objKind("kind", OFTString);
	OGRFieldDefn fsid("FSID", OFTInteger);
	OGRFieldDefn terrainHeight("terrainHeight", OFTReal);
	OGRFieldDefn height("height", OFTReal);
	OGRFieldDefn lat("lat", OFTReal);
	OGRFieldDefn lon("lon", OFTReal);
	objKind.SetWidth(64);
	fsid.SetWidth(32);
	terrainHeight.SetWidth(32);
	height.SetWidth(32);
	lat.SetWidth(32);
	lon.SetWidth(32);

	if (exclusionLayer->CreateField(&objKind) != OGRERR_NONE)
	{
		throw std::runtime_error("AfcManager::generateExclusionZone(): Could not create 'kind' field in layer of the output data source");
	}
	if (exclusionLayer->CreateField(&fsid) != OGRERR_NONE)
	{
		throw std::runtime_error("AfcManager::generateExclusionZone(): Could not create 'fsid' field in layer of the output data source");
	}
	if (exclusionLayer->CreateField(&terrainHeight) != OGRERR_NONE)
	{
		throw std::runtime_error("AfcManager::generateExclusionZone(): Could not create 'terrainHeight' field in layer of the output data source");
	}
	if (exclusionLayer->CreateField(&height) != OGRERR_NONE)
	{
		throw std::runtime_error("AfcManager::generateExclusionZone(): Could not create 'height' field in layer of the output data source");
	}
	if (exclusionLayer->CreateField(&lat) != OGRERR_NONE)
	{
		throw std::runtime_error("AfcManager::generateExclusionZone(): Could not create 'lat' field in layer of the output data source");
	}
	if (exclusionLayer->CreateField(&lon) != OGRERR_NONE)
	{
		throw std::runtime_error("AfcManager::generateExclusionZone(): Could not create 'lon' field in layer of the output data source");
	}


	// Instantiate polygon object
	std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> exclusionZoneFeature(OGRFeature::CreateFeature(exclusionLayer->GetLayerDefn()));

	// Intantiate unique-pointers to OGRPolygon and OGRLinearRing for storing the beam coverage
	GdalHelpers::GeomUniquePtr<OGRPolygon> exZone(GdalHelpers::createGeometry<OGRPolygon>()); // Use GdalHelpers.h templates to have unique pointers create these on the heap
	GdalHelpers::GeomUniquePtr<OGRLinearRing> exteriorOfZone(GdalHelpers::createGeometry<OGRLinearRing>());

	ULSClass *uls = findULSID(_exclusionZoneFSID);
	// Add properties to the geoJSON features
	exclusionZoneFeature->SetField("FSID", _exclusionZoneFSID);
	exclusionZoneFeature->SetField("kind", "ZONE");
	exclusionZoneFeature->SetField("terrainHeight", _exclusionZoneFSTerrainHeight);
	exclusionZoneFeature->SetField("height", _exclusionZoneHeightAboveTerrain);
	exclusionZoneFeature->SetField("lat", uls->getRxLatitudeDeg());
	exclusionZoneFeature->SetField("lon", uls->getRxLongitudeDeg());

	// Must set points manually since cannot construct while pointing at with unique pointer
	GdalHelpers::GeomUniquePtr<OGRPoint> startPoint(GdalHelpers::createGeometry<OGRPoint>());
	startPoint->setX(_exclusionZone.back().first); // Lon
	startPoint->setY(_exclusionZone.back().second); // Lat
	exteriorOfZone->addPoint(startPoint.get()); // Using .get() gives access to the pointer without giving up ownership

	for (const auto &point : _exclusionZone)
	{
		// Create OGRPoints to store the coordinates of the beam triangle
		// ***IMPORTANT NOTE: Coordinates stored as (Lon,Lat) here, required by geoJSON specifications
		GdalHelpers::GeomUniquePtr<OGRPoint> posPoint(GdalHelpers::createGeometry<OGRPoint>());
		posPoint->setX(point.first);
		posPoint->setY(point.second);

		// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
		exteriorOfZone->addPoint(posPoint.get());
	}

	// Add exterior boundary of cone's polygon
	exZone->addRingDirectly(exteriorOfZone.release()); // Points unique-pointer to null and gives up ownership of exteriorOfCone to beamCone

	// Add geometry to feature
	exclusionZoneFeature->SetGeometryDirectly(exZone.release());

	if (exclusionLayer->CreateFeature(exclusionZoneFeature.release()) != OGRERR_NONE)
	{
		throw std::runtime_error("Could not add cone feature in layer of output data source");
	}

	// Allocation clean-up
	OGRDataSource::DestroyDataSource(ptrOutputDS.release()); // Remove the reference to the data source

	// Create file to be written to (creates a file, a json object, and a json document in order to store)
	std::ifstream tempFileStream(tempOutFilePath.toStdString(), std::ifstream::in);
	const std::string geoJsonCollection = slurp(tempFileStream);

	QJsonDocument geoJsonDoc = QJsonDocument::fromJson(QString::fromStdString(geoJsonCollection).toUtf8());
	QJsonObject geoJsonObj = geoJsonDoc.object();

	QJsonObject analysisJsonObj = {
		{"geoJson", geoJsonObj},
		{"statusMessageList", generateStatusMessages(statusMessageList)}
	};

	return QJsonDocument(analysisJsonObj);
}

QJsonDocument AfcManager::generateHeatmap()
{
	// Temporarily export layer so that we can re-import it as a geoJSON-formatted string
	const std::string gdalDriverName = "GeoJSON";
	OGRRegisterAll();

	OGRSFDriver* ptrDriver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(gdalDriverName.c_str());
	// This is the same as !(unique_ptr.get() != nullptr)
	if (ptrDriver == nullptr)
	{
		throw std::runtime_error("AfcManager::generateHeatmap(): " + gdalDriverName + " driver was not found");
	}

	QTemporaryDir tempDir;
	if (!tempDir.isValid())
	{
		throw std::runtime_error("AfcManager::generateHeatmap(): Failed to create a temporary directory to store output of GeoJSON driver");
	}

	const QString tempOutFileName = "output.tmp";
	const QString tempOutFilePath = tempDir.filePath(tempOutFileName);

	std::unique_ptr<OGRDataSource> ptrOutputDS(ptrDriver->CreateDataSource(tempOutFilePath.toStdString().c_str(), NULL));
	if (ptrOutputDS == nullptr)
	{
		throw std::runtime_error("AfcManager::generateHeatmap(): Could not create a data source at " + tempOutFilePath.toStdString());
	}

	OGRSpatialReference spatialRefWGS84; // Set the desired spatial reference (WGS84 in this case)
	spatialRefWGS84.SetWellKnownGeogCS("WGS84");
	OGRLayer *heatmapLayer = ptrOutputDS->CreateLayer("Temp_Output", &spatialRefWGS84, wkbPolygon, NULL);
	if (heatmapLayer == nullptr)
	{
		throw std::runtime_error("AfcManager::generateHeatmap(): Could not create a layer in output data source");
	}

	OGRFieldDefn objKind("kind", OFTString);
	OGRFieldDefn iToN("ItoN", OFTReal);
	OGRFieldDefn indoor("indoor", OFTString);
	objKind.SetWidth(64);
	iToN.SetWidth(32);
	indoor.SetWidth(32);

	if (heatmapLayer->CreateField(&objKind) != OGRERR_NONE)
	{
		throw std::runtime_error("AfcManager::generateHeatmap(): Could not create 'kind' field in layer of the output data source");
	}
	if (heatmapLayer->CreateField(&iToN) != OGRERR_NONE)
	{
		throw std::runtime_error("AfcManager::generateHeatmap(): Could not create 'ItoN' field in layer of the output data source");
	}
	if (heatmapLayer->CreateField(&indoor) != OGRERR_NONE)
	{
		throw std::runtime_error("AfcManager::generateHeatmap(): Could not create 'indoor' field in layer of the output data source");
	}

	double latDel = 0.5 * (_heatmapMaxLat - _heatmapMinLat) / _heatmapNumPtsLat; // distance from center point to top/bot side of square
	double lonDel = 0.5 * (_heatmapMaxLon - _heatmapMinLon) / _heatmapNumPtsLon; // distance from center point to left/right side of square
	LOGGER_DEBUG(logger) << "generating heatmap: " << _heatmapNumPtsLon << "x" << _heatmapNumPtsLat;
	for (int lonIdx = 0; lonIdx < _heatmapNumPtsLon; lonIdx++)
	{
		for (int latIdx = 0; latIdx < _heatmapNumPtsLat; latIdx++)
		{
			double lon = (_heatmapMinLon*(2*_heatmapNumPtsLon-2*lonIdx-1) + _heatmapMaxLon*(2*lonIdx+1)) / (2*_heatmapNumPtsLon);
			double lat = (_heatmapMinLat*(2*_heatmapNumPtsLat-2*latIdx-1) + _heatmapMaxLat*(2*latIdx+1)) / (2*_heatmapNumPtsLat);
			// Instantiate polygon object
			std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> heatmapFeature(OGRFeature::CreateFeature(heatmapLayer->GetLayerDefn()));

			// Intantiate unique-pointers to OGRPolygon and OGRLinearRing for storing the beam coverage
			GdalHelpers::GeomUniquePtr<OGRPolygon> mapZone(GdalHelpers::createGeometry<OGRPolygon>()); // Use GdalHelpers.h templates to have unique pointers create these on the heap
			GdalHelpers::GeomUniquePtr<OGRLinearRing> mapExt(GdalHelpers::createGeometry<OGRLinearRing>());

			// Add properties to the geoJSON features
			heatmapFeature->SetField("kind", "HMAP");
			heatmapFeature->SetField("ItoN", _heatmapIToNDB[lonIdx][latIdx]);
			heatmapFeature->SetField("indoor", _heatmapIsIndoor[lonIdx][latIdx] ? "Y" : "N");

			// Create OGRPoints to store the coordinates of the heatmap box
			// ***IMPORTANT NOTE: Coordinates stored as (Lon,Lat) here, required by geoJSON specifications
			GdalHelpers::GeomUniquePtr<OGRPoint> tl(GdalHelpers::createGeometry<OGRPoint>());
			tl->setX(lon - lonDel);
			tl->setY(lat + latDel);
			// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
			mapExt->addPoint(tl.get());

			GdalHelpers::GeomUniquePtr<OGRPoint> tr(GdalHelpers::createGeometry<OGRPoint>());
			tr->setX(lon + lonDel);
			tr->setY(lat + latDel);
			// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
			mapExt->addPoint(tr.get());

			GdalHelpers::GeomUniquePtr<OGRPoint> br(GdalHelpers::createGeometry<OGRPoint>());
			br->setX(lon + lonDel);
			br->setY(lat - latDel);
			// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
			mapExt->addPoint(br.get());

			GdalHelpers::GeomUniquePtr<OGRPoint> bl(GdalHelpers::createGeometry<OGRPoint>());
			bl->setX(lon - lonDel);
			bl->setY(lat - latDel);
			// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
			mapExt->addPoint(bl.get());

			// add end point again
			mapExt->addPoint(tl.get());

			// Add exterior boundary of cone's polygon
			mapZone->addRingDirectly(mapExt.release()); // Points unique-pointer to null and gives up ownership of exteriorOfCone to beamCone

			// Add geometry to feature
			heatmapFeature->SetGeometryDirectly(mapZone.release());

			if (heatmapLayer->CreateFeature(heatmapFeature.release()) != OGRERR_NONE)
			{
				throw std::runtime_error("Could not add heat map tile feature in layer of output data source");
			}
		}
	}

	addBuildingDatabaseTiles(heatmapLayer);

	// Allocation clean-up
	OGRDataSource::DestroyDataSource(ptrOutputDS.release()); // Remove the reference to the data source

	// Create file to be written to (creates a file, a json object, and a json document in order to store)
	std::ifstream tempFileStream(tempOutFilePath.toStdString(), std::ifstream::in);
	const std::string geoJsonCollection = slurp(tempFileStream);

	QJsonDocument geoJsonDoc = QJsonDocument::fromJson(QString::fromStdString(geoJsonCollection).toUtf8());
	QJsonObject geoJsonObj = geoJsonDoc.object();

	QJsonObject analysisJsonObj = {
		{"geoJson", geoJsonObj},
		{"minItoN", QJsonValue(_heatmapMinIToNDB)},
		{"maxItoN", QJsonValue(_heatmapMaxIToNDB)},
		{"threshold", QJsonValue(_heatmapIToNThresholdDB)},
		{"statusMessageList", generateStatusMessages(statusMessageList)}
	};

	return QJsonDocument(analysisJsonObj);
}

void AfcManager::exportGUIjson(const QString &exportJsonPath)
{

	QJsonDocument outputDocument;
	if (_analysisType == "APAnalysis")
	{
		// if the request type is PAWS then we only return the spectrum data
		outputDocument = QJsonDocument(jsonSpectrumData(_channelList, _deviceDesc, _wlanMinFreq));

		// Write PAWS outputs to JSON file
		// QFile outputAnalysisFile(exportJsonPath);
		// outputAnalysisFile.open(QFile::WriteOnly);
		// outputAnalysisFile.write(outputDocument.toJson());
		// outputAnalysisFile.close();
		// LOGGER_DEBUG(logger) << "PAWS response saved to the following JSON file: " << outputAnalysisFile.fileName().toStdString();
		// return; // Short circuit since export is complete now
	}
	else if (_analysisType == "AP-AFC") 
	{
		// temporarily return PAWS until we write new generator function
		//outputDocument = QJsonDocument(jsonSpectrumData(_rlanBWList, _numChan, _channelData, _deviceDesc, _wlanMinFreq));
		outputDocument = generateRatAfcJson();
	}
	else if (_analysisType == "ExclusionZoneAnalysis")
	{
		outputDocument = generateExclusionZoneJson();
	}
	else if (_analysisType == "HeatmapAnalysis")
	{
		outputDocument = generateHeatmap();
	}
	else
	{
		// Temporarily export layer so that we can re-import it as a geoJSON-formatted string
		const std::string gdalDriverName = "GeoJSON";
		OGRRegisterAll();

		OGRSFDriver* ptrDriver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(gdalDriverName.c_str());
		// This is the same as !(unique_ptr.get() != nullptr)
		if (ptrDriver == nullptr)
		{
			throw std::runtime_error("AfcManager::exportGUIjson(): " + gdalDriverName + " driver was not found");
		}

		QTemporaryDir tempDir;
		if (!tempDir.isValid())
		{
			throw std::runtime_error("AfcManager::exportGUIjson(): Failed to create a temporary directory to store output of GeoJSON driver");
		}

		const QString tempOutFileName = "output.tmp";
		const QString tempOutFilePath = tempDir.filePath(tempOutFileName);

		std::unique_ptr<OGRDataSource> ptrOutputDS(ptrDriver->CreateDataSource(tempOutFilePath.toStdString().c_str(), NULL));
		if (ptrOutputDS == nullptr)
		{
			throw std::runtime_error("AfcManager::exportGUIjson(): Could not create a data source at " + tempOutFilePath.toStdString());
		}

		OGRSpatialReference spatialRefWGS84; // Set the desired spatial reference (WGS84 in this case)
		spatialRefWGS84.SetWellKnownGeogCS("WGS84");
		OGRLayer *coneLayer = ptrOutputDS->CreateLayer("Temp_Output", &spatialRefWGS84, wkbPolygon, NULL);
		if (coneLayer == nullptr)
		{
			throw std::runtime_error("AfcManager::exportGUIjson(): Could not create a layer in output data source");
		}

		OGRFieldDefn objKind("kind", OFTString);
		OGRFieldDefn fsidField("FSID", OFTInteger);
		OGRFieldDefn startFreq("startFreq", OFTReal);
		OGRFieldDefn stopFreq("stopFreq", OFTReal);
		objKind.SetWidth(64); /* fsLonField.SetWidth(64); fsLatField.SetWidth(64);*/
		fsidField.SetWidth(32); /* fsLonField.SetWidth(64); fsLatField.SetWidth(64);*/
		startFreq.SetWidth(32);
		stopFreq.SetWidth(32);

		if (coneLayer->CreateField(&objKind) != OGRERR_NONE)
		{
			throw std::runtime_error("AfcManager::exportGUIjson(): Could not create 'kind' field in layer of the output data source");
		}
		if (coneLayer->CreateField(&fsidField) != OGRERR_NONE)
		{
			throw std::runtime_error("AfcManager::exportGUIjson(): Could not create 'FSID' field in layer of the output data source");
		}
		if (coneLayer->CreateField(&startFreq) != OGRERR_NONE)
		{
			throw std::runtime_error("AfcManager::exportGUIjson(): Could not create 'startFreq' field in layer of the output data source");
		}
		if (coneLayer->CreateField(&stopFreq) != OGRERR_NONE)
		{
			throw std::runtime_error("AfcManager::exportGUIjson(): Could not create 'stopFreq' field in layer of the output data source");
		}

		// Calculate the cones in iterative loop
		LatLon FSLatLonVal, posPointLatLon, negPointLatLon;
		int FSID;
		for (const auto &ulsIdx : _ulsIdxList)
		{
			// Instantiate cone object
			std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> coneFeature(OGRFeature::CreateFeature(coneLayer->GetLayerDefn()));

			// Grab FSID for storing with coverage polygon
			FSID = (*_ulsList)[ulsIdx]->getID();

			// Compute the beam coordinates and store into DoublePairs
			std::tie(FSLatLonVal, posPointLatLon, negPointLatLon) = computeBeamConeLatLon((*_ulsList)[ulsIdx]);

			// Intantiate unique-pointers to OGRPolygon and OGRLinearRing for storing the beam coverage
			GdalHelpers::GeomUniquePtr<OGRPolygon> beamCone(GdalHelpers::createGeometry<OGRPolygon>()); // Use GdalHelpers.h templates to have unique pointers create these on the heap
			GdalHelpers::GeomUniquePtr<OGRLinearRing> exteriorOfCone(GdalHelpers::createGeometry<OGRLinearRing>());

			// Create OGRPoints to store the coordinates of the beam triangle
			// ***IMPORTANT NOTE: Coordinates stored as (Lon,Lat) here, required by geoJSON specifications
			GdalHelpers::GeomUniquePtr<OGRPoint> FSPoint(GdalHelpers::createGeometry<OGRPoint>());
			FSPoint->setX(FSLatLonVal.second);
			FSPoint->setY(FSLatLonVal.first); // Must set points manually since cannot construct while pointing at with unique pointer
			GdalHelpers::GeomUniquePtr<OGRPoint> posPoint(GdalHelpers::createGeometry<OGRPoint>());
			posPoint->setX(posPointLatLon.second);
			posPoint->setY(posPointLatLon.first);
			GdalHelpers::GeomUniquePtr<OGRPoint> negPoint(GdalHelpers::createGeometry<OGRPoint>());
			negPoint->setX(negPointLatLon.second);
			negPoint->setY(negPointLatLon.first);

			// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
			exteriorOfCone->addPoint(FSPoint.get()); // Using .get() gives access to the pointer without giving up ownership
			exteriorOfCone->addPoint(posPoint.get());
			exteriorOfCone->addPoint(negPoint.get());
			exteriorOfCone->addPoint(FSPoint.get()); // Adds this again just so that the polygon closes where it starts (at FS location)

			// Add exterior boundary of cone's polygon
			beamCone->addRingDirectly(exteriorOfCone.release()); // Points unique-pointer to null and gives up ownership of exteriorOfCone to beamCone

			// Add properties to the geoJSON features
			coneFeature->SetField("FSID", FSID);
			coneFeature->SetField("kind", "FS");
			coneFeature->SetField("startFreq", (*_ulsList)[ulsIdx]->getStartAllocFreq() / 1.0e6);
			coneFeature->SetField("stopFreq", (*_ulsList)[ulsIdx]->getStopAllocFreq() / 1.0e6);
			/* coneFeature->SetField("FS Lon", FSLatLonVal.second); coneFeature->SetField("FS lat", FSLatLonVal.first);*/

			// Add geometry to feature
			coneFeature->SetGeometryDirectly(beamCone.release());

			if (coneLayer->CreateFeature(coneFeature.release()) != OGRERR_NONE)
			{
				throw std::runtime_error("Could not add cone feature in layer of output data source");
			}
		}

		// add building database raster boundary if loaded
		addBuildingDatabaseTiles(coneLayer);

		// Allocation clean-up
		OGRDataSource::DestroyDataSource(ptrOutputDS.release()); // Remove the reference to the data source

		// Create file to be written to (creates a file, a json object, and a json document in order to store)
		std::ifstream tempFileStream(tempOutFilePath.toStdString(), std::ifstream::in);
		const std::string geoJsonCollection = slurp(tempFileStream);

		QJsonDocument geoJsonDoc = QJsonDocument::fromJson(QString::fromStdString(geoJsonCollection).toUtf8());
		QJsonObject geoJsonObj = geoJsonDoc.object();

		QJsonObject analysisJsonObj = 
		{
			{"geoJson", geoJsonObj},
			{"spectrumData", jsonSpectrumData(_channelList, _deviceDesc, _wlanMinFreq)},
			{"channelData", jsonChannelData(_channelList)},
			{"statusMessageList", generateStatusMessages(statusMessageList)}
		};

		outputDocument = QJsonDocument(analysisJsonObj);
		//QJsonDocument analysisJsonDoc(analysisJsonObj);
	}

	// Write analysis outputs to JSON file
	auto outputAnalysisFile = FileHelpers::open(exportJsonPath, QIODevice::WriteOnly);
	auto gzip_writer = new GzipStream(outputAnalysisFile.get());
	if (!gzip_writer->open(QIODevice::WriteOnly))
	{
		throw std::runtime_error("Gzip failed to open.");
	}
	gzip_writer->write(outputDocument.toJson());
	gzip_writer->close();
	LOGGER_DEBUG(logger) << "Output file written to " << outputAnalysisFile->fileName().toStdString();
}

// Convert channel data into a valid JSON Object
// represents the available channels for an analysis
// returns a QJson array with channel data contained
QJsonArray jsonChannelData(const std::vector<ChannelStruct> &channelList)
{
	// get a list of the bandwidths used
	auto initialPair = std::make_pair(channelList.front().bandwidth(), std::vector<ChannelStruct>());
	auto rlanBWList = std::vector<std::pair<int, std::vector<ChannelStruct>>> { initialPair };
	for (const ChannelStruct &channel : channelList)
	{
		bool found = false;
		for (auto &band : rlanBWList)
		{
			if (channel.bandwidth() == band.first)
			{
				band.second.push_back(channel);
				found = true;
				break;
			}
		}
		if (!found)
		{
			auto newPair = std::make_pair(channel.bandwidth(), std::vector<ChannelStruct> { channel });
			rlanBWList.push_back(newPair);
		}
	}

	QJsonArray array = QJsonArray();

	std::map<int, int> channelNameStart;
	channelNameStart[20] = 1;
	channelNameStart[40] = 3;
	channelNameStart[80] = 7;
	channelNameStart[160] = 15;
	std::map<int, int> channelNameStep;
	channelNameStep[20] = 4;
	channelNameStep[40] = 8;
	channelNameStep[80] = 16;
	channelNameStep[160] = 32;

	for (const auto &channelGroup : rlanBWList)
	{
		QJsonObject rowObj = QJsonObject();
		rowObj.insert(QString("channelWidth"), QJsonValue(channelGroup.first));
		QJsonArray channels = QJsonArray();

		LOGGER_DEBUG(logger) << "Adding Channel Width: " << channelGroup.first << " MHz" << '\n'
			<< "with " << channelGroup.second.size() << " channels";
		for (int chanIdx = 0; chanIdx < (int) channelGroup.second.size(); chanIdx++)
		{
			ChannelStruct props = channelGroup.second.at(chanIdx);

			std::string color;
			switch (props.availability)
			{
				case GREEN:
					color = "green";
					break;
				case YELLOW:
					color = "yellow";
					break;
				case RED:
					color = "red";
					break;
				case BLACK:
					color = "black";
					break;
				default:
					throw std::invalid_argument("Invalid channel color");
			}

			channels.push_back(QJsonObject{
					{"color", QJsonValue(color.c_str())},
					{"maxEIRP", QJsonValue(props.eirpLimit_dBm)},
					{"name", QJsonValue(channelNameStart.at(channelGroup.first) + channelNameStep.at(channelGroup.first) * chanIdx)}});
		}

		rowObj.insert(QString("channels"), channels);
		array.push_back(rowObj);
	}
	return array;
}

// Converts spectrum data into a valid PAWS response
QJsonObject jsonSpectrumData(const std::vector<ChannelStruct> &channelList, const QJsonObject &deviceDesc, const double &startFreq)
{
	// get a list of the bandwidths used and group the channels
	auto initialPair = std::make_pair(channelList.front().bandwidth(), std::vector<ChannelStruct>());
	auto rlanBWList = std::vector<std::pair<int, std::vector<ChannelStruct>>> { initialPair };
	for (const ChannelStruct &channel : channelList)
	{
		bool found = false;
		for (auto &band : rlanBWList)
		{
			if (channel.bandwidth() == band.first)
			{
				band.second.push_back(channel);
				found = true;
				break;
			}
		}
		if (!found) 
		{
			auto newPair = std::make_pair(channel.bandwidth(), std::vector<ChannelStruct> { channel });
			rlanBWList.push_back(newPair);
		}
	}

	QJsonArray spectra = QJsonArray();

	// iterate over bandwidths (rows)
	for (const auto &bandwidth : rlanBWList)
	{
		QJsonArray parentProfiles = QJsonArray();
		QJsonArray profiles = QJsonArray();
		// iterate over channels that use the same bandwidth
		for (ChannelStruct channel : bandwidth.second)
		{
			// add two points for each channel to make a step function
			profiles.push_back(QJsonObject({// start of channel
						{"hz", QJsonValue(channel.startFreqMHz * 1e6)},
						{"dbm", QJsonValue(channel.eirpLimit_dBm)}}));
			profiles.push_back(QJsonObject({// end of channel
						{"hz", QJsonValue(channel.stopFreqMHz * 1e6)},
						{"dbm", QJsonValue(channel.eirpLimit_dBm)}}));
		}

		parentProfiles.push_back(profiles);
		QJsonObject spectrum = QJsonObject{
			{ "resolutionBwHz", bandwidth.first * 1e6 },
			{ "profiles", parentProfiles }
			};
		spectra.push_back(spectrum);
	}

	// add properties to return object
	// most of it (esspecially for phase 1) is static
	QJsonObject result = QJsonObject{
		{"type", "AVAIL_SPECTRUM_RESP"},
			{"version", "1.0"},
			{"timestamp", ISO8601TimeUTC() },
			{"deviceDesc", deviceDesc},
			{"spectrumSpecs",
				QJsonArray{
					QJsonObject{
						{"rulesetInfo",
							QJsonObject
							{
								{"authority", "US"},
								{"rulesetId", "AFC-6GHZ-DEMO-1.0"}
							}
						},
						{"spectrumSchedules",
							QJsonArray
							{
								QJsonObject
								{
									{"eventTime",
										QJsonObject
										{
											{"startTime", ISO8601TimeUTC() },
											{"stopTime", ISO8601TimeUTC(1) }
										}
									},
									{"spectra", spectra} // add generated spectra here
								}
							}
						}
					}
				}
			}
	};

	return result;
}

/******************************************************************************************/
/**** AfcManager::readPopulationGrid                                                   ****/
/******************************************************************************************/
void AfcManager::readPopulationGrid()
{
    int regionIdx; // CONUS simulations typically treat CONUS as one region; left here in case of EU or other simulation

    if (_worldPopulationFile.empty()) {
	std::vector<std::string> regionNameIDList = split(_regionStr, ','); // Split apart comma-separarated regions in list
	_numRegion = regionNameIDList.size();

	for (regionIdx = 0; regionIdx < _numRegion; ++regionIdx)
	{
		std::vector<std::string> nameIDStrList = split(regionNameIDList[regionIdx], ':');
		if (nameIDStrList.size() != 2)
		{
			throw std::runtime_error(ErrStream() << "ERROR: Invalid name:ID string = \"" << regionNameIDList[regionIdx] << "\"");
		}
		_regionNameList.push_back(nameIDStrList[0]);
		_regionIDList.push_back(std::stoi(nameIDStrList[1]));
	} // Regions broken up in respective IDs

	// Population grid allocated to store population data ahead
	_popGrid = new PopGridClass(_densityThrUrban, _densityThrSuburban, _densityThrRural);

	// Reads given population data into the population grid
	_popGrid->readData(_popDensityFile, _regionNameList, _regionIDList,
			_popDensityNumLon, _popDensityResLon, _popDensityMinLon,
			_popDensityNumLat, _popDensityResLat, _popDensityMinLat);
	LOGGER_DEBUG(logger) << "Population grid read complete";
    } else {
        std::vector<std::string> regionPolygonFileStrList = split(_regionPolygonFileList, ',');
        _numRegion = regionPolygonFileStrList.size();
        std::vector<PolygonClass *> regionPolygonList;
        std::vector<std::tuple<double, double>> unused_lon_list;
        unused_lon_list.push_back(std::make_tuple(-180.0, 180.0));

        for(regionIdx=0; regionIdx<_numRegion; ++regionIdx) {
            PolygonClass *regionPolygon = new PolygonClass(regionPolygonFileStrList[regionIdx], _regionPolygonResolution);
            std::cout << "REGION: " << regionPolygon->name << " AREA: " << regionPolygon->comp_bdy_area() << std::endl;
            regionPolygonList.push_back(regionPolygon);
            int minx, maxx, miny, maxy;
            regionPolygon->comp_bdy_min_max(minx, maxx, miny, maxy);
            double minLon = (minx-1)*_regionPolygonResolution;
            while(minLon <  -180.0) { minLon += 360.0; }
            while(minLon >=  180.0) { minLon -= 360.0; }
            double maxLon = (maxx+1)*_regionPolygonResolution;
            while(maxLon <= -180.0) { maxLon += 360.0; }
            while(maxLon >   180.0) { maxLon -= 360.0; }

            int segIdx = 0;
            while(segIdx<(int) unused_lon_list.size()) {
                if (minLon < maxLon) {
                    if (    (maxLon <= std::get<0>(unused_lon_list[segIdx]))
                         || (minLon >= std::get<1>(unused_lon_list[segIdx])) ) {
                        // No overlap, do nothing
                        segIdx++;
                    } else if (    (maxLon >= std::get<1>(unused_lon_list[segIdx]))
                                && (minLon <= std::get<0>(unused_lon_list[segIdx])) ) {
                        // Remove segment
                        unused_lon_list.erase(unused_lon_list.begin() + segIdx);
                    } else if (minLon <= std::get<0>(unused_lon_list[segIdx])) {
                        // Clip bottom of segment
                        unused_lon_list[segIdx] = std::make_tuple(maxLon, std::get<1>(unused_lon_list[segIdx]));
                        segIdx++;
                    } else if (maxLon >= std::get<1>(unused_lon_list[segIdx])) {
                        // Clip top of segment
                        unused_lon_list[segIdx] = std::make_tuple(std::get<0>(unused_lon_list[segIdx]), minLon);
                        segIdx++;
                    } else {
                        // Split Segment
                        double minA = std::get<0>(unused_lon_list[segIdx]);
                        double maxA = std::get<1>(unused_lon_list[segIdx]);
                        unused_lon_list[segIdx] = std::make_tuple(minA, minLon);
                        unused_lon_list.insert(unused_lon_list.begin()+segIdx+1, std::make_tuple(maxLon, maxA));
                        segIdx+=2;
                    }
                } else if (minLon > maxLon) {
                    // Polygon wraps around discontinuity at +/- 180
                    if (    (maxLon <= std::get<0>(unused_lon_list[segIdx]))
                         && (minLon >= std::get<1>(unused_lon_list[segIdx])) ) {
                        // No overlap, do nothing
                        segIdx++;
                    } else if (    (maxLon >= std::get<1>(unused_lon_list[segIdx]))
                                || (minLon <= std::get<0>(unused_lon_list[segIdx])) ) {
                        // Remove segment
                        unused_lon_list.erase(unused_lon_list.begin() + segIdx);
                    } else if (maxLon > std::get<0>(unused_lon_list[segIdx])) {
                        // Clip bottom of segment
                        unused_lon_list[segIdx] = std::make_tuple(maxLon, std::get<1>(unused_lon_list[segIdx]));
                        segIdx++;
                    } else if (minLon < std::get<1>(unused_lon_list[segIdx])) {
                        // Clip top of segment
                        unused_lon_list[segIdx] = std::make_tuple(std::get<0>(unused_lon_list[segIdx]), minLon);
                        segIdx++;
                    } else {
                        // Wrap around case can't split segment
                        throw std::runtime_error(ErrStream() << "ERROR: Unable to compute polygon extents");
                    }
                }
            }
        }
        double populationDensityMinLon;
        if (unused_lon_list.size()) {
            populationDensityMinLon = std::get<0>(unused_lon_list[0]);
        } else {
            throw std::runtime_error(ErrStream() << "ERROR: region polygons wrap around entire 360 degrees");
        }

        int minN = (int) floor(populationDensityMinLon / _regionPolygonResolution + 0.5);
        int translateN = (int) floor(360.0 / _regionPolygonResolution + 0.5);

        for(regionIdx=0; regionIdx<_numRegion; ++regionIdx) {
            PolygonClass *regionPolygon = regionPolygonList[regionIdx];
            while(regionPolygon->bdy_pt_x[0][0] < minN) {
                regionPolygon->translate(translateN, 0);
            }
            while(regionPolygon->bdy_pt_x[0][0] > minN + translateN) {
                regionPolygon->translate(-translateN, 0);
            }
        }

        _popGrid = new PopGridClass(_worldPopulationFile, regionPolygonList, _regionPolygonResolution, _densityThrUrban, _densityThrSuburban, _densityThrRural);

        for(regionIdx=0; regionIdx<_numRegion; ++regionIdx) {
            PolygonClass *regionPolygon = regionPolygonList[regionIdx];
            delete regionPolygon;
        }
    }
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::getBandwidth                                               ****/
/**** Process emissions designator and return bandwidth in Hz.                         ****/
/******************************************************************************************/
double AfcManager::getBandwidth(std::string emissionsDesignator)
{
	std::size_t strpos;
	std::string ed = emissionsDesignator.substr(0, emissionsDesignator.size() - 3);
	bool found = false;
	std::string chList = "HKMGT";
	std::size_t chIdx;
	char *chptr;

	double scale;
	double sVal = 1.0;
	for (chIdx = 0; (chIdx < chList.size()) && (!found); chIdx++)
	{
		strpos = ed.find(chList.at(chIdx));
		if (strpos != std::string::npos)
		{
			ed.at(strpos) = '.';
			scale = sVal;
			found = true;
		}
		sVal *= 1000.0;
	}

	if (!found)
	{
		throw std::runtime_error(ErrStream() << "ERROR: Unable to get bandwidth from emissions designator \"" << emissionsDesignator << "\"");
	}

	double bandwidth = strtod(ed.c_str(), &chptr) * scale;

	return (bandwidth);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::getAngleFromDMS                                            ****/
/**** Process DMS string and return angle (lat or lon) in deg.                         ****/
/******************************************************************************************/
double AfcManager::getAngleFromDMS(std::string dmsStr)
{
	char *chptr;
	double angleDeg;

	bool error = false;

	std::size_t dashPosn1;
	std::size_t dashPosn2;
	std::size_t letterPosn;

	dashPosn1 = dmsStr.find('-');
	if ((dashPosn1 == std::string::npos) || (dashPosn1 == 0))
	{
		// Angle is in decimal format, not DMS
		angleDeg = strtod(dmsStr.c_str(), &chptr);
	}
	else
	{

		if (!error)
		{
			dashPosn2 = dmsStr.find('-', dashPosn1 + 1);
			if (dashPosn2 == std::string::npos)
			{
				error = true;
			}
		}

		double dVal, mVal, sVal;
		if (!error)
		{
			letterPosn = dmsStr.find_first_of("NEWS", dashPosn2 + 1);

			std::string dStr = dmsStr.substr(0, dashPosn1);
			std::string mStr = dmsStr.substr(dashPosn1 + 1, dashPosn2 - dashPosn1 - 1);
			std::string sStr = ((letterPosn == std::string::npos) ? dmsStr.substr(dashPosn2 + 1) : dmsStr.substr(dashPosn2 + 1, letterPosn - dashPosn2 - 1));

			dVal = strtod(dStr.c_str(), &chptr);
			mVal = strtod(mStr.c_str(), &chptr);
			sVal = strtod(sStr.c_str(), &chptr);
		}

		if (error)
		{
			throw std::runtime_error(ErrStream() << "ERROR: Unable to convert DMS string to angle, DMS string = \"" << dmsStr << "\"");
		}

		angleDeg = dVal + (mVal + sVal / 60.0) / 60.0;

		if (letterPosn != std::string::npos)
		{
			if ((dmsStr.at(letterPosn) == 'W') || (dmsStr.at(letterPosn) == 'S'))
			{
				angleDeg *= -1;
			}
		}
	}

	return (angleDeg);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::findULSAntenna()                                           ****/
/******************************************************************************************/
int AfcManager::findULSAntenna(std::string strval)
{
	int antIdx = -1;
	bool found = false;

	for (int aIdx = 0; (aIdx < (int)_ulsAntennaList.size()) && (!found); aIdx++)
	{
		if (std::string(_ulsAntennaList[aIdx]->get_strid()) == strval)
		{
			found = true;
			antIdx = aIdx;
		}
	}

	return (antIdx);
}
/******************************************************************************************/

/******************************************************************************************/
/**** AfcManager::findULSID                                                            ****/
/******************************************************************************************/
ULSClass *AfcManager::findULSID(int ulsID)
{
	int ulsIdx, ulsIdxA, ulsIdxB;
	int id, idA, idB;
	ULSClass *uls = (ULSClass *) NULL;

	bool found = false;

	ulsIdxA = 0;
	idA = (*_ulsList)[ulsIdxA]->getID();
	if (idA == ulsID) {
		found = true;
		uls = (*_ulsList)[ulsIdxA];
	} else if (ulsID < idA) {
		throw std::runtime_error(ErrStream() << "ERROR: Invalid FSID = " << ulsID);
	}

	ulsIdxB = _ulsList->getSize()-1;;
	idB = (*_ulsList)[ulsIdxB]->getID();
	if (idB == ulsID) {
		found = true;
		uls = (*_ulsList)[ulsIdxB];
	} else if (ulsID > idB) {
		throw std::runtime_error(ErrStream() << "ERROR: Invalid FSID = " << ulsID);
	}

	while((ulsIdxA + 1 < ulsIdxB) && (!found)) {
		ulsIdx = (ulsIdxA + ulsIdxB)/2;
		id = (*_ulsList)[ulsIdx]->getID();
		if (ulsID == id) {
			found = true;
			uls = (*_ulsList)[ulsIdx];
		} else if (ulsID > id) {
			ulsIdxA = ulsIdx;
			idA = id;
		} else {
			ulsIdxB = ulsIdx;
			idB = id;
		}
	}

	if (!found) {
		throw std::runtime_error(ErrStream() << "ERROR: Invalid FSID = " << ulsID);
	}

	return(uls);
}
/******************************************************************************************/

/******************************************************************************************/
/**** AfcManager::computeBeamConeLatLon                                                ****/
/******************************************************************************************/
// Calculates and stores the beam cone coordinates
std::tuple<LatLon, LatLon, LatLon> AfcManager::computeBeamConeLatLon(ULSClass *uls)
{
	// Store lat/lon from ULS class
	LatLon FSLatLonVal = std::make_pair(uls->getRxLatitudeDeg(), uls->getRxLongitudeDeg());

	// Obtain the angle in radians for 3 dB attenuation
	double theta_rad = uls->computeBeamWidth(3.0) * M_PI / 180;

	// Create beam cone lat/lons:
	// Start by grabbing vector connecting ULS Rx and Tx
	//Vector3 antennaPointing_ecef = uls->getAntennaPointing()*uls->getLinkDistance();
	Vector3 rxPosn = uls->getRxPosition(), txPosn = uls->getTxPosition();
	double linkDist_km = (uls->getLinkDistance()) / 1000.0;
	Vector3 zVec = (txPosn - rxPosn).normalized();   // Pointing vector direction
	Vector3 upVec = txPosn.normalized();             // ECEF vector pointing "up"
	Vector3 xVec = (upVec.cross(zVec)).normalized(); // Forces x to lay flat on the Earth's surface

	// Rotate the vectors in 2D to create beam triangle (add rxPosn back to make ECEF again)
	Vector3 posPoint_ecef = rxPosn + linkDist_km * (cos(theta_rad) * zVec + sin(theta_rad) * xVec);
	Vector3 negPoint_ecef = rxPosn + linkDist_km * (cos(theta_rad) * zVec - sin(theta_rad) * xVec);

	// Convert to lat/lon
	GeodeticCoord posPoint_lla = EcefModel::ecefToGeodetic(posPoint_ecef);
	GeodeticCoord negPoint_lla = EcefModel::ecefToGeodetic(negPoint_ecef);

	// Store in DoublePairs
	LatLon posPointLatLon = std::make_pair(posPoint_lla.latitudeDeg, posPoint_lla.longitudeDeg);
	LatLon negPointLatLon = std::make_pair(negPoint_lla.latitudeDeg, negPoint_lla.longitudeDeg);

	// Return as a tuple
	return std::make_tuple(FSLatLonVal, posPointLatLon, negPointLatLon);
}

/******************************************************************************************/
/**** inline function aciFn() used only in computeSpectralOverlap                      ****/
/******************************************************************************************/
inline double aciFn(double fMHz, double BMHz)
{
    double overlap;
    double fabsMHz = fabs(fMHz);
    int sign;

    if (fMHz < 0.0) {
        sign = -1;
    } else if (fMHz > 0.0) {
        sign = 1;
    } else {
        return 0.0;
    }

    // LOGGER_INFO(logger) << "ACIFN: ==========================";
    // LOGGER_INFO(logger) << "ACIFN: fMHz = " << fMHz;
    // LOGGER_INFO(logger) << "ACIFN: BMHz = " << BMHz;

    if (fabsMHz <= BMHz/2) {
        overlap = fabsMHz;
    } else {
        overlap = BMHz/2;
    }

    // LOGGER_INFO(logger) << "ACIFN: overlap_0 = " << overlap;

    if (fabsMHz > BMHz/2) {
        if (fabsMHz <= BMHz/2 + 1.0) {
            overlap += (1.0 - exp(log(10.0)*(BMHz-2*fabsMHz)))/(2*log(10.0));
        } else {
            overlap += 0.99/(2*log(10.0));
        }
    }

    // LOGGER_INFO(logger) << "ACIFN: overlap_1 = " << overlap;

    if (fabsMHz > BMHz/2 + 1.0) {
        if (fabsMHz <= BMHz) {
            overlap += exp(log(10.0)*(-6*BMHz+28.0)/(5*BMHz-10.0))*(exp(log(10.0)*((-8.0)/(5*BMHz-10.0))*(BMHz/2+1.0))-exp(log(10.0)*((-8.0*fabsMHz)/(5*BMHz-10.0))))/((8.0*log(10.0))/(5*BMHz-10.0));
        } else {
            overlap += exp(log(10.0)*(-6*BMHz+28.0)/(5*BMHz-10.0))*(exp(log(10.0)*((-8.0)/(5*BMHz-10.0))*(BMHz/2+1.0))-exp(log(10.0)*((-8.0*BMHz)/(5*BMHz-10.0))))/((8.0*log(10.0))/(5*BMHz-10.0));
        }
    }

    // LOGGER_INFO(logger) << "ACIFN: overlap_2 = " << overlap;

    if (fabsMHz > BMHz) {
        if (fabsMHz <= 3*BMHz/2) {
            overlap += exp(-log(10.0)*0.4)*(exp(log(10.0)*(-2.4)) - exp(log(10.0)*(-2.4*fabsMHz/BMHz)))/((2.4*log(10.0)/BMHz));
        } else {
            overlap += exp(-log(10.0)*0.4)*(exp(log(10.0)*(-2.4)) - exp(log(10.0)*(-3.6)))/((2.4*log(10.0)/BMHz));
        }
    }

    // LOGGER_INFO(logger) << "ACIFN: overlap_3 = " << overlap;

    overlap = sign*overlap/BMHz;

    // LOGGER_INFO(logger) << "ACIFN: overlap = " << overlap;
    // LOGGER_INFO(logger) << "ACIFN: ==========================";

// printf("ACIFN: %12.10f %12.10f %12.10f\n", fMHz, BMHz, overlap);


    return overlap;
}
/******************************************************************************************/

/******************************************************************************************/
/**** AfcManager::computeSpectralOverlap                                               ****/
/******************************************************************************************/
double AfcManager::computeSpectralOverlap(double sigStartFreq, double sigStopFreq, double rxStartFreq, double rxStopFreq, bool aciFlag) const
{
    double overlap;

    if (!aciFlag) {
        if ((sigStopFreq <= rxStartFreq) || (sigStartFreq >= rxStopFreq)) {
            overlap = 0.0;
        } else {
            double f1 = (sigStartFreq < rxStartFreq ? rxStartFreq : sigStartFreq);
            double f2 = (sigStopFreq > rxStopFreq ? rxStopFreq : sigStopFreq);
            overlap = (f2 - f1) / (sigStopFreq - sigStartFreq);
        }
    } else {
        if ((2*sigStopFreq-sigStartFreq <= rxStartFreq) || (2*sigStartFreq-sigStopFreq >= rxStopFreq)) {
            overlap = 0.0;
        } else {
            double BMHz = (sigStopFreq - sigStartFreq)*1.0e-6;
            double fStartMHz = (rxStartFreq - (sigStartFreq + sigStopFreq)/2)*1.0e-6;
            double fStopMHz  = (rxStopFreq  - (sigStartFreq + sigStopFreq)/2)*1.0e-6;
            overlap = aciFn(fStopMHz, BMHz) - aciFn(fStartMHz, BMHz);

        }
    }

    return (overlap);
}
/******************************************************************************************/

/******************************************************************************************/
/**** AfcManager::readULSData()                                                        ****/
/**** linkDirection: 0: RX                                                             ****/
/****                1: TX                                                             ****/
/****                2: RX and TX                                                      ****/
/******************************************************************************************/
void AfcManager::readULSData(std::string filename, PopGridClass *popGridVal, int linkDirection, double minFreq, double maxFreq, bool removeMobileFlag, CConst::SimulationEnum simulationFlag,
		const double& minLat, const double& maxLat, const double& minLon, const double& maxLon)
{
	LOGGER_INFO(logger) << "Reading ULS Data: " << filename;
	int linenum;
	//char *chptr;
	std::string str;
	std::string reasonIgnored;
	//std::size_t strpos;

        bool fixAnomalousEntries = false;

	int numIgnoreInvalid = 0;
	int numIgnoreOutOfBand = 0;
	int numIgnoreOutsideSimulationRegion = 0;
	int numIgnoreMobile = 0;
	int numFixed = 0;
	int numValid = 0;

	int fsid = -1;
	std::string callsign;
	std::string radioService;
	std::string entityName;
	std::string rxCallsign;
	int rxAntennaNumber;
	std::string frequencyAssigned;
	std::string emissionsDesignator;
	double startFreq, stopFreq;
	double bandwidth;
	double rxLatitudeDeg, rxLongitudeDeg;
	double rxGroundElevation;
	double rxHeightAboveTerrain;
	double txLatitudeDeg, txLongitudeDeg;
	double txGroundElevation;
	std::string txPolarization;
	double txHeightAboveTerrain;
	double rxGain;
	double txGain;
	double txEIRP;
	CConst::ULSAntennaTypeEnum rxAntennaType;
	CConst::ULSAntennaTypeEnum txAntennaType;
	// double operatingRadius;
	// double rxSensitivity;
	int mobileUnit;

	AntennaClass *rxAntenna = (AntennaClass *)NULL;
	AntennaClass *txAntenna = (AntennaClass *)NULL;
	double rxAntennaFeederLossDB;
	double fadeMarginDB;
	std::string status;

	ULSClass *uls;

	if (filename.empty())
	{
		throw std::runtime_error("ERROR: No ULS data file specified");
	}

	auto fs_anom_writer = GzipCsvWriter(_fsAnomFile);
	auto &fanom = fs_anom_writer.csv_writer;

	if (fanom) {
		fanom->writeRow({"FSID,CALLSIGN,RX_LATITUDE,RX_LONGITUDE,ANOMALY_DESCRIPTION\n"});
	}

	int numUrbanULS = 0;
	int numSuburbanULS = 0;
	int numRuralULS = 0;
	int numBarrenULS = 0;

	LOGGER_INFO(logger) << "Analysis Band: [" << minFreq << ", " << maxFreq << "]";

	linenum = 0;

	std::vector<UlsRecord> rows = std::vector<UlsRecord>();
	if (_analysisType == "ExclusionZoneAnalysis")
	{
		// can also use UlsDatabase::getFSById(QString, int) to get a single record only by Id
		UlsDatabase::loadFSById(QString::fromStdString(filename), rows, _exclusionZoneFSID);
	}
	else
	{
		UlsDatabase::loadUlsData(QString::fromStdString(filename), rows, minLat, maxLat, minLon, maxLon);
	}

	for (UlsRecord row : rows)
	{
		linenum++;
		bool ignoreFlag = false;
		bool fixedFlag = false;
		bool randPointingFlag = false;
		bool randTxPosnFlag = false;
		std::string fixedStr = "";
		char rxPropEnv, txPropEnv;

		radioService = row.radioService;
		entityName = row.entityName;
		for (auto &c : entityName)
			c = toupper(c);

		/**************************************************************************/
		/* FSID                                                                   */
		/**************************************************************************/
		fsid = row.fsid;
		/**************************************************************************/

		/**************************************************************************/
		/* channelBandwidth / emissionsDesignator => bandwidth                    */
		/**************************************************************************/
		if (!ignoreFlag)
		{
			emissionsDesignator = row.emissionsDesignator;

			if (emissionsDesignator.empty())
			{
                            if (fixAnomalousEntries) {
				if (radioService == "CF")
				{
					emissionsDesignator = "30MXXXX";
					fixedStr += "Fixed: missing emissions designator set to " + emissionsDesignator;
					fixedFlag = true;
				}
				else if (radioService == "CT")
				{
					emissionsDesignator = "10MXXXX";
					fixedStr += "Fixed: missing emissions designator set to " + emissionsDesignator;
					fixedFlag = true;
				}
				else if (radioService == "TP")
				{
					emissionsDesignator = "25MXXXX";
					fixedStr += "Fixed: missing emissions designator set to " + emissionsDesignator;
					fixedFlag = true;
				}
				else if (radioService == "TS")
				{
					emissionsDesignator = "25MXXXX";
					fixedStr += "Fixed: missing emissions designator set to " + emissionsDesignator;
					fixedFlag = true;
				}
				else if (radioService == "MW")
				{
					emissionsDesignator = "30MXXXX";
					fixedStr += "Fixed: missing emissions designator set to " + emissionsDesignator;
					fixedFlag = true;
				}
				else
				{
					ignoreFlag = true;
					reasonIgnored = "Ignored: Missing emission designator";
					numIgnoreInvalid++;
				}
                            } else {
                                ignoreFlag = true;
                                reasonIgnored = "Ignored: Missing emission designator";
                                numIgnoreInvalid++;
                            }
			}
		}

		if (!ignoreFlag)
		{
			bandwidth = getBandwidth(emissionsDesignator);

			if (bandwidth == 0.0)
			{
                            if (fixAnomalousEntries) {
				if (radioService == "CF")
				{
					bandwidth = 30.0e6;
					fixedStr += "Fixed: emissions designator specifies bandwidth = 0: set to 30 MHz";
					fixedFlag = true;
				}
				else if (radioService == "CT")
				{
					bandwidth = 10.0e6;
					fixedStr += "Fixed: emissions designator specifies bandwidth = 0: set to 10 MHz";
					fixedFlag = true;
				}
				else if (radioService == "TP")
				{
					bandwidth = 25.0e6;
					fixedStr += "Fixed: emissions designator specifies bandwidth = 0: set to 25 MHz";
					fixedFlag = true;
				}
				else if (radioService == "TS")
				{
					bandwidth = 25.0e6;
					fixedStr += "Fixed: emissions designator specifies bandwidth = 0: set to 25 MHz";
					fixedFlag = true;
				}
				else
				{
					ignoreFlag = true;
					reasonIgnored = "Ignored: emission designator specifies bandwidth = 0";
					numIgnoreInvalid++;
				}
                            } else {
                                ignoreFlag = true;
                                reasonIgnored = "Ignored: emission designator specifies bandwidth = 0";
                                numIgnoreInvalid++;
                            }
			}
		}
		/**************************************************************************/

		/**************************************************************************/
		/* callsign, rxCallsign                                                   */
		/**************************************************************************/
		callsign = row.callsign;
		rxCallsign = row.rxCallsign;
		/**************************************************************************/

		/**************************************************************************/
		/* rxAntennaNumber                                                        */
		/**************************************************************************/
		rxAntennaNumber = row.rxAntennaNumber;
		/**************************************************************************/

		/**************************************************************************/
		/* frequencyAssigned => startFreq, stopFreq                               */
		/**************************************************************************/

		if (!_filterSimRegionOnly)
		{
			if (!ignoreFlag)
			{
				if (row.startFreq == row.stopFreq)
				{
					double cf = row.startFreq * 1.0e6;
					startFreq = cf - bandwidth / 2;
					stopFreq = cf + bandwidth / 2;
				}
				else
				{
					startFreq = row.startFreq * 1.0e6;
					stopFreq = row.stopFreq * 1.0e6;
				}

				if (stopFreq < startFreq)
				{
					throw std::runtime_error(ErrStream() << "ERROR reading ULS data: FSID = " << fsid << ", startFreq = " << startFreq << ", stopFreq = " << stopFreq << ", must have startFreq < stopFreq");
				}

				if (stopFreq - startFreq < bandwidth - 1.0e-3)
				{
                                    if (fixAnomalousEntries) {
					if (callsign == "WLF419")
					{
						bandwidth = 17.0e6;
						double cf = (startFreq + stopFreq) / 2;
						startFreq = cf - bandwidth / 2;
						stopFreq = cf + bandwidth / 2;
						fixedStr += "Fixed: frequency assigned less than bandwidth: WLF419 bandwidth set to 17.0 MHz";
						fixedFlag = true;
					}
					else if ((callsign == "WHY789") || (callsign == "WPVI710") || (radioService == "CF"))
					{
						double cf = (startFreq + stopFreq) / 2;
						startFreq = cf - bandwidth / 2;
						stopFreq = cf + bandwidth / 2;
						fixedStr += "Fixed: frequency assigned less than bandwidth: " + callsign + " frequency range expanded to accomodate bandwidth";
						fixedFlag = true;
					}
					else
					{
						ignoreFlag = true;
						reasonIgnored = "frequency assigned less than bandwidth";
						numIgnoreInvalid++;
					}
                                    } else {
                                        ignoreFlag = true;
                                        reasonIgnored = "frequency assigned less than bandwidth";
                                        numIgnoreInvalid++;
                                    }
				}
			}

			if (!ignoreFlag)
			{
				if ((stopFreq <= minFreq) || (startFreq >= maxFreq))
				{
					ignoreFlag = true;
					reasonIgnored = "out of analysis band";
					numIgnoreOutOfBand++;
				}
			}
		}
		/**************************************************************************/

		/**************************************************************************/
		/* Remove mobile ULS entries                                              */
		/**************************************************************************/
		if ((!_filterSimRegionOnly) && (removeMobileFlag) && (!ignoreFlag))
		{
			if ((row.mobile || (radioService == "TP") || ((startFreq < 6525.0e6) && (stopFreq > 6425.0e6))))
			{
				ignoreFlag = true;
				reasonIgnored = "Mobile ULS entry";
				numIgnoreMobile++;
			}
		}
		/**************************************************************************/

		/**************************************************************************/
		/* rxLatCoords => rxLatitudeDeg                                           */
		/**************************************************************************/
		if (!ignoreFlag)
		{
			rxLatitudeDeg = row.rxLatitudeDeg;

			if (rxLatitudeDeg == 0.0)
			{
				if ((linkDirection == 0) || (linkDirection == 2))
				{
					ignoreFlag = true;
					reasonIgnored = "RX Latitude has value 0";
					numIgnoreInvalid++;
				}
				else if (linkDirection == 1)
				{
					// randPointingFlag = true;
					// fixedStr += "Fixed: Rx Latitude has value 0: using random direction for Tx antenna";
					// fixedFlag = true;

					reasonIgnored = "Ignored: Rx Latitude has value 0";
					ignoreFlag = true;
					numIgnoreInvalid++;
				}
				else
				{
					throw std::runtime_error(ErrStream() << "ERROR reading ULS data: linkDirection = " << linkDirection << " INVALID value");
				}
			}
		}
		/**************************************************************************/

		/**************************************************************************/
		/* rxLongCoords => rxLongitudeDeg                                         */
		/**************************************************************************/
		if (!randPointingFlag)
		{
			if (!ignoreFlag)
			{
				rxLongitudeDeg = row.rxLongitudeDeg;

				if (rxLongitudeDeg == 0.0)
				{
					if ((linkDirection == 0) || (linkDirection == 2))
					{
						ignoreFlag = true;
						reasonIgnored = "RX Longitude has value 0";
					}
					else if (linkDirection == 1)
					{
						// randPointingFlag = true;
						// fixedStr += "Fixed: Rx Longitude has value 0: using random direction for Tx antenna";
						// fixedFlag = true;

						reasonIgnored = "Ignored: Rx Longitude has value 0";
						ignoreFlag = true;
						numIgnoreInvalid++;
					}
					numIgnoreInvalid++;
				}
			}
		}
		/**************************************************************************/

		/**************************************************************************/
		/* Check rxLatitude and rxLongitude region defined by popGrid (SIMULATION REGION)     */
		/**************************************************************************/
		if ((!ignoreFlag) && ((linkDirection == 0) || (linkDirection == 2)) && (popGridVal))
		{
			int lonIdx;
			int latIdx;
			int regionIdx;
			popGridVal->findDeg(rxLongitudeDeg, rxLatitudeDeg, lonIdx, latIdx, rxPropEnv, regionIdx);
			if ((rxPropEnv == (char)NULL) || (rxPropEnv == 'X'))
			{
				ignoreFlag = true;
				reasonIgnored = "RX outside SIMULATION REGION";
				numIgnoreOutsideSimulationRegion++;
			}
		}
		/**************************************************************************/

		/**************************************************************************/
		/* rxGroundElevation                                                      */
		/**************************************************************************/
		rxGroundElevation = row.rxGroundElevation; // could be NaN
		/**************************************************************************/

		/**************************************************************************/
		/* rxHeightAboveTerrain                                                   */
		/**************************************************************************/
		if (!_filterSimRegionOnly)
		{
			rxHeightAboveTerrain = row.rxHeightAboveTerrain;
			if (!ignoreFlag)
			{
				if (std::isnan(rxHeightAboveTerrain))
				{
				    bool fixedMissingRxHeight = false;
                                    if (fixAnomalousEntries) {
					if (!std::isnan(row.txHeightAboveTerrain))
					{
						txHeightAboveTerrain = row.txHeightAboveTerrain;
						if (txHeightAboveTerrain > 0.0)
						{
							rxHeightAboveTerrain = row.txHeightAboveTerrain;
							fixedStr += "Fixed: missing Rx Height above Terrain set to Tx Height above Terrain";
							fixedMissingRxHeight = true;
							fixedFlag = true;
						}
						else if (txHeightAboveTerrain == 0.0)
						{
							rxHeightAboveTerrain = 0.1;
							fixedStr += "Fixed: missing Rx Height above Terrain set to " + std::to_string(rxHeightAboveTerrain);
							fixedMissingRxHeight = true;
							fixedFlag = true;
						}
					}
					else
					{
						if (radioService == "CF")
						{
							rxHeightAboveTerrain = 39.3;
							fixedStr += "Fixed: missing Rx Height above Terrain for " + radioService + " set to " + std::to_string(rxHeightAboveTerrain);
							fixedMissingRxHeight = true;
							fixedFlag = true;
						}
						else if (radioService == "MG")
						{
							rxHeightAboveTerrain = 41.0;
							fixedStr += "Fixed: missing Rx Height above Terrain for " + radioService + " set to " + std::to_string(rxHeightAboveTerrain);
							fixedMissingRxHeight = true;
							fixedFlag = true;
						}
						else if (radioService == "MW")
						{
							rxHeightAboveTerrain = 39.9;
							fixedStr += "Fixed: missing Rx Height above Terrain for " + radioService + " set to " + std::to_string(rxHeightAboveTerrain);
							fixedMissingRxHeight = true;
							fixedFlag = true;
						}
						else if (radioService == "TI")
						{
							rxHeightAboveTerrain = 41.8;
							fixedStr += "Fixed: missing Rx Height above Terrain for " + radioService + " set to " + std::to_string(rxHeightAboveTerrain);
							fixedMissingRxHeight = true;
							fixedFlag = true;
						}
						else if (radioService == "TP")
						{
							rxHeightAboveTerrain = 30.0;
							fixedStr += "Fixed: missing Rx Height above Terrain for " + radioService + " set to " + std::to_string(rxHeightAboveTerrain);
							fixedMissingRxHeight = true;
							fixedFlag = true;
						}
						else if (radioService == "TS")
						{
							rxHeightAboveTerrain = 41.5;
							fixedStr += "Fixed: missing Rx Height above Terrain for " + radioService + " set to " + std::to_string(rxHeightAboveTerrain);
							fixedMissingRxHeight = true;
							fixedFlag = true;
						}
						else if (radioService == "TT")
						{
							rxHeightAboveTerrain = 42.1;
							fixedStr += "Fixed: missing Rx Height above Terrain for " + radioService + " set to " + std::to_string(rxHeightAboveTerrain);
							fixedMissingRxHeight = true;
							fixedFlag = true;
						}
					}
				    }

				    if (!fixedMissingRxHeight)
				    {
				    	ignoreFlag = true;
				    	reasonIgnored = "missing Rx Height above Terrain";
				    	numIgnoreInvalid++;
				    }
				}
			}

			if (!ignoreFlag)
			{
				if (rxHeightAboveTerrain < 3.0)
				{
                                    if (fixAnomalousEntries) {
					rxHeightAboveTerrain = 3.0;
					fixedStr += "Fixed: Rx Height above Terrain < 3.0 set to 3.0";
					fixedFlag = true;
                                    } else {
                                        LOGGER_WARN(logger) << "WARNING: ULS data for FSID = " << fsid << ", rxHeightAboveTerrain = " << rxHeightAboveTerrain << " is < 3.0";
                                    }
				}
			}
		}
		/**************************************************************************/

		/**************************************************************************/
		/* txLatCoords => txLatitudeDeg                                           */
		/**************************************************************************/
		txLatitudeDeg = 0.0;
		if (!_filterSimRegionOnly)
		{
			if (!ignoreFlag)
			{
				txLatitudeDeg = row.txLatitudeDeg;

				if (txLatitudeDeg == 0.0)
				{
					if (linkDirection == 0)
					{
						// randPointingFlag = true;
						// fixedStr += "Fixed: Tx Latitude has value 0: using random direction for Rx antenna";
						// fixedFlag = true;

						reasonIgnored = "Ignored: Tx Latitude has value 0";
						ignoreFlag = true;
						numIgnoreInvalid++;
					}
					else if ((linkDirection == 1) || (linkDirection == 2))
					{
						if (simulationFlag == CConst::FSToFSSimulation)
						{
							ignoreFlag = true;
							reasonIgnored = "TX Latitude has value 0";
						}
						else
						{
							// randTxPosnFlag = true;
							// fixedStr += "Fixed: Tx Latitude has value 0: using random posn in SIMULATION REGION for Tx antenna";
							// fixedFlag = true;

							reasonIgnored = "Ignored: Tx Latitude has value 0";
							ignoreFlag = true;
							numIgnoreInvalid++;
						}
					}
					else
					{
						throw std::runtime_error(ErrStream() << "ERROR reading ULS data: linkDirection = " << linkDirection << " INVALID value");
					}
				}
			}
		}
		/**************************************************************************/

		/**************************************************************************/
		/* txLongCoords => txLongitudeDeg                                         */
		/**************************************************************************/
		txLongitudeDeg = 0.0;
		if (!_filterSimRegionOnly)
		{
			if ((!randPointingFlag) && (!randTxPosnFlag))
			{

				if (!ignoreFlag)
				{
					txLongitudeDeg = row.txLongitudeDeg;

					if (txLongitudeDeg == 0.0)
					{
						if (linkDirection == 0)
						{
							// randPointingFlag = true;
							// fixedStr += "Fixed: Tx Longitude has value 0: using random direction for Rx antenna";
							// fixedFlag = true;

							reasonIgnored = "Ignored: Tx Longitude has value 0";
							ignoreFlag = true;
							numIgnoreInvalid++;
						}
						else if ((linkDirection == 1) || (linkDirection == 2))
						{
							if (simulationFlag == CConst::FSToFSSimulation)
							{
								ignoreFlag = true;
								reasonIgnored = "TX Longitude has value 0";
							}
							else
							{
								// randTxPosnFlag = true;
								// fixedStr += "Fixed: Tx Longitude has value 0: using random posn in SIMULATION REGION for Tx antenna";
								// fixedFlag = true;

								reasonIgnored = "Ignored: Tx Longitude has value 0";
								ignoreFlag = true;
								numIgnoreInvalid++;
							}
						}
						else
						{
							throw std::runtime_error(ErrStream() << "ERROR reading ULS data: linkDirection = " << linkDirection << " INVALID value");
						}
					}
				}
			}
		}
		/**************************************************************************/

		/**************************************************************************/
		/* txGroundElevation                                                      */
		/**************************************************************************/
		txGroundElevation = row.txGroundElevation; // may be NaN
		/**************************************************************************/

		/**************************************************************************/
		/* txPolarization                                                         */
		/**************************************************************************/
		txPolarization = row.txPolarization;
		/**************************************************************************/

		/**************************************************************************/
		/* txHeightAboveTerrain                                                   */
		/**************************************************************************/
		if (!_filterSimRegionOnly)
		{
			txHeightAboveTerrain = row.txHeightAboveTerrain;
			if (!ignoreFlag)
			{
				if (std::isnan(txHeightAboveTerrain))
				{
				    bool fixedMissingTxHeight = false;
                                    if (fixAnomalousEntries) {
					if (radioService == "CF")
					{
						txHeightAboveTerrain = 38.1;
						fixedStr += "Fixed: missing Tx Height above Terrain for " + radioService + " set to " + std::to_string(txHeightAboveTerrain);
						fixedMissingTxHeight = true;
						fixedFlag = true;
					}
					else if (radioService == "MW")
					{
						txHeightAboveTerrain = 38.1;
						fixedStr += "Fixed: missing Tx Height above Terrain for " + radioService + " set to " + std::to_string(txHeightAboveTerrain);
						fixedMissingTxHeight = true;
						fixedFlag = true;
					}
					else if (radioService == "TI")
					{
						txHeightAboveTerrain = 38.1;
						fixedStr += "Fixed: missing Tx Height above Terrain for " + radioService + " set to " + std::to_string(txHeightAboveTerrain);
						fixedMissingTxHeight = true;
						fixedFlag = true;
					}
					else if (radioService == "TP")
					{
						txHeightAboveTerrain = 6.1;
						fixedStr += "Fixed: missing Tx Height above Terrain for " + radioService + " set to " + std::to_string(txHeightAboveTerrain);
						fixedMissingTxHeight = true;
						fixedFlag = true;
					}
					else if (radioService == "TS")
					{
						txHeightAboveTerrain = 30.5;
						fixedStr += "Fixed: missing Tx Height above Terrain for " + radioService + " set to " + std::to_string(txHeightAboveTerrain);
						fixedMissingTxHeight = true;
						fixedFlag = true;
					}
                                    }

				    if (!fixedMissingTxHeight)
				    {
				    	ignoreFlag = true;
				    	reasonIgnored = "missing Tx Height above Terrain";
				    	numIgnoreInvalid++;
				    }
				}
			}

			if (!ignoreFlag)
			{
				if (txHeightAboveTerrain <= 0.0)
				{
                                    if (fixAnomalousEntries) {
					txHeightAboveTerrain = 0.1;
					fixedStr += "Fixed: Tx Height above Terrain <= 0 set to 0.1";
					fixedFlag = true;
                                    } else {
                                        LOGGER_WARN(logger) << "WARNING: ULS data for FSID = " << fsid << ", txHeightAboveTerrain = " << txHeightAboveTerrain << " is < 0.0";
                                    }
				}
			}
		}
		/**************************************************************************/

		if (!_filterSimRegionOnly)
		{
			if ((!ignoreFlag) && (randTxPosnFlag))
			{
				int numLon = popGridVal->getNumLon();
				int numLat = popGridVal->getNumLat();
				int lonIdx, latIdx;
				char propEnv;
				do
				{
					lonIdx = (int)floor(((double)rand() / (RAND_MAX + 1.0)) * numLon);
					latIdx = (int)floor(((double)rand() / (RAND_MAX + 1.0)) * numLat);
					propEnv = popGridVal->getPropEnv(lonIdx, latIdx);
				} while (propEnv != 'X');
				popGridVal->getLonLatDeg(lonIdx, latIdx, txLongitudeDeg, txLatitudeDeg);
				randPointingFlag = true;
			}
		}

		/**************************************************************************/
		/* Check txLatitude and txLongitude region defined by popGrid (SIMULATION REGION)     */
		/**************************************************************************/
		if ((!ignoreFlag) && ((linkDirection == 1) || (linkDirection == 2)) && popGridVal)
		{
			int lonIdx;
			int latIdx;
			int regionIdx;
			popGridVal->findDeg(txLongitudeDeg, txLatitudeDeg, lonIdx, latIdx, txPropEnv, regionIdx);
			if ((txPropEnv == (char)NULL) || (txPropEnv == 'X'))
			{
				ignoreFlag = true;
				reasonIgnored = "TX outside SIMULATION REGION";
				numIgnoreOutsideSimulationRegion++;
			}
		}
		/**************************************************************************/

		if (!_filterSimRegionOnly)
		{
			if ((linkDirection == 0) || (linkDirection == 2) || (simulationFlag == CConst::MobileSimulation))
			{
				/**************************************************************************/
				/* rxGain                                                                 */
				/**************************************************************************/
				rxGain = row.rxGain;
				if (!ignoreFlag)
				{
					if (std::isnan(rxGain))
					{
                                            if (fixAnomalousEntries) {
						if (radioService == "CF")
						{
							rxGain = 39.3;
							fixedStr += "Fixed: missing Rx Gain for " + radioService + " gain set to " + std::to_string(rxGain);
							fixedFlag = true;
						}
						else if (radioService == "MG")
						{
							rxGain = 41.0;
							fixedStr += "Fixed: missing Rx Gain for " + radioService + " gain set to " + std::to_string(rxGain);
							fixedFlag = true;
						}
						else if (radioService == "MW")
						{
							rxGain = 39.9;
							fixedStr += "Fixed: missing Rx Gain for " + radioService + " gain set to " + std::to_string(rxGain);
							fixedFlag = true;
						}
						else if (radioService == "TI")
						{
							rxGain = 41.8;
							fixedStr += "Fixed: missing Rx Gain for " + radioService + " gain set to " + std::to_string(rxGain);
							fixedFlag = true;
						}
						else if (radioService == "TP")
						{
							rxGain = 30.0;
							fixedStr += "Fixed: missing Rx Gain for " + radioService + " gain set to " + std::to_string(rxGain);
							fixedFlag = true;
						}
						else if (radioService == "TS")
						{
							rxGain = 41.5;
							fixedStr += "Fixed: missing Rx Gain for " + radioService + " gain set to " + std::to_string(rxGain);
							fixedFlag = true;
						}
						else if (radioService == "TT")
						{
							rxGain = 42.1;
							fixedStr += "Fixed: missing Rx Gain for " + radioService + " gain set to " + std::to_string(rxGain);
							fixedFlag = true;
						}
						else if (radioService == "TB")
						{
							rxGain = 40.7;
							fixedStr += "Fixed: missing Rx Gain for " + radioService + " gain set to " + std::to_string(rxGain);
							fixedFlag = true;
						}
						else
						{
							ignoreFlag = true;
							reasonIgnored = "missing Rx Gain";
							numIgnoreInvalid++;
						}
                                            } else {
                                                ignoreFlag = true;
                                                reasonIgnored = "missing Rx Gain";
                                                numIgnoreInvalid++;
                                            }
					}
					else if ((callsign == "WQUY451") && (rxGain == 1.8))
					{
                                            if (fixAnomalousEntries) {
						rxGain = 39.3;
						fixedStr += "Fixed: anomalous Rx Gain for " + callsign + " changed from 1.8 to " + std::to_string(rxGain);
						fixedFlag = true;
                                            }
					}
				}

				if (!ignoreFlag)
				{
					if (rxGain < 10.0)
					{
                                            if (fixAnomalousEntries) {
						if (radioService == "CF")
						{
							rxGain = 39.3;
							fixedStr += "Fixed: invalid Rx Gain " + std::to_string(rxGain) + " for " + radioService + " set to 39.3";
							fixedFlag = true;
						}
						else if (radioService == "MG")
						{
							rxGain = 41.0;
							fixedStr += "Fixed: invalid Rx Gain " + std::to_string(rxGain) + " for " + radioService + " set to 41.0";
							fixedFlag = true;
						}
						else if (radioService == "MW")
						{
							rxGain = 39.9;
							fixedStr += "Fixed: invalid Rx Gain " + std::to_string(rxGain) + " for " + radioService + " set to 39.9";
							fixedFlag = true;
						}
						else if (radioService == "TI")
						{
							rxGain = 41.8;
							fixedStr += "Fixed: invalid Rx Gain " + std::to_string(rxGain) + " for " + radioService + " set to 41.8";
							fixedFlag = true;
						}
						else if (radioService == "TS")
						{
							rxGain = 41.5;
							fixedStr += "Fixed: invalid Rx Gain " + std::to_string(rxGain) + " for " + radioService + " set to 41.5";
							fixedFlag = true;
						}
						else
						{
							ignoreFlag = true;
							reasonIgnored = "invalid Rx Gain";
							numIgnoreInvalid++;
						}
                                            } else {
						ignoreFlag = true;
						reasonIgnored = "invalid Rx Gain";
						numIgnoreInvalid++;
                                            }
					}
				}
				/**************************************************************************/

				/**************************************************************************/
				/* rxAntenna                                                              */
				/**************************************************************************/
				if (!ignoreFlag) {
					if ((_ulsAntennaList.size() == 0)||(row.rxAntennaModel.empty())) {
						rxAntennaType = CConst::F1245AntennaType;
					} else {
						std::string strval = row.rxAntennaModel;

						int ulsAntIdx = findULSAntenna(strval);
						if (ulsAntIdx != -1) {
							LOGGER_DEBUG(logger) << "Antenna Found " << fsid << ": " << strval;
							rxAntennaType = CConst::LUTAntennaType;
							rxAntenna = _ulsAntennaList[ulsAntIdx];
						} else {
							int validFlag;
							rxAntennaType = (CConst::ULSAntennaTypeEnum) CConst::strULSAntennaTypeList->str_to_type(strval, validFlag, 0);
							rxAntenna = (AntennaClass *) NULL;
							if (!validFlag) {
								std::ostringstream errStr;
								errStr << "Invalid ULS data for FSID = " << fsid << ", Unknown Rx Antenna \"" << strval << "\" using F.1245";
								LOGGER_WARN(logger) << errStr.str();
								statusMessageList.push_back(errStr.str());

								rxAntennaType = CConst::F1245AntennaType;
							}
						}
					}
				}
				/**************************************************************************/

				/**************************************************************************/
				/* rxFeederLoss                                                           */
				/**************************************************************************/

				// ULS db does not have rxAntennaFeederLossDB so set this as default
				rxAntennaFeederLossDB = 0.0;

				/**************************************************************************/

				/**************************************************************************/
				/* fadeMargin                                                             */
				/**************************************************************************/

				// ULS Db does not have fadeMargin field so set this as default
				fadeMarginDB = -1.0;

				/**************************************************************************/
			}
		}

		if (!_filterSimRegionOnly)
		{
			if ((linkDirection == 1) || (linkDirection == 2) || (simulationFlag == CConst::MobileSimulation) || (simulationFlag == CConst::RLANSensingSimulation) || (simulationFlag == CConst::showFSPwrAtRLANSimulation))
			{
				/**************************************************************************/
				/* txGain                                                                 */
				/**************************************************************************/
				txGain = row.txGain;
				if (!ignoreFlag)
				{
					if (std::isnan(txGain))
					{
                                            if (fixAnomalousEntries) {
						if (radioService == "CF")
						{
							txGain = 39.3;
							fixedStr += "Fixed: missing Tx Gain for " + radioService + " gain set to " + std::to_string(txGain);
							fixedFlag = true;
						}
						else
						{
							ignoreFlag = true;
							reasonIgnored = "missing Tx Gain";
							numIgnoreInvalid++;
						}
                                            } else {
                                                ignoreFlag = true;
                                                reasonIgnored = "missing Tx Gain";
                                                numIgnoreInvalid++;
                                            }
					}
				}

				/**************************************************************************/

				/**************************************************************************/
				/* txEIRP                                                                 */
				/**************************************************************************/
				txEIRP = row.txEIRP;
				if (!ignoreFlag)
				{
					if (std::isnan(txEIRP))
					{
						if (fixAnomalousEntries && (radioService == "CF"))
						{
							txEIRP = 66;
							fixedStr += "Fixed: missing txEIRP set to " + std::to_string(txEIRP) + " dBm";
							fixedFlag = true;
						}
						else
						{
							ignoreFlag = true;
							reasonIgnored = "missing Tx EIRP";
							numIgnoreInvalid++;
						}
					}
				}

				if (!ignoreFlag)
				{
					txEIRP = txEIRP - 30; // Convert dBm to dBW

					if (txEIRP >= 80.0)
					{
                                            if (fixAnomalousEntries) {
						txEIRP = 39.3;
						fixedStr += "Fixed: Tx EIRP > 80 dBW set to 39.3 dBW";
						fixedFlag = true;
                                            } else {
                                                LOGGER_WARN(logger) << "WARNING: ULS data for FSID = " << fsid << ", txEIRP = " << txEIRP << " (dBW) is >= 80.0";
                                            }
					}
				}
				/**************************************************************************/

				/**************************************************************************/
				/* txAntenna                                                              */
				/**************************************************************************/

				// ULS db does not have txAntenna field so use these as defaults
				txAntennaType = CConst::F1245AntennaType;
				// phase 1 does not implement this because no antenna pattern file is specified
				//int ulsAntIdx = findULSAntenna("F.1245");
				//txAntenna = _ulsAntennaList[ulsAntIdx];

				/**************************************************************************/
			}
		}

		/**************************************************************************/
		/* status                                                                 */
		/**************************************************************************/
		status = row.status;
		/**************************************************************************/

		if (!_filterSimRegionOnly)
		{
			if (!ignoreFlag)
			{
				if ((rxLatitudeDeg == txLatitudeDeg) && (rxLongitudeDeg == txLongitudeDeg))
				{
					// randPointingFlag = true;
					// fixedStr += "Fixed: RX and TX LON/LAT values are identical: using random direction for Rx antenna";
					// fixedFlag = true;

					reasonIgnored = "Ignored: RX and TX LON/LAT values are identical";
					ignoreFlag = true;
					numIgnoreInvalid++;
				}
			}

			if (!ignoreFlag)
			{
				if (rxGain > 80.0)
				{
                                    if (fixAnomalousEntries) {
					rxGain = 30.0;
					fixedStr += "Fixed: RX Gain > 80 dB: set to 30 dB";
					fixedFlag = true;
                                    } else {
                                        LOGGER_WARN(logger) << "WARNING: ULS data for FSID = " << fsid << ", rxGain = " << rxGain << " is > 80.0";
                                    }
				}
			}
		}

		if ((!ignoreFlag) && (!fixedFlag))
		{
			numValid++;
		}
		else if ((!ignoreFlag) && (fixedFlag))
		{
			numFixed++;
		}

		if (!ignoreFlag)
		{
if ( (fabs(rxLongitudeDeg-(-73.79756))<1.0e-4) && (fabs(rxLatitudeDeg-(40.75925))<1.0e-4) ) {
printf("HELLO\n");
}

			uls = new ULSClass(this, fsid);
			_ulsList->append(uls);
			uls->setCallsign(callsign);
			uls->setRxCallsign(rxCallsign);
			uls->setRxAntennaNumber(rxAntennaNumber);
			uls->setRadioService(radioService);
			uls->setEntityName(entityName);
			uls->setStartAllocFreq(startFreq);
			uls->setStopAllocFreq(stopFreq);
			uls->setBandwidth(bandwidth);
			uls->setRxGroundElevation(rxGroundElevation);
			uls->setRxLatitudeDeg(rxLatitudeDeg);
			uls->setRxLongitudeDeg(rxLongitudeDeg);
			uls->setTxGroundElevation(txGroundElevation);
			uls->setTxPolarization(txPolarization);
			uls->setTxLatitudeDeg(txLatitudeDeg);
			uls->setTxLongitudeDeg(txLongitudeDeg);
			uls->setRxGain(rxGain);
			uls->setRxAntennaType(rxAntennaType);
			uls->setTxAntennaType(txAntennaType);
			uls->setRxAntenna(rxAntenna);
			uls->setTxAntenna(txAntenna);
			uls->setTxGain(txGain);
			uls->setTxEIRP(txEIRP);
			uls->setUseFrequency();
			uls->setRxAntennaFeederLossDB(rxAntennaFeederLossDB);
			uls->setFadeMarginDB(fadeMarginDB);
			uls->setStatus(status);

			uls->setRxHeightAboveTerrain(rxHeightAboveTerrain);
			uls->setRxTerrainHeight(rxHeightAboveTerrain);

			uls->setTxHeightAboveTerrain(txHeightAboveTerrain);
			uls->setTxTerrainHeight(txHeightAboveTerrain);

			bool mobileRxFlag = ((simulationFlag == CConst::MobileSimulation) && (mobileUnit == 0));
			bool mobileTxFlag = ((simulationFlag == CConst::MobileSimulation) && (mobileUnit == 1));

			if (simulationFlag == CConst::MobileSimulation)
			{
				throw std::invalid_argument("Mobile simulation not supported");
#if 0
				uls->setOperatingRadius(operatingRadius);
				uls->setRxSensitivity(rxSensitivity);
				uls->setMobileUnit(mobileUnit);
				if (mobileRxFlag)
				{
					// Mobile RX
					uls->setOperatingCenterLongitudeDeg(rxLongitudeDeg);
					uls->setOperatingCenterLatitudeDeg(rxLatitudeDeg);
				}
				else if (mobileTxFlag)
				{
					// Mobile TX
					uls->setOperatingCenterLongitudeDeg(txLongitudeDeg);
					uls->setOperatingCenterLatitudeDeg(txLatitudeDeg);
				}
				uls->computeMobilePopGrid(popGridVal);
#endif
			}

			bool rxTerrainHeightFlag, txTerrainHeightFlag;
			double terrainHeight;
			double bldgHeight;
			MultibandRasterClass::HeightResult lidarHeightResult;
			CConst::HeightSourceEnum rxHeightSource;
			CConst::HeightSourceEnum txHeightSource;
			Vector3 rxPosition, txPosition;
			if (!mobileRxFlag)
			{
				if ((_terrainDataModel))
				{
					_terrainDataModel->getTerrainHeight(rxLongitudeDeg,rxLatitudeDeg, terrainHeight,bldgHeight, lidarHeightResult, rxHeightSource);
					rxTerrainHeightFlag = true;
				}
				else
				{
					rxTerrainHeightFlag = false;
					terrainHeight = 0.0;
				}
				uls->setRxTerrainHeightFlag(rxTerrainHeightFlag);
				double rxHeight = rxHeightAboveTerrain + terrainHeight;
				rxPosition = EcefModel::geodeticToEcef(rxLatitudeDeg, rxLongitudeDeg, rxHeight / 1000.0);
				uls->setRxPosition(rxPosition);
				uls->setRxTerrainHeight(terrainHeight);
				uls->setRxHeightAMSL(rxHeight);
				uls->setRxHeightSource(rxHeightSource);
			}

			if ((!mobileTxFlag) && (!randPointingFlag))
			{
				if ( (_terrainDataModel))
				{
					_terrainDataModel->getTerrainHeight(txLongitudeDeg,txLatitudeDeg, terrainHeight,bldgHeight, lidarHeightResult, txHeightSource);
					txTerrainHeightFlag = true;
				}
				else
				{
					txTerrainHeightFlag = false;
					terrainHeight = 0.0;
				}
				uls->setTxTerrainHeightFlag(txTerrainHeightFlag);
				double txHeight = txHeightAboveTerrain + terrainHeight;
				txPosition = EcefModel::geodeticToEcef(txLatitudeDeg, txLongitudeDeg, txHeight / 1000.0);
				uls->setTxPosition(txPosition);
				uls->setTxTerrainHeight(terrainHeight);
				uls->setTxHeightSource(txHeightSource);
				uls->setTxHeightAMSL(txHeight);

			}

			if ((!mobileRxFlag) && (!mobileTxFlag))
			{
				if (!randPointingFlag)
				{
					uls->setAntennaPointing((txPosition - rxPosition).normalized()); // Pointing of Rx antenna
					uls->setLinkDistance((txPosition - rxPosition).len() * 1000.0);
				}
				else
				{
					double az, el;
					Vector3 xvec, yvec, zvec;
					az = (((double)rand() / (RAND_MAX)) - 0.5) * 2 * M_PI;
					el = (((double)rand() / (RAND_MAX)) - 0.5) * 10 * M_PI / 180.0;
					zvec = ((linkDirection == 0) ? rxPosition.normalized() : rxPosition.normalized());
					xvec = (Vector3(zvec.y(), -zvec.x(), 0.0)).normalized();
					yvec = zvec.cross(xvec);
					uls->setAntennaPointing(zvec * sin(el) + (xvec * cos(az) + yvec * sin(az)) * cos(el));
					uls->setLinkDistance(-1.0);
				}
			}

			double noiseLevelDBW = 10.0 * log(CConst::boltzmannConstant * CConst::T0 * bandwidth) / log(10.0) + _ulsNoiseFigureDB;
			uls->setNoiseLevelDBW(noiseLevelDBW);
			if (fixedFlag) {
				fanom->writeRow({QString::asprintf("%d,%s,%.15f,%.15f,%s\n", fsid, callsign.c_str(), rxLatitudeDeg, rxLongitudeDeg, fixedStr.c_str())});
			}
		} else if (fanom) {
			fanom->writeRow({QString::asprintf("%d,%s,%.15f,%.15f,%s\n", fsid, callsign.c_str(), rxLatitudeDeg, rxLongitudeDeg, reasonIgnored.c_str())});
		}

	}

	LOGGER_INFO(logger) << "TOTAL NUM VALID ULS: " << numValid;
	LOGGER_INFO(logger) << "TOTAL NUM IGNORE ULS (invalid data):" << numIgnoreInvalid;
	LOGGER_INFO(logger) << "TOTAL NUM IGNORE ULS (out of band): " << numIgnoreOutOfBand;
	LOGGER_INFO(logger) << "TOTAL NUM IGNORE ULS (out of SIMULATION REGION): " << numIgnoreOutsideSimulationRegion;
	LOGGER_INFO(logger) << "TOTAL NUM IGNORE ULS (Mobile): " << numIgnoreMobile;
	LOGGER_INFO(logger) << "TOTAL NUM FIXED ULS: " << numFixed;
	LOGGER_INFO(logger) << "TOTAL NUM VALID ULS IN SIMULATION (VALID + FIXED): " << _ulsList->getSize();
	if (linkDirection == 0)
	{
		LOGGER_INFO(logger) << "NUM URBAN ULS: " << numUrbanULS << " = " << ((double)numUrbanULS / _ulsList->getSize()) * 100.0 << " \%";
		LOGGER_INFO(logger) << "NUM SUBURBAN ULS: " << numSuburbanULS << " = " << ((double)numSuburbanULS / _ulsList->getSize()) * 100.0 << " \%";
		LOGGER_INFO(logger) << "NUM RURAL ULS: " << numRuralULS << " = " << ((double)numRuralULS / _ulsList->getSize()) * 100.0 << " \%";
		LOGGER_INFO(logger) << "NUM BARREN ULS: " << numBarrenULS << " = " << ((double)numBarrenULS / _ulsList->getSize()) * 100.0 << " \%";
	}

	if (_filterSimRegionOnly)
	{
		exit(1);
	}

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** AfcManager::readRASData()                                                        ****/
/******************************************************************************************/
void AfcManager::readRASData(std::string filename)
{
    LOGGER_INFO(logger) << "Reading RAS Data: " << filename;

    int linenum, fIdx;
    std::string line, strval;
    char *chptr;
    FILE *fp = (FILE *) NULL;
    std::string str;
    std::string reasonIgnored;
    std::ostringstream errStr;

    int rasidFieldIdx = -1;
    int startFreqFieldIdx = -1;
    int stopFreqFieldIdx = -1;
    int exclusionZoneTypeFieldIdx = -1;

    int lat1Rect1FieldIdx;
    int lat2Rect1FieldIdx;
    int lon1Rect1FieldIdx;
    int lon2Rect1FieldIdx;

    int lat1Rect2FieldIdx;
    int lat2Rect2FieldIdx;
    int lon1Rect2FieldIdx;
    int lon2Rect2FieldIdx;

    int radiusFieldIdx;
    int latCircleFieldIdx;
    int lonCircleFieldIdx;

    int heightAGLFieldIdx;

    std::vector<int *> fieldIdxList;                       std::vector<std::string> fieldLabelList;
    fieldIdxList.push_back(&rasidFieldIdx);                fieldLabelList.push_back("RAS ID");
    fieldIdxList.push_back(&rasidFieldIdx);                fieldLabelList.push_back("RASID");
    fieldIdxList.push_back(&startFreqFieldIdx);            fieldLabelList.push_back("Start Freq (MHz)");
    fieldIdxList.push_back(&stopFreqFieldIdx);             fieldLabelList.push_back("Stop Freq (MHz)");
    fieldIdxList.push_back(&stopFreqFieldIdx);             fieldLabelList.push_back("End Freq (MHz)");
    fieldIdxList.push_back(&exclusionZoneTypeFieldIdx);    fieldLabelList.push_back("Exclusion Zone");

    fieldIdxList.push_back(&lat1Rect1FieldIdx);            fieldLabelList.push_back("Rectangle1 Lat 1");
    fieldIdxList.push_back(&lat2Rect1FieldIdx);            fieldLabelList.push_back("Rectangle1 Lat 2");
    fieldIdxList.push_back(&lon1Rect1FieldIdx);            fieldLabelList.push_back("Rectangle1 Lon 1");
    fieldIdxList.push_back(&lon2Rect1FieldIdx);            fieldLabelList.push_back("Rectangle1 Lon 2");

    fieldIdxList.push_back(&lat1Rect2FieldIdx);            fieldLabelList.push_back("Rectangle2 Lat 1");
    fieldIdxList.push_back(&lat2Rect2FieldIdx);            fieldLabelList.push_back("Rectangle2 Lat 2");
    fieldIdxList.push_back(&lon1Rect2FieldIdx);            fieldLabelList.push_back("Rectangle2 Lon 1");
    fieldIdxList.push_back(&lon2Rect2FieldIdx);            fieldLabelList.push_back("Rectangle2 Lon 2");

    fieldIdxList.push_back(&radiusFieldIdx);               fieldLabelList.push_back("Circle Radius (km)");
    fieldIdxList.push_back(&latCircleFieldIdx);            fieldLabelList.push_back("Circle center Lat");
    fieldIdxList.push_back(&lonCircleFieldIdx);            fieldLabelList.push_back("Circle center Lon");

    fieldIdxList.push_back(&heightAGLFieldIdx);            fieldLabelList.push_back("Antenna AGL height (m)");

    int prev_rasid;
    int rasid = -1;
    double startFreq, stopFreq;
    RASClass::RASExclusionZoneTypeEnum exclusionZoneType;
    double lat1Rect1, lat2Rect1, lon1Rect1, lon2Rect1;
    double lat1Rect2, lat2Rect2, lon1Rect2, lon2Rect2;
    double radius;
    double latCircle, lonCircle;
    bool horizonDistFlag;
    bool heightAGL;

    int fieldIdx;

    if (filename.empty()) {
        throw std::runtime_error("ERROR: No RAS data file specified");
    }

    LOGGER_INFO(logger) << "Reading RAS Datafile: " << filename;

    if ( !(fp = fopen(filename.c_str(), "rb")) ) {
        str = std::string("ERROR: Unable to open RAS Data File \"") + filename + std::string("\"\n");
        throw std::runtime_error(str);
    }

    enum LineTypeEnum {
         labelLineType,
          dataLineType,
        ignoreLineType,
       unknownLineType
    };

    LineTypeEnum lineType;

    RASClass *ras;

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
                        errStr << "ERROR: Invalid RAS Data file \"" << filename << "\" label line missing \"" << fieldLabelList.at(fIdx) << "\"" << std::endl;
                        throw std::runtime_error(errStr.str());
                    }
                }

                break;
            case    dataLineType:
		/**************************************************************************/
		/* RASID                                                                   */
		/**************************************************************************/
                prev_rasid = rasid;
                strval = fieldList.at(rasidFieldIdx);
                if (strval.empty()) {
                    errStr << "ERROR: Invalid RAS Data file \"" << filename << "\" line " << linenum << " missing RASID" << std::endl;
                    throw std::runtime_error(errStr.str());
                }

                rasid = std::stoi(strval);
                if (rasid <= prev_rasid) {
                    errStr << "ERROR: Invalid RAS Data file \"" << filename << "\" line " << linenum << " RASID values not monitonically increasing" << std::endl;
                    throw std::runtime_error(errStr.str());
                }
		/**************************************************************************/

		/**************************************************************************/
		/* startFreq                                                              */
		/**************************************************************************/
                strval = fieldList.at(startFreqFieldIdx);
                if (strval.empty()) {
                    errStr << "ERROR: Invalid RAS Data file \"" << filename << "\" line " << linenum << " missing Start Freq" << std::endl;
                    throw std::runtime_error(errStr.str());
                }
                startFreq = std::strtod(strval.c_str(), &chptr)*1.0e6; // Convert MHz to Hz
		/**************************************************************************/

		/**************************************************************************/
		/* stopFreq                                                               */
		/**************************************************************************/
                strval = fieldList.at(stopFreqFieldIdx);
                if (strval.empty()) {
                    errStr << "ERROR: Invalid RAS Data file \"" << filename << "\" line " << linenum << " missing Stop Freq" << std::endl;
                    throw std::runtime_error(errStr.str());
                }
                stopFreq = std::strtod(strval.c_str(), &chptr)*1.0e6; // Convert MHz to Hz
		/**************************************************************************/

		/**************************************************************************/
		/* exclusionZoneType                                                      */
		/**************************************************************************/
                strval = fieldList.at(exclusionZoneTypeFieldIdx);

                if (strval == "One Rectangle") {
                    exclusionZoneType = RASClass::rectRASExclusionZoneType;
                } else if (strval == "Two Rectangles") {
                    exclusionZoneType = RASClass::rect2RASExclusionZoneType;
                } else if (strval == "Circle") {
                    exclusionZoneType = RASClass::circleRASExclusionZoneType;
                } else if (strval == "Horizon Distance") {
                    exclusionZoneType = RASClass::horizonDistRASExclusionZoneType;
                } else {
                    errStr << "ERROR: Invalid RAS Data file \"" << filename << "\" line " << linenum << " exclusion zone set to unrecognized value " << strval << std::endl;
                    throw std::runtime_error(errStr.str());
                }
		/**************************************************************************/

                switch(exclusionZoneType) {
                    case RASClass::rectRASExclusionZoneType:
                    case RASClass::rect2RASExclusionZoneType:
                        ras = (RASClass *) new RectRASClass(rasid);

                        strval = fieldList.at(lat1Rect1FieldIdx);
                        lat1Rect1 = getAngleFromDMS(strval);
                        strval = fieldList.at(lat2Rect1FieldIdx);
                        lat2Rect1 = getAngleFromDMS(strval);
                        strval = fieldList.at(lon1Rect1FieldIdx);
                        lon1Rect1 = getAngleFromDMS(strval);
                        strval = fieldList.at(lon2Rect1FieldIdx);
                        lon2Rect1 = getAngleFromDMS(strval);
                        ((RectRASClass *) ras)->addRect(lon1Rect1, lon2Rect1, lat1Rect1, lat2Rect1);

                        if (exclusionZoneType == RASClass::rect2RASExclusionZoneType) {
                            strval = fieldList.at(lat1Rect2FieldIdx);
                            lat1Rect2 = getAngleFromDMS(strval);
                            strval = fieldList.at(lat2Rect2FieldIdx);
                            lat2Rect2 = getAngleFromDMS(strval);
                            strval = fieldList.at(lon1Rect2FieldIdx);
                            lon1Rect2 = getAngleFromDMS(strval);
                            strval = fieldList.at(lon2Rect2FieldIdx);
                            lon2Rect2 = getAngleFromDMS(strval);
                            ((RectRASClass *) ras)->addRect(lon1Rect2, lon2Rect2, lat1Rect2, lat2Rect2);
                        }
                        break;
                    case RASClass::circleRASExclusionZoneType:
                    case RASClass::horizonDistRASExclusionZoneType:
                        strval = fieldList.at(lonCircleFieldIdx);
                        lonCircle = getAngleFromDMS(strval);
                        strval = fieldList.at(latCircleFieldIdx);
                        latCircle = getAngleFromDMS(strval);

                        horizonDistFlag = (exclusionZoneType == RASClass::horizonDistRASExclusionZoneType);

                        ras = (RASClass *) new CircleRASClass(rasid, horizonDistFlag);

                        ((CircleRASClass *) ras)->setLongitudeCenter(lonCircle);
                        ((CircleRASClass *) ras)->setLatitudeCenter(latCircle);

                        if (!horizonDistFlag) {
                            strval = fieldList.at(radiusFieldIdx);
                            if (strval.empty()) {
                                errStr << "ERROR: Invalid RAS Data file \"" << filename << "\" line " << linenum << " missing Circle Radius" << std::endl;
                                throw std::runtime_error(errStr.str());
                            }
                            radius = std::strtod(strval.c_str(), &chptr)*1.0e3; // Convert km to m
                            ((CircleRASClass *) ras)->setRadius(radius);
                        } else {
		            /**************************************************************************/
		            /* heightAGL                                                              */
		            /**************************************************************************/
                            strval = fieldList.at(heightAGLFieldIdx);
                            if (strval.empty()) {
                                errStr << "ERROR: Invalid RAS Data file \"" << filename << "\" line " << linenum << " missing Antenna AGL Height" << std::endl;
                                throw std::runtime_error(errStr.str());
                            }
                            heightAGL = std::strtod(strval.c_str(), &chptr);
                            ras->setHeightAGL(heightAGL);
		            /**************************************************************************/
                        }

                        break;
                    default:
                        CORE_DUMP;
                        break;
                }

                _rasList->append(ras);
                ras->setStartFreq(startFreq);
                ras->setStopFreq(stopFreq);

                break;
            case  ignoreLineType:
            case unknownLineType:
                // do nothing
                break;
            default:
                CORE_DUMP;
                break;
	}
    }

    if (fp) { fclose(fp); }

    LOGGER_INFO(logger) << "TOTAL NUM RAS: " << _rasList->getSize();

    return;
}
/******************************************************************************************/

void AfcManager::fixFSTerrain()
{
	int ulsIdx;
	for (ulsIdx = 0; ulsIdx <= _ulsList->getSize() - 1; ulsIdx++) {
		ULSClass *uls = (*_ulsList)[ulsIdx];
		bool rxTerrainHeightFlag = false;
		bool txTerrainHeightFlag = false;
		double bldgHeight, terrainHeight;
		MultibandRasterClass::HeightResult lidarHeightResult;
		CConst::HeightSourceEnum heightSource;	
		if (!uls->getRxTerrainHeightFlag()) {
			_terrainDataModel->getTerrainHeight(uls->getRxLongitudeDeg(),uls->getRxLatitudeDeg(), terrainHeight,bldgHeight, lidarHeightResult, heightSource);
			rxTerrainHeightFlag = true;
			uls->setRxTerrainHeightFlag(rxTerrainHeightFlag);
			double rxHeight = uls->getRxHeightAboveTerrain() + terrainHeight;
			Vector3 rxPosition = EcefModel::geodeticToEcef(uls->getRxLatitudeDeg(), uls->getRxLongitudeDeg(), rxHeight / 1000.0);
			uls->setRxPosition(rxPosition);
			uls->setRxTerrainHeight(terrainHeight);
			uls->setRxHeightAMSL(rxHeight);
			uls->setRxHeightSource(heightSource);
		}
		if (!uls->getTxTerrainHeightFlag()) {
			_terrainDataModel->getTerrainHeight(uls->getTxLongitudeDeg(),uls->getTxLatitudeDeg(), terrainHeight,bldgHeight, lidarHeightResult, heightSource);
			txTerrainHeightFlag = true;
			uls->setTxTerrainHeightFlag(txTerrainHeightFlag);
			double txHeight = uls->getTxHeightAboveTerrain() + terrainHeight;
			Vector3 txPosition = EcefModel::geodeticToEcef(uls->getTxLatitudeDeg(), uls->getTxLongitudeDeg(), txHeight / 1000.0);
			uls->setTxPosition(txPosition);
			uls->setTxTerrainHeight(terrainHeight);
			uls->setTxHeightAMSL(txHeight);
			uls->setTxHeightSource(heightSource);
		}

		if (rxTerrainHeightFlag || txTerrainHeightFlag) {
			Vector3 txPosition = uls->getTxPosition();
			Vector3 rxPosition = uls->getRxPosition();
			uls->setAntennaPointing((txPosition - rxPosition).normalized()); // Pointing of Rx antenna
			uls->setLinkDistance((txPosition - rxPosition).len() * 1000.0);
		}
	}

	return;
}


/******************************************************************************************/
/**** FUNCTION: AfcManager::q()                                                        ****/
/**** Gaussian Q() function                                                            ****/
/******************************************************************************************/
double AfcManager::q(double Z) const
{
	static const double sqrt2 = sqrt(2.0);

	return (0.5 * erfc(Z / sqrt2));
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::computeBuildingPenetration                               ****/
/**** Compute Random Building Penetration according to ITU-R P.[BEL]                   ****/
/**** Note that a loss value in DB is returned as a negative number.                   ****/
/******************************************************************************************/
double AfcManager::computeBuildingPenetration(CConst::BuildingTypeEnum buildingType, double elevationAngleRad, double frequency, std::string &buildingPenetrationModelStr, double &buildingPenetrationCDF, bool fixedProbFlag) const
{
	double r, s, t, u, v, w, x, y, z;
	double A, B, C;
	double mA, sA, mB, sB;

	if (_fixedBuildingLossFlag) {
		buildingPenetrationModelStr = "FIXED VALUE";
		buildingPenetrationCDF = 0.5;
		return(_fixedBuildingLossValue);
	} else if (buildingType == CConst::noBuildingType) {
		buildingPenetrationModelStr = "NONE";
		buildingPenetrationCDF = 0.5;
		return (0.0);
	} else if (buildingType == CConst::traditionalBuildingType) {
		r = 12.64;
		s = 3.72;
		t = 0.96;
		u = 9.6;
		v = 2.0;
		w = 9.1;
		x = -3.0;
		y = 4.5;
		z = -2.0;
	}
	else if (buildingType == CConst::thermallyEfficientBuildingType)
	{
		r = 28.19;
		s = -3.00;
		t = 8.48;
		u = 13.5;
		v = 3.8;
		w = 27.8;
		x = -2.9;
		y = 9.4;
		z = -2.1;
	}
	else
	{
		throw std::runtime_error("ERROR in computeBuildingPenetration(), Invalid building type");
	}

	buildingPenetrationModelStr = "P.2109";

	double fGHz = frequency * 1.0e-9;
	double logf = log(fGHz) / log(10.0);
	double Le = (0.212 * 180.0 / M_PI) * fabs(elevationAngleRad);
	double Lh = r + s * logf + t * logf * logf;

	mA = Lh + Le;
	mB = w + x * logf;
	sA = u + v * logf;
	sB = y + z * logf;

	arma::vec gauss(1);
	if (fixedProbFlag)
	{
		gauss[0] = _zbldg2109;
	}
	else
	{
		gauss = arma::randn(1);
	}

	A = gauss[0] * sA + mA;
	B = gauss[0] * sB + mB;
	C = -3.0;

	double lossDB = 10.0 * log(exp(A * log(10.0) / 10.0) + exp(B * log(10.0) / 10.0) + exp(C * log(10.0) / 10.0)) / log(10.0);
	buildingPenetrationCDF = q(-gauss[0]);


	return (lossDB);
}
/******************************************************************************************/

/******************************************************************************************/
/**** AfcManager::computePathLoss                                                      ****/
/******************************************************************************************/
void AfcManager::computePathLoss(CConst::PropEnvEnum propEnv, double distKm, double frequency,
		double txLongitudeDeg, double txLatitudeDeg, double txHeightM,
		double rxLongitudeDeg, double rxLatitudeDeg, double rxHeightM, double elevationAngleRad,
		double &pathLoss, double &pathClutterDB, bool fixedProbFlag,
		std::string &pathLossModelStr, double &pathLossCDF,
		std::string &pathClutterModelStr, double &pathClutterCDF,
		const iturp452::ITURP452 *itu452, std::string *txClutterStrPtr, double **heightProfilePtr
#if MM_DEBUG
		, std::vector<std::string> &ITMHeightType
#endif
		) const
{
	// If fixedProbFlag is set, compute the path loss at specified confidence.

	double frequencyGHz = frequency * 1.0e-9;

	if (txClutterStrPtr)
	{
		*txClutterStrPtr = "";
	}

	pathLossModelStr = "";
	if (!fixedProbFlag)
	{
		pathLossCDF = -1.0;
	}
	pathClutterModelStr = "";
	pathClutterCDF = -1.0;

	if (_pathLossModel == CConst::ITMBldgPathLossModel)
	{
		if ((propEnv == CConst::urbanPropEnv) || (propEnv == CConst::suburbanPropEnv))
		{
			if (distKm * 1000 < _closeInDist)
			{
				if (_closeInPathLossModel == "WINNER2")
				{

                                    int winner2LOSValue = 0; // 1: Force LOS, 2: Force NLOS, 0: Compute probLOS, then select or combine.
                                    if (_winner2BldgLOSFlag) {
			                double terrainHeight;
			                double bldgHeight;
			                MultibandRasterClass::HeightResult lidarHeightResult;
			                CConst::HeightSourceEnum txHeightSource, rxHeightSource;
					_terrainDataModel->getTerrainHeight(txLongitudeDeg, txLatitudeDeg, terrainHeight, bldgHeight, lidarHeightResult, txHeightSource);
					_terrainDataModel->getTerrainHeight(rxLongitudeDeg, rxLatitudeDeg, terrainHeight, bldgHeight, lidarHeightResult, rxHeightSource);

                                        if ( (txHeightSource == CConst::lidarHeightSource) && (rxHeightSource == CConst::lidarHeightSource) ) {
				            int numPts = std::min(((int)floor(distKm / 0.003)) + 1, _maxNumPointsITM);
                                            bool losFlag = UlsMeasurementAnalysis::isLOS(_terrainDataModel,
			                                                                QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
			                                                                QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
                                                                                        distKm, numPts, heightProfilePtr);
                                            winner2LOSValue = (losFlag ? 1 : 2);
                                        }
                                    }


                                        double sigma;
					if (propEnv == CConst::urbanPropEnv)
					{
						// Winner2 C2: urban
						pathLoss = Winner2_C2urban(1000 * distKm, rxHeightM, txHeightM, frequency, fixedProbFlag, sigma, pathLossModelStr, pathLossCDF, winner2LOSValue);
					}
					else if (propEnv == CConst::suburbanPropEnv)
					{
						// Winner2 C1: suburban
						pathLoss = Winner2_C1suburban(1000 * distKm, rxHeightM, txHeightM, frequency, fixedProbFlag, sigma, pathLossModelStr, pathLossCDF, winner2LOSValue);
					}
				}
				else
				{
					throw std::runtime_error(ErrStream() << "ERROR: Invalid close in path loss model = " << _closeInPathLossModel);
				}
				pathClutterModelStr = "NONE";
				pathClutterDB = 0.0;
			}
			else
			{
				// Terrain propagation: Terrain + ITM
				double frequencyMHz = 1.0e-6 * frequency;
				//                std::cerr << "PATHLOSS," << txLatitudeDeg << "," << txLongitudeDeg << "," << rxLatitudeDeg << "," << rxLongitudeDeg << std::endl;
				int numPts = std::min(((int)floor(distKm / 0.003)) + 1, _maxNumPointsITM);

				/******************************************************************************************/
				/**** NOTE: ITM based on signal loss, so higher confidence corresponds to higher loss. ****/
				/**** For interference calculations, we want higher confidence to correspond to lower  ****/
				/**** loss so we use 1-confidence.                                                     ****/
				/******************************************************************************************/
				double u = _confidenceITM;

				// for(int i=1; i<=99; i++) {
				// u = ((double) i)/100.0;
				pathLoss = UlsMeasurementAnalysis::runPointToPoint(_terrainDataModel, 
				true,
				QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
				QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
				distKm, fixedEpsDielect, fixedSgmConductivity,fixedEsNoSurfRef, frequencyMHz, fixedRadioClimate, fixedPolarization, u, fixedRelevance, numPts, NULL, heightProfilePtr);
				//LOGGER_DEBUG(logger) << "Called runPointToPointBldg() successfully... u = " << u << ", pathLoss = " << -pathLoss;
				// }
				// exit(1);
				pathLossModelStr = "ITM_BLDG";
				pathLossCDF = _confidenceITM;
				pathClutterDB = 0.0;
				pathClutterModelStr = "NONE";
				pathClutterCDF = 0.5;
			}
		}
		else if ((propEnv == CConst::ruralPropEnv) || (propEnv == CConst::barrenPropEnv))
		{
			// Terrain propagation: Terrain + ITM
			double frequencyMHz = 1.0e-6 * frequency;
			int numPts = std::min(((int)floor(distKm / 0.003)) + 1, _maxNumPointsITM);
			double u = _confidenceITM;
			pathLoss = UlsMeasurementAnalysis::runPointToPoint(_terrainDataModel, 
			true,
			QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
			QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
			distKm, fixedEpsDielect, fixedSgmConductivity,fixedEsNoSurfRef, frequencyMHz, fixedRadioClimate, fixedPolarization, u, fixedRelevance, numPts, NULL, heightProfilePtr);
			pathLossModelStr = "ITM_BLDG";
			pathLossCDF = _confidenceITM;

			pathLossModelStr = "ITM_BLDG";
			pathLossCDF = _confidenceITM;
			pathClutterDB = 0.0;
			pathClutterModelStr = "NONE";
			pathClutterCDF = 0.5;
#if 0
#if 1
			if (txHeightM <= 3.0) {
				// P.452 clutter, (elevation angle < 2.86 deg) && (dist > 0.7 Km) => 18.4 dB clutter
				if ((elevationAngleRad <= 2.86*M_PI/180.0)&&(distKm > 0.7)) {
					pathClutterDB = -18.4;
				} else {
					pathClutterDB = 0.0;
				}
			} else {
				pathClutterDB = 0.0;
			}
#else
			pathClutterDB = computeClutter452HtEl(txHeightM, frequencyMHz, elevationAngleRad);
#endif
#endif

			pathClutterModelStr = "452_HT_ELANG";
			pathClutterCDF = 0.5;
		}
		else
		{
			throw std::runtime_error(ErrStream() << "ERROR reading ULS data: propEnv = " << propEnv << " INVALID value");
		}
	}
	else if (_pathLossModel == CConst::CoalitionOpt6PathLossModel)
	{
		// Path Loss Model selected for 6 GHz Coalition
		// Option 6 in PropagationModelOptions Winner-II 20171004.pptx

		if ((propEnv == CConst::urbanPropEnv) || (propEnv == CConst::suburbanPropEnv))
		{
			if (distKm * 1000 < _closeInDist)
			{
				if (_closeInPathLossModel == "WINNER2")
				{
                                    int winner2LOSValue = 0; // 1: Force LOS, 2: Force NLOS, 0: Compute probLOS, then select or combine.
                                    if (_winner2BldgLOSFlag) {
			                double terrainHeight;
			                double bldgHeight;
			                MultibandRasterClass::HeightResult lidarHeightResult;
			                CConst::HeightSourceEnum txHeightSource, rxHeightSource;
					_terrainDataModel->getTerrainHeight(txLongitudeDeg, txLatitudeDeg, terrainHeight, bldgHeight, lidarHeightResult, txHeightSource);
					_terrainDataModel->getTerrainHeight(rxLongitudeDeg, rxLatitudeDeg, terrainHeight, bldgHeight, lidarHeightResult, rxHeightSource);

                                        if ( (txHeightSource == CConst::lidarHeightSource) && (rxHeightSource == CConst::lidarHeightSource) ) {
				            int numPts = std::min(((int)floor(distKm / 0.003)) + 1, _maxNumPointsITM);
                                            bool losFlag = UlsMeasurementAnalysis::isLOS(_terrainDataModel,
			                                                                QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
			                                                                QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
                                                                                        distKm, numPts, heightProfilePtr);
                                            winner2LOSValue = (losFlag ? 1 : 2);
                                        }
                                    }

                                        double sigma;
					if (propEnv == CConst::urbanPropEnv)
					{
						// Winner2 C2: urban
						pathLoss = Winner2_C2urban(1000 * distKm, rxHeightM, txHeightM, frequency, fixedProbFlag, sigma, pathLossModelStr, pathLossCDF, winner2LOSValue);
					}
					else if (propEnv == CConst::suburbanPropEnv)
					{
						// Winner2 C1: suburban
						pathLoss = Winner2_C1suburban(1000 * distKm, rxHeightM, txHeightM, frequency, fixedProbFlag, sigma, pathLossModelStr, pathLossCDF, winner2LOSValue);
					}
				}
				else
				{
					throw std::runtime_error(ErrStream() << "ERROR: Invalid close in path loss model = " << _closeInPathLossModel);
				}
				pathClutterModelStr = "NONE";
				pathClutterDB = 0.0;
			}
			else
			{
				// Terrain propagation: Terrain + ITM
				double frequencyMHz = 1.0e-6 * frequency;
				//                std::cerr << "PATHLOSS," << txLatitudeDeg << "," << txLongitudeDeg << "," << rxLatitudeDeg << "," << rxLongitudeDeg << std::endl;
				int numPts = std::min(((int)floor(distKm / 0.003)) + 1, _maxNumPointsITM);

				/******************************************************************************************/
				/**** NOTE: ITM based on signal loss, so higher confidence corresponds to higher loss. ****/
				/**** For interference calculations, we want higher confidence to correspond to lower  ****/
				/**** loss so we use 1-confidence.                                                     ****/
				/******************************************************************************************/
				double u = _confidenceITM;

				pathLoss = UlsMeasurementAnalysis::runPointToPoint(_terrainDataModel, 
				false,
				QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
				QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
				distKm, fixedEpsDielect, fixedSgmConductivity,fixedEsNoSurfRef, frequencyMHz, fixedRadioClimate, fixedPolarization, u, fixedRelevance, numPts, NULL,heightProfilePtr);
				pathLossModelStr = "ITM";
				pathLossCDF = _confidenceITM;

				// ITU-R P.[CLUTTER] sec 3.2
				double Ll = 23.5 + 9.6 * log(frequencyGHz) / log(10.0);
				double Ls = 32.98 + 23.9 * log(distKm) / log(10.0) + 3.0 * log(frequencyGHz) / log(10.0);

				arma::vec gauss(1);
				if (fixedProbFlag)
				{
					gauss[0] = _zclutter2108;
				}
				else
				{
					gauss = arma::randn(1);
				}

				double Lctt = -5.0 * log(exp(-0.2 * Ll * log(10.0)) + exp(-0.2 * Ls * log(10.0))) / log(10.0) + 6.0 * gauss[0];
				pathClutterDB = Lctt;

				pathClutterModelStr = "P.2108";
				pathClutterCDF = q(-gauss[0]);
			}
		}
		else if ((propEnv == CConst::ruralPropEnv) || (propEnv == CConst::barrenPropEnv))
		{
			// Terrain propagation: Terrain + ITM
			double frequencyMHz = 1.0e-6 * frequency;
			int numPts = std::min(((int)floor(distKm / 0.003)) + 1, _maxNumPointsITM);
			double u = _confidenceITM;
			pathLoss = UlsMeasurementAnalysis::runPointToPoint(_terrainDataModel, 
				false,
				QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
				QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
				distKm, fixedEpsDielect, fixedSgmConductivity,fixedEsNoSurfRef, frequencyMHz, fixedRadioClimate, fixedPolarization, u, fixedRelevance, numPts, NULL,heightProfilePtr);
			pathLossModelStr = "ITM";
			pathLossCDF = _confidenceITM;

#if 0
			if (txHeightM <= 3.0) {
				// P.452 clutter, (elevation angle < 2.86 deg) && (dist > 0.7 Km) => 18.4 dB clutter
				if ((elevationAngleRad <= 2.86*M_PI/180.0)&&(distKm > 0.7)) {
					pathClutterDB = -18.4;
				} else {
					pathClutterDB = 0.0;
				}
			} else {
				pathClutterDB = 0.0;
			}
#else
			// ITU-R p.452 Clutter loss function
			pathClutterDB = AfcManager::computeClutter452HtEl(txHeightM, distKm, elevationAngleRad);
#endif

			pathClutterModelStr = "452_HT_ELANG";
			pathClutterCDF = 0.5;
		}
		else
		{
			throw std::runtime_error(ErrStream() << "ERROR reading ULS data: propEnv = " << propEnv << " INVALID value");
		}
	}
	else if (_pathLossModel == CConst::FCC6GHzReportAndOrderPathLossModel)
        {
            // Path Loss Model used in FCC Report and Order

            if (distKm*1000 < 30.0) {
		pathLoss = 20.0 * log((4 * M_PI * frequency * distKm * 1000) / CConst::c) / log(10.0);
                pathLossModelStr = "FSPL";
                pathLossCDF = 0.5;

                pathClutterDB = 0.0;
                pathClutterModelStr = "NONE";
                pathClutterCDF = 0.5;
            } else if (distKm * 1000 < _closeInDist) {
                int winner2LOSValue = 0; // 1: Force LOS, 2: Force NLOS, 0: Compute probLOS, then select or combine.
                if (_winner2BldgLOSFlag) {
                    double terrainHeight;
                    double bldgHeight;
                    MultibandRasterClass::HeightResult lidarHeightResult;
                    CConst::HeightSourceEnum txHeightSource, rxHeightSource;
                    _terrainDataModel->getTerrainHeight(txLongitudeDeg, txLatitudeDeg, terrainHeight, bldgHeight, lidarHeightResult, txHeightSource);
                    _terrainDataModel->getTerrainHeight(rxLongitudeDeg, rxLatitudeDeg, terrainHeight, bldgHeight, lidarHeightResult, rxHeightSource);

                    if ( (txHeightSource == CConst::lidarHeightSource) && (rxHeightSource == CConst::lidarHeightSource) ) {
                        int numPts = std::min(((int)floor(distKm / 0.003)) + 1, _maxNumPointsITM);
                        bool losFlag = UlsMeasurementAnalysis::isLOS(_terrainDataModel,
			                                             QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
			                                             QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
                                                                     distKm, numPts, heightProfilePtr);
                        winner2LOSValue = (losFlag ? 1 : 2);
                    }
                }

                double sigma;
                if (propEnv == CConst::urbanPropEnv) {
                    // Winner2 C2: urban
                    pathLoss = Winner2_C2urban(1000*distKm, rxHeightM, txHeightM, frequency, fixedProbFlag, sigma, pathLossModelStr, pathLossCDF, winner2LOSValue);
                } else if (propEnv == CConst::suburbanPropEnv) {
                    // Winner2 C1: suburban
                    pathLoss = Winner2_C1suburban(1000*distKm, rxHeightM, txHeightM, frequency, fixedProbFlag, sigma, pathLossModelStr, pathLossCDF, winner2LOSValue);
                } else if ( (propEnv == CConst::ruralPropEnv) || (propEnv == CConst::barrenPropEnv) ) {
                    // Winner2 D1: rural
                    pathLoss = Winner2_D1rural(1000*distKm, rxHeightM, txHeightM, frequency, fixedProbFlag, sigma, pathLossModelStr, pathLossCDF, winner2LOSValue);
                }
                pathClutterModelStr = "NONE";
                pathClutterDB = 0.0;
                pathClutterCDF = 0.5;
            } else if ((propEnv == CConst::urbanPropEnv) || (propEnv == CConst::suburbanPropEnv)) {
                // Terrain propagation: SRTM + ITM
                double frequencyMHz = 1.0e-6*frequency;
		int numPts = std::min(((int)floor(distKm / 0.003)) + 1, _maxNumPointsITM);
		double u = _confidenceITM;
		pathLoss = UlsMeasurementAnalysis::runPointToPoint(_terrainDataModel, 
			false,
			QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
			QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
			distKm, fixedEpsDielect, fixedSgmConductivity,fixedEsNoSurfRef, frequencyMHz, fixedRadioClimate, fixedPolarization, u, fixedRelevance, numPts, NULL,heightProfilePtr);
		pathLossModelStr = "ITM";
		pathLossCDF = _confidenceITM;

		// ITU-R P.[CLUTTER] sec 3.2
		double Ll = 23.5 + 9.6 * log(frequencyGHz) / log(10.0);
		double Ls = 32.98 + 23.9 * log(distKm) / log(10.0) + 3.0 * log(frequencyGHz) / log(10.0);

		arma::vec gauss(1);
		if (fixedProbFlag)
		{
			gauss[0] = _zclutter2108;
		}
		else
		{
			gauss = arma::randn(1);
		}

		double Lctt = -5.0 * log(exp(-0.2 * Ll * log(10.0)) + exp(-0.2 * Ls * log(10.0))) / log(10.0) + 6.0 * gauss[0];
		pathClutterDB = Lctt;

		pathClutterModelStr = "P.2108";
		pathClutterCDF = q(-gauss[0]);
            } else if ( (propEnv == CConst::ruralPropEnv) || (propEnv == CConst::barrenPropEnv) ) {
                // Terrain propagation: SRTM + ITM
                double frequencyMHz = 1.0e-6*frequency;
		double u = _confidenceITM;
		int numPts = std::min(((int)floor(distKm / 0.003)) + 1, _maxNumPointsITM);
		pathLoss = UlsMeasurementAnalysis::runPointToPoint(_terrainDataModel, 
			false,
			QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
			QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
			distKm, fixedEpsDielect, fixedSgmConductivity,fixedEsNoSurfRef, frequencyMHz, fixedRadioClimate, fixedPolarization, u, fixedRelevance, numPts, NULL,heightProfilePtr);
		pathLossModelStr = "ITM";
		pathLossCDF = _confidenceITM;

                if (txHeightM <= 3.0) {
                    // P.452 clutter, (elevation angle < 2.86 deg) && (dist > 0.7 Km) => 18.4 dB clutter
                    if ((elevationAngleRad <= 2.86*M_PI/180.0)&&(distKm > 0.7)) {
                        pathClutterDB = 18.4;
                    } else {
                        pathClutterDB = 0.0;
                    }
                } else {
                    pathClutterDB = 0.0;
                }

                pathClutterModelStr = "452_HT_ELANG";
                pathClutterCDF = 0.5;
            } else {
                CORE_DUMP;
            }
        }
	else if (_pathLossModel == CConst::FSPLPathLossModel)
	{
		pathLoss = 20.0 * log((4 * M_PI * frequency * distKm * 1000) / CConst::c) / log(10.0);
		pathLossModelStr = "FSPL";
		pathLossCDF = 0.5;

		pathClutterDB = 0.0;
		pathClutterModelStr = "NONE";
		pathClutterCDF = 0.5;
	}
	else
	{
		throw std::runtime_error(ErrStream() << "ERROR reading ULS data: pathLossModel = " << _pathLossModel << " INVALID value");
	}

        if (_pathLossClampFSPL) {
	    double fspl = 20.0 * log((4 * M_PI * frequency * distKm * 1000) / CConst::c) / log(10.0);
            if (pathLoss < fspl) {
                pathLossModelStr += std::to_string(pathLoss) + "_CLAMPFSPL";
                pathLoss = fspl;
            }
        }

}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::Winner2_C1suburban_LOS                                     ****/
/**** Winner II: C1, suburban LOS                                                      ****/
/**** distance = link distance (m)                                                     ****/
/**** hBS = BS antenna height (m)                                                      ****/
/**** hMS = MS antenna height (m)                                                      ****/
/**** frequency = Frequency (Hz)                                                       ****/
/******************************************************************************************/
double AfcManager::Winner2_C1suburban_LOS(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double zval, double& sigma, double &pathLossCDF) const
{
	double retval;

	double dBP = 4 * hBS * hMS * frequency / CConst::c;

	if (distance < 30.0)
	{
		// FSPL
		sigma = 0.0;
		retval = -(20.0 * log10(CConst::c / (4 * M_PI * frequency * distance)));
	}
	else if (distance < dBP)
	{
		sigma = 4.0;
		retval = 23.8 * log10(distance) + 41.2 + 20 * log10(frequency * 1.0e-9 / 5);
	}
	else
	{
		sigma = 6.0;
		retval = 40.0 * log10(distance) + 11.65 - 16.2 * log10(hBS) - 16.2 * log10(hMS) + 3.8 * log10(frequency * 1.0e-9 / 5);
	}

	arma::vec gauss(1);
	if (fixedProbFlag)
	{
		gauss[0] = zval;
	}
	else
	{
		gauss = arma::randn(1);
	}

	retval += sigma * (gauss[0]);
        pathLossCDF = q(-gauss[0]);

	return (retval);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::Winner2_C1suburban_NLOS                                    ****/
/**** Winner II: C1, suburban NLOS                                                     ****/
/**** distance = link distance (m)                                                     ****/
/**** hBS = BS antenna height (m)                                                      ****/
/**** hMS = MS antenna height (m)                                                      ****/
/**** frequency = Frequency (Hz)                                                       ****/
/******************************************************************************************/
double AfcManager::Winner2_C1suburban_NLOS(double distance, double hBS, double /* hMS */, double frequency, bool fixedProbFlag, double zval, double& sigma, double &pathLossCDF) const
{
	double retval;

	sigma = 8.0;
	retval = (44.9 - 6.55 * log10(hBS)) * log10(distance) + 31.46 + 5.83 * log10(hBS) + 23.0 * log10(frequency * 1.0e-9 / 5);

	arma::vec gauss(1);
	if (fixedProbFlag)
	{
		gauss[0] = zval;
	}
	else
	{
		gauss = arma::randn(1);
	}

	retval += sigma * (gauss[0]);
	pathLossCDF = q(-gauss[0]);

	return (retval);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::Winner2_C1suburban                                         ****/
/**** Winner II: C1, suburban                                                          ****/
/**** distance = link distance (m)                                                     ****/
/**** hBS = BS antenna height (m)                                                      ****/
/**** hMS = MS antenna height (m)                                                      ****/
/**** frequency = Frequency (Hz)                                                       ****/
/******************************************************************************************/
double AfcManager::Winner2_C1suburban(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double& sigma, std::string &pathLossModelStr, double &pathLossCDF, int losValue) const
{
	double retval;

    if (losValue == 0) {
	double probLOS = (((_closeInHgtFlag) && (hMS > _closeInHgtLOS)) ? 1.0 : exp(-distance / 200));

        if (_winner2CombineFlag) {
            double sigmaLOS, sigmaNLOS;
            double plLOS  = Winner2_C1suburban_LOS( distance, hBS, hMS, frequency, true, 0.0, sigmaLOS, pathLossCDF);
            double plNLOS = Winner2_C1suburban_NLOS(distance, hBS, hMS, frequency, true, 0.0, sigmaNLOS, pathLossCDF);
            retval = probLOS*plLOS + (1.0-probLOS)*plNLOS;
            sigma = sqrt(probLOS*probLOS*sigmaLOS*sigmaLOS + (1.0-probLOS)*(1.0-probLOS)*sigmaNLOS*sigmaNLOS);
    
	    arma::vec gauss(1);
	    if (fixedProbFlag)
	    {
	        gauss[0] = _zwinner2;
	    }
	    else
	    {
		gauss = arma::randn(1);
	    }

            retval += sigma*gauss[0];
            pathLossCDF = q(-(gauss[0]));

            pathLossModelStr = "W2C1_SUBURBAN_COMB";
        } else {
	    if (probLOS > _winner2ProbLOSThr)
	    {
		    retval = Winner2_C1suburban_LOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2, sigma, pathLossCDF);
		    pathLossModelStr = "W2C1_SUBURBAN_LOS";
	    }
	    else
	    {
		    retval = Winner2_C1suburban_NLOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2, sigma, pathLossCDF);
		    pathLossModelStr = "W2C1_SUBURBAN_NLOS";
	    }
	}
    } else if (losValue == 1) {
	    retval = Winner2_C1suburban_LOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2, sigma, pathLossCDF);
	    pathLossModelStr = "W2C1_SUBURBAN_LOSBLDG";
    } else if (losValue == 2) {
	    retval = Winner2_C1suburban_NLOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2, sigma, pathLossCDF);
	    pathLossModelStr = "W2C1_SUBURBAN_NLOSBLDG";
    } else {
        CORE_DUMP;
    }

	return (retval);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::Winner2_C2urban_LOS                                        ****/
/**** Winner II: C2, urban LOS                                                         ****/
/**** distance = link distance (m)                                                     ****/
/**** hBS = BS antenna height (m)                                                      ****/
/**** hMS = MS antenna height (m)                                                      ****/
/**** frequency = Frequency (Hz)                                                       ****/
/******************************************************************************************/
double AfcManager::Winner2_C2urban_LOS(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double zval, double& sigma, double &pathLossCDF) const
{
	double retval;

	double dBP = 4 * (hBS - 1.0) * (hMS - 1.0) * frequency / CConst::c;

	if (distance < 10.0)
	{
		// FSPL
		sigma = 0.0;
		retval = -(20.0 * log10(CConst::c / (4 * M_PI * frequency * distance)));
	}
	else if (distance < dBP)
	{
		sigma = 4.0;
		retval = 26.0 * log10(distance) + 39.0 + 20 * log10(frequency * 1.0e-9 / 5);
	}
	else
	{
		sigma = 6.0;
		retval = 40.0 * log10(distance) + 13.47 - 14.0 * log10(hBS - 1) - 14.0 * log10(hMS - 1) + 6.0 * log10(frequency * 1.0e-9 / 5);
	}

	arma::vec gauss(1);
	if (fixedProbFlag)
	{
		gauss[0] = zval;
	}
	else
	{
		gauss = arma::randn(1);
	}

	retval += sigma * (gauss[0]);
	pathLossCDF = q(-gauss[0]);

	return (retval);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::Winner2_C2urban_NLOS                                       ****/
/**** Winner II: C2, urban NLOS                                                        ****/
/**** distance = link distance (m)                                                     ****/
/**** hBS = BS antenna height (m)                                                      ****/
/**** hMS = MS antenna height (m)                                                      ****/
/**** frequency = Frequency (Hz)                                                       ****/
/******************************************************************************************/
double AfcManager::Winner2_C2urban_NLOS(double distance, double hBS, double /* hMS */, double frequency, bool fixedProbFlag, double zval, double& sigma, double &pathLossCDF) const
{
	double retval;

	sigma = 8.0;
	retval = (44.9 - 6.55 * log10(hBS)) * log10(distance) + 34.46 + 5.83 * log10(hBS) + 23.0 * log10(frequency * 1.0e-9 / 5);

	arma::vec gauss(1);
	if (fixedProbFlag)
	{
		gauss[0] = zval;
	}
	else
	{
		gauss = arma::randn(1);
	}

	retval += sigma * (gauss[0]);
	pathLossCDF = q(-gauss[0]);

	return (retval);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::Winner2_C2urban                                            ****/
/**** Winner II: C2, suburban                                                          ****/
/**** distance = link distance (m)                                                     ****/
/**** hBS = BS antenna height (m)                                                      ****/
/**** hMS = MS antenna height (m)                                                      ****/
/**** frequency = Frequency (Hz)                                                       ****/
/******************************************************************************************/
double AfcManager::Winner2_C2urban(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double& sigma, std::string &pathLossModelStr, double &pathLossCDF, int losValue) const
{
    double retval;

    if (losValue == 0) {

        double probLOS;
        if ((_closeInHgtFlag) && (hMS > _closeInHgtLOS))
        {
    	    probLOS = 1.0;
        }
        else
        {
    	    probLOS = (distance > 18.0 ? 18.0 / distance : 1.0) * (1.0 - exp(-distance / 63.0)) + exp(-distance / 63.0);
        }

        if (_winner2CombineFlag) {
            double sigmaLOS, sigmaNLOS;
            double plLOS  = Winner2_C2urban_LOS( distance, hBS, hMS, frequency, true, 0.0, sigmaLOS, pathLossCDF);
            double plNLOS = Winner2_C2urban_NLOS(distance, hBS, hMS, frequency, true, 0.0, sigmaNLOS, pathLossCDF);
            retval = probLOS*plLOS + (1.0-probLOS)*plNLOS;
            sigma = sqrt(probLOS*probLOS*sigmaLOS*sigmaLOS + (1.0-probLOS)*(1.0-probLOS)*sigmaNLOS*sigmaNLOS);

	    arma::vec gauss(1);
	    if (fixedProbFlag)
	    {
	        gauss[0] = _zwinner2;
	    } else {
	        gauss = arma::randn(1);
	    }

            retval += sigma*gauss[0];
            pathLossCDF = q(-(gauss[0]));

            pathLossModelStr = "W2C2_URBAN_COMB";
        } else {
	    if (probLOS > _winner2ProbLOSThr)
	    {
		retval = Winner2_C2urban_LOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2, sigma, pathLossCDF);
		pathLossModelStr = "W2C2_URBAN_LOS";
	    }
	    else
	    {
		retval = Winner2_C2urban_NLOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2, sigma, pathLossCDF);
		pathLossModelStr = "W2C2_URBAN_NLOS";
	    }
        }
    } else if (losValue == 1) {
	retval = Winner2_C2urban_LOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2, sigma, pathLossCDF);
	pathLossModelStr = "W2C2_URBAN_LOSBLDG";
    } else if (losValue == 2) {
	retval = Winner2_C2urban_NLOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2, sigma, pathLossCDF);
	pathLossModelStr = "W2C2_URBAN_NLOSBLDG";
    } else {
        CORE_DUMP;
    }

    return (retval);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::Winner2_D1rural_LOS                                        ****/
/**** Winner II: D1, rural LOS                                                         ****/
/**** distance = link distance (m)                                                     ****/
/**** hBS = BS antenna height (m)                                                      ****/
/**** hMS = MS antenna height (m)                                                      ****/
/**** frequency = Frequency (Hz)                                                       ****/
/******************************************************************************************/
double AfcManager::Winner2_D1rural_LOS(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double zval, double& sigma, double &pathLossCDF) const
{
	double retval;

	double dBP = 4 * (hBS) * (hMS)*frequency / CConst::c;

	if (distance < 10.0)
	{
		// FSPL
		sigma = 0.0;
		retval = -(20.0 * log10(CConst::c / (4 * M_PI * frequency * distance)));
	}
	else if (distance < dBP)
	{
		sigma = 4.0;
		retval = 21.5 * log10(distance) + 44.2 + 20 * log10(frequency * 1.0e-9 / 5);
	}
	else
	{
		sigma = 6.0;
		retval = 40.0 * log10(distance) + 10.5 - 18.5 * log10(hBS) - 18.5 * log10(hMS) + 1.5 * log10(frequency * 1.0e-9 / 5);
	}

	arma::vec gauss(1);
	if (fixedProbFlag)
	{
		gauss[0] = zval;
	}
	else
	{
		gauss = arma::randn(1);
	}

	retval += sigma * gauss[0];
	pathLossCDF = q(-gauss[0]);

	return (retval);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::Winner2_D1rural_NLOS                                       ****/
/**** Winner II: D1, rural NLOS                                                        ****/
/**** distance = link distance (m)                                                     ****/
/**** hBS = BS antenna height (m)                                                      ****/
/**** hMS = MS antenna height (m)                                                      ****/
/**** frequency = Frequency (Hz)                                                       ****/
/******************************************************************************************/
double AfcManager::Winner2_D1rural_NLOS(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double zval, double& sigma, double &pathLossCDF) const
{
	double retval;

	sigma = 8.0;
	retval = 25.1 * log10(distance) + 55.4 - 0.13 * (hBS - 25) * log10(distance / 100) - 0.9 * (hMS - 1.5) + 21.3 * log10(frequency * 1.0e-9 / 5);

	arma::vec gauss(1);
	if (fixedProbFlag)
	{
		gauss[0] = zval;
	}
	else
	{
		gauss = arma::randn(1);
	}

	retval += sigma * gauss[0];
	pathLossCDF = q(-gauss[0]);

	return (retval);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::Winner2_D1rural                                            ****/
/**** Winner II: D1, rural                                                             ****/
/**** distance = link distance (m)                                                     ****/
/**** hBS = BS antenna height (m)                                                      ****/
/**** hMS = MS antenna height (m)                                                      ****/
/**** frequency = Frequency (Hz)                                                       ****/
/******************************************************************************************/
double AfcManager::Winner2_D1rural(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double& sigma, std::string &pathLossModelStr, double &pathLossCDF, int losValue) const
{
	double retval;

    if (losValue == 0) {

	double probLOS;
	if ((_closeInHgtFlag) && (hMS > _closeInHgtLOS))
	{
		probLOS = 1.0;
	}
	else
	{
		probLOS = exp(-distance / 1000);
	}

        if (_winner2CombineFlag) {
            double sigmaLOS, sigmaNLOS;
            double plLOS  = Winner2_D1rural_LOS( distance, hBS, hMS, frequency, true, 0.0, sigmaLOS, pathLossCDF);
            double plNLOS = Winner2_D1rural_NLOS(distance, hBS, hMS, frequency, true, 0.0, sigmaNLOS, pathLossCDF);
            retval = probLOS*plLOS + (1.0-probLOS)*plNLOS;
            sigma = sqrt(probLOS*probLOS*sigmaLOS*sigmaLOS + (1.0-probLOS)*(1.0-probLOS)*sigmaNLOS*sigmaNLOS);

	    arma::vec gauss(1);
	    if (fixedProbFlag)
	    {
	        gauss[0] = _zwinner2;
	    } else {
	        gauss = arma::randn(1);
	    }

            retval += sigma*gauss[0];
            pathLossCDF = q(-(gauss[0]));

            pathLossModelStr = "W2D1_RURAL_COMB";
        } else {

	    if (probLOS > _winner2ProbLOSThr)
	    {
		retval = Winner2_D1rural_LOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2, sigma, pathLossCDF);
		pathLossModelStr = "W2D1_RURAL_LOS";
	    }
	    else
	    {
		retval = Winner2_D1rural_NLOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2, sigma, pathLossCDF);
		pathLossModelStr = "W2D1_RURAL_NLOS";
	    }
        }
    } else if (losValue == 1) {
	retval = Winner2_D1rural_LOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2, sigma, pathLossCDF);
	pathLossModelStr = "W2D1_RURAL_LOSBLDG";
    } else if (losValue == 2) {
	retval = Winner2_D1rural_NLOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2, sigma, pathLossCDF);
	pathLossModelStr = "W2D1_RURAL_NLOSBLDG";
    } else {
        CORE_DUMP;
    }

    return (retval);
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::compute()                                                                  */
/******************************************************************************************/
void AfcManager::compute()
{
	// initialize all channels to max EIRP before computing
	for (auto& channel : _channelList)
		channel.eirpLimit_dBm = _maxEIRP_dBm;

	if (_analysisType == "PointAnalysis") {
#if 0
            double longitudeDeg = -7.174311111000000e+01;
            double latitudeDeg = 4.189369444000000e+01;
            double terrainHeight, bldgHeight;
            MultibandRasterClass::HeightResult lidarHeightResult;
            CConst::HeightSourceEnum rxHeightSource;
            _terrainDataModel->getTerrainHeight(longitudeDeg, latitudeDeg, terrainHeight,bldgHeight, lidarHeightResult, rxHeightSource);
            exit(0);
#endif
		runPointAnalysis();
	} else if (_analysisType == "APAnalysis") {
		runPointAnalysis();
	} else if (_analysisType == "AP-AFC") {
		runPointAnalysis();
	} else if (_analysisType == "ExclusionZoneAnalysis") {
		runExclusionZoneAnalysis();
	} else if (_analysisType == "HeatmapAnalysis") {
		runHeatmapAnalysis();
#if MM_DEBUG
	} else if (_analysisType == "test_aciFn") {
            double fStartMHz = -5.0;
            double fStopMHz  = 25.0;
            double BMHz      = 40.0;
            printf("fStartMHz = %.10f\n", fStartMHz);
            printf("fStopMHz = %.10f\n", fStopMHz);
            printf("BMHz = %.10f\n", BMHz);
            double aciFnStart = aciFn(fStartMHz, BMHz);
            double aciFnStop = aciFn(fStopMHz, BMHz);
            printf("aciFnStart = %.10f\n", aciFnStart);
            printf("aciFnStop = %.10f\n", aciFnStop);
#endif
	} else {
		throw std::runtime_error(ErrStream() << "ERROR: Unrecognized analysis type = \"" << _analysisType << "\"");
	}

}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::runPointAnalysis()                                                         */
/******************************************************************************************/
void AfcManager::runPointAnalysis()
{
	std::ostringstream errStr;

#if MM_DEBUG
	// std::vector<int> fsidTraceList{2128, 3198, 82443};
	// std::vector<int> fsidTraceList{64324};
	std::vector<int> fsidTraceList{93911};
	std::string pathTraceFile = "path_trace.csv.gz";
#endif

	LOGGER_INFO(logger) << "Executing AfcManager::runPointAnalysis()";

        _rlanRegion->configure(_rlanHeightType.toStdString(), _terrainDataModel);

	/**************************************************************************************/
	/* Allocate / Initialize channelAvailability                                          */
	/**************************************************************************************/
#if 0
	int totalNumChan = 0;
	std::vector<std::string> rlanBWStrList = split(_rlanBWStr, ','); // Splits comma-separated list containing the different bandwidths

	_channelData = (ChannelStruct **)malloc(rlanBWStrList.size() * sizeof(ChannelStruct *)); // Allocate memory
	_numChan = (int *)malloc(rlanBWStrList.size() * sizeof(int));

	int bwIdx;
	for (bwIdx = 0; bwIdx < (int)rlanBWStrList.size(); ++bwIdx)
	{ // Iterate over each bandwidth (not channels yet)
		char *chptr;
		double bandwidth = std::strtod(rlanBWStrList[bwIdx].c_str(), &chptr);
		_rlanBWList.push_back(bandwidth); // Load the bandwidth list with the designated bandwidths

		int numChan = (int)floor((_wlanMaxFreq - _wlanMinFreq) / bandwidth + 1.0e-6);
		_numChan[bwIdx] = numChan;

		_channelData[bwIdx] = (ChannelStruct *)malloc(numChan * sizeof(ChannelStruct)); // Load the channel color into the availability matrix
		int chanIdx;
		for (chanIdx = 0; chanIdx < numChan; ++chanIdx)
		{
			_channelData[bwIdx][chanIdx].availability = GREEN; // Everything initialized to GREEN
			_channelData[bwIdx][chanIdx].eirpLimit_dBm = _maxEIRP_dBm;
		}
		totalNumChan += numChan;
	}

	for (bwIdx = 0; bwIdx < (int)rlanBWStrList.size(); ++bwIdx)
	{ // Check that it works
		LOGGER_INFO(logger) << "RLAN BANDWIDTH (MHz) = " << _rlanBWList[bwIdx] * 1.0e-6 << "  NUM_CHAN = " << _numChan[bwIdx];
	}
#endif
	/**************************************************************************************/

	const double rlanOrient_rad = _rlanOrientation_deg * M_PI / 180.0;

	// double rlanTerrainHeight, bldgHeight;
	// MultibandRasterClass::HeightResult lidarHeightResult;
	// CConst::HeightSourceEnum rlanHeightSource;
	// _terrainDataModel->getTerrainHeight(centerLon,centerLat, rlanTerrainHeight,bldgHeight, lidarHeightResult, rlanHeightSource);

	// LOGGER_DEBUG(logger) << "rlanHeight: " << rlanTerrainHeight << ", building height: " << bldgHeight << ", from: " << rlanHeightSource;

	// if (_rlanHeightType == "AMSL") {
	// 	centerHeight = rlanHeightInput;
	// } else if (_rlanHeightType == "AGL") {
	// 	centerHeight = rlanHeightInput + rlanTerrainHeight;
	// } else {
	// 	throw std::runtime_error(ErrStream() << "ERROR: INVALID _rlanHeightType = " << _rlanHeightType);
	// }

	// Vector3 rlanCenterPosn = EcefModel::geodeticToEcef(centerLat, centerLon, centerHeight / 1000.0);

	// define orthogonal unit vectors at RLAN ellipse center for East, North, and Up
	// Vector3 upVec = rlanCenterPosn.normalized();
	// Vector3 eastVec = (Vector3(-upVec.y(), upVec.x(), 0.0)).normalized();
	// Vector3 northVec = upVec.cross(eastVec);

	// define orthogonal unit vectors in the directions of semiMajor and semiMinor axis of ellipse
	// Vector3 majorVec = cos(rlanOrient_rad) * northVec + sin(rlanOrient_rad) * eastVec;
	// Vector3 minorVec = sin(rlanOrient_rad) * northVec - cos(rlanOrient_rad) * eastVec;
	/**************************************************************************************/

	/**************************************************************************************/
	/* Determine propagation environment of RLAN using centerLat, centerLon, popGrid      */
	/**************************************************************************************/
	CConst::PropEnvEnum rlanPropEnv = computeRlanPropEnv(_rlanRegion->getCenterLongitude(), _rlanRegion->getCenterLatitude());
	/**************************************************************************************/

	/**************************************************************************************/
	/* Create excThrFile, useful for debugging                                            */
	/**************************************************************************************/
	auto excthr_writer = GzipCsvWriter(_excThrFile);
	auto &fexcthrwifi = excthr_writer.csv_writer;

	if (fexcthrwifi)
	{
		fexcthrwifi->writeRow({ "FS_ID",
				"RLAN_POSN_IDX",
				"CALLSIGN",
				"FS_RX_LONGITUDE (deg)",
				"FS_RX_LATITUDE (deg)",
				"FS_RX_HEIGHT_ABOVE_TERRAIN (m)",
				"FS_RX_TERRAIN_HEIGHT (m)",
				"FS_RX_TERRAIN_SOURCE",
				"FS_RX_PROP_ENV",
				"RLAN_LONGITUDE (deg)",
				"RLAN_LATITUDE (deg)",
				"RLAN_HEIGHT_ABOVE_TERRAIN (m)",
				"RLAN_TERRAIN_HEIGHT (m)",
				"RLAN_TERRAIN_SOURCE",
				"RLAN_PROP_ENV",
				"RLAN_FS_RX_DIST (km)",
				"RLAN_FS_RX_ELEVATION_ANGLE (deg)",
				"FS_RX_ANGLE_OFF_BORESIGHT (deg)",
				"RLAN_TX_EIRP (dBm)",
				"BODY_LOSS (dB)",
				"RLAN_CLUTTER_CATEGORY",
				"BUILDING TYPE",
				"RLAN_FS_RX_BUILDING_PENETRATION (dB)",
				"BUILDING_PENETRATION_MODEL",
				"BUILDING_PENETRATION_CDF",
				"PATH_LOSS (dB)",
				"PATH_LOSS_MODEL",
				"PATH_LOSS_CDF",
				"PATH_CLUTTER (DB)",
				"PATH_CLUTTER_MODEL",
				"PATH_CLUTTER_CDF",
				"RLAN BANDWIDTH (MHz)",
				"RLAN CHANNEL START FREQ (MHz)",
				"RLAN CHANNEL STOP FREQ (MHz)",
				"ULS START FREQ (MHz)",
				"ULS STOP FREQ (MHz)",
				"FS_ANT_TYPE",
				"FS_ANT_GAIN_PEAK (dB)",
				"FS_ANT_GAIN_TO_RLAN (dB)",
				"RX_SPECTRAL_OVERLAP_LOSS (dB)",
				"POLARIZATION_LOSS (dB)",
				"FS_RX_FEEDER_LOSS (dB)",
				"FS_RX_PWR (dBW)",
				"FS I/N (dB)",
				"ULS_LINK_DIST (m)",
				"RLAN_CENTER_FREQ (Hz)",
				"FS_TX_TO_RLAN_DIST (m)",
				"PATH_DIFFERENCE (m)",
				"ULS_WAVELENGTH (mm)",
				"FRESNEL_INDEX"

		});
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Create KML file                                                                    */
	/**************************************************************************************/
	ZXmlWriter kml_writer(_kmlFile);
	auto &fkml = kml_writer.xml_writer;

	if (fkml) {
		fkml->setAutoFormatting(true);
		fkml->writeStartDocument();
		fkml->writeStartElement("kml");
		fkml->writeAttribute("xmlns", "http://www.opengis.net/kml/2.2");
		fkml->writeStartElement("Document");
		fkml->writeTextElement("name", "FB RLAN AFC");
		fkml->writeTextElement("open", "1");
		fkml->writeTextElement("description", "Display Point Analysis Results");

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "transGrayPoly");
		fkml->writeStartElement("LineStyle");
		fkml->writeTextElement("width", "1.5");
		fkml->writeEndElement(); // LineStyle
		fkml->writeStartElement("PolyStyle");
		fkml->writeTextElement("color", "7d7f7f7f");
		fkml->writeEndElement(); // Polystyle
	        fkml->writeAttribute("id", "transBluePoly");
	        fkml->writeStartElement("LineStyle");
	        fkml->writeTextElement("width", "1.5");
	        fkml->writeEndElement(); // LineStyle
	        fkml->writeStartElement("PolyStyle");
	        fkml->writeTextElement("color", "7dff0000");
	        fkml->writeEndElement(); // PolyStyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "redPoly");
		fkml->writeStartElement("LineStyle");
		fkml->writeTextElement("color", "ff0000ff");
		fkml->writeTextElement("width", "1.5");
		fkml->writeEndElement(); // LineStyle
		fkml->writeStartElement("PolyStyle");
		fkml->writeTextElement("color", "7d0000ff");
		fkml->writeEndElement(); // Polystyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "yellowPoly");
		fkml->writeStartElement("LineStyle");
		fkml->writeTextElement("color", "ff00ffff");
		fkml->writeTextElement("width", "1.5");
		fkml->writeEndElement(); // LineStyle
		fkml->writeStartElement("PolyStyle");
		fkml->writeTextElement("color", "7d00ffff");
		fkml->writeEndElement(); // Polystyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "greenPoly");
		fkml->writeStartElement("LineStyle");
		fkml->writeTextElement("color", "ff00ff00");
		fkml->writeTextElement("width", "1.5");
		fkml->writeEndElement(); // LineStyle
		fkml->writeStartElement("PolyStyle");
		fkml->writeTextElement("color", "7d00ff00");
		fkml->writeEndElement(); // Polystyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "blackPoly");
		fkml->writeStartElement("LineStyle");
		fkml->writeTextElement("color", "ff000000");
		fkml->writeTextElement("width", "1.5");
		fkml->writeEndElement(); // LineStyle
		fkml->writeStartElement("PolyStyle");
		fkml->writeTextElement("color", "7d000000");
		fkml->writeEndElement(); // Polystyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "dotStyle");
		fkml->writeStartElement("IconStyle");
		fkml->writeStartElement("Icon");
		fkml->writeTextElement("href", "http://maps.google.com/mapfiles/kml/shapes/placemark_circle.png");
		fkml->writeEndElement(); // Icon
		fkml->writeEndElement(); // IconStyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "redPlacemark");
		fkml->writeStartElement("IconStyle");
		fkml->writeTextElement("color", "ff0000ff");
		fkml->writeStartElement("Icon");
		fkml->writeTextElement("href", "http://maps.google.com/mapfiles/kml/pushpin/ylw-pushpin.png");
		fkml->writeEndElement(); // Icon
		fkml->writeEndElement(); // IconStyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "yellowPlacemark");
		fkml->writeStartElement("IconStyle");
		fkml->writeTextElement("color", "ff00ffff");
		fkml->writeStartElement("Icon");
		fkml->writeTextElement("href", "http://maps.google.com/mapfiles/kml/pushpin/ylw-pushpin.png");
		fkml->writeEndElement(); // Icon
		fkml->writeEndElement(); // IconStyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "greenPlacemark");
		fkml->writeStartElement("IconStyle");
		fkml->writeTextElement("color", "ff00ff00");
		fkml->writeStartElement("Icon");
		fkml->writeTextElement("href", "http://maps.google.com/mapfiles/kml/pushpin/ylw-pushpin.png");
		fkml->writeEndElement(); // Icon
		fkml->writeEndElement(); // IconStyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "blackPlacemark");
		fkml->writeStartElement("IconStyle");
		fkml->writeTextElement("color", "ff000000");
		fkml->writeStartElement("Icon");
		fkml->writeTextElement("href", "http://maps.google.com/mapfiles/kml/pushpin/ylw-pushpin.png");
		fkml->writeEndElement(); // Icon
		fkml->writeEndElement(); // IconStyle
		fkml->writeEndElement(); // Style
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Draw uncertainty cylinder in KML                                                   */
	/**************************************************************************************/
	if (fkml) {
		int ptIdx;
		fkml->writeStartElement("Folder");
		fkml->writeTextElement("name", "RLAN");

		std::vector<GeodeticCoord> ptList = _rlanRegion->getBoundary();

#if 0
		int numRLANPoints = 32;
		for(ptIdx=0; ptIdx<numRLANPoints; ptIdx++) {
			double phi = 2*M_PI*ptIdx/numRLANPoints;
			Vector3 ptgVec = cos(phi)*eastVec + sin(phi)*northVec;
			double ex = ptgVec.dot(majorVec);    // Component of ptgVec in semiMajorAxis direction
			double ey = ptgVec.dot(minorVec);    // Component of ptgVec in semiMinorAxis direction
			double distToEllipseEdge = 1.0 / sqrt((ex / semiMajorAxis) * (ex / semiMajorAxis) + (ey / semiMinorAxis) * (ey / semiMinorAxis)); // Dist from ellipse center to ellipse edge in direction ptgVec
			Vector3 rlanEllipsePt = rlanCenterPosn + (distToEllipseEdge / 1000.0) * ptgVec;
			GeodeticCoord rlanEllipsePtGeo = EcefModel::toGeodetic(rlanEllipsePt);
			ptList.push_back(rlanEllipsePtGeo);
		}
#endif

		/**********************************************************************************/
		/* CENTER                                                                         */
		/**********************************************************************************/
		GeodeticCoord rlanCenterPtGeo = EcefModel::toGeodetic(_rlanRegion->getCenterPosn());

		fkml->writeStartElement("Placemark");
		fkml->writeTextElement("name", "CENTER");
		fkml->writeTextElement("visibility", "1");
		fkml->writeTextElement("styleUrl", "#dotStyle");
		fkml->writeStartElement("Point");
		fkml->writeTextElement("altitudeMode", "absolute");
		fkml->writeTextElement("coordinates", QString::asprintf("%.10f,%.10f,%.2f", rlanCenterPtGeo.longitudeDeg, rlanCenterPtGeo.latitudeDeg, rlanCenterPtGeo.heightKm * 1000.0));
		fkml->writeEndElement(); // Point
		fkml->writeEndElement(); // Placemark
		/**********************************************************************************/

		/**********************************************************************************/
		/* TOP                                                                            */
		/**********************************************************************************/
		fkml->writeStartElement("Placemark");
		fkml->writeTextElement("name", "TOP");
		fkml->writeTextElement("visibility", "1");
		fkml->writeTextElement("styleUrl", "#transGrayPoly");
		fkml->writeStartElement("Polygon");
		fkml->writeTextElement("extrude", "0");
		fkml->writeTextElement("tessellate", "0");
		fkml->writeTextElement("altitudeMode", "absolute");
		fkml->writeStartElement("outerBoundaryIs");
		fkml->writeStartElement("LinearRing");

		QString top_coords = QString();
		for(ptIdx=0; ptIdx<=(int) ptList.size(); ptIdx++) {
			GeodeticCoord pt = ptList[ptIdx % ptList.size()];
			top_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", pt.longitudeDeg, pt.latitudeDeg, pt.heightKm*1000.0 + _rlanRegion->getHeightUncertainty()));
		}

		fkml->writeTextElement("coordinates", top_coords);
		fkml->writeEndElement(); // LinearRing
		fkml->writeEndElement(); // outerBoundaryIs
		fkml->writeEndElement(); // Polygon
		fkml->writeEndElement(); // Placemark
		/**********************************************************************************/

		/**********************************************************************************/
		/* BOTTOM                                                                         */
		/**********************************************************************************/
		fkml->writeStartElement("Placemark");
		fkml->writeTextElement("name", "BOTTOM");
		fkml->writeTextElement("visibility", "1");
		fkml->writeTextElement("styleUrl", "#transGrayPoly");
		fkml->writeStartElement("Polygon");
		fkml->writeTextElement("extrude", "0");
		fkml->writeTextElement("tessellate", "0");
		fkml->writeTextElement("altitudeMode", "absolute");
		fkml->writeStartElement("outerBoundaryIs");
		fkml->writeStartElement("LinearRing");

		QString bottom_coords = QString();
		for(ptIdx=0; ptIdx<=(int) ptList.size(); ptIdx++) {
			GeodeticCoord pt = ptList[ptIdx % ptList.size()];
			bottom_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", pt.longitudeDeg, pt.latitudeDeg, pt.heightKm*1000.0 - _rlanRegion->getHeightUncertainty()));
		}
		fkml->writeTextElement("coordinates", bottom_coords);
		fkml->writeEndElement(); // LinearRing
		fkml->writeEndElement(); // outerBoundaryIs
		fkml->writeEndElement(); // Polygon
		fkml->writeEndElement(); // Placemark

		/**********************************************************************************/

		/**********************************************************************************/
		/* SIDES                                                                          */
		/**********************************************************************************/
		for(ptIdx=0; ptIdx<(int) ptList.size(); ptIdx++) {
			fkml->writeStartElement("Placemark");
			fkml->writeTextElement("name", QString::asprintf("S_%d", ptIdx));
			fkml->writeTextElement("visibility", "1");
			fkml->writeTextElement("styleUrl", "#transGrayPoly");
			fkml->writeStartElement("Polygon");
			fkml->writeTextElement("extrude", "0");
			fkml->writeTextElement("tessellate", "0");
			fkml->writeTextElement("altitudeMode", "absolute");
			fkml->writeStartElement("outerBoundaryIs");
			fkml->writeStartElement("LinearRing");

			GeodeticCoord pt1 = ptList[ptIdx];
			GeodeticCoord pt2 = ptList[(ptIdx +1) % ptList.size()];
			QString side_coords;
			side_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", pt1.longitudeDeg, pt1.latitudeDeg, pt1.heightKm*1000.0 - _rlanRegion->getHeightUncertainty()));
			side_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", pt1.longitudeDeg, pt1.latitudeDeg, pt1.heightKm*1000.0 + _rlanRegion->getHeightUncertainty()));
			side_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", pt2.longitudeDeg, pt2.latitudeDeg, pt2.heightKm*1000.0 + _rlanRegion->getHeightUncertainty()));
			side_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", pt2.longitudeDeg, pt2.latitudeDeg, pt2.heightKm*1000.0 - _rlanRegion->getHeightUncertainty()));
			side_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", pt1.longitudeDeg, pt1.latitudeDeg, pt1.heightKm*1000.0 - _rlanRegion->getHeightUncertainty()));

			fkml->writeTextElement("coordinates", side_coords);
			fkml->writeEndElement(); // LinearRing
			fkml->writeEndElement(); // outerBoundaryIs
			fkml->writeEndElement(); // Polygon
			fkml->writeEndElement(); // Placemark
		}
		/**********************************************************************************/

		fkml->writeEndElement(); // Folder

		fkml->writeStartElement("Folder");
		fkml->writeTextElement("name", "RAS");
                int rasIdx;
                for (rasIdx = 0; rasIdx < _rasList->getSize(); ++rasIdx) {
		    RASClass *ras = (*_rasList)[rasIdx];

		    fkml->writeStartElement("Folder");
		    fkml->writeTextElement("name", QString("RAS_") + QString::number(ras->getID()));

                    int numPtsCircle = 32;
                    int rectIdx, numRect;
                    double rectLonStart, rectLonStop, rectLatStart, rectLatStop;
                    double circleRadius, longitudeCenter, latitudeCenter;
	            double rasTerrainHeight, rasBldgHeight, rasHeightAGL;
                    Vector3 rasCenterPosn;
                    Vector3 rasUpVec;
                    Vector3 rasEastVec;
                    Vector3 rasNorthVec;
                    QString ras_coords;
	            MultibandRasterClass::HeightResult rasLidarHeightResult;
	            CConst::HeightSourceEnum rasHeightSource;
                    RASClass::RASExclusionZoneTypeEnum rasType = ras->type();
                    switch(rasType) {
                        case RASClass::rectRASExclusionZoneType:
                        case RASClass::rect2RASExclusionZoneType:
                            numRect = ((RectRASClass *) ras)->getNumRect();
                            for(rectIdx=0; rectIdx<numRect; rectIdx++) {
                                std::tie(rectLonStart, rectLonStop, rectLatStart, rectLatStop) = ((RectRASClass *) ras)->getRect(rectIdx);

                                fkml->writeStartElement("Placemark");
                                fkml->writeTextElement("name", QString("RECT_") + QString::number(rectIdx));
                                fkml->writeTextElement("visibility", "1");
                                fkml->writeTextElement("styleUrl", "#transBluePoly");
                                fkml->writeStartElement("Polygon");
                                fkml->writeTextElement("extrude", "0");
                                fkml->writeTextElement("tessellate", "0");
                                fkml->writeTextElement("altitudeMode", "clampToGround");
                                fkml->writeStartElement("outerBoundaryIs");
                                fkml->writeStartElement("LinearRing");

                                ras_coords = QString();
                                ras_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", rectLonStart, rectLatStart, 0.0));
                                ras_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", rectLonStop,  rectLatStart, 0.0));
                                ras_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", rectLonStop,  rectLatStop,  0.0));
                                ras_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", rectLonStart, rectLatStop,  0.0));
                                ras_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", rectLonStart, rectLatStart, 0.0));

                                fkml->writeTextElement("coordinates", ras_coords);
                                fkml->writeEndElement(); // LinearRing
                                fkml->writeEndElement(); // outerBoundaryIs
                                fkml->writeEndElement(); // Polygon
                                fkml->writeEndElement(); // Placemark
                            }
                            break;
                        case RASClass::circleRASExclusionZoneType:
                        case RASClass::horizonDistRASExclusionZoneType:
                            circleRadius = ((CircleRASClass *) ras)->computeRadius(_rlanRegion->getMaxHeightAGL());
                            longitudeCenter = ((CircleRASClass *) ras)->getLongitudeCenter();
                            latitudeCenter = ((CircleRASClass *) ras)->getLatitudeCenter();
                            rasHeightAGL = ras->getHeightAGL();
	                    _terrainDataModel->getTerrainHeight(longitudeCenter, latitudeCenter, rasTerrainHeight, rasBldgHeight, rasLidarHeightResult, rasHeightSource);

	                    rasCenterPosn = EcefModel::geodeticToEcef(latitudeCenter, longitudeCenter, (rasTerrainHeight + rasHeightAGL)/ 1000.0);
                            rasUpVec = rasCenterPosn.normalized();
                            rasEastVec = (Vector3(-rasUpVec.y(), rasUpVec.x(), 0.0)).normalized();
                            rasNorthVec = rasUpVec.cross(rasEastVec);

                            fkml->writeStartElement("Placemark");
                            fkml->writeTextElement("name", QString("RECT_") + QString::number(rectIdx));
                            fkml->writeTextElement("visibility", "1");
                            fkml->writeTextElement("styleUrl", "#transBluePoly");
                            fkml->writeStartElement("Polygon");
                            fkml->writeTextElement("extrude", "0");
                            fkml->writeTextElement("tessellate", "0");
                            fkml->writeTextElement("altitudeMode", "clampToGround");
                            fkml->writeStartElement("outerBoundaryIs");
                            fkml->writeStartElement("LinearRing");

                            ras_coords = QString();
                            for(ptIdx=0; ptIdx<=numPtsCircle; ++ptIdx) {
                                double phi = 2*M_PI*ptIdx / numPtsCircle;
                                Vector3 circlePtPosn = rasCenterPosn + (circleRadius/1000)*(rasEastVec*cos(phi) + rasNorthVec*sin(phi));

                                GeodeticCoord circlePtPosnGeodetic = EcefModel::ecefToGeodetic(circlePtPosn);

                                ras_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", circlePtPosnGeodetic.longitudeDeg, circlePtPosnGeodetic.latitudeDeg, 0.0));
                            }

                            fkml->writeTextElement("coordinates", ras_coords);
                            fkml->writeEndElement(); // LinearRing
                            fkml->writeEndElement(); // outerBoundaryIs
                            fkml->writeEndElement(); // Polygon
                            fkml->writeEndElement(); // Placemark

                            break;
                        default:
                            CORE_DUMP;
                            break;
                    }
		    fkml->writeEndElement(); // Folder
                }
		fkml->writeEndElement(); // Folder
	}
	/**************************************************************************************/

#if MM_DEBUG
	/**************************************************************************************/
	/* Create pathTraceFile, for debugging                                                */
	/**************************************************************************************/
	auto path_writer = GzipCsvWriter(pathTraceFile);
	auto &ftrace = path_writer.csv_writer;
	ftrace->writeRow({"PT_ID,PT_LON (deg),PT_LAT (deg),HORIZ_DIST (Km),PT_HEIGHT_AMSL (m),BLDG_FLAG"});
	/**************************************************************************************/
#endif

	/**************************************************************************************/
	/* Compute Channel Availability                                                       */
	/**************************************************************************************/
	_zbldg2109 = -qerfi(_confidenceBldg2109);
	_zclutter2108 = -qerfi(_confidenceClutter2108);
	_zwinner2 = -qerfi(_confidenceWinner2);

	const double exclusionDistKmSquared = (_exclusionDist / 1000.0) * (_exclusionDist / 1000.0);
	const double maxRadiusKmSquared = (_maxRadius / 1000.0) * (_maxRadius / 1000.0);

	Vector3 rlanPosnList[3];
	GeodeticCoord rlanCoordList[3];

	if (_rlanRegion->getMinHeightAGL() < _minRlanHeightAboveTerrain) {
		throw std::runtime_error(ErrStream()
				<< std::string("ERROR: Point Analysis: Invalid RLAN parameter settings.") << std::endl
				<< std::string("RLAN Min Height above terrain = ") << _rlanRegion->getMinHeightAGL() << std::endl

				// << std::string("RLAN Height = ") << centerHeight << std::endl
				// << std::string("Height Uncertainty = ") << heightUncertainty << std::endl
				// << std::string("Terrain Height at RLAN Location = ") << rlanTerrainHeight << std::endl
				// << std::string("RLAN is ") << centerHeight - heightUncertainty - rlanTerrainHeight << " meters above terrain" << std::endl

				<< std::string("RLAN must be more than ") << _minRlanHeightAboveTerrain << " meters above terrain" << std::endl
				);
	}
	char *tstr;

#   if MM_DEBUG
	time_t tStartRAS = time(NULL);
	tstr = strdup(ctime(&tStartRAS));
	strtok(tstr, "\n");

	std::cout << "Begin Processing RAS's " << tstr << std::endl << std::flush;

	free(tstr);
#   endif

        int rasIdx;
        double rlanRegionMaxDist = _rlanRegion->getMaxDist();
        double rlanRegionMaxHeightAGL = _rlanRegion->getMaxHeightAGL();
        for (rasIdx = 0; rasIdx < _rasList->getSize(); ++rasIdx) {
            RASClass *ras = (*_rasList)[rasIdx];
            if (ras->intersect(_rlanRegion->getCenterLongitude(), _rlanRegion->getCenterLatitude(), rlanRegionMaxDist, rlanRegionMaxHeightAGL)) {
                int chanIdx;
                for (chanIdx = 0; chanIdx < (int) _channelList.size(); ++chanIdx) {
                    ChannelStruct *channel = &(_channelList[chanIdx]);
                    if (channel->availability != BLACK) {
                        double chanStartFreq = channel->startFreqMHz * 1.0e6;
                        double chanStopFreq = channel->stopFreqMHz * 1.0e6;
                        double spectralOverlap = computeSpectralOverlap(chanStartFreq, chanStopFreq, ras->getStartFreq(), ras->getStopFreq(), false);
                        if (spectralOverlap > 0.0) {
                            channel->availability = BLACK;
                            channel->eirpLimit_dBm = -std::numeric_limits<double>::infinity();
                        }
                    }
                }
            }
        }

#   if MM_DEBUG
	time_t tEndRAS = time(NULL);
	tstr = strdup(ctime(&tEndRAS));
	strtok(tstr, "\n");

	int elapsedTimeRAS = (int) (tEndRAS-tStartRAS);

	int etRAS = elapsedTimeRAS;
	int elapsedTimeRASSec = etRAS % 60;
	etRAS = etRAS / 60;
	int elapsedTimeRASMin = etRAS % 60;
	etRAS = etRAS / 60;
	int elapsedTimeRASHour = etRAS % 24;
	etRAS = etRAS / 24;
	int elapsedTimeRASDay = etRAS;

	std::cout << "End Processing RAS's " << tstr
		<< " Elapsed time = " << (tEndRAS-tStartRAS) << " sec = "
		<< elapsedTimeRASDay  << " days "
		<< elapsedTimeRASHour << " hours "
		<< elapsedTimeRASMin  << " min "
		<< elapsedTimeRASSec  << " sec."
		<< std::endl << std::flush;

	free(tstr);

#   endif

#   if MM_DEBUG
	time_t tStartULS = time(NULL);
	tstr = strdup(ctime(&tStartULS));
	strtok(tstr, "\n");

	std::cout << "Begin Processing ULS RX's " << tstr << std::endl << std::flush;

	free(tstr);

	int traceIdx = 0;
#   endif

	int ulsIdx;
        double *eirpLimitList = (double *) malloc(_ulsList->getSize()*sizeof(double));
        bool *ulsFlagList   = (bool *) malloc(_ulsList->getSize()*sizeof(bool));
	for (ulsIdx = 0; ulsIdx < _ulsList->getSize(); ++ulsIdx) {
            ulsFlagList[ulsIdx] = false;
        }

        int totNumProc = _ulsList->getSize();

        int numPct = 100;

        if (numPct > totNumProc) { numPct = totNumProc; }

        int xN = 1;

        int pctIdx;
        std::chrono::high_resolution_clock::time_point tstart;

	bool cont = true;
	int numProc = 0;
	for (ulsIdx = 0; (ulsIdx < _ulsList->getSize())&&(cont); ++ulsIdx)
	{
		LOGGER_DEBUG(logger) << "considering ULSIdx: " << ulsIdx << '/' << _ulsList->getSize();
		ULSClass *uls = (*_ulsList)[ulsIdx];
		const Vector3 ulsRxPos = uls->getRxPosition();
		Vector3 lineOfSightVectorKm = ulsRxPos - _rlanRegion->getCenterPosn();
		double distKmSquared = (lineOfSightVectorKm).dot(lineOfSightVectorKm);

#if 0
		// For debugging, identifies anomalous ULS entries
		if (uls->getLinkDistance() == -1) {
			std::cout << uls->getID() << std::endl;
		}
#endif

		if ((distKmSquared < maxRadiusKmSquared) && (distKmSquared > exclusionDistKmSquared) && (uls->getLinkDistance() > 0.0))
		{
#           if MM_DEBUG
			time_t t1 = time(NULL);

			bool traceFlag = false;
			if (traceIdx<(int) fsidTraceList.size()) {
				if (uls->getID() == fsidTraceList[traceIdx]) {
					traceFlag = true;
				}
			}
#           endif

			if (_useFixedRxAntennaFeederLoss) {
				uls->setRxAntennaFeederLossDB(_receiverFeedLoss_dB);
			}
			_ulsIdxList.push_back(ulsIdx); // Store the ULS indices that are used in analysis
			int ulsLonIdx;
			int ulsLatIdx;
			int regionIdx;
			char ulsRxPropEnv;
			_popGrid->findDeg(uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), ulsLonIdx, ulsLatIdx, ulsRxPropEnv, regionIdx);
			double ulsPopVal = _popGrid->getPop(ulsLonIdx, ulsLatIdx);
			if (ulsPopVal == 0.0)
			{
				ulsRxPropEnv = 'Z';
			}

                        LatLon ulsRxLatLon = std::pair<double, double>(uls->getRxLatitudeDeg(), uls->getRxLongitudeDeg());
                        bool contains;

                        LatLon ptLatLon = _rlanRegion->closestPoint(ulsRxLatLon, contains);

                        if (contains) {
                            int chanIdx;
			    for (chanIdx = 0; chanIdx < (int) _channelList.size(); ++chanIdx) {
                                ChannelStruct *channel = &(_channelList[chanIdx]);
                                if (channel->availability != BLACK) {
				    double chanStartFreq = channel->startFreqMHz * 1.0e6;
				    double chanStopFreq = channel->stopFreqMHz * 1.0e6;
				    double spectralOverlap = computeSpectralOverlap(chanStartFreq, chanStopFreq, uls->getStartUseFreq(), uls->getStopUseFreq(), _aciFlag);
                                    if (spectralOverlap > 0.0) {
				        double eirpLimit_dBm = -std::numeric_limits<double>::infinity();

				        if (eirpLimit_dBm < channel->eirpLimit_dBm) {
					    channel->eirpLimit_dBm = eirpLimit_dBm;
				        }

                                        if ( (!ulsFlagList[ulsIdx]) || (eirpLimit_dBm < eirpLimitList[ulsIdx]) ) {
                                            eirpLimitList[ulsIdx] = eirpLimit_dBm;
                                            ulsFlagList[ulsIdx] = true;
                                        }
                                    }
                                }
                            }
                            LOGGER_INFO(logger) << "FSID = " << uls->getID() << " is inside specified RLAN region.";
                        } else {
			    int rlanPosnIdx;

                            double centerHeight = _rlanRegion->getCenterHeight();
                            double heightUncertainty = _rlanRegion->getHeightUncertainty();
                            rlanCoordList[0] = GeodeticCoord::fromLatLon(ptLatLon.first, ptLatLon.second, centerHeight/1000.0);
                            rlanCoordList[1] = GeodeticCoord::fromLatLon(ptLatLon.first, ptLatLon.second, (centerHeight+heightUncertainty)/1000.0);
                            rlanCoordList[2] = GeodeticCoord::fromLatLon(ptLatLon.first, ptLatLon.second, (centerHeight-heightUncertainty)/1000.0);

                            rlanPosnList[0] = EcefModel::fromGeodetic(rlanCoordList[0]);
                            rlanPosnList[1] = EcefModel::fromGeodetic(rlanCoordList[1]);
                            rlanPosnList[2] = EcefModel::fromGeodetic(rlanCoordList[2]);

			    int numRlanPosn = ((heightUncertainty == 0.0) ? 1 : 3);

#                           if MM_DEBUG
			        if (traceFlag) {
				    ftrace->writeRow({ QString::asprintf("BEGIN_%d,,,,,-1\n", uls->getID()) });
				    for (rlanPosnIdx = 0; rlanPosnIdx < numRlanPosn; ++rlanPosnIdx) {
					ftrace->writeRow({ QString::asprintf("RLAN_%d,%.10f,%.10f,,%.5f,AMSL\n", rlanPosnIdx,rlanCoordList[rlanPosnIdx].longitudeDeg,rlanCoordList[rlanPosnIdx].latitudeDeg,rlanCoordList[rlanPosnIdx].heightKm*1000.0) } );
				    }
				    ftrace->writeRow({ QString::asprintf("FS_RX,%.10f,%.10f,,%.5f,AMSL\n", uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), uls->getRxHeightAboveTerrain()+uls->getRxTerrainHeight()) } );
			        }
#                           endif

			    CConst::ULSAntennaTypeEnum ulsRxAntennaType = uls->getRxAntennaType();

	                    double rlanTerrainHeight, bldgHeight;
	                    MultibandRasterClass::HeightResult lidarHeightResult;
	                    CConst::HeightSourceEnum rlanHeightSource;
	                    _terrainDataModel->getTerrainHeight(ptLatLon.second, ptLatLon.first, rlanTerrainHeight,bldgHeight, lidarHeightResult, rlanHeightSource);

			    for (rlanPosnIdx = 0; rlanPosnIdx < numRlanPosn; ++rlanPosnIdx)
			    {
				Vector3 rlanPosn = rlanPosnList[rlanPosnIdx];
				GeodeticCoord rlanCoord = rlanCoordList[rlanPosnIdx];
				lineOfSightVectorKm = ulsRxPos - rlanPosn;
				double distKm = lineOfSightVectorKm.len();
				double dAP = rlanPosn.len();
				double elevationAngleRad = M_PI / 2 - acos(rlanPosn.dot(lineOfSightVectorKm) / (dAP * distKm));

                                int chanIdx;
				for (chanIdx = 0; chanIdx < (int) _channelList.size(); ++chanIdx) {
                                    ChannelStruct *channel = &(_channelList[chanIdx]);
                                    if (channel->availability != BLACK) {
				        double chanStartFreq = channel->startFreqMHz * 1.0e6;
				        double chanStopFreq = channel->stopFreqMHz * 1.0e6;
                                        // LOGGER_INFO(logger) << "COMPUTING SPECTRAL OVERLAP FOR FSID = " << uls->getID();
				        double spectralOverlap = computeSpectralOverlap(chanStartFreq, chanStopFreq, uls->getStartUseFreq(), uls->getStopUseFreq(), _aciFlag);
                                        if (spectralOverlap > 0.0) {
					    double bandwidth = chanStopFreq - chanStartFreq;
					    double chanCenterFreq = (chanStartFreq + chanStopFreq)/2;
					    double spectralOverlapLossDB = -10.0 * log(spectralOverlap) / log(10.0);

					    std::string buildingPenetrationModelStr;
					    double buildingPenetrationCDF;
					    double buildingPenetrationDB = computeBuildingPenetration(_buildingType, elevationAngleRad, chanCenterFreq, buildingPenetrationModelStr, buildingPenetrationCDF, true);

					    std::string txClutterStr;
					    std::string pathLossModelStr;
					    double pathLossCDF;
					    std::string pathClutterModelStr;
					    double pathClutterCDF;
					    double pathLoss;
					    double pathClutterDB;

					    double rlanHtAboveTerrain = rlanCoord.heightKm * 1000.0 - rlanTerrainHeight;

					    computePathLoss(rlanPropEnv, distKm, chanCenterFreq,
							    rlanCoord.longitudeDeg, rlanCoord.latitudeDeg, rlanHtAboveTerrain,
							    uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), uls->getRxHeightAboveTerrain(), elevationAngleRad,
							    pathLoss, pathClutterDB, true,
							    pathLossModelStr, pathLossCDF,
							    pathClutterModelStr, pathClutterCDF,
							    (iturp452::ITURP452 *)NULL, &txClutterStr, &(uls->ITMHeightProfile)
#if MM_DEBUG
							    , uls->ITMHeightType
#endif
						           );
	
					    double rxGainDB;
					    double angleOffBoresightDeg = acos(uls->getAntennaPointing().dot(-(lineOfSightVectorKm.normalized()))) * 180.0 / M_PI;
					    if (ulsRxAntennaType == CConst::F1245AntennaType)
					    {
                                                rxGainDB = calcItu1245::CalcITU1245(angleOffBoresightDeg, uls->getRxGain());
					    }
					    else if (ulsRxAntennaType == CConst::F1336OmniAntennaType)
					    {
					        rxGainDB = calcItu1336_4::CalcITU1336_omni_avg(elevationAngleRad * 180.0 / M_PI, uls->getRxGain(), chanCenterFreq);
					    }
					    else if (ulsRxAntennaType == CConst::OmniAntennaType)
					    {
						rxGainDB = 0.0;
					    }
					    else if (ulsRxAntennaType == CConst::LUTAntennaType)
					    {
						rxGainDB = uls->getRxAntenna()->gainDB(angleOffBoresightDeg * M_PI / 180.0) + uls->getRxGain();
					    }
					    else
					    {
						throw std::runtime_error(ErrStream() << "ERROR reading ULS data: ulsRxAntennaType = " << ulsRxAntennaType << " INVALID value");
					    }

					    double rxPowerDBW = (_maxEIRP_dBm - 30.0) - _bodyLossDB - buildingPenetrationDB - pathLoss - pathClutterDB + rxGainDB - spectralOverlapLossDB - _polarizationLossDB - uls->getRxAntennaFeederLossDB();

					    double I2NDB = rxPowerDBW - uls->getNoiseLevelDBW();

					    double marginDB = _IoverN_threshold_dB - I2NDB;

					    double eirpLimit_dBm = _maxEIRP_dBm + marginDB;

					    if (eirpLimit_dBm < channel->eirpLimit_dBm)
					    {
						channel->eirpLimit_dBm = eirpLimit_dBm;
					    }

                                            if ( (!ulsFlagList[ulsIdx]) || (eirpLimit_dBm < eirpLimitList[ulsIdx]) ) {
                                                eirpLimitList[ulsIdx] = eirpLimit_dBm;
                                                ulsFlagList[ulsIdx] = true;
                                            }

					    if ( fexcthrwifi && (std::isnan(rxPowerDBW) || (I2NDB > _visibilityThreshold) || (distKm * 1000 < _closeInDist)) )
					    {
					        double d1;
					        double d2;
					        double pathDifference;
					        double fresnelIndex = -1.0;
					        const Vector3 ulsTxPos = uls->getTxPosition();
					        double ulsLinkDistance = uls->getLinkDistance();
					        double ulsWavelength = CConst::c / ((uls->getStartUseFreq() + uls->getStopUseFreq()) / 2);
					        if (ulsLinkDistance != -1.0)
					        {
						    d1 = (ulsRxPos - rlanPosn).len() * 1000;
						    d2 = (ulsTxPos - rlanPosn).len() * 1000;
						    pathDifference = d1 + d2 - ulsLinkDistance;
						    fresnelIndex = pathDifference / (ulsWavelength / 2);
					        }
					        else
					        {
						    d1 = (ulsRxPos - rlanPosn).len() * 1000;
						    d2 = -1.0;
						    pathDifference = -1.0;
					        }

					        std::string rxAntennaTypeStr;
					        if (ulsRxAntennaType == CConst::F1245AntennaType)
					        {
					    	    rxAntennaTypeStr = "F.1245";
					        }
					        else if (ulsRxAntennaType == CConst::F1336OmniAntennaType)
					        {
					    	    rxAntennaTypeStr = "F.1336_OMNI";
					        }
					        else if (ulsRxAntennaType == CConst::OmniAntennaType)
					        {
					    	    rxAntennaTypeStr = "OMNI";
					        }
					        else if (ulsRxAntennaType == CConst::LUTAntennaType)
					        {
					    	    rxAntennaTypeStr = std::string(uls->getRxAntenna()->get_strid());
					        }
					        else
					        {
					    	    throw std::runtime_error(ErrStream() << "ERROR reading ULS data: ulsRxAntennaType = " << ulsRxAntennaType << " INVALID value");
					        }

					        std::string bldgTypeStr = (_fixedBuildingLossFlag ? "INDOOR_FIXED" :
					    		    _buildingType == CConst::noBuildingType ? "OUTDOOR" :
					    		    _buildingType == CConst::traditionalBuildingType ?  "TRADITIONAL" :
					    		    "THERMALLY_EFFICIENT");

					        QStringList msg;
					        msg << QString::number(uls->getID()) << QString::number(rlanPosnIdx) << QString::fromStdString(uls->getCallsign());
					        msg << QString::number(uls->getRxLongitudeDeg(), 'f', 5) << QString::number(uls->getRxLatitudeDeg(), 'f', 5);
					        msg << QString::number(uls->getRxHeightAboveTerrain(), 'f', 2) << QString::number(uls->getRxTerrainHeight(), 'f', 2) << 
					        QString::fromStdString(_terrainDataModel->getSourceName(uls->getRxHeightSource())) <<
					        QString(ulsRxPropEnv);
					        msg << QString::number(rlanCoord.longitudeDeg, 'f', 5) << QString::number(rlanCoord.latitudeDeg, 'f', 5);
					        msg << QString::number(rlanCoord.heightKm * 1000.0 - rlanTerrainHeight, 'f', 1) << QString::number(rlanTerrainHeight, 'f', 1) << 
					        QString::fromStdString(_terrainDataModel->getSourceName(rlanHeightSource)) <<
					        CConst::strPropEnvList->type_to_str(rlanPropEnv);
					        msg << QString::number(distKm, 'f', 3) << QString::number(elevationAngleRad * 180.0 / M_PI, 'f', 3) << QString::number(angleOffBoresightDeg);
					        msg << QString::number(_maxEIRP_dBm, 'f', 3) << QString::number(_bodyLossDB, 'f', 3) << QString::fromStdString(txClutterStr) << QString::fromStdString(bldgTypeStr);
					        msg << QString::number(buildingPenetrationDB, 'f', 3) << QString::fromStdString(buildingPenetrationModelStr) << QString::number(buildingPenetrationCDF, 'f', 8);
					        msg << QString::number(pathLoss, 'f', 3) << QString::fromStdString(pathLossModelStr) << QString::number(pathLossCDF, 'f', 8);

					        msg << QString::number(pathClutterDB, 'f', 3) << QString::fromStdString(pathClutterModelStr) << QString::number(pathClutterCDF, 'f', 8);

					        msg << QString::number(bandwidth * 1.0e-6, 'f', 0) << QString::number(chanStartFreq * 1.0e-6, 'f', 0) << QString::number(chanStopFreq * 1.0e-6, 'f', 0)
					    	    << QString::number(uls->getStartUseFreq() * 1.0e-6, 'f', 2) << QString::number(uls->getStopUseFreq() * 1.0e-6, 'f', 2);

					        msg << QString::fromStdString(rxAntennaTypeStr) << QString::number(uls->getRxGain(), 'f', 3);
					        msg << QString::number(rxGainDB, 'f', 3) << QString::number(spectralOverlapLossDB, 'f', 3);
					        msg << QString::number(_polarizationLossDB, 'f', 3);
					        msg << QString::number(uls->getRxAntennaFeederLossDB(), 'f', 3);
					        msg << QString::number(rxPowerDBW, 'f', 3) << QString::number(rxPowerDBW - uls->getNoiseLevelDBW(), 'f', 3);
					        msg << QString::number(ulsLinkDistance, 'f', 3) << QString::number(chanCenterFreq, 'f', 3) << QString::number(d2, 'f', 3) << QString::number(pathDifference, 'f', 6);
					        msg << QString::number(ulsWavelength * 1000, 'f', 3) << QString::number(fresnelIndex, 'f', 3);

					        fexcthrwifi->writeRow(msg);
					    }

#if 0
					    int ellipseIdx, heightIdx;
					    for (ellipseIdx=0; ellipseIdx<(int) uncertEllipse.size(); ++ellipseIdx) {
						double rlanLat = uncertEllipse[ellipseIdx].first;
						double rlanLon = uncertEllipse[ellipseIdx].second;
						for(heightIdx=-nh; heightIdx<=nh; heightIdx++) {
							double rlanHeight = centerHeight + heightIdx*_rlanHeightResolution;
							Vector3 rlanPosn = EcefModel::geodeticToEcef(rlanLat, rlanLon, rlanHeight / 1000.0);

							lineOfSightVectorKm = ulsRxPos - rlanPosn;

							const double distKm = lineOfSightVectorKm.len();

							double pathLoss;
							double pathClutterDB;
							double dAP = rlanPosn.len();
							double elevationAngleRad = M_PI/2 - acos(rlanPosn.dot(lineOfSightVectorKm)/(dAP*distKm));

							std::string buildingPenetrationModelStr;
							double buildingPenetrationCDF;
							double buildingPenetrationDB = computeBuildingPenetration(_buildingType, elevationAngleRad, chanCenterFreq, buildingPenetrationModelStr, buildingPenetrationCDF, true);


						}
					    }
#endif
				        }
				    }
				}
			    }
			}

#           if MM_DEBUG
			time_t t2 = time(NULL);
			tstr = strdup(ctime(&t2));
			strtok(tstr, "\n");

			std::cout << numProc << " [" << ulsIdx+1 << " / " <<  _ulsList->getSize() << "] " << tstr << " Elapsed Time = " << (t2-t1) << std::endl << std::flush;

			free(tstr);

			if (false&&(numProc == 10)) {
				cont = false;
			}

			if (traceFlag&&(!contains)) {
				traceIdx++;
				if (uls->ITMHeightProfile) {
					double rlanLon = rlanCoordList[0].longitudeDeg;
					double rlanLat = rlanCoordList[0].latitudeDeg;
					double fsLon = uls->getRxLongitudeDeg();
					double fsLat = uls->getRxLatitudeDeg();
					int N = ((int) uls->ITMHeightProfile[0]) + 1;
				        Vector3 rlanCenterPosn = rlanPosnList[0];
				        lineOfSightVectorKm = ulsRxPos - rlanCenterPosn;
                                        Vector3 upVec = rlanCenterPosn.normalized();
                                        Vector3 horizVec = (lineOfSightVectorKm - (lineOfSightVectorKm.dot(upVec) * upVec));
					double horizDistKm = sqrt((horizVec).dot(horizVec));
					for(int ptIdx=0; ptIdx<N; ptIdx++) {
						double ptLon = ( rlanLon * (N-1-ptIdx) + fsLon*ptIdx ) / (N-1);
						double ptLat = ( rlanLat * (N-1-ptIdx) + fsLat*ptIdx ) / (N-1);
						double ptDistKm = horizDistKm*ptIdx / (N-1);

						ftrace->writeRow({ QString::asprintf("PT_%d", ptIdx),
								QString::number(ptLon, 'f', 10),
								QString::number(ptLat, 'f', 10),
								QString::number(ptDistKm, 'f', 5),
								QString::number(uls->ITMHeightProfile[2+ptIdx], 'f', 5),
								});

					}
				}
				ftrace->writeRow({
						QString::asprintf("END_%d", uls->getID()),
						QString(),
						QString(),
						QString(),
						QString(),
						QString(),
						QString::number(-1)
						});

			}
#           endif

			if (uls->ITMHeightProfile) {
				free(uls->ITMHeightProfile);
				uls->ITMHeightProfile = (double *) NULL;
			}

		}
		else
		{
			// uls is not included in calculations
			//LOGGER_DEBUG(logger) << "ID: " << uls->getID() << ", distKm: " << distKmSquared << ", link: " << uls->getLinkDistance() << ",";
		}

                numProc++;

                if (numProc == xN) {
                    if (xN == 1) {
                        tstart = std::chrono::high_resolution_clock::now();
                        pctIdx = 1;
                    } else {
                        auto tcurrent = std::chrono::high_resolution_clock::now();
                        double elapsedTime = std::chrono::duration_cast<std::chrono::duration<double>>(tcurrent-tstart).count();
                        double remainingTime = elapsedTime*(totNumProc-numProc)/(numProc-1);

                        std::ofstream progressFile;
                        progressFile.open(_progressFile);
                        progressFile << (int) std::floor(100.0*numProc/totNumProc) << std::endl
                            << "Elapsed Time: " << (int) std::floor(elapsedTime)
                            << " s, Remaining: " << (int) std::floor(remainingTime) << " s";
                        progressFile.close();

                        // printf("%f %%  :  Elapsed Time = %f seconds, Remaining %f seconds\n", 100.0*numProc/totNumProc, elapsedTime, remainingTime);
                        pctIdx++;
                    }
                    xN = ((totNumProc-1)*pctIdx + numPct-1)/numPct + 1;
               }
	}

        for (int colorIdx=0; (colorIdx<3)&&(fkml); ++colorIdx) {
            fkml->writeStartElement("Folder");
            std::string visibilityStr;
            int addPlacemarks;
            std::string placemarkStyleStr;
            std::string polyStyleStr;
            if (colorIdx == 0) {
                fkml->writeTextElement("name", "RED");	
                visibilityStr = "1";
                addPlacemarks = 1;
                placemarkStyleStr = "#redPlacemark";
                polyStyleStr = "#redPoly";
            } else if (colorIdx == 1) {
                fkml->writeTextElement("name", "YELLOW");	
                visibilityStr = "1";
                addPlacemarks = 1;
                placemarkStyleStr = "#yellowPlacemark";
                polyStyleStr = "#yellowPoly";
            } else {
                fkml->writeTextElement("name", "GREEN");	
                visibilityStr = "0";
                addPlacemarks = 0;
                placemarkStyleStr = "#greenPlacemark";
                polyStyleStr = "#greenPoly";
            }
            fkml->writeTextElement("visibility", visibilityStr.c_str());

            for (ulsIdx = 0; ulsIdx < _ulsList->getSize(); ulsIdx++) {
                bool useFlag = ulsFlagList[ulsIdx];

                if (useFlag) {
                    if (colorIdx == 0) {
                        useFlag = ((eirpLimitList[ulsIdx] < _minEIRP_dBm) ? true : false);
                    } else if (colorIdx == 1) {
                        useFlag = (((eirpLimitList[ulsIdx] < _maxEIRP_dBm) && (eirpLimitList[ulsIdx] >= _minEIRP_dBm)) ? true : false);
                    } else if (colorIdx == 2) {
                        useFlag = ((eirpLimitList[ulsIdx] >= _maxEIRP_dBm) ? true : false);
                    }
                }
                if (useFlag) {
		        ULSClass *uls = (*_ulsList)[ulsIdx];
		        const Vector3 ulsRxPos = uls->getRxPosition();
			if (fkml) {
				double beamWidthDeg = uls->computeBeamWidth(3.0);
				double beamWidthRad = beamWidthDeg*(M_PI/180.0);

				const Vector3 ulsTxPos = uls->getTxPosition();
				double linkDistKm = (ulsTxPos - ulsRxPos).len();

				double ulsRxHeight = uls->getRxHeightAboveTerrain() + uls->getRxTerrainHeight();
				double ulsTxHeight = uls->getTxHeightAboveTerrain() + uls->getTxTerrainHeight();

				Vector3 zvec = (ulsTxPos-ulsRxPos).normalized();
				Vector3 xvec = (Vector3(zvec.y(), -zvec.x(),0.0)).normalized();
				Vector3 yvec = zvec.cross(xvec);

				int numCvgPoints = 32;
				fkml->writeStartElement("Folder");
				fkml->writeTextElement("name", QString::fromStdString(std::to_string(uls->getID())));	
				// fkml->writeTextElement("name", QString::fromStdString(uls->getCallsign()));	

				std::vector<GeodeticCoord> ptList;
				double cvgTheta = beamWidthRad;
				int cvgPhiIdx;
				for(cvgPhiIdx=0; cvgPhiIdx<numCvgPoints; ++cvgPhiIdx) {
					double cvgPhi = 2*M_PI*cvgPhiIdx / numCvgPoints;
					Vector3 cvgIntPosn = ulsRxPos + linkDistKm*(zvec*cos(cvgTheta) + (xvec*cos(cvgPhi) + yvec*sin(cvgPhi))*sin(cvgTheta));

					GeodeticCoord cvgIntPosnGeodetic = EcefModel::ecefToGeodetic(cvgIntPosn);
					ptList.push_back(cvgIntPosnGeodetic);
				}

                                if (addPlacemarks) {
                                    fkml->writeStartElement("Placemark");
                                    fkml->writeTextElement("name", QString::asprintf("RX %d", uls->getID()));
                                    fkml->writeTextElement("visibility", "1");
                                    fkml->writeTextElement("styleUrl", placemarkStyleStr.c_str());
                                    fkml->writeStartElement("Point");
                                    fkml->writeTextElement("altitudeMode", "absolute");
                                    fkml->writeTextElement("coordinates", QString::asprintf("%.10f,%.10f,%.2f", uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), ulsRxHeight));
                                    fkml->writeEndElement(); // Point
                                    fkml->writeEndElement(); // Placemark

                                    fkml->writeStartElement("Placemark");
                                    fkml->writeTextElement("name", QString::asprintf("TX %d", uls->getID()));
                                    fkml->writeTextElement("visibility", "1");
                                    fkml->writeTextElement("styleUrl", placemarkStyleStr.c_str());
                                    fkml->writeStartElement("Point");
                                    fkml->writeTextElement("altitudeMode", "absolute");
                                    fkml->writeTextElement("coordinates", QString::asprintf("%.10f,%.10f,%.2f", uls->getTxLongitudeDeg(), uls->getTxLatitudeDeg(), ulsTxHeight));
                                    fkml->writeEndElement(); // Point
                                    fkml->writeEndElement(); // Placemark
                                }

				fkml->writeStartElement("Folder");
				fkml->writeTextElement("name", "Beamcone");	

				for(cvgPhiIdx=0; cvgPhiIdx<numCvgPoints; ++cvgPhiIdx) {
					fkml->writeStartElement("Placemark");
					fkml->writeTextElement("name", QString::asprintf("p%d", cvgPhiIdx));
					fkml->writeTextElement("styleUrl", polyStyleStr.c_str());
                                        fkml->writeTextElement("visibility", visibilityStr.c_str());
					fkml->writeStartElement("Polygon");	
					fkml->writeTextElement("extrude", "0");
					fkml->writeTextElement("altitudeMode", "absolute");
					fkml->writeStartElement("outerBoundaryIs");
					fkml->writeStartElement("LinearRing");

					QString more_coords = QString::asprintf("%.10f,%.10f,%.2f\n", uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), ulsRxHeight);

					GeodeticCoord pt = ptList[cvgPhiIdx];
					more_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", pt.longitudeDeg, pt.latitudeDeg, pt.heightKm*1000.0));

					pt = ptList[(cvgPhiIdx +1) % numCvgPoints];
					more_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", pt.longitudeDeg, pt.latitudeDeg, pt.heightKm*1000.0));

					more_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), ulsRxHeight));

					fkml->writeTextElement("coordinates", more_coords);
					fkml->writeEndElement(); // LinearRing
					fkml->writeEndElement(); // outerBoundaryIs
					fkml->writeEndElement(); // Polygon
					fkml->writeEndElement(); // Placemark
				}
				fkml->writeEndElement(); // Beamcone

				fkml->writeEndElement(); // Folder
			}
                }
            }
            fkml->writeEndElement(); // Folder
        }

	if (fkml) {
		fkml->writeEndElement(); // Document
		fkml->writeEndElement(); // kml
		fkml->writeEndDocument();
	}

	if (numProc == 0) {
		errStr << "Analysis region contains no FS receivers";
		LOGGER_WARN(logger) << errStr.str();
		statusMessageList.push_back(errStr.str());
	}

#   if MM_DEBUG
	time_t tEndULS = time(NULL);
	tstr = strdup(ctime(&tEndULS));
	strtok(tstr, "\n");

	int elapsedTime = (int) (tEndULS-tStartULS);

	int et = elapsedTime;
	int elapsedTimeSec = et % 60;
	et = et / 60;
	int elapsedTimeMin = et % 60;
	et = et / 60;
	int elapsedTimeHour = et % 24;
	et = et / 24;
	int elapsedTimeDay = et;

	std::cout << "End Processing ULS RX's " << tstr
		<< " Elapsed time = " << (tEndULS-tStartULS) << " sec = "
		<< elapsedTimeDay  << " days "
		<< elapsedTimeHour << " hours "
		<< elapsedTimeMin  << " min "
		<< elapsedTimeSec  << " sec."
		<< std::endl << std::flush;

	free(tstr);

#   endif
	/**************************************************************************************/

	_terrainDataModel->printStats();

        int chanIdx;
	for (chanIdx = 0; chanIdx < (int) _channelList.size(); ++chanIdx) {
            ChannelStruct *channel = &(_channelList[chanIdx]);

            if (channel->availability != BLACK) {
	        if (channel->eirpLimit_dBm == _maxEIRP_dBm)
	        {
	    	    channel->availability = GREEN;
	        }
	        else if (channel->eirpLimit_dBm >= _minEIRP_dBm)
	        {
	    	    channel->availability = YELLOW;
	        }
	        else
	        {
	    	    channel->availability = RED;
	        }
	    }
	}

        free(eirpLimitList);
        free(ulsFlagList);
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::runExclusionZoneAnalysis()                                                 */
/******************************************************************************************/
void AfcManager::runExclusionZoneAnalysis()
{

#if MM_DEBUG
    // std::vector<int> fsidTraceList{2128, 3198, 82443};
    // std::vector<int> fsidTraceList{64324};
    std::vector<int> fsidTraceList{66423};
    std::string pathTraceFile = "path_trace.csv.gz";
#endif

    LOGGER_INFO(logger) << "Executing AfcManager::runExclusionZoneAnalysis()";

    int numContourPoints = 360;

    /**************************************************************************************/
    /* Allocate / Initialize Exclusion Zone Point Vector                                  */
    /**************************************************************************************/
    _exclusionZone.resize(numContourPoints);
    /**************************************************************************************/

#if 0
    /**************************************************************************************/
    /* Create list of RLAN bandwidths                                                     */
    /**************************************************************************************/
    std::vector<std::string> rlanBWStrList = split(_rlanBWStr, ','); // Splits comma-separated list containing the different bandwidths

    _numChan = (int *)malloc(rlanBWStrList.size() * sizeof(int));

    int bwIdx;
    for (bwIdx = 0; bwIdx < (int)rlanBWStrList.size(); ++bwIdx) { 
        // Iterate over each bandwidth (not channels yet)
        char *chptr;
        double bandwidth = std::strtod(rlanBWStrList[bwIdx].c_str(), &chptr);
        _rlanBWList.push_back(bandwidth); // Load the bandwidth list with the designated bandwidths

        int numChan = (int)floor((_wlanMaxFreq - _wlanMinFreq) / bandwidth + 1.0e-6);
        _numChan[bwIdx] = numChan;
    }
    /**************************************************************************************/
#endif

    ULSClass *uls = findULSID(_exclusionZoneFSID);

    ChannelStruct channel = _channelList[_exclusionZoneRLANChanIdx];

    double bandwidth = (channel.stopFreqMHz - channel.startFreqMHz)*1.0e6;

    double chanStopFreq = channel.stopFreqMHz*1.0e6;
    double chanStartFreq = channel.startFreqMHz*1.0e6;
    double spectralOverlap = computeSpectralOverlap(chanStartFreq, chanStopFreq, uls->getStartUseFreq(), uls->getStopUseFreq(), _aciFlag);
    double chanCenterFreq = (chanStartFreq + chanStopFreq)/2;

    if (spectralOverlap == 0.0) {
        throw std::runtime_error(ErrStream() << "ERROR: Specified RLAN spectrum does not overlap FS spectrum. FSID: " << _exclusionZoneFSID
                << " goes from " << uls->getStartUseFreq() / 1.0e6 << " MHz to " << uls->getStopUseFreq() / 1.0e6 << " MHz");
    }
    LOGGER_INFO(logger) << "FSID = " << _exclusionZoneFSID << " found";
    LOGGER_INFO(logger) << "LON: " << uls->getRxLongitudeDeg();
    LOGGER_INFO(logger) << "LAT: " << uls->getRxLatitudeDeg();
    double spectralOverlapLossDB = -10.0 * log(spectralOverlap) / log(10.0);
    LOGGER_INFO(logger) << "SPECTRAL_OVERLAP_LOSS (dB) = " << spectralOverlapLossDB;

    /**************************************************************************************/
    /* Create excThrFile, useful for debugging                                            */
    /**************************************************************************************/
    auto exc_writer = GzipCsvWriter(_excThrFile);
    auto &fexcthrwifi = exc_writer.csv_writer;

    if (fexcthrwifi) {
        fexcthrwifi->writeRow({ "FS_ID",
                "RLAN_POSN_IDX",
                "CALLSIGN",
                "FS_RX_LONGITUDE (deg)",
                "FS_RX_LATITUDE (deg)",
                "FS_RX_HEIGHT_ABOVE_TERRAIN (m)",
                "FS_RX_TERRAIN_HEIGHT (m)",
				"FS_RX_TERRAIN_SOURCE",
                "FS_RX_PROP_ENV",
                "RLAN_LONGITUDE (deg)",
                "RLAN_LATITUDE (deg)",
                "RLAN_HEIGHT_ABOVE_TERRAIN (m)",
                "RLAN_TERRAIN_HEIGHT (m)",
				"RLAN_TERRAIN_SOURCE",
                "LAN_PROP_ENV",
                "RLAN_FS_RX_DIST (km)",
                "RLAN_FS_RX_ELEVATION_ANGLE (deg)",
                "FS_RX_ANGLE_OFF_BORESIGHT (deg)",
                "RLAN_TX_EIRP (dBm)",
                "BODY_LOSS (dB)",
                "RLAN_CLUTTER_CATEGORY",
                "BUILDING TYPE",
                "RLAN_FS_RX_BUILDING_PENETRATION (dB)",
                "BUILDING_PENETRATION_MODEL",
                "BUILDING_PENETRATION_CDF",
                "PATH_LOSS (dB)",
                "PATH_LOSS_MODEL",
                "PATH_LOSS_CDF",
                "PATH_CLUTTER (DB)",
                "PATH_CLUTTER_MODEL",
                "PATH_CLUTTER_CDF",
                "RLAN BANDWIDTH (MHz)",
                "RLAN CHANNEL START FREQ (MHz)",
                "RLAN CHANNEL STOP FREQ (MHz)",
                "ULS START FREQ (MHz)",
                "ULS STOP FREQ (MHz)",
                "FS_ANT_TYPE",
                "FS_ANT_GAIN_PEAK (dB)",
                "FS_ANT_GAIN_TO_RLAN (dB)",
                "RX_SPECTRAL_OVERLAP_LOSS (dB)",
                "POLARIZATION_LOSS (dB)",
                "FS_RX_FEEDER_LOSS (dB)",
                "FS_RX_PWR (dBW)",
                "FS I/N (dB)",
                "ULS_LINK_DIST (m)",
                "RLAN_CENTER_FREQ (Hz)",
                "FS_TX_TO_RLAN_DIST (m)",
                "PATH_DIFFERENCE (m)",
                "ULS_WAVELENGTH (mm)",
                "FRESNEL_INDEX",
                "COMMENT"
        });

    }
    /**************************************************************************************/

#if MM_DEBUG
	/**************************************************************************************/
	/* Create pathTraceFile, for debugging                                                */
	/**************************************************************************************/
	auto path_writer = GzipCsvWriter(pathTraceFile);
	auto &ftrace = path_writer.csv_writer;

	ftrace->writeRow({ "PT_ID",
			"PT_LON (deg)",
			"PT_LAT (deg)",
			"PT_HEIGHT_AMSL (m)",
			"BLDG_FLAG"});
	/**************************************************************************************/
#endif

	if (_useFixedRxAntennaFeederLoss) {
		uls->setRxAntennaFeederLossDB(_receiverFeedLoss_dB);
	}

	int ulsLonIdx;
	int ulsLatIdx;
	int regionIdx;
	char ulsRxPropEnv;
	_popGrid->findDeg(uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), ulsLonIdx, ulsLatIdx, ulsRxPropEnv, regionIdx);
	double ulsPopVal = _popGrid->getPop(ulsLonIdx, ulsLatIdx);
	if (ulsPopVal == 0.0) {
		ulsRxPropEnv = 'Z';
	}

	/**************************************************************************************/
	/* Compute Exclusion Zone                                                             */
	/**************************************************************************************/
	LOGGER_INFO(logger) << "Begin computing exclusion zone";
	_zbldg2109 = -qerfi(_confidenceBldg2109);
	_zclutter2108 = -qerfi(_confidenceClutter2108);
	_zwinner2 = -qerfi(_confidenceWinner2);

        /******************************************************************************/
        /* Estimate dist for which FSPL puts I/N below threshold                      */
        /******************************************************************************/
        double pathLossDB = (_exclusionZoneRLANEIRPDBm - 30.0) - _bodyLossDB + uls->getRxGain()
                          - spectralOverlapLossDB - _polarizationLossDB - uls->getRxAntennaFeederLossDB()
                          - uls->getNoiseLevelDBW() - _IoverN_threshold_dB;

        double dFSPL = exp(pathLossDB*log(10.0)/20.0)*CConst::c/(4*M_PI*chanCenterFreq);
        /******************************************************************************/

        double initialD0 = (dFSPL*180.0 / (CConst::earthRadius*M_PI*cos(uls->getRxLatitudeDeg()*M_PI/180.0)));

        const double minPossibleRadius = 10.0;
        double minPossibleD = (minPossibleRadius*180.0 / (CConst::earthRadius*M_PI));

        double distKm0, distKm1, distKmM, maxDistKm;
	int exclPtIdx;

        int totNumProc = numContourPoints;

        int numPct = 100;

        if (numPct > totNumProc) { numPct = totNumProc; }

        int xN = 1;

        int pctIdx;
        std::chrono::high_resolution_clock::time_point tstart;

        int numProc = 0;

	for(exclPtIdx=0; exclPtIdx<numContourPoints; exclPtIdx++) {
		LOGGER_DEBUG(logger) << "computing exlPtIdx: " << exclPtIdx << '/' << numContourPoints;
            double cc = cos(exclPtIdx*2*M_PI/numContourPoints);
            double ss = sin(exclPtIdx*2*M_PI/numContourPoints);

            bool cont;

            /******************************************************************************/
            /* Step 1: Compute margin at dFSPL, If this margin is not positive something  */
            /* is seriously wrong.                                                        */
            /******************************************************************************/
            double margin0;
            double d0 = initialD0;
            do {
                margin0 = computeIToNMargin(d0, cc, ss, uls, chanCenterFreq, bandwidth,
                                            chanStartFreq, chanStopFreq, spectralOverlapLossDB, ulsRxPropEnv, distKm0, "", nullptr);

                if (margin0 < 0.0) {
                    d0 *= 1.1;
                    cont = true;
                    printf("FSID = %d, EXCL_PT_IDX = %d, dFSPL = %.1f DIST = %.1f margin = %.3f\n", uls->getID(), exclPtIdx, dFSPL, 1000*distKm0, margin0);
                } else {
                    cont = false;
                }
            } while(cont);
            /******************************************************************************/
            // printf("exclPtIdx = %d, dFSPL = %.3f margin = %.3f\n", exclPtIdx, dFSPL, margin0);

            bool minRadiusFlag = false;
            /******************************************************************************/
            /* Step 2: Bound position for which margin = 0                                */
            /******************************************************************************/
            double d1, margin1;
            cont = true;
            do {
                d1 = d0*0.95;
                margin1 = computeIToNMargin(d1, cc, ss, uls, chanCenterFreq, bandwidth,
                                            chanStartFreq, chanStopFreq, spectralOverlapLossDB, ulsRxPropEnv, distKm1, "", nullptr);

                if (d1 <= minPossibleD) {
                    d0 = d1;
                    margin0 = margin1;
                    distKm0 = distKm1;
                    minRadiusFlag = true;
                    cont = false;
                } else if (margin1>=0.0) {
                    d0 = d1;
                    margin0 = margin1;
                    distKm0 = distKm1;
                } else {
                    cont = false;
                }
            } while(cont);
            // printf("Position Bounded [%.10f, %.10f]\n", d1, d0);
            /**********************************************************************************/

            if (!minRadiusFlag) {
            /******************************************************************************/
            /* Step 3: Shrink interval to find where margin = 0                           */
            /******************************************************************************/
            while(d0-d1 > 1.0e-6) {
                double dm = (d1 + d0)/2;
                double marginM = computeIToNMargin(dm, cc, ss, uls, chanCenterFreq, bandwidth,
                                                   chanStartFreq, chanStopFreq, spectralOverlapLossDB, ulsRxPropEnv, distKmM, "", nullptr);
                if (marginM < 0.0) {
                    d1 = dm;
                    margin1 = marginM;
                    distKm1 = distKmM;
                } else {
                    d0 = dm;
                    margin0 = marginM;
                    distKm0 = distKmM;
                }
            }
            /**********************************************************************************/
            }

            margin1 = computeIToNMargin(d1, cc, ss, uls, chanCenterFreq, bandwidth,
                                        chanStartFreq, chanStopFreq, spectralOverlapLossDB, ulsRxPropEnv, distKm1, "Above Thr", fexcthrwifi.get());
            margin0 = computeIToNMargin(d0, cc, ss, uls, chanCenterFreq, bandwidth,
                                        chanStartFreq, chanStopFreq, spectralOverlapLossDB, ulsRxPropEnv, distKm0, "Below Thr", fexcthrwifi.get());


            double rlanLon = uls->getRxLongitudeDeg() + d0*cc;
            double rlanLat = uls->getRxLatitudeDeg() + d0*ss;

            _exclusionZone[exclPtIdx] = std::make_pair(rlanLon, rlanLat);

            // LOGGER_DEBUG(logger) << std::setprecision(15) << exclusionZonePtLon << " " << exclusionZonePtLat;

            numProc++;

            if (numProc == xN) {
                if (xN == 1) {
                    tstart = std::chrono::high_resolution_clock::now();
                    pctIdx = 1;
                } else {
                    auto tcurrent = std::chrono::high_resolution_clock::now();
                    double elapsedTime = std::chrono::duration_cast<std::chrono::duration<double>>(tcurrent-tstart).count();
                    double remainingTime = elapsedTime*(totNumProc-numProc)/(numProc-1);

                    std::ofstream progressFile;
                    progressFile.open(_progressFile);
                    progressFile << (int) std::floor(100.0*numProc/totNumProc) << std::endl
                        << "Elapsed Time: " << (int) std::floor(elapsedTime)
                        << " s, Remaining: " << (int) std::floor(remainingTime) << " s";
                    progressFile.close();

                    // printf("%f %%  :  Elapsed Time = %f seconds, Remaining %f seconds\n", 100.0*numProc/totNumProc, elapsedTime, remainingTime);
                    pctIdx++;
                }
                xN = ((totNumProc-1)*pctIdx + numPct-1)/numPct + 1;
            }

	}
	LOGGER_INFO(logger) << "Done computing exclusion zone";

	_exclusionZoneFSTerrainHeight = uls->getRxTerrainHeight();
	_exclusionZoneHeightAboveTerrain = uls->getRxHeightAboveTerrain();

	writeKML();

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::computeIToNMargin()                                                        */
/******************************************************************************************/
double AfcManager::computeIToNMargin(double d, double cc, double ss, ULSClass *uls, double chanCenterFreq, double bandwidth, double chanStartFreq, double chanStopFreq, double spectralOverlapLossDB, char ulsRxPropEnv, double& distKmM, std::string comment, CsvWriter *fexcthrwifi)
{
	Vector3 rlanPosnList[3];
	GeodeticCoord rlanCoordList[3];

	double rlanLon, rlanLat, rlanHeight;
	rlanLon = uls->getRxLongitudeDeg() + d*cc;
	rlanLat = uls->getRxLatitudeDeg()  + d*ss;


	double fsHeight = uls->getRxTerrainHeight() + uls->getRxHeightAboveTerrain();

	double rlanHeightInput = std::get<2>(_rlanLLA);
	double heightUncertainty = std::get<2>(_rlanUncerts_m);

	double rlanTerrainHeight, bldgHeight;
	MultibandRasterClass::HeightResult lidarHeightResult;
	CConst::HeightSourceEnum rlanHeightSource;
	_terrainDataModel->getTerrainHeight(rlanLon,rlanLat, rlanTerrainHeight,bldgHeight, lidarHeightResult, rlanHeightSource);
	if (_rlanHeightType == "AMSL") {
		rlanHeight = rlanHeightInput;
	} else if (_rlanHeightType == "AGL") {
		rlanHeight = rlanHeightInput + rlanTerrainHeight;
	} else {
		throw std::runtime_error(ErrStream() << "ERROR: INVALID_VALUE _rlanHeightType = " << _rlanHeightType);
	}

	if (rlanHeight - heightUncertainty - rlanTerrainHeight < _minRlanHeightAboveTerrain) {
		throw std::runtime_error(ErrStream() 
				<< std::string("ERROR: ItoN: Invalid RLAN parameter settings.") << std::endl
				<< std::string("RLAN Height = ") << rlanHeight << std::endl
				<< std::string("Height Uncertainty = ") << heightUncertainty << std::endl
				<< std::string("Terrain Height at RLAN Location = ") << rlanTerrainHeight << std::endl
				<< std::string("RLAN is ") << rlanHeight - heightUncertainty - rlanTerrainHeight << " meters above terrain" << std::endl
				<< std::string("RLAN must be more than ") << _minRlanHeightAboveTerrain << " meters above terrain" << std::endl
				);
	}

	Vector3 rlanCenterPosn = EcefModel::geodeticToEcef(rlanLat, rlanLon, rlanHeight / 1000.0);

	Vector3 rlanPosn0;
	if ((rlanHeight - heightUncertainty < fsHeight) && (rlanHeight + heightUncertainty > fsHeight)) {
		rlanPosn0 = EcefModel::geodeticToEcef(rlanLat, rlanLon, fsHeight / 1000.0);
	} else {
		rlanPosn0 = rlanCenterPosn;
	}

	/**************************************************************************************/
	/* Determine propagation environment of RLAN using centerLat, centerLon, popGrid      */
	/**************************************************************************************/
	CConst::PropEnvEnum rlanPropEnv = computeRlanPropEnv(rlanLon, rlanLat, false);
	/**************************************************************************************/

	Vector3 upVec = rlanCenterPosn.normalized();
	const Vector3 ulsRxPos = uls->getRxPosition();

	rlanPosnList[0] = rlanPosn0;                                                  // RLAN Position
	rlanPosnList[1] = rlanCenterPosn + (heightUncertainty / 1000.0) * upVec;      // RLAN Position raised by height uncertainty
	rlanPosnList[2] = rlanCenterPosn - (heightUncertainty / 1000.0) * upVec;      // RLAN Position lowered by height uncertainty

	rlanCoordList[0] = EcefModel::toGeodetic(rlanPosnList[0]);
	rlanCoordList[1] = EcefModel::toGeodetic(rlanPosnList[1]);
	rlanCoordList[2] = EcefModel::toGeodetic(rlanPosnList[2]);

	int numRlanPosn = ((heightUncertainty == 0.0) ? 1 : 3);

	CConst::ULSAntennaTypeEnum ulsRxAntennaType = uls->getRxAntennaType();

	int rlanPosnIdx;

	double minMarginDB = 0.0;
	for (rlanPosnIdx = 0; rlanPosnIdx < numRlanPosn; ++rlanPosnIdx) {
		Vector3 rlanPosn = rlanPosnList[rlanPosnIdx];
		GeodeticCoord rlanCoord = rlanCoordList[rlanPosnIdx];
		Vector3 lineOfSightVectorKm = ulsRxPos - rlanPosn;
		double distKm = lineOfSightVectorKm.len();
		double dAP = rlanPosn.len();
		double elevationAngleRad = M_PI / 2 - acos(rlanPosn.dot(lineOfSightVectorKm) / (dAP * distKm));

		std::string buildingPenetrationModelStr;
		double buildingPenetrationCDF;
		double buildingPenetrationDB = computeBuildingPenetration(_buildingType, elevationAngleRad, chanCenterFreq, buildingPenetrationModelStr, buildingPenetrationCDF, true);

		std::string txClutterStr;
		std::string pathLossModelStr;
		double pathLossCDF;
		std::string pathClutterModelStr;
		double pathClutterCDF;
		double pathLoss;
		double pathClutterDB;

		double rlanHtAboveTerrain = rlanCoord.heightKm * 1000.0 - rlanTerrainHeight;

		computePathLoss((rlanPropEnv == CConst::unknownPropEnv ? CConst::barrenPropEnv : rlanPropEnv), distKm, chanCenterFreq,
				rlanCoord.longitudeDeg, rlanCoord.latitudeDeg, rlanHtAboveTerrain,
				uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), uls->getRxHeightAboveTerrain(), elevationAngleRad,
				pathLoss, pathClutterDB, true,
				pathLossModelStr, pathLossCDF,
				pathClutterModelStr, pathClutterCDF,
				(iturp452::ITURP452 *)NULL, &txClutterStr, &(uls->ITMHeightProfile)
#if MM_DEBUG
				, uls->ITMHeightType
#endif
			       );

		double rxGainDB;
		double angleOffBoresightDeg = acos(uls->getAntennaPointing().dot(-(lineOfSightVectorKm.normalized()))) * 180.0 / M_PI;
		if (ulsRxAntennaType == CConst::F1245AntennaType) {
			rxGainDB = calcItu1245::CalcITU1245(angleOffBoresightDeg, uls->getRxGain());
		} else if (ulsRxAntennaType == CConst::F1336OmniAntennaType) {
			rxGainDB = calcItu1336_4::CalcITU1336_omni_avg(elevationAngleRad * 180.0 / M_PI, uls->getRxGain(), chanCenterFreq);
		} else if (ulsRxAntennaType == CConst::OmniAntennaType) {
			rxGainDB = 0.0;
		} else if (ulsRxAntennaType == CConst::LUTAntennaType) {
			rxGainDB = uls->getRxAntenna()->gainDB(angleOffBoresightDeg * M_PI / 180.0) + uls->getRxGain();
		} else {
			throw std::runtime_error(ErrStream() << "ERROR: INVALID ulsRxAntennaType: " << ulsRxAntennaType);
		}

		double rxPowerDBW = (_exclusionZoneRLANEIRPDBm - 30.0) - _bodyLossDB - buildingPenetrationDB - pathLoss - pathClutterDB + rxGainDB - spectralOverlapLossDB - _polarizationLossDB - uls->getRxAntennaFeederLossDB();

		double I2NDB = rxPowerDBW - uls->getNoiseLevelDBW();

		double marginDB = _IoverN_threshold_dB - I2NDB;

		if ((rlanPosnIdx == 0) || (marginDB < minMarginDB)) {
			minMarginDB = marginDB;
			distKmM     = distKm;
		}

		if ( fexcthrwifi ) {
			double d1;
			double d2;
			double pathDifference;
			double fresnelIndex = -1.0;
			const Vector3 ulsTxPos = uls->getTxPosition();
			double ulsLinkDistance = uls->getLinkDistance();
			double ulsWavelength = CConst::c / ((uls->getStartUseFreq() + uls->getStopUseFreq()) / 2);
			if (ulsLinkDistance != -1.0)
			{
				d1 = (ulsRxPos - rlanPosn).len() * 1000;
				d2 = (ulsTxPos - rlanPosn).len() * 1000;
				pathDifference = d1 + d2 - ulsLinkDistance;
				fresnelIndex = pathDifference / (ulsWavelength / 2);
			}
			else
			{
				d1 = (ulsRxPos - rlanPosn).len() * 1000;
				d2 = -1.0;
				pathDifference = -1.0;
			}

			std::string rxAntennaTypeStr;
			if (ulsRxAntennaType == CConst::F1245AntennaType)
			{
				rxAntennaTypeStr = "F.1245";
			}
			else if (ulsRxAntennaType == CConst::F1336OmniAntennaType)
			{
				rxAntennaTypeStr = "F.1336_OMNI";
			}
			else if (ulsRxAntennaType == CConst::OmniAntennaType)
			{
				rxAntennaTypeStr = "OMNI";
			}
			else if (ulsRxAntennaType == CConst::LUTAntennaType)
			{
				rxAntennaTypeStr = std::string(uls->getRxAntenna()->get_strid());
			}
			else
			{
				throw std::runtime_error(ErrStream() << "ERROR: INVALID ulsRxAntennaType = " << ulsRxAntennaType);
			}

			std::string bldgTypeStr = (_fixedBuildingLossFlag ? "INDOOR_FIXED" :
					_buildingType == CConst::noBuildingType ? "OUTDOOR" :
					_buildingType == CConst::traditionalBuildingType ?  "TRADITIONAL" :
					"THERMALLY_EFFICIENT");

			QStringList msg;
			msg << QString::number(uls->getID()) << QString::number(rlanPosnIdx) << QString::fromStdString(uls->getCallsign());
			msg << QString::number(uls->getRxLongitudeDeg(), 'f', 8) << QString::number(uls->getRxLatitudeDeg(), 'f', 8);
			msg << QString::number(uls->getRxHeightAboveTerrain(), 'f', 2) << QString::number(uls->getRxTerrainHeight(), 'f', 2) <<
			QString::fromStdString(_terrainDataModel->getSourceName(uls->getRxHeightSource()))
			<< QString(ulsRxPropEnv);
			msg << QString::number(rlanCoord.longitudeDeg, 'f', 8) << QString::number(rlanCoord.latitudeDeg, 'f', 8);
			msg << QString::number(rlanCoord.heightKm * 1000.0 - rlanTerrainHeight, 'f', 1) << QString::number(rlanTerrainHeight, 'f', 1) << 
			QString::fromStdString(_terrainDataModel->getSourceName(rlanHeightSource)) <<
			CConst::strPropEnvList->type_to_str(rlanPropEnv);
			msg << QString::number(distKm, 'f', 3) << QString::number(elevationAngleRad * 180.0 / M_PI, 'f', 3) << QString::number(angleOffBoresightDeg);
			msg << QString::number(_exclusionZoneRLANEIRPDBm, 'f', 3) << QString::number(_bodyLossDB, 'f', 3) << QString::fromStdString(txClutterStr) << QString::fromStdString(bldgTypeStr);
			msg << QString::number(buildingPenetrationDB, 'f', 3) << QString::fromStdString(buildingPenetrationModelStr) << QString::number(buildingPenetrationCDF, 'f', 8);
			msg << QString::number(pathLoss, 'f', 3) << QString::fromStdString(pathLossModelStr) << QString::number(pathLossCDF, 'f', 8);

			msg << QString::number(pathClutterDB, 'f', 3) << QString::fromStdString(pathClutterModelStr) << QString::number(pathClutterCDF, 'f', 8);

			msg << QString::number(bandwidth * 1.0e-6, 'f', 0) << QString::number(chanStartFreq * 1.0e-6, 'f', 0) << QString::number(chanStopFreq * 1.0e-6, 'f', 0)
				<< QString::number(uls->getStartUseFreq() * 1.0e-6, 'f', 2) << QString::number(uls->getStopUseFreq() * 1.0e-6, 'f', 2);

			msg << QString::fromStdString(rxAntennaTypeStr) << QString::number(uls->getRxGain(), 'f', 3);
			msg << QString::number(rxGainDB, 'f', 3) << QString::number(spectralOverlapLossDB, 'f', 3);
			msg << QString::number(_polarizationLossDB, 'f', 3);
			msg << QString::number(uls->getRxAntennaFeederLossDB(), 'f', 3);
			msg << QString::number(rxPowerDBW, 'f', 3) << QString::number(rxPowerDBW - uls->getNoiseLevelDBW(), 'f', 3);
			msg << QString::number(ulsLinkDistance, 'f', 3) << QString::number(chanCenterFreq, 'f', 3) << QString::number(d2, 'f', 3) << QString::number(pathDifference, 'f', 6);
			msg << QString::number(ulsWavelength * 1000, 'f', 3) << QString::number(fresnelIndex, 'f', 3);
                        msg << QString::fromStdString(comment);

			fexcthrwifi->writeRow(msg);
		}
	}

	if (uls->ITMHeightProfile) {
		free(uls->ITMHeightProfile);
		uls->ITMHeightProfile = (double *) NULL;
	}
	/**************************************************************************************/

	return(minMarginDB);
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::writeKML()                                                                 */
/******************************************************************************************/
void AfcManager::writeKML()
{

	ULSClass *uls = findULSID(_exclusionZoneFSID);
	double rlanHeightInput = std::get<2>(_rlanLLA);

	/**************************************************************************************/
	/* Create kmlFile, useful for debugging                                            */
	/**************************************************************************************/
	ZXmlWriter kml_writer(_kmlFile);
	auto &fkml = kml_writer.xml_writer;
	/**************************************************************************************/
	fkml->writeStartDocument();
	fkml->writeStartElement("kml");
	fkml->writeAttribute("xmlns", "http://www.opengis.net/kml/2.2");
	fkml->writeStartElement("Document");
	fkml->writeTextElement("name", "FB RLAN AFC");
	fkml->writeTextElement("open", "1");
	fkml->writeTextElement("description", "Display Exclusion Zone Analysis Results");
	fkml->writeStartElement("Style");
	fkml->writeAttribute("id", "transBluePoly");
	fkml->writeStartElement("LineStyle");
	fkml->writeTextElement("width", "1.5");
	fkml->writeEndElement(); // LineStyle
	fkml->writeStartElement("PolyStyle");
	fkml->writeTextElement("color", "7dff0000");
	fkml->writeEndElement(); // PolyStyle
	fkml->writeEndElement(); // Style

	fkml->writeStartElement("Placemark");
	fkml->writeTextElement("name", QString("FSID : %1").arg(uls->getID()));
	fkml->writeTextElement("visibility", "0");
	fkml->writeStartElement("Point");
	fkml->writeTextElement("extrude", "1");
	fkml->writeTextElement("altitudeMode", "absolute");
	fkml->writeTextElement("coordinates", QString::asprintf("%12.10f,%12.10f,%5.3f", uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), _exclusionZoneFSTerrainHeight + _exclusionZoneHeightAboveTerrain));
	fkml->writeEndElement(); // Point;
	fkml->writeEndElement(); // Placemark

	fkml->writeStartElement("Placemark");
	fkml->writeTextElement("name", "Exclusion Zone");
	fkml->writeTextElement("visibility", "1");
	fkml->writeTextElement("styleUrl", "#transBluePoly");
	fkml->writeStartElement("Polygon");
	fkml->writeTextElement("altitudeMode", "clampToGround");
	fkml->writeStartElement("outerBoundaryIs");
	fkml->writeStartElement("LinearRing");

	QString excls_coords;
	int exclPtIdx;
	for(exclPtIdx=0; exclPtIdx < (int) _exclusionZone.size(); ++exclPtIdx) {
		double rlanLon = std::get<0>(_exclusionZone[exclPtIdx]);
		double rlanLat = std::get<1>(_exclusionZone[exclPtIdx]);
		double rlanHeight;
		double rlanTerrainHeight, bldgHeight;
		MultibandRasterClass::HeightResult lidarHeightResult;
		CConst::HeightSourceEnum heightSource;
		_terrainDataModel->getTerrainHeight(rlanLon,rlanLat, rlanTerrainHeight,bldgHeight, lidarHeightResult, heightSource);
		if (_rlanHeightType == "AMSL") {
			rlanHeight = rlanHeightInput;
		} else if (_rlanHeightType == "AGL") {
			rlanHeight = rlanHeightInput + rlanTerrainHeight;
		} else {
			throw std::runtime_error(ErrStream() << "ERROR: INVALID _rlanHeightType = " << _rlanHeightType);
		}
		excls_coords.append(QString::asprintf("%.10f,%.10f,%.5f\n", rlanLon, rlanLat, rlanHeight));
	}
	fkml->writeTextElement("coordinates", excls_coords);
	fkml->writeEndElement(); // LinearRing
	fkml->writeEndElement(); // outerBoundaryIs
	fkml->writeEndElement(); // Polygon
	fkml->writeEndElement(); // Placemark

	fkml->writeEndElement(); // Document
	fkml->writeEndElement(); // kml
	fkml->writeEndDocument();

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::runHeatmapAnalysis()                                                       */
/******************************************************************************************/
void AfcManager::runHeatmapAnalysis()
{
	std::ostringstream errStr;
	int numPtsLon;
	int numPtsLat;

	LOGGER_INFO(logger) << "Executing AfcManager::runHeatmapAnalysis()";

        ChannelStruct channel = _channelList[_heatmapRLANChanIdx];

        double chanStartFreq = channel.startFreqMHz*1.0e6;
        double chanStopFreq = channel.stopFreqMHz*1.0e6;
        double chanCenterFreq = (chanStartFreq + chanStopFreq)/2;

	_heatmapNumPtsLat = ceil( (_heatmapMaxLat - _heatmapMinLat)*M_PI/180.0*CConst::earthRadius / _heatmapRLANSpacing);

	double minAbsLat;
	if ((_heatmapMinLat < 0.0) && (_heatmapMaxLat > 0.0)) {
		minAbsLat = 0.0;
	} else {
		minAbsLat = std::min(fabs(_heatmapMinLat), fabs(_heatmapMaxLat));
	}

	_heatmapNumPtsLon = ceil( (_heatmapMaxLon - _heatmapMinLon)*M_PI/180.0*CConst::earthRadius*cos(minAbsLat * M_PI / 180.0) / _heatmapRLANSpacing);

	int totNumProc = _heatmapNumPtsLon*_heatmapNumPtsLat;
	LOGGER_INFO(logger) << "NUM_PTS_LON: " << _heatmapNumPtsLon;
	LOGGER_INFO(logger) << "NUM_PTS_LAT: " << _heatmapNumPtsLat;
	LOGGER_INFO(logger) << "TOT_PTS: " << totNumProc;

	_heatmapIToNThresholdDB = _IoverN_threshold_dB;

	/**************************************************************************************/
	/* Allocate / Initialize heatmap                                                      */
	/**************************************************************************************/
	_heatmapIToNDB = (double **)malloc(_heatmapNumPtsLon * sizeof(double *));
	_heatmapIsIndoor = (bool **)malloc(_heatmapNumPtsLon * sizeof(bool *));

	int lonIdx, latIdx;
	for (lonIdx = 0; lonIdx < _heatmapNumPtsLon; ++lonIdx) {
		_heatmapIToNDB[lonIdx] = (double *) malloc(_heatmapNumPtsLat * sizeof(double));
		_heatmapIsIndoor[lonIdx] = (bool *) malloc(_heatmapNumPtsLat * sizeof(bool));
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Create excThrFile, useful for debugging                                            */
	/**************************************************************************************/
	auto exc_thr_writer = GzipCsvWriter(_excThrFile);
	auto &fexcthrwifi = exc_thr_writer.csv_writer;

	if (fexcthrwifi)
	{
		fexcthrwifi->writeRow({ "FS_ID",
				"RLAN_POSN_IDX",
				"CALLSIGN",
				"FS_RX_LONGITUDE (deg)",
				"FS_RX_LATITUDE (deg)",
				"FS_RX_HEIGHT_ABOVE_TERRAIN (m)",
				"FS_RX_TERRAIN_HEIGHT (m)",
				"FS_RX_TERRAIN_SOURCE",
				"FS_RX_PROP_ENV",
				"RLAN_LONGITUDE (deg)",
				"RLAN_LATITUDE (deg)",
				"RLAN_HEIGHT_ABOVE_TERRAIN (m)",
				"RLAN_TERRAIN_HEIGHT (m)",
				"RLAN_TERRAIN_SOURCE",
				"RLAN_PROP_ENV",
				"RLAN_FS_RX_DIST (km)",
				"RLAN_FS_RX_ELEVATION_ANGLE (deg)",
				"FS_RX_ANGLE_OFF_BORESIGHT (deg)",
				"RLAN_TX_EIRP (dBm)",
				"BODY_LOSS (dB)",
				"RLAN_CLUTTER_CATEGORY",
				"BUILDING TYPE",
				"RLAN_FS_RX_BUILDING_PENETRATION (dB)",
				"BUILDING_PENETRATION_MODEL",
				"BUILDING_PENETRATION_CDF",
				"PATH_LOSS (dB)",
				"PATH_LOSS_MODEL",
				"PATH_LOSS_CDF",
				"PATH_CLUTTER (DB)",
				"PATH_CLUTTER_MODEL",
				"PATH_CLUTTER_CDF",
				"RLAN BANDWIDTH (MHz)",
				"RLAN CHANNEL START FREQ (MHz)",
				"RLAN CHANNEL STOP FREQ (MHz)",
				"ULS START FREQ (MHz)",
				"ULS STOP FREQ (MHz)",
				"FS_ANT_TYPE",
				"FS_ANT_GAIN_PEAK (dB)",
				"FS_ANT_GAIN_TO_RLAN (dB)",
				"RX_SPECTRAL_OVERLAP_LOSS (dB)",
				"POLARIZATION_LOSS (dB)",
				"FS_RX_FEEDER_LOSS (dB)",
				"FS_RX_PWR (dBW)",
				"FS I/N (dB)",
				"ULS_LINK_DIST (m)",
				"RLAN_CENTER_FREQ (Hz)",
				"FS_TX_TO_RLAN_DIST (m)",
				"PATH_DIFFERENCE (m)",
				"ULS_WAVELENGTH (mm)",
				"FRESNEL_INDEX"
		});

	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Compute Heatmap                                                                    */
	/**************************************************************************************/
	_zbldg2109 = -qerfi(_confidenceBldg2109);
	_zclutter2108 = -qerfi(_confidenceClutter2108);
	_zwinner2 = -qerfi(_confidenceWinner2);

	const double exclusionDistKmSquared = (_exclusionDist / 1000.0) * (_exclusionDist / 1000.0);
	const double maxRadiusKmSquared = (_maxRadius / 1000.0) * (_maxRadius / 1000.0);

	Vector3 rlanPosnList[3];
	GeodeticCoord rlanCoordList[3];

	char *tstr;
#   if MM_DEBUG
	time_t tStartHeatmap = time(NULL);
	tstr = strdup(ctime(&tStartHeatmap));
	strtok(tstr, "\n");

	LOGGER_INFO(logger) << "Begin Heatmap Scan: " << tstr;

	free(tstr);

#   endif

	int numPct = 100;

	if (numPct > totNumProc) { numPct = totNumProc; }

	int xN = 1;

	int pctIdx;
	std::chrono::high_resolution_clock::time_point tstart;

	bool initFlag = false;
	int numInvalid = 0;
	int numProc = 0;
	for (lonIdx = 0; lonIdx < _heatmapNumPtsLon; ++lonIdx) {
		double rlanLon = (_heatmapMinLon*(2*_heatmapNumPtsLon-2*lonIdx-1) + _heatmapMaxLon*(2*lonIdx+1)) / (2*_heatmapNumPtsLon);
		for (latIdx = 0; latIdx < _heatmapNumPtsLat; ++latIdx) {
			double rlanLat = (_heatmapMinLat*(2*_heatmapNumPtsLat-2*latIdx-1) + _heatmapMaxLat*(2*latIdx+1)) / (2*_heatmapNumPtsLat);
			// LOGGER_DEBUG(logger) << "Heatmap point: (" << lonIdx << ", " << latIdx << ")";

#           if MM_DEBUG
			auto t1 = std::chrono::high_resolution_clock::now();
#           endif

			double rlanHeight;
			double rlanTerrainHeight, bldgHeight;
			MultibandRasterClass::HeightResult lidarHeightResult;
			CConst::HeightSourceEnum rlanHeightSource;
			_terrainDataModel->getTerrainHeight(rlanLon,rlanLat, rlanTerrainHeight,bldgHeight, lidarHeightResult, rlanHeightSource);

			if (_heatmapIndoorOutdoorStr == "Outdoor") {
				_buildingType = CConst::noBuildingType;
			} else if (_heatmapIndoorOutdoorStr == "Indoor") {
				_buildingType = CConst::traditionalBuildingType;
			} else if (_heatmapIndoorOutdoorStr == "Database") {
				if (lidarHeightResult != MultibandRasterClass::HeightResult::NO_BUILDING) {
					_buildingType = CConst::noBuildingType;
				} else {
					_buildingType = CConst::traditionalBuildingType;
				}
			}
			double rlanEIRP, rlanHeightInput, heightUncertainty;
			QString rlanHeightType;
			if (_buildingType == CConst::noBuildingType) {
				rlanEIRP = _heatmapRLANOutdoorEIRPDBm;
				rlanHeightInput = _heatmapRLANOutdoorHeight;
				heightUncertainty = _heatmapRLANOutdoorHeightUncertainty;
				rlanHeightType = _heatmapRLANOutdoorHeightType;
				_bodyLossDB = _bodyLossOutdoorDB;
			} else {
				rlanEIRP = _heatmapRLANIndoorEIRPDBm;
				rlanHeightInput = _heatmapRLANIndoorHeight;
				heightUncertainty = _heatmapRLANIndoorHeightUncertainty;
				rlanHeightType = _heatmapRLANIndoorHeightType;
				_bodyLossDB = _bodyLossIndoorDB;
			}

			if (rlanHeightType == "AMSL") {
				rlanHeight = rlanHeightInput;
			} else if (rlanHeightType == "AGL") {
				rlanHeight = rlanHeightInput + rlanTerrainHeight;
			} else {
				throw std::runtime_error(ErrStream() << "ERROR: INVALID_VALUE rlanHeightType = " << rlanHeightType);
			}

			if (rlanHeight - heightUncertainty - rlanTerrainHeight < _minRlanHeightAboveTerrain) {
				throw std::runtime_error(ErrStream()
						<< std::string("ERROR: Heat Map: Invalid RLAN parameter settings.") << std::endl
						<< std::string("RLAN Height = ") << rlanHeight << std::endl
						<< std::string("Height Uncertainty = ") << heightUncertainty << std::endl
						<< std::string("Terrain Height at RLAN Location = ") << rlanTerrainHeight << std::endl
						<< std::string("RLAN is ") << rlanHeight - heightUncertainty - rlanTerrainHeight << " meters above terrain" << std::endl
						<< std::string("RLAN must be more than ") << _minRlanHeightAboveTerrain << " meters above terrain" << std::endl
						);
			}

			CConst::PropEnvEnum rlanPropEnv = computeRlanPropEnv(rlanLon, rlanLat);

			Vector3 rlanCenterPosn = EcefModel::geodeticToEcef(rlanLat, rlanLon, rlanHeight / 1000.0);
			Vector3 upVec = rlanCenterPosn.normalized();

			rlanPosnList[0] = EcefModel::geodeticToEcef(rlanLat, rlanLon, rlanHeight / 1000.0);
			rlanPosnList[1] = rlanCenterPosn + (heightUncertainty / 1000.0) * upVec;
			rlanPosnList[2] = rlanCenterPosn - (heightUncertainty / 1000.0) * upVec;

			rlanCoordList[0] = EcefModel::toGeodetic(rlanPosnList[0]);
			rlanCoordList[1] = EcefModel::toGeodetic(rlanPosnList[1]);
			rlanCoordList[2] = EcefModel::toGeodetic(rlanPosnList[2]);

			int numRlanPosn = ((heightUncertainty == 0.0) ? 1 : 3);

			double maxIToNDB = -999.0;
			int ulsIdx;
			for (ulsIdx = 0; ulsIdx < _ulsList->getSize(); ulsIdx++) {
				ULSClass *uls = (*_ulsList)[ulsIdx];
				const Vector3 ulsRxPos = uls->getRxPosition();
				Vector3 lineOfSightVectorKm = ulsRxPos - rlanCenterPosn;
				double distKmSquared = (lineOfSightVectorKm).dot(lineOfSightVectorKm);

#if 0
				// For debugging, identifies anomalous ULS entries
				if (uls->getLinkDistance() == -1) {
					std::cout << uls->getID() << std::endl;
				}
#endif

				if ((distKmSquared < maxRadiusKmSquared) && (distKmSquared > exclusionDistKmSquared) && (uls->getLinkDistance() > 0.0)) {
					if (_useFixedRxAntennaFeederLoss) {
						uls->setRxAntennaFeederLossDB(_receiverFeedLoss_dB);
					}
					_ulsIdxList.push_back(ulsIdx); // Store the ULS indices that are used in analysis
					int ulsLonIdx;
					int ulsLatIdx;
					int regionIdx;
					char ulsRxPropEnv;
					_popGrid->findDeg(uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), ulsLonIdx, ulsLatIdx, ulsRxPropEnv, regionIdx);
					double ulsPopVal = _popGrid->getPop(ulsLonIdx, ulsLatIdx);
					if (ulsPopVal == 0.0) {
						ulsRxPropEnv = 'Z';
					}

					CConst::ULSAntennaTypeEnum ulsRxAntennaType = uls->getRxAntennaType();

					int rlanPosnIdx;

					double spectralOverlap = computeSpectralOverlap(chanStartFreq, chanStopFreq, uls->getStartUseFreq(), uls->getStopUseFreq(), _aciFlag);

					if (spectralOverlap > 0.0) {
						for (rlanPosnIdx = 0; rlanPosnIdx < numRlanPosn; ++rlanPosnIdx) {
							Vector3 rlanPosn = rlanPosnList[rlanPosnIdx];
							GeodeticCoord rlanCoord = rlanCoordList[rlanPosnIdx];
							lineOfSightVectorKm = ulsRxPos - rlanPosn;
							double distKm = lineOfSightVectorKm.len();
							double dAP = rlanPosn.len();
							double elevationAngleRad = M_PI / 2 - acos(rlanPosn.dot(lineOfSightVectorKm) / (dAP * distKm));

							double spectralOverlapLossDB = -10.0 * log(spectralOverlap) / log(10.0);

							std::string buildingPenetrationModelStr;
							double buildingPenetrationCDF;
							double buildingPenetrationDB = computeBuildingPenetration(_buildingType, elevationAngleRad, chanCenterFreq, buildingPenetrationModelStr, buildingPenetrationCDF, true);

							std::string txClutterStr;
							std::string pathLossModelStr;
							double pathLossCDF;
							std::string pathClutterModelStr;
							double pathClutterCDF;
							double pathLoss;
							double pathClutterDB;

							double rlanHtAboveTerrain = rlanCoord.heightKm * 1000.0 - rlanTerrainHeight;

							computePathLoss(rlanPropEnv, distKm, chanCenterFreq,
									rlanCoord.longitudeDeg, rlanCoord.latitudeDeg, rlanHtAboveTerrain,
									uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), uls->getRxHeightAboveTerrain(), elevationAngleRad,
									pathLoss, pathClutterDB, true,
									pathLossModelStr, pathLossCDF,
									pathClutterModelStr, pathClutterCDF,
									(iturp452::ITURP452 *)NULL, &txClutterStr, &(uls->ITMHeightProfile)
#if MM_DEBUG
									, uls->ITMHeightType
#endif
								       );

							double rxGainDB;
							double angleOffBoresightDeg = acos(uls->getAntennaPointing().dot(-(lineOfSightVectorKm.normalized()))) * 180.0 / M_PI;
							if (ulsRxAntennaType == CConst::F1245AntennaType)
							{
								rxGainDB = calcItu1245::CalcITU1245(angleOffBoresightDeg, uls->getRxGain());
							}
							else if (ulsRxAntennaType == CConst::F1336OmniAntennaType)
							{
								rxGainDB = calcItu1336_4::CalcITU1336_omni_avg(elevationAngleRad * 180.0 / M_PI, uls->getRxGain(), chanCenterFreq);
							}
							else if (ulsRxAntennaType == CConst::OmniAntennaType)
							{
								rxGainDB = 0.0;
							}
							else if (ulsRxAntennaType == CConst::LUTAntennaType)
							{
								rxGainDB = uls->getRxAntenna()->gainDB(angleOffBoresightDeg * M_PI / 180.0) + uls->getRxGain();
							}
							else
							{
								throw std::runtime_error(ErrStream() << "ERROR: INVALID ulsRxAntennaType = " << ulsRxAntennaType);
							}

							double rxPowerDBW = (rlanEIRP - 30.0) - _bodyLossDB - buildingPenetrationDB - pathLoss - pathClutterDB + rxGainDB - spectralOverlapLossDB - _polarizationLossDB - uls->getRxAntennaFeederLossDB();

							double I2NDB = rxPowerDBW - uls->getNoiseLevelDBW();

							if (I2NDB > maxIToNDB) {
								maxIToNDB = I2NDB;
								_heatmapIsIndoor[lonIdx][latIdx] = (_buildingType == CConst::noBuildingType ? false : true);
							}

							if ( fexcthrwifi && (std::isnan(rxPowerDBW) || (I2NDB > _visibilityThreshold) || (distKm * 1000 < _closeInDist)) )
							{
								double d1;
								double d2;
								double pathDifference;
								double fresnelIndex = -1.0;
								const Vector3 ulsTxPos = uls->getTxPosition();
								double ulsLinkDistance = uls->getLinkDistance();
								double ulsWavelength = CConst::c / ((uls->getStartUseFreq() + uls->getStopUseFreq()) / 2);
								if (ulsLinkDistance != -1.0)
								{
									d1 = (ulsRxPos - rlanPosn).len() * 1000;
									d2 = (ulsTxPos - rlanPosn).len() * 1000;
									pathDifference = d1 + d2 - ulsLinkDistance;
									fresnelIndex = pathDifference / (ulsWavelength / 2);
								}
								else
								{
									d1 = (ulsRxPos - rlanPosn).len() * 1000;
									d2 = -1.0;
									pathDifference = -1.0;
								}

								std::string rxAntennaTypeStr;
								if (ulsRxAntennaType == CConst::F1245AntennaType)
								{
									rxAntennaTypeStr = "F.1245";
								}
								else if (ulsRxAntennaType == CConst::F1336OmniAntennaType)
								{
									rxAntennaTypeStr = "F.1336_OMNI";
								}
								else if (ulsRxAntennaType == CConst::OmniAntennaType)
								{
									rxAntennaTypeStr = "OMNI";
								}
								else if (ulsRxAntennaType == CConst::LUTAntennaType)
								{
									rxAntennaTypeStr = std::string(uls->getRxAntenna()->get_strid());
								}
								else
								{
									throw std::runtime_error(ErrStream() << "ERROR: INVALID ulsRxAntennaType = " << ulsRxAntennaType);
								}

								std::string bldgTypeStr = (_fixedBuildingLossFlag ? "INDOOR_FIXED" :
										_buildingType == CConst::noBuildingType ? "OUTDOOR" :
										_buildingType == CConst::traditionalBuildingType ?  "TRADITIONAL" :
										"THERMALLY_EFFICIENT");

								QStringList msg;
								msg << QString::number(uls->getID()) << QString::number(rlanPosnIdx) << QString::fromStdString(uls->getCallsign());
								msg << QString::number(uls->getRxLongitudeDeg(), 'f', 5) << QString::number(uls->getRxLatitudeDeg(), 'f', 5);
								msg << QString::number(uls->getRxHeightAboveTerrain(), 'f', 2) << QString::number(uls->getRxTerrainHeight(), 'f', 2) <<
								QString::fromStdString(_terrainDataModel->getSourceName(uls->getRxHeightSource()))
								<< QString(ulsRxPropEnv);
								msg << QString::number(rlanCoord.longitudeDeg, 'f', 5) << QString::number(rlanCoord.latitudeDeg, 'f', 5);
								msg << QString::number(rlanCoord.heightKm * 1000.0 - rlanTerrainHeight, 'f', 1) << QString::number(rlanTerrainHeight, 'f', 1) << 
								QString::fromStdString(_terrainDataModel->getSourceName(rlanHeightSource))
								<<
								CConst::strPropEnvList->type_to_str(rlanPropEnv);
								msg << QString::number(distKm, 'f', 3) << QString::number(elevationAngleRad * 180.0 / M_PI, 'f', 3) << QString::number(angleOffBoresightDeg);
								msg << QString::number(rlanEIRP, 'f', 3) << QString::number(_bodyLossDB, 'f', 3) << QString::fromStdString(txClutterStr) << QString::fromStdString(bldgTypeStr);
								msg << QString::number(buildingPenetrationDB, 'f', 3) << QString::fromStdString(buildingPenetrationModelStr) << QString::number(buildingPenetrationCDF, 'f', 8);
								msg << QString::number(pathLoss, 'f', 3) << QString::fromStdString(pathLossModelStr) << QString::number(pathLossCDF, 'f', 8);

								msg << QString::number(pathClutterDB, 'f', 3) << QString::fromStdString(pathClutterModelStr) << QString::number(pathClutterCDF, 'f', 8);

								msg << QString::number(_heatmapRLANBWHz * 1.0e-6, 'f', 0) << QString::number(chanStartFreq * 1.0e-6, 'f', 0) << QString::number(chanStopFreq * 1.0e-6, 'f', 0)
									<< QString::number(uls->getStartUseFreq() * 1.0e-6, 'f', 2) << QString::number(uls->getStopUseFreq() * 1.0e-6, 'f', 2);

								msg << QString::fromStdString(rxAntennaTypeStr) << QString::number(uls->getRxGain(), 'f', 3);
								msg << QString::number(rxGainDB, 'f', 3) << QString::number(spectralOverlapLossDB, 'f', 3);
								msg << QString::number(_polarizationLossDB, 'f', 3);
								msg << QString::number(uls->getRxAntennaFeederLossDB(), 'f', 3);
								msg << QString::number(rxPowerDBW, 'f', 3) << QString::number(rxPowerDBW - uls->getNoiseLevelDBW(), 'f', 3);
								msg << QString::number(ulsLinkDistance, 'f', 3) << QString::number(chanCenterFreq, 'f', 3) << QString::number(d2, 'f', 3) << QString::number(pathDifference, 'f', 6);
								msg << QString::number(ulsWavelength * 1000, 'f', 3) << QString::number(fresnelIndex, 'f', 3);

								fexcthrwifi->writeRow(msg);
							}
						}

						if (uls->ITMHeightProfile) {
							free(uls->ITMHeightProfile);
							uls->ITMHeightProfile = (double *) NULL;
						}
					}
				} else {
					// uls is not included in calculations
					//LOGGER_ERROR(logger) << "ID: " << uls->getID() << ", distKm: " << distKmSquared << ", link: " << uls->getLinkDistance() << ",";
				}
			}
			_heatmapIToNDB[lonIdx][latIdx] = maxIToNDB;

			if (maxIToNDB == -999.0) {
				numInvalid++;
				errStr << "At position LON = " << rlanLon << " LAT = " << rlanLat  << " there are no FS receivers within 150 Km of RLAN that have spectral overlap with RLAN";
				LOGGER_INFO(logger) << errStr.str();
			}

			if (!initFlag) {
				_heatmapMinIToNDB = maxIToNDB;
				_heatmapMaxIToNDB = maxIToNDB;
				initFlag = true;
			} else if (maxIToNDB < _heatmapMinIToNDB) {
				_heatmapMinIToNDB = maxIToNDB;
			} else if (maxIToNDB > _heatmapMaxIToNDB) {
				_heatmapMaxIToNDB = maxIToNDB;
			}

			numProc++;

#           if MM_DEBUG
			auto t2 = std::chrono::high_resolution_clock::now();

			std::cout << " [" << numProc << " / " <<  totNumProc  << "] " << " Elapsed Time = " << std::setprecision(6) << std::chrono::duration_cast<std::chrono::duration<double>>(t2-t1).count() << std::endl << std::flush;

#           endif

			if (numProc == xN) {
				if (xN == 1) {
					tstart = std::chrono::high_resolution_clock::now();
					pctIdx = 1;
				} else {
					auto tcurrent = std::chrono::high_resolution_clock::now();
					double elapsedTime = std::chrono::duration_cast<std::chrono::duration<double>>(tcurrent-tstart).count();
					double remainingTime = elapsedTime*(totNumProc-numProc)/(numProc-1);

					std::ofstream progressFile;
					progressFile.open(_progressFile);
					progressFile << (int) std::floor(100.0*numProc/totNumProc) << std::endl
						<< "Elapsed Time: " << (int) std::floor(elapsedTime)
						<< " s, Remaining: " << (int) std::floor(remainingTime) << " s";
					progressFile.close();

					// printf("%f %%  :  Elapsed Time = %f seconds, Remaining %f seconds\n", 100.0*numProc/totNumProc, elapsedTime, remainingTime);
					pctIdx++;
				}
				xN = ((totNumProc-1)*pctIdx + numPct-1)/numPct + 1;
			}
		}
	}

	if (numInvalid) {
		errStr << "There were a total of " << numInvalid << " RLAN locations for which there are no FS receivers within 150 Km that have nonzero spectral overlap";
		LOGGER_WARN(logger) << errStr.str();
		statusMessageList.push_back(errStr.str());
	}

#   if MM_DEBUG
	time_t tEndHeatmap = time(NULL);
	tstr = strdup(ctime(&tEndHeatmap));
	strtok(tstr, "\n");

	int elapsedTime = (int) (tEndHeatmap-tStartHeatmap);

	int et = elapsedTime;
	int elapsedTimeSec = et % 60;
	et = et / 60;
	int elapsedTimeMin = et % 60;
	et = et / 60;
	int elapsedTimeHour = et % 24;
	et = et / 24;
	int elapsedTimeDay = et;

	LOGGER_INFO(logger) << "End Processing Heatmap " << tstr
		<< " Elapsed time = " << (tEndHeatmap-tStartHeatmap) << " sec = "
		<< elapsedTimeDay  << " days "
		<< elapsedTimeHour << " hours "
		<< elapsedTimeMin  << " min "
		<< elapsedTimeSec  << " sec.";

	free(tstr);
#   endif
	/**************************************************************************************/

	_terrainDataModel->printStats();
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::printUserInputs                                                            */
/******************************************************************************************/
void AfcManager::printUserInputs()
{
	QStringList msg;

	LOGGER_INFO(logger) << "printing user inputs " << _userInputsFile;
	auto inputs_writer = GzipCsvWriter(_userInputsFile);
	auto &fUserInputs = inputs_writer.csv_writer;

	double lat, lon, alt, minor, major, height_uncert;
	std::tie(lat, lon, alt) = _rlanLLA;
	std::tie(minor, major, height_uncert) = _rlanUncerts_m;

	if (fUserInputs) {
		fUserInputs->writeRow({ "ANALYSIS_TYPE", QString::fromStdString(_analysisType) });
		fUserInputs->writeRow({ "SERIAL_NUMBER", QString(_serialNumber) } );
		fUserInputs->writeRow({ "LATITUDE (DEG)", QString::number(lat, 'e', 20) } );
		fUserInputs->writeRow({ "LONGITUDE (DEG)", QString::number(lon, 'e', 20) } );
		fUserInputs->writeRow({ "ANTENNA_HEIGHT (M)", QString::number(alt, 'e', 20) } );
		fUserInputs->writeRow({ "SEMI-MAJOR_AXIS (M)", QString::number(major, 'e', 20) } );
		fUserInputs->writeRow({ "SEMI-MINOR_AXIS (M)", QString::number(minor, 'e', 20) } );
		fUserInputs->writeRow({ "HEIGHT_UNCERTAINTY (M)", QString::number(height_uncert, 'e', 20) } );
		fUserInputs->writeRow({ "ORIENTATION (DEG)", QString::number(_rlanOrientation_deg, 'e', 20) } );
		fUserInputs->writeRow({ "HEIGHT_TYPE", QString(_rlanHeightType) } );
		fUserInputs->writeRow({ "INDOOR/OUTDOOR", (_rlanType == RLAN_INDOOR ? "indoor" : _rlanType == RLAN_OUTDOOR ? "outdoo" : "error") } );

		fUserInputs->writeRow({ "ULS_DATABASE", QString(_inputULSDatabaseStr) } );
		fUserInputs->writeRow({ "AP/CLIENT_PROPAGATION_ENVIRO", QString(_propagationEnviro) } );
		fUserInputs->writeRow({ "AP/CLIENT_MIN_EIRP (DBM)", QString::number(_minEIRP_dBm, 'e', 20) } );
		fUserInputs->writeRow({ "AP/CLIENT_MAX_EIRP (DBM)", QString::number(_maxEIRP_dBm, 'e', 20) } );

		fUserInputs->writeRow({ "BUILDING_PENETRATION_LOSS_MODEL", QString(_buildingLossModel) } );
		fUserInputs->writeRow({ "BUILDING_TYPE", QString((_buildingType == CConst::traditionalBuildingType ? "traditional" : _buildingType == CConst::thermallyEfficientBuildingType ? "thermally efficient" : "no building type")) } );
		fUserInputs->writeRow({ "BUILDING_PENETRATION_CONFIDENCE", QString::number(_confidenceBldg2109, 'e', 20) } );
		fUserInputs->writeRow({ "BUILDING_PENETRATION_LOSS_FIXED_VALUE (DB)", QString::number(_fixedBuildingLossValue, 'e', 20) } );
		fUserInputs->writeRow({ "FS_RECEIVER_FEEDER_LOSS (DB)", QString::number(_polarizationLossDB, 'e', 20) } );
		fUserInputs->writeRow({ "RLAN_BODY_LOSS_INDOOR (DB)", QString::number(_bodyLossIndoorDB, 'e', 20) } );
		fUserInputs->writeRow({ "RLAN_BODY_LOSS_OUTDOOR (DB)", QString::number(_bodyLossOutdoorDB, 'e', 20) } );
		fUserInputs->writeRow({ "I/N_THRESHOLD", QString::number(_IoverN_threshold_dB, 'e', 20) } );
		fUserInputs->writeRow({ "FS_RECEIVER_ANTENNA_PATTERN", QString(_antennaPattern) } );


		if (_pathLossModelStr == "COALITION_OPT_6") {
			fUserInputs->writeRow({ "PROPAGATION_MODEL", QString("ITM_WITHNO_BUILDING_DATA (SRTM)") } );
			fUserInputs->writeRow({ "WINNER_II_PROB_LOS_THRESHOLD", QString::number(_winner2ProbLOSThr, 'e', 20) } );
			fUserInputs->writeRow({ "WINNER_II_CONFIDENCE", QString::number(_confidenceWinner2, 'e', 20) } );
			fUserInputs->writeRow({ "ITM_CONFIDENCE", QString::number(_confidenceITM, 'e', 20) } );
			fUserInputs->writeRow({ "P.2108_CONFIDENCE", QString::number(_confidenceClutter2108, 'e', 20) } );
		} else if (_pathLossModelStr == "ITM_BLDG") {
			fUserInputs->writeRow({ "PROPAGATION_MODEL", QString::fromStdString(_pathLossModelStr) } );
			fUserInputs->writeRow({ "WINNER_II_PROB_LOS_THRESHOLD", "N/A" } );
			fUserInputs->writeRow({ "WINNER_II_CONFIDENCE", "N/A" } );
			fUserInputs->writeRow({ "ITM_CONFIDENCE", QString::number(_confidenceITM, 'e', 20) } );
			fUserInputs->writeRow({ "P.2108_CONFIDENCE", "N/A" } );
		} else {
			fUserInputs->writeRow({ "PROPAGATION_MODEL", QString::fromStdString(_pathLossModelStr) } );
			fUserInputs->writeRow({ "WINNER_II_PROB_LOS_THRESHOLD", "N/A" } );
			fUserInputs->writeRow({ "WINNER_II_CONFIDENCE", "N/A" } );
			fUserInputs->writeRow({ "ITM_CONFIDENCE", "N/A" } );
			fUserInputs->writeRow({ "P.2108_CONFIDENCE", "N/A" } );
		}

		if (_analysisType == "ExclusionZoneAnalysis") {
			double chanCenterFreq = _wlanMinFreq + (_exclusionZoneRLANChanIdx + 0.5) * _exclusionZoneRLANBWHz;

			fUserInputs->writeRow({ "EXCLUSION_ZONE_FSID", QString::number(_exclusionZoneFSID) });
			fUserInputs->writeRow({ "EXCLUSION_ZONE_RLAN_BW (Hz)", QString::number(_exclusionZoneRLANBWHz, 'e', 20) } );
			fUserInputs->writeRow({ "EXCLUSION_ZONE_RLAN_CENTER_FREQ (Hz)", QString::number(chanCenterFreq, 'e', 20) } );
			fUserInputs->writeRow({ "EXCLUSION_ZONE_RLAN_EIRP (dBm)", QString::number(_exclusionZoneRLANEIRPDBm, 'e', 20) } );

		} else if (_analysisType == "HeatmapAnalysis") {
			double chanCenterFreq = _wlanMinFreq + (_heatmapRLANChanIdx + 0.5) * _heatmapRLANBWHz;

			fUserInputs->writeRow({ "HEATMAP_RLAN_BW (Hz)", QString::number(_heatmapRLANBWHz, 'e', 20) } );
			fUserInputs->writeRow({ "HEATMAP_RLAN_CENTER_FREQ (Hz)", QString::number(chanCenterFreq, 'e', 20) } );
			fUserInputs->writeRow({ "HEATMAP_MIN_LON (DEG)", QString::number(_heatmapMinLon, 'e', 20) } );
			fUserInputs->writeRow({ "HEATMAP_MIN_LAT (DEG)", QString::number(_heatmapMaxLon, 'e', 20) } );
			fUserInputs->writeRow({ "HEATMAP_RLAN_SPACING (m)", QString::number(_heatmapRLANSpacing, 'e', 20) } );
			fUserInputs->writeRow({ "HEATMAP_INDOOR_OUTDOOR_STR", QString::fromStdString(_heatmapIndoorOutdoorStr) } );


			fUserInputs->writeRow({ "HEATMAP_RLAN_INDOOR_EIRP (dBm)", QString::number(_heatmapRLANIndoorEIRPDBm, 'e', 20) } );
			fUserInputs->writeRow({ "HEATMAP_RLAN_INDOOR_HEIGHT_TYPE", QString(_heatmapRLANIndoorHeightType) } );
			fUserInputs->writeRow({ "HEATMAP_RLAN_INDOOR_HEIGHT (m)", QString::number(_heatmapRLANIndoorHeight, 'e', 20) } );
			fUserInputs->writeRow({ "HEATMAP_RLAN_INDOOR_HEIGHT_UNCERTAINTY (m)", QString::number(_heatmapRLANIndoorHeightUncertainty, 'e', 20) } );

			fUserInputs->writeRow({ "HEATMAP_RLAN_OUTDOOR_EIRP (dBm)", QString::number(_heatmapRLANOutdoorEIRPDBm, 'e', 20) } );
			fUserInputs->writeRow({ "HEATMAP_RLAN_OUTDOOR_HEIGHT_TYPE", QString(_heatmapRLANOutdoorHeightType) } );
			fUserInputs->writeRow({ "HEATMAP_RLAN_OUTDOOR_HEIGHT (m)", QString::number(_heatmapRLANOutdoorHeight, 'e', 20) } );
			fUserInputs->writeRow({ "HEATMAP_RLAN_OUTDOOR_HEIGHT_UNCERTAINTY (m)", QString::number(_heatmapRLANOutdoorHeightUncertainty, 'e', 20) } );
		}

	}
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::computeRlanPropEnv                                                         */
/******************************************************************************************/
CConst::PropEnvEnum AfcManager::computeRlanPropEnv(double lonDeg, double latDeg, bool errorFlag) const
{
	// If user uses a density map versus a constant environmental input
	int rlanLonIdx;
	int rlanLatIdx;
	CConst::PropEnvEnum rlanPropEnv;
	if (_propagationEnviro.toStdString() == "Population Density Map" || _propagationEnviro.isEmpty()) { //.isEmpty() == true in DEBUG mode
                int regionIdx;
		char rlanPropEnvChar;
		_popGrid->findDeg(lonDeg, latDeg, rlanLonIdx, rlanLatIdx, rlanPropEnvChar, regionIdx);

		switch (rlanPropEnvChar)
		{
			case 'U':
				rlanPropEnv = CConst::urbanPropEnv;
				break;
			case 'S':
				rlanPropEnv = CConst::suburbanPropEnv;
				break;
			case 'R':
				rlanPropEnv = CConst::ruralPropEnv;
				break;
			case 'B':
				rlanPropEnv = CConst::barrenPropEnv;
				break;
			case 'X':
				rlanPropEnv = CConst::unknownPropEnv;
				break;
			default:
				rlanPropEnv = CConst::unknownPropEnv;
				break;
		}

		if ((rlanPropEnv == CConst::unknownPropEnv) && (errorFlag))
		{
			throw std::runtime_error(ErrStream() << "ERROR: RLAN Location LAT = " << latDeg << " LON = " << lonDeg << " outside Simulation Region defined by population density file");
		}
		// For constant set environments:
	} else if (_propagationEnviro.toStdString() == "Urban") {
		rlanPropEnv = CConst::urbanPropEnv;
	} else if (_propagationEnviro.toStdString() == "Suburban") {
		rlanPropEnv = CConst::suburbanPropEnv;
	} else if (_propagationEnviro.toStdString() == "Rural") {
		rlanPropEnv = CConst::ruralPropEnv;
	} else {
		throw std::runtime_error("Error in selecting a constant propagation environment (e.g. Urban)");
	}

	return(rlanPropEnv);
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::computeCluter452HtEl                                                       */
/* See ITU-R p.452-16, Section 4.5.3                                                      */
/******************************************************************************************/
double AfcManager::computeClutter452HtEl(double txHeightM, double distKm, double elevationAngleRad) const
{
	// Input values
	double d_k = 0.07; // Distance (km) from nominal clutter point to antenna
	double h_a = 5.0;  // Nominal clutter height (m) above local ground level

	// double distKm = 1; Test case
	//double frequency = 6; // GHz per ITU-R p.452; this is unused if using F_fc=1

	// Calculate elevationAngleClutterLimitDeg
	double tanVal = (h_a - txHeightM) / (d_k * 1e3);                  // Tan value of elevation AngleClutterLimitDeg
	double elevationAngleClutterLimitDeg = atan(tanVal) * 180 / M_PI; // Returns elevationAngleClutterLimit in degrees

	// Calculate calculate clutter loss
	double htanVal = 6 * (txHeightM / h_a - 0.625); // htan angle
	// double F_fc = 0.25 + 0.375 * (1 + tanh(7.5 * (frequency - 0.5))); // Use this equation for non-6GHz frequencies
	double F_fc = 1;                                                        // Near 6GHz frequency, this is always approximately 1
	double A_h = 10.25 * F_fc * exp(-1 * d_k) * (1 - tanh(htanVal)) - 0.33; // Clutter loss

	if ((elevationAngleRad <= (elevationAngleClutterLimitDeg * M_PI / 180)) && (distKm > d_k * 10)) // If signal is below clutter
		return (A_h);                                                                               // Set path clutter loss
	else
		return (0.0); // Otherwise, no clutter loss
}

/******************************************************************************************/
/* AfcManager::setDBGInputs()                                                             */
/******************************************************************************************/
void AfcManager::setDBGInputs(const std::string& tempDir)
{
	LOGGER_INFO(logger) << "Executing AfcManager::setDBGInputs()";

	setConstInputs(tempDir);

        _serialNumber = "0";
        _aciFlag = true;
        _winner2BldgLOSFlag = true;
        _pathLossClampFSPL = true;

	/**************************************************************************************/
	/* Input Parameters                                                                   */
	/**************************************************************************************/
	// _analysisType = "HeatmapAnalysis";
	// _analysisType = "ExclusionZoneAnalysis";
	_analysisType = "PointAnalysis";
	// _analysisType = "test_aciFn";

        _inquiredFrquencyRangesMHz.push_back(std::pair<int, int>(5945,7095)); // list of low-high frequencies in MHz

        auto chList = std::vector<int> { 119, 97, 187, 111 };

        _inquiredChannels.push_back(std::pair<int, std::vector<int>>(0, chList));

        createChannelList();

        _use3DEP = true;
	_useLiDAR = false;

        _ulsDataFile = "/var/lib/fbrat/ULS_Database/CONUS_filtered_ULS_21Jan2020_6GHz_1.1.0_fixbps_sort_1record_unii5_7.sqlite3";
        _rasDataFile = "/usr/share/fbrat/rat_transfer/RAS_Database/RASdatabase.csv";

	if (_analysisType == "HeatmapAnalysis") {
		_heatmapRLANBWHz = 20.0e6;
		_heatmapRLANChanIdx = 9;
		_rlanLLA = std::make_tuple(40.74326, -73.98719, 55.0); // lat (deg), lon (deg), height (m) (this should be within a building in the NYC demo data)
		_heatmapMinLon = -73.9871;
		_heatmapMaxLon = -73.9870;
		_heatmapMinLat =  40.7432;
		_heatmapMaxLat =  40.7433;
		_heatmapRLANSpacing = 10.0;
		_heatmapIndoorOutdoorStr = "Database";   // Can be: "Indoor", "Outdoor", "Database"

		_heatmapRLANIndoorEIRPDBm = 36.0;
		_heatmapRLANIndoorHeight = 30.0;
		_heatmapRLANIndoorHeightUncertainty = 5.0;

		_heatmapRLANOutdoorEIRPDBm = 30.0;
		_heatmapRLANOutdoorHeight = 5.0;
		_heatmapRLANOutdoorHeightUncertainty = 2.0;

		_heatmapRLANIndoorHeightType = "AGL";
		_heatmapRLANOutdoorHeightType = "AGL";

		_IoverN_threshold_dB = -6.0; // IoverN not to exceed this value for a viable channel
		_bodyLossIndoorDB = 4.0;     // Indoor Body Loss (dB)
		_bodyLossOutdoorDB = 5.0;    // Outdoor Body Loss (dB)
		_polarizationLossDB = 3.0;   // Polarization Loss (dB)
		_rlanOrientation_deg = 0.0; // Orientation (deg) of ellipse clockwise from North

		_buildingType = CConst::traditionalBuildingType; // Defaults to traditionalBuildingType
		_fixedBuildingLossFlag = true;
		_fixedBuildingLossValue = 10.0;

		_confidenceBldg2109 = 0.9;
		_confidenceClutter2108 = 0.9;
		_confidenceWinner2 = 0.9;
		_confidenceITM = 0.9;

		_winner2ProbLOSThr = 0.2;

		_receiverFeedLoss_dB = 2.0;

		_pathLossModelStr = "ITM_BLDG";

		_minEIRP_dBm = 0.0;
		_maxEIRP_dBm = 34.0; // Don't forget that units are dBm

		_rlanType = RLANType::RLAN_INDOOR;
	} else if (_analysisType == "ExclusionZoneAnalysis") {
		_exclusionZoneFSID = 93911;
		_exclusionZoneRLANBWHz = 160.0e6;
		_exclusionZoneRLANChanIdx = 46;
		_exclusionZoneRLANEIRPDBm = 24.0;

		_rlanLLA = std::make_tuple(0.0, 0.0, 20.0); // lat (deg), lon (deg), height (m) (this should be within a building in the NYC demo data)
		_rlanUncerts_m = std::make_tuple(0.0, 0.0, 0.0);    // minor uncertainity, major uncertainity, height uncertainty;

		_rlanHeightType = "AGL";
		// _rlanHeightType = "AMSL";

		_minEIRP_dBm = 0.0;
		_maxEIRP_dBm = 34.0; // Don't forget that units are dBm

		_IoverN_threshold_dB = -6.0; // IoverN not to exceed this value for a viable channel
		_bodyLossIndoorDB = 4.0;     // Indoor Body Loss (dB)
		_bodyLossOutdoorDB = 4.0;    // Outdoor Body Loss (dB)
		_polarizationLossDB = 3.0;   // Polarization Loss (dB)
		_rlanOrientation_deg = 0.0; // Orientation (deg) of ellipse clockwise from North

		// _buildingType = CConst::traditionalBuildingType; // Defaults to traditionalBuildingType
		_buildingType = CConst::noBuildingType; // Defaults to traditionalBuildingType
		_fixedBuildingLossFlag = false;
		_fixedBuildingLossValue = 10.0;

		_confidenceBldg2109 = 0.5;
		_confidenceClutter2108 = 0.5;
		_confidenceWinner2 = 0.5;
		_confidenceITM = 0.5;

		_winner2ProbLOSThr = 0.2;

		_receiverFeedLoss_dB = 2.0;

		_pathLossModelStr = "ITM_BLDG";
		// _pathLossModelStr = "COALITION_OPT_6";
		// _pathLossModelStr = "FSPL";
	} else {
                // In Canada
		// _rlanLLA = std::make_tuple(53.87, -103.83, 20.0); // lat (deg), lon (deg), height (m)

		// _rlanLLA = std::make_tuple(40.76252, -73.95735, 20.0); // lat (deg), lon (deg), height (m) (this should be within a building in the NYC demo data)

		// _rlanLLA = std::make_tuple(40.787960, -73.931108, 20.0); // lat (deg), lon (deg), height (m)

		_rlanLLA = std::make_tuple(40.75923, -73.976731,   20.0); // lat (deg), lon (deg), height (m)

		// _rlanLLA = std::make_tuple(43.0, -72.0, 20.0); // lat (deg), lon (deg), height (m)

                if (1) {
		    _rlanUncerts_m = std::make_tuple(1.0, 1.0, 0.0);    // minor uncertainity, major uncertainity, height uncertainty;
		    _rlanUncertaintyRegionType = RLANBoundary::ELLIPSE;
                } else if (0) {
		    _rlanUncertaintyRegionType = RLANBoundary::LINEAR_POLY;

		    _rlanLinearPolygon.clear();
		    _rlanLinearPolygon.push_back(std::make_pair(40.762435, -73.957537));
		    _rlanLinearPolygon.push_back(std::make_pair(40.762554, -73.957116));
		    _rlanLinearPolygon.push_back(std::make_pair(40.762884, -73.956976));
		    _rlanLinearPolygon.push_back(std::make_pair(40.763060, -73.957507));
		    _rlanLinearPolygon.push_back(std::make_pair(40.762464, -73.957877));

                    double centerLongitude;
                    double centerLatitude;
                    {
                        double sumX = 0.0;
                        double sumY = 0.0;
                        double sumA = 0.0;
                        int i;
                        for(i=0; i< (int) _rlanLinearPolygon.size(); i++) {
                            double xval0 = _rlanLinearPolygon[i].second;
                            double yval0 = _rlanLinearPolygon[i].first;
                            double xval1 = _rlanLinearPolygon[(i+1)%_rlanLinearPolygon.size()].second;
                            double yval1 = _rlanLinearPolygon[(i+1)%_rlanLinearPolygon.size()].first;
                            sumX += (xval0 + xval1)*(xval0*yval1-xval1*yval0);
                            sumY += (yval0 + yval1)*(xval0*yval1-xval1*yval0);
                            sumA += (xval0*yval1-xval1*yval0);
                        }
                        centerLongitude = sumX / (3*sumA);
                        centerLatitude  = sumY / (3*sumA);
                    }

		    _rlanLLA = std::make_tuple(centerLatitude,
			centerLongitude,
			20.0);
		    _rlanUncerts_m = std::make_tuple(std::numeric_limits<double>::quiet_NaN(),
			std::numeric_limits<double>::quiet_NaN(),
			5.0);
                } else if (0) {
		    _rlanUncertaintyRegionType = RLANBoundary::RADIAL_POLY;

		    _rlanRadialPolygon.clear();
		    _rlanRadialPolygon.push_back(std::make_pair(  0.0,  5.0));
		    _rlanRadialPolygon.push_back(std::make_pair( 45.0,  6.0));
		    _rlanRadialPolygon.push_back(std::make_pair( 90.0,  7.0));
		    _rlanRadialPolygon.push_back(std::make_pair(135.0,  8.0));
		    _rlanRadialPolygon.push_back(std::make_pair(180.0,  9.0));
		    _rlanRadialPolygon.push_back(std::make_pair(225.0, 10.0));
		    _rlanRadialPolygon.push_back(std::make_pair(270.0, 11.0));
		    _rlanRadialPolygon.push_back(std::make_pair(315.0, 12.0));

		    _rlanUncerts_m = std::make_tuple(std::numeric_limits<double>::quiet_NaN(),
			std::numeric_limits<double>::quiet_NaN(),
			5.0);
                } else if (0) {
		    _rlanUncerts_m = std::make_tuple(0.0, 0.0, 0.0);  // minor uncertainity, major uncertainity, height uncertainty;
		    _rlanUncertaintyRegionType = RLANBoundary::ELLIPSE;
                }

		_rlanHeightType = "AGL";
		// _rlanHeightType = "AMSL";

		_minEIRP_dBm = 24.0;
		_maxEIRP_dBm = 34.0; // Don't forget that units are dBm

		_IoverN_threshold_dB = -6.0; // IoverN not to exceed this value for a viable channel
		_bodyLossIndoorDB = 4.0;     // Indoor Body Loss (dB)
		_bodyLossOutdoorDB = 5.0;    // Outdoor Body Loss (dB)
		_polarizationLossDB = 3.0;   // Polarization Loss (dB)
		_rlanOrientation_deg = 22.5; // Orientation (deg) of ellipse clockwise from North

		// _buildingType = CConst::traditionalBuildingType; // Defaults to traditionalBuildingType
		_buildingType = CConst::noBuildingType; // outdoor
		_fixedBuildingLossFlag = false;
		_fixedBuildingLossValue = 10.0;

		_confidenceBldg2109 = 0.9;
		_confidenceClutter2108 = 0.9;
		_confidenceWinner2 = 0.9;
		_confidenceITM = 0.9;

		_winner2ProbLOSThr = 0.2;

		_receiverFeedLoss_dB = 2.0;

		// _pathLossModelStr = "ITM_BLDG";
                _pathLossModelStr = "FCC_6GHZ_REPORT_AND_ORDER";
		// _pathLossModelStr = "COALITION_OPT_6";
		// _pathLossModelStr = "FSPL";
	}
	/**************************************************************************************/

	if (_buildingType == CConst::noBuildingType) {
            _bodyLossDB = _bodyLossOutdoorDB;
            _rlanType = RLANType::RLAN_OUTDOOR;
        } else {
            _bodyLossDB = _bodyLossIndoorDB;
            _rlanType =  RLANType::RLAN_INDOOR;
        }

	_ulsAntennaPatternFile = "";
	// _ulsAntennaPatternFile = "/home/mmandell/facebook_rlan_afc/trunk/afc_antennas_example.csv";

}

/**************************************************************************************/
/* AfcManager::setConstInputs()                                                       */
/**************************************************************************************/
void AfcManager::setConstInputs(const std::string& tempDir)	
{
	QDir tempBuild = QDir();
	if (!tempBuild.exists(QString::fromStdString(tempDir))) {
		tempBuild.mkdir(QString::fromStdString(tempDir));
	}

	SearchPaths::init();

	/**************************************************************************************/
	/* Constant Parameters                                                                */
	/**************************************************************************************/

	_minRlanHeightAboveTerrain = 1.0;
	_maxNumPointsITM = 2000;

	_maxRadius = 150.0e3;
	_exclusionDist = 1.0;

	_illuminationEfficiency = 1.0;
	_closeInHgtFlag = true;            // Whether or not to force LOS when mobile height above closeInHgtLOS for close in model",
	_closeInHgtLOS = 15.0;             // RLAN height above which prob of LOS = 100% for close in model",
	_closeInDist = 1.0e3;              // Radius in which close in path loss model is used
	_closeInPathLossModel = "WINNER2"; // Close in path loss model is used

	_wlanMinFreq = 5945.0e6;
	_wlanMaxFreq = 7125.0e6;

	_srtmDir = SearchPaths::forReading("data", "fbrat/rat_transfer/srtm3arcsecondv003", true).toStdString();
	
        _depDir = SearchPaths::forReading("data", "fbrat/rat_transfer/3dep/1_arcsec", true).toStdString();

	_lidarDir = SearchPaths::forReading("data", "fbrat/rat_transfer/proc_lidar_2019", true);

    _globeDir = SearchPaths::forReading("data", "fbrat/rat_transfer/globe", true);

	// _popDensityFile = SearchPaths::forReading("population", "conus_1arcmin.sqlite3", true).toStdString();
	_popDensityFile = SearchPaths::forReading("data", "fbrat/rat_transfer/population/conus_1arcmin.sqlite3", true).toStdString();
	_popDensityResLon = 1.0 / 60;
	_popDensityResLat = 1.0 / 60;
	_popDensityMinLon = -124.7333;
	_popDensityNumLon = 3467;
	_popDensityMinLat = 24.5333;
	_popDensityNumLat = 1491;

	_densityThrUrban = 486.75e-6;
	_densityThrSuburban = 211.205e-6;
	_densityThrRural = 57.1965e-6;

	_ulsNoiseFigureDB = 5.0;
	_removeMobile = true;

	_filterSimRegionOnly = false;

 	// _rlanBWStr = "20.0e6,40.0e6,80.0e6,160.0e6"; // Channel bandwidths in Hz

	_visibilityThreshold = -10000.0;

	_useFixedRxAntennaFeederLoss = true;

        _worldPopulationFile = SearchPaths::forReading("data", "fbrat/rat_transfer/population/gpw_v4_population_density_rev11_2020_30_sec.tif", true).toStdString();
        _regionPolygonResolution = 1.0e-5;

	_regionStr = "CONUS";
        if (_regionStr == "CONUS") {
            _regionPolygonFileList = SearchPaths::forReading("data", "fbrat/rat_transfer/population/conus.kml", true).toStdString();
        } else if (_regionStr == "Canada") {
            _regionPolygonFileList = SearchPaths::forReading("data", "fbrat/rat_transfer/population/Canada.kml", true).toStdString();
        } else {
            throw std::runtime_error(
                ErrStream() << "AfcManager::setDBGInputs(): regionStr = \"" << _regionStr << "\" not supported"
				);
        }

    	_excThrFile = QDir(QString::fromStdString(tempDir)).filePath("exc_thr.csv.gz").toStdString();
	_fsAnomFile = QDir(QString::fromStdString(tempDir)).filePath("fs_anom.csv.gz").toStdString();
	_userInputsFile = QDir(QString::fromStdString(tempDir)).filePath("userInputs.csv.gz").toStdString();
	_kmlFile = QDir(QString::fromStdString(tempDir)).filePath("results.kmz").toStdString();
	_progressFile = QDir(QString::fromStdString(tempDir)).filePath("progress.txt").toStdString();

	/**************************************************************************************/
}
/**************************************************************************************/

/**************************************************************************************/
/* AfcManager::convertCFI()                                                           */
/**************************************************************************************/
int AfcManager::convertCFI(int cfi, int& bandwidthMHz, int& startFreqMHz, int& stopFreqMHz)
{
    int posn = 0;
    int retval;

    while(cfi & (1<<posn)) {
        posn++;
    }

    if ((posn == 0) || (posn > 4)) {
        retval = 0;
    } else {
        bandwidthMHz = 20*(1<<(posn-1));
        startFreqMHz = 5945 + (cfi - (1<<posn) + 1)*5;
        stopFreqMHz = startFreqMHz + bandwidthMHz;
        if (stopFreqMHz > 7125) {
            retval = 0;
        } else {
            retval = 1;
        }
    }

    return(retval);
}
/**************************************************************************************/

/**************************************************************************************/
/* AfcManager::computeInquiredFreqRangesPSD()                                         */
/**************************************************************************************/
void AfcManager::computeInquiredFreqRangesPSD(std::vector<psdFreqRangeClass> &psdFreqRangeList)
{
    for(auto freqRange : _inquiredFrquencyRangesMHz) {
        auto startFreqMHz = freqRange.first;
        auto stopFreqMHz = freqRange.second;
        psdFreqRangeClass psdFreqRange;
        psdFreqRange.freqMHzList.push_back(startFreqMHz);

        int prevFreqMHz = startFreqMHz;
        while(prevFreqMHz < stopFreqMHz) {
            bool initFlag = true;
            int nextFreqMHz = stopFreqMHz;
            double minPSD;
            for(auto channel : _channelList) {
                if (channel.type == INQUIRED_FREQUENCY) {
                    if ((channel.startFreqMHz <= prevFreqMHz) && (channel.stopFreqMHz > prevFreqMHz)) {
                        double psd = channel.eirpLimit_dBm - 10.0*log(channel.bandwidth())/log(10.0);
                        if ((initFlag) || (psd < minPSD)) {
                            minPSD = psd;
                        }
                        if ((initFlag) || (channel.stopFreqMHz < nextFreqMHz)) {
                            nextFreqMHz = channel.stopFreqMHz;
                        }
                        initFlag = false;
                    }
                 }
            }
            if (initFlag) {
				throw std::runtime_error("Error computing PSD over inquired frequency range");
            }
            psdFreqRange.freqMHzList.push_back(nextFreqMHz);
            psdFreqRange.psd_dBm_MHzList.push_back(minPSD);
            prevFreqMHz = nextFreqMHz;
        }

        int segIdx;
        for(segIdx=psdFreqRange.psd_dBm_MHzList.size()-2; segIdx>=0; --segIdx) {
            if (psdFreqRange.psd_dBm_MHzList[segIdx] == psdFreqRange.psd_dBm_MHzList[segIdx+1]) {
                psdFreqRange.psd_dBm_MHzList.erase (psdFreqRange.psd_dBm_MHzList.begin()+segIdx+1);
                psdFreqRange.freqMHzList.erase(psdFreqRange.freqMHzList.begin()+segIdx+1);
            }
        }

        psdFreqRangeList.push_back(psdFreqRange);
    }

    return;
}
/**************************************************************************************/

/**************************************************************************************/
/* AfcManager::createChannelList()                                                    */
/**************************************************************************************/
void AfcManager::createChannelList()
{
    // add channel plan to channel list
    auto bwList = std::vector<int> { 20, 40, 80, 160 };
    int minFreqMHz = (int) floor(_wlanMinFreq*1.0e-6 + 0.5);
    int maxFreqMHz = (int) floor(_wlanMaxFreq*1.0e-6 + 0.5);

    for(auto freqRange : _inquiredFrquencyRangesMHz) {
        auto startFreqMHz = freqRange.first;
        auto stopFreqMHz = freqRange.second;

        if (   (stopFreqMHz > startFreqMHz) 
			&& (stopFreqMHz <= maxFreqMHz) 
			&& (startFreqMHz >= minFreqMHz)) {

            for(auto bwMHz : bwList) {
                int startChanIdx = (startFreqMHz - minFreqMHz)/bwMHz;
                int stopChanIdx = (stopFreqMHz - minFreqMHz - 1)/bwMHz;
                for (int chanIdx = startChanIdx; chanIdx <= stopChanIdx; ++chanIdx) {
                    ChannelStruct channel;
                    channel.startFreqMHz = minFreqMHz + chanIdx*bwMHz;
                    channel.stopFreqMHz = channel.startFreqMHz + bwMHz;
                    channel.availability = GREEN; // Everything initialized to GREEN
                    channel.type = INQUIRED_FREQUENCY;
                    channel.eirpLimit_dBm = 0;
                    _channelList.push_back(channel);
                }
            }
        }
		else
		{
			// the start/stop frequencies are not valid
			throw std::runtime_error("UNSUPPORTED_SPECTRUM: Invalid frequency range. Valid frequencies are in [5945 MHz, 7125 MHz].");
		}
    }

    for(auto channelPair : _inquiredChannels) {
		LOGGER_DEBUG(logger) << "creating channels for operating class " << channelPair.first; 
        auto cfiList = channelPair.second;
		if (cfiList.size() == 0 && channelPair.first == 133) {
			LOGGER_DEBUG(logger) << "creating ALL channels for operating class " << channelPair.first;
			// include all channels in global operating class 133 which is our default
			auto numChannels = std::vector<int> { 59, 29, 14, 7 };
			auto startIndex = std::vector<int> { 1, 3, 7, 15 };
			auto indexInc = std::vector<int> { 4, 8, 16, 32 };
			int startFreq = 5945; // MHz
			for(int bwIdx = 0; bwIdx < (int) bwList.size(); bwIdx++) 
			{
				for (int chanIdx = 0; chanIdx < numChannels.at(bwIdx); chanIdx++)
				{
					ChannelStruct channel;
					channel.operatingClass = channelPair.first;
					channel.index = startIndex.at(bwIdx) + chanIdx * indexInc.at(bwIdx);
					channel.startFreqMHz = startFreq + chanIdx * bwList.at(bwIdx);
					channel.stopFreqMHz = startFreq + (chanIdx + 1) * bwList.at(bwIdx);
					channel.type = ChannelType::INQUIRED_CHANNEL;
					channel.availability = GREEN; // Everything initialized to GREEN
					channel.eirpLimit_dBm = 0;
					_channelList.push_back(channel);
				}
			}
			LOGGER_DEBUG(logger) << "added " << (59+29+14+7) << " channels";
		} else if (cfiList.size() == 0) {
			throw std::runtime_error("UNSUPPORTED_SPECTRUM Global operating class " + std::to_string(channelPair.first) + " not supported. 133 is currently the only supported class.");
		}
        for(auto cfi : cfiList) {
			LOGGER_DEBUG(logger) << "creating cherry picked channels in operating class " << channelPair.first;
            int bandwidthMHz, startFreqMHz, stopFreqMHz;
            if (convertCFI(cfi, bandwidthMHz, startFreqMHz, stopFreqMHz)) {
                ChannelStruct channel;
                channel.startFreqMHz = startFreqMHz;
                channel.stopFreqMHz = stopFreqMHz;
				channel.index = cfi;
				channel.operatingClass = channelPair.first;
                channel.availability = GREEN; // Everything initialized to GREEN
                channel.type = INQUIRED_CHANNEL;
                channel.eirpLimit_dBm = 0;
                _channelList.push_back(channel);
            } else {
				throw std::runtime_error("UNSUPPORTED_SPECTRUM Invalid channel with index " + std::to_string(cfi) + " found.");
            }
        }
    }
}
/**************************************************************************************/

