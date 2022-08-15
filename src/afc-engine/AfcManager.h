// mainUtilities.h -- Reads in and writes JSON files, assigns values to
// InputParameters structure for main.cpp and exports calculations from main.cpp
#ifndef INCLUDE_AFCMANAGER_H
#define INCLUDE_AFCMANAGER_H

#ifndef DEBUG_AFC
#define DEBUG_AFC 0
#endif

#define USE_BUILDING_RASTER 1

// Standard library
#include <stdlib.h>
#include <algorithm>
#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>
#include <ctime>
#include <exception>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <map>
#include <chrono>
#include <limits>
#include <utility>
#include <vector>
// AFC Engine
#include "antenna.h"
#include "cconst.h"
#include "EcefModel.h"
#include "freq_band.h"
#include "global_fn.h"
#include "WorldData.h"
#include "GdalHelpers.h"
#include "GdalDataDir.h"
#include "GdalDataModel.h"
#include "GdalImageFile2.h"
#include "BuildingRasterModel.h"
#include "ras.h"
#include "readITUFiles.hpp"
#include "str_type.h"
#include "UlsDatabase.h"
#include "uls.h"
#include "UlsMeasurementAnalysis.h"
#include "terrain.h"
// Loggers
#include "rkflogging/ErrStream.h"
#include "rkflogging/Logging.h"
#include "rkflogging/LoggingConfig.h"
// rat common
#include "ratcommon/SearchPaths.h"
#include "ratcommon/CsvWriter.h"
#include "ratcommon/FileHelpers.h"
#include "ratcommon/GzipStream.h"
#include "ratcommon/ZipWriter.h"
// Qt
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QByteArray>
#include <QTemporaryDir>
#include <QRectF>
// ITU-R P.452
#include <iturp452/ITURP452.h>
// Uses OGR from GDAL v1.11
#include "gdal/ogrsf_frmts.h"
#include "data_if.h"

#include "AfcDefinitions.h"

template<class T> class ListClass;
class ULSClass;
class GdalDataDir;
class GdalDataModel;
class BuildingRasterModel;
class WorldData;
class PopGridClass;
class AntennaClass;
class RlanRegionClass;

namespace OpClass
{
	class OpClass
	{
		public:
			const int opClass;
			const int bandWidth;
			const int startFreq;
			const std::vector<int> channels;
	};
}

class AfcManager
{
	public:  
		AfcManager();
		~AfcManager();

		bool isNull() { // Checks if there are NaN values in any of the required inputs
			double lat, lon, alt, minorUncert, majorUncert, heightUncert;
			std::tie(lat,lon,alt) = _rlanLLA;
			std::tie(minorUncert,majorUncert,heightUncert) = _rlanUncerts_m;
			return (((_rlanUncertaintyRegionType == RLANBoundary::ELLIPSE || _rlanUncertaintyRegionType == RLANBoundary::RADIAL_POLY) && (std::isnan(lat) || std::isnan(lon))) || 
					(_analysisType != "HeatmapAnalysis" && std::isnan(alt)) || 
					(_rlanUncertaintyRegionType == RLANBoundary::ELLIPSE && (std::isnan(minorUncert) || std::isnan(majorUncert))) || std::isnan(heightUncert) ||
					std::isnan(_minEIRP_dBm) || std::isnan(_maxEIRP_dBm) || std::isnan(_IoverN_threshold_dB) ||
					std::isnan(_bodyLossDB) || std::isnan(_polarizationLossDB) ||
					(_rlanUncertaintyRegionType == RLANBoundary::ELLIPSE && std::isnan(_rlanOrientation_deg)) ||
					(_ulsDatabaseList.size() == 0) ||
					std::isnan((int)_buildingType) || std::isnan(_confidenceBldg2109) ||
					_pathLossModel==CConst::unknownPathLossModel);// || 
			//std::isnan(_confidenceClutter2108) || std::isnan(_confidenceWinner2) || std::isnan(_confidenceITM) || std::isnan(_winner2ProbLOSThr));
		}

		void setAnalysisType(std::string analysisTypeVal) { _analysisType = analysisTypeVal; return; }
		void setStateRoot(std::string stateRootVal) { _stateRoot = stateRootVal; return; }

		// Read all of the database information into the AfcManager
		void initializeDatabases();

		// Start assigning the values from json object into the vars in struct
		void importGUIjson(const std::string &inputJSONpath);

