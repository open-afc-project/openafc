// AfcManager.cpp -- Manages I/O and top-level operations for the AFC Engine
#include "AfcManager.h"
#include "RlanRegion.h"

// "--runtime_opt" masks
#define RUNTIME_OPT_ENABLE_DBG 1
#define RUNTIME_OPT_ENABLE_GUI 2
#define RUNTIME_OPT_URL 4

extern double qerfi(double q);

QJsonArray jsonChannelData(const std::vector<ChannelStruct> &channelList);
QJsonObject jsonSpectrumData(const std::vector<ChannelStruct> &channelList, const QJsonObject &deviceDesc, const double &startFreq);

double noiseFloorToNoiseFigure(double noiseFloor) {
	const double B = CConst::boltzmannConstant;
	return noiseFloor - 30.0 - 10.0 * log10(290.0 * B * pow(10.0, 6.0));
}

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

		// const double fixedEpsDielect = 15;
		// const double fixedSgmConductivity = 0.005;
		// const double fixedEsNoSurfRef = 301;
		// const int fixedRadioClimate = 6;
		// const int fixedPolarization = 1;
		// const double fixedConfidence = 0.5;
		// const double fixedRelevance = 0.5;
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

namespace OpClass
{
	// Note: Per 11ax D 8.0, section 27.3..23.2
	// Channel's start frequency is calculated using formula
	// Channel center frequency = Channel starting frequency + 5 × nch
	// where nch = 1, 2, ...233, is the channel index
	// For channel 1, center frequency is (5950 + 5 * 1) = 5955
	// and start frequency = (center frequency - BW/2) = (5955 - 20/2) = 5945
	OpClass  GlobalOpClass_131 =
	{
		131,		// Operating class
		20,		// Bandwidth
		5950,		// Start frequency.
		{
			1,   5,   9,  13,  17,  21,  25,  29,
			33,  37,  41,  45,  49,  53,  57,  61,
			65,  69,  73,  77,  81,  85,  89,  93,
			97, 101, 105, 109, 113, 117, 121, 125,
			129, 133, 137, 141, 145, 149, 153, 157,
			161, 165, 169, 173, 177, 181, 185, 189,
			193, 197, 201, 205, 209, 213, 217, 221,
			225, 229, 233
		}
	};

	const OpClass GlobalOpClass_132 =
	{
		132,
		40,
		5950,
		{
			3,  11,  19,  27,  35,  43,  51, 59,
			67,  75,  83,  91,  99, 107, 115, 123,
			131, 139, 147, 155, 163, 171, 179, 187,
			195, 203, 211, 219, 227
		}
	};

	const OpClass GlobalOpClass_133 =
	{
		133,
		80,
		5950,
		{
			7,   23,   39,  55,  71,  87, 103, 119,
			135, 151, 167, 183, 199, 215
		}
	};

	const OpClass GlobalOpClass_134 =
	{
		134,
		160,
		5950,
		{
			15,  47,  79, 111, 143, 175, 207
		}
	};

	const OpClass GlobalOpClass_135 =
	{
		135,
		80,
		5950,
		{
			7,   23,   39,  55,  71,  87, 103, 119,
			135, 151, 167, 183, 199, 215
		}
	};

	const OpClass GlobalOpClass_136 =
	{
		136,
		20,
		5925,
		{ 2 }
	};

	const std::vector<OpClass> USOpClass =
	{
		GlobalOpClass_131,
		GlobalOpClass_132,
		GlobalOpClass_133,
		GlobalOpClass_134,
		// Opclass 135 is a non contiguous 80+80 channels.
		// GlobalOpClass_135,
		GlobalOpClass_136
	};

	// Hardcode for PSD to only consider 20MHz channels
	const std::vector<OpClass> PSDOpClassList =
	{
		GlobalOpClass_131,
		GlobalOpClass_136
	};

}

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

	_pathLossModel = CConst::unknownPathLossModel;
	_responseCode = CConst::successResponseCode;
	_ulsDefaultAntennaType = CConst::F1245AntennaType;
	_rlanITMTxClutterMethod = CConst::ForceTrueITMClutterMethod;
	_createKmz = false;
	_createDebugFiles = false;
	_mapDataGeoJsonFile = "";

	_nfa = (NFAClass *) NULL;
	_prTable = (PRTABLEClass *) NULL;
}

/******************************************************************************************/
/**** FUNCTION: AfcManager::~AfcManager()                                              ****/
/******************************************************************************************/
AfcManager::~AfcManager()
{
	clearData();
	if (AfcManager::_dataIf) {
		delete AfcManager::_dataIf;
	}
	delete _ulsList;
	delete _rasList;
}
/******************************************************************************************/

/******************************************************************************************/
/**** sortFunction used for RADIAL_POLY                                                ****/
/******************************************************************************************/
bool sortFunction(std::pair<double, double> p0,std::pair<double, double> p1) { return (p0.first<p1.first); }
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::initializeDatabases()                                      ****/
/******************************************************************************************/
void AfcManager::initializeDatabases()
{
	if (_responseCode != CConst::successResponseCode) {
		return;
	}

	// Following lines are finding the minimum and maximum longitudes and latitudes with the 150km rlan range
	double minLon, maxLon, minLat, maxLat;
	double minLonBldg, maxLonBldg, minLatBldg, maxLatBldg;

	createChannelList();

	if (_responseCode != CConst::successResponseCode) {
		return;
	}

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
	switch(_pathLossModel) {
		case CConst::ITMBldgPathLossModel:
			_closeInDist = 0.0;              // Radius in which close in path loss model is used
			break;
		case CConst::CoalitionOpt6PathLossModel:
			_closeInDist = 1.0e3;              // Radius in which close in path loss model is used
			break;
		case CConst::FCC6GHzReportAndOrderPathLossModel:
			_closeInDist = 1.0e3;              // Radius in which close in path loss model is used
			break;
		case CConst::FSPLPathLossModel:
			_closeInDist = 0.0;              // Radius in which close in path loss model is used
			break;
		default:
			throw std::runtime_error(ErrStream() << std::string("ERROR: Path Loss Model set to invalid value \"") + CConst::strPathLossModelList->type_to_str(_pathLossModel) + "\"");
			break;
	}

	ULSClass::pathLossModel = _pathLossModel;

	/**************************************************************************************/

	if (_analysisType == "AP-AFC" ||  _analysisType == "ScanAnalysis") {
		bool fixedHeightAMSL;
		if (_rlanType ==  RLANType::RLAN_INDOOR) {
			fixedHeightAMSL = _indoorFixedHeightAMSL;
		} else {
			fixedHeightAMSL = false;
		}

		double centerLat, centerLon;

		/**************************************************************************************/
		/* Create Rlan Uncertainty Region                                                     */
		/**************************************************************************************/
		switch(_rlanUncertaintyRegionType) {
			case ELLIPSE:
				_rlanRegion = (RlanRegionClass *) new EllipseRlanRegionClass(_rlanLLA, _rlanUncerts_m, _rlanOrientation_deg, fixedHeightAMSL);
				break;
			case LINEAR_POLY:
				_rlanRegion = (RlanRegionClass *) new PolygonRlanRegionClass(_rlanLLA, _rlanUncerts_m, _rlanLinearPolygon, LINEAR_POLY, fixedHeightAMSL);
				break;
			case RADIAL_POLY:
				std::sort(_rlanRadialPolygon.begin(), _rlanRadialPolygon.end(), sortFunction);
				_rlanRegion = (RlanRegionClass *) new PolygonRlanRegionClass(_rlanLLA, _rlanUncerts_m, _rlanRadialPolygon, RADIAL_POLY, fixedHeightAMSL);
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
			double maxDistBldg;
			if (_rlanITMTxClutterMethod == CConst::BldgDataITMCLutterMethod) {
				maxDistBldg = _maxRadius;
			} else {
				maxDistBldg = _closeInDist;
			}

			minLatBldg = centerLat - ((maxDistBldg + rlanRegionSize) / CConst::earthRadius) * 180.0 / M_PI;
			maxLatBldg = centerLat + ((maxDistBldg + rlanRegionSize) / CConst::earthRadius) * 180.0 / M_PI;

			double maxAbsLatBldg = std::max(fabs(minLatBldg), fabs(maxLatBldg));
			minLonBldg = centerLon - ((maxDistBldg + rlanRegionSize) / (CConst::earthRadius * cos(maxAbsLatBldg * M_PI / 180.0))) * 180.0 / M_PI;
			maxLonBldg = centerLon + ((maxDistBldg + rlanRegionSize) / (CConst::earthRadius * cos(maxAbsLatBldg * M_PI / 180.0))) * 180.0 / M_PI;
		} else {
			minLatBldg = minLat;
			maxLatBldg = maxLat;
			minLonBldg = minLon;
			maxLonBldg = maxLon;
		}
	} else if (_analysisType == "ExclusionZoneAnalysis") {
		readULSData(_ulsDatabaseList, (PopGridClass *) NULL, 0, ulsMinFreq, ulsMaxFreq, _removeMobile, CConst::FixedSimulation, 0.0, 0.0, 0.0, 0.0);
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
#if DEBUG_AFC
	} else if (_analysisType == "test_winner2") {
		// Do nothing
#endif
	} else {
		throw std::runtime_error(QString("Invalid analysis type: %1").arg(QString::fromStdString(_analysisType)).toStdString());
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

		// _lidarDir = SearchPaths::forReading("data", "/mnt/s3/rat_transfer/proc_lidar_2019", true);
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
	if (_pathLossModel == CConst::ITMBldgPathLossModel) {
		if (_terrainDataModel->getNumLidarRegion() == 0) {
			throw std::runtime_error(ErrStream() << "Path loss model set to ITM_BLDG, but no building data found within the analysis region.");
		}
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Setup NLCD data                                                                    */
	/**************************************************************************************/
	if (!_nlcdFile.empty()) {
		cgNlcd.reset(new CachedGdal<uint8_t>(_nlcdFile, "nlcd"));
		cgNlcd->setNoData(0);
	} else {
		throw std::runtime_error("AfcManager::initializeDatabases(): _nlcdFile not defined.");
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Setup ITU data                                                                    */
	/**************************************************************************************/
	_ituData = new ITUDataClass(_radioClimateFile, _surfRefracFile);
	LOGGER_INFO(logger) << "Reading ITU data files: " << _radioClimateFile << " and " << _surfRefracFile;
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
	/* Read Near Field Adjustment table                                                   */
	/**************************************************************************************/
	if (_nearFieldAdjFlag) {
		_nfa = new NFAClass(_nfaTableFile);
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Read Passive Repeater table                                                        */
	/**************************************************************************************/
	if (_passiveRepeaterFlag) {
		_prTable = new PRTABLEClass(_prTableFile);
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Read Population data                                                               */
	/**************************************************************************************/
	if (_propagationEnviro.toStdString() == "Population Density Map") {
		readPopulationGrid(); // Reads population density in grid of lat/lon, multiply by area to get population of specified regions
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Read ULS data                                                                      */
	/**************************************************************************************/
	//_ulsDataFile = "/home/ssmucny/ULS_23Jan2019_perlink_fixedwithbas.sqlite3";

	if (_analysisType == "HeatmapAnalysis" || _analysisType == "AP-AFC" || _analysisType == "ScanAnalysis") {
		readULSData(_ulsDatabaseList, _popGrid, 0, ulsMinFreq, ulsMaxFreq, _removeMobile, CConst::FixedSimulation, minLat, maxLat, minLon, maxLon);
		readRASData(_rasDataFile);
	} else if (_analysisType == "ExclusionZoneAnalysis") {
		fixFSTerrain();
#if DEBUG_AFC
	} else if (_analysisType == "test_winner2") {
		// Do nothing
#endif
	} else {
		throw std::runtime_error(QString("Invalid analysis type: %1").arg(QString::fromStdString(_analysisType)).toStdString());
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Convert confidences it gaussian thresholds                                         */
	/**************************************************************************************/
	_zbldg2109 = -qerfi(_confidenceBldg2109);
	_zclutter2108 = -qerfi(_confidenceClutter2108);
	_fsZclutter2108 = -qerfi(_fsConfidenceClutter2108);
	_zwinner2LOS = -qerfi(_confidenceWinner2LOS);
	_zwinner2NLOS = -qerfi(_confidenceWinner2NLOS);
	_zwinner2Combined = -qerfi(_confidenceWinner2Combined);
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

	if (_nearFieldAdjFlag) {
		delete _nfa;
		_nfa = (NFAClass *) NULL;
	}

	if (_passiveRepeaterFlag) {
		delete _prTable;
		_prTable = (PRTABLEClass *) NULL;
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

/******************************************************************************************/
/**** FUNCTION: AfcManager::importGUIjson                                              ****/
/******************************************************************************************/
void AfcManager::importGUIjson(const std::string &inputJSONpath)
{
	// Read input parameters from GUI in JSON file
	QJsonDocument jsonDoc;
	// Use SearchPaths::forReading("data", ..., true) to ensure that the input file exists before reading it in
	QString fName = QString(inputJSONpath.c_str());
	QByteArray data;

	if (AfcManager::_dataIf->readFile(fName, data)) {
		jsonDoc = QJsonDocument::fromJson(data);
	} else {
		throw std::runtime_error("AfcManager::importGUIjson(): Failed to open JSON file specifying user's input parameters.");
	}

	// Print entirety of imported JSON file to debug log
	LOGGER_DEBUG(logger) << "Contents of imported JSON file: " << std::endl <<
		jsonDoc.toJson().toStdString() << std::endl;

	// Read JSON from file
	QJsonObject jsonObj = jsonDoc.object();


	// Properly set _aciFlag from GUI.  When set to true, ACI is considered in interference calculations, when set to false, there is no ACI.
	_aciFlag = true;

	if (jsonObj.contains("version") && !jsonObj["version"].isUndefined()) {
		_guiJsonVersion = jsonObj["version"].toString();
	} else {
		_guiJsonVersion = QString("1.3");
	}

	if (_guiJsonVersion == "1.3") {
		importGUIjsonVersion1_3(jsonObj);
	} else if (_guiJsonVersion == "1.1") {
		// 1.3 and 1.1 are the same
		importGUIjsonVersion1_3(jsonObj);
	} else if (_guiJsonVersion == "1.0") {
		importGUIjsonVersion1_0(jsonObj);
	} else {
		LOGGER_WARN(logger) <<
			"VERSION NOT SUPPORTED: GUI JSON FILE \"" << inputJSONpath << "\": version: " << _guiJsonVersion;
		_responseCode = CConst::versionNotSupportedResponseCode;
		return;
	}

	return;
}

void AfcManager::importGUIjsonVersion1_3(const QJsonObject &jsonObj)
{
	QString errMsg;

	if ( (_analysisType == "AP-AFC") || (_analysisType == "ScanAnalysis") ) {
		QStringList requiredParams;
		QStringList optionalParams;

		/**********************************************************************/
		/* AvailableSpectrumInquiryRequestMessage Object (Table 5)            */
		/**********************************************************************/
		requiredParams = QStringList() << "availableSpectrumInquiryRequests" << "version";
		optionalParams = QStringList() << "vendorExtensions";
		for(auto key : jsonObj.keys()) {
			int rIdx = requiredParams.indexOf(key);
			if (rIdx != -1) {
				requiredParams.erase(requiredParams.begin()+rIdx);
			} else {
				int oIdx = optionalParams.indexOf(key);
				if (oIdx != -1) {
					optionalParams.erase(optionalParams.begin()+oIdx);
				} else {
					_unexpectedParams << key;
				}
			}
		}
		_missingParams << requiredParams;

		QJsonObject requestObj;
		if (!requiredParams.contains("availableSpectrumInquiryRequests")) {
			QJsonArray requestArray = jsonObj["availableSpectrumInquiryRequests"].toArray();
			if (requestArray.size() != 1) {
				LOGGER_WARN(logger) << "GENERAL FAILURE: afc-engine only processes a single request, "
					<< requestArray.size() << " requests specified";
				_responseCode = CConst::generalFailureResponseCode;
				return;
			}
			requestObj = requestArray.at(0).toObject();
		}
		/**********************************************************************/

		/**********************************************************************/
		/* AvailableSpectrumInquiryRequest Object (Table 6)                   */
		/**********************************************************************/
		requiredParams = QStringList() << "requestId" << "deviceDescriptor" << "location";
		optionalParams = QStringList() << "inquiredFrequencyRange" << "inquiredChannels" << "minDesiredPower" << "vendorExtensions";
		for(auto key : requestObj.keys()) {
			int rIdx = requiredParams.indexOf(key);
			if (rIdx != -1) {
				requiredParams.erase(requiredParams.begin()+rIdx);
			} else {
				int oIdx = optionalParams.indexOf(key);
				if (oIdx != -1) {
					optionalParams.erase(optionalParams.begin()+oIdx);
				} else {
					_unexpectedParams << key;
				}
			}
		}
		_missingParams << requiredParams;

		QJsonObject deviceDescriptorObj;
		if (!requiredParams.contains("deviceDescriptor")) {
			deviceDescriptorObj = requestObj["deviceDescriptor"].toObject();
		}

		QJsonObject locationObj;
		if (!requiredParams.contains("location")) {
			locationObj = requestObj["location"].toObject();
		}

		QJsonArray inquiredFrequencyRangeArray;
		if (!optionalParams.contains("inquiredFrequencyRange")) {
			inquiredFrequencyRangeArray = requestObj["inquiredFrequencyRange"].toArray();
		}

		QJsonArray inquiredChannelsArray;
		if (!optionalParams.contains("inquiredChannels")) {
			inquiredChannelsArray = requestObj["inquiredChannels"].toArray();
		}

		if (!optionalParams.contains("minDesiredPower")) {
			_minEIRP_dBm = requestObj["minDesiredPower"].toDouble();
		} else {
			_minEIRP_dBm = std::numeric_limits<double>::quiet_NaN();
		}
		/**********************************************************************/

		/**********************************************************************/
		/* DeviceDescriptor Object (Table 7)                                  */
		/**********************************************************************/
		requiredParams = QStringList() << "serialNumber" << "certificationId" << "rulesetIds";
		optionalParams = QStringList();
		for(auto key : deviceDescriptorObj.keys()) {
			int rIdx = requiredParams.indexOf(key);
			if (rIdx != -1) {
				requiredParams.erase(requiredParams.begin()+rIdx);
			} else {
				int oIdx = optionalParams.indexOf(key);
				if (oIdx != -1) {
					optionalParams.erase(optionalParams.begin()+oIdx);
				} else {
					_unexpectedParams << key;
				}
			}
		}
		_missingParams << requiredParams;

		QJsonArray certificationIdArray;
		if (!requiredParams.contains("certificationId")) {
			certificationIdArray = deviceDescriptorObj["certificationId"].toArray();
		}

		QJsonArray rulesetIdsArray;
		if (!requiredParams.contains("rulesetIds")) {
			rulesetIdsArray = deviceDescriptorObj["rulesetIds"].toArray();
		}
		/**********************************************************************/

		/**********************************************************************/
		/* CertificationID Object (Table 8)                                   */
		/**********************************************************************/
		for(auto certificationIDVal : certificationIdArray) {
			auto certificationIDObj = certificationIDVal.toObject();
			requiredParams = QStringList() << "nra" << "id";
			optionalParams = QStringList();
			for(auto key : certificationIDObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin()+rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin()+oIdx);
					} else {
						_unexpectedParams << key;
					}
				}
			}
			_missingParams << requiredParams;
		}
		/**********************************************************************/

		/**********************************************************************/
		/* Location Object (Table 9)                                          */
		/**********************************************************************/
		requiredParams = QStringList() << "elevation";
		optionalParams = QStringList() << "ellipse" << "linearPolygon" << "radialPolygon" << "indoorDeployment";
		for(auto key : locationObj.keys()) {
			int rIdx = requiredParams.indexOf(key);
			if (rIdx != -1) {
				requiredParams.erase(requiredParams.begin()+rIdx);
			} else {
				int oIdx = optionalParams.indexOf(key);
				if (oIdx != -1) {
					optionalParams.erase(optionalParams.begin()+oIdx);
				} else {
					_unexpectedParams << key;
				}
			}
		}
		_missingParams << requiredParams;

		int hasIndoorDeploymentFlag = (optionalParams.contains("indoorDeployment") ? 0 : 1);

		int hasEllipseFlag       = (optionalParams.contains("ellipse")       ? 0 : 1);
		int hasLinearPolygonFlag = (optionalParams.contains("linearPolygon") ? 0 : 1);
		int hasRadialPolygonFlag = (optionalParams.contains("radialPolygon") ? 0 : 1);

		int n = hasEllipseFlag + hasLinearPolygonFlag + hasRadialPolygonFlag;

		if (n != 1) {
			LOGGER_WARN(logger) << "GENERAL FAILURE: location object must contain exactly one instance of ellipse, linearPolygon, or radialPolygon, total of "
				<< n << " instances found";
			_responseCode = CConst::generalFailureResponseCode;
			return;
		}

		QJsonObject ellipseObj;
		if (hasEllipseFlag) {
			ellipseObj = locationObj["ellipse"].toObject();
		}

		QJsonObject linearPolygonObj;
		if (hasLinearPolygonFlag) {
			linearPolygonObj = locationObj["linearPolygon"].toObject();
		}

		QJsonObject radialPolygonObj;
		if (hasRadialPolygonFlag) {
			radialPolygonObj = locationObj["radialPolygon"].toObject();
		}

		QJsonObject elevationObj;
		if (!requiredParams.contains("elevation")) {
			elevationObj = locationObj["elevation"].toObject();
		}
		/**********************************************************************/

		/**********************************************************************/
		/* Ellipse Object (Table 10)                                          */
		/**********************************************************************/
		bool hasCenterFlag = false;
		QJsonObject centerObj;
		if (hasEllipseFlag) {
			requiredParams = QStringList() << "center" << "majorAxis" << "minorAxis" << "orientation";
			optionalParams = QStringList();
			for(auto key : ellipseObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin()+rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin()+oIdx);
					} else {
						_unexpectedParams << key;
					}
				}
			}
			_missingParams << requiredParams;

			if (!requiredParams.contains("center")) {
				centerObj = ellipseObj["center"].toObject();
				hasCenterFlag = true;
			}
		}
		/**********************************************************************/

		/**********************************************************************/
		/* LinearPolygon Object (Table 11)                                    */
		/**********************************************************************/
		bool hasOuterBoundaryPointArrayFlag = false;
		QJsonArray outerBoundaryPointArray;
		if (hasLinearPolygonFlag) {
			requiredParams = QStringList() << "outerBoundary";
			optionalParams = QStringList();
			for(auto key : linearPolygonObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin()+rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin()+oIdx);
					} else {
						_unexpectedParams << key;
					}
				}
			}
			_missingParams << requiredParams;

			if (!requiredParams.contains("outerBoundary")) {
				outerBoundaryPointArray = linearPolygonObj["outerBoundary"].toArray();
				hasOuterBoundaryPointArrayFlag = true;
			}
		}
		/**********************************************************************/

		/**********************************************************************/
		/* RadialPolygon Object (Table 12)                                    */
		/**********************************************************************/
		bool hasOuterBoundaryVectorArrayFlag = false;
		QJsonArray outerBoundaryVectorArray;
		if (hasRadialPolygonFlag) {
			requiredParams = QStringList() << "center" << "outerBoundary";
			optionalParams = QStringList();
			for(auto key : radialPolygonObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin()+rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin()+oIdx);
					} else {
						_unexpectedParams << key;
					}
				}
			}
			_missingParams << requiredParams;

			if (!requiredParams.contains("center")) {
				centerObj = radialPolygonObj["center"].toObject();
				hasCenterFlag = true;
			}

			if (!requiredParams.contains("outerBoundary")) {
				outerBoundaryVectorArray = radialPolygonObj["outerBoundary"].toArray();
				hasOuterBoundaryVectorArrayFlag = true;
			}
		}
		/**********************************************************************/

		/**********************************************************************/
		/* ElevationPolygon Object (Table 13)                                 */
		/**********************************************************************/
		requiredParams = QStringList() << "height" << "heightType" << "verticalUncertainty";
		optionalParams = QStringList();
		for(auto key : elevationObj.keys()) {
			int rIdx = requiredParams.indexOf(key);
			if (rIdx != -1) {
				requiredParams.erase(requiredParams.begin()+rIdx);
			} else {
				int oIdx = optionalParams.indexOf(key);
				if (oIdx != -1) {
					optionalParams.erase(optionalParams.begin()+oIdx);
				} else {
					_unexpectedParams << key;
				}
			}
		}
		_missingParams << requiredParams;
		/**********************************************************************/

		/**********************************************************************/
		/* Point Object (Table 14)                                            */
		/**********************************************************************/
		if (hasCenterFlag) {
			requiredParams = QStringList() << "longitude" << "latitude";
			optionalParams = QStringList();
			for(auto key : centerObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin()+rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin()+oIdx);
					} else {
						_unexpectedParams << key;
					}
				}
			}
			_missingParams << requiredParams;
		}

		if (hasOuterBoundaryPointArrayFlag) {
			for(auto outerBoundaryPointVal : outerBoundaryPointArray) {
				auto outerBoundaryPointObj = outerBoundaryPointVal.toObject();
				requiredParams = QStringList() << "longitude" << "latitude";
				optionalParams = QStringList();
				for(auto key : outerBoundaryPointObj.keys()) {
					int rIdx = requiredParams.indexOf(key);
					if (rIdx != -1) {
						requiredParams.erase(requiredParams.begin()+rIdx);
					} else {
						int oIdx = optionalParams.indexOf(key);
						if (oIdx != -1) {
							optionalParams.erase(optionalParams.begin()+oIdx);
						} else {
							_unexpectedParams << key;
						}
					}
				}
				_missingParams << requiredParams;
			}
		}
		/**********************************************************************/

		/**********************************************************************/
		/* Vector Object (Table 15)                                           */
		/**********************************************************************/
		if (hasOuterBoundaryVectorArrayFlag) {
			for(auto outerBoundaryVectorVal : outerBoundaryVectorArray) {
				auto outerBoundaryVectorObj = outerBoundaryVectorVal.toObject();
				requiredParams = QStringList() << "length" << "angle";
				optionalParams = QStringList();
				for(auto key : outerBoundaryVectorObj.keys()) {
					int rIdx = requiredParams.indexOf(key);
					if (rIdx != -1) {
						requiredParams.erase(requiredParams.begin()+rIdx);
					} else {
						int oIdx = optionalParams.indexOf(key);
						if (oIdx != -1) {
							optionalParams.erase(optionalParams.begin()+oIdx);
						} else {
							_unexpectedParams << key;
						}
					}
				}
				_missingParams << requiredParams;
			}
		}
		/**********************************************************************/

		/**********************************************************************/
		/* FrequencyRange Object (Table 16)                                   */
		/**********************************************************************/
		for(auto inquiredFrequencyRangeVal : inquiredFrequencyRangeArray) {
			auto inquiredFrequencyRangeObj = inquiredFrequencyRangeVal.toObject();
			requiredParams = QStringList() << "lowFrequency" << "highFrequency";
			optionalParams = QStringList();
			for(auto key : inquiredFrequencyRangeObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin()+rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin()+oIdx);
					} else {
						_unexpectedParams << key;
					}
				}
			}
			_missingParams << requiredParams;
		}
		/**********************************************************************/

		/**********************************************************************/
		/* Channels Object (Table 17)                                         */
		/**********************************************************************/
		for(auto inquiredChannelsVal : inquiredChannelsArray) {
			auto inquiredChannelsObj = inquiredChannelsVal.toObject();
			requiredParams = QStringList() << "globalOperatingClass";
			optionalParams = QStringList() << "channelCfi";
			for(auto key : inquiredChannelsObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin()+rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin()+oIdx);
					} else {
						_unexpectedParams << key;
					}
				}
			}
			_missingParams << requiredParams;
		}
		/**********************************************************************/

		if (_missingParams.size()) {
			_responseCode = CConst::missingParamResponseCode;
			return;
		} else if (_unexpectedParams.size()) {
			_responseCode = CConst::unexpectedParamResponseCode;
			return;
		}

		/**********************************************************************/
		/* Extract values                                                     */
		/**********************************************************************/
		_requestId = requestObj["requestId"].toString();
		_serialNumber = deviceDescriptorObj["serialNumber"].toString();

		if (rulesetIdsArray.size() == 0) {
			_responseCode = CConst::invalidValueResponseCode;
			_invalidParams << "rulesetIds";
			return;
		}
		_rulesetId = rulesetIdsArray.at(0).toString();

		if (hasIndoorDeploymentFlag) {
			int indoorDeploymentVal = locationObj["indoorDeployment"].toInt();
			switch(indoorDeploymentVal) {
				case 0:
				case 2:
					_rlanType = RLANType::RLAN_OUTDOOR;
					break;
				case 1:
					_rlanType = RLANType::RLAN_INDOOR;
					break;
				default:
					_invalidParams << "indoorDeployment";
			}
		} else {
			_rlanType = RLANType::RLAN_OUTDOOR;
		}

		QString rlanHeightType = elevationObj["heightType"].toString();

		if (rlanHeightType == "AMSL") {
			_rlanHeightType = CConst::AMSLHeightType;
		} else if (rlanHeightType == "AGL") {
			_rlanHeightType = CConst::AGLHeightType;
		} else {
			_invalidParams << "heightType";
		}

		double verticalUncertainty = elevationObj["verticalUncertainty"].toDouble();
		if (verticalUncertainty < 0.0) {
			_invalidParams << "verticalUncertainty";
		}
		double centerHeight = elevationObj["height"].toDouble();

		if (hasEllipseFlag) {
			_rlanUncertaintyRegionType = RLANBoundary::ELLIPSE;
			double centerLatitude = centerObj["latitude"].toDouble();
			double centerLongitude = centerObj["longitude"].toDouble();

			double minorAxis = ellipseObj["minorAxis"].toDouble();
			double majorAxis = ellipseObj["majorAxis"].toDouble();

			double orientation = ellipseObj["orientation"].toDouble();

			if ( (centerLatitude < -90.0) || (centerLatitude > 90.0) ) {
				_invalidParams << "latitude";
			}
			if ( (centerLongitude < -180.0) || (centerLongitude > 180.0) ) {
				_invalidParams << "longitude";
			}

			if (majorAxis < minorAxis) {
				_invalidParams << "minorAxis";
				_invalidParams << "majorAxis";
			} else {
				if (minorAxis < 0.0) {
					_invalidParams << "minorAxis";
				}
				if (majorAxis < 0.0) {
					_invalidParams << "majorAxis";
				}
			}

			if ( (orientation < 0.0) || (orientation > 180.0) ) {
				_invalidParams << "orientation";
			}

			_rlanLLA = std::make_tuple(centerLatitude, centerLongitude, centerHeight);
			_rlanUncerts_m = std::make_tuple(minorAxis, majorAxis, verticalUncertainty);
			_rlanOrientation_deg = orientation;
		} else if (hasLinearPolygonFlag) {
			_rlanUncertaintyRegionType = RLANBoundary::LINEAR_POLY;

			if ((outerBoundaryPointArray.size() < 3) || (outerBoundaryPointArray.size() > 15)) {
				_invalidParams << "outerBoundary";
			}

			for(auto outerBoundaryPointVal : outerBoundaryPointArray) {
				auto outerBoundaryPointObj = outerBoundaryPointVal.toObject();
				double latitude = outerBoundaryPointObj["latitude"].toDouble();
				double longitude = outerBoundaryPointObj["longitude"].toDouble();

				if ( (latitude < -90.0) || (latitude > 90.0) ) {
					_invalidParams << "latitude";
				}
				if ( (longitude < -180.0) || (longitude > 180.0) ) {
					_invalidParams << "longitude";
				}

				_rlanLinearPolygon.push_back(std::make_pair(latitude, longitude));
			}

			double centerLongitude;
			double centerLatitude;

			// Average LON/LAT of vertices
			double sumLon = 0.0;
			double sumLat = 0.0;
			int i;
			for(i=0; i< (int) _rlanLinearPolygon.size(); i++) {
				sumLon += _rlanLinearPolygon[i].second;
				sumLat += _rlanLinearPolygon[i].first;
			}
			centerLongitude = sumLon / _rlanLinearPolygon.size();
			centerLatitude  = sumLat / _rlanLinearPolygon.size();

			_rlanLLA = std::make_tuple(centerLatitude, centerLongitude, centerHeight);
			_rlanUncerts_m = std::make_tuple(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), verticalUncertainty);
		} else if (hasRadialPolygonFlag) {
			_rlanUncertaintyRegionType = RLANBoundary::RADIAL_POLY;

			if ((outerBoundaryVectorArray.size() < 3) || (outerBoundaryVectorArray.size() > 15)) {
				_invalidParams << "outerBoundary";
			}

			for(auto outerBoundaryVectorVal : outerBoundaryVectorArray) {
				auto outerBoundaryVectorObj = outerBoundaryVectorVal.toObject();
				double angle = outerBoundaryVectorObj["angle"].toDouble();
				double length = outerBoundaryVectorObj["length"].toDouble();

				if (length < 0.0) {
					_invalidParams << "length";
				}
				if ( (angle < 0.0) || (angle > 360.0) ) {
					_invalidParams << "angle";
				}

				_rlanRadialPolygon.push_back(std::make_pair(angle, length));
			}

			double centerLatitude = centerObj["latitude"].toDouble();
			double centerLongitude = centerObj["longitude"].toDouble();

			_rlanLLA = std::make_tuple(centerLatitude, centerLongitude, centerHeight);
			_rlanUncerts_m = std::make_tuple(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), verticalUncertainty);
		}

		for(auto inquiredChannelsVal : inquiredChannelsArray) {
			auto inquiredChannelsObj = inquiredChannelsVal.toObject();
			auto chanClass = std::make_pair<int, std::vector<int>>(inquiredChannelsObj["globalOperatingClass"].toInt(), std::vector<int>());
			if (inquiredChannelsObj.contains("channelCfi")) {
				for (const QJsonValue& chanIdx : inquiredChannelsObj["channelCfi"].toArray())
					chanClass.second.push_back(chanIdx.toInt());
			}
			LOGGER_INFO(logger)
				<< (chanClass.second.empty() ? "ALL" : std::to_string(chanClass.second.size()))
				<< " channels requested in operating class "
				<< chanClass.first;
			// TODO: are we handling the case where they want all?

			_inquiredChannels.push_back(chanClass);
		}
		LOGGER_INFO(logger) << inquiredChannelsArray.size() << " operating class(es) requested";

		for(auto inquiredFrequencyRangeVal : inquiredFrequencyRangeArray) {
			auto inquiredFrequencyRangeObj = inquiredFrequencyRangeVal.toObject();
			_inquiredFrquencyRangesMHz.push_back(std::make_pair(
						inquiredFrequencyRangeObj["lowFrequency"].toInt(),
						inquiredFrequencyRangeObj["highFrequency"].toInt()));
		}
		LOGGER_INFO(logger) << inquiredFrequencyRangeArray.size() << " frequency range(s) requested";

		if (inquiredChannelsArray.size() + inquiredFrequencyRangeArray.size() == 0) {
			LOGGER_WARN(logger) << "GENERAL FAILURE: must specify either inquiredChannels or inquiredFrequencies";
			_responseCode = CConst::generalFailureResponseCode;
			return;
		}

		if (_invalidParams.size()) {
			_responseCode = CConst::invalidValueResponseCode;
		}
#if DEBUG_AFC
	} else if (_analysisType == "test_winner2") {
		// Do nothing
#endif
	} else {
		throw std::runtime_error(QString("Invalid analysis type for version 1.1: %1").arg(QString::fromStdString(_analysisType)).toStdString());
	}

	return;
}

void AfcManager::importGUIjsonVersion1_0(const QJsonObject &jsonObj)
{

	if (_analysisType == "AP-AFC")
	{
		if (jsonObj.contains("requestId") && !jsonObj["ellipse"].isUndefined())
			_requestId = jsonObj["requestId"].toString();
		_deviceDesc = jsonObj["deviceDescriptor"].toObject();
		_serialNumber = _deviceDesc["serialNumber"].toString();

		QJsonObject location = jsonObj["location"].toObject();
		_rlanHeightType = CConst::AGLHeightType;
		if (location["indoorDeployment"].toInt() ==1){
			_rlanType =  RLANType::RLAN_INDOOR;
		}
		else {
			_rlanType = RLANType::RLAN_OUTDOOR;
		}

		if (location.contains("ellipse") && !location["ellipse"].isUndefined())
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
		else if (location.contains("linearPolygon") && !location["linearPolygon"].isUndefined())
		{
			_rlanUncertaintyRegionType = RLANBoundary::LINEAR_POLY;
			QJsonArray boundary = location["linearPolygon"].toObject()["outerBoundary"].toArray();
			for (QJsonValue vertexVal : boundary)
			{
				QJsonObject vertex = vertexVal.toObject();
				_rlanLinearPolygon.push_back(std::make_pair(vertex["latitude"].toDouble(), vertex["longitude"].toDouble()));
			}

			double centerLongitude;
			double centerLatitude;
#if 1
			// Average LON/LAT of vertices
			double sumLon = 0.0;
			double sumLat = 0.0;
			int i;
			for(i=0; i< (int) _rlanLinearPolygon.size(); i++) {
				sumLon += _rlanLinearPolygon[i].second;
				sumLat += _rlanLinearPolygon[i].first;
			}
			centerLongitude = sumLon / _rlanLinearPolygon.size();
			centerLatitude  = sumLat / _rlanLinearPolygon.size();
#else
			// Center of mass calculation
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
#endif

			_rlanLLA = std::make_tuple(centerLatitude,
					centerLongitude,
					location["height"].toDouble());
			_rlanUncerts_m = std::make_tuple(std::numeric_limits<double>::quiet_NaN(),
					std::numeric_limits<double>::quiet_NaN(),
					location["verticalUncertainty"].toDouble());
		}
		else if (location.contains("radialPolygon") && !location["radialPolygon"].isUndefined())
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

		if (jsonObj.contains("minDesiredPower") && !jsonObj["minDesiredPower"].isUndefined()) {
			_minEIRP_dBm = jsonObj["minDesiredPower"].toDouble();
		} else {
			_minEIRP_dBm = std::numeric_limits<double>::quiet_NaN();
		}

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
		if (containsChannel(_allowableFreqBandList, channel.startFreqMHz, channel.stopFreqMHz)) {
			_channelList.push_back(channel);
		}

		_exclusionZoneRLANChanIdx = 0;//round((centerFreq - _wlanMinFreq) / _exclusionZoneRLANBWHz - 0.5);
		_exclusionZoneRLANEIRPDBm = jsonObj["EIRP"].toDouble();

		QJsonObject antenna = jsonObj["antenna"].toObject();
		QString rlanHeightType = antenna["heightType"].toString();
		if (rlanHeightType == "AMSL") {
			_rlanHeightType = CConst::AMSLHeightType;
		} else if (rlanHeightType == "AGL") {
			_rlanHeightType = CConst::AGLHeightType;
		} else {
			throw std::runtime_error(QString("Invalid height type: %1").arg(rlanHeightType).toStdString());
		}

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
		if (containsChannel(_allowableFreqBandList, channel.startFreqMHz, channel.stopFreqMHz)) {
			_channelList.push_back(channel);
		}

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

	return;
}

// Support command line interface with AFC Engine
void AfcManager::setCmdLineParams(std::string &inputFilePath, std::string &configFilePath, std::string &outputFilePath, std::string &tempDir, std::string &logLevel, int argc, char **argv)
{
	// Declare the supported options
	po::options_description optDescript{"Allowed options"};
	// Create command line options
	optDescript.add_options()
		("help,h", "Use input-file-path, config-file-path, or output-file-path.")
		("request-type,r", po::value<std::string>()->default_value("AP-AFC"), "set request-type (AP-AFC, HeatmapAnalysis, ExclusionZoneAnalysis)")
		("state-root,s", po::value<std::string>()->default_value("/var/lib/fbrat"), "set fbrat state root directory")
		("input-file-path,i", po::value<std::string>()->default_value("inputFile.json"), "set input-file-path level")
		("config-file-path,c", po::value<std::string>()->default_value("configFile.json"), "set config-file-path level")
		("output-file-path,o", po::value<std::string>()->default_value("outputFile.json"), "set output-file-path level")
		("progress-file-path,p", po::value<std::string>()->default_value("progress.txt"), "set progress file path")
		("temp-dir,t", po::value<std::string>()->default_value(""), "set temp-dir level")
		("log-level,l", po::value<std::string>()->default_value("debug"), "set log-level")
		("runtime_opt,u", po::value<uint32_t>()->default_value(3), "bit 0: create debug files; bit 1: create kmz and progress files; bit 2: interpret file pathes as URLs");

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
	if (cmdLineArgs.count("runtime_opt"))
	{
		uint32_t tmp = cmdLineArgs["runtime_opt"].as<uint32_t>();
		if (tmp & RUNTIME_OPT_ENABLE_DBG) {
			AfcManager::_createDebugFiles = true;
		}
		if (tmp & RUNTIME_OPT_ENABLE_GUI) {
			AfcManager::_createKmz = true;
		}
		AfcManager::_dataIf = new AfcDataIf(tmp & RUNTIME_OPT_URL);
	}
	if (cmdLineArgs.count("progress-file-path"))
	{
		if (AfcManager::_createKmz) {
			AfcManager::_progressFile =
				cmdLineArgs["progress-file-path"].as<std::string>();
		}
	}
	else
	{
		throw std::runtime_error("AfcManager::setCmdLineParams(): progress-file-path(p) command line argument was not set.");
	}

}

void AfcManager::importConfigAFCjson(const std::string &inputJSONpath, const std::string &tempDir)
{
	QString errMsg;

	if (_responseCode != CConst::successResponseCode) {
		return;
	}

	// read json file in
	QJsonDocument jsonDoc;
	QString fName = QString(inputJSONpath.c_str());
	QByteArray data;
	if (AfcManager::_dataIf->readFile(fName, data)) {
		jsonDoc = QJsonDocument::fromJson(data);
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): Failed to open JSON file specifying configuration parameters.");
	}

	// Print raw contents of input JSON file
	LOGGER_DEBUG(logger) << "Raw contents of input JSON file provided by the GUI: " << jsonDoc.toJson().toStdString();

	// Read JSON from file
	QJsonObject jsonObj = jsonDoc.object();

	/**********************************************************************/
	/* Create _allowableFreqBandList                                      */
	/**********************************************************************/
	QJsonArray freqBandArray = jsonObj["freqBands"].toArray();
	for (QJsonValue freqBandVal : freqBandArray) {
		QJsonObject freqBandObj = freqBandVal.toObject();
		std::string name = freqBandObj["name"].toString().toStdString();
		int startFreqMHz = freqBandObj["startFreqMHz"].toInt();
		int stopFreqMHz = freqBandObj["stopFreqMHz"].toInt();

		if (stopFreqMHz <= startFreqMHz) {
			errMsg = QString("ERROR: Freq Band %1 Invalid, startFreqMHz = %2, stopFreqMHz = %3.  Require startFreqMHz < stopFreqMHz")
					.arg(QString::fromStdString(name)).arg(startFreqMHz).arg(stopFreqMHz);
			throw std::runtime_error(errMsg.toStdString());
		} else if ((stopFreqMHz <= _wlanMinFreqMHz) || (startFreqMHz >= _wlanMaxFreqMHz)) {
			errMsg = QString("ERROR: Freq Band %1 Invalid, startFreqMHz = %2, stopFreqMHz = %3.  Has no overlap with band [%4,%5].")
					.arg(QString::fromStdString(name)).arg(startFreqMHz).arg(stopFreqMHz).arg(_wlanMinFreqMHz).arg(_wlanMaxFreqMHz);
			throw std::runtime_error(errMsg.toStdString());
		}

		_allowableFreqBandList.push_back(FreqBandClass(name, startFreqMHz, stopFreqMHz));
	}
	if (!_allowableFreqBandList.size()) {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): ERROR: no allowable frequency bands defined.");
	}
	/**********************************************************************/

	// These are created for ease of copying
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

	if (jsonObj.contains("nlcdFile") && !jsonObj["nlcdFile"].isUndefined()) {
		_nlcdFile = SearchPaths::forReading("data", QString("fbrat/rat_transfer/nlcd/") + jsonObj["nlcdFile"].toString(), true).toStdString();
	} else {
		_nlcdFile = SearchPaths::forReading("data", "fbrat/rat_transfer/nlcd/nlcd_2019_land_cover_l48_20210604_resample.tif", true).toStdString();
	}

	if (jsonObj.contains("fsAnalysisListFile") && !jsonObj["fsAnalysisListFile"].isUndefined()) {
		_fsAnalysisListFile = QDir(QString::fromStdString(tempDir)).filePath(jsonObj["fsAnalysisListFile"].toString()).toStdString();
	} else {
		_fsAnalysisListFile = QDir(QString::fromStdString(tempDir)).filePath("fs_analysis_list.csv").toStdString();
	}

	// ***********************************
	// If this flag is set, indoor rlan's have a fixed AMSL height over the uncertainty region (with no height uncertainty).
	// By default, this flag is false in which case the AGL height is fixed over the uncertainty region (with no height uncertainty).
	// ***********************************
	if (jsonObj.contains("removeMobile") && !jsonObj["removeMobile"].isUndefined()) {
		_indoorFixedHeightAMSL = jsonObj["indoorFixedHeightAMSL"].toBool();
	} else {
		_indoorFixedHeightAMSL = false;
	}

	// ***********************************
	//
	// ***********************************
	if (jsonObj.contains("scanPointBelowGroundMethod") && !jsonObj["scanPointBelowGroundMethod"].isUndefined()) {
		std::string scanPointBelowGroundMethodStr = jsonObj["scanPointBelowGroundMethod"].toString().toStdString();
		if (scanPointBelowGroundMethodStr == "discard") {
			_scanPointBelowGroundMethod = CConst::DiscardScanPointBelowGroundMethod;
		} else if (scanPointBelowGroundMethodStr == "truncate") {
			_scanPointBelowGroundMethod = CConst::TruncateScanPointBelowGroundMethod;
		} else {
			throw std::runtime_error("AfcManager::importConfigAFCjson(): Invalid scanPointBelowGroundMethod specified.");
		}
	} else {
		_scanPointBelowGroundMethod = CConst::TruncateScanPointBelowGroundMethod;
	}

	// ITM Parameters
	_itmEpsDielect =  jsonObj["ITMParameters"].toObject()["dielectricConst"].toDouble();
	_itmSgmConductivity = jsonObj["ITMParameters"].toObject()["conductivity"].toDouble();
	_itmPolarization = jsonObj["ITMParameters"].toObject()["polarization"].toString().toStdString() == "Vertical";

	// ***********************************
	// More ITM parameters
	// ***********************************
	_itmMinSpacing = jsonObj["ITMParameters"].toObject()["minSpacing"].toDouble();
	_itmMaxNumPts = jsonObj["ITMParameters"].toObject()["maxPoints"].toInt();
	// ***********************************

	QJsonObject uncertaintyObj = jsonObj["APUncertainty"].toObject();

	if (uncertaintyObj.contains("scanMethod") && !uncertaintyObj["scanMethod"].isUndefined()) {
		std::string scanMethodStr = uncertaintyObj["scanMethod"].toString().toStdString();
		if (scanMethodStr == "xyAlignRegionNorthEast") {
			_scanRegionMethod = CConst::xyAlignRegionNorthEastScanRegionMethod;
		} else if (scanMethodStr == "xyAlignRegionMajorMinor") {
			_scanRegionMethod = CConst::xyAlignRegionMajorMinorScanRegionMethod;
		} else if (scanMethodStr == "latLonAlignGrid") {
			_scanRegionMethod = CConst::latLonAlignGridScanRegionMethod;
		} else {
			throw std::runtime_error("AfcManager::importConfigAFCjson(): Invalid scanPointBelowGroundMethod specified.");
		}
	} else {
		_scanRegionMethod = CConst::latLonAlignGridScanRegionMethod;
	}

	// AP uncertainty scanning resolution
	_scanres_xy = -1.0;
	_scanres_points_per_degree = -1;
	switch(_scanRegionMethod) {
		case CConst::xyAlignRegionNorthEastScanRegionMethod:
		case CConst::xyAlignRegionMajorMinorScanRegionMethod:
			if (uncertaintyObj.contains("horizontal") && !uncertaintyObj["horizontal"].isUndefined()) {
				_scanres_xy = uncertaintyObj["horizontal"].toDouble();
			} else {
				_scanres_xy = 30.0;
			}
			break;
		case CConst::latLonAlignGridScanRegionMethod:
			if (uncertaintyObj.contains("points_per_degree") && !uncertaintyObj["points_per_degree"].isUndefined()) {
				_scanres_points_per_degree = uncertaintyObj["points_per_degree"].toInt();
			} else {
				_scanres_points_per_degree = 3600;  // Defalut 1 arcsec
			}
			break;
		default:
			CORE_DUMP;
			break;
	}

	if (uncertaintyObj.contains("height") && !uncertaintyObj["height"].isUndefined()) {
		_scanres_ht = uncertaintyObj["height"].toDouble();
	} else {
		_scanres_ht = 5.0;
	}

	if (jsonObj.contains("ulsDatabaseList") && !jsonObj["ulsDatabaseList"].isUndefined()) {
		QJsonArray ulsDatabaseArray = jsonObj["ulsDatabaseList"].toArray();
		for (QJsonValue ulsDatabaseVal : ulsDatabaseArray) {
			QJsonObject ulsDatabaseObj = ulsDatabaseVal.toObject();
			std::string name = ulsDatabaseObj["name"].toString().toStdString();
			std::string dbfile = _stateRoot + "/ULS_Database/" + ulsDatabaseObj["ulsDatabase"].toString().toStdString();
			_ulsDatabaseList.push_back(std::make_tuple(name, dbfile));
		}
	} else {
		std::string dbfile = _stateRoot + "/ULS_Database/" + jsonObj["ulsDatabase"].toString().toStdString();
		_ulsDatabaseList.push_back(std::make_tuple("CONUS", dbfile));
	}

	_rasDataFile = _stateRoot + "/RAS_Database/" + jsonObj["rasDatabase"].toString().toStdString();

	double cfgMinEIRP;
	if (jsonObj.contains("minEIRP") && !jsonObj["minEIRP"].isUndefined()) {
		cfgMinEIRP = jsonObj["minEIRP"].toDouble();
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): minEIRP is missing.");
	}

	if (std::isnan(_minEIRP_dBm)) {
		_minEIRP_dBm = cfgMinEIRP;
	} else if (cfgMinEIRP > _minEIRP_dBm) {
		_minEIRP_dBm = cfgMinEIRP;
	}
	_maxEIRP_dBm = jsonObj["maxEIRP"].toDouble();

	if (jsonObj.contains("minPSD") && !jsonObj["minPSD"].isUndefined()) {
		_minPSD_dBmPerMHz = jsonObj["minPSD"].toDouble();
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): minPSD is missing.");
	}

	_IoverN_threshold_dB = jsonObj["threshold"].toDouble();
	_maxRadius = jsonObj["maxLinkDistance"].toDouble() * 1000.0; // convert from km to m
	_bodyLossIndoorDB = jsonObj["bodyLoss"].toObject()["valueIndoor"].toDouble();
	_bodyLossOutdoorDB = jsonObj["bodyLoss"].toObject()["valueOutdoor"].toDouble();
	_polarizationLossDB = jsonObj["polarizationMismatchLoss"].toObject()["value"].toDouble();
	_buildingLossModel = (buildingLoss["kind"]).toString();
	_propagationEnviro = jsonObj["propagationEnv"].toString();

	// ***********************************
	// Feeder loss parameters
	// ***********************************
	_rxFeederLossDBUNII5 = jsonObj["receiverFeederLoss"].toObject()["UNII5"].toDouble();
	_rxFeederLossDBUNII7 = jsonObj["receiverFeederLoss"].toObject()["UNII7"].toDouble();
	_rxFeederLossDBOther = jsonObj["receiverFeederLoss"].toObject()["other"].toDouble();
	// ***********************************

	// ***********************************
	// Noise Figure parameters
	// ***********************************
	auto ulsRecieverNoise = jsonObj["fsReceiverNoise"].toObject();

	_ulsNoiseFigureDBUNII5 = noiseFloorToNoiseFigure(ulsRecieverNoise["UNII5"].toDouble());
	_ulsNoiseFigureDBUNII7 = noiseFloorToNoiseFigure(ulsRecieverNoise["UNII7"].toDouble());
	_ulsNoiseFigureDBOther = noiseFloorToNoiseFigure(ulsRecieverNoise["other"].toDouble());
	// ***********************************

	// ***********************************
	// apply clutter bool
	// ***********************************
	if (jsonObj.contains("clutterAtFS") && !jsonObj["clutterAtFS"].isUndefined()) {
		_applyClutterFSRxFlag = jsonObj["clutterAtFS"].toBool();
	} else {
		_applyClutterFSRxFlag = false;
	}

	if (jsonObj.contains("fsClutterModel") && !jsonObj["fsClutterModel"].isUndefined()) {
		QJsonObject fsClutterModel = jsonObj["fsClutterModel"].toObject();
		if (fsClutterModel.contains("p2108Confidence") && !fsClutterModel["p2108Confidence"].isUndefined()) {
			_fsConfidenceClutter2108 = fsClutterModel["p2108Confidence"].toDouble()/100.0;
		} else {
			throw std::runtime_error("AfcManager::importConfigAFCjson(): fsClutterModel[p2108Confidence] missing.");
		}
		if (fsClutterModel.contains("maxFsAglHeight") && !fsClutterModel["maxFsAglHeight"].isUndefined()) {
			_maxFsAglHeight = fsClutterModel["maxFsAglHeight"].toDouble();
		} else {
			throw std::runtime_error("AfcManager::importConfigAFCjson(): fsClutterModel[maxFsAglHeight] missing.");
		}
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): fsClutterModel missing.");
	}

	int validFlag;
	if (jsonObj.contains("rlanITMTxClutterMethod") && !jsonObj["rlanITMTxClutterMethod"].isUndefined()) {
		_rlanITMTxClutterMethod = (CConst::ITMClutterMethodEnum) CConst::strITMClutterMethodList->str_to_type(jsonObj["rlanITMTxClutterMethod"].toString().toStdString(), validFlag, 0);
		if (!validFlag) {
			throw std::runtime_error("ERROR: Invalid rlanITMTxClutterMethod");
		}
	} else {
		_rlanITMTxClutterMethod = CConst::ForceTrueITMClutterMethod;
	}
	// ***********************************

	QString antennaPattern = jsonObj["antennaPattern"].toObject()["kind"].toString();

	if (antennaPattern == "User Upload") {
		//_ulsAntennaPatternFile = SearchPaths::forReading("data", "fbrat/AntennaPatterns/" + jsonObj["antennaPattern"].toObject()["value"].toString(), true).toStdString();
		_ulsAntennaPatternFile = _stateRoot + "/AntennaPatterns/" + jsonObj["antennaPattern"].toObject()["value"].toString().toStdString();
		LOGGER_INFO(logger) << "Antenna pattern file set to: " << _ulsAntennaPatternFile;
	} else {
		_ulsAntennaPatternFile = "";
	}

	if (jsonObj.contains("ulsDefaultAntennaType") && !jsonObj["ulsDefaultAntennaType"].isUndefined()) {
		_ulsDefaultAntennaType = (CConst::ULSAntennaTypeEnum) CConst::strULSAntennaTypeList->str_to_type(jsonObj["ulsDefaultAntennaType"].toString().toStdString(), validFlag, 0);
		if (!validFlag) {
			throw std::runtime_error("ERROR: Invalid ulsDefaultAntennaType");
		}
	}

	// ***********************************
	// As of Feb-2022, it was agreed to include all possible Mobile links (e.g. those marked as Mobile or TP radio service) in UNII-5 and UNII-7 for AFC analysis.
	// ***********************************
	if (jsonObj.contains("removeMobile") && !jsonObj["removeMobile"].isUndefined()) {
		_removeMobile = jsonObj["removeMobile"].toBool();
	} else {
		_removeMobile = false;
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

	/**************************************************************************************/
	/* Path Loss Model Parmeters                                                       ****/
	/**************************************************************************************/
	_pathLossClampFSPL = false;

	// Set pathLossModel
	_pathLossModel = (CConst::PathLossModelEnum)CConst::strPathLossModelList->str_to_type(propModel["kind"].toString().toStdString(), validFlag, 0);
	if (!validFlag) {
		throw std::runtime_error("ERROR: Invalid propagationModel[\"kind\"]");
	}

	if (_pathLossModel == CConst::CustomPathLossModel) {
		_pathLossModel = CConst::FCC6GHzReportAndOrderPathLossModel;
	}

	switch(_pathLossModel) {
		case CConst::unknownPathLossModel:
			throw std::runtime_error("ERROR: Invalid propagationModel[\"kind\"]");
			break;
		case CConst::FSPLPathLossModel:
			// FSPL
			break;
		case CConst::ITMBldgPathLossModel:
		// ITM model using Building data as terrain
			// GUI gets these values as percentiles from 0-100, convert to probabilities 0-1
			_winner2ProbLOSThr = propModel["win2ProbLosThreshold"].toDouble()/100.0;
			_confidenceITM = propModel["itmConfidence"].toDouble()/100.0;
			_confidenceClutter2108 = propModel["p2108Confidence"].toDouble()/100.0;
			_useBDesignFlag = propModel["buildingSource"].toString() == "B-Design3D";
			_useLiDAR = propModel["buildingSource"].toString() == "LiDAR";
			_use3DEP = true;
			_winner2LOSOption = CConst::UnknownLOSOption;
			_winner2UnknownLOSMethod = CConst::PLOSThresholdWinner2UnknownLOSMethod;
			break;
		case CConst::CoalitionOpt6PathLossModel:
			// No buildings, Winner II, ITM, P.456 Clutter
			// GUI gets these values as percentiles from 0-100, convert to probabilities 0-1
			_winner2ProbLOSThr = propModel["win2ProbLosThreshold"].toDouble()/100.0;
			_confidenceITM = propModel["itmConfidence"].toDouble()/100.0;
			_confidenceClutter2108 = propModel["p2108Confidence"].toDouble()/100.0;

			_use3DEP = propModel["terrainSource"].toString() == "3DEP (30m)";

			_winner2LOSOption = CConst::UnknownLOSOption;
			_winner2UnknownLOSMethod = CConst::PLOSThresholdWinner2UnknownLOSMethod;
			break;
		case CConst::FCC6GHzReportAndOrderPathLossModel:
			// FCC_6GHZ_REPORT_AND_ORDER path loss model
			// GUI gets these values as percentiles from 0-100, convert to probabilities 0-1
			_winner2ProbLOSThr = std::numeric_limits<double>::quiet_NaN();
			_confidenceITM = propModel["itmConfidence"].toDouble()/100.0;
			_confidenceClutter2108 = propModel["p2108Confidence"].toDouble()/100.0;

			_useBDesignFlag = propModel["buildingSource"].toString() == "B-Design3D";
			_useLiDAR = propModel["buildingSource"].toString() == "LiDAR";
			// for now, we always use 3DEP for FCC 6Ghz
			_use3DEP = true;//propModel["terrainSource"].toString() == "3DEP (30m)";

			_winner2LOSOption = ((_useBDesignFlag || _useLiDAR) ? CConst::BldgDataReqTxRxLOSOption : CConst::UnknownLOSOption);
			_winner2UnknownLOSMethod = CConst::PLOSCombineWinner2UnknownLOSMethod;

			_pathLossClampFSPL = true;
			break;
		default:
			CORE_DUMP;
			break;
	}


	if (propModel.contains("win2Confidence") && !propModel["win2Confidence"].isUndefined()) {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): Outdated afc_config, win2Confidence not supported parameter.");
	}

	if (propModel.contains("win2ConfidenceCombined") && !propModel["win2ConfidenceCombined"].isUndefined()) {
		_confidenceWinner2Combined = propModel["win2ConfidenceCombined"].toDouble()/100.0;
	} else {
		_confidenceWinner2Combined = 0.5;
	}

	if (propModel.contains("win2ConfidenceLOS") && !propModel["win2ConfidenceLOS"].isUndefined()) {
		_confidenceWinner2LOS = propModel["win2ConfidenceLOS"].toDouble()/100.0;
	} else {
		_confidenceWinner2LOS = 0.5;
	}

	if (propModel.contains("win2ConfidenceNLOS") && !propModel["win2ConfidenceNLOS"].isUndefined()) {
		_confidenceWinner2NLOS = propModel["win2ConfidenceNLOS"].toDouble()/100.0;
	} else {
		_confidenceWinner2NLOS = 0.5;
	}

	if (propModel.contains("win2UseGroundDistance") && !propModel["win2UseGroundDistance"].isUndefined()) {
		_winner2UseGroundDistanceFlag = propModel["win2UseGroundDistance"].toBool();
	} else {
		_winner2UseGroundDistanceFlag = true;
	}

	// Whether or not to force LOS when mobile height above closeInHgtLOS for close in model",
	// RLAN height above which prob of LOS = 100% for close in model",
	if (propModel.contains("winner2HgtFlag") && !propModel["winner2HgtFlag"].isUndefined()) {
		_closeInHgtFlag = propModel["winner2HgtFlag"].toBool();
	} else {
		_closeInHgtFlag = true;
	}

	if (propModel.contains("winner2HgtLOS") && !propModel["winner2HgtLOS"].isUndefined()) {
		 _closeInHgtLOS = propModel["winner2HgtLOS"].toDouble();
	} else {
		 _closeInHgtLOS = 0.5;
	}

	if (propModel.contains("fsplUseGroundDistance") && !propModel["fsplUseGroundDistance"].isUndefined()) {
		_fsplUseGroundDistanceFlag = propModel["fsplUseGroundDistance"].toBool();
	} else {
		_fsplUseGroundDistanceFlag = false;
	}

	if (propModel.contains("itmReliability") && !propModel["itmReliability"].isUndefined()) {
		_reliabilityITM = propModel["itmReliability"].toDouble()/100.0;
	} else {
		_reliabilityITM = 0.5;
	}

	if (propModel.contains("winner2LOSOption") && !propModel["winner2LOSOption"].isUndefined()) {
		_winner2LOSOption = (CConst::LOSOptionEnum)CConst::strLOSOptionList->str_to_type(propModel["winner2LOSOption"].toString().toStdString(), validFlag, 1);
	}

	// ***********************************
	// If this flag is set, map data geojson is generated
	// ***********************************
	_mapDataGeoJsonFile = "";
	if (jsonObj.contains("enableMapInVirtualAp") && !jsonObj["enableMapInVirtualAp"].isUndefined() &&
		AfcManager::_createKmz) {
		if (jsonObj["enableMapInVirtualAp"].toBool()) {
		_mapDataGeoJsonFile = "mapData.json.gz";
		}
	}

	if (jsonObj.contains("channelResponseAlgorithm") && !jsonObj["channelResponseAlgorithm"].isUndefined()) {
		std::string strval = jsonObj["channelResponseAlgorithm"].toString().toStdString();
		_channelResponseAlgorithm = (CConst::SpectralAlgorithmEnum) CConst::strSpectralAlgorithmList->str_to_type(strval, validFlag, 1);
	} else {
		_channelResponseAlgorithm = CConst::pwrSpectralAlgorithm;
	}

	if (jsonObj.contains("visibilityThreshold") && !jsonObj["visibilityThreshold"].isUndefined()) {
		_visibilityThreshold = jsonObj["visibilityThreshold"].toDouble();
	} else {
		_visibilityThreshold = -10000.0;
	}

	if (jsonObj.contains("allowScanPtsInUncReg") && !jsonObj["allowScanPtsInUncReg"].isUndefined()) {
		_allowScanPtsInUncRegFlag = jsonObj["allowScanPtsInUncReg"].toBool();
	} else {
		_allowScanPtsInUncRegFlag = false;
	}

	// ***********************************
	// If this flag is set, FS RX antenna will have near field adjustment applied to antenna gain
	// ***********************************
	if (jsonObj.contains("nearFieldAdjFlag") && !jsonObj["nearFieldAdjFlag"].isUndefined()) {
		_nearFieldAdjFlag = jsonObj["nearFieldAdjFlag"].toBool();
	} else {
		_nearFieldAdjFlag = true;
	}

	if (_nearFieldAdjFlag) {
		_nfaTableFile = SearchPaths::forReading("data", "fbrat/rat_transfer/nfa/nfa_table_data.csv", true).toStdString();
	}
	if (_passiveRepeaterFlag) {
		_prTableFile = SearchPaths::forReading("data", "fbrat/rat_transfer/pr/WINNF-TS-1014-V1.2.0-App02.csv", true).toStdString();
	}
	// ***********************************

	// ***********************************
	// If this flag is set, compute passive repeaters, otherwise ignore passive repeaters
	// ***********************************
	if (jsonObj.contains("passiveRepeaterFlag") && !jsonObj["passiveRepeaterFlag"].isUndefined()) {
		_passiveRepeaterFlag = jsonObj["passiveRepeaterFlag"].toBool();
	} else {
		_passiveRepeaterFlag = true;
	}
	// ***********************************
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
	if (_pathLossModel == CConst::ITMBldgPathLossModel)
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
	for (auto freqRange : psdFreqRangeList) {
		for (int i = 0; i < (int) freqRange.psd_dBm_MHzList.size(); i++) {
			if ( !std::isnan(freqRange.psd_dBm_MHzList.at(i)) ) {
				spectrumInfos.append(QJsonObject
						{
						{ "frequencyRange", QJsonObject
						{
						{ "lowFrequency", freqRange.freqMHzList.at(i) },
						{ "highFrequency", freqRange.freqMHzList.at(i+1) }
						}
						},
						{ "maxPsd", freqRange.psd_dBm_MHzList.at(i) }
						});
			}
		}
	}

	QJsonArray channelInfos = QJsonArray();
	if (_responseCode == CConst::successResponseCode) {
		for (const auto &group : _inquiredChannels) {
			auto operatingClass = group.first;
			auto indexArray = QJsonArray();
			auto eirpArray = QJsonArray();

			for (const auto &chan : _channelList) {
				if (    (chan.type == ChannelType::INQUIRED_CHANNEL)
						&& (chan.operatingClass == operatingClass)
						&& (chan.availability != BLACK)
						&& (chan.availability != RED)
				) {
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
	}

	std::string shortDescription;

	switch(_responseCode) {
		case      CConst::generalFailureResponseCode: shortDescription = "General Failure";       break;
		case             CConst::successResponseCode: shortDescription = "Success";               break;
		case CConst::versionNotSupportedResponseCode: shortDescription = "Version Not Supported"; break;
		case    CConst::deviceDisallowedResponseCode: shortDescription = "Device Disallowed";     break;
		case        CConst::missingParamResponseCode: shortDescription = "Missing Param";         break;
		case        CConst::invalidValueResponseCode: shortDescription = "Invalid Value";         break;
		case     CConst::unexpectedParamResponseCode: shortDescription = "Unexpected Param";      break;
		case CConst::unsupportedSpectrumResponseCode: shortDescription = "Unsupported Spectrum";  break;

		default: CORE_DUMP; break;
	}

	bool hasSupplementalInfoFlag = false;
	if (_responseCode == CConst::missingParamResponseCode) {
		hasSupplementalInfoFlag = true;
	} else if (_responseCode == CConst::invalidValueResponseCode) {
		hasSupplementalInfoFlag = true;
	} else if (_responseCode == CConst::unexpectedParamResponseCode) {
		hasSupplementalInfoFlag = true;
	}

	QJsonObject responseObj;
	responseObj.insert("responseCode", _responseCode);
	responseObj.insert("shortDescription", shortDescription.c_str());

	if (hasSupplementalInfoFlag) {
		QJsonObject paramObj;
		QJsonArray paramsArray;

		if (_missingParams.size()) {
			paramsArray = QJsonArray();
			for(auto param : _missingParams) {
				paramsArray.append(param);
			}
			paramObj.insert("missingParams", paramsArray);
		}

		if (_invalidParams.size()) {
			paramsArray = QJsonArray();
			for(auto param : _invalidParams) {
				paramsArray.append(param);
			}
			paramObj.insert("invalidParams", paramsArray);
		}

		if (_unexpectedParams.size()) {
			paramsArray = QJsonArray();
			for(auto param : _unexpectedParams) {
				paramsArray.append(param);
			}
			paramObj.insert("unexpectedParams", paramsArray);
		}

		responseObj.insert("supplementalInfo", paramObj);
	}

	QJsonObject availableSpectrumInquiryResponse;
	availableSpectrumInquiryResponse.insert("requestId", _requestId);
	availableSpectrumInquiryResponse.insert("rulesetId", _rulesetId);
	if (channelInfos.size()) {
		availableSpectrumInquiryResponse.insert("availableChannelInfo", channelInfos);
	}
	if (spectrumInfos.size()) {
		availableSpectrumInquiryResponse.insert("availableFrequencyInfo", spectrumInfos);
	}
	if (_responseCode == CConst::successResponseCode) {
		availableSpectrumInquiryResponse.insert("availabilityExpireTime", ISO8601TimeUTC(1));
	}
	availableSpectrumInquiryResponse.insert("response", responseObj);

	QJsonObject responses {
		{ "version", _guiJsonVersion },
			{ "availableSpectrumInquiryResponses",
				QJsonArray
				{
					availableSpectrumInquiryResponse
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

	ULSClass *uls = findULSID(_exclusionZoneFSID, 0);
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

void AfcManager::exportGUIjson(const QString &exportJsonPath, const std::string& tempDir)
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

		if (!_mapDataGeoJsonFile.empty()) {
			generateMapDataGeoJson(tempDir);
		}
	}
	else if (_analysisType == "ExclusionZoneAnalysis")
	{
		outputDocument = generateExclusionZoneJson();
	}
	else if (_analysisType == "HeatmapAnalysis")
	{
		outputDocument = generateHeatmap();
	}
	else if (_analysisType == "ScanAnalysis")
	{
		outputDocument = QJsonDocument();
#if DEBUG_AFC
	} else if (_analysisType == "test_winner2") {
		// Do nothing
#endif
	}
	else
	{
		throw std::runtime_error(ErrStream() << "ERROR: Unrecognized analysis type = \"" << _analysisType << "\"");
	}

	// Write analysis outputs to JSON file
	QByteArray data = outputDocument.toJson();
	if (!AfcManager::_dataIf->gzipAndWriteFile(exportJsonPath, data)) {
		throw std::runtime_error("Error writing output file");
	}
	LOGGER_DEBUG(logger) << "Output file written to " << exportJsonPath.toStdString();
}

void AfcManager::generateMapDataGeoJson(const std::string& tempDir)
{
	QJsonDocument outputDocument;

	// Temporarily export layer so that we can re-import it as a geoJSON-formatted string
	const std::string gdalDriverName = "GeoJSON";
	OGRRegisterAll();

	OGRSFDriver* ptrDriver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(gdalDriverName.c_str());
	// This is the same as !(unique_ptr.get() != nullptr)
	if (ptrDriver == nullptr)
	{
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): " + gdalDriverName + " driver was not found");
	}

	QTemporaryDir geoTempDir;
	if (!geoTempDir.isValid())
	{
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Failed to create a temporary directory to store output of GeoJSON driver");
	}

	const QString tempOutFileName = "output.tmp";
	const QString tempOutFilePath = geoTempDir.filePath(tempOutFileName);

	std::unique_ptr<OGRDataSource> ptrOutputDS(ptrDriver->CreateDataSource(tempOutFilePath.toStdString().c_str(), NULL));
	if (ptrOutputDS == nullptr)
	{
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Could not create a data source at " + tempOutFilePath.toStdString());
	}

	OGRSpatialReference spatialRefWGS84; // Set the desired spatial reference (WGS84 in this case)
	spatialRefWGS84.SetWellKnownGeogCS("WGS84");
	OGRLayer *coneLayer = ptrOutputDS->CreateLayer("Temp_Output", &spatialRefWGS84, wkbPolygon, NULL);
	if (coneLayer == nullptr)
	{
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Could not create a layer in output data source");
	}

	OGRFieldDefn objKind("kind", OFTString);
	OGRFieldDefn dbNameField("DBNAME", OFTString);
	OGRFieldDefn fsidField("FSID", OFTInteger);
	OGRFieldDefn startFreq("startFreq", OFTReal);
	OGRFieldDefn stopFreq("stopFreq", OFTReal);
	objKind.SetWidth(64); /* fsLonField.SetWidth(64); fsLatField.SetWidth(64);*/
	fsidField.SetWidth(32); /* fsLonField.SetWidth(64); fsLatField.SetWidth(64);*/
	startFreq.SetWidth(32);
	stopFreq.SetWidth(32);

	if (coneLayer->CreateField(&objKind) != OGRERR_NONE)
	{
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Could not create 'kind' field in layer of the output data source");
	}
	if (coneLayer->CreateField(&dbNameField) != OGRERR_NONE)
	{
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Could not create 'DBNAME' field in layer of the output data source");
	}
	if (coneLayer->CreateField(&fsidField) != OGRERR_NONE)
	{
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Could not create 'FSID' field in layer of the output data source");
	}
	if (coneLayer->CreateField(&startFreq) != OGRERR_NONE)
	{
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Could not create 'startFreq' field in layer of the output data source");
	}
	if (coneLayer->CreateField(&stopFreq) != OGRERR_NONE)
	{
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Could not create 'stopFreq' field in layer of the output data source");
	}

	// Calculate the cones in iterative loop
	LatLon FSLatLonVal, posPointLatLon, negPointLatLon;
	for (const auto &ulsIdx : _ulsIdxList)
	{
		ULSClass *uls = (*_ulsList)[ulsIdx];

		// Instantiate cone object
		std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> coneFeature(OGRFeature::CreateFeature(coneLayer->GetLayerDefn()));

		// Grab FSID for storing with coverage polygon
		int FSID = uls->getID();

		std::string dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);

		// Compute the beam coordinates and store into DoublePairs
		std::tie(FSLatLonVal, posPointLatLon, negPointLatLon) = computeBeamConeLatLon(uls);

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
		coneFeature->SetField("DBNAME", dbName.c_str());
		coneFeature->SetField("kind", "FS");
		coneFeature->SetField("startFreq", uls->getStartAllocFreq() / 1.0e6);
		coneFeature->SetField("stopFreq", uls->getStopAllocFreq() / 1.0e6);
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
		{"geoJson", geoJsonObj}
	};

	outputDocument = QJsonDocument(analysisJsonObj);

	// Write map data GEOJSON file
	std::string fullPathMapDataFile = QDir(QString::fromStdString(tempDir)).filePath(QString::fromStdString(_mapDataGeoJsonFile)).toStdString();
	auto mapDataFile = FileHelpers::open(QString::fromStdString(fullPathMapDataFile), QIODevice::WriteOnly);
	auto gzip_writer = new GzipStream(mapDataFile.get());
	if (!gzip_writer->open(QIODevice::WriteOnly))
	{
		throw std::runtime_error("Gzip failed to open.");
	}
	gzip_writer->write(outputDocument.toJson());
	gzip_writer->close();
	LOGGER_DEBUG(logger) << "Output file written to " << mapDataFile->fileName().toStdString();
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
ULSClass *AfcManager::findULSID(int ulsID, int dbIdx)
{
	int ulsIdx, ulsIdxA, ulsIdxB;
	int id, db, idA, idB, dbIdxA, dbIdxB;
	ULSClass *uls = (ULSClass *) NULL;

	bool found = false;

	ulsIdxA = 0;
	idA = (*_ulsList)[ulsIdxA]->getID();
	dbIdxA = (*_ulsList)[ulsIdxA]->getDBIdx();
	if ( (idA == ulsID) && (dbIdxA == dbIdx) ) {
		found = true;
		uls = (*_ulsList)[ulsIdxA];
	} else if ((dbIdx < dbIdxA) || ((dbIdx == dbIdxA) && (ulsID < idA))) {
		throw std::runtime_error(ErrStream() << "ERROR: Invalid DBIDX = " << dbIdx << " FSID = " << ulsID);
	}

	ulsIdxB = _ulsList->getSize()-1;;
	idB = (*_ulsList)[ulsIdxB]->getID();
	dbIdxB = (*_ulsList)[ulsIdxB]->getDBIdx();
	if ( (idB == ulsID) && (dbIdxB == dbIdx) ) {
		found = true;
		uls = (*_ulsList)[ulsIdxB];
	} else if ( (dbIdx > dbIdxB) || ((dbIdx == dbIdxB) && (ulsID > idB)) ) {
		throw std::runtime_error(ErrStream() << "ERROR: Invalid DBIDX = " << dbIdx << " FSID = " << ulsID);
	}

	while((ulsIdxA + 1 < ulsIdxB) && (!found)) {
		ulsIdx = (ulsIdxA + ulsIdxB)/2;
		id = (*_ulsList)[ulsIdx]->getID();
		db = (*_ulsList)[ulsIdx]->getDBIdx();
		if ( (ulsID == id) && (dbIdx == db) ) {
			found = true;
			uls = (*_ulsList)[ulsIdx];
		} else if ( (dbIdx > db) || ((dbIdx == db) && (ulsID > id)) ) {
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
/**** inline function aciFn() used only in computeSpectralOverlapLoss                  ****/
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
/**** AfcManager::computeSpectralOverlapLoss                                           ****/
/******************************************************************************************/
bool AfcManager::computeSpectralOverlapLoss(double *spectralOverlapLossDBptr, double sigStartFreq, double sigStopFreq, double rxStartFreq, double rxStopFreq,
	bool aciFlag, CConst::SpectralAlgorithmEnum spectralAlgorithm) const
{
	bool hasOverlap;

	if (!aciFlag) {
		if ((sigStopFreq <= rxStartFreq) || (sigStartFreq >= rxStopFreq)) {
			hasOverlap = false;
			if (spectralOverlapLossDBptr) {
				*spectralOverlapLossDBptr = -std::numeric_limits<double>::infinity();
			}
		} else {
			hasOverlap = true;
			if (spectralOverlapLossDBptr) {
				double f1 = (sigStartFreq < rxStartFreq ? rxStartFreq : sigStartFreq);
				double f2 = (sigStopFreq > rxStopFreq ? rxStopFreq : sigStopFreq);
				double overlap;
				if (spectralAlgorithm == CConst::pwrSpectralAlgorithm) {
					overlap = (f2 - f1) / (sigStopFreq - sigStartFreq);
				} else {
					overlap = (rxStopFreq - rxStartFreq) / (sigStopFreq - sigStartFreq);
				}
				*spectralOverlapLossDBptr = -10.0 * log(overlap) / log(10.0);
			}
		}
	} else {
		if ((2*sigStopFreq-sigStartFreq <= rxStartFreq) || (2*sigStartFreq-sigStopFreq >= rxStopFreq)) {
			hasOverlap = false;
			if (spectralOverlapLossDBptr) {
				*spectralOverlapLossDBptr = -std::numeric_limits<double>::infinity();
			}
		} else {
			hasOverlap = true;
			if (spectralOverlapLossDBptr) {
				double BMHz = (sigStopFreq - sigStartFreq)*1.0e-6;
				double fStartMHz = (rxStartFreq - (sigStartFreq + sigStopFreq)/2)*1.0e-6;
				double fStopMHz  = (rxStopFreq  - (sigStartFreq + sigStopFreq)/2)*1.0e-6;
				if (spectralAlgorithm == CConst::pwrSpectralAlgorithm) {
					double overlap = aciFn(fStopMHz, BMHz) - aciFn(fStartMHz, BMHz);
					*spectralOverlapLossDBptr = -10.0 * log(overlap) / log(10.0);
				} else {
					double fCrit;
					if ((fStartMHz <= 0.0) && (fStopMHz >= 0.0)) {
						fCrit = 0.0;
					} else {
						fCrit = std::min(fabs(fStartMHz), fabs(fStopMHz));
					}
					double psdDB;
					if (fCrit < BMHz/2) {
						psdDB = 0.0;
					} else if (fCrit < BMHz/2 + 1.0) {
						psdDB = -20.0*(fCrit - BMHz/2);
					} else if (fCrit < BMHz) {
						psdDB = (-20.0*(BMHz - fCrit) - 28.0*(fCrit - BMHz/2 - 1.0))/(BMHz/2 - 1.0);
					} else {
						psdDB = (-28.0*(3*BMHz/2 - fCrit) - 40.0*(fCrit - BMHz))/(BMHz/2);
					}
					double overlap = (rxStopFreq - rxStartFreq) / (sigStopFreq - sigStartFreq);
					*spectralOverlapLossDBptr = -psdDB - 10.0 * log(overlap) / log(10.0);
				}
			}

		}
	}

	return (hasOverlap);
}
/******************************************************************************************/

/******************************************************************************************/
/**** AfcManager::readULSData()                                                        ****/
/**** linkDirection: 0: RX                                                             ****/
/****                1: TX                                                             ****/
/****                2: RX and TX                                                      ****/
/******************************************************************************************/
void AfcManager::readULSData(const std::vector<std::tuple<std::string, std::string>>& ulsDatabaseList, PopGridClass *popGridVal, int linkDirection, double minFreq, double maxFreq, bool removeMobileFlag, CConst::SimulationEnum simulationFlag,
	const double& minLat, const double& maxLat, const double& minLon, const double& maxLon)
{

	auto fs_anom_writer = GzipCsvWriter(_fsAnomFile);
	auto &fanom = fs_anom_writer.csv_writer;

	if (fanom) {
		fanom->writeRow({"FSID,DBNAME,CALLSIGN,RX_LATITUDE,RX_LONGITUDE,ANOMALY_DESCRIPTION\n"});
	}

	int prIdx;
	int dbIdx;
	bool prevGdalDirectMode = _terrainDataModel->setGdalDirectMode(true);
	for(dbIdx=0; dbIdx<(int) ulsDatabaseList.size(); ++dbIdx) {
		std::string name = std::get<0>(ulsDatabaseList[dbIdx]);
		std::string filename = std::get<1>(ulsDatabaseList[dbIdx]);

		LOGGER_INFO(logger) << "Reading " << name << " ULS Database: " << filename;
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
		int numPR;
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
		double rxAntennaDiameter;
		double rxNearFieldAntDiameter;
		double rxNearFieldDistLimit;
		double rxNearFieldAntEfficiency;
		CConst::AntennaCategoryEnum rxAntennaCategory;
		double txGain;
		double txEIRP;
		CConst::ULSAntennaTypeEnum rxAntennaType;
		CConst::ULSAntennaTypeEnum txAntennaType;
		// double operatingRadius;
		// double rxSensitivity;
		int mobileUnit = -1;

		bool hasDiversity;
		double diversityHeightAboveTerrain;
		double diversityGain;
		double diversityAntennaDiameter;

		AntennaClass *rxAntenna = (AntennaClass *)NULL;
		AntennaClass *txAntenna = (AntennaClass *)NULL;
		double fadeMarginDB;
		std::string status;

		ULSClass *uls;
		UlsDatabase *ulsDatabase = new UlsDatabase();

		if (filename.empty())
		{
			throw std::runtime_error("ERROR: No ULS data file specified");
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
			ulsDatabase->loadFSById(QString::fromStdString(filename), rows, _exclusionZoneFSID);
		}
		else
		{
			ulsDatabase->loadUlsData(QString::fromStdString(filename), rows, minLat, maxLat, minLon, maxLon);
		}
		// Distributing FS TX by 1x1 degree squares to minimize GDAL reopening
		std::sort(rows.begin(), rows.end(),
			[](const UlsRecord &rl, const UlsRecord &rr) {
				double latDegL = std::floor(rl.txLatitudeDeg);
				double latDegR = std::floor(rr.txLatitudeDeg);
				if (latDegL != latDegR) {
					return latDegL < latDegR;
				}
				double lonDegL = std::floor(rl.txLongitudeDeg);
				double lonDegR = std::floor(rr.txLongitudeDeg);
				if (lonDegL != lonDegR) {
					return lonDegL < lonDegR;
				}
				return false;
			});

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

			/**************************************************************************/
			/* numPR (Number of Passive Repeaters)                                    */
			/**************************************************************************/
			numPR = row.numPR;
			/**************************************************************************/

			/**************************************************************************/
			/* Passive Repeater Parameters                                            */
			/**************************************************************************/
			if ( (!_filterSimRegionOnly) && (!ignoreFlag) ) {
				for(prIdx=0; prIdx<numPR; ++prIdx) {

					/******************************************************************/
					/* prLongitudeDeg                                                 */
					/******************************************************************/
					if (std::isnan(row.prLongitudeDeg[prIdx]) || (row.prLongitudeDeg[prIdx] == 0.0)) {
						reasonIgnored = "Ignored: PR Longitude has value nan or 0";
						ignoreFlag = true;
						numIgnoreInvalid++;
					}
					/******************************************************************/

					/******************************************************************/
					/* prLatitudeDeg                                                  */
					/******************************************************************/
					if (std::isnan(row.prLatitudeDeg[prIdx]) || (row.prLatitudeDeg[prIdx] == 0.0)) {
						reasonIgnored = "Ignored: PR Latitude has value nan or 0";
						ignoreFlag = true;
						numIgnoreInvalid++;
					}
					/******************************************************************/

					/******************************************************************/
					/* prHeightAboveTerrain                                           */
					/******************************************************************/
					if (std::isnan(row.prHeightAboveTerrain[prIdx])) {
						ignoreFlag = true;
						reasonIgnored = "missing PR Height above Terrain";
						numIgnoreInvalid++;
					}

					if (!ignoreFlag) {
						if (row.prHeightAboveTerrain[prIdx] <= 0.0) {
							LOGGER_WARN(logger) << "WARNING: ULS data for FSID = " << fsid
								<< ", Passive Repeater " << (prIdx+1)
								<< ", prHeightAboveTerrain = " << row.prHeightAboveTerrain[prIdx] << " is < 0.0";
						}
					}
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
					/* rxAntennaDiameter                                                      */
					/**************************************************************************/
					rxAntennaDiameter = row.rxAntennaDiameter;
					/**************************************************************************/

					/**************************************************************************/
					/* rxNearFieldAntDiameter;                                                */
					/**************************************************************************/
					rxNearFieldAntDiameter = row.rxNearFieldAntDiameter;
					/**************************************************************************/

					/**************************************************************************/
					/* rxNearFieldDistLimit;                                                  */
					/**************************************************************************/
					rxNearFieldDistLimit = row.rxNearFieldDistLimit;
					/**************************************************************************/

					/**************************************************************************/
					/* rxNearFieldAntEfficiency;                                              */
					/**************************************************************************/
					rxNearFieldAntEfficiency = row.rxNearFieldAntEfficiency;
					/**************************************************************************/

					/**************************************************************************/
					/* rxAntennaCategory                                                      */
					/**************************************************************************/
					rxAntennaCategory = row.rxAntennaCategory;
					/**************************************************************************/

					/**************************************************************************/
					/* rxAntenna                                                              */
					/**************************************************************************/
					if (!ignoreFlag) {
						if ((_ulsAntennaList.size() == 0)||(row.rxAntennaModel.empty())) {
							rxAntennaType = _ulsDefaultAntennaType;
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
									errStr << "Invalid ULS data for FSID = " << fsid
										<< ", Unknown Rx Antenna \"" << strval
										<< "\" using " << CConst::strULSAntennaTypeList->type_to_str(_ulsDefaultAntennaType);
									LOGGER_WARN(logger) << errStr.str();
									statusMessageList.push_back(errStr.str());

									rxAntennaType = _ulsDefaultAntennaType;
								}
							}
						}
					}
					/**************************************************************************/

					/**************************************************************************/
					/* hasDiversity                                                           */
					/**************************************************************************/
					hasDiversity = row.hasDiversity;
					/**************************************************************************/

					if (hasDiversity) {
						/**********************************************************************/
						/* diversityHeightAboveTerrain                                        */
						/**********************************************************************/
						diversityHeightAboveTerrain = row.diversityHeightAboveTerrain;
						if (!ignoreFlag) {
							if (std::isnan(diversityHeightAboveTerrain)) {
								ignoreFlag = true;
								reasonIgnored = "missing Rx Diversity Height above Terrain";
								numIgnoreInvalid++;
							}
						}

						if (!ignoreFlag) {
							if (diversityHeightAboveTerrain < 3.0) {
								LOGGER_WARN(logger) << "WARNING: ULS data for FSID = " << fsid << ", diversityHeightAboveTerrain = " << diversityHeightAboveTerrain << " is < 3.0";
							}
						}
						/**********************************************************************/

						/**********************************************************************/
						/* diversityGain                                                      */
						/**********************************************************************/
						diversityGain = row.diversityGain;
						if (!ignoreFlag) {
							if (std::isnan(diversityGain)) {
								ignoreFlag = true;
								reasonIgnored = "missing Rx Diversity Gain";
								numIgnoreInvalid++;
							}
						}
						/**********************************************************************/

						/**********************************************************************/
						/* rxAntennaDiameter                                                  */
						/**********************************************************************/
						diversityAntennaDiameter = row.diversityAntennaDiameter;
						/**********************************************************************/
					}

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
					// ULS db does not have txAntenna field so use default
					txAntennaType = _ulsDefaultAntennaType;
					/**************************************************************************/
				}
			}

			/**************************************************************************/
			/* status                                                                 */
			/**************************************************************************/
			status = row.status;
			/**************************************************************************/

			if (!_filterSimRegionOnly) {
				if (!ignoreFlag) {
					for(int segIdx=0; segIdx<numPR+1; ++segIdx) {
						Vector3 txLon = (segIdx==0     ? txLongitudeDeg : row.prLongitudeDeg[prIdx-1]);
						Vector3 txLat = (segIdx==0     ? txLatitudeDeg  : row.prLatitudeDeg[prIdx-1]);
						Vector3 rxLon = (segIdx==numPR ? rxLongitudeDeg : row.prLongitudeDeg[prIdx]);
						Vector3 rxLat = (segIdx==numPR ? rxLatitudeDeg  : row.prLatitudeDeg[prIdx]);

						if ((rxLat == txLat) && (rxLon == txLon)) {
							reasonIgnored = "Ignored: RX and TX LON/LAT values are identical for segment " + std::to_string(segIdx);
							ignoreFlag = true;
							numIgnoreInvalid++;
						}
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
				bool unii5Flag = computeSpectralOverlapLoss((double *) NULL, startFreq, stopFreq, 5925.0e6, 6425.0e6, false, CConst::psdSpectralAlgorithm);

				bool unii7Flag = computeSpectralOverlapLoss((double *) NULL, startFreq, stopFreq, 6525.0e6, 6875.0e6, false, CConst::psdSpectralAlgorithm);

				double rxAntennaFeederLossDB = row.rxLineLoss;
				if (std::isnan(rxAntennaFeederLossDB)) {
					if (unii5Flag && unii7Flag) {
						rxAntennaFeederLossDB = std::min(_rxFeederLossDBUNII5, _rxFeederLossDBUNII7);
					} else if (unii5Flag) {
						rxAntennaFeederLossDB = _rxFeederLossDBUNII5;
					} else if (unii7Flag) {
						rxAntennaFeederLossDB = _rxFeederLossDBUNII7;
					} else {
						rxAntennaFeederLossDB = _rxFeederLossDBOther;
					}
				}
				double noiseFigureDB;
				if (unii5Flag && unii7Flag) {
					noiseFigureDB = std::min(_ulsNoiseFigureDBUNII5, _ulsNoiseFigureDBUNII7);
				} else if (unii5Flag) {
					noiseFigureDB = _ulsNoiseFigureDBUNII5;
				} else if (unii7Flag) {
					noiseFigureDB = _ulsNoiseFigureDBUNII7;
				} else {
					noiseFigureDB = _ulsNoiseFigureDBOther;
				}
				double centerFreq = (startFreq + stopFreq)/2;
				double lambda = CConst::c / centerFreq;
				double rxDlambda = rxAntennaDiameter / lambda;

				uls = new ULSClass(this, fsid, dbIdx, numPR);
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
				uls->setRxDlambda(rxDlambda);
				uls->setRxNearFieldAntDiameter(rxNearFieldAntDiameter);
				uls->setRxNearFieldDistLimit(rxNearFieldDistLimit);
				uls->setRxNearFieldAntEfficiency(rxNearFieldAntEfficiency);
				uls->setRxAntennaModel(row.rxAntennaModel);
				uls->setRxAntennaType(rxAntennaType);
				uls->setTxAntennaType(txAntennaType);
				uls->setRxAntenna(rxAntenna);
				uls->setTxAntenna(txAntenna);
				uls->setRxAntennaCategory(rxAntennaCategory);
				uls->setTxGain(txGain);
				uls->setTxEIRP(txEIRP);
				uls->setUseFrequency();
				uls->setRxAntennaFeederLossDB(rxAntennaFeederLossDB);
				uls->setFadeMarginDB(fadeMarginDB);
				uls->setStatus(status);

				uls->setHasDiversity(hasDiversity);
				if (hasDiversity) {
					uls->setDiversityGain(diversityGain);

					double diversityDlambda = diversityAntennaDiameter / lambda;
					uls->setDiversityDlambda(diversityDlambda);
				}

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
				CConst::HeightSourceEnum prHeightSource;
				Vector3 rxPosition, txPosition, prPosition, diversityPosition;
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
					double rxHeight = rxHeightAboveTerrain + terrainHeight;

					uls->setRxTerrainHeightFlag(rxTerrainHeightFlag);
					uls->setRxTerrainHeight(terrainHeight);
					uls->setRxHeightAboveTerrain(rxHeightAboveTerrain);
					uls->setRxHeightAMSL(rxHeight);
					uls->setRxHeightSource(rxHeightSource);

					rxPosition = EcefModel::geodeticToEcef(rxLatitudeDeg, rxLongitudeDeg, rxHeight / 1000.0);
					uls->setRxPosition(rxPosition);

					if (hasDiversity) {
						double diversityHeight = diversityHeightAboveTerrain + terrainHeight;
						uls->setDiversityHeightAboveTerrain(diversityHeightAboveTerrain);
						uls->setDiversityHeightAMSL(diversityHeight);

						diversityPosition = EcefModel::geodeticToEcef(rxLatitudeDeg, rxLongitudeDeg, diversityHeight / 1000.0);
						uls->setDiversityPosition(diversityPosition);

					}

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
					double txHeight = txHeightAboveTerrain + terrainHeight;

					uls->setTxTerrainHeightFlag(txTerrainHeightFlag);
					uls->setTxTerrainHeight(terrainHeight);
					uls->setTxHeightAboveTerrain(txHeightAboveTerrain);
					uls->setTxHeightSource(txHeightSource);
					uls->setTxHeightAMSL(txHeight);

					txPosition = EcefModel::geodeticToEcef(txLatitudeDeg, txLongitudeDeg, txHeight / 1000.0);
					uls->setTxPosition(txPosition);
				}

				if ((!mobileTxFlag) && (!randPointingFlag))
				{
					for(prIdx=0; prIdx<numPR; ++prIdx) {
						PRClass& pr = uls->getPR(prIdx);

						int validFlag;
						pr.type = (CConst::PRTypeEnum) CConst::strPRTypeList->str_to_type(row.prType[prIdx], validFlag, 1);
						pr.longitudeDeg = row.prLongitudeDeg[prIdx];
						pr.latitudeDeg = row.prLatitudeDeg[prIdx];
						pr.heightAboveTerrain = row.prHeightAboveTerrain[prIdx];

						if ( (_terrainDataModel))
						{
							_terrainDataModel->getTerrainHeight(pr.longitudeDeg, pr.latitudeDeg, terrainHeight, bldgHeight, lidarHeightResult, prHeightSource);
							pr.terrainHeightFlag = true;
						}
						else
						{
							pr.terrainHeightFlag = false;
							terrainHeight = 0.0;
						}

						pr.terrainHeight = terrainHeight;
						pr.heightAMSL = pr.heightAboveTerrain + pr.terrainHeight;
						pr.heightSource = prHeightSource;

						pr.position = EcefModel::geodeticToEcef(pr.latitudeDeg, pr.longitudeDeg, pr.heightAMSL / 1000.0);

						if (row.prType[prIdx] == "Ant") {
							pr.type = CConst::backToBackAntennaPRType;
							pr.txGain = row.prTxGain[prIdx];
							pr.txDlambda = row.prTxAntennaDiameter[prIdx] / lambda;
							pr.rxGain = row.prRxGain[prIdx];
							pr.rxDlambda = row.prRxAntennaDiameter[prIdx] / lambda;
						} else if (row.prType[prIdx] == "Ref") {
							pr.type = CConst::billboardReflectorPRType;
							pr.reflectorHeightLambda = row.prReflectorHeight[prIdx] / lambda;
							pr.reflectorWidthLambda = row.prReflectorWidth[prIdx] / lambda;

						} else {
							CORE_DUMP;
						}
					}
				}

				if ((!mobileRxFlag) && (!mobileTxFlag)) {
					if (!randPointingFlag) {
						for(prIdx=0; prIdx<=numPR; ++prIdx) {
							Vector3 segTxPosn = (prIdx == 0 ? txPosition : uls->getPR(prIdx-1).position);
							Vector3 segRxPosn = (prIdx == numPR ? rxPosition : uls->getPR(prIdx).position);
							Vector3 pointing = (segTxPosn - segRxPosn).normalized();
							double segDist = (segTxPosn - segRxPosn).len() * 1000.0;

							if (prIdx == numPR) {
								uls->setAntennaPointing(pointing); // Pointing of Rx antenna
								uls->setLinkDistance(segDist);
								if (hasDiversity) {
									pointing = (segTxPosn - diversityPosition).normalized();
									uls->setDiversityAntennaPointing(pointing); // Pointing of Rx Diversity antenna
								}
							} else {
								uls->getPR(prIdx).pointing = pointing; // Pointing of Passive Receiver
								uls->getPR(prIdx).segmentDistance = segDist;
							}
						}

						/******************************************************************/
						/* Calculate PR parameters:                                       */
						/*     Orthonormal basis                                          */
						/*     thetaIN                                                    */
						/*     alphaAZ                                                    */
						/*     alphaEL                                                    */
						/******************************************************************/
						for(prIdx=numPR-1; prIdx>=0; prIdx--) {
							PRClass& pr = uls->getPR(prIdx);
							double nextSegDist = ((prIdx == numPR-1) ? uls->getLinkDistance() : uls->getPR(prIdx+1).segmentDistance);
							double nextSegFSPL = 20.0 * log((4 * M_PI * centerFreq * nextSegDist) / CConst::c) / log(10.0);

							if (pr.type == CConst::backToBackAntennaPRType) {
								pr.pathSegGain = pr.rxGain + pr.txGain - nextSegFSPL;
							} else if (pr.type == CConst::billboardReflectorPRType) {
								Vector3 pointingA = pr.pointing;
								Vector3 pointingB = -(prIdx == numPR-1 ? uls->getAntennaPointing() : uls->getPR(prIdx+1).pointing);
								pr.reflectorZ = (pointingA + pointingB).normalized();              // Perpendicular to reflector surface
								Vector3 upVec = pr.position.normalized();
								pr.reflectorX = (upVec.cross(pr.reflectorZ)).normalized();         // Horizontal
								pr.reflectorY = (pr.reflectorZ.cross(pr.reflectorX)).normalized();

								double Ax = pointingA.dot(pr.reflectorX);
								double Ay = pointingA.dot(pr.reflectorY);
								double Az = pointingA.dot(pr.reflectorZ);

								double alphaAZ = (180.0/M_PI)*fabs(atan(Ax/Az));
								double alphaEL = (180.0/M_PI)*fabs(atan(Ay/Az));

								double cosThetaIN = pointingA.dot(pr.reflectorZ);

								if (alphaEL <= alphaAZ) {
									pr.reflectorSLambda = pr.reflectorWidthLambda*cosThetaIN;
								} else {
									pr.reflectorSLambda = pr.reflectorHeightLambda*cosThetaIN;
								}
								pr.reflectorTheta1 = (180.0/M_PI)*asin(1.0/(2*pr.reflectorSLambda));
								double Ks = 4*pr.reflectorWidthLambda*pr.reflectorHeightLambda*cosThetaIN*lambda/(M_PI*nextSegDist);
								double alpha_n = 20*log10(M_PI*Ks/4);

								if ( (Ks <= 0.4) || ((prIdx < numPR-1) && (uls->getPR(prIdx+1).type == CConst::billboardReflectorPRType)) ) {
									pr.pathSegGain = std::min(3.0,alpha_n);
								} else {
									double nextDlambda = ((prIdx == numPR-1) ? uls->getRxDlambda() : uls->getPR(prIdx+1).rxDlambda);
									double Q = nextDlambda*sqrt(M_PI/(4*pr.reflectorWidthLambda*pr.reflectorHeightLambda*cosThetaIN));
									pr.pathSegGain = _prTable->computePRTABLE(Q, 1.0/Ks);
								}
							}
							pr.effectiveGain = (prIdx == numPR-1 ? 0 : uls->getPR(prIdx+1).effectiveGain) + pr.pathSegGain;
						}
						/******************************************************************/

					} else {
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

				double noiseLevelDBW = 10.0 * log(CConst::boltzmannConstant * CConst::T0 * bandwidth) / log(10.0) + noiseFigureDB;
				uls->setNoiseLevelDBW(noiseLevelDBW);
				if (fixedFlag && fanom) {
					fanom->writeRow({QString::asprintf("%d,%s,%s,%.15f,%.15f,%s\n", fsid, name.c_str(), callsign.c_str(), rxLatitudeDeg, rxLongitudeDeg, fixedStr.c_str())});
				}
			} else if (fanom) {
				fanom->writeRow({QString::asprintf("%d,%s,%s,%.15f,%.15f,%s\n", fsid, name.c_str(), callsign.c_str(), rxLatitudeDeg, rxLongitudeDeg, reasonIgnored.c_str())});
			}

		}
		_terrainDataModel->setGdalDirectMode(prevGdalDirectMode);

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

		delete ulsDatabase;
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
		bool updateFlag = false;
		double bldgHeight, terrainHeight;
		MultibandRasterClass::HeightResult lidarHeightResult;
		CConst::HeightSourceEnum heightSource;	
		if (!uls->getRxTerrainHeightFlag()) {
			_terrainDataModel->getTerrainHeight(uls->getRxLongitudeDeg(),uls->getRxLatitudeDeg(), terrainHeight,bldgHeight, lidarHeightResult, heightSource);
			updateFlag = true;
			uls->setRxTerrainHeightFlag(true);
			double rxHeight = uls->getRxHeightAboveTerrain() + terrainHeight;
			Vector3 rxPosition = EcefModel::geodeticToEcef(uls->getRxLatitudeDeg(), uls->getRxLongitudeDeg(), rxHeight / 1000.0);
			uls->setRxPosition(rxPosition);
			uls->setRxTerrainHeight(terrainHeight);
			uls->setRxHeightAMSL(rxHeight);
			uls->setRxHeightSource(heightSource);
		}
		if (!uls->getTxTerrainHeightFlag()) {
			_terrainDataModel->getTerrainHeight(uls->getTxLongitudeDeg(),uls->getTxLatitudeDeg(), terrainHeight,bldgHeight, lidarHeightResult, heightSource);
			updateFlag = true;
			uls->setTxTerrainHeightFlag(true);
			double txHeight = uls->getTxHeightAboveTerrain() + terrainHeight;
			Vector3 txPosition = EcefModel::geodeticToEcef(uls->getTxLatitudeDeg(), uls->getTxLongitudeDeg(), txHeight / 1000.0);
			uls->setTxPosition(txPosition);
			uls->setTxTerrainHeight(terrainHeight);
			uls->setTxHeightAMSL(txHeight);
			uls->setTxHeightSource(heightSource);
		}
		for(int prIdx=0; prIdx<uls->getNumPR(); ++prIdx) {
			PRClass& pr = uls->getPR(prIdx);

			if (!pr.terrainHeightFlag) {
				_terrainDataModel->getTerrainHeight(pr.longitudeDeg, pr.latitudeDeg, terrainHeight, bldgHeight, lidarHeightResult, heightSource);
				updateFlag = true;
				pr.terrainHeightFlag = true;

				double prHeight = pr.heightAboveTerrain + terrainHeight;
				Vector3 prPosition = EcefModel::geodeticToEcef(pr.latitudeDeg, pr.longitudeDeg, prHeight / 1000.0);
				pr.position = prPosition;
				pr.terrainHeight = terrainHeight;
				pr.heightAMSL = prHeight;
				pr.heightSource = heightSource;
			}
		}

		if (updateFlag) {
			for(int segIdx=0; segIdx<uls->getNumPR()+1; ++segIdx) {
				Vector3 txPosition = (segIdx==0 ? uls->getTxPosition() : uls->getPR(segIdx-1).position);
				Vector3 rxPosition = (segIdx==uls->getNumPR() ? uls->getRxPosition() : uls->getPR(segIdx).position);

				Vector3 antennaPointing = (txPosition - rxPosition).normalized(); // boresight pointing direction of RX
				double segmentDistance = (txPosition - rxPosition).len() * 1000.0;

				if (segIdx==uls->getNumPR()) {
					uls->setAntennaPointing(antennaPointing); // Pointing of Rx antenna
					uls->setLinkDistance(segmentDistance);
				} else {
					uls->getPR(segIdx).pointing = antennaPointing;
					uls->getPR(segIdx).segmentDistance = segmentDistance;
				}
			}
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
double AfcManager::computeBuildingPenetration(CConst::BuildingTypeEnum buildingType, double elevationAngleDeg, double frequency, std::string &buildingPenetrationModelStr, double &buildingPenetrationCDF, bool fixedProbFlag) const
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
	double Le = 0.212 * fabs(elevationAngleDeg);
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
void AfcManager::computePathLoss(CConst::PathLossModelEnum pathLossModel, CConst::PropEnvEnum propEnv, CConst::PropEnvEnum propEnvRx,
		CConst::NLCDLandCatEnum nlcdLandCatTx, CConst::NLCDLandCatEnum nlcdLandCatRx,
		double distKm, double fsplDistKm, double win2DistKm, double frequency,
		double txLongitudeDeg, double txLatitudeDeg, double txHeightM, double elevationAngleTxDeg,
		double rxLongitudeDeg, double rxLatitudeDeg, double rxHeightM, double elevationAngleRxDeg,
		double &pathLoss, double &pathClutterTxDB, double &pathClutterRxDB, bool fixedProbFlag,
		std::string &pathLossModelStr, double &pathLossCDF,
		std::string &pathClutterTxModelStr, double &pathClutterTxCDF, std::string &pathClutterRxModelStr, double &pathClutterRxCDF,
		const iturp452::ITURP452 *itu452, std::string *txClutterStrPtr, std::string *rxClutterStrPtr, double **ITMProfilePtr, double **isLOSProfilePtr
#if DEBUG_AFC
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

	if (rxClutterStrPtr)
	{
		*rxClutterStrPtr = "";
	}

	pathLossModelStr = "";
	if (!fixedProbFlag)
	{
		pathLossCDF = -1.0;
	}
	pathClutterTxModelStr = "";
	pathClutterTxCDF = -1.0;
	pathClutterRxModelStr = "";
	pathClutterRxCDF = -1.0;


	if (pathLossModel == CConst::ITMBldgPathLossModel)
	{
		if ((propEnv == CConst::urbanPropEnv) || (propEnv == CConst::suburbanPropEnv))
		{
			if (win2DistKm * 1000 < _closeInDist)
			{
				if (_closeInPathLossModel == "WINNER2")
				{
					int winner2LOSValue = 0; // 1: Force LOS, 2: Force NLOS, 0: Compute probLOS, then select or combine.
					if (win2DistKm * 1000 <= 50.0) {
						winner2LOSValue = 1;
					} else if (
							(_winner2LOSOption == CConst::BldgDataLOSOption)
							|| (_winner2LOSOption == CConst::BldgDataReqTxLOSOption)
							|| (_winner2LOSOption == CConst::BldgDataReqRxLOSOption)
							|| (_winner2LOSOption == CConst::BldgDataReqTxRxLOSOption) ) {
						double terrainHeight;
						double bldgHeight;
						MultibandRasterClass::HeightResult lidarHeightResult;
						CConst::HeightSourceEnum txHeightSource, rxHeightSource;
						_terrainDataModel->getTerrainHeight(txLongitudeDeg, txLatitudeDeg, terrainHeight, bldgHeight, lidarHeightResult, txHeightSource);
						_terrainDataModel->getTerrainHeight(rxLongitudeDeg, rxLatitudeDeg, terrainHeight, bldgHeight, lidarHeightResult, rxHeightSource);

						bool reqTx = (_winner2LOSOption == CConst::BldgDataReqTxLOSOption) || (_winner2LOSOption == CConst::BldgDataReqTxRxLOSOption);
						bool reqRx = (_winner2LOSOption == CConst::BldgDataReqRxLOSOption) || (_winner2LOSOption == CConst::BldgDataReqTxRxLOSOption);

						if (    ((!reqTx) || (txHeightSource == CConst::lidarHeightSource))
								&& ((!reqRx) || (rxHeightSource == CConst::lidarHeightSource)) ) {
							int numPts = std::min(((int)floor(distKm*1000 / _itmMinSpacing)) + 1, _itmMaxNumPts);
							bool losFlag = UlsMeasurementAnalysis::isLOS(_terrainDataModel,
									QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
									QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
									distKm, numPts, isLOSProfilePtr);
							winner2LOSValue = (losFlag ? 1 : 2);
						}
					} else if (_winner2LOSOption == CConst::ForceLOSLOSOption) {
						winner2LOSValue = 1;
					} else if (_winner2LOSOption == CConst::ForceNLOSLOSOption) {
						winner2LOSValue = 2;
					}

					double sigma, probLOS;
					if (propEnv == CConst::urbanPropEnv)
					{
						// Winner2 C2: urban
						pathLoss = Winner2_C2urban(1000 * win2DistKm, rxHeightM, txHeightM, frequency, fixedProbFlag, sigma, pathLossModelStr, pathLossCDF, probLOS, winner2LOSValue);
					}
					else if (propEnv == CConst::suburbanPropEnv)
					{
						// Winner2 C1: suburban
						pathLoss = Winner2_C1suburban(1000 * win2DistKm, rxHeightM, txHeightM, frequency, fixedProbFlag, sigma, pathLossModelStr, pathLossCDF, probLOS, winner2LOSValue);
					}
				}
				else
				{
					throw std::runtime_error(ErrStream() << "ERROR: Invalid close in path loss model = " << _closeInPathLossModel);
				}
			}
			else
			{
				// Terrain propagation: Terrain + ITM
				double frequencyMHz = 1.0e-6 * frequency;
				// std::cerr << "PATHLOSS," << txLatitudeDeg << "," << txLongitudeDeg << "," << rxLatitudeDeg << "," << rxLongitudeDeg << std::endl;
				int numPts = std::min(((int)floor(distKm*1000 / _itmMinSpacing)) + 1, _itmMaxNumPts);

				int radioClimate    = _ituData->getRadioClimateValue(txLatitudeDeg, txLongitudeDeg);
				int radioClimateTmp = _ituData->getRadioClimateValue(rxLatitudeDeg, rxLongitudeDeg);
				if (radioClimateTmp < radioClimate) {
					radioClimate = radioClimateTmp;
				}
				double surfaceRefractivity = _ituData->getSurfaceRefractivityValue((txLatitudeDeg+rxLatitudeDeg)/2, (txLongitudeDeg+rxLongitudeDeg)/2);

				/******************************************************************************************/
				/**** NOTE: ITM based on signal loss, so higher confidence corresponds to higher loss. ****/
				/******************************************************************************************/
				double u = _confidenceITM;

				pathLoss = UlsMeasurementAnalysis::runPointToPoint(_terrainDataModel,
						true,
						QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
						QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
						distKm, _itmEpsDielect, _itmSgmConductivity, surfaceRefractivity, frequencyMHz, radioClimate, _itmPolarization, u, _reliabilityITM, numPts, NULL, ITMProfilePtr);
				pathLossModelStr = "ITM_BLDG";
				pathLossCDF = _confidenceITM;
			}
		}
		else if ((propEnv == CConst::ruralPropEnv) || (propEnv == CConst::barrenPropEnv))
		{
			// Terrain propagation: Terrain + ITM
			double frequencyMHz = 1.0e-6 * frequency;
			int numPts = std::min(((int)floor(distKm*1000 / _itmMinSpacing)) + 1, _itmMaxNumPts);
			int radioClimate    = _ituData->getRadioClimateValue(txLatitudeDeg, txLongitudeDeg);
			int radioClimateTmp = _ituData->getRadioClimateValue(rxLatitudeDeg, rxLongitudeDeg);
			if (radioClimateTmp < radioClimate) {
				radioClimate = radioClimateTmp;
			}
			double surfaceRefractivity = _ituData->getSurfaceRefractivityValue((txLatitudeDeg+rxLatitudeDeg)/2, (txLongitudeDeg+rxLongitudeDeg)/2);
			double u = _confidenceITM;
			pathLoss = UlsMeasurementAnalysis::runPointToPoint(_terrainDataModel,
					true,
					QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
					QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
					distKm, _itmEpsDielect, _itmSgmConductivity, surfaceRefractivity, frequencyMHz, radioClimate, _itmPolarization, u, _reliabilityITM, numPts, NULL, ITMProfilePtr);
			pathLossModelStr = "ITM_BLDG";
			pathLossCDF = _confidenceITM;

			pathLossModelStr = "ITM_BLDG";
			pathLossCDF = _confidenceITM;
		}
		else
		{
			throw std::runtime_error(ErrStream() << "ERROR reading ULS data: propEnv = " << propEnv << " INVALID value");
		}
		pathClutterTxDB = 0.0;
		pathClutterTxModelStr = "NONE";
		pathClutterTxCDF = 0.5;
		pathClutterRxDB = 0.0;
		pathClutterRxModelStr = "NONE";
		pathClutterRxCDF = 0.5;
	}
	else if (pathLossModel == CConst::CoalitionOpt6PathLossModel)
	{
#if 1
		// As of 2021.12.03 this path loss model is no longer supported for AFC.
		throw std::runtime_error(ErrStream() << "ERROR: unsupported path loss model selected");
#else
		// Path Loss Model selected for 6 GHz Coalition
		// Option 6 in PropagationModelOptions Winner-II 20171004.pptx

		if ((propEnv == CConst::urbanPropEnv) || (propEnv == CConst::suburbanPropEnv))
		{
			if (distKm * 1000 < _closeInDist)
			{
				if (_closeInPathLossModel == "WINNER2")
				{
					int winner2LOSValue = 0; // 1: Force LOS, 2: Force NLOS, 0: Compute probLOS, then select or combine.
					if (distKm * 1000 <= 50.0) {
						winner2LOSValue = 1;
					} else if (_winner2LOSOption == CConst::BldgDataLOSOption) {
						double terrainHeight;
						double bldgHeight;
						MultibandRasterClass::HeightResult lidarHeightResult;
						CConst::HeightSourceEnum txHeightSource, rxHeightSource;
						_terrainDataModel->getTerrainHeight(txLongitudeDeg, txLatitudeDeg, terrainHeight, bldgHeight, lidarHeightResult, txHeightSource);
						_terrainDataModel->getTerrainHeight(rxLongitudeDeg, rxLatitudeDeg, terrainHeight, bldgHeight, lidarHeightResult, rxHeightSource);

						if ( (txHeightSource == CConst::lidarHeightSource) && (rxHeightSource == CConst::lidarHeightSource) ) {
							int numPts = std::min(((int)floor(distKm*1000 / _itmMinSpacing)) + 1, _itmMaxNumPts);
							bool losFlag = UlsMeasurementAnalysis::isLOS(_terrainDataModel,
									QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
									QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
									distKm, numPts, isLOSProfilePtr);
							winner2LOSValue = (losFlag ? 1 : 2);
						}
					} else if (_winner2LOSOption == CConst::ForceLOSLOSOption) {
						winner2LOSValue = 1;
					} else if (_winner2LOSOption == CConst::ForceNLOSLOSOption) {
						winner2LOSValue = 2;
					}

					double sigma, probLOS;
					if (propEnv == CConst::urbanPropEnv)
					{
						// Winner2 C2: urban
						pathLoss = Winner2_C2urban(1000 * distKm, rxHeightM, txHeightM, frequency, fixedProbFlag, sigma, pathLossModelStr, pathLossCDF, probLOS, winner2LOSValue);
					}
					else if (propEnv == CConst::suburbanPropEnv)
					{
						// Winner2 C1: suburban
						pathLoss = Winner2_C1suburban(1000 * distKm, rxHeightM, txHeightM, frequency, fixedProbFlag, sigma, pathLossModelStr, pathLossCDF, probLOS, winner2LOSValue);
					}
				}
				else
				{
					throw std::runtime_error(ErrStream() << "ERROR: Invalid close in path loss model = " << _closeInPathLossModel);
				}
				pathClutterTxDB = 0.0;
				pathClutterTxModelStr = "NONE";
				pathClutterTxCDF = 0.5;
			}
			else
			{
				// Terrain propagation: Terrain + ITM
				double frequencyMHz = 1.0e-6 * frequency;
				// std::cerr << "PATHLOSS," << txLatitudeDeg << "," << txLongitudeDeg << "," << rxLatitudeDeg << "," << rxLongitudeDeg << std::endl;
				int numPts = std::min(((int)floor(distKm*1000 / _itmMinSpacing)) + 1, _itmMaxNumPts);
				int radioClimate    = _ituData->getRadioClimateValue(txLatitudeDeg, txLongitudeDeg);
				int radioClimateTmp = _ituData->getRadioClimateValue(rxLatitudeDeg, rxLongitudeDeg);
				if (radioClimateTmp < radioClimate) {
					radioClimate = radioClimateTmp;
				}
				double surfaceRefractivity = _ituData->getSurfaceRefractivityValue((txLatitudeDeg+rxLatitudeDeg)/2, (txLongitudeDeg+rxLongitudeDeg)/2);

				/******************************************************************************************/
				/**** NOTE: ITM based on signal loss, so higher confidence corresponds to higher loss. ****/
				/******************************************************************************************/
				double u = _confidenceITM;

				pathLoss = UlsMeasurementAnalysis::runPointToPoint(_terrainDataModel,
						false,
						QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
						QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
						distKm, _itmEpsDielect, _itmSgmConductivity, surfaceRefractivity, frequencyMHz, radioClimate, _itmPolarization, u, _reliabilityITM, numPts, NULL, ITMProfilePtr);
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
				pathClutterTxDB = Lctt;

				pathClutterTxModelStr = "P.2108";
				pathClutterTxCDF = q(-gauss[0]);
				if (_applyClutterFSRxFlag && (rxHeightM <= 10.0) && (distKm >= 1.0)) {
					pathClutterRxDB = pathClutterTxDB;
					pathClutterRxModelStr = pathClutterTxModelStr;
					pathClutterRxCDF = pathClutterTxCDF;
				} else {
					pathClutterRxDB = 0.0;
					pathClutterRxModelStr = "NONE";
					pathClutterRxCDF = 0.5;
				}
			}
		}
		else if ((propEnv == CConst::ruralPropEnv) || (propEnv == CConst::barrenPropEnv))
		{
			// Terrain propagation: Terrain + ITM
			double frequencyMHz = 1.0e-6 * frequency;
			int numPts = std::min(((int)floor(distKm*1000 / _itmMinSpacing)) + 1, _itmMaxNumPts);
			int radioClimate    = _ituData->getRadioClimateValue(txLatitudeDeg, txLongitudeDeg);
			int radioClimateTmp = _ituData->getRadioClimateValue(rxLatitudeDeg, rxLongitudeDeg);
			if (radioClimateTmp < radioClimate) {
				radioClimate = radioClimateTmp;
			}
			double surfaceRefractivity = _ituData->getSurfaceRefractivityValue((txLatitudeDeg+rxLatitudeDeg)/2, (txLongitudeDeg+rxLongitudeDeg)/2);
			double u = _confidenceITM;
			pathLoss = UlsMeasurementAnalysis::runPointToPoint(_terrainDataModel,
					false,
					QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
					QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
					distKm, _itmEpsDielect, _itmSgmConductivity, surfaceRefractivity, frequencyMHz, radioClimate, _itmPolarization, u, _reliabilityITM, numPts, NULL, ITMProfilePtr);
			pathLossModelStr = "ITM";
			pathLossCDF = _confidenceITM;

			// ITU-R p.452 Clutter loss function
			pathClutterTxDB = AfcManager::computeClutter452HtEl(txHeightM, distKm, elevationAngleTxDeg);
			pathClutterTxModelStr = "452_HT_ELANG";
			pathClutterTxCDF = 0.5;

			if (_applyClutterFSRxFlag && (rxHeightM <= 10.0) && (distKm >= 1.0)) {
				pathClutterRxDB = AfcManager::computeClutter452HtEl(rxHeightM, distKm, elevationAngleRxDeg);
				pathClutterRxModelStr = "452_HT_ELANG";
				pathClutterRxCDF = 0.5;
			} else {
				pathClutterRxDB = 0.0;
				pathClutterRxModelStr = "NONE";
				pathClutterRxCDF = 0.5;
			}
		}
		else
		{
			throw std::runtime_error(ErrStream() << "ERROR: propEnv = " << propEnv << " INVALID value");
		}
#endif
	}
	else if (pathLossModel == CConst::FCC6GHzReportAndOrderPathLossModel)
	{
		// Path Loss Model used in FCC Report and Order

		if (fsplDistKm*1000 < 30.0) {
			pathLoss = 20.0 * log((4 * M_PI * frequency * fsplDistKm * 1000) / CConst::c) / log(10.0);
			pathLossModelStr = "FSPL";
			pathLossCDF = 0.5;

			pathClutterTxDB = 0.0;
			pathClutterTxModelStr = "NONE";
			pathClutterTxCDF = 0.5;
		} else if (win2DistKm * 1000 < _closeInDist) {
			int winner2LOSValue = 0; // 1: Force LOS, 2: Force NLOS, 0: Compute probLOS, then select or combine.
			if (win2DistKm * 1000 <= 50.0) {
				winner2LOSValue = 1;
			} else if (
					(_winner2LOSOption == CConst::BldgDataLOSOption)
					|| (_winner2LOSOption == CConst::BldgDataReqTxLOSOption)
					|| (_winner2LOSOption == CConst::BldgDataReqRxLOSOption)
					|| (_winner2LOSOption == CConst::BldgDataReqTxRxLOSOption) ) {
				double terrainHeight;
				double bldgHeight;
				MultibandRasterClass::HeightResult lidarHeightResult;
				CConst::HeightSourceEnum txHeightSource, rxHeightSource;
				_terrainDataModel->getTerrainHeight(txLongitudeDeg, txLatitudeDeg, terrainHeight, bldgHeight, lidarHeightResult, txHeightSource);
				_terrainDataModel->getTerrainHeight(rxLongitudeDeg, rxLatitudeDeg, terrainHeight, bldgHeight, lidarHeightResult, rxHeightSource);

				bool reqTx = (_winner2LOSOption == CConst::BldgDataReqTxLOSOption) || (_winner2LOSOption == CConst::BldgDataReqTxRxLOSOption);
				bool reqRx = (_winner2LOSOption == CConst::BldgDataReqRxLOSOption) || (_winner2LOSOption == CConst::BldgDataReqTxRxLOSOption);

				if (    ((!reqTx) || (txHeightSource == CConst::lidarHeightSource))
						&& ((!reqRx) || (rxHeightSource == CConst::lidarHeightSource)) ) {
					int numPts = std::min(((int)floor(distKm*1000 / _itmMinSpacing)) + 1, _itmMaxNumPts);
					bool losFlag = UlsMeasurementAnalysis::isLOS(_terrainDataModel,
							QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
							QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
							distKm, numPts, isLOSProfilePtr);
					winner2LOSValue = (losFlag ? 1 : 2);
				}
			} else if (_winner2LOSOption == CConst::ForceLOSLOSOption) {
				winner2LOSValue = 1;
			} else if (_winner2LOSOption == CConst::ForceNLOSLOSOption) {
				winner2LOSValue = 2;
			}

			double sigma, probLOS;
			if (propEnv == CConst::urbanPropEnv) {
				// Winner2 C2: urban
				pathLoss = Winner2_C2urban(1000*win2DistKm, rxHeightM, txHeightM, frequency, fixedProbFlag, sigma, pathLossModelStr, pathLossCDF, probLOS, winner2LOSValue);
			} else if (propEnv == CConst::suburbanPropEnv) {
				// Winner2 C1: suburban
				pathLoss = Winner2_C1suburban(1000*win2DistKm, rxHeightM, txHeightM, frequency, fixedProbFlag, sigma, pathLossModelStr, pathLossCDF, probLOS, winner2LOSValue);
			} else if ( (propEnv == CConst::ruralPropEnv) || (propEnv == CConst::barrenPropEnv) ) {
				// Winner2 D1: rural
				pathLoss = Winner2_D1rural(1000*win2DistKm, rxHeightM, txHeightM, frequency, fixedProbFlag, sigma, pathLossModelStr, pathLossCDF, probLOS, winner2LOSValue);
			} else {
				throw std::runtime_error(ErrStream() << "ERROR: propEnv = " << propEnv << " INVALID value");
			}
			pathClutterTxModelStr = "NONE";
			pathClutterTxDB = 0.0;
			pathClutterTxCDF = 0.5;
		} else {
			bool rlanHasClutter;
			switch(_rlanITMTxClutterMethod) {
				case CConst::ForceTrueITMClutterMethod:
					rlanHasClutter = true;
					break;
				case CConst::ForceFalseITMClutterMethod:
					rlanHasClutter = false;
					break;
				case CConst::BldgDataITMCLutterMethod:
					{
						int numPts = std::min(((int)floor(distKm*1000 / _itmMinSpacing)) + 1, _itmMaxNumPts);
						bool losFlag = UlsMeasurementAnalysis::isLOS(_terrainDataModel,
								QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
								QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
								distKm, numPts, isLOSProfilePtr);
						rlanHasClutter = !losFlag;
					}
					break;
			}

			if ((propEnv == CConst::urbanPropEnv) || (propEnv == CConst::suburbanPropEnv)) {
				// Terrain propagation: SRTM + ITM
				double frequencyMHz = 1.0e-6*frequency;
				int numPts = std::min(((int)floor(distKm*1000 / _itmMinSpacing)) + 1, _itmMaxNumPts);
				int radioClimate    = _ituData->getRadioClimateValue(txLatitudeDeg, txLongitudeDeg);
				int radioClimateTmp = _ituData->getRadioClimateValue(rxLatitudeDeg, rxLongitudeDeg);
				if (radioClimateTmp < radioClimate) {
					radioClimate = radioClimateTmp;
				}
				double surfaceRefractivity = _ituData->getSurfaceRefractivityValue((txLatitudeDeg+rxLatitudeDeg)/2, (txLongitudeDeg+rxLongitudeDeg)/2);
				double u = _confidenceITM;
				pathLoss = UlsMeasurementAnalysis::runPointToPoint(_terrainDataModel,
						false,
						QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
						QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
						distKm, _itmEpsDielect, _itmSgmConductivity, surfaceRefractivity, frequencyMHz, radioClimate, _itmPolarization, u, _reliabilityITM, numPts, NULL, ITMProfilePtr);
				pathLossModelStr = "ITM";
				pathLossCDF = _confidenceITM;

				if (rlanHasClutter) {
					// ITU-R P.[CLUTTER] sec 3.2
					double Ll = 23.5 + 9.6 * log(frequencyGHz) / log(10.0);
					double Ls = 32.98 + 23.9 * log(distKm) / log(10.0) + 3.0 * log(frequencyGHz) / log(10.0);

					arma::vec gauss(1);
					if (fixedProbFlag) {
						gauss[0] = _zclutter2108;
					} else {
						gauss = arma::randn(1);
					}

					double Lctt = -5.0 * log(exp(-0.2 * Ll * log(10.0)) + exp(-0.2 * Ls * log(10.0))) / log(10.0) + 6.0 * gauss[0];

					pathClutterTxDB = Lctt;
					pathClutterTxModelStr = "P.2108";
					pathClutterTxCDF = q(-gauss[0]);
				} else {
					pathClutterTxModelStr = "NONE";
					pathClutterTxDB = 0.0;
					pathClutterTxCDF = 0.5;
				}

			} else if ( (propEnv == CConst::ruralPropEnv) || (propEnv == CConst::barrenPropEnv) ) {
				// Terrain propagation: SRTM + ITM
				double frequencyMHz = 1.0e-6*frequency;
				double u = _confidenceITM;
				int numPts = std::min(((int)floor(distKm*1000 / _itmMinSpacing)) + 1, _itmMaxNumPts);
				int radioClimate	= _ituData->getRadioClimateValue(txLatitudeDeg, txLongitudeDeg);
				int radioClimateTmp = _ituData->getRadioClimateValue(rxLatitudeDeg, rxLongitudeDeg);
				if (radioClimateTmp < radioClimate) {
					radioClimate = radioClimateTmp;
				}
				double surfaceRefractivity = _ituData->getSurfaceRefractivityValue((txLatitudeDeg+rxLatitudeDeg)/2, (txLongitudeDeg+rxLongitudeDeg)/2);
				pathLoss = UlsMeasurementAnalysis::runPointToPoint(_terrainDataModel,
						false,
						QPointF(txLatitudeDeg, txLongitudeDeg), txHeightM,
						QPointF(rxLatitudeDeg, rxLongitudeDeg), rxHeightM,
						distKm, _itmEpsDielect, _itmSgmConductivity, surfaceRefractivity, frequencyMHz, radioClimate, _itmPolarization, u, _reliabilityITM, numPts, NULL, ITMProfilePtr);
				pathLossModelStr = "ITM";
				pathLossCDF = _confidenceITM;

				if ( (rlanHasClutter) && (nlcdLandCatTx == CConst::noClutterNLCDLandCat) ) {
					rlanHasClutter = false;
				}

				if (rlanHasClutter) {
					double ha, dk;
					switch(nlcdLandCatTx) {
						case CConst::deciduousTreesNLCDLandCat:
							ha = 15.0;
							dk = 0.05;
							if (txClutterStrPtr) { *txClutterStrPtr = "DECIDUOUS_TREES"; }
							break;
						case CConst::coniferousTreesNLCDLandCat:
							ha = 20.0;
							dk = 0.05;
							if (txClutterStrPtr) { *txClutterStrPtr = "CONIFEROUS_TREES"; }
							break;
						case CConst::highCropFieldsNLCDLandCat:
							ha = 4.0;
							dk = 0.1;
							if (txClutterStrPtr) { *txClutterStrPtr = "HIGH_CROP_FIELDS"; }
							break;
						case CConst::villageCenterNLCDLandCat:
						case CConst::unknownNLCDLandCat:
							ha = 5.0;
							dk = 0.07;
							if (txClutterStrPtr) { *txClutterStrPtr = "VILLAGE_CENTER"; }
							break;
						default:
							CORE_DUMP;
							break;
					}

					if (distKm < 10*dk) {
						pathClutterTxDB = 0.0;
					} else {
						double elevationAngleThresholdDeg = std::atan((ha-txHeightM)/(dk*1000.0))*180.0/M_PI;
						if (elevationAngleTxDeg > elevationAngleThresholdDeg) {
							pathClutterTxDB = 0.0;
						} else {
							const double Ffc = 0.25 + 0.375  * (1 + std::tanh(7.5 * (frequencyGHz - 0.5)));
							double result = 10.25 * Ffc * exp(-1 * dk);
							result *= 1 - std::tanh(6 * (txHeightM / ha - 0.625));
							result -= 0.33;
							pathClutterTxDB = result;
						}
					}

					pathClutterTxModelStr = "452_NLCD";
					pathClutterTxCDF = 0.5;
				} else {
					pathClutterTxModelStr = "NONE";
					pathClutterTxDB = 0.0;
					pathClutterTxCDF = 0.5;
				}
			} else {
				CORE_DUMP;
			}
		}

		if (_applyClutterFSRxFlag && (rxHeightM <= _maxFsAglHeight) && (distKm >= 1.0)) {
			if (distKm * 1000 < _closeInDist) {
				pathClutterRxDB = 0.0;
				pathClutterRxModelStr = "NONE";
				pathClutterRxCDF = 0.5;
			} else if ((propEnvRx == CConst::urbanPropEnv) || (propEnvRx == CConst::suburbanPropEnv)) {
				// ITU-R P.[CLUTTER] sec 3.2
				double Ll = 23.5 + 9.6 * log(frequencyGHz) / log(10.0);
				double Ls = 32.98 + 23.9 * log(distKm) / log(10.0) + 3.0 * log(frequencyGHz) / log(10.0);

				arma::vec gauss(1);
				if (fixedProbFlag)
				{
					gauss[0] = _fsZclutter2108;
				}
				else
				{
					gauss = arma::randn(1);
				}

				double Lctt = -5.0 * log(exp(-0.2 * Ll * log(10.0)) + exp(-0.2 * Ls * log(10.0))) / log(10.0) + 6.0 * gauss[0];

				pathClutterRxDB = Lctt;
				pathClutterRxModelStr = "P.2108";
				pathClutterRxCDF = q(-gauss[0]);
			} else if ( (propEnvRx == CConst::ruralPropEnv) || (propEnvRx == CConst::barrenPropEnv) ) {
				bool allowRuralFSClutterFlag = false;
				bool clutterFlag = allowRuralFSClutterFlag && (nlcdLandCatRx == CConst::noClutterNLCDLandCat ? false : true);

				if (clutterFlag) {
					double ha, dk;
					switch(nlcdLandCatRx) {
						case CConst::deciduousTreesNLCDLandCat:
							ha = 15.0;
							dk = 0.05;
							if (rxClutterStrPtr) { *rxClutterStrPtr = "DECIDUOUS_TREES"; }
							break;
						case CConst::coniferousTreesNLCDLandCat:
							ha = 20.0;
							dk = 0.05;
							if (rxClutterStrPtr) { *rxClutterStrPtr = "CONIFEROUS_TREES"; }
							break;
						case CConst::highCropFieldsNLCDLandCat:
							ha = 4.0;
							dk = 0.1;
							if (rxClutterStrPtr) { *rxClutterStrPtr = "HIGH_CROP_FIELDS"; }
							break;
						case CConst::villageCenterNLCDLandCat:
						case CConst::unknownNLCDLandCat:
							ha = 5.0;
							dk = 0.07;
							if (rxClutterStrPtr) { *rxClutterStrPtr = "VILLAGE_CENTER"; }
							break;
						default:
							CORE_DUMP;
							break;
					}

					if (distKm < 10*dk) {
						pathClutterRxDB = 0.0;
					} else {
						double elevationAngleThresholdDeg = std::atan((ha-rxHeightM)/(dk*1000.0))*180.0/M_PI;
						if (elevationAngleRxDeg > elevationAngleThresholdDeg) {
							pathClutterRxDB = 0.0;
						} else {
							const double Ffc = 0.25 + 0.375  * (1 + std::tanh(7.5 * (frequencyGHz - 0.5)));
							double result = 10.25 * Ffc * exp(-1 * dk);
							result *= 1 - std::tanh(6 * (rxHeightM / ha - 0.625));
							result -= 0.33;
							pathClutterRxDB = result;
						}
					}

					pathClutterRxModelStr = "452_NLCD";
					pathClutterRxCDF = 0.5;
				} else {
					pathClutterRxDB = 0.0;
					pathClutterRxModelStr = "NONE";
					pathClutterRxCDF = 0.5;
				}
			} else {
				throw std::runtime_error(ErrStream() << "ERROR: Invalid morphology for location " << rxLongitudeDeg << " " << rxLatitudeDeg);
			}
		} else {
			pathClutterRxDB = 0.0;
			pathClutterRxModelStr = "NONE";
			pathClutterRxCDF = 0.5;
		}
	} else if (pathLossModel == CConst::FSPLPathLossModel) {
		pathLoss = 20.0 * log((4 * M_PI * frequency * fsplDistKm * 1000) / CConst::c) / log(10.0);
		pathLossModelStr = "FSPL";
		pathLossCDF = 0.5;

		pathClutterTxDB = 0.0;
		pathClutterTxModelStr = "NONE";
		pathClutterTxCDF = 0.5;

		pathClutterRxDB = 0.0;
		pathClutterRxModelStr = "NONE";
		pathClutterRxCDF = 0.5;
	} else {
		throw std::runtime_error(ErrStream() << "ERROR reading ULS data: pathLossModel = " << pathLossModel << " INVALID value");
	}

	if (_pathLossClampFSPL) {
		double fspl = 20.0 * log((4 * M_PI * frequency * fsplDistKm * 1000) / CConst::c) / log(10.0);
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
double AfcManager::Winner2_C1suburban(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double& sigma, std::string &pathLossModelStr, double &pathLossCDF, double &probLOS, int losValue) const
{
	double retval;

	if ((losValue == 0) && (_closeInHgtFlag) && (hMS > _closeInHgtLOS)) {
		losValue = 1;
		probLOS = 1.0;
	} else {
		probLOS = exp(-distance / 200);
	}

	if (losValue == 0) {
		if (_winner2UnknownLOSMethod == CConst::PLOSCombineWinner2UnknownLOSMethod) {
			double sigmaLOS, sigmaNLOS;
			double plLOS  = Winner2_C1suburban_LOS( distance, hBS, hMS, frequency, true, 0.0, sigmaLOS, pathLossCDF);
			double plNLOS = Winner2_C1suburban_NLOS(distance, hBS, hMS, frequency, true, 0.0, sigmaNLOS, pathLossCDF);
			retval = probLOS*plLOS + (1.0-probLOS)*plNLOS;
			sigma = sqrt(probLOS*probLOS*sigmaLOS*sigmaLOS + (1.0-probLOS)*(1.0-probLOS)*sigmaNLOS*sigmaNLOS);

			arma::vec gauss(1);
			if (fixedProbFlag)
			{
				gauss[0] = _zwinner2Combined;
			}
			else
			{
				gauss = arma::randn(1);
			}

			retval += sigma*gauss[0];
			pathLossCDF = q(-(gauss[0]));

			pathLossModelStr = "W2C1_SUBURBAN_COMB";
		} else if (_winner2UnknownLOSMethod == CConst::PLOSThresholdWinner2UnknownLOSMethod) {
			if (probLOS > _winner2ProbLOSThr)
			{
				retval = Winner2_C1suburban_LOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2LOS, sigma, pathLossCDF);
				pathLossModelStr = "W2C1_SUBURBAN_LOS";
			}
			else
			{
				retval = Winner2_C1suburban_NLOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2NLOS, sigma, pathLossCDF);
				pathLossModelStr = "W2C1_SUBURBAN_NLOS";
			}
		} else {
			CORE_DUMP;
		}
	} else if (losValue == 1) {
		retval = Winner2_C1suburban_LOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2LOS, sigma, pathLossCDF);
		pathLossModelStr = "W2C1_SUBURBAN_LOS";
	} else if (losValue == 2) {
		retval = Winner2_C1suburban_NLOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2NLOS, sigma, pathLossCDF);
		pathLossModelStr = "W2C1_SUBURBAN_NLOS";
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
double AfcManager::Winner2_C2urban(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double& sigma, std::string &pathLossModelStr, double &pathLossCDF, double &probLOS, int losValue) const
{
	double retval;

	if ((losValue == 0) && (_closeInHgtFlag) && (hMS > _closeInHgtLOS)) {
		losValue = 1;
		probLOS = 1.0;
	} else {
		probLOS = (distance > 18.0 ? 18.0 / distance : 1.0) * (1.0 - exp(-distance / 63.0)) + exp(-distance / 63.0);
	}

	if (losValue == 0) {
		if (_winner2UnknownLOSMethod == CConst::PLOSCombineWinner2UnknownLOSMethod) {
			double sigmaLOS, sigmaNLOS;
			double plLOS  = Winner2_C2urban_LOS( distance, hBS, hMS, frequency, true, 0.0, sigmaLOS, pathLossCDF);
			double plNLOS = Winner2_C2urban_NLOS(distance, hBS, hMS, frequency, true, 0.0, sigmaNLOS, pathLossCDF);
			retval = probLOS*plLOS + (1.0-probLOS)*plNLOS;
			sigma = sqrt(probLOS*probLOS*sigmaLOS*sigmaLOS + (1.0-probLOS)*(1.0-probLOS)*sigmaNLOS*sigmaNLOS);

			arma::vec gauss(1);
			if (fixedProbFlag)
			{
				gauss[0] = _zwinner2Combined;
			} else {
				gauss = arma::randn(1);
			}

			retval += sigma*gauss[0];
			pathLossCDF = q(-(gauss[0]));

			pathLossModelStr = "W2C2_URBAN_COMB";
		} else if (_winner2UnknownLOSMethod == CConst::PLOSThresholdWinner2UnknownLOSMethod) {
			if (probLOS > _winner2ProbLOSThr)
			{
				retval = Winner2_C2urban_LOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2LOS, sigma, pathLossCDF);
				pathLossModelStr = "W2C2_URBAN_LOS";
			}
			else
			{
				retval = Winner2_C2urban_NLOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2NLOS, sigma, pathLossCDF);
				pathLossModelStr = "W2C2_URBAN_NLOS";
			}
		} else {
			CORE_DUMP;
		}
	} else if (losValue == 1) {
		retval = Winner2_C2urban_LOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2LOS, sigma, pathLossCDF);
		pathLossModelStr = "W2C2_URBAN_LOS";
	} else if (losValue == 2) {
		retval = Winner2_C2urban_NLOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2NLOS, sigma, pathLossCDF);
		pathLossModelStr = "W2C2_URBAN_NLOS";
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
double AfcManager::Winner2_D1rural(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double& sigma, std::string &pathLossModelStr, double &pathLossCDF, double &probLOS, int losValue) const
{
	double retval;

	if ((losValue == 0) && (_closeInHgtFlag) && (hMS > _closeInHgtLOS)) {
		losValue = 1;
		probLOS = 1.0;
	} else {
		probLOS = exp(-distance / 1000);
	}

	if (losValue == 0) {
		if (_winner2UnknownLOSMethod == CConst::PLOSCombineWinner2UnknownLOSMethod) {
			double sigmaLOS, sigmaNLOS;
			double plLOS  = Winner2_D1rural_LOS( distance, hBS, hMS, frequency, true, 0.0, sigmaLOS, pathLossCDF);
			double plNLOS = Winner2_D1rural_NLOS(distance, hBS, hMS, frequency, true, 0.0, sigmaNLOS, pathLossCDF);
			retval = probLOS*plLOS + (1.0-probLOS)*plNLOS;
			sigma = sqrt(probLOS*probLOS*sigmaLOS*sigmaLOS + (1.0-probLOS)*(1.0-probLOS)*sigmaNLOS*sigmaNLOS);

			arma::vec gauss(1);
			if (fixedProbFlag)
			{
				gauss[0] = _zwinner2Combined;
			} else {
				gauss = arma::randn(1);
			}

			retval += sigma*gauss[0];
			pathLossCDF = q(-(gauss[0]));

			pathLossModelStr = "W2D1_RURAL_COMB";
		} else if (_winner2UnknownLOSMethod == CConst::PLOSThresholdWinner2UnknownLOSMethod) {

			if (probLOS > _winner2ProbLOSThr)
			{
				retval = Winner2_D1rural_LOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2LOS, sigma, pathLossCDF);
				pathLossModelStr = "W2D1_RURAL_LOS";
			}
			else
			{
				retval = Winner2_D1rural_NLOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2NLOS, sigma, pathLossCDF);
				pathLossModelStr = "W2D1_RURAL_NLOS";
			}
		} else {
			CORE_DUMP;
		}
	} else if (losValue == 1) {
		retval = Winner2_D1rural_LOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2LOS, sigma, pathLossCDF);
		pathLossModelStr = "W2D1_RURAL_LOS";
	} else if (losValue == 2) {
		retval = Winner2_D1rural_NLOS(distance, hBS, hMS, frequency, fixedProbFlag, _zwinner2NLOS, sigma, pathLossCDF);
		pathLossModelStr = "W2D1_RURAL_NLOS";
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
	if (_responseCode != CConst::successResponseCode) {
		return;
	}

	// initialize all channels to max EIRP before computing
	for (auto& channel : _channelList)
		channel.eirpLimit_dBm = _maxEIRP_dBm;

	if (_analysisType == "AP-AFC") {
		runPointAnalysis();
	} else if (_analysisType == "ScanAnalysis") {
		runScanAnalysis();
	} else if (_analysisType == "ExclusionZoneAnalysis") {
		runExclusionZoneAnalysis();
	} else if (_analysisType == "HeatmapAnalysis") {
		runHeatmapAnalysis();
#if DEBUG_AFC
	} else if (_analysisType == "test_winner2") {
		runTestWinner2("w2_alignment.csv", "w2_alignment_afc.csv");
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
	} else if (_analysisType == "ANALYZE_NLCD") {
		runAnalyzeNLCD();
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

#if DEBUG_AFC
	// std::vector<int> fsidTraceList{2128, 3198, 82443};
	// std::vector<int> fsidTraceList{64324};
	std::vector<int> fsidTraceList{93911};
	std::string pathTraceFile = "path_trace.csv.gz";
#endif

	LOGGER_INFO(logger) << "Executing AfcManager::runPointAnalysis()";

	_rlanRegion->configure(_rlanHeightType, _terrainDataModel);

	// Sorting by distance from AP
	double cosLatSq = cos(_rlanRegion->getCenterLatitude() / 180.0 * M_PI);
	cosLatSq *= cosLatSq;
	_ulsList->sort(
		[this, cosLatSq](ULSClass *const &l, ULSClass *const &r) {
			double latDistL = l->getRxLatitudeDeg() - _rlanRegion->getCenterLatitude();
			double latDistR = r->getRxLatitudeDeg() - _rlanRegion->getCenterLatitude();
			double lonDistL = l->getRxLongitudeDeg() - _rlanRegion->getCenterLongitude();
			double lonDistR = r->getRxLongitudeDeg() - _rlanRegion->getCenterLongitude();
			return (latDistL * latDistL + lonDistL * lonDistL * cosLatSq) <
				(latDistR * latDistR + lonDistR * lonDistR * cosLatSq);
		});

	double heightUncertainty = _rlanRegion->getHeightUncertainty();
	int NHt = (int) ceil(heightUncertainty / _scanres_ht);
	Vector3 rlanPosnList[2*NHt+1];
	GeodeticCoord rlanCoordList[2*NHt+1];

	/**************************************************************************************/
	/* Get Uncertainty Region Scan Points                                                 */
	/**************************************************************************************/
	std::vector<LatLon> scanPointList = _rlanRegion->getScan(_scanRegionMethod, _scanres_xy, _scanres_points_per_degree);
	/**************************************************************************************/

	/**************************************************************************************/
	/* Create excThrFile, useful for debugging                                            */
	/**************************************************************************************/
	auto excthr_writer = GzipCsvWriter(_excThrFile);
	auto &fexcthrwifi = excthr_writer.csv_writer;

	if (fexcthrwifi) {
		fexcthrwifi->writeRow({ "FS_ID", "DBNAME",
				"RLAN_POSN_IDX",
				"CALLSIGN",
				"FS_RX_LONGITUDE (deg)",
				"FS_RX_LATITUDE (deg)",
				"FS_RX_HEIGHT_ABOVE_TERRAIN (m)",
				"FS_RX_TERRAIN_HEIGHT (m)",
				"FS_RX_TERRAIN_SOURCE",
				"FS_RX_PROP_ENV",
				"NUM_PASSIVE_REPEATER",
				"IS_DIVERSITY_LINK",
				"SEGMENT_IDX",
				"SEGMENT_RX_LONGITUDE (deg)",
				"SEGMENT_RX_LATITUDE (deg)",
				"RLAN_LONGITUDE (deg)",
				"RLAN_LATITUDE (deg)",
				"RLAN_HEIGHT_ABOVE_TERRAIN (m)",
				"RLAN_TERRAIN_HEIGHT (m)",
				"RLAN_TERRAIN_SOURCE",
				"RLAN_PROP_ENV",
				"RLAN_FS_RX_DIST (km)",
				"RLAN_FS_RX_GROUND_DIST (km)",
				"RLAN_FS_RX_ELEVATION_ANGLE (deg)",
				"FS_RX_ANGLE_OFF_BORESIGHT (deg)",
				"RLAN_TX_EIRP (dBm)",
				"BODY_LOSS (dB)",
				"RLAN_CLUTTER_CATEGORY",
				"FS_CLUTTER_CATEGORY",
				"BUILDING TYPE",
				"RLAN_FS_RX_BUILDING_PENETRATION (dB)",
				"BUILDING_PENETRATION_MODEL",
				"BUILDING_PENETRATION_CDF",
				"PATH_LOSS (dB)",
				"PATH_LOSS_MODEL",
				"PATH_LOSS_CDF",
				"PATH_CLUTTER_TX (DB)",
				"PATH_CLUTTER_TX_MODEL",
				"PATH_CLUTTER_TX_CDF",
				"PATH_CLUTTER_RX (DB)",
				"PATH_CLUTTER_RX_MODEL",
				"PATH_CLUTTER_RX_CDF",
				"RLAN BANDWIDTH (MHz)",
				"RLAN CHANNEL START FREQ (MHz)",
				"RLAN CHANNEL STOP FREQ (MHz)",
				"ULS START FREQ (MHz)",
				"ULS STOP FREQ (MHz)",
				"FS_ANT_TYPE",
				"FS_ANT_CATEGORY",
				"FS_ANT_GAIN_PEAK (dB)",
				"PR_TYPE (dB)",
				"PR_EFFECTIVE_GAIN (dB)",
				"PR_DISCRIMINATION_GAIN (dB)",
				"FS_ANT_GAIN_TO_RLAN (dB)",
				"FS_ANT_NEAR_FIELD_XDB",
				"FS_ANT_NEAR_FIELD_U",
				"FS_ANT_NEAR_FIELD_EFF",
				"FS_ANT_NEAR_FIELD_OFFSET (dB)",
				"RX_SPECTRAL_OVERLAP_LOSS (dB)",
				"POLARIZATION_LOSS (dB)",
				"FS_RX_FEEDER_LOSS (dB)",
				"FS_RX_PWR (dBW)",
				"FS I/N (dB)",
				"EIRP_LIMIT (dBm)",
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
	/* Create _fsAnalysisListFile                                                         */
	/**************************************************************************************/
	FILE *fFSList;
	if (_fsAnalysisListFile.empty()) {
		fFSList = (FILE *) NULL;
	} else {
		if ( !(fFSList = fopen(_fsAnalysisListFile.c_str(), "wb")) ) {
			errStr << std::string("ERROR: Unable to open fsAnalysisListFile \"") + _fsAnalysisListFile + std::string("\"\n");
			throw std::runtime_error(errStr.str());
		}
	}
	if (fFSList) {
		fprintf(fFSList,   "FSID"
			",RX_CALLSIGN"
			",TX_CALLSIGN"
			",FS_START_FREQ (MHz)"
			",FS_STOP_FREQ (MHz)"
			",FS_RX_LONGITUDE (deg)"
			",FS_RX_LATITUDE (deg)"
			",FS_RX_HEIGHT_AGL (m)"
			",FS_RX_RLAN_DIST (m)"
			"\n");
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

		std::vector<GeodeticCoord> ptList = _rlanRegion->getBoundary(_terrainDataModel);

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

		/**********************************************************************************/
		/* Scan Points                                                                    */
		/**********************************************************************************/
		fkml->writeStartElement("Folder");
		fkml->writeTextElement("name", "SCAN POINTS");

		for(ptIdx=0; ptIdx<(int) scanPointList.size(); ptIdx++) {
			LatLon scanPt = scanPointList[ptIdx];

			double rlanTerrainHeight, bldgHeight;
			MultibandRasterClass::HeightResult lidarHeightResult;
			CConst::HeightSourceEnum rlanHeightSource;
			_terrainDataModel->getTerrainHeight(scanPt.second, scanPt.first, rlanTerrainHeight,bldgHeight, lidarHeightResult, rlanHeightSource);

			double height0;
			if (_rlanRegion->getFixedHeightAMSL()) {
				height0 = _rlanRegion->getCenterHeightAMSL();
			} else {
				height0 = _rlanRegion->getCenterHeightAMSL() - _rlanRegion->getCenterTerrainHeight() + rlanTerrainHeight;
			}

			int htIdx;
			bool lowHeightFlag = false;
			for(htIdx=0; (htIdx<=2*NHt)&&(!lowHeightFlag); ++htIdx) {
				double heightAMSL = height0 + (NHt ? (NHt - htIdx)*heightUncertainty/NHt : 0.0); // scan from top down
				double heightAGL  = heightAMSL - rlanTerrainHeight;
				bool useFlag;
				if (heightAGL < _minRlanHeightAboveTerrain) {
					switch(_scanPointBelowGroundMethod) {
						case CConst::DiscardScanPointBelowGroundMethod:
							useFlag = false;
							break;
						case CConst::TruncateScanPointBelowGroundMethod:
							heightAMSL = rlanTerrainHeight + _minRlanHeightAboveTerrain;
							useFlag = true;
							break;
						default: CORE_DUMP; break;
					}
					lowHeightFlag = true;
				} else {
					useFlag = true;
				}

				if (useFlag) {
					fkml->writeStartElement("Placemark");
					fkml->writeTextElement("name", QString::asprintf("SCAN_POINT_%d_%d", ptIdx, htIdx));
					fkml->writeTextElement("visibility", "1");
					fkml->writeTextElement("styleUrl", "#dotStyle");
					fkml->writeStartElement("Point");
					fkml->writeTextElement("altitudeMode", "absolute");
					fkml->writeTextElement("coordinates", QString::asprintf("%.10f,%.10f,%.2f", scanPt.second, scanPt.first, heightAMSL));
					fkml->writeEndElement(); // Point
					fkml->writeEndElement(); // Placemark
				}
			}
		}

		fkml->writeEndElement(); // Scan Points
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

#if DEBUG_AFC
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
	const double exclusionDistKmSquared = (_exclusionDist / 1000.0) * (_exclusionDist / 1000.0);
	const double maxRadiusKmSquared = (_maxRadius / 1000.0) * (_maxRadius / 1000.0);

	if (_rlanRegion->getMaxHeightAGL() < _minRlanHeightAboveTerrain) {
		LOGGER_WARN(logger) << std::string("ERROR: Point Analysis: Invalid RLAN parameter settings.") << std::endl
			<< std::string("RLAN uncertainty region has a max AGL height of ") << _rlanRegion->getMaxHeightAGL()
			<< std::string(", which is < ") << _minRlanHeightAboveTerrain << std::endl;
		_responseCode = CConst::invalidValueResponseCode;
		_invalidParams << "heightType" << "height" << "verticalUncertainty";
		return;
	}

#   if DEBUG_AFC
	char *tstr;

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
					bool hasOverlap = computeSpectralOverlapLoss((double *) NULL, chanStartFreq, chanStopFreq, ras->getStartFreq(), ras->getStopFreq(), false, CConst::psdSpectralAlgorithm);
					if (hasOverlap) {
						channel->availability = BLACK;
						channel->eirpLimit_dBm = -std::numeric_limits<double>::infinity();
					}
				}
			}
		}
	}

#   if DEBUG_AFC
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

#   if DEBUG_AFC
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

#if 0
		// For debugging, identifies anomalous ULS entries
		if (uls->getLinkDistance() == -1) {
			std::cout << uls->getID() << std::endl;
		}
#endif
		if (uls->getLinkDistance() > 0.0) {

			int numPR = uls->getNumPR();
			int numDiversity = (uls->getHasDiversity() ? 2 : 1);

			int segStart = (_passiveRepeaterFlag ? 0 : numPR);

			for(int segIdx=segStart; segIdx<numPR+1; ++segIdx) {
			for(int divIdx=0; divIdx<numDiversity; ++divIdx) {
				Vector3 ulsRxPos = (segIdx == numPR ? (divIdx == 0 ? uls->getRxPosition() : uls->getDiversityPosition()) : uls->getPR(segIdx).position);
				double ulsRxLongitude = (segIdx == numPR ? uls->getRxLongitudeDeg() : uls->getPR(segIdx).longitudeDeg);
				double ulsRxLatitude = (segIdx == numPR ? uls->getRxLatitudeDeg() : uls->getPR(segIdx).latitudeDeg);

				Vector3 lineOfSightVectorKm = ulsRxPos - _rlanRegion->getCenterPosn();
				double distKmSquared = (lineOfSightVectorKm).dot(lineOfSightVectorKm);

				if (distKmSquared < maxRadiusKmSquared) {
#					if DEBUG_AFC
					time_t t1 = time(NULL);

					bool traceFlag = false;
					if (traceIdx<(int) fsidTraceList.size()) {
						if (uls->getID() == fsidTraceList[traceIdx]) {
							traceFlag = true;
						}
					}
#					endif

					_ulsIdxList.push_back(ulsIdx); // Store the ULS indices that are used in analysis

					double ulsRxHeightAGL  = (segIdx == numPR ? (divIdx == 0 ? uls->getRxHeightAboveTerrain() : uls->getDiversityHeightAboveTerrain()) : uls->getPR(segIdx).heightAboveTerrain);
					double ulsRxHeightAMSL = (segIdx == numPR ? (divIdx == 0 ? uls->getRxHeightAMSL() : uls->getDiversityHeightAMSL()) : uls->getPR(segIdx).heightAMSL);

					/**************************************************************************************/
					/* Determine propagation environment of FS segment RX, if needed.                     */
					/**************************************************************************************/
					char ulsRxPropEnv = ' ';
					CConst::NLCDLandCatEnum nlcdLandCatRx;
					CConst::PropEnvEnum fsPropEnv;
					if ((_applyClutterFSRxFlag) && (ulsRxHeightAGL <= _maxFsAglHeight)) {
						fsPropEnv = computePropEnv(ulsRxLongitude, ulsRxLatitude, nlcdLandCatRx);
						switch(fsPropEnv) {
							case CConst::urbanPropEnv:    ulsRxPropEnv = 'U'; break;
							case CConst::suburbanPropEnv: ulsRxPropEnv = 'S'; break;
							case CConst::ruralPropEnv:    ulsRxPropEnv = 'R'; break;
							case CConst::barrenPropEnv:   ulsRxPropEnv = 'B'; break;
							case CConst::unknownPropEnv:  ulsRxPropEnv = 'X'; break;
							default: CORE_DUMP;
						}
					} else {
						fsPropEnv = CConst::unknownPropEnv;
						ulsRxPropEnv = ' ';
					}
					/**************************************************************************************/

					LatLon ulsRxLatLon = std::pair<double, double>(ulsRxLatitude, ulsRxLongitude);
					bool contains2D, contains3D;

					// If contains2D is set, FS lon/lat is inside 2D-uncertainty region, depending on height may be above, below, or actually inside uncertainty region.
					// If contains3D is set, FS is inside 3D-uncertainty
					_rlanRegion->closestPoint(ulsRxLatLon, contains2D);

					contains3D = false;
					if (contains2D) {
						int scanPtIdx;
						for(scanPtIdx=0; (scanPtIdx<(int) scanPointList.size())&&(!contains3D); scanPtIdx++) {
							LatLon scanPt = scanPointList[scanPtIdx];

							double rlanTerrainHeight, bldgHeight;
							MultibandRasterClass::HeightResult lidarHeightResult;
							CConst::HeightSourceEnum rlanHeightSource;
							_terrainDataModel->getTerrainHeight(scanPt.second, scanPt.first, rlanTerrainHeight, bldgHeight, lidarHeightResult, rlanHeightSource);

							double height0;
							if (_rlanRegion->getFixedHeightAMSL()) {
								height0 = _rlanRegion->getCenterHeightAMSL();
							} else {
								height0 = _rlanRegion->getCenterHeightAMSL() - _rlanRegion->getCenterTerrainHeight() + rlanTerrainHeight;
							}
							double minHeight = height0 - heightUncertainty;
							double maxHeight = height0 + heightUncertainty;
							if ((ulsRxHeightAMSL >= minHeight) && (ulsRxHeightAMSL <= maxHeight)) {
								contains3D = true;
							}
						}
					}

					double minRLANDist = -1.0;
					if (distKmSquared <= exclusionDistKmSquared) {
						LOGGER_INFO(logger) << "FSID = " << uls->getID() << (segIdx == numPR ? " RX" : " PR " + std::to_string(segIdx)) << " is inside exclusion distance.";
						minRLANDist = sqrt(distKmSquared)*1000.0;
					} else if ((contains3D) && (!_allowScanPtsInUncRegFlag)) {
						int chanIdx;
						for (chanIdx = 0; chanIdx < (int) _channelList.size(); ++chanIdx) {
							ChannelStruct *channel = &(_channelList[chanIdx]);
							if (channel->availability != BLACK) {
								double chanStartFreq = channel->startFreqMHz * 1.0e6;
								double chanStopFreq = channel->stopFreqMHz * 1.0e6;
								bool hasOverlap = computeSpectralOverlapLoss((double *) NULL, chanStartFreq, chanStopFreq, uls->getStartUseFreq(), uls->getStopUseFreq(), _aciFlag, CConst::psdSpectralAlgorithm);
								if (hasOverlap > 0.0) {
									double eirpLimit_dBm = -std::numeric_limits<double>::infinity();

									channel->availability = BLACK;
									channel->eirpLimit_dBm = eirpLimit_dBm;

									if ( (!ulsFlagList[ulsIdx]) || (eirpLimit_dBm < eirpLimitList[ulsIdx]) ) {
										eirpLimitList[ulsIdx] = eirpLimit_dBm;
										ulsFlagList[ulsIdx] = true;
									}
								}
							}
						}
						LOGGER_INFO(logger) << "FSID = " << uls->getID() << " is inside specified RLAN region.";
						minRLANDist = 0.0;
					} else {

						int scanPtIdx;
						for(scanPtIdx=0; scanPtIdx<(int) scanPointList.size(); scanPtIdx++) {
							LatLon scanPt = scanPointList[scanPtIdx];

							/**************************************************************************************/
							/* Determine propagation environment of RLAN using scan point                         */
							/**************************************************************************************/
							CConst::NLCDLandCatEnum nlcdLandCatTx;
							CConst::PropEnvEnum rlanPropEnv = computePropEnv(scanPt.second, scanPt.first, nlcdLandCatTx);
							/**************************************************************************************/

							double rlanTerrainHeight, bldgHeight;
							MultibandRasterClass::HeightResult lidarHeightResult;
							CConst::HeightSourceEnum rlanHeightSource;
							_terrainDataModel->getTerrainHeight(scanPt.second, scanPt.first, rlanTerrainHeight, bldgHeight, lidarHeightResult, rlanHeightSource);

							double height0;
							if (_rlanRegion->getFixedHeightAMSL()) {
								height0 = _rlanRegion->getCenterHeightAMSL();
							} else {
								height0 = _rlanRegion->getCenterHeightAMSL() - _rlanRegion->getCenterTerrainHeight() + rlanTerrainHeight;
							}

							// Use Haversine formula with average earth radius of 6371 km
							double lon1Rad = scanPt.second*M_PI/180.0;
							double lat1Rad = scanPt.first*M_PI/180.0;
							double lon2Rad = ulsRxLongitude*M_PI/180.0;
							double lat2Rad = ulsRxLatitude*M_PI/180.0;
							double slat = sin((lat2Rad-lat1Rad)/2);
							double slon = sin((lon2Rad-lon1Rad)/2);
							double groundDistanceKm = 2*CConst::averageEarthRadius*asin(sqrt(slat*slat+cos(lat1Rad)*cos(lat2Rad)*slon*slon))*1.0e-3;

							int htIdx;
							int numRlanPosn = 0;
							bool lowHeightFlag = false;
							for(htIdx=0; (htIdx<=2*NHt)&&(!lowHeightFlag); ++htIdx) {
								double heightAMSL = height0 + (NHt ? (NHt - htIdx)*heightUncertainty/NHt : 0.0); // scan from top down
								double heightAGL  = heightAMSL - rlanTerrainHeight;
								bool useFlag;
								if (heightAGL < _minRlanHeightAboveTerrain) {
									switch(_scanPointBelowGroundMethod) {
										case CConst::DiscardScanPointBelowGroundMethod:
											useFlag = false;
											break;
										case CConst::TruncateScanPointBelowGroundMethod:
											heightAMSL = rlanTerrainHeight + _minRlanHeightAboveTerrain;
											useFlag = true;
											break;
										default: CORE_DUMP; break;
									}
									lowHeightFlag = true;
								} else {
									useFlag = true;
								}
								if (useFlag) {
									rlanCoordList[htIdx] = GeodeticCoord::fromLatLon(scanPt.first, scanPt.second, heightAMSL/1000.0);
									rlanPosnList[htIdx] = EcefModel::fromGeodetic(rlanCoordList[htIdx]);
									numRlanPosn++;
								}
							}

							int rlanPosnIdx;

#if DEBUG_AFC
							if (traceFlag) {
								ftrace->writeRow({ QString::asprintf("BEGIN_%d,,,,,-1\n", uls->getID()) });
								for (rlanPosnIdx = 0; rlanPosnIdx < numRlanPosn; ++rlanPosnIdx) {
									ftrace->writeRow({ QString::asprintf("RLAN_%d,%.10f,%.10f,,%.5f,AMSL\n", rlanPosnIdx,rlanCoordList[rlanPosnIdx].longitudeDeg,rlanCoordList[rlanPosnIdx].latitudeDeg,rlanCoordList[rlanPosnIdx].heightKm*1000.0) } );
								}
								ftrace->writeRow({ QString::asprintf("FS_RX,%.10f,%.10f,,%.5f,AMSL\n", ulsRxLongitude, ulsRxLatitude, ulsRxHeightAMSL) } );
							}
#endif

							for (rlanPosnIdx = 0; rlanPosnIdx < numRlanPosn; ++rlanPosnIdx)
							{
								Vector3 rlanPosn = rlanPosnList[rlanPosnIdx];
								GeodeticCoord rlanCoord = rlanCoordList[rlanPosnIdx];
								lineOfSightVectorKm = ulsRxPos - rlanPosn;
								double distKm = lineOfSightVectorKm.len();
								double win2DistKm;
								if (_winner2UseGroundDistanceFlag) {
									win2DistKm = groundDistanceKm;
								} else {
									win2DistKm = distKm;
								}
								double fsplDistKm;
								if (_fsplUseGroundDistanceFlag) {
									fsplDistKm = groundDistanceKm;
								} else {
									fsplDistKm = distKm;
								}
								double dAP = rlanPosn.len();
								double duls = ulsRxPos.len();
								double elevationAngleTxDeg = 90.0 - acos(rlanPosn.dot(lineOfSightVectorKm)/(dAP*distKm))*180.0/M_PI;
								double elevationAngleRxDeg = 90.0 - acos(ulsRxPos.dot(-lineOfSightVectorKm)/(duls*distKm))*180.0/M_PI;

								if ((minRLANDist == -1.0) || (distKm*1000.0 < minRLANDist)) {
									minRLANDist = distKm*1000.0;
								}

								int chanIdx;
								for (chanIdx = 0; chanIdx < (int) _channelList.size(); ++chanIdx) {
									ChannelStruct *channel = &(_channelList[chanIdx]);
									if ( (channel->availability != BLACK) && (channel->availability != RED) ) {
										double chanStartFreq = channel->startFreqMHz * 1.0e6;
										double chanStopFreq = channel->stopFreqMHz * 1.0e6;
										bool useACI = (channel->type == INQUIRED_FREQUENCY ? false : _aciFlag);
										CConst::SpectralAlgorithmEnum spectralAlgorithm = (channel->type == INQUIRED_FREQUENCY ? CConst::psdSpectralAlgorithm : _channelResponseAlgorithm);
										// LOGGER_INFO(logger) << "COMPUTING SPECTRAL OVERLAP FOR FSID = " << uls->getID();
										double spectralOverlapLossDB;
										bool hasOverlap = computeSpectralOverlapLoss(&spectralOverlapLossDB, chanStartFreq, chanStopFreq, uls->getStartUseFreq(), uls->getStopUseFreq(), useACI, spectralAlgorithm);
										if (hasOverlap) {
											double bandwidth = chanStopFreq - chanStartFreq;
											double chanCenterFreq = (chanStartFreq + chanStopFreq)/2;

											std::string buildingPenetrationModelStr;
											double buildingPenetrationCDF;
											double buildingPenetrationDB = computeBuildingPenetration(_buildingType, elevationAngleTxDeg, chanCenterFreq, buildingPenetrationModelStr, buildingPenetrationCDF, true);

											std::string txClutterStr;
											std::string rxClutterStr;
											std::string pathLossModelStr;
											double pathLossCDF;
											double pathLoss;
											std::string pathClutterTxModelStr;
											double pathClutterTxCDF;
											double pathClutterTxDB;
											std::string pathClutterRxModelStr;
											double pathClutterRxCDF;
											double pathClutterRxDB;
											double rxGainDB;
											double discriminationGain;
											std::string rxAntennaSubModelStr;
											double angleOffBoresightDeg;
											double rxPowerDBW;
											double I2NDB;
											double marginDB;
											double eirpLimit_dBm;
											double nearFieldOffsetDB;
											double nearField_xdb;
											double nearField_u;
											double nearField_eff;

											double rlanHtAboveTerrain = rlanCoord.heightKm * 1000.0 - rlanTerrainHeight;

											// Trying Free Space Path Loss then (if not skipped) - configured Path Loss.
											bool skip = false;
											for (bool forceFspl : {true, false}) {
												if (forceFspl && (fsplDistKm == 0)) {
													// Possible if FSPL distance is horizontal. This should be extremely rare
													continue;
												}
												computePathLoss((forceFspl || contains2D) ? CConst::FSPLPathLossModel : _pathLossModel, rlanPropEnv, fsPropEnv, nlcdLandCatTx,
														nlcdLandCatRx, distKm, fsplDistKm, win2DistKm, chanCenterFreq,
														rlanCoord.longitudeDeg, rlanCoord.latitudeDeg, rlanHtAboveTerrain, elevationAngleTxDeg,
														ulsRxLongitude, ulsRxLatitude, ulsRxHeightAGL, elevationAngleRxDeg,
														pathLoss, pathClutterTxDB, pathClutterRxDB, true,
														pathLossModelStr, pathLossCDF,
														pathClutterTxModelStr, pathClutterTxCDF, pathClutterRxModelStr, pathClutterRxCDF,
														(iturp452::ITURP452 *)NULL, &txClutterStr, &rxClutterStr, &(uls->ITMHeightProfile), &(uls->isLOSHeightProfile)
#if DEBUG_AFC
														, uls->ITMHeightType
#endif
														);

												Vector3 ulsAntennaPointing = (segIdx == numPR ? (divIdx == 0 ? uls->getAntennaPointing() : uls->getDiversityAntennaPointing()) : uls->getPR(segIdx).pointing);
												angleOffBoresightDeg = acos(ulsAntennaPointing.dot(-(lineOfSightVectorKm.normalized()))) * 180.0 / M_PI;
												if (segIdx == numPR) {
													rxGainDB = uls->computeRxGain(angleOffBoresightDeg, elevationAngleRxDeg, chanCenterFreq, rxAntennaSubModelStr, divIdx);
												} else {
													discriminationGain = uls->getPR(segIdx).computeDiscriminationGain(angleOffBoresightDeg, elevationAngleRxDeg, chanCenterFreq);
													rxGainDB = uls->getPR(segIdx).effectiveGain - discriminationGain;
												}

												nearFieldOffsetDB = 0.0;
												nearField_xdb = std::numeric_limits<float>::quiet_NaN();
												nearField_u = std::numeric_limits<float>::quiet_NaN();
												nearField_eff = std::numeric_limits<float>::quiet_NaN();
												if (segIdx == numPR) {
													if (_nearFieldAdjFlag && (distKm*1000.0 < uls->getRxNearFieldDistLimit()) && (angleOffBoresightDeg < 90.0) ) {
														bool unii5Flag = computeSpectralOverlapLoss((double *) NULL, uls->getStartUseFreq(), uls->getStopUseFreq(), 5925.0e6, 6425.0e6, false, CConst::psdSpectralAlgorithm);
														double Fc;
														if (unii5Flag) {
															Fc = 6175.0e6;
														} else {
															Fc = 6700.0e6;
														}
														nearField_eff = uls->getRxNearFieldAntEfficiency();
														double D = uls->getRxNearFieldAntDiameter();

														nearField_xdb = 10.0*log10(CConst::c*distKm*1000.0/(2*Fc*D*D));
														nearField_u = (Fc*D*sin(angleOffBoresightDeg*M_PI/180.0)/CConst::c);

														nearFieldOffsetDB = _nfa->computeNFA(nearField_xdb, nearField_u, nearField_eff);
													}
												}

												rxPowerDBW = (_maxEIRP_dBm - 30.0) - _bodyLossDB - buildingPenetrationDB - pathLoss - pathClutterTxDB - pathClutterRxDB + rxGainDB + nearFieldOffsetDB - spectralOverlapLossDB - _polarizationLossDB - uls->getRxAntennaFeederLossDB();

												I2NDB = rxPowerDBW - uls->getNoiseLevelDBW();

												marginDB = _IoverN_threshold_dB - I2NDB;

												eirpLimit_dBm = _maxEIRP_dBm + marginDB;

												if (forceFspl) {
													// Skipping further computation if Free Space path loss
													// EIRP is larger than already established (hence
													// configured path loss will be even larger),
													// otherwise proceeding with (potentially slow)
													// configured path loss computation

													// 1dB allowance to accommodate for amplifying clutters and other artifacts
													if ((eirpLimit_dBm - 1) < channel->eirpLimit_dBm) {
														continue;    // Proceed to configured path loss
													} else {
														skip = true;
														break;       // Skipping this scanpoint
													}
												}
											}
											if (skip) {
												continue;
											}

											if (channel->type == ChannelType::INQUIRED_CHANNEL) {
												if (eirpLimit_dBm < _minEIRP_dBm) {
													channel->eirpLimit_dBm = _minEIRP_dBm;
													channel->availability = RED;
												}
											} else {
												// INQUIRED_FREQUENCY
												double psd = eirpLimit_dBm - 10.0*log((double) channel->bandwidth())/log(10.0);
												if (psd < _minPSD_dBmPerMHz) {
													channel->eirpLimit_dBm = _minPSD_dBmPerMHz + 10.0*log((double) channel->bandwidth())/log(10.0);
													channel->availability = RED;
												}
											}

											if ((channel->availability != RED) && (eirpLimit_dBm < channel->eirpLimit_dBm)) {
												channel->eirpLimit_dBm = eirpLimit_dBm;
											}

											if ( (!ulsFlagList[ulsIdx]) || (channel->eirpLimit_dBm < eirpLimitList[ulsIdx]) ) {
												eirpLimitList[ulsIdx] = channel->eirpLimit_dBm;
												ulsFlagList[ulsIdx] = true;
											}

											if ( fexcthrwifi && (std::isnan(rxPowerDBW) || (I2NDB > _visibilityThreshold) || (distKm * 1000 < _closeInDist)) )
											{
												double d1;
												double d2;
												double pathDifference;
												double fresnelIndex = -1.0;
												double ulsLinkDistance = uls->getLinkDistance();
												double ulsWavelength = CConst::c / ((uls->getStartUseFreq() + uls->getStopUseFreq()) / 2);
												if (ulsLinkDistance != -1.0) {
													const Vector3 ulsTxPos = (segIdx ? uls->getPR(segIdx-1).position : uls->getTxPosition());
													d1 = (ulsRxPos - rlanPosn).len() * 1000;
													d2 = (ulsTxPos - rlanPosn).len() * 1000;
													pathDifference = d1 + d2 - ulsLinkDistance;
													fresnelIndex = pathDifference / (ulsWavelength / 2);
												} else {
													d1 = (ulsRxPos - rlanPosn).len() * 1000;
													d2 = -1.0;
													pathDifference = -1.0;
												}

												std::string rxAntennaTypeStr;
												CConst::ULSAntennaTypeEnum ulsRxAntennaType = uls->getRxAntennaType();
												if (ulsRxAntennaType == CConst::LUTAntennaType) {
													rxAntennaTypeStr = std::string(uls->getRxAntenna()->get_strid());
												} else {
													rxAntennaTypeStr = std::string(CConst::strULSAntennaTypeList->type_to_str(ulsRxAntennaType)) + rxAntennaSubModelStr;
												}

												CConst::AntennaCategoryEnum rxAntennaCategory = uls->getRxAntennaCategory();
												std::string rxAntennaCategoryStr = (
														rxAntennaCategory == CConst::B1AntennaCategory ? "B1" :
														rxAntennaCategory == CConst::HPAntennaCategory ? "HP" :
														"UNKNOWN");

												std::string bldgTypeStr = (_fixedBuildingLossFlag ? "INDOOR_FIXED" :
														_buildingType == CConst::noBuildingType ? "OUTDOOR" :
														_buildingType == CConst::traditionalBuildingType ?  "TRADITIONAL" :
														"THERMALLY_EFFICIENT");

												std::string dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);

												QStringList msg;
												msg << QString::number(uls->getID()) << QString::fromStdString(dbName) << QString::number(rlanPosnIdx) << QString::fromStdString(uls->getCallsign());
												msg << QString::number(uls->getRxLongitudeDeg(), 'f', 5) << QString::number(uls->getRxLatitudeDeg(), 'f', 5);
												msg << QString::number((divIdx == 0 ? uls->getRxHeightAboveTerrain() : uls->getDiversityHeightAboveTerrain()), 'f', 2)
													<< QString::number(uls->getRxTerrainHeight(), 'f', 2)
													<< QString::fromStdString(_terrainDataModel->getSourceName(uls->getRxHeightSource()))
													<< QString(ulsRxPropEnv);
												msg << QString::number(uls->getNumPR());
												msg << QString::number(divIdx);
												msg << QString::number(segIdx) << QString::number(ulsRxLongitude, 'f', 5) << QString::number(ulsRxLatitude, 'f', 5);
												msg << QString::number(rlanCoord.longitudeDeg, 'f', 5) << QString::number(rlanCoord.latitudeDeg, 'f', 5);
												msg << QString::number(rlanCoord.heightKm * 1000.0 - rlanTerrainHeight, 'f', 2) << QString::number(rlanTerrainHeight, 'f', 2)
													<< QString::fromStdString(_terrainDataModel->getSourceName(rlanHeightSource))
													<< CConst::strPropEnvList->type_to_str(rlanPropEnv);
												msg << QString::number(distKm, 'f', 3) << QString::number(groundDistanceKm, 'f', 3) << QString::number(elevationAngleTxDeg, 'f', 3) << QString::number(angleOffBoresightDeg);
												msg << QString::number(_maxEIRP_dBm, 'f', 3) << QString::number(_bodyLossDB, 'f', 3) << QString::fromStdString(txClutterStr) << QString::fromStdString(rxClutterStr) << QString::fromStdString(bldgTypeStr);
												msg << QString::number(buildingPenetrationDB, 'f', 3) << QString::fromStdString(buildingPenetrationModelStr) << QString::number(buildingPenetrationCDF, 'f', 8);
												msg << QString::number(pathLoss, 'f', 3) << QString::fromStdString(pathLossModelStr) << QString::number(pathLossCDF, 'f', 8);
		
												msg << QString::number(pathClutterTxDB, 'f', 3) << QString::fromStdString(pathClutterTxModelStr) << QString::number(pathClutterTxCDF, 'f', 8);
												msg << QString::number(pathClutterRxDB, 'f', 3) << QString::fromStdString(pathClutterRxModelStr) << QString::number(pathClutterRxCDF, 'f', 8);
		
												msg << QString::number(bandwidth * 1.0e-6, 'f', 0) << QString::number(chanStartFreq * 1.0e-6, 'f', 0) << QString::number(chanStopFreq * 1.0e-6, 'f', 0)
													<< QString::number(uls->getStartUseFreq() * 1.0e-6, 'f', 2) << QString::number(uls->getStopUseFreq() * 1.0e-6, 'f', 2);
		
												msg << QString::fromStdString(rxAntennaTypeStr) << QString::fromStdString(rxAntennaCategoryStr)
													<< QString::number((divIdx == 0 ? uls->getRxGain() : uls->getDiversityGain()), 'f', 3);

												if (segIdx == numPR) {
													msg << QString("") << QString("") << QString("");
												} else {
													msg << CConst::strPRTypeList->type_to_str(uls->getPR(segIdx).type);
													msg << QString::number(uls->getPR(segIdx).effectiveGain, 'f', 3);
													msg << QString::number(discriminationGain, 'f', 3);
												}

												msg << QString::number(rxGainDB, 'f', 3);
												if (std::isnan(nearField_xdb)) {
													msg << QString("");
												} else {
													msg << QString::number(nearField_xdb, 'f', 5);
												}
												if (std::isnan(nearField_u)) {
													msg << QString("");
												} else {
													msg << QString::number(nearField_u, 'f', 5);
												}
												if (std::isnan(nearField_eff)) {
													msg << QString("");
												} else {
													msg << QString::number(nearField_eff, 'f', 5);
												}
												msg << QString::number(nearFieldOffsetDB, 'f', 3) << QString::number(spectralOverlapLossDB, 'f', 3);
												msg << QString::number(_polarizationLossDB, 'f', 3);
												msg << QString::number(uls->getRxAntennaFeederLossDB(), 'f', 3);
												msg << QString::number(rxPowerDBW, 'f', 3) << QString::number(I2NDB, 'f', 3) <<  QString::number(eirpLimit_dBm, 'f', 3);
												msg << QString::number(ulsLinkDistance, 'f', 3) << QString::number(chanCenterFreq, 'f', 3) << QString::number(d2, 'f', 3) << QString::number(pathDifference, 'f', 6);
												msg << QString::number(ulsWavelength * 1000, 'f', 3) << QString::number(fresnelIndex, 'f', 3);

												fexcthrwifi->writeRow(msg);
											}
										}
									}
								}
							}


							if (uls->ITMHeightProfile) {
								free(uls->ITMHeightProfile);
								uls->ITMHeightProfile = (double *) NULL;
							}
							if (uls->isLOSHeightProfile) {
								free(uls->isLOSHeightProfile);
								uls->isLOSHeightProfile = (double *) NULL;
							}
						}
					}

					if (fFSList) {
						fprintf(fFSList,   "%d,%s,%s,%.1f,%.1f,%.6f,%.6f,%.1f,%.1f\n",
							uls->getID(),
							uls->getRxCallsign().c_str(),
							uls->getCallsign().c_str(),
							uls->getStartUseFreq()*1.0e-6,
							uls->getStopUseFreq()*1.0e-6,
							uls->getRxLongitudeDeg(),
							uls->getRxLatitudeDeg(),
							uls->getRxHeightAboveTerrain(),
							minRLANDist
						);
					}

#					if DEBUG_AFC
					time_t t2 = time(NULL);
					tstr = strdup(ctime(&t2));
					strtok(tstr, "\n");

					std::cout << numProc << " [" << ulsIdx+1 << " / " <<  _ulsList->getSize() << "] " << tstr << " Elapsed Time = " << (t2-t1) << std::endl << std::flush;

					free(tstr);

					if (false&&(numProc == 10)) {
						cont = false;
					}

					if (traceFlag&&(!contains2D)) {
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
						std::string dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);
						ftrace->writeRow({
								QString::asprintf("END_%s_%d", dbName.c_str(), uls->getID()),
								QString(),
								QString(),
								QString(),
								QString(),
								QString(),
								QString::number(-1)
								});

					}
#					endif

				}
				else
				{
#					if DEBUG_AFC
						// uls is not included in calculations
						LOGGER_DEBUG(logger) << "ID: " << uls->getID() << ", distKm: " << sqrt(distKmSquared) << ", link: " << uls->getLinkDistance() << ",";
#					endif
				}
			}
			}
		}

		numProc++;

		if (numProc == xN) {
			if (xN == 1) {
				tstart = std::chrono::high_resolution_clock::now();
				pctIdx = 1;
			} else if (!_progressFile.empty()) {
				auto tcurrent = std::chrono::high_resolution_clock::now();
				double elapsedTime = std::chrono::duration_cast<std::chrono::duration<double>>(tcurrent-tstart).count();
				double remainingTime = elapsedTime*(totNumProc-numProc)/(numProc-1);
				std::ostringstream progress;
				QByteArray progressBA;

				progress << (int) std::floor(100.0*numProc/totNumProc) << std::endl
					<< "Elapsed Time: " << (int) std::floor(elapsedTime)
					<< " s, Remaining: " << (int) std::floor(remainingTime) << " s";
				progressBA = QByteArray(progress.str().c_str());
				/* dont throw error */
				AfcManager::_dataIf->writeFile(QString::fromStdString(_progressFile), progressBA);

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
			if ((useFlag) && (fkml)) {
				ULSClass *uls = (*_ulsList)[ulsIdx];
				std::string dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);

				Vector3 ulsTxPos = uls->getTxPosition();
				double ulsTxLongitude = uls->getTxLongitudeDeg();
				double ulsTxLatitude = uls->getTxLatitudeDeg();
				double ulsTxHeight = uls->getTxHeightAMSL();

				fkml->writeStartElement("Folder");
				fkml->writeTextElement("name", QString::fromStdString(dbName + "_" + std::to_string(uls->getID())));
				// fkml->writeTextElement("name", QString::fromStdString(uls->getCallsign()));
				if (addPlacemarks) {
					fkml->writeStartElement("Placemark");
					fkml->writeTextElement("name", QString::asprintf("%s %s_%d", "TX", dbName.c_str(), uls->getID()));
					fkml->writeTextElement("visibility", "1");
					fkml->writeTextElement("styleUrl", placemarkStyleStr.c_str());
					fkml->writeStartElement("Point");
					fkml->writeTextElement("altitudeMode", "absolute");
					fkml->writeTextElement("coordinates", QString::asprintf("%.10f,%.10f,%.2f", ulsTxLongitude, ulsTxLatitude, ulsTxHeight));
					fkml->writeEndElement(); // Point
					fkml->writeEndElement(); // Placemark
				}

				int numPR = uls->getNumPR();
				for(int segIdx=0; segIdx<numPR+1; ++segIdx) {

					Vector3 ulsRxPos = (segIdx == numPR ? uls->getRxPosition() : uls->getPR(segIdx).position);
					double ulsRxLongitude = (segIdx == numPR ? uls->getRxLongitudeDeg() : uls->getPR(segIdx).longitudeDeg);
					double ulsRxLatitude = (segIdx == numPR ? uls->getRxLatitudeDeg() : uls->getPR(segIdx).latitudeDeg);
					double ulsRxHeight = (segIdx == numPR ? uls->getRxHeightAMSL() : uls->getPR(segIdx).heightAMSL);


					double beamWidthDeg = uls->computeBeamWidth(3.0);
					double beamWidthRad = beamWidthDeg*(M_PI/180.0);

					double linkDistKm = (ulsTxPos - ulsRxPos).len();


					Vector3 zvec = (ulsTxPos-ulsRxPos).normalized();
					Vector3 xvec = (Vector3(zvec.y(), -zvec.x(),0.0)).normalized();
					Vector3 yvec = zvec.cross(xvec);

					int numCvgPoints = 32;

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
						std::string nameStr;
						if (segIdx == numPR) {
							nameStr = "RX";
						} else {
							nameStr = "PR " + std::to_string(segIdx+1);;
						}
						fkml->writeStartElement("Placemark");
						fkml->writeTextElement("name", QString::asprintf("%s %s_%d", nameStr.c_str(), dbName.c_str(), uls->getID()));
						fkml->writeTextElement("visibility", "1");
						fkml->writeTextElement("styleUrl", placemarkStyleStr.c_str());
						fkml->writeStartElement("Point");
						fkml->writeTextElement("altitudeMode", "absolute");
						fkml->writeTextElement("coordinates", QString::asprintf("%.10f,%.10f,%.2f", ulsRxLongitude, ulsRxLatitude, ulsRxHeight));
						fkml->writeEndElement(); // Point
						fkml->writeEndElement(); // Placemark
					}

					fkml->writeStartElement("Folder");
					fkml->writeTextElement("name", QString::asprintf("Beamcone_%d", segIdx+1));

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

						QString more_coords = QString::asprintf("%.10f,%.10f,%.2f\n", ulsRxLongitude, ulsRxLatitude, ulsRxHeight);

						GeodeticCoord pt = ptList[cvgPhiIdx];
						more_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", pt.longitudeDeg, pt.latitudeDeg, pt.heightKm*1000.0));

						pt = ptList[(cvgPhiIdx +1) % numCvgPoints];
						more_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", pt.longitudeDeg, pt.latitudeDeg, pt.heightKm*1000.0));

						more_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n", ulsRxLongitude, ulsRxLatitude, ulsRxHeight));

						fkml->writeTextElement("coordinates", more_coords);
						fkml->writeEndElement(); // LinearRing
						fkml->writeEndElement(); // outerBoundaryIs
						fkml->writeEndElement(); // Polygon
						fkml->writeEndElement(); // Placemark
					}
					fkml->writeEndElement(); // Beamcone

					ulsTxPos       = ulsRxPos;
					ulsTxLongitude = ulsRxLongitude;
					ulsTxLatitude  = ulsRxLatitude;
					ulsTxHeight    = ulsRxHeight;
				}
				fkml->writeEndElement(); // Folder
			}
		}
		fkml->writeEndElement(); // Folder
	}

	if (fkml) {
		fkml->writeEndElement(); // Document
		fkml->writeEndElement(); // kml
		fkml->writeEndDocument();
	}

	if (fFSList) {
		fclose(fFSList);
	}

	if (numProc == 0) {
		errStr << "Analysis region contains no FS receivers";
		LOGGER_WARN(logger) << errStr.str();
		statusMessageList.push_back(errStr.str());
	}

#   if DEBUG_AFC
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

		if ( (channel->type == ChannelType::INQUIRED_CHANNEL) && (channel->availability != BLACK) && (channel->availability != RED) ) {
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
/* AfcManager::runScanAnalysis()                                                          */
/******************************************************************************************/
void AfcManager::runScanAnalysis()
{
	std::ostringstream errStr;

	LOGGER_INFO(logger) << "Executing AfcManager::runScanAnalysis()";

	std::map<int, int> bw_index_map;

	FILE *fscan;
	std::string scanFileName = _serialNumber.toStdString() + "_scan.csv";
	if ( !(fscan = fopen(scanFileName.c_str(), "wb")) ) {
		errStr << std::string("ERROR: Unable to open scanFile \"") + "/tmp/scan.csv" + std::string("\"\n");
		throw std::runtime_error(errStr.str());
	}
	fprintf(fscan,  "SCAN_PT_IDX,HEIGHT_IDX,RLAN_LONGITUDE (deg)"
			",RLAN_LATITUDE (deg)"
			",RLAN_HEIGHT_ABOVE_TERRAIN (m)"
			",RLAN_TERRAIN_HEIGHT (m)"
			",RLAN_TERRAIN_SOURCE"
			",RLAN_PROP_ENV");


	// List all bandwidths covered by op-classes, merge duplicates
	for (auto &opClass: _opClass) {
		int bw = opClass.bandWidth;
		bw_index_map[opClass.bandWidth] = 0;
	}

	int numBW = 0;
	for (auto &map: bw_index_map) {
		int bw = map.first;
		fprintf(fscan, ",NUM_CHAN_BLACK_%d_MHZ,NUM_CHAN_RED_%d_MHZ,NUM_CHAN_YELLOW_%d_MHZ,NUM_CHAN_GREEN_%d_MHZ", bw, bw, bw, bw);
		// Note down the index of bandwidth in the map
		map.second = numBW++;
	}
	fprintf(fscan, "\n");
	fflush(fscan);

	_rlanRegion->configure(_rlanHeightType, _terrainDataModel);

	double heightUncertainty = _rlanRegion->getHeightUncertainty();
	int NHt = (int) ceil(heightUncertainty / _scanres_ht);
	Vector3 rlanPosnList[2*NHt+1];
	GeodeticCoord rlanCoordList[2*NHt+1];

	int bwIdx;
	int numBlack[numBW];
	int numGreen[numBW];
	int numYellow[numBW];
	int numRed[numBW];

	/**************************************************************************************/
	/* Get Uncertainty Region Scan Points                                                 */
	/**************************************************************************************/
	std::vector<LatLon> scanPointList = _rlanRegion->getScan(_scanRegionMethod, _scanres_xy, _scanres_points_per_degree);
	/**************************************************************************************/

	/**************************************************************************************/
	/* Create KML file                                                                    */
	/**************************************************************************************/
	std::string kmzFileName = _serialNumber.toStdString() + "_scan.kmz";
	ZXmlWriter *kml_writer = new ZXmlWriter(kmzFileName);
	auto &fkml = kml_writer->xml_writer;

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
		fkml->writeStartElement("LabelStyle");
		fkml->writeTextElement("scale", "0");
		fkml->writeEndElement(); // LabelStyle
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

		std::vector<GeodeticCoord> ptList = _rlanRegion->getBoundary(_terrainDataModel);

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

		/**********************************************************************************/
		/* Scan Points                                                                    */
		/**********************************************************************************/
		fkml->writeStartElement("Folder");
		fkml->writeTextElement("name", "SCAN POINTS");

		for(ptIdx=0; ptIdx<(int) scanPointList.size(); ptIdx++) {
			LatLon scanPt = scanPointList[ptIdx];

			double rlanTerrainHeight, bldgHeight;
			MultibandRasterClass::HeightResult lidarHeightResult;
			CConst::HeightSourceEnum rlanHeightSource;
			_terrainDataModel->getTerrainHeight(scanPt.second, scanPt.first, rlanTerrainHeight,bldgHeight, lidarHeightResult, rlanHeightSource);

			double height0;
			if (_rlanRegion->getFixedHeightAMSL()) {
				height0 = _rlanRegion->getCenterHeightAMSL();
			} else {
				height0 = _rlanRegion->getCenterHeightAMSL() - _rlanRegion->getCenterTerrainHeight() + rlanTerrainHeight;
			}

			int htIdx;
			for(htIdx=0; htIdx<=2*NHt; ++htIdx) {
				double heightAMSL = height0 + (NHt ? (htIdx-NHt)*heightUncertainty/NHt : 0.0);

				fkml->writeStartElement("Placemark");
				fkml->writeTextElement("name", QString::asprintf("SCAN_POINT_%d_%d", ptIdx, htIdx));
				fkml->writeTextElement("visibility", "1");
				fkml->writeTextElement("styleUrl", "#dotStyle");
				fkml->writeStartElement("Point");
				fkml->writeTextElement("altitudeMode", "absolute");
				fkml->writeTextElement("coordinates", QString::asprintf("%.10f,%.10f,%.2f", scanPt.second, scanPt.first, heightAMSL));
				fkml->writeEndElement(); // Point
				fkml->writeEndElement(); // Placemark
			}
		}

		fkml->writeEndElement(); // Scan Points
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

	if (fkml) {
		fkml->writeEndElement(); // Document
		fkml->writeEndElement(); // kml
		fkml->writeEndDocument();
		delete kml_writer;
	}


	/**************************************************************************************/
	/* Compute Channel Availability                                                       */
	/**************************************************************************************/
	const double exclusionDistKmSquared = (_exclusionDist / 1000.0) * (_exclusionDist / 1000.0);
	const double maxRadiusKmSquared = (_maxRadius / 1000.0) * (_maxRadius / 1000.0);

	if (_rlanRegion->getMinHeightAGL() < _minRlanHeightAboveTerrain) {
		throw std::runtime_error(ErrStream()
				<< std::string("ERROR: Point Analysis: Invalid RLAN parameter settings.") << std::endl
				<< std::string("RLAN Min Height above terrain = ") << _rlanRegion->getMinHeightAGL() << std::endl
				<< std::string("RLAN must be more than ") << _minRlanHeightAboveTerrain << " meters above terrain" << std::endl
				);
	}

	int scanPtIdx;
	for(scanPtIdx=0; scanPtIdx<(int) scanPointList.size(); scanPtIdx++) {
		LatLon scanPt = scanPointList[scanPtIdx];

		/**************************************************************************************/
		/* Determine propagation environment of RLAN using scan point                         */
		/**************************************************************************************/
		CConst::NLCDLandCatEnum nlcdLandCatTx;
		CConst::PropEnvEnum rlanPropEnv = computePropEnv(scanPt.second, scanPt.first, nlcdLandCatTx);
		/**************************************************************************************/

		double rlanTerrainHeight, bldgHeight;
		MultibandRasterClass::HeightResult lidarHeightResult;
		CConst::HeightSourceEnum rlanHeightSource;
		_terrainDataModel->getTerrainHeight(scanPt.second, scanPt.first, rlanTerrainHeight,bldgHeight, lidarHeightResult, rlanHeightSource);

		double height0;
		if (_rlanRegion->getFixedHeightAMSL()) {
			height0 = _rlanRegion->getCenterHeightAMSL();
		} else {
			height0 = _rlanRegion->getCenterHeightAMSL() - _rlanRegion->getCenterTerrainHeight() + rlanTerrainHeight;
		}

		int rlanPosnIdx;
		int htIdx;
		for(htIdx=0; htIdx<=2*NHt; ++htIdx) {
			rlanCoordList[htIdx] = GeodeticCoord::fromLatLon(scanPt.first, scanPt.second, (height0 + (NHt ? (htIdx-NHt)*heightUncertainty/NHt : 0.0))/1000.0);
			rlanPosnList[htIdx] = EcefModel::fromGeodetic(rlanCoordList[htIdx]);
		}

		int numRlanPosn = 2*NHt + 1;

		for (rlanPosnIdx = 0; rlanPosnIdx < numRlanPosn; ++rlanPosnIdx) {
			Vector3 rlanPosn = rlanPosnList[rlanPosnIdx];
			GeodeticCoord rlanCoord = rlanCoordList[rlanPosnIdx];

			/**************************************************************************************/
			/* Initialize eirpLimit_dBm to _maxEIRP_dBm for all channels                          */
			/**************************************************************************************/
			for (auto& channel : _channelList) {
				channel.eirpLimit_dBm = _maxEIRP_dBm;
				channel.availability = GREEN;
			}
			/**************************************************************************************/

			double rlanHtAboveTerrain = rlanCoord.heightKm * 1000.0 - rlanTerrainHeight;

			int rasIdx;
			for (rasIdx = 0; rasIdx < _rasList->getSize(); ++rasIdx) {
				RASClass *ras = (*_rasList)[rasIdx];
				if (ras->intersect(rlanCoord.longitudeDeg, rlanCoord.latitudeDeg, 0.0, rlanHtAboveTerrain)) {
					for (auto& channel : _channelList) {
						if (channel.availability != BLACK) {
							double chanStartFreq = channel.startFreqMHz * 1.0e6;
							double chanStopFreq = channel.stopFreqMHz * 1.0e6;
							bool hasOverlap = computeSpectralOverlapLoss((double *) NULL, chanStartFreq, chanStopFreq, ras->getStartFreq(), ras->getStopFreq(), false, CConst::psdSpectralAlgorithm);
							if (hasOverlap) {
								channel.availability = BLACK;
								channel.eirpLimit_dBm = -std::numeric_limits<double>::infinity();
							}
						}
					}
				}
			}

			int ulsIdx;
			for (ulsIdx = 0; ulsIdx < _ulsList->getSize(); ++ulsIdx) {
				ULSClass *uls = (*_ulsList)[ulsIdx];
				const Vector3 ulsRxPos = uls->getRxPosition();
				Vector3 lineOfSightVectorKm = ulsRxPos - rlanPosn;
				double distKmSquared = (lineOfSightVectorKm).dot(lineOfSightVectorKm);
				double distKm = lineOfSightVectorKm.len();
				double dAP = rlanPosn.len();
				double duls = ulsRxPos.len();
				double elevationAngleTxDeg = 90.0 - acos(rlanPosn.dot(lineOfSightVectorKm)/(dAP*distKm))*180.0/M_PI;
				double elevationAngleRxDeg = 90.0 - acos(ulsRxPos.dot(-lineOfSightVectorKm)/(duls*distKm))*180.0/M_PI;

				// Use Haversine formula with average earth radius of 6371 km
				double lon1Rad = rlanCoord.longitudeDeg*M_PI/180.0;
				double lat1Rad = rlanCoord.latitudeDeg*M_PI/180.0;
				double lon2Rad = uls->getRxLongitudeDeg()*M_PI/180.0;
				double lat2Rad = uls->getRxLatitudeDeg()*M_PI/180.0;
				double slat = sin((lat2Rad-lat1Rad)/2);
				double slon = sin((lon2Rad-lon1Rad)/2);
				double groundDistanceKm = 2*CConst::averageEarthRadius*asin(sqrt(slat*slat+cos(lat1Rad)*cos(lat2Rad)*slon*slon))*1.0e-3;

				double win2DistKm;
				if (_winner2UseGroundDistanceFlag) {
					win2DistKm = groundDistanceKm;
				} else {
					win2DistKm = distKm;
				}

				double fsplDistKm;
				if (_fsplUseGroundDistanceFlag) {
					fsplDistKm = groundDistanceKm;
				} else {
					fsplDistKm = distKm;
				}

				if ((distKmSquared < maxRadiusKmSquared) && (distKmSquared > exclusionDistKmSquared) && (uls->getLinkDistance() > 0.0)) {

					CConst::ULSAntennaTypeEnum ulsRxAntennaType = uls->getRxAntennaType();

					/**************************************************************************************/
					/* Determine propagation environment of FS, if needed.                                */
					/**************************************************************************************/
					char ulsRxPropEnv;
					CConst::NLCDLandCatEnum nlcdLandCatRx;
					CConst::PropEnvEnum fsPropEnv;
					if ((_applyClutterFSRxFlag) && (uls->getRxHeightAboveTerrain() <= _maxFsAglHeight)) {
						fsPropEnv = computePropEnv(uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), nlcdLandCatRx);
						switch(fsPropEnv) {
							case CConst::urbanPropEnv:    ulsRxPropEnv = 'U'; break;
							case CConst::suburbanPropEnv: ulsRxPropEnv = 'S'; break;
							case CConst::ruralPropEnv:    ulsRxPropEnv = 'R'; break;
							case CConst::barrenPropEnv:   ulsRxPropEnv = 'B'; break;
							case CConst::unknownPropEnv:  ulsRxPropEnv = 'X'; break;
							default: CORE_DUMP; break;
						}
					} else {
						fsPropEnv = CConst::unknownPropEnv;
						ulsRxPropEnv = ' ';
					}
					/**************************************************************************************/

					for (auto& channel : _channelList) {
						if (channel.availability != BLACK) {
							double chanStartFreq = channel.startFreqMHz * 1.0e6;
							double chanStopFreq = channel.stopFreqMHz * 1.0e6;
							bool useACI = (channel.type == INQUIRED_FREQUENCY ? false : _aciFlag);
							CConst::SpectralAlgorithmEnum spectralAlgorithm = (channel.type == INQUIRED_FREQUENCY ? CConst::psdSpectralAlgorithm : _channelResponseAlgorithm);
							// LOGGER_INFO(logger) << "COMPUTING SPECTRAL OVERLAP FOR FSID = " << uls->getID();
							double spectralOverlapLossDB;
							bool hasOverlap = computeSpectralOverlapLoss(&spectralOverlapLossDB, chanStartFreq, chanStopFreq, uls->getStartUseFreq(), uls->getStopUseFreq(), useACI, spectralAlgorithm);

							if (hasOverlap) {
								// double bandwidth = chanStopFreq - chanStartFreq;
								double chanCenterFreq = (chanStartFreq + chanStopFreq)/2;

								std::string buildingPenetrationModelStr;
								double buildingPenetrationCDF;
								double buildingPenetrationDB = computeBuildingPenetration(_buildingType, elevationAngleTxDeg, chanCenterFreq, buildingPenetrationModelStr, buildingPenetrationCDF, true);

								std::string txClutterStr;
								std::string rxClutterStr;
								std::string pathLossModelStr;
								double pathLossCDF;
								double pathLoss;
								std::string pathClutterTxModelStr;
								double pathClutterTxCDF;
								double pathClutterTxDB;
								std::string pathClutterRxModelStr;
								double pathClutterRxCDF;
								double pathClutterRxDB;

								computePathLoss(_pathLossModel, rlanPropEnv, fsPropEnv, nlcdLandCatTx, nlcdLandCatRx, distKm, fsplDistKm, win2DistKm, chanCenterFreq,
										rlanCoord.longitudeDeg, rlanCoord.latitudeDeg, rlanHtAboveTerrain, elevationAngleTxDeg,
										uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), uls->getRxHeightAboveTerrain(), elevationAngleRxDeg,
										pathLoss, pathClutterTxDB, pathClutterRxDB, true,
										pathLossModelStr, pathLossCDF,
										pathClutterTxModelStr, pathClutterTxCDF, pathClutterRxModelStr, pathClutterRxCDF,
										(iturp452::ITURP452 *)NULL, &txClutterStr, &rxClutterStr, &(uls->ITMHeightProfile), &(uls->isLOSHeightProfile)
#if DEBUG_AFC
										, uls->ITMHeightType
#endif
										);

								std::string rxAntennaSubModelStr;
								double angleOffBoresightDeg = acos(uls->getAntennaPointing().dot(-(lineOfSightVectorKm.normalized()))) * 180.0 / M_PI;
								double rxGainDB = uls->computeRxGain(angleOffBoresightDeg, elevationAngleRxDeg, chanCenterFreq, rxAntennaSubModelStr, 0);

								double rxPowerDBW = (_maxEIRP_dBm - 30.0) - _bodyLossDB - buildingPenetrationDB - pathLoss - pathClutterTxDB - pathClutterRxDB + rxGainDB - spectralOverlapLossDB - _polarizationLossDB - uls->getRxAntennaFeederLossDB();

								double I2NDB = rxPowerDBW - uls->getNoiseLevelDBW();

								double marginDB = _IoverN_threshold_dB - I2NDB;

								double eirpLimit_dBm = _maxEIRP_dBm + marginDB;

								if (eirpLimit_dBm < channel.eirpLimit_dBm) {
									channel.eirpLimit_dBm = eirpLimit_dBm;
								}
							}
						}
					}
				}

				if (uls->ITMHeightProfile) {
					free(uls->ITMHeightProfile);
					uls->ITMHeightProfile = (double *) NULL;
				}
				if (uls->isLOSHeightProfile) {
					free(uls->isLOSHeightProfile);
					uls->isLOSHeightProfile = (double *) NULL;
				}
			}

			for(bwIdx=0; bwIdx<numBW; ++bwIdx) {
				numBlack[bwIdx] = 0;
				numGreen[bwIdx] = 0;
				numYellow[bwIdx] = 0;
				numRed[bwIdx] = 0;
			}

			for (auto& channel : _channelList) {
				if (channel.type == ChannelType::INQUIRED_CHANNEL) {
					bwIdx = bw_index_map[channel.bandwidth()];
					if (channel.availability == BLACK) {
						numBlack[bwIdx]++;
					} else if (channel.eirpLimit_dBm == _maxEIRP_dBm) {
						numGreen[bwIdx]++;
					}
					else if (channel.eirpLimit_dBm >= _minEIRP_dBm) {
						numYellow[bwIdx]++;
					} else {
						numRed[bwIdx]++;
					}
				}
			}

			fprintf(fscan, "%d,%d,%.6f,%.6f,%.1f,%.1f,%s,%s",
					scanPtIdx, rlanPosnIdx,
					rlanCoord.longitudeDeg, rlanCoord.latitudeDeg, rlanHtAboveTerrain, rlanTerrainHeight,
					_terrainDataModel->getSourceName(rlanHeightSource).c_str(),
					CConst::strPropEnvList->type_to_str(rlanPropEnv));

			for(bwIdx=0; bwIdx<numBW; ++bwIdx) {
				fprintf(fscan, ",%d,%d,%d,%d",
						numBlack[bwIdx], numRed[bwIdx], numYellow[bwIdx], numGreen[bwIdx]);
			}
			fprintf(fscan, "\n");
			fflush(fscan);
		}

	}
	fclose(fscan);


	/**************************************************************************************/

	_terrainDataModel->printStats();
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::runExclusionZoneAnalysis()                                                 */
/******************************************************************************************/
void AfcManager::runExclusionZoneAnalysis()
{

#if DEBUG_AFC
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

	ULSClass *uls = findULSID(_exclusionZoneFSID, 0);
	std::string dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);

	ChannelStruct channel = _channelList[_exclusionZoneRLANChanIdx];

	double bandwidth = (channel.stopFreqMHz - channel.startFreqMHz)*1.0e6;

	double chanStopFreq = channel.stopFreqMHz*1.0e6;
	double chanStartFreq = channel.startFreqMHz*1.0e6;
	bool useACI = (channel.type == INQUIRED_FREQUENCY ? false : _aciFlag);
	CConst::SpectralAlgorithmEnum spectralAlgorithm = (channel.type == INQUIRED_FREQUENCY ? CConst::psdSpectralAlgorithm : _channelResponseAlgorithm);
	// LOGGER_INFO(logger) << "COMPUTING SPECTRAL OVERLAP FOR FSID = " << uls->getID();
	double spectralOverlapLossDB;
	bool hasOverlap = computeSpectralOverlapLoss(&spectralOverlapLossDB, chanStartFreq, chanStopFreq, uls->getStartUseFreq(), uls->getStopUseFreq(), useACI, spectralAlgorithm);
	double chanCenterFreq = (chanStartFreq + chanStopFreq)/2;

	if (!hasOverlap) {
		throw std::runtime_error(ErrStream() << "ERROR: Specified RLAN spectrum does not overlap FS spectrum. FSID: " << _exclusionZoneFSID
				<< " goes from " << uls->getStartUseFreq() / 1.0e6 << " MHz to " << uls->getStopUseFreq() / 1.0e6 << " MHz");
	}
	LOGGER_INFO(logger) << "FSID = " << _exclusionZoneFSID << " found";
	LOGGER_INFO(logger) << "LON: " << uls->getRxLongitudeDeg();
	LOGGER_INFO(logger) << "LAT: " << uls->getRxLatitudeDeg();
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
				"FS_HAS_PASSIVE_REPEATER",
				"RLAN_LONGITUDE (deg)",
				"RLAN_LATITUDE (deg)",
				"RLAN_HEIGHT_ABOVE_TERRAIN (m)",
				"RLAN_TERRAIN_HEIGHT (m)",
				"RLAN_TERRAIN_SOURCE",
				"LAN_PROP_ENV",
				"RLAN_FS_RX_DIST (km)",
				"RLAN_FS_RX_GROUND_DIST (km)",
				"RLAN_FS_RX_ELEVATION_ANGLE (deg)",
				"FS_RX_ANGLE_OFF_BORESIGHT (deg)",
				"RLAN_TX_EIRP (dBm)",
				"BODY_LOSS (dB)",
				"RLAN_CLUTTER_CATEGORY",
				"FS_CLUTTER_CATEGORY",
				"BUILDING TYPE",
				"RLAN_FS_RX_BUILDING_PENETRATION (dB)",
				"BUILDING_PENETRATION_MODEL",
				"BUILDING_PENETRATION_CDF",
				"PATH_LOSS (dB)",
				"PATH_LOSS_MODEL",
				"PATH_LOSS_CDF",
				"PATH_CLUTTER_TX (DB)",
				"PATH_CLUTTER_TX_MODEL",
				"PATH_CLUTTER_TX_CDF",
				"PATH_CLUTTER_RX (DB)",
				"PATH_CLUTTER_RX_MODEL",
				"PATH_CLUTTER_RX_CDF",
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

#if DEBUG_AFC
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

#if 0
	int ulsLonIdx;
	int ulsLatIdx;
	int regionIdx;
	char ulsRxPropEnv;
	_popGrid->findDeg(uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), ulsLonIdx, ulsLatIdx, ulsRxPropEnv, regionIdx);
	double ulsPopVal = _popGrid->getPop(ulsLonIdx, ulsLatIdx);
	if (ulsPopVal == 0.0) {
		ulsRxPropEnv = 'Z';
	}
#else
	char ulsRxPropEnv = ' ';
#endif

	/**************************************************************************************/
	/* Compute Exclusion Zone                                                             */
	/**************************************************************************************/
	LOGGER_INFO(logger) << "Begin computing exclusion zone";

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

	double distKm0, distKm1, distKmM;
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
				printf("DBNAME = %s FSID = %d, EXCL_PT_IDX = %d, dFSPL = %.1f DIST = %.1f margin = %.3f\n", dbName.c_str(), uls->getID(), exclPtIdx, dFSPL, 1000*distKm0, margin0);
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
			} else if (!_progressFile.empty()) {
				auto tcurrent = std::chrono::high_resolution_clock::now();
				double elapsedTime = std::chrono::duration_cast<std::chrono::duration<double>>(tcurrent-tstart).count();
				double remainingTime = elapsedTime*(totNumProc-numProc)/(numProc-1);
				std::ostringstream progress;
				QByteArray progressBA;

				progress << (int) std::floor(100.0*numProc/totNumProc) << std::endl
					<< "Elapsed Time: " << (int) std::floor(elapsedTime)
					<< " s, Remaining: " << (int) std::floor(remainingTime) << " s";
				progressBA = QByteArray(progress.str().c_str());
				/* dont throw error */
				AfcManager::_dataIf->writeFile(QString::fromStdString(_progressFile), progressBA);

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
	if (_rlanHeightType == CConst::AMSLHeightType) {
		rlanHeight = rlanHeightInput;
	} else if (_rlanHeightType == CConst::AGLHeightType) {
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
	CConst::NLCDLandCatEnum nlcdLandCatTx;
	CConst::PropEnvEnum rlanPropEnv = computePropEnv(rlanLon, rlanLat, nlcdLandCatTx, false);
	/**************************************************************************************/

	/**************************************************************************************/
	/* Determine propagation environment of FS, if needed.                                */
	/**************************************************************************************/
	CConst::NLCDLandCatEnum nlcdLandCatRx;
	CConst::PropEnvEnum fsPropEnv;
	if ((_applyClutterFSRxFlag) && (uls->getRxHeightAboveTerrain() <= _maxFsAglHeight)) {
		fsPropEnv = computePropEnv(uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), nlcdLandCatRx);
	} else {
		fsPropEnv = CConst::unknownPropEnv;
	}
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
		double duls = ulsRxPos.len();
		double elevationAngleTxDeg = 90.0 - acos(rlanPosn.dot(lineOfSightVectorKm)/(dAP*distKm))*180.0/M_PI;
		double elevationAngleRxDeg = 90.0 - acos(ulsRxPos.dot(-lineOfSightVectorKm)/(duls*distKm))*180.0/M_PI;

		// Use Haversine formula with average earth radius of 6371 km
		double lon1Rad = rlanCoord.longitudeDeg*M_PI/180.0;
		double lat1Rad = rlanCoord.latitudeDeg*M_PI/180.0;
		double lon2Rad = uls->getRxLongitudeDeg()*M_PI/180.0;
		double lat2Rad = uls->getRxLatitudeDeg()*M_PI/180.0;
		double slat = sin((lat2Rad-lat1Rad)/2);
		double slon = sin((lon2Rad-lon1Rad)/2);
		double groundDistanceKm = 2*CConst::averageEarthRadius*asin(sqrt(slat*slat+cos(lat1Rad)*cos(lat2Rad)*slon*slon))*1.0e-3;

		double win2DistKm;
		if (_winner2UseGroundDistanceFlag) {
			win2DistKm = groundDistanceKm;
		} else {
			win2DistKm = distKm;
		}

		double fsplDistKm;
		if (_fsplUseGroundDistanceFlag) {
			fsplDistKm = groundDistanceKm;
		} else {
			fsplDistKm = distKm;
		}

		std::string buildingPenetrationModelStr;
		double buildingPenetrationCDF;
		double buildingPenetrationDB = computeBuildingPenetration(_buildingType, elevationAngleTxDeg, chanCenterFreq, buildingPenetrationModelStr, buildingPenetrationCDF, true);

		std::string txClutterStr;
		std::string rxClutterStr;
		std::string pathLossModelStr;
		double pathLossCDF;
		double pathLoss;
		std::string pathClutterTxModelStr;
		double pathClutterTxCDF;
		double pathClutterTxDB;
		std::string pathClutterRxModelStr;
		double pathClutterRxCDF;
		double pathClutterRxDB;

		double rlanHtAboveTerrain = rlanCoord.heightKm * 1000.0 - rlanTerrainHeight;

		computePathLoss(_pathLossModel, (rlanPropEnv == CConst::unknownPropEnv ? CConst::barrenPropEnv : rlanPropEnv), fsPropEnv, nlcdLandCatTx, nlcdLandCatRx, distKm, fsplDistKm, win2DistKm, chanCenterFreq,
				rlanCoord.longitudeDeg, rlanCoord.latitudeDeg, rlanHtAboveTerrain, elevationAngleTxDeg,
				uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), uls->getRxHeightAboveTerrain(), elevationAngleRxDeg,
				pathLoss, pathClutterTxDB, pathClutterRxDB, true,
				pathLossModelStr, pathLossCDF,
				pathClutterTxModelStr, pathClutterTxCDF, pathClutterRxModelStr, pathClutterRxCDF,
				(iturp452::ITURP452 *)NULL, &txClutterStr, &rxClutterStr, &(uls->ITMHeightProfile), &(uls->isLOSHeightProfile)
#if DEBUG_AFC
				, uls->ITMHeightType
#endif
				);

		std::string rxAntennaSubModelStr;
		double angleOffBoresightDeg = acos(uls->getAntennaPointing().dot(-(lineOfSightVectorKm.normalized()))) * 180.0 / M_PI;
		double rxGainDB = uls->computeRxGain(angleOffBoresightDeg, elevationAngleRxDeg, chanCenterFreq, rxAntennaSubModelStr, 0);

		double rxPowerDBW = (_exclusionZoneRLANEIRPDBm - 30.0) - _bodyLossDB - buildingPenetrationDB - pathLoss - pathClutterTxDB - pathClutterRxDB + rxGainDB - spectralOverlapLossDB - _polarizationLossDB - uls->getRxAntennaFeederLossDB();

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
			double ulsLinkDistance = uls->getLinkDistance();
			double ulsWavelength = CConst::c / ((uls->getStartUseFreq() + uls->getStopUseFreq()) / 2);
			if (ulsLinkDistance != -1.0)
			{
				int numPR = uls->getNumPR();
				const Vector3 ulsTxPos = (numPR ? uls->getPR(numPR-1).position : uls->getTxPosition());
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
			if (ulsRxAntennaType == CConst::LUTAntennaType) {
				rxAntennaTypeStr = std::string(uls->getRxAntenna()->get_strid());
			} else {
				rxAntennaTypeStr = std::string(CConst::strULSAntennaTypeList->type_to_str(ulsRxAntennaType)) + rxAntennaSubModelStr;
			}

			std::string bldgTypeStr = (_fixedBuildingLossFlag ? "INDOOR_FIXED" :
					_buildingType == CConst::noBuildingType ? "OUTDOOR" :
					_buildingType == CConst::traditionalBuildingType ?  "TRADITIONAL" :
					"THERMALLY_EFFICIENT");

			std::string dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);

			QStringList msg;
			msg << QString::number(uls->getID()) << QString::fromStdString(dbName) << QString::number(rlanPosnIdx) << QString::fromStdString(uls->getCallsign());
			msg << QString::number(uls->getRxLongitudeDeg(), 'f', 8) << QString::number(uls->getRxLatitudeDeg(), 'f', 8);
			msg << QString::number(uls->getRxHeightAboveTerrain(), 'f', 2) << QString::number(uls->getRxTerrainHeight(), 'f', 2) <<
				QString::fromStdString(_terrainDataModel->getSourceName(uls->getRxHeightSource()))
				<< QString(ulsRxPropEnv);
			msg << QString::number(uls->getNumPR() ? 1 : 0);
			msg << QString::number(rlanCoord.longitudeDeg, 'f', 8) << QString::number(rlanCoord.latitudeDeg, 'f', 8);
			msg << QString::number(rlanCoord.heightKm * 1000.0 - rlanTerrainHeight, 'f', 2) << QString::number(rlanTerrainHeight, 'f', 2) <<
				QString::fromStdString(_terrainDataModel->getSourceName(rlanHeightSource)) <<
				CConst::strPropEnvList->type_to_str(rlanPropEnv);
			msg << QString::number(distKm, 'f', 3) << QString::number(groundDistanceKm, 'f', 3) << QString::number(elevationAngleTxDeg, 'f', 3) << QString::number(angleOffBoresightDeg);
			msg << QString::number(_exclusionZoneRLANEIRPDBm, 'f', 3) << QString::number(_bodyLossDB, 'f', 3) << QString::fromStdString(txClutterStr) << QString::fromStdString(rxClutterStr) << QString::fromStdString(bldgTypeStr);
			msg << QString::number(buildingPenetrationDB, 'f', 3) << QString::fromStdString(buildingPenetrationModelStr) << QString::number(buildingPenetrationCDF, 'f', 8);
			msg << QString::number(pathLoss, 'f', 3) << QString::fromStdString(pathLossModelStr) << QString::number(pathLossCDF, 'f', 8);

			msg << QString::number(pathClutterTxDB, 'f', 3) << QString::fromStdString(pathClutterTxModelStr) << QString::number(pathClutterTxCDF, 'f', 8);
			msg << QString::number(pathClutterRxDB, 'f', 3) << QString::fromStdString(pathClutterRxModelStr) << QString::number(pathClutterRxCDF, 'f', 8);

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
	if (uls->isLOSHeightProfile) {
		free(uls->isLOSHeightProfile);
		uls->isLOSHeightProfile = (double *) NULL;
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

	ULSClass *uls = findULSID(_exclusionZoneFSID, 0);
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

	std::string dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);
	fkml->writeStartElement("Placemark");
	fkml->writeTextElement("name", QString("FSID : %1_%s").arg(QString::fromStdString(dbName)).arg(uls->getID()));
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
		if (_rlanHeightType == CConst::AMSLHeightType) {
			rlanHeight = rlanHeightInput;
		} else if (_rlanHeightType == CConst::AGLHeightType) {
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
				"DBNAME",
				"RLAN_POSN_IDX",
				"CALLSIGN",
				"FS_RX_LONGITUDE (deg)",
				"FS_RX_LATITUDE (deg)",
				"FS_RX_HEIGHT_ABOVE_TERRAIN (m)",
				"FS_RX_TERRAIN_HEIGHT (m)",
				"FS_RX_TERRAIN_SOURCE",
				"FS_RX_PROP_ENV",
				"FS_HAS_PASSIVE_REPEATER",
				"RLAN_LONGITUDE (deg)",
				"RLAN_LATITUDE (deg)",
				"RLAN_HEIGHT_ABOVE_TERRAIN (m)",
				"RLAN_TERRAIN_HEIGHT (m)",
				"RLAN_TERRAIN_SOURCE",
				"RLAN_PROP_ENV",
				"RLAN_FS_RX_DIST (km)",
				"RLAN_FS_RX_GROUND_DIST (km)",
				"RLAN_FS_RX_ELEVATION_ANGLE (deg)",
				"FS_RX_ANGLE_OFF_BORESIGHT (deg)",
				"RLAN_TX_EIRP (dBm)",
				"BODY_LOSS (dB)",
				"RLAN_CLUTTER_CATEGORY",
				"FS_CLUTTER_CATEGORY",
				"BUILDING TYPE",
				"RLAN_FS_RX_BUILDING_PENETRATION (dB)",
				"BUILDING_PENETRATION_MODEL",
				"BUILDING_PENETRATION_CDF",
				"PATH_LOSS (dB)",
				"PATH_LOSS_MODEL",
				"PATH_LOSS_CDF",
				"PATH_CLUTTER_TX (DB)",
				"PATH_CLUTTER_TX_MODEL",
				"PATH_CLUTTER_TX_CDF",
				"PATH_CLUTTER_RX (DB)",
				"PATH_CLUTTER_RX_MODEL",
				"PATH_CLUTTER_RX_CDF",
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
	const double exclusionDistKmSquared = (_exclusionDist / 1000.0) * (_exclusionDist / 1000.0);
	const double maxRadiusKmSquared = (_maxRadius / 1000.0) * (_maxRadius / 1000.0);

	Vector3 rlanPosnList[3];
	GeodeticCoord rlanCoordList[3];

#   if DEBUG_AFC
	char *tstr;

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

#			if DEBUG_AFC
			auto t1 = std::chrono::high_resolution_clock::now();
#			endif

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

			CConst::NLCDLandCatEnum nlcdLandCatTx;
			CConst::PropEnvEnum rlanPropEnv = computePropEnv(rlanLon, rlanLat, nlcdLandCatTx);

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
					std::string dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);
					std::cout << dbName << "_" << uls->getID() << std::endl;
				}