		// Read configuration parameters for AFC
		void importConfigAFCjson(const std::string &inputJSONpath, const std::string &tempDir);

		// create JSON object for exclusion zone to be sent to GUI
		QJsonDocument generateExclusionZoneJson();

		// create JSON object for heat map to be sent to GUI
		QJsonDocument generateHeatmap();

		// create JSON object for new AFC specification
		QJsonDocument generateRatAfcJson();

		// add building database bounds to OGR layer
		void addBuildingDatabaseTiles(OGRLayer *layer);

		// Export calculations and FS locations in geoJSON format for GUI
		void exportGUIjson(const QString &exportJsonPath, const std::string& tempDir);

		// Generate map data geojson file
		void generateMapDataGeoJson(const std::string& tempDir);

		// Print the user inputs for testing and debugging
		void printUserInputs();

		ULSClass *findULSID(int ulsID, int dbIdx);

		// ***** Perform AFC Engine computations *****
		void compute(); // Computes all the necessary losses and stores them into the output member variables below

		double computeClutter452HtEl(double txHeightM, double distKm, double elevationAngleRad) const; // Clutter loss from ITU-R p.452

		std::tuple<LatLon, LatLon, LatLon> computeBeamConeLatLon(ULSClass *uls); // Calculates and stores the beam cone coordinates

		// Support command line interface with AFC Engine
		void setCmdLineParams(std::string &inputFilePath, std::string &configFilePath, std::string &outputFilePath, std::string &tempDir, std::string &logLevel, int argc, char **argv);

		void setConstInputs(const std::string& tempDir); // set inputs not specified by user

		void setFixedBuildingLossFlag(bool fixedBuildingLossFlag) { _fixedBuildingLossFlag = fixedBuildingLossFlag; }
		void setFixedBuildingLossValue(double fixedBuildingLossValue) { _fixedBuildingLossValue = fixedBuildingLossValue; }

		void clearData();
		void clearULSList();
		void clearRASList();

		void readPopulationGrid();  // Reads the population density
		void readULSData(const std::vector<std::tuple<std::string, std::string>>& ulsDatabaseList, PopGridClass *popGrid, int linkDirection,
				double minFreq, double maxFreq, bool removeMobileFlag, CConst::SimulationEnum simulationFlag,
				const double& minLat=-90, const double& maxLat=90, const double& minLon=-180, const double& maxLon=180); // Reads a database for FS stations information
		double getBandwidth(std::string emissionsDesignator);
		double getAngleFromDMS(std::string dmsStr);
		int findULSAntenna(std::string strval);
		bool computeSpectralOverlapLoss(double *spectralOverlapLossDBptr, double sigStartFreq, double sigStopFreq, double rxStartFreq, double rxStopFreq, bool aciFlag, CConst::SpectralAlgorithmEnum spectralAlgorithm) const;

		void readRASData(std::string filename);

		void computePathLoss(CConst::PropEnvEnum propEnv, CConst::PropEnvEnum propEnvRx, CConst::NLCDLandCatEnum nlcdLandCatTx, CConst::NLCDLandCatEnum nlcdLandCatRx,
				double distKm, double fsplDistKm, double win2DistKm, double frequency,
				double txLongitudeDeg, double txLatitudeDeg, double txHeightM, double elevationAngleTxDeg,
				double rxLongitudeDeg, double rxLatitudeDeg, double rxHeightM, double elevationAngleRxDeg,
				double& pathLoss, double& pathClutterTxDB, double& pathClutterRxDB, bool meanFlag,
				std::string& pathLossModelStr, double& pathLossCDF,
				std::string& pathClutterTxModelStr, double& pathClutterTxCDF, std::string& pathClutterRxModelStr, double& pathClutterRxCDF,
				const iturp452::ITURP452 *itu452, std::string *txClutterStrPtr, std::string *rxClutterStrPtr, double **ITMProfilePtr, double **isLOSProfilePtr
#if DEBUG_AFC
				, std::vector<std::string> &ITMHeightType
#endif
				) const;

		double q(double Z) const;
		double computeBuildingPenetration(CConst::BuildingTypeEnum buildingType, double elevationAngleDeg, double frequency, std::string& buildingPenetrationModelStr, double& buildingPenetrationCDF, bool fixedProbFlag = false) const;

		double computeNearFieldLoss(double frequency, double maxGain, double distance) const;

		// Winner II models
		double Winner2_C1suburban_LOS(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double zval, double& sigma, double& pathLossCDF) const;
		double Winner2_C1suburban_NLOS(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double zval, double& sigma, double& pathLossCDF) const;
		double Winner2_C2urban_LOS(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double zval, double& sigma, double& pathLossCDF) const;
		double Winner2_C2urban_NLOS(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double zval, double& sigma, double& pathLossCDF) const;
		double Winner2_D1rural_LOS(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double zval, double& sigma, double& pathLossCDF) const;
		double Winner2_D1rural_NLOS(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double zval, double& sigma, double& pathLossCDF) const;

		double Winner2_C1suburban(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double& sigma, std::string& pathLossModelStr, double& pathLossCDF, double& probLOS, int losValue) const;
		double Winner2_C2urban(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double& sigma, std::string& pathLossModelStr, double& pathLossCDF, double& probLOS, int losValue) const;
		double Winner2_D1rural(double distance, double hBS, double hMS, double frequency, bool fixedProbFlag, double& sigma, std::string& pathLossModelStr, double& pathLossCDF, double& probLOS, int losValue) const;

		void computeInquiredFreqRangesPSD(std::vector<psdFreqRangeClass> &psdFreqRangeList); // Compute list of psdSegments for each inquired frequency range

	private:

		void importGUIjsonVersion1_0(const QJsonObject &jsonObj);
		void importGUIjsonVersion1_1(const QJsonObject &jsonObj);

		void isValid() const { // Checks the inputs to ensure cooperation with terrain
			if (_rlanHeightType == CConst::AMSLHeightType) {
				if (_terrainDataModel == NULL) {
					throw std::runtime_error("isValid() called before terrain data has been initialized.");
				}
				// Check to make sure antenna height is valid with terrain
				double minRLANHeight_m = std::get<2>(_rlanLLA) - std::get<2>(_rlanUncerts_m);
				double terrainHeight_m, bldgHeight;
				MultibandRasterClass::HeightResult lidarHeightResult;
				CConst::HeightSourceEnum heightSource;
				_terrainDataModel->getTerrainHeight(std::get<1>(_rlanLLA), std::get<0>(_rlanLLA), terrainHeight_m,bldgHeight, lidarHeightResult, heightSource);
				if (minRLANHeight_m <= terrainHeight_m) {
					throw std::invalid_argument("AfcManager::isValid(): User provided RLAN height (" + std::to_string(minRLANHeight_m)
							+ " meters) that goes below the terrain height (" + std::to_string(terrainHeight_m)
							+ " meters) at the specified RLAN location");
				}
			}
		}

		void runPointAnalysis();
		void runScanAnalysis();
		void runExclusionZoneAnalysis();
		void runHeatmapAnalysis();
		void writeKML();
		void createChannelList();
		bool containsChannel(const std::vector<FreqBandClass>& freqBandList, int chanStartFreqMHz, int chanStopFreqMHz);
		// Returns 1 is successful, 0 of cfi invalid

		void fixFSTerrain();
		CConst::PropEnvEnum computePropEnv(double lonDeg, double latDeg, CConst::NLCDLandCatEnum &nlcdLandCat, bool errorFlag = true) const;
		double computeIToNMargin(double d, double cc, double ss, ULSClass *uls, double chanCenterFreq, double bandwidth, double chanStartFreq, double chanStopFreq, double spectralOverlapLossDB, char ulsRxPropEnv, double& distKmM, std::string comment, CsvWriter *fexcthrwifi);

		/**************************************************************************************/
		/* Input Parameters                                                                   */
		/**************************************************************************************/
		std::string _analysisType;              // Parsed Analysis Type: "AP-AFC", "ExclusionZoneAnalysis", "HeatmapAnalysis";
		std::string _stateRoot;                 // Parsed path of fbrat state root
		bool _createKmz;
		bool _createDebugFiles;

		AfcDataIf* _dataIf;