#endif

				if ((distKmSquared < maxRadiusKmSquared) && (distKmSquared > exclusionDistKmSquared) && (uls->getLinkDistance() > 0.0)) {
					_ulsIdxList.push_back(ulsIdx); // Store the ULS indices that are used in analysis
#if 0
					int ulsLonIdx;
					int ulsLatIdx;
					int regionIdx;
					char ulsRxPropEnv;
					_popGrid->findDeg(uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), ulsLonIdx, ulsLatIdx, ulsRxPropEnv, regionIdx);
					double ulsPopVal = _popGrid->getPop(ulsLonIdx, ulsLatIdx);
					if (ulsPopVal == 0.0) {
						ulsRxPropEnv = 'Z';
					}
#else
					char ulsRxPropEnv = ' ';
#endif

					CConst::ULSAntennaTypeEnum ulsRxAntennaType = uls->getRxAntennaType();

					int rlanPosnIdx;

					bool useACI = (channel.type == INQUIRED_FREQUENCY ? false : _aciFlag);
					CConst::SpectralAlgorithmEnum spectralAlgorithm = (channel.type == INQUIRED_FREQUENCY ? CConst::psdSpectralAlgorithm : _channelResponseAlgorithm);
					// LOGGER_INFO(logger) << "COMPUTING SPECTRAL OVERLAP FOR FSID = " << uls->getID();
					double spectralOverlapLossDB;
					bool hasOverlap = computeSpectralOverlapLoss(&spectralOverlapLossDB, chanStartFreq, chanStopFreq, uls->getStartUseFreq(), uls->getStopUseFreq(), useACI, spectralAlgorithm);

					if (hasOverlap > 0.0) {
						/**************************************************************************************/
						/* Determine propagation environment of FS, if needed.                                */
						/**************************************************************************************/
						CConst::NLCDLandCatEnum nlcdLandCatRx;
						CConst::PropEnvEnum fsPropEnv;
						if ((_applyClutterFSRxFlag) && (uls->getRxHeightAboveTerrain() <= _maxFsAglHeight)) {
							fsPropEnv = computePropEnv(uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), nlcdLandCatRx);
						} else {
							fsPropEnv = CConst::unknownPropEnv;
						}
						/**************************************************************************************/
						for (rlanPosnIdx = 0; rlanPosnIdx < numRlanPosn; ++rlanPosnIdx) {
							Vector3 rlanPosn = rlanPosnList[rlanPosnIdx];
							GeodeticCoord rlanCoord = rlanCoordList[rlanPosnIdx];
							lineOfSightVectorKm = ulsRxPos - rlanPosn;
							double distKm = lineOfSightVectorKm.len();
							double dAP = rlanPosn.len();
							double duls = ulsRxPos.len();
							double elevationAngleTxDeg = 90.0 - acos(rlanPosn.dot(lineOfSightVectorKm)/(dAP*distKm))*180.0/M_PI;
							double elevationAngleRxDeg = 90.0 - acos(ulsRxPos.dot(-lineOfSightVectorKm)/(duls*distKm))*180.0/M_PI;

							// Use Haversine formula with average earth radius of 6371 km
							double lon1Rad = rlanCoord.longitudeDeg*M_PI/180.0;
							double lat1Rad = rlanCoord.latitudeDeg*M_PI/180.0;
							double lon2Rad = uls->getRxLongitudeDeg()*M_PI/180.0;
							double lat2Rad = uls->getRxLatitudeDeg()*M_PI/180.0;
							double slat = sin((lat2Rad-lat1Rad)/2);
							double slon = sin((lon2Rad-lon1Rad)/2);
							double groundDistanceKm = 2*CConst::averageEarthRadius*asin(sqrt(slat*slat+cos(lat1Rad)*cos(lat2Rad)*slon*slon))*1.0e-3;

							double win2DistKm;
							if (_winner2UseGroundDistanceFlag) {
								win2DistKm = groundDistanceKm;
							} else {
								win2DistKm = distKm;
							}

							double fsplDistKm;
							if (_fsplUseGroundDistanceFlag) {
								fsplDistKm = groundDistanceKm;
							} else {
								fsplDistKm = distKm;
							}

							std::string buildingPenetrationModelStr;
							double buildingPenetrationCDF;
							double buildingPenetrationDB = computeBuildingPenetration(_buildingType, elevationAngleTxDeg, chanCenterFreq, buildingPenetrationModelStr, buildingPenetrationCDF, true);

							std::string txClutterStr;
							std::string rxClutterStr;
							std::string pathLossModelStr;
							double pathLossCDF;
							double pathLoss;
							std::string pathClutterTxModelStr;
							double pathClutterTxCDF;
							double pathClutterTxDB;
							std::string pathClutterRxModelStr;
							double pathClutterRxCDF;
							double pathClutterRxDB;

							double rlanHtAboveTerrain = rlanCoord.heightKm * 1000.0 - rlanTerrainHeight;

							computePathLoss(_pathLossModel, rlanPropEnv, fsPropEnv, nlcdLandCatTx, nlcdLandCatRx, distKm, fsplDistKm, win2DistKm, chanCenterFreq,
									rlanCoord.longitudeDeg, rlanCoord.latitudeDeg, rlanHtAboveTerrain, elevationAngleTxDeg,
									uls->getRxLongitudeDeg(), uls->getRxLatitudeDeg(), uls->getRxHeightAboveTerrain(), elevationAngleRxDeg,
									pathLoss, pathClutterTxDB, pathClutterRxDB, true,
									pathLossModelStr, pathLossCDF,
									pathClutterTxModelStr, pathClutterTxCDF, pathClutterRxModelStr, pathClutterRxCDF,
									(iturp452::ITURP452 *)NULL, &txClutterStr, &rxClutterStr, &(uls->ITMHeightProfile), &(uls->isLOSHeightProfile)
#if DEBUG_AFC
									, uls->ITMHeightType
#endif
									);

							std::string rxAntennaSubModelStr;
							double angleOffBoresightDeg = acos(uls->getAntennaPointing().dot(-(lineOfSightVectorKm.normalized()))) * 180.0 / M_PI;
							double rxGainDB = uls->computeRxGain(angleOffBoresightDeg, elevationAngleRxDeg, chanCenterFreq, rxAntennaSubModelStr, 0);

							double rxPowerDBW = (rlanEIRP - 30.0) - _bodyLossDB - buildingPenetrationDB - pathLoss - pathClutterTxDB - pathClutterRxDB + rxGainDB - spectralOverlapLossDB - _polarizationLossDB - uls->getRxAntennaFeederLossDB();

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
								double ulsLinkDistance = uls->getLinkDistance();
								double ulsWavelength = CConst::c / ((uls->getStartUseFreq() + uls->getStopUseFreq()) / 2);
								if (ulsLinkDistance != -1.0)
								{
									int numPR = uls->getNumPR();
									const Vector3 ulsTxPos = (numPR ? uls->getPR(numPR-1).position : uls->getTxPosition());
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
								if (ulsRxAntennaType == CConst::LUTAntennaType) {
									rxAntennaTypeStr = std::string(uls->getRxAntenna()->get_strid());
								} else {
									rxAntennaTypeStr = std::string(CConst::strULSAntennaTypeList->type_to_str(ulsRxAntennaType)) + rxAntennaSubModelStr;
								}

								std::string bldgTypeStr = (_fixedBuildingLossFlag ? "INDOOR_FIXED" :
										_buildingType == CConst::noBuildingType ? "OUTDOOR" :
										_buildingType == CConst::traditionalBuildingType ?  "TRADITIONAL" :
										"THERMALLY_EFFICIENT");

								std::string dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);

								QStringList msg;
								msg << QString::number(uls->getID()) << QString::fromStdString(dbName) << QString::number(rlanPosnIdx) << QString::fromStdString(uls->getCallsign());
								msg << QString::number(uls->getRxLongitudeDeg(), 'f', 5) << QString::number(uls->getRxLatitudeDeg(), 'f', 5);
								msg << QString::number(uls->getRxHeightAboveTerrain(), 'f', 2) << QString::number(uls->getRxTerrainHeight(), 'f', 2) <<
									QString::fromStdString(_terrainDataModel->getSourceName(uls->getRxHeightSource()))
									<< QString(ulsRxPropEnv);
								msg << QString::number(uls->getNumPR() ? 1 : 0);
								msg << QString::number(rlanCoord.longitudeDeg, 'f', 5) << QString::number(rlanCoord.latitudeDeg, 'f', 5);
								msg << QString::number(rlanCoord.heightKm * 1000.0 - rlanTerrainHeight, 'f', 2) << QString::number(rlanTerrainHeight, 'f', 2) <<
									QString::fromStdString(_terrainDataModel->getSourceName(rlanHeightSource))
									<<
									CConst::strPropEnvList->type_to_str(rlanPropEnv);
								msg << QString::number(distKm, 'f', 3) << QString::number(groundDistanceKm, 'f', 3) << QString::number(elevationAngleTxDeg, 'f', 3) << QString::number(angleOffBoresightDeg);
								msg << QString::number(rlanEIRP, 'f', 3) << QString::number(_bodyLossDB, 'f', 3) << QString::fromStdString(txClutterStr) << QString::fromStdString(rxClutterStr) << QString::fromStdString(bldgTypeStr);
								msg << QString::number(buildingPenetrationDB, 'f', 3) << QString::fromStdString(buildingPenetrationModelStr) << QString::number(buildingPenetrationCDF, 'f', 8);
								msg << QString::number(pathLoss, 'f', 3) << QString::fromStdString(pathLossModelStr) << QString::number(pathLossCDF, 'f', 8);

								msg << QString::number(pathClutterTxDB, 'f', 3) << QString::fromStdString(pathClutterTxModelStr) << QString::number(pathClutterTxCDF, 'f', 8);
								msg << QString::number(pathClutterRxDB, 'f', 3) << QString::fromStdString(pathClutterRxModelStr) << QString::number(pathClutterRxCDF, 'f', 8);

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
						if (uls->isLOSHeightProfile) {
							free(uls->isLOSHeightProfile);
							uls->isLOSHeightProfile = (double *) NULL;
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

#			if DEBUG_AFC
			auto t2 = std::chrono::high_resolution_clock::now();

			std::cout << " [" << numProc << " / " <<  totNumProc  << "] " << " Elapsed Time = " << std::setprecision(6) << std::chrono::duration_cast<std::chrono::duration<double>>(t2-t1).count() << std::endl << std::flush;

#			endif

			if (numProc == xN) {
				if (xN == 1) {
					tstart = std::chrono::high_resolution_clock::now();
					pctIdx = 1;
				} else if (!_progressFile.empty()) {
					auto tcurrent = std::chrono::high_resolution_clock::now();
					double elapsedTime = std::chrono::duration_cast<std::chrono::duration<double>>(tcurrent-tstart).count();
					double remainingTime = elapsedTime*(totNumProc-numProc)/(numProc-1);
					std::ostringstream progress;
					QByteArray progressBA;

					progress << (int) std::floor(100.0*numProc/totNumProc) << std::endl
						<< "Elapsed Time: " << (int) std::floor(elapsedTime)
						<< " s, Remaining: " << (int) std::floor(remainingTime) << " s";
					progressBA = QByteArray(progress.str().c_str());
					/* dont throw error */
					AfcManager::_dataIf->writeFile(QString::fromStdString(_progressFile), progressBA);

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

#   if DEBUG_AFC
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
	if (!AfcManager::_createDebugFiles) {
		return;
	}
	if (_responseCode != CConst::successResponseCode) {
		return;
	}
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
		fUserInputs->writeRow({ "HEIGHT_TYPE", (_rlanHeightType == CConst::AMSLHeightType ? "AMSL" : _rlanHeightType == CConst::AGLHeightType ? "AGL" : "INVALID") } );
		fUserInputs->writeRow({ "ALLOW_SCAN_PTS_IN_UNC_REG", (_allowScanPtsInUncRegFlag ? "true" : "false" ) } );
		fUserInputs->writeRow({ "INDOOR/OUTDOOR", (_rlanType == RLAN_INDOOR ? "indoor" : _rlanType == RLAN_OUTDOOR ? "outdoor" : "error") } );
		fUserInputs->writeRow({ "CHANNEL_RESPONSE_ALGORITHM", QString::fromStdString(CConst::strSpectralAlgorithmList->type_to_str(_channelResponseAlgorithm)) } );

		// fUserInputs->writeRow({ "ULS_DATABASE", QString(_inputULSDatabaseStr) } );
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
		fUserInputs->writeRow({ "FS_RECEIVER_ANTENNA_PATTERN_FILE", QString::fromStdString(_ulsAntennaPatternFile) } );
		fUserInputs->writeRow({ "FS_RECEIVER_DEFAULT_ANTENNA", QString::fromStdString(CConst::strULSAntennaTypeList->type_to_str(_ulsDefaultAntennaType )) } );
		fUserInputs->writeRow({ "RLAN_ITM_TX_CLUTTER_METHOD", QString::fromStdString(CConst::strITMClutterMethodList->type_to_str(_rlanITMTxClutterMethod)) } );

		fUserInputs->writeRow({ "PROPAGATION_MODEL", QString::fromStdString(CConst::strPathLossModelList->type_to_str(_pathLossModel)) } );
		fUserInputs->writeRow({ "WINNER_II_PROB_LOS_THRESHOLD", QString::number(_winner2ProbLOSThr, 'e', 20) } );
		fUserInputs->writeRow({ "WINNER_II_LOS_CONFIDENCE", QString::number(_confidenceWinner2LOS, 'e', 20) } );
		fUserInputs->writeRow({ "WINNER_II_NLOS_CONFIDENCE", QString::number(_confidenceWinner2NLOS, 'e', 20) } );
		fUserInputs->writeRow({ "WINNER_II_COMBINED_CONFIDENCE", QString::number(_confidenceWinner2Combined, 'e', 20) } );
		fUserInputs->writeRow({ "WINNER_II_HGT_FLAG", (_closeInHgtFlag ? "true" : "false" ) } );
		fUserInputs->writeRow({ "WINNER_II_HGT_LOS", QString::number(_closeInHgtLOS, 'e', 20) } );
		fUserInputs->writeRow({ "ITM_CONFIDENCE", QString::number(_confidenceITM, 'e', 20) } );
		fUserInputs->writeRow({ "ITM_RELIABILITY", QString::number(_reliabilityITM, 'e', 20) } );
		fUserInputs->writeRow({ "P.2108_CONFIDENCE", QString::number(_confidenceClutter2108, 'e', 20) } );
		fUserInputs->writeRow({ "WINNER_II_USE_GROUND_DISTANCE", (_winner2UseGroundDistanceFlag ? "true" : "false" ) } );
		fUserInputs->writeRow({ "FSPL_USE_GROUND_DISTANCE", (_fsplUseGroundDistanceFlag ? "true" : "false" ) } );
		fUserInputs->writeRow({ "PASSIVE_REPEATER_FLAG", (_passiveRepeaterFlag ? "true" : "false" ) } );

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
		fUserInputs->writeRow({ "VISIBILITY_THRESHOLD", QString::number(_visibilityThreshold, 'e', 20) } );

	}
	LOGGER_DEBUG(logger) << "User inputs written to userInputs.csv";
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::computePropEnv                                                             */
/******************************************************************************************/
CConst::PropEnvEnum AfcManager::computePropEnv(double lonDeg, double latDeg, CConst::NLCDLandCatEnum &nlcdLandCat, bool errorFlag) const
{
	// If user uses a density map versus a constant environmental input
	int lonIdx;
	int latIdx;
	CConst::PropEnvEnum propEnv;
	nlcdLandCat = CConst::unknownNLCDLandCat;
	if (_propagationEnviro.toStdString() == "NLCD Point" || _propagationEnviro.isEmpty()) { //.isEmpty() == true in DEBUG mode
		unsigned int landcat = cgNlcd->valueAt(latDeg, lonDeg);

		switch(landcat) {
			case 23:
			case 24:
				propEnv = CConst::urbanPropEnv;
				break;
			case 21:
			case 22:
				propEnv = CConst::suburbanPropEnv;
				break;
			case 41:
			case 43:
			case 90:
				nlcdLandCat = CConst::deciduousTreesNLCDLandCat;
				propEnv = CConst::ruralPropEnv;
				break;
			case 42:
				nlcdLandCat = CConst::coniferousTreesNLCDLandCat;
				propEnv = CConst::ruralPropEnv;
				break;
			case 52:
			case 82:
				nlcdLandCat = CConst::highCropFieldsNLCDLandCat;
				propEnv = CConst::ruralPropEnv;
				break;
			case 11:
			case 12:
			case 31:
			case 51:
			case 71:
			case 72:
			case 73:
			case 74:
			case 81:
			case 95:
				nlcdLandCat = CConst::noClutterNLCDLandCat;
				propEnv = CConst::ruralPropEnv;
				break;
			default:
				nlcdLandCat = CConst::villageCenterNLCDLandCat;
				propEnv = CConst::ruralPropEnv;
				break;
		}
	} else if (_propagationEnviro.toStdString() == "Population Density Map") {
		int regionIdx;
		char propEnvChar;
		_popGrid->findDeg(lonDeg, latDeg, lonIdx, latIdx, propEnvChar, regionIdx);

		switch (propEnvChar)
		{
			case 'U':
				propEnv = CConst::urbanPropEnv;
				break;
			case 'S':
				propEnv = CConst::suburbanPropEnv;
				break;
			case 'R':
				propEnv = CConst::ruralPropEnv;
				break;
			case 'B':
				propEnv = CConst::barrenPropEnv;
				break;
			case 'X':
				propEnv = CConst::unknownPropEnv;
				break;
			default:
				propEnv = CConst::unknownPropEnv;
				break;
		}

		// For constant set environments:
	} else if (_propagationEnviro.toStdString() == "Urban") {
		propEnv = CConst::urbanPropEnv;
	} else if (_propagationEnviro.toStdString() == "Suburban") {
		propEnv = CConst::suburbanPropEnv;
	} else if (_propagationEnviro.toStdString() == "Rural") {
		propEnv = CConst::ruralPropEnv;
	} else {
		throw std::runtime_error("Error in selecting a constant propagation environment (e.g. Urban)");
	}

	if ((propEnv == CConst::unknownPropEnv) && (errorFlag))
	{
		throw std::runtime_error(ErrStream() << "ERROR: RLAN Location LAT = " << latDeg << " LON = " << lonDeg << " outside Simulation Region defined by population density file");
	}

	return(propEnv);
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::computeClutter452HtEl                                                      */
/* See ITU-R p.452-16, Section 4.5.3                                                      */
/******************************************************************************************/
double AfcManager::computeClutter452HtEl(double txHeightM, double distKm, double elevationAngleDeg) const
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

	if ((elevationAngleDeg <= elevationAngleClutterLimitDeg) && (distKm > d_k * 10)) // If signal is below clutter
		return (A_h);                                                                               // Set path clutter loss
	else
		return (0.0); // Otherwise, no clutter loss
}
/**************************************************************************************/

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

	_minRlanHeightAboveTerrain = 1.5;

	_maxRadius = 150.0e3;
	_exclusionDist = 1.0;

	_illuminationEfficiency = 1.0;
	_closeInDist = 1.0e3;              // Radius in which close in path loss model is used
	_closeInPathLossModel = "WINNER2"; // Close in path loss model is used

	_wlanMinFreqMHz = 5925;
	_wlanMaxFreqMHz = 7125;

	// Hardcode to US for now.
	// When multiple countries are supported this need to come from AFC Configuration
	for (auto opClass: OpClass::USOpClass) {
		_opClass.push_back(opClass);
	}

	for (auto opClass: OpClass::PSDOpClassList) {
		_psdOpClassList.push_back(opClass);
	}

	_wlanMinFreq = _wlanMinFreqMHz*1.0e6;
	_wlanMaxFreq = _wlanMaxFreqMHz*1.0e6;

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

	_filterSimRegionOnly = false;

	// _rlanBWStr = "20.0e6,40.0e6,80.0e6,160.0e6"; // Channel bandwidths in Hz

	_worldPopulationFile = SearchPaths::forReading("data", "fbrat/rat_transfer/population/gpw_v4_population_density_rev11_2020_30_sec.tif", true).toStdString();
	_radioClimateFile = SearchPaths::forReading("data", "fbrat/rat_transfer/itudata/TropoClim.txt", true).toStdString();
	_surfRefracFile = SearchPaths::forReading("data", "fbrat/rat_transfer/itudata/N050.TXT", true).toStdString();
	_regionPolygonResolution = 1.0e-5;

	if (AfcManager::_createDebugFiles) {
		_excThrFile = QDir(QString::fromStdString(tempDir)).filePath("exc_thr.csv.gz").toStdString();
		_fsAnomFile = QDir(QString::fromStdString(tempDir)).filePath("fs_anom.csv.gz").toStdString();
		_userInputsFile = QDir(QString::fromStdString(tempDir)).filePath("userInputs.csv.gz").toStdString();
	}
	if (AfcManager::_createKmz) {
		_kmlFile = QDir(QString::fromStdString(tempDir)).filePath("results.kmz").toStdString();
	}
	/**************************************************************************************/
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
			int minNextFreq = stopFreqMHz;
			for(auto channel : _channelList) {
				if ( (channel.type == INQUIRED_FREQUENCY) && (channel.availability != BLACK) && (channel.availability != RED) ) {
					if ((channel.startFreqMHz <= prevFreqMHz) && (channel.stopFreqMHz > prevFreqMHz)) {
						double psd = channel.eirpLimit_dBm - 10.0*log((double) channel.bandwidth())/log(10.0);
						if ((initFlag) || (psd < minPSD)) {
							minPSD = psd;
						}
						if (channel.stopFreqMHz < nextFreqMHz) {
							nextFreqMHz = channel.stopFreqMHz;
						}
						initFlag = false;
					} else if ((channel.startFreqMHz > prevFreqMHz) && (initFlag)) {
						if (channel.startFreqMHz < minNextFreq) {
							minNextFreq = channel.startFreqMHz;
						}
					}
				}
			}
			if (initFlag) {
				// throw std::runtime_error(std::string("Error computing PSD over inquired frequency range ")
				//     + "[" + std::to_string(startFreqMHz) + "," + std::to_string(stopFreqMHz) + "]"
				//     + ", problem at frequency " + std::to_string(prevFreqMHz)
				// );
				nextFreqMHz = minNextFreq;
				minPSD = std::numeric_limits<double>::quiet_NaN();
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
	int totalNumChan = 0;

	for(auto freqRange : _inquiredFrquencyRangesMHz) {
		auto inquiredStartFreqMHz = freqRange.first;
		auto inquiredStopFreqMHz = freqRange.second;

		if (inquiredStopFreqMHz <= inquiredStartFreqMHz) {
			LOGGER_DEBUG(logger) << "Inquired Freq Range INVALID: [" << inquiredStartFreqMHz << ", " << inquiredStopFreqMHz << "] stop freq must be > start freq" << std::endl;
			_responseCode = CConst::unsupportedSpectrumResponseCode;
			return;
		}

		if (containsChannel(_allowableFreqBandList, inquiredStartFreqMHz, inquiredStopFreqMHz)) {
			int numChan;

			numChan = 0;

			// Iterate each operating classes and add all channels within the given frequency range
			for (auto &opClass: _psdOpClassList)
			{
				for (auto &cfi: opClass.channels)
				{
					ChannelStruct channel;
					channel.startFreqMHz = (opClass.startFreq + 5 * cfi) - (opClass.bandWidth >> 1);;
					channel.stopFreqMHz = channel.startFreqMHz + opClass.bandWidth;

					if (containsChannel(_allowableFreqBandList, channel.startFreqMHz, channel.stopFreqMHz)) {
						if ( (channel.stopFreqMHz > inquiredStartFreqMHz) && (channel.startFreqMHz < inquiredStopFreqMHz) ) {
							channel.operatingClass = opClass.opClass;
							channel.index = cfi;
							channel.availability = GREEN; // Everything initialized to GREEN
							channel.eirpLimit_dBm = 0;
							channel.type = INQUIRED_FREQUENCY;
							_channelList.push_back(channel);
							numChan++;
							totalNumChan++;
						}
					}
				}
			}

			if (numChan == 0) {
				LOGGER_DEBUG(logger) << "Inquired Freq Range: [" << inquiredStartFreqMHz << ", " << inquiredStopFreqMHz << "] contains no channels" << std::endl;
				_responseCode = CConst::invalidValueResponseCode;
				_invalidParams << "lowFrequency" << "highFrequency";
				return;
			}
		} else {
			// the start/stop frequencies are not valid
			LOGGER_DEBUG(logger) << "Inquired Freq Range INVALID: [" << inquiredStartFreqMHz << ", " << inquiredStopFreqMHz << "]" << std::endl;
			_responseCode = CConst::unsupportedSpectrumResponseCode;
			return;
		}
	}

	for(auto channelPair : _inquiredChannels) {
		LOGGER_DEBUG(logger) << "creating channels for operating class " << channelPair.first;

		int numChan;
		numChan = 0;

		// Iterate each operating classes and add all channels of given operating class
		for (auto &opClass: _opClass)
		{
			// Skip of classes of not in inquired channel list
			if (opClass.opClass != channelPair.first) {
				continue;
			}

			for (auto &cfi: opClass.channels)
			{
				bool includeChannel;

				includeChannel = false;

				// If channel indexes are provided check for channel validity.
				// If channel indexes are not provided then include all channels of
				// given operating class.
				if (channelPair.second.size() != 0) {
					for(auto inquired_cfi : channelPair.second) {
						if (inquired_cfi == cfi) {
							includeChannel = true;
							break;
						}
					}
				} else {
					includeChannel = true;
				}

				if (includeChannel) {
					ChannelStruct channel;
					channel.startFreqMHz = (opClass.startFreq + 5 * cfi) - (opClass.bandWidth >> 1);;
					channel.stopFreqMHz = channel.startFreqMHz + opClass.bandWidth;

					// Include channel if it is within the frequency bands in AFC config
					if (containsChannel(_allowableFreqBandList, channel.startFreqMHz, channel.stopFreqMHz)) {
						channel.operatingClass = opClass.opClass;
						channel.index = cfi;

						channel.availability = GREEN; // Everything initialized to GREEN
						channel.eirpLimit_dBm = 0;
						channel.type = ChannelType::INQUIRED_CHANNEL;
						_channelList.push_back(channel);
						numChan++;
						totalNumChan++;
						LOGGER_DEBUG(logger) << "added " << numChan << " channels";
					}
				}
			}
			if (numChan == 0) {
				LOGGER_DEBUG(logger) << "Inquired Channel INVALID Operating Class: " << channelPair.first << std::endl;
				_responseCode = CConst::unsupportedSpectrumResponseCode;
				return;
			}
		}

		if (totalNumChan == 0) {
			LOGGER_DEBUG(logger) << "Missing valid Inquired channel and frequency " << std::endl;
			_responseCode = CConst::missingParamResponseCode;
			return;
		}
	}
}
/**************************************************************************************/

/**************************************************************************************/
/* AfcManager::containsChannel()                                                      */
/**************************************************************************************/
bool AfcManager::containsChannel(const std::vector<FreqBandClass>& freqBandList, int chanStartFreqMHz, int chanStopFreqMHz)
{
	auto segmentList = std::vector<std::pair<int,int>> { std::make_pair(chanStartFreqMHz, chanStopFreqMHz) };

	for(auto freqBand : freqBandList) {
		int bandStart = freqBand.getStartFreqMHz();
		int bandStop  = freqBand.getStopFreqMHz();
		int segIdx=0;
		while(segIdx < (int) segmentList.size()) {
			int segStart = segmentList[segIdx].first;
			int segStop  = segmentList[segIdx].second;

			if ((bandStop <= segStart) || (bandStart >= segStop)) {
				// No overlap, do nothing
				segIdx++;
			} else if (    (bandStart <= segStart)
					&& (bandStop  >= segStop ) ) {
				// Remove segment
				segmentList.erase(segmentList.begin() + segIdx);
			} else if (bandStart <= segStart) {
				// Clip bottom of segment
				segmentList[segIdx] = std::make_pair(bandStop, segStop);
				segIdx++;
			} else if (bandStop >= segStop) {
				// Clip top of segment
				segmentList[segIdx] = std::make_pair(segStart, bandStart);
				segIdx++;
			} else {
				// Split Segment
				segmentList[segIdx] = std::make_pair(segStart, bandStart);
				segmentList.insert(segmentList.begin()+segIdx+1, std::make_pair(bandStop, segStop));
				segIdx+=2;
			}
		}
	}

	bool containsFlag = (segmentList.size() == 0);

	return(containsFlag);
}
/**************************************************************************************/

#if DEBUG_AFC
/******************************************************************************************/
/* AfcManager::runTestWinner2()                                                           */
/******************************************************************************************/
void AfcManager::runTestWinner2(std::string inputFile, std::string outputFile)
{
	LOGGER_INFO(logger) << "Executing AfcManager::runTestWinner2()";

	int linenum, fIdx, validFlag;
	std::string line, strval;
	char *chptr;
	std::string str;
	std::string reasonIgnored;
	std::ostringstream errStr;

	int regionFieldIdx = -1;
	int distanceFieldIdx = -1;
	int hbFieldIdx = -1;
	int hmFieldIdx = -1;
	int frequencyFieldIdx = -1;
	int confidenceFieldIdx = -1;

	std::vector<int *> fieldIdxList;                       std::vector<std::string> fieldLabelList;
	fieldIdxList.push_back(&regionFieldIdx);               fieldLabelList.push_back("Region");
	fieldIdxList.push_back(&distanceFieldIdx);             fieldLabelList.push_back("Distance (m)");
	fieldIdxList.push_back(&hbFieldIdx);                   fieldLabelList.push_back("hb (m)");
	fieldIdxList.push_back(&hmFieldIdx);                   fieldLabelList.push_back("hm (m)");
	fieldIdxList.push_back(&frequencyFieldIdx);            fieldLabelList.push_back("Frequency (GHz)");
	fieldIdxList.push_back(&confidenceFieldIdx);           fieldLabelList.push_back("Confidence");

	CConst::PropEnvEnum propEnv;
	double distance;
	double hb;
	double hm;
	double frequency;
	double confidence;

	int winner2LOSValue = 0;
	_winner2UnknownLOSMethod = CConst::PLOSCombineWinner2UnknownLOSMethod;
	double plLOS, plNLOS, plCombined;
	double zval, probLOS, pathLossCDF;
	double sigmaLOS, sigmaNLOS, sigmaCombined;
	std::string pathLossModelStr;

	int fieldIdx;

	if (inputFile.empty()) {
		throw std::runtime_error("ERROR: No Winner2 Test File specified");
	}

	LOGGER_INFO(logger) << "Reading Winner2 Test File: " << inputFile;

	FILE *fin;
	if ( !(fin = fopen(inputFile.c_str(), "rb")) ) {
		str = std::string("ERROR: Unable to open Winner2 Test File \"") + inputFile + std::string("\"\n");
		throw std::runtime_error(str);
	}


	FILE *fout;
	if ( !(fout = fopen(outputFile.c_str(), "wb")) ) {
		errStr << std::string("ERROR: Unable to open Winner2 Test Output File \"") + outputFile + std::string("\"\n");
		throw std::runtime_error(errStr.str());
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
	while (fgetline(fin, line, false)) {
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
						errStr << "ERROR: Invalid Winner2 Test Input file \"" << inputFile << "\" label line missing \"" << fieldLabelList.at(fIdx) << "\"" << std::endl;
						throw std::runtime_error(errStr.str());
					}
				}

				fprintf(fout, "%s,%s,%s,%s,%s\n", line.c_str(), "afc_probLOS", "afc_plLOS", "afc_plNLOS", "afc_plCombined");

				break;
			case    dataLineType:
				/**************************************************************************/
				/* REGION (propEnv)                                                        */
				/**************************************************************************/
				strval = fieldList.at(regionFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid Winner2 Test Input file \"" << inputFile << "\" line " << linenum << " missing REGION" << std::endl;
					throw std::runtime_error(errStr.str());
				}

				propEnv = (CConst::PropEnvEnum) CConst::strPropEnvList->str_to_type(strval, validFlag);

				if (!validFlag) {
					errStr << "ERROR: Invalid Winner2 Test Input file \"" << inputFile << "\" line " << linenum << " INVALID REGION = " << strval << std::endl;
					throw std::runtime_error(errStr.str());
				}
				/**************************************************************************/

				/**************************************************************************/
				/* distance                                                               */
				/**************************************************************************/
				strval = fieldList.at(distanceFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid Winner2 Test Input file \"" << inputFile << "\" line " << linenum << " missing Distance (m)" << std::endl;
					throw std::runtime_error(errStr.str());
				}
				distance = std::strtod(strval.c_str(), &chptr);
				/**************************************************************************/

				/**************************************************************************/
				/* hb                                                               */
				/**************************************************************************/
				strval = fieldList.at(hbFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid Winner2 Test Input file \"" << inputFile << "\" line " << linenum << " missing hb" << std::endl;
					throw std::runtime_error(errStr.str());
				}
				hb = std::strtod(strval.c_str(), &chptr);
				/**************************************************************************/

				/**************************************************************************/
				/* hm                                                               */
				/**************************************************************************/
				strval = fieldList.at(hmFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid Winner2 Test Input file \"" << inputFile << "\" line " << linenum << " missing hm" << std::endl;
					throw std::runtime_error(errStr.str());
				}
				hm = std::strtod(strval.c_str(), &chptr);
				/**************************************************************************/

				/**************************************************************************/
				/* frequency                                                              */
				/**************************************************************************/
				strval = fieldList.at(frequencyFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid Winner2 Test Input file \"" << inputFile << "\" line " << linenum << " missing Frequency (GHz)" << std::endl;
					throw std::runtime_error(errStr.str());
				}
				frequency = std::strtod(strval.c_str(), &chptr)*1.0e9; // Convert GHz to Hz
				/**************************************************************************/

				/**************************************************************************/
				/* confidence                                                             */
				/**************************************************************************/
				strval = fieldList.at(confidenceFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid Winner2 Test Input file \"" << inputFile << "\" line " << linenum << " missing Confidence" << std::endl;
					throw std::runtime_error(errStr.str());
				}
				confidence = std::strtod(strval.c_str(), &chptr);
				/**************************************************************************/


				zval = -qerfi(confidence);
				_zwinner2Combined = zval;
				if (propEnv == CConst::urbanPropEnv) {
					plLOS      = Winner2_C2urban_LOS( distance, hb, hm, frequency, true, zval, sigmaLOS, pathLossCDF);
					plNLOS     = Winner2_C2urban_NLOS(distance, hb, hm, frequency, true, zval, sigmaNLOS, pathLossCDF);
					plCombined = Winner2_C2urban(     distance, hb, hm, frequency, true, sigmaCombined, pathLossModelStr, pathLossCDF, probLOS, winner2LOSValue);
				} else if (propEnv == CConst::suburbanPropEnv) {
					plLOS      = Winner2_C1suburban_LOS( distance, hb, hm, frequency, true, zval, sigmaLOS, pathLossCDF);
					plNLOS     = Winner2_C1suburban_NLOS(distance, hb, hm, frequency, true, zval, sigmaNLOS, pathLossCDF);
					plCombined = Winner2_C1suburban(     distance, hb, hm, frequency, true, sigmaCombined, pathLossModelStr, pathLossCDF, probLOS, winner2LOSValue);
				} else if (propEnv == CConst::ruralPropEnv) {
					plLOS      = Winner2_D1rural_LOS( distance, hb, hm, frequency, true, zval, sigmaLOS, pathLossCDF);
					plNLOS     = Winner2_D1rural_NLOS(distance, hb, hm, frequency, true, zval, sigmaNLOS, pathLossCDF);
					plCombined = Winner2_D1rural(     distance, hb, hm, frequency, true, sigmaCombined, pathLossModelStr, pathLossCDF, probLOS, winner2LOSValue);
				} else {
					CORE_DUMP;
				}

				if (distance < 50.0) {
					probLOS = 1.0;
					plCombined = plLOS;
				}

				fprintf(fout, "%s,%.3f,%3f,%.3f,%.3f\n", line.c_str(), probLOS, plLOS, plNLOS, plCombined);

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

	if (fin) { fclose(fin); }
	if (fout) { fclose(fout); }

	return;
}
/******************************************************************************************/
#endif

#if DEBUG_AFC
/******************************************************************************************/
/* AfcManager::runAnalyzeNLCD()                                                           */
/******************************************************************************************/
void AfcManager::runAnalyzeNLCD()
{
	LOGGER_INFO(logger) << "Executing AfcManager::runAnalyzeNLCD()";

	Vector3 esPosn, satPosn, apPosn;
	Vector3 d, projd, eastVec, northVec, zVec;
	Vector3 esEast, esNorth, esZ, esBoresight;
	std::string str;
	std::ostringstream errStr;
	std::vector<double> probList;

#define DBG_POSN 0

	FILE *fkml = (FILE *) NULL;
	/**************************************************************************************/
	/* Open KML File and write header                                                     */
	/**************************************************************************************/
	if (1) {
		if ( !(fkml = fopen("/tmp/doc.kml", "wb")) ) {
			errStr << std::string("ERROR: Unable to open kmlFile \"") + "/tmp/doc.kml" + std::string("\"\n");
			throw std::runtime_error(errStr.str());
		}
	} else {
		fkml = (FILE *)NULL;
	}

	if (fkml) {
		fprintf(fkml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		fprintf(fkml, "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n");
		fprintf(fkml, "\n");
		fprintf(fkml, "    <Document>\n");
		fprintf(fkml, "        <name>Analyze NLCD</name>\n");
		fprintf(fkml, "        <open>1</open>\n");
		fprintf(fkml, "        <description>%s : Show NLCD categories.</description>\n", "TEST");
		fprintf(fkml, "\n");
		fprintf(fkml, "        <Style id=\"style\">\n");
		fprintf(fkml, "                <LineStyle>\n");
		fprintf(fkml, "                        <color>00000000</color>\n");
		fprintf(fkml, "                </LineStyle>\n");
		fprintf(fkml, "                <PolyStyle>\n");
		fprintf(fkml, "                        <color>80ffaa55</color>\n");
		fprintf(fkml, "                </PolyStyle>\n");
		fprintf(fkml, "        </Style>\n");
		fprintf(fkml, "        <Style id=\"style0\">\n");
		fprintf(fkml, "                <LineStyle>\n");
		fprintf(fkml, "                        <color>00000000</color>\n");
		fprintf(fkml, "                </LineStyle>\n");
		fprintf(fkml, "                <PolyStyle>\n");
		fprintf(fkml, "                        <color>80ffaa55</color>\n");
		fprintf(fkml, "                </PolyStyle>\n");
		fprintf(fkml, "        </Style>\n");
		fprintf(fkml, "        <StyleMap id=\"stylemap_id1\">\n");
		fprintf(fkml, "                <Pair>\n");
		fprintf(fkml, "                        <key>normal</key>\n");
		fprintf(fkml, "                        <styleUrl>#style0</styleUrl>\n");
		fprintf(fkml, "                </Pair>\n");
		fprintf(fkml, "                <Pair>\n");
		fprintf(fkml, "                        <key>highlight</key>\n");
		fprintf(fkml, "                        <styleUrl>#style</styleUrl>\n");
		fprintf(fkml, "                </Pair>\n");
		fprintf(fkml, "        </StyleMap>\n");
		fprintf(fkml, "        <Style id=\"dotStyle\">\n");
		fprintf(fkml, "            <IconStyle>\n");
		fprintf(fkml, "                <color>ff0000ff</color>\n");
		fprintf(fkml, "                <Icon>\n");
		fprintf(fkml, "                    <href>redDot.png</href>\n");
		fprintf(fkml, "                </Icon>\n");
		fprintf(fkml, "            </IconStyle>\n");
		fprintf(fkml, "            <LabelStyle>\n");
		fprintf(fkml, "                 <scale>0</scale>\n");
		fprintf(fkml, "             </LabelStyle>\n");
		fprintf(fkml, "        </Style> \n");
	}
	/**************************************************************************************/

	GdalTransform::BoundRect nlcdBr(cgNlcd->boundRect());
	std::cout << "    NLCD_TOP_RIGHT: "   << nlcdBr.lonDegMax << " " << nlcdBr.latDegMax << std::endl;
	std::cout << "    NLCD_BOTTOM_LEFT: " << nlcdBr.lonDegMin << " " << nlcdBr.latDegMin << std::endl;

	std::vector<std::string> colorList;

	int i;
	for(i=0; i<255; i++) {
		std::string color;
		switch(i) {
			case 21: color = "221 201 201"; break;
			case 22: color = "216 147 130"; break;
			case 23: color = "237   0   0"; break;
			case 31: color = "178 173 163"; break;
			case 32: color = "249 249 249"; break;
			case 41: color = "104 170  99"; break;
			case 42: color = " 28  99  48"; break;
			case 43: color = "181 201 142"; break;
			case 52: color = "204,186,124"; break;

			case  1: color = "  0 249   0"; break;
			case 11: color = " 71 107 160"; break;
			case 12: color = "209 221 249"; break;
			case 24: color = "170   0   0"; break;
			case 51: color = "165 140  48"; break;
			case 71: color = "226 226 193"; break;
			case 72: color = "201 201 119"; break;
			case 73: color = "153 193  71"; break;
			case 74: color = "119 173 147"; break;
			case 81: color = "219 216  61"; break;
			case 82: color = "170 112  40"; break;
			case 90: color = "186 216 234"; break;
			case 91: color = "181 211 229"; break;
			case 92: color = "181 211 229"; break;
			case 93: color = "181 211 229"; break;
			case 94: color = "181 211 229"; break;
			case 95: color = "112 163 186"; break;

			default: color = "255 255 255"; break;
		}
		colorList.push_back(color);
	}

	char *tstr;
	time_t t0 = time(NULL);
	tstr = strdup(ctime(&t0));
	strtok(tstr, "\n");
	std::cout << tstr << " : ITERATION START." << std::endl;
	free(tstr);

	/**************************************************************************************/
	/* Write FS RLAN Regions to KML                                                       */
	/**************************************************************************************/
	double resolutionLon = (30.0 / CConst::earthRadius)*180.0/M_PI;
	double resolutionLat = (30.0 / CConst::earthRadius)*180.0/M_PI;

	std::vector<std::string> imageFileList;

	/**************************************************************************************/
	/* Define regions                                                                     */
	/**************************************************************************************/
	int maxPtsPerRegion = 5000;

	double longitudeDegStart = _popGrid->getMinLonDeg();
	double latitudeDegStart = _popGrid->getMinLatDeg();

	int numLon = (_popGrid->getMaxLonDeg() - longitudeDegStart)/resolutionLon;
	int numLat = (_popGrid->getMaxLatDeg() -  latitudeDegStart)/resolutionLat;

	int numRegionLon = (numLon + maxPtsPerRegion - 1)/maxPtsPerRegion;
	int numRegionLat = (numLat + maxPtsPerRegion - 1)/maxPtsPerRegion;

	int lonRegionIdx;
	int latRegionIdx;
	int startLonIdx, stopLonIdx;
	int startLatIdx, stopLatIdx;
	int lonN = numLon/numRegionLon;
	int lonq = numLon%numRegionLon;
	int latN = numLat/numRegionLat;
	int latq = numLat%numRegionLat;
	/**************************************************************************************/

	std::cout << "    NUM_REGION_LON: " << numRegionLon << std::endl;
	std::cout << "    NUM_REGION_LAT: " << numRegionLat << std::endl;

	int interpolationFactor = 1;
	int interpLon, interpLat;

	fprintf(fkml, "        <Folder>\n");
	fprintf(fkml, "            <name>%s</name>\n", "NLCD");
	fprintf(fkml, "            <visibility>1</visibility>\n");

	for(lonRegionIdx=0; lonRegionIdx<numRegionLon; lonRegionIdx++) {

		if (lonRegionIdx < lonq) {
			startLonIdx = (lonN+1)*lonRegionIdx;
			stopLonIdx  = (lonN+1)*lonRegionIdx+lonN;
		} else {
			startLonIdx = lonN*lonRegionIdx + lonq;
			stopLonIdx  = lonN*lonRegionIdx + lonq + lonN - 1;
		}

		for(latRegionIdx=0; latRegionIdx<numRegionLat; latRegionIdx++) {

			if (latRegionIdx < latq) {
				startLatIdx = (latN+1)*latRegionIdx;
				stopLatIdx  = (latN+1)*latRegionIdx+latN;
			} else {
				startLatIdx = latN*latRegionIdx + latq;
				stopLatIdx  = latN*latRegionIdx + latq + latN - 1;
			}

			/**************************************************************************************/
			/* Create PPM File                                                                    */
			/**************************************************************************************/
			FILE *fppm;
			if ( !(fppm = fopen("/tmp/image.ppm", "wb")) ) {
				throw std::runtime_error("ERROR");
			}
			fprintf(fppm, "P3\n");
			fprintf(fppm, "%d %d %d\n", (stopLonIdx-startLonIdx+1)*interpolationFactor, (stopLatIdx-startLatIdx+1)*interpolationFactor, 255);

			int latIdx, lonIdx;
			for(latIdx=stopLatIdx; latIdx>=startLatIdx; --latIdx) {
				double latDeg = latitudeDegStart + (latIdx + 0.5)*resolutionLon;
				for(interpLat=interpolationFactor-1; interpLat>=0; --interpLat) {
					for(lonIdx=startLonIdx; lonIdx<=stopLonIdx; ++lonIdx) {
						double lonDeg = longitudeDegStart + (lonIdx + 0.5)*resolutionLon;

						unsigned int landcat = (unsigned int)cgNlcd->valueAt(latDeg, lonDeg);

						// printf("LON = %.6f LAT = %.6f LANDCAT = %d\n", lonDeg, latDeg, landcat);

						std::string colorStr = colorList[landcat];

						for(interpLon=0; interpLon<interpolationFactor; interpLon++) {
							if (lonIdx || interpLon) { fprintf(fppm, " "); }
							fprintf(fppm, "%s", colorStr.c_str());
						}
					}
					fprintf(fppm, "\n");
				}
			}

			fclose(fppm);
			/**************************************************************************************/

			std::string pngFile = std::string("/tmp/image")
				+ "_" + std::to_string(lonRegionIdx)
				+ "_" + std::to_string(latRegionIdx) + ".png";

			imageFileList.push_back(pngFile);

			std::string command = "convert /tmp/image.ppm -transparent white " + pngFile;
			std::cout << "COMMAND: " << command << std::endl;
			system(command.c_str());

			/**************************************************************************************/
			/* Write to KML File                                                                  */
			/**************************************************************************************/
			fprintf(fkml, "<GroundOverlay>\n");
			fprintf(fkml, "    <name>Region: %d_%d</name>\n", lonRegionIdx, latRegionIdx);
			fprintf(fkml, "    <visibility>%d</visibility>\n", 1);
			fprintf(fkml, "    <color>C0ffffff</color>\n");
			fprintf(fkml, "    <Icon>\n");
			fprintf(fkml, "        <href>image_%d_%d.png</href>\n", lonRegionIdx, latRegionIdx);
			fprintf(fkml, "    </Icon>\n");
			fprintf(fkml, "    <LatLonBox>\n");
			fprintf(fkml, "        <north>%.8f</north>\n", latitudeDegStart  + (stopLatIdx + 1)*resolutionLat);
			fprintf(fkml, "        <south>%.8f</south>\n", latitudeDegStart  + (startLatIdx   )*resolutionLat);
			fprintf(fkml, "        <east>%.8f</east>\n",   longitudeDegStart + (stopLonIdx + 1)*resolutionLon);
			fprintf(fkml, "        <west>%.8f</west>\n",   longitudeDegStart + (startLonIdx   )*resolutionLon);
			fprintf(fkml, "    </LatLonBox>\n");
			fprintf(fkml, "</GroundOverlay>\n");
			/**************************************************************************************/
		}
	}

	fprintf(fkml, "        </Folder>\n");
	/**************************************************************************************/

	/**************************************************************************************/
	/* Write end of KML and close                                                         */
	/**************************************************************************************/
	if (fkml) {
		fprintf(fkml, "    </Document>\n");
		fprintf(fkml, "</kml>\n");
		fclose(fkml);

		std::string command;

		std::cout << "CLEARING KMZ FILE: " << std::endl;
		command = "rm -fr " + _kmlFile;
		system(command.c_str());

		command = "zip -j " + _kmlFile + " /tmp/doc.kml ";

		for(int imageIdx=0; imageIdx<((int) imageFileList.size()); imageIdx++) {
			command += " " + imageFileList[imageIdx];
		}
		std::cout << "COMMAND: " << command.c_str() << std::endl;
		system(command.c_str());
	}
	/**************************************************************************************/

	delete _popGrid;

	_popGrid = (PopGridClass *) NULL;
}
/******************************************************************************************/
#endif