		RLANBoundary _rlanUncertaintyRegionType = RLANBoundary::NO_BOUNDARY;          // specifies the type of horizontal uncertainty region being used (ellipse, linear polygon, or radial polygon)
		DoubleTriplet _rlanLLA = std::make_tuple(quietNaN, quietNaN, quietNaN);       // lat (deg) (NaN if not ellipse), lon (deg) (NaN if not ellipse), height (m)
		DoubleTriplet _rlanUncerts_m = std::make_tuple(quietNaN, quietNaN, quietNaN); // minor uncertainity (NaN if not ellipse), major uncertainity (NaN if not ellipse), height uncertainty
		std::vector<LatLon> _rlanLinearPolygon = std::vector<LatLon>();
		std::vector<AngleRadius> _rlanRadialPolygon = std::vector<AngleRadius>();

		CConst::ScanRegionMethodEnum _scanRegionMethod;

		int _scanres_points_per_degree;
		double _scanres_xy, _scanres_ht;
		bool _indoorFixedHeightAMSL;

		// Method used to treat RLAN uncertainty region scan points that have an AGL height less than _minRlanHeightAboveTerrain
		// DiscardScanPointBelowGroundMethod : Discard these scan points
		// TruncateScanPointBelowGroundMethod : Set the AGL height if these scan points to _minRlanHeightAboveTerrain
		CConst::ScanPointBelowGroundMethodEnum _scanPointBelowGroundMethod;

		double _minEIRP_dBm = std::numeric_limits<double>::quiet_NaN();                    // minimum RLAN EIRP (in dBm)
		double _maxEIRP_dBm;                    // maximum RLAN EIRP (in dBm)
		double _IoverN_threshold_dB;            // IoverN not to exceed this value for a viable channel
		double _bodyLossIndoorDB;               // Indoor body Loss (dB)
		double _bodyLossOutdoorDB;              // Outdoor body Loss (dB)
		double _polarizationLossDB;             // Polarization Loss (dB)
		double _rlanOrientation_deg;            // Orientation (deg) of ellipse clockwise from North in [-90, 90]
		RLANType _rlanType;
		CConst::HeightTypeEnum _rlanHeightType; // Above Mean Sea Level (AMSL), Above Ground Level (AGL)
		QString _serialNumber;
		QString _requestId;
		QString _rulesetId;
		QString _guiJsonVersion;

		std::vector<std::pair<int, int>> _inquiredFrquencyRangesMHz = std::vector<std::pair<int, int>>(); // list of low-high frequencies in MHz

		// first part of pair is global operating class and the second is a list of the channel indicies for that operating class
		std::vector<std::pair<int, std::vector<int>>> _inquiredChannels = std::vector<std::pair<int, std::vector<int>>>(); 

		QString _buildingLossModel;
		CConst::BuildingTypeEnum _buildingType; // Defaults to traditionalBuildingType

		bool _fixedBuildingLossFlag;            // If set, use fixed building loss value, otherwise run P.2109
		double _fixedBuildingLossValue;         // Building loss value to use if _fixedBuildingLossFlag is set

		double _confidenceBldg2109;             // Statistical confidence for P.2109 building penetration loss
		double _confidenceClutter2108;          // Statistical confidence for P.2108 clutter loss
		double _confidenceWinner2LOS;           // Statistical confidence for Winner2 LOS path loss model
		double _confidenceWinner2NLOS;          // Statistical confidence for Winner2 NLOS path loss model
		double _confidenceWinner2Combined;      // Statistical confidence for Winner2 combined path loss model
		double _confidenceITM;                  // Statistical confidence for ITM path loss model
		double _reliabilityITM;                 // Statistical reliability   for ITM path loss model

		CConst::LOSOptionEnum _winner2LOSOption;  // Method used to determine LOS for Winner2
		// LOS Unknown, always use _winner2UnknownLOSMethod
		// BldgDataWinner2LOSOption : use building data 
		// ForceLOSWinner2LOSOption : Always use LOS
		// ForceNLOSWinner2LOSOption : Always use NLOS

		CConst::SpectralAlgorithmEnum _channelResponseAlgorithm;
		// pwrSpectralAlgorithm : use power method
		// psdSpectralAlgorithm : use psd   method

		CConst::Winner2UnknownLOSMethodEnum _winner2UnknownLOSMethod;  // Method used to compute Winner2 PL when LOS not known
		// PLOSCombineWinner2UnknownLOSMethod : Compute probLOS, then combine
		// PLOSThresholdWinner2UnknownLOSMethod : Compute probLOS, use LOS if exceeds _winner2ProbLOSThr

		double _winner2ProbLOSThr;              // Winner2 prob LOS threshold, if probLOS exceeds threshold, use LOS model, otherwise use NLOS
		// bool _winner2CombineFlag;               // Whether or not to combine LOS and NLOS path loss values in Winner2.
		// bool _winner2BldgLOSFlag;               // If set, use building data to determine if winner2 LOS or NLOS model is used

		bool _winner2UseGroundDistanceFlag;     // If set, use ground distance in winner2 model
		bool _fsplUseGroundDistanceFlag;        // If set, use ground distance for FSPL

		std::vector<std::tuple<std::string, std::string>> _ulsDatabaseList;
		// List of tuples where each tuple contains database name and database sqlite file name.

		QString _inputULSDatabaseStr;           // ULS Database being used
		std::string _rasDataFile;               // File contining RAS data

		QString _propagationEnviro;             // Propagation environment (e.g. Population Density Map)

		double _rxFeederLossDBUNII5;            // User-inputted ULS receiver feeder loss for UNII-5
		double _rxFeederLossDBUNII7;            // User-inputted ULS receiver feeder loss for UNII-7
		double _rxFeederLossDBOther;            // User-inputted ULS receiver feeder loss for others (not UNII-5 or UNII-7)

		double _ulsNoiseFigureDBUNII5;          // Noise Figure for ULS receiver in UNII-5 band;
		double _ulsNoiseFigureDBUNII7;          // Noise Figure for ULS receiver in UNII-7 band;
		double _ulsNoiseFigureDBOther;          // Noise Figure for ULS receiver in Other band;

		double _itmEpsDielect;
		double _itmSgmConductivity;
		int _itmPolarization;
		double _itmMinSpacing;                   // Min spacing, in meters, between points in ITM path profile
		int _itmMaxNumPts;                       // Max number of points to use in ITM path profile

		QJsonObject _deviceDesc;                // parsed device description to be returned in response

		int _exclusionZoneFSID;                 // FSID to use for Exclusion Zone Analysis
		int _exclusionZoneRLANChanIdx;          // RLAN channel Index to use for Exclusion Zone Analysis
		double _exclusionZoneRLANBWHz;          // RLAN bandwidth (Hz) to use for Exclusion Zone Analysis
		double _exclusionZoneRLANEIRPDBm;       // RLAN EIRP (dBm) to use for Exclusion Zone Analysis

		double _heatmapRLANBWHz;                   // RLAN bandwidth (Hz) to use for Heatmap Analysis
		int _heatmapRLANChanIdx;                // RLAN channel Index to use for Heatmap Analysis
		double _heatmapMinLon;                  // Min Lon for region in which Heatmap Analysis is performed
		double _heatmapMaxLon;                  // Max Lon for region in which Heatmap Analysis is performed
		double _heatmapMinLat;                  // Min Lat for region in which Heatmap Analysis is performed
		double _heatmapMaxLat;                  // Max Lat for region in which Heatmap Analysis is performed
		double _heatmapRLANSpacing;             // Maximum spacing (m) between points in Heatmap Analysis
		std::string _heatmapIndoorOutdoorStr;   // Can be: "Indoor", "Outdoor", "Database"

		double _heatmapRLANIndoorEIRPDBm;       // RLAN Indoor EIRP (dBm) to use for Heatmap Analysis
		QString _heatmapRLANIndoorHeightType;   // Above Mean Sea Level (AMSL), Above Ground Level (AGL) for Indoor RLAN's
		double _heatmapRLANIndoorHeight;        // RLAN Indoor Height (m) to use for Heatmap Analysis
		double _heatmapRLANIndoorHeightUncertainty;  // RLAN Indoor Height Uncertainty (m) to use for Heatmap Analysis

		double _heatmapRLANOutdoorEIRPDBm;      // RLAN Outdoor EIRP (dBm) to use for Heatmap Analysis
		QString _heatmapRLANOutdoorHeightType;   // Above Mean Sea Level (AMSL), Above Ground Level (AGL) for OutIndoor RLAN's
		double _heatmapRLANOutdoorHeight;       // RLAN Outdoor Height (m) to use for Heatmap Analysis
		double _heatmapRLANOutdoorHeightUncertainty; // RLAN Outdoor Height Uncertainty (m) to use for Heatmap Analysis

		bool _applyClutterFSRxFlag;
		double _fsConfidenceClutter2108;
		double _maxFsAglHeight;

		CConst::ITMClutterMethodEnum _rlanITMTxClutterMethod;

		std::vector<FreqBandClass> _allowableFreqBandList; // List of allowable freq bands.  For USA, correspond to UNII-5 and UNII-7
		std::string _mapDataGeoJsonFile;               // File to write map data geojson
		/**************************************************************************************/

		/**************************************************************************************/
		/* Constant Parameters                                                                */
		/**************************************************************************************/
		std::string _srtmDir;                   // Directory that contain SRTM terrain height files
		std::string _depDir;                   // Directory that contain 3DEP terrain height files
		QString _globeDir;                  // Directory that contains NOAA GLOBE data files
		QString _lidarDir;                      // Directory that contain LiDAR multiband raster files.
		bool _useBDesignFlag = false;           // Force use B-Design3D building data in Manhattan
		bool _useLiDAR = false;                 // flag to enable use of LiDAR files for computation
		bool _use3DEP = false;                  // flag to enable use of 3DEP 10m terrain data

		double _minRlanHeightAboveTerrain;      // Min height above terrain for RLAN

		double _maxRadius;                      // Max link distance to consider, links longer are ignored
		double _exclusionDist;                  // Min link distance to consider, links shorter are ignored

		bool _nearFieldLossFlag;                // If set compute near field loss, otherwise near field loss is 0
		double _illuminationEfficiency;         // Illumination Efficiency value to use for near field loss calculation",
		bool _closeInHgtFlag;                   // Whether or not to force LOS when mobile height above closeInHgtLOS for close in model",
		double _closeInHgtLOS;                  // RLAN height above which prob of LOS = 100% for close in model",
		double _closeInDist;                    // Radius in which close in path loss model is used
		std::string _closeInPathLossModel;      // Close in path loss model is used
		bool _pathLossClampFSPL;                // If set, when path loss < fspl, clamp to fspl value

		int _wlanMinFreqMHz;                    // Min Frequency for WiFi system (integer in MHz)
		int _wlanMaxFreqMHz;                    // Max Frequency for WiFi system (integer in MHz)
		double _wlanMinFreq;                    // Min Frequency for WiFi system (double in Hz)
		double _wlanMaxFreq;                    // Max Frequency for WiFi system (double in Hz)
		std::vector <OpClass::OpClass> _opClass;
		std::vector <OpClass::OpClass> _psdOpClassList;

		std::string _popDensityFile;            // File contining population density data
		double _popDensityResLon;               // Population density file resolution for longitude
		double _popDensityResLat;               // Population density file resolution for latitude
		double _popDensityMinLon;               // Population density minimum longitude
		int _popDensityNumLon;                  // Population density number of longitude
		double _popDensityMinLat;               // Population density minimum latitude
		int _popDensityNumLat;                  // Population density number of latitude

		std::string _regionStr;                 // Comma separated list of names of regions in sim, corresp to pop density file
		std::string _worldPopulationFile;       // GDAL file (tiff) containing population density data
		std::string _regionPolygonFileList;     // Comma separated list of KML files, one for each region in simulation
		double _regionPolygonResolution;   // Resolution to use for polygon vertices, 1.0e-5 corresp to 1.11m, should not have to change
		std::string _nlcdFile;                  // GDAL file contining NLCD data
		std::string _radioClimateFile;           // ITU radio climate data
		std::string _surfRefracFile;             // ITU surface refractivity data

		double _densityThrUrban;                // Population density threshold above which region is considered URBAN
		double _densityThrSuburban;             // Population density above this thr and below urban thr is SUBURBAN
		double _densityThrRural;                // Population density above this thr and below suburban thr is RURAL

		bool _removeMobile;                     // If set to true, mobile entries are removed when reading ULS file

		bool _filterSimRegionOnly;              // Filter ULS file only for in/out of simulation region

		std::string _ulsAntennaPatternFile;     // File containing ULS antenna patterns as a function of angle off boresight;
		CConst::ULSAntennaTypeEnum _ulsDefaultAntennaType; // Default ULS antenna type to use when antenna pattern is not otherwise specified.

		// std::string _rlanBWStr;                 // Comma separated list of RLAN bandwidths (Hz), "b0,b1,b2"

		double _visibilityThreshold;             // I/N threshold to determine whether or not an RLAN is visible to an FS
		std::string _excThrFile;                 // Generate file containing data for wifi devices where single entry I/N > visibility Threshold
		std::string _fsAnomFile;                 // Generate file containing anomalous FS entries
		std::string _userInputsFile;             // Generate file containing user inputs
		std::string _kmlFile;                    // Generate kml file showing simulation results, primarily for debugging
		std::string _fsAnalysisListFile;         // File containing list of FS used in the analysis
		std::string _progressFile;               // stores messages to be sent to GUI on progress of execution. line 0: percentage (number), line 1: message (string)
		int _maxLidarRegionLoadVal;
		/**************************************************************************************/

		/**************************************************************************************/
		/* Data                                                                               */
		/**************************************************************************************/
		TerrainClass *_terrainDataModel; // Contains building/terrain data, auto falls back to SRTM -> Population

		double _bodyLossDB;                     // Body Loss (dB)

		std::vector<std::string> _regionNameList;
		std::vector<int> _regionIDList;
		int _numRegion;

		PopGridClass *_popGrid;                  // Population data stored in here after being read in for a particular city/region

		std::vector<double> _rlanBWList;         // In this case four elements (20MHz, 40MHz, etc.) 

		ListClass<ULSClass *> *_ulsList;         // List of the FS stations that are being used in the analysis

		ListClass<RASClass *> *_rasList;         // List of the RAS (Radio Astronomy Stations) that each have exclusion zone.

		GdalImageFile2 *nlcdImageFile;

		std::vector<AntennaClass *> _ulsAntennaList;

		CConst::PathLossModelEnum _pathLossModel;

		double _zbldg2109;
		double _zclutter2108;
		double _fsZclutter2108;
		double _zwinner2LOS;
		double _zwinner2NLOS;
		double _zwinner2Combined;

		std::vector<int> _ulsIdxList;            // Stores the indices of the ULS stations we are analyzing
		DoubleTriplet _beamConeLatLons;          // Stores beam cone coordinates together to be loaded into geometries

		RlanRegionClass *_rlanRegion;            // RLAN Uncertainty Region

		ITUDataClass *_ituData;
		/**************************************************************************************/

		/**************************************************************************************/
		/* Output Parameters                                                                  */
		/**************************************************************************************/
		std::vector<LatLon> FSLatLon;          // Three vertices for complete coverage triangle in lat/lon
		std::vector<double> calculatedIoverN;
		std::vector<double> EIRPMask;              // Maximum EIRP for a given channel frequency

		std::vector<ChannelStruct> _channelList;    // List of channels, each channel identified by startFreq, stopFreq.  Computed results are availability and eirp_limit_dbm
		bool _aciFlag;                              // If set, consider ACI in the overal interference calculation

		std::vector<LatLon> _exclusionZone;    // List of vertices of exclusion zone contour (Lon, Lat)
		double _exclusionZoneFSTerrainHeight;      // Terrain height at location of FS used in exclusion zone analysis
		double _exclusionZoneHeightAboveTerrain;   // Height above terrain for FS used in exclusion zone analysis

		double **_heatmapIToNDB;                   // Matrix of I/N values for heatmap _heatmapIToNDB[lonIdx][latIdx], lonIdx in [0,_heatmapNumPtsLon-1] latIdx in [0,_heatmapNumPtsLat-1]
		bool **_heatmapIsIndoor;                   // Matrix of bool values: true for indoor, false for outdoor for grid point (lonIdx, latIdx)
		int _heatmapNumPtsLon;                     // Num LON values in heatmap matrix
		int _heatmapNumPtsLat;                     // Num LAT values in heatmap matrix
		double _heatmapMinIToNDB;                  // Min I/N in _heatmapIToNDB
		double _heatmapMaxIToNDB;                  // Max I/N in _heatmapIToNDB
		double _heatmapIToNThresholdDB;            // I/N threshold value used to determine colors in heatmap graphical desplay

		std::vector<std::string> statusMessageList; // List of messages regarding run status to send to GUI
		CConst::ResponseCodeEnum _responseCode;
		QStringList _missingParams;
		QStringList _invalidParams;
		QStringList _unexpectedParams;
		/**************************************************************************************/

		//bool _configChange = false;

#if DEBUG_AFC
		void runTestWinner2(std::string inputFile, std::string outputFile);
		void runAnalyzeNLCD();
#endif
};

#endif // INCLUDE_AFCMANAGER_H
