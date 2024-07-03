// AfcManager.cpp -- Manages I/O and top-level operations for the AFC Engine
#include <boost/format.hpp>
#include <QFileInfo>

#include "AfcManager.h"
#include "RlanRegion.h"
#include "lininterp.h"

// "--runtime_opt" masks
// These bits corresponds to RNTM_OPT_... bits in src/ratapi/ratapi/defs.py
// Please keep all these definitions synchronous
#define RUNTIME_OPT_ENABLE_DBG 1
#define RUNTIME_OPT_ENABLE_GUI 2
#define RUNTIME_OPT_URL 4
#define RUNTIME_OPT_ENABLE_SLOW_DBG 16
#define RUNTIME_OPT_CERT_ID 32

extern double qerfi(double q);

// QJsonArray jsonChannelData(const std::vector<ChannelStruct> &channelList);
// QJsonObject jsonSpectrumData(const std::vector<ChannelStruct> &channelList, const QJsonObject
// &deviceDesc, const double &startFreq);

std::string padStringFront(const std::string &s, const char &padder, const int &amount)
{
	std::string r = s;
	while ((int)(r.length()) < amount) {
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
	rawtime += dayStep * 24 * 3600;

	ptm = gmtime(&rawtime);

	// "yyyy-mm-ddThh:mm:ssZ"
	std::string result = padStringFront(std::to_string(ptm->tm_year + 1900), '0', 4) + "-" +
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
	if (!filename.empty()) {
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
OpClass GlobalOpClass_131 = {131, // Operating class
			     20, // Bandwidth
			     5950, // Start frequency.
			     {1,   5,	9,   13,  17,  21,  25,	 29,  33,  37,	41,  45,
			      49,  53,	57,  61,  65,  69,  73,	 77,  81,  85,	89,  93,
			      97,  101, 105, 109, 113, 117, 121, 125, 129, 133, 137, 141,
			      145, 149, 153, 157, 161, 165, 169, 173, 177, 181, 185, 189,
			      193, 197, 201, 205, 209, 213, 217, 221, 225, 229, 233}};

const OpClass GlobalOpClass_132 = {132, 40, 5950, {3,	11,  19,  27,  35,  43,	 51,  59,  67,	75,
						   83,	91,  99,  107, 115, 123, 131, 139, 147, 155,
						   163, 171, 179, 187, 195, 203, 211, 219, 227}};

const OpClass GlobalOpClass_133 = {133,
				   80,
				   5950,
				   {7, 23, 39, 55, 71, 87, 103, 119, 135, 151, 167, 183, 199, 215}};

const OpClass GlobalOpClass_134 = {134, 160, 5950, {15, 47, 79, 111, 143, 175, 207}};

const OpClass GlobalOpClass_137 = {137, 320, 5950, {31, 63, 95, 127, 159, 191}};

const OpClass GlobalOpClass_135 = {135,
				   80,
				   5950,
				   {7, 23, 39, 55, 71, 87, 103, 119, 135, 151, 167, 183, 199, 215}};

const OpClass GlobalOpClass_136 = {136, 20, 5925, {2}};

const std::vector<OpClass> USOpClass = {GlobalOpClass_131,
					GlobalOpClass_132,
					GlobalOpClass_133,
					GlobalOpClass_134,
					// Opclass 135 is a non contiguous 80+80 channels.
					// GlobalOpClass_135,
					GlobalOpClass_136,
					GlobalOpClass_137};

// Hardcode for PSD to only consider 20MHz channels
const std::vector<OpClass> PSDOpClassList = {GlobalOpClass_131, GlobalOpClass_136};

}

static const std::map<int, std::string> nlcdCodeNames = {
	{0, "Unclassified"},
	{11, "Open Water"},
	{12, "Ice/Snow"},
	{21, "Developed, Open Space"},
	{22, "Developed, Low Intensity"},
	{23, "Developed, Medium Intensity"},
	{24, "Developed, High Intensity"},
	{31, "Barren Land"},
	{41, "Deciduous Forest"},
	{42, "Evergreen Forest"},
	{43, "Mixed Forest"},
	{51, "Alaska Dwarf Scrub"},
	{52, "Shrub/Scrub"},
	{71, "Grassland/Herbaceous"},
	{72, "Alaska Sedge/Herbaceous"},
	{73, "Alaska Lichens"},
	{74, "Alaska Moss"},
	{81, "Pasture/Hay"},
	{82, "Cultivated Crops"},
	{90, "Woody Wetlands"},
	{95, "Emergent Herbaceous Wetlands"},
};
static const std::map<int, std::string> nlcdLandCatNames = {
	{CConst::deciduousTreesNLCDLandCat, "deciduousTreesNLCDLandCat"},
	{CConst::coniferousTreesNLCDLandCat, "coniferousTreesNLCDLandCat"},
	{CConst::highCropFieldsNLCDLandCat, "highCropFieldsNLCDLandCat"},
	{CConst::noClutterNLCDLandCat, "noClutterNLCDLandCat"},
	{CConst::villageCenterNLCDLandCat, "villageCenterNLCDLandCat"},
	{CConst::unknownNLCDLandCat, "unknownNLCDLandCat"},
};
static const std::map<int, std::string> pathLossModelNames = {
	{CConst::unknownPathLossModel, "unknownPathLossModel"},
	{CConst::ITMBldgPathLossModel, "ITMBldgPathLossModel"},
	{CConst::CoalitionOpt6PathLossModel, "CoalitionOpt6PathLossModel"},
	{CConst::FCC6GHzReportAndOrderPathLossModel, "FCC6GHzReportAndOrderPathLossModel"},
	{CConst::CustomPathLossModel, "CustomPathLossModel"},
	{CConst::ISEDDBS06PathLossModel, "ISEDDBS06PathLossModel"},
	{CConst::BrazilPathLossModel, "BrazilPathLossModel"},
	{CConst::OfcomPathLossModel, "OfcomPathLossModel"},
	{CConst::FSPLPathLossModel, "FSPLPathLossModel"}};
static const std::map<int, std::string> propEnvNames = {{CConst::unknownPropEnv, "unknownPropEnv"},
							{CConst::urbanPropEnv, "urbanPropEnv"},
							{CConst::suburbanPropEnv,
							 "suburbanPropEnv"},
							{CConst::ruralPropEnv, "ruralPropEnv"},
							{CConst::barrenPropEnv, "barrenPropEnv"}};

/** GZIP CSV for EIRP computation */
class EirpGzipCsv : public GzipCsv
{
	public:
		ColStr callsign; // ULS Path Callsign
		ColInt pathNum; // ULS Path number
		ColInt ulsId; // ULS Path ID from DB
		ColInt segIdx; // Segment index
		ColInt divIdx; // Diversity index
		ColDouble scanLat; // Scanpoint latitude
		ColDouble scanLon; // Scanpoint longitude
		ColDouble scanAgl; // Scanpoint elevation above ground
		ColDouble scanAmsl; // Scanpoint elevation above terrain
		ColInt scanPtIdx; // Scanpoint index
		ColDouble distKm; // Distance from scanpoint to FS RX
		ColDouble elevationAngleTx; // Elevation from scanpoint to FS RX
		ColInt channel; // Channel number
		ColDouble chanStartMhz; // Channel start MHz
		ColDouble chanEndMhz; // Channel end MHz
		ColDouble chanBwMhz; // Channel bandwidth MHz
		ColEnum chanType; // Request type (by freq/by number)
		ColDouble eirpLimit; // Resulting EIRP limit dB
		ColBool fspl; // Freespace (trial) pathLoss computation
		ColDouble pathLossDb; // Path loss dB
		ColEnum configPathLossModel; // Configured path loss model
		ColStr resultedPathLossModel; // Resulted path poss model
		ColDouble buildingPenetrationDb; // Building penetration loss dB
		ColDouble offBoresight; // Angle beween RX beam and direction to scanpoint
		ColDouble rxGainDb; // RX Gain DB (loss due to antenna diagram)
		ColDouble discriminationGainDb; // Discrimination gain
		ColEnum txPropEnv; // TX Propagation environment
		ColEnum nlcdTx; // Land use at RLAN
		ColStr pathClutterTxModel; // Path Clutter TX model
		ColDouble pathClutterTxDb; // Path clutter TX dB
		ColStr txClutter; // TX Clutter
		ColEnum rxPropEnv; // RX Propagation environment
		ColEnum nlcdRx; // Land use at FS
		ColStr pathClutterRxModel; // Path Clutter RX model
		ColDouble pathClutterRxDb; // Path Clutter RX dB
		ColStr rxClutter; // RX Clutter
		ColDouble nearFieldOffsetDb; // Near field offset dB
		ColDouble spectralOverlapLossDb; // Spectral overlap loss dB
		ColDouble ulsAntennaFeederLossDb; // FS Antenna feeder loss dB
		ColDouble rxPowerDbW; // Intermediate aggregated loss
		ColDouble ulsNoiseLevelDbW; // FS Noise level dB

		EirpGzipCsv(const std::string &filename) :
			GzipCsv(filename),
			callsign(this, "CallSign"),
			pathNum(this, "PathNumber"),
			ulsId(this, "UlsId"),
			segIdx(this, "SegIdx"),
			divIdx(this, "DivIdx"),
			scanLat(this, "ScanLat"),
			scanLon(this, "ScanLon"),
			scanAgl(this, "ScanAGL"),
			scanAmsl(this, "ScanAMSL"),
			scanPtIdx(this, "ScanIdx"),
			distKm(this, "DistKm"),
			elevationAngleTx(this, "ElevationAngleTx"),
			channel(this, "Channel"),
			chanStartMhz(this, "ChanStartMhz"),
			chanEndMhz(this, "ChanEndMhz"),
			chanBwMhz(this, "ChanBwMhz"),
			chanType(this,
				 "ChanType",
				 {{INQUIRED_FREQUENCY, "ByFreq"}, {INQUIRED_CHANNEL, "ByNumber"}}),
			eirpLimit(this, "EIRP"),
			fspl(this, "FreeSpace"),
			pathLossDb(this, "PathLossDb"),
			configPathLossModel(this, "ConfigPathLossModel", pathLossModelNames),
			resultedPathLossModel(this, "ResultedPathLossModel"),
			buildingPenetrationDb(this, "BuildingPenetrationDb"),
			offBoresight(this, "OffBoresightDeg"),
			rxGainDb(this, "RxGainDb"),
			discriminationGainDb(this, "DiscrGainDb"),
			txPropEnv(this, "TxPropEnv", propEnvNames),
			nlcdTx(this, "TxLandUse", nlcdLandCatNames),
			pathClutterTxModel(this, "TxClutterModel"),
			pathClutterTxDb(this, "PathClutterTxDb"),
			txClutter(this, "TxClutter"),
			rxPropEnv(this, "RxPropEnv", propEnvNames),
			nlcdRx(this, "RxLandUse", nlcdLandCatNames),
			pathClutterRxModel(this, "RxClutterModel"),
			pathClutterRxDb(this, "PathClutterRxDb"),
			rxClutter(this, "RxClutter"),
			nearFieldOffsetDb(this, "NearFieldOffsetDb"),
			spectralOverlapLossDb(this, "SpectralOverlapLossDb"),
			ulsAntennaFeederLossDb(this, "UlsAntennaFeederLossDb"),
			rxPowerDbW(this, "RxPowerDbW"),
			ulsNoiseLevelDbW(this, "UlsNoiseLevel")

		{
		}
};

/** GZIP CSV for anomaly writer */
class AnomalyGzipCsv : public GzipCsv
{
	public:
		ColInt fsid;
		ColStr dbName;
		ColStr callsign;
		ColDouble rxLatitudeDeg;
		ColDouble rxLongitudeDeg;
		ColStr anomaly;

		AnomalyGzipCsv(const std::string &filename) :
			GzipCsv(filename),
			fsid(this, "FSID"),
			dbName(this, "DBNAME"),
			callsign(this, "CALLSIGN"),
			rxLatitudeDeg(this, "RX_LATITUDE"),
			rxLongitudeDeg(this, "RX_LONGITUDE"),
			anomaly(this, "ANOMALY_DESCRIPTION")
		{
		}
};

class ExcThrParamClass
{
	public:
		double rlanDiscriminationGainDB;
		double bodyLossDB;
		std::string buildingPenetrationModelStr;
		double buildingPenetrationCDF;
		double buildingPenetrationDB;
		double angleOffBoresightDeg;
		std::string pathLossModelStr;
		double pathLossCDF;
		std::string pathClutterTxModelStr;
		double pathClutterTxCDF;
		double pathClutterTxDB;
		std::string txClutterStr;
		std::string pathClutterRxModelStr;
		double pathClutterRxCDF;
		double pathClutterRxDB;
		std::string rxClutterStr;
		double rxGainDB;
		double discriminationGain;
		std::string rxAntennaSubModelStr;
		double nearFieldOffsetDB;
		double spectralOverlapLossDB;
		double polarizationLossDB;
		double rxAntennaFeederLossDB;
		double reflectorD0;
		double reflectorD1;
		double nearField_xdb;
		double nearField_u;
		double nearField_eff;
};

/** GZIP CSV for excThr */
class ExThrGzipCsv : public GzipCsv
{
	public:
		ColInt fsid;
		ColStr region;
		ColStr dbName;
		ColInt rlanPosnIdx;
		ColStr callsign;
		ColDouble fsLon;
		ColDouble fsLat;
		ColDouble fsAgl;
		ColDouble fsTerrainHeight;
		ColStr fsTerrainSource;
		ColStr fsPropEnv;
		ColInt numPr;
		ColInt divIdx;
		ColInt segIdx;
		ColDouble segRxLon;
		ColDouble segRxLat;
		ColDouble refThetaIn;
		ColDouble refKs;
		ColDouble refQ;
		ColDouble refD0;
		ColDouble refD1;
		ColDouble rlanLon;
		ColDouble rlanLat;
		ColDouble rlanAgl;
		ColDouble rlanTerrainHeight;
		ColStr rlanTerrainSource;
		ColStr rlanPropEnv;
		ColDouble rlanFsDist;
		ColDouble rlanFsGroundDist;
		ColDouble rlanElevAngle;
		ColDouble boresightAngle;
		ColDouble rlanTxEirp;
		ColStr rlanAntennaModel;
		ColDouble rlanAOB;
		ColDouble rlanDiscriminationGainDB;
		ColDouble bodyLoss;
		ColStr rlanClutterCategory;
		ColStr fsClutterCategory;
		ColStr buildingType;
		ColDouble buildingPenetration;
		ColStr buildingPenetrationModel;
		ColDouble buildingPenetrationCdf;
		ColDouble pathLoss;
		ColStr pathLossModel;
		ColDouble pathLossCdf;
		ColDouble pathClutterTx;
		ColStr pathClutterTxMode;
		ColDouble pathClutterTxCdf;
		ColDouble pathClutterRx;
		ColStr pathClutterRxMode;
		ColDouble pathClutterRxCdf;
		ColDouble rlanBandwidth;
		ColDouble rlanStartFreq;
		ColDouble rlanStopFreq;
		ColDouble ulsStartFreq;
		ColDouble ulsStopFreq;
		ColStr antType;
		ColStr antCategory;
		ColDouble antGainPeak;
		ColStr prType;
		ColDouble prEffectiveGain;
		ColDouble prDiscrinminationGain;
		ColDouble fsGainToRlan;
		ColDouble fsNearFieldXdb;
		ColDouble fsNearFieldU;
		ColDouble fsNearFieldEff;
		ColDouble fsNearFieldOffset;
		ColDouble spectralOverlapLoss;
		ColDouble polarizationLoss;
		ColDouble fsRxFeederLoss;
		ColDouble fsRxPwr;
		ColDouble fsIN;
		ColDouble eirpLimit;
		ColDouble fsSegDist;
		ColDouble ulsLinkDist; // Field from runExclusionZoneAnalysis() and
				       // runHeatMapAnalysis()
		ColDouble rlanCenterFreq;
		ColDouble fsTxToRlanDist;
		ColDouble pathDifference;
		ColDouble ulsWavelength;
		ColDouble fresnelIndex;
		ColStr comment; // Field from runExclusionZoneAnalysis()

		ExThrGzipCsv(const std::string filename) :
			GzipCsv(filename),
			fsid(this, "FS_ID"),
			region(this, "FS_REGION"),
			dbName(this, "DBNAME"),
			rlanPosnIdx(this, "RLAN_POSN_IDX"),
			callsign(this, "CALLSIGN"),
			fsLon(this, "FS_RX_LONGITUDE (deg)"),
			fsLat(this, "FS_RX_LATITUDE (deg)"),
			fsAgl(this, "FS_RX_HEIGHT_ABOVE_TERRAIN (m)"),
			fsTerrainHeight(this, "FS_RX_TERRAIN_HEIGHT (m)"),
			fsTerrainSource(this, "FS_RX_TERRAIN_SOURCE"),
			fsPropEnv(this, "FS_RX_PROP_ENV"),
			numPr(this, "NUM_PASSIVE_REPEATER"),
			divIdx(this, "IS_DIVERSITY_LINK"),
			segIdx(this, "SEGMENT_IDX"),
			segRxLon(this, "SEGMENT_RX_LONGITUDE (deg)"),
			segRxLat(this, "SEGMENT_RX_LATITUDE (deg)"),
			refThetaIn(this, "PR_REF_THETA_IN (deg)"),
			refKs(this, "PR_REF_KS"),
			refQ(this, "PR_REF_Q"),
			refD0(this, "PR_REF_D0 (dB)"),
			refD1(this, "PR_REF_D1 (dB)"),
			rlanLon(this, "RLAN_LONGITUDE (deg)"),
			rlanLat(this, "RLAN_LATITUDE (deg)"),
			rlanAgl(this, "RLAN_HEIGHT_ABOVE_TERRAIN (m)"),
			rlanTerrainHeight(this, "RLAN_TERRAIN_HEIGHT (m)"),
			rlanTerrainSource(this, "RLAN_TERRAIN_SOURCE"),
			rlanPropEnv(this, "RLAN_PROP_ENV"),
			rlanFsDist(this, "RLAN_FS_RX_DIST (km)"),
			rlanFsGroundDist(this, "RLAN_FS_RX_GROUND_DIST (km)"),
			rlanElevAngle(this, "RLAN_FS_RX_ELEVATION_ANGLE (deg)"),
			boresightAngle(this, "FS_RX_ANGLE_OFF_BORESIGHT (deg)"),
			rlanTxEirp(this, "RLAN_TX_EIRP (dBm)"),
			rlanAntennaModel(this, "RLAN_ANTENNA_MODEL"),
			rlanAOB(this, "RLAN_ANGLE_OFF_BORESIGHT (deg)"),
			rlanDiscriminationGainDB(this, "RLAN_DISCRIMINATION_GAIN (dB)"),
			bodyLoss(this, "BODY_LOSS (dB)"),
			rlanClutterCategory(this, "RLAN_CLUTTER_CATEGORY"),
			fsClutterCategory(this, "FS_CLUTTER_CATEGORY"),
			buildingType(this, "BUILDING TYPE"),
			buildingPenetration(this, "RLAN_FS_RX_BUILDING_PENETRATION (dB)"),
			buildingPenetrationModel(this, "BUILDING_PENETRATION_MODEL"),
			buildingPenetrationCdf(this, "BUILDING_PENETRATION_CDF"),
			pathLoss(this, "PATH_LOSS (dB)"),
			pathLossModel(this, "PATH_LOSS_MODEL"),
			pathLossCdf(this, "PATH_LOSS_CDF"),
			pathClutterTx(this, "PATH_CLUTTER_TX (DB)"),
			pathClutterTxMode(this, "PATH_CLUTTER_TX_MODEL"),
			pathClutterTxCdf(this, "PATH_CLUTTER_TX_CDF"),
			pathClutterRx(this, "PATH_CLUTTER_RX (DB)"),
			pathClutterRxMode(this, "PATH_CLUTTER_RX_MODEL"),
			pathClutterRxCdf(this, "PATH_CLUTTER_RX_CDF"),
			rlanBandwidth(this, "RLAN BANDWIDTH (MHz)"),
			rlanStartFreq(this, "RLAN CHANNEL START FREQ (MHz)"),
			rlanStopFreq(this, "RLAN CHANNEL STOP FREQ (MHz)"),
			ulsStartFreq(this, "ULS START FREQ (MHz)"),
			ulsStopFreq(this, "ULS STOP FREQ (MHz)"),
			antType(this, "FS_ANT_TYPE"),
			antCategory(this, "FS_ANT_CATEGORY"),
			antGainPeak(this, "FS_ANT_GAIN_PEAK (dB)"),
			prType(this, "PR_TYPE (dB)"),
			prEffectiveGain(this, "PR_EFFECTIVE_GAIN (dB)"),
			prDiscrinminationGain(this, "PR_DISCRIMINATION_GAIN (dB)"),
			fsGainToRlan(this, "FS_ANT_GAIN_TO_RLAN (dB)"),
			fsNearFieldXdb(this, "FS_ANT_NEAR_FIELD_XDB"),
			fsNearFieldU(this, "FS_ANT_NEAR_FIELD_U"),
			fsNearFieldEff(this, "FS_ANT_NEAR_FIELD_EFF"),
			fsNearFieldOffset(this, "FS_ANT_NEAR_FIELD_OFFSET (dB)"),
			spectralOverlapLoss(this, "RX_SPECTRAL_OVERLAP_LOSS (dB)"),
			polarizationLoss(this, "POLARIZATION_LOSS (dB)"),
			fsRxFeederLoss(this, "FS_RX_FEEDER_LOSS (dB)"),
			fsRxPwr(this, "FS_RX_PWR (dBW)"),
			fsIN(this, "FS I/N (dB)"),
			eirpLimit(this, "EIRP_LIMIT (dBm)"),
			fsSegDist(this, "FS_SEGMENT_DIST (m)"),
			ulsLinkDist(this, "ULS_LINK_DIST (m)"),
			rlanCenterFreq(this, "RLAN_CENTER_FREQ (Hz)"),
			fsTxToRlanDist(this, "FS_TX_TO_RLAN_DIST (m)"),
			pathDifference(this, "PATH_DIFFERENCE (m)"),
			ulsWavelength(this, "ULS_WAVELENGTH (mm)"),
			fresnelIndex(this, "FRESNEL_INDEX"),
			comment(this, "COMMENT")
		{
		}
};

/** GZIP CSV for traces */
class TraceGzipCsv : public GzipCsv
{
	public:
		ColStr ptId;
		ColDouble lon;
		ColDouble lat;
		ColDouble dist;
		ColDouble amsl;
		ColDouble losAmsl;
		ColInt fsid;
		ColInt divIdx;
		ColInt segIdx;
		ColInt scanPtIdx;
		ColInt rlanHtIdx;

		TraceGzipCsv(std::string filename) :
			GzipCsv(filename),
			ptId(this, "PT_ID"),
			lon(this, "PT_LON (deg)"),
			lat(this, "PT_LAT (deg)"),
			dist(this, "GROUND_DIST (Km)"),
			amsl(this, "TERRAIN_HEIGHT_AMSL (m)"),
			losAmsl(this, "LOS_PATH_HEIGHT_AMSL (m)"),
			fsid(this, "FSID"),
			divIdx(this, "DIV_IDX"),
			segIdx(this, "SEG_IDX"),
			scanPtIdx(this, "SCAN_PT_IDX"),
			rlanHtIdx(this, "RLAN_HEIGHT_IDX")
		{
		}
};

AfcManager::AfcManager()
{
	_createKmz = false;
	_createDebugFiles = false;
	_createSlowDebugFiles = false;
	_certifiedIndoor = false;

	_dataIf = (AfcDataIf *)NULL;

	_rlanUncertaintyRegionType = RLANBoundary::NO_BOUNDARY;
	_rlanLLA = std::make_tuple(quietNaN, quietNaN, quietNaN);
	_rlanUncerts_m = std::make_tuple(quietNaN, quietNaN, quietNaN);
	_allowScanPtsInUncRegFlag = false;

	_scanRegionMethod = CConst::latLonAlignGridScanRegionMethod;

	_scanres_points_per_degree = -1;
	_scanres_xy = quietNaN;
	_scanres_ht = quietNaN;
	_indoorFixedHeightAMSL = false;

	_maxVerticalUncertainty = quietNaN;
	_maxHorizontalUncertaintyDistance = quietNaN;

	_scanPointBelowGroundMethod = CConst::TruncateScanPointBelowGroundMethod;

	_minEIRPIndoor_dBm = quietNaN;
	_minEIRPOutdoor_dBm = quietNaN;
	_minEIRP_dBm = quietNaN;
	_maxEIRP_dBm = quietNaN;
	_minPSD_dBmPerMHz = quietNaN;
	_reportUnavailPSDdBmPerMHz = quietNaN;
	_inquiredFrequencyMaxPSD_dBmPerMHz = quietNaN;
	_rlanAzimuthPointing = quietNaN;
	_rlanElevationPointing = quietNaN;

	_IoverN_threshold_dB = -6.0;
	_bodyLossIndoorDB = 0.0; // Indoor body Loss (dB)
	_bodyLossOutdoorDB = 0.0; // Outdoor body Loss (dB)
	_polarizationLossDB = 0.0; // Polarization Loss (dB)
	_rlanOrientation_deg =
		0.0; // Orientation (deg) of ellipse clockwise from North in [-90, 90]
	_rlanType = RLANType::RLAN_OUTDOOR;
	_rlanHeightType = CConst::AGLHeightType;

	_buildingType = CConst::noBuildingType;

	_fixedBuildingLossFlag = false;
	_fixedBuildingLossValue = 0.0;

	_confidenceBldg2109 = quietNaN;
	_confidenceClutter2108 = quietNaN;
	_confidenceWinner2LOS = quietNaN;
	_confidenceWinner2NLOS = quietNaN;
	_confidenceWinner2Combined = quietNaN;
	_confidenceITM = quietNaN;
	_reliabilityITM = quietNaN;

	_winner2LOSOption = CConst::UnknownLOSOption;

	_channelResponseAlgorithm = CConst::pwrSpectralAlgorithm;

	_winner2UnknownLOSMethod = CConst::PLOSCombineWinner2UnknownLOSMethod;

	_winner2ProbLOSThr = quietNaN; // Winner2 prob LOS threshold, if probLOS exceeds threshold,
				       // use LOS model, otherwise use NLOS

	_winner2UseGroundDistanceFlag = true;
	_fsplUseGroundDistanceFlag = false;

	_propEnvMethod = CConst::unknownPropEnvMethod;

	_rxFeederLossDBIDU = quietNaN;
	_rxFeederLossDBODU = quietNaN;
	_rxFeederLossDBUnknown = quietNaN;

	_itmEpsDielect = quietNaN;
	_itmSgmConductivity = quietNaN;
	_itmPolarization = 1;
	_itmMinSpacing = 5.0;
	_itmMaxNumPts = 1500;

	_exclusionZoneFSID = 0;
	_exclusionZoneRLANChanIdx = -1;
	_exclusionZoneRLANBWHz = quietNaN;
	_exclusionZoneRLANEIRPDBm = quietNaN;

	_heatmapMinLon = quietNaN;
	_heatmapMaxLon = quietNaN;
	_heatmapMinLat = quietNaN;
	_heatmapMaxLat = quietNaN;
	_heatmapRLANSpacing = quietNaN;

	_heatmapRLANIndoorEIRPDBm = quietNaN;
	_heatmapRLANIndoorHeight = quietNaN;
	_heatmapRLANIndoorHeightUncertainty = quietNaN;

	_heatmapRLANOutdoorEIRPDBm = quietNaN;
	_heatmapRLANOutdoorHeight = quietNaN;
	_heatmapRLANOutdoorHeightUncertainty = quietNaN;

	_applyClutterFSRxFlag = false;
	_allowRuralFSClutterFlag = false;
	_fsConfidenceClutter2108 = quietNaN;
	_maxFsAglHeight = quietNaN;

	_rlanITMTxClutterMethod = CConst::ForceTrueITMClutterMethod;

	_cdsmLOSThr = quietNaN;

	_minRlanHeightAboveTerrain = quietNaN;

	_maxRadius = quietNaN;
	_exclusionDist = quietNaN;

	_nearFieldAdjFlag = true;
	_passiveRepeaterFlag = true;
	_reportErrorRlanHeightLowFlag = false;
	_illuminationEfficiency = quietNaN;
	_closeInHgtFlag = true;
	_closeInHgtLOS = quietNaN;
	_closeInDist = quietNaN;
	_pathLossClampFSPL = false;
	_printSkippedLinksFlag = false;
	_roundPSDEIRPFlag = true;

	_wlanMinFreqMHz = -1;
	_wlanMaxFreqMHz = -1;
	_wlanMinFreq = quietNaN;
	_wlanMaxFreq = quietNaN;

	_regionPolygonResolution = quietNaN;
	_rainForestPolygon = (PolygonClass *)NULL;

	_densityThrUrban = quietNaN;
	_densityThrSuburban = quietNaN;
	_densityThrRural = quietNaN;

	_removeMobile = false;

	_filterSimRegionOnly = false;

	_ulsDefaultAntennaType = CConst::F1245AntennaType;

	_visibilityThreshold =
		quietNaN; // I/N threshold to determine whether or not an RLAN is visible to an FS
	_maxLidarRegionLoadVal = -1;

	_terrainDataModel = (TerrainClass *)NULL;

	_bodyLossDB = 0.0;

	_numRegion = -1;

	_popGrid = (PopGridClass *)NULL;

	_ulsList = new ListClass<ULSClass *>(0);

	_pathLossModel = CConst::unknownPathLossModel;

	_zbldg2109 = quietNaN;
	_zclutter2108 = quietNaN;
	_fsZclutter2108 = quietNaN;
	_zwinner2LOS = quietNaN;
	_zwinner2NLOS = quietNaN;
	_zwinner2Combined = quietNaN;

	_rlanRegion = (RlanRegionClass *)NULL;

	_ituData = (ITUDataClass *)NULL;
	_nfa = (NFAClass *)NULL;
	_prTable = (PRTABLEClass *)NULL;

	_aciFlag = true;

	_rlanAntenna = (AntennaClass *)NULL;
	_rlanPointing = Vector3(0.0, 0.0, 0.0);

	_exclusionZoneFSTerrainHeight = quietNaN;
	_exclusionZoneHeightAboveTerrain = quietNaN;

	_heatmapIToNDB = (double **)NULL;
	_heatmapIsIndoor = (bool **)NULL;
	_heatmapNumPtsLon = 0;
	_heatmapNumPtsLat = 0;
	_heatmapMinIToNDB = quietNaN;
	_heatmapMaxIToNDB = quietNaN;
	_heatmapIToNThresholdDB = quietNaN;

	_responseCode = CConst::successResponseCode;
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

	if (_ituData) {
		delete _ituData;
	}

	if (_nfa) {
		delete _nfa;
	}

	if (_prTable) {
		delete _prTable;
	}
}
/******************************************************************************************/

/******************************************************************************************/
/**** sortFunction used for RADIAL_POLY                                                ****/
/******************************************************************************************/
bool sortFunction(std::pair<double, double> p0, std::pair<double, double> p1)
{
	return (p0.first < p1.first);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::initializeDatabases()                                      ****/
/******************************************************************************************/
void AfcManager::initializeDatabases()
{
	if (_responseCode != CConst::successResponseCode) {
		return;
	}

	/**************************************************************************************/
	/* Read region polygons and confirm that rlan is inside simulation region             */
	/**************************************************************************************/
	std::vector<std::string> regionPolygonFileStrList = split(_regionPolygonFileList, ',');
	_numRegion = regionPolygonFileStrList.size();

	for (int regionIdx = 0; regionIdx < _numRegion; ++regionIdx) {
		std::vector<PolygonClass *> polyList =
			PolygonClass::readMultiGeometry(regionPolygonFileStrList[regionIdx],
							_regionPolygonResolution);
		PolygonClass *regionPoly = PolygonClass::combinePolygons(polyList);
		regionPoly->name = _regionPolygonFileList[regionIdx];

		double regionArea = regionPoly->comp_bdy_area();

		double checkArea = 0.0;
		for (int polyIdx = 0; polyIdx < (int)polyList.size(); ++polyIdx) {
			PolygonClass *poly = polyList[polyIdx];
			checkArea += poly->comp_bdy_area();
			delete poly;
		}

		if (fabs(regionArea - checkArea) > 1.0e-6) {
			throw std::runtime_error(ErrStream()
						 << "ERROR: INVALID region polygon file = "
						 << regionPolygonFileStrList[regionIdx]);
		}

		_regionPolygonList.push_back(regionPoly);
		LOGGER_INFO(logger) << "REGION: " << regionPolygonFileStrList[regionIdx]
				    << " AREA: " << regionArea;
	}

	double rlanLatitude, rlanLongitude, rlanHeightInput;
	std::tie(rlanLatitude, rlanLongitude, rlanHeightInput) = _rlanLLA;
	int xIdx = (int)floor(rlanLongitude / _regionPolygonResolution + 0.5);
	int yIdx = (int)floor(rlanLatitude / _regionPolygonResolution + 0.5);

	bool found = false;
	for (int polyIdx = 0; (polyIdx < (int)_regionPolygonList.size()) && (!found); ++polyIdx) {
		PolygonClass *poly = _regionPolygonList[polyIdx];

		if (poly->in_bdy_area(xIdx, yIdx)) {
			found = true;
		}
	}
	if (!found) {
		_responseCode = CConst::invalidValueResponseCode;
		_invalidParams << "latitude";
		_invalidParams << "longitude";
		return;
	}
	/**************************************************************************************/

	// Following lines are finding the minimum and maximum longitudes and latitudes with the
	// 150km rlan range
	double minLon, maxLon, minLat, maxLat;
	double minLonBldg, maxLonBldg, minLatBldg, maxLatBldg;

	if (_analysisType == "HeatmapAnalysis") {
		defineHeatmapColors();
	}

	createChannelList();

	if (_responseCode != CConst::successResponseCode) {
		return;
	}

	/**************************************************************************************/
	/* Compute ulsMinFreq, and ulsMaxFreq                                                 */
	/**************************************************************************************/
	int chanIdx;
	int ulsMinFreqMHz = 0;
	int ulsMaxFreqMHz = 0;
	for (chanIdx = 0; chanIdx < (int)_channelList.size(); ++chanIdx) {
		ChannelStruct *channel = &(_channelList[chanIdx]);
		int minFreqMHz;
		int maxFreqMHz;
		int startFreqMHz = channel->freqMHzList.front();
		int stopFreqMHz = channel->freqMHzList.back();
		if ((channel->type == ChannelType::INQUIRED_CHANNEL) && (_aciFlag)) {
			minFreqMHz = 2 * startFreqMHz - stopFreqMHz;
			maxFreqMHz = 2 * stopFreqMHz - startFreqMHz;
		} else {
			minFreqMHz = startFreqMHz;
			maxFreqMHz = stopFreqMHz;
		}
		if ((ulsMinFreqMHz == 0) || (minFreqMHz < ulsMinFreqMHz)) {
			ulsMinFreqMHz = minFreqMHz;
		}
		if ((ulsMaxFreqMHz == 0) || (maxFreqMHz > ulsMaxFreqMHz)) {
			ulsMaxFreqMHz = maxFreqMHz;
		}
	}

	double ulsMinFreq = ulsMinFreqMHz * 1.0e6;
	double ulsMaxFreq = ulsMaxFreqMHz * 1.0e6;
	/**************************************************************************************/

	/**************************************************************************************/
	/* Set Path Loss Model Parameters                                                     */
	/**************************************************************************************/
	switch (_pathLossModel) {
		case CConst::ITMBldgPathLossModel:
			_closeInDist = 0.0; // Radius in which close in path loss model is used
			break;
		case CConst::CoalitionOpt6PathLossModel:
			_closeInDist = 1.0e3; // Radius in which close in path loss model is used
			break;
		case CConst::FCC6GHzReportAndOrderPathLossModel:
			_closeInDist = 1.0e3; // Radius in which close in path loss model is used
			break;
		case CConst::FSPLPathLossModel:
			_closeInDist = 0.0; // Radius in which close in path loss model is used
			break;
		default:
			throw std::runtime_error(
				ErrStream()
				<< std::string("ERROR: Path Loss Model set to invalid value \"") +
					   CConst::strPathLossModelList->type_to_str(
						   _pathLossModel) +
					   "\"");
			break;
	}

	ULSClass::pathLossModel = _pathLossModel;

	/**************************************************************************************/

	if (_analysisType == "AP-AFC" || _analysisType == "ScanAnalysis" ||
	    _analysisType == "test_itm") {
		bool fixedHeightAMSL;
		if (_rlanType == RLANType::RLAN_INDOOR) {
			fixedHeightAMSL = _indoorFixedHeightAMSL;
		} else {
			fixedHeightAMSL = false;
		}

		double centerLat, centerLon;

		/**************************************************************************************/
		/* Create Rlan Uncertainty Region */
		/**************************************************************************************/
		switch (_rlanUncertaintyRegionType) {
			case ELLIPSE:
				_rlanRegion = (RlanRegionClass *)new EllipseRlanRegionClass(
					_rlanLLA,
					_rlanUncerts_m,
					_rlanOrientation_deg,
					fixedHeightAMSL);
				break;
			case LINEAR_POLY:
				_rlanRegion = (RlanRegionClass *)new PolygonRlanRegionClass(
					_rlanLLA,
					_rlanUncerts_m,
					_rlanLinearPolygon,
					LINEAR_POLY,
					fixedHeightAMSL);
				break;
			case RADIAL_POLY:
				std::sort(_rlanRadialPolygon.begin(),
					  _rlanRadialPolygon.end(),
					  sortFunction);
				_rlanRegion = (RlanRegionClass *)new PolygonRlanRegionClass(
					_rlanLLA,
					_rlanUncerts_m,
					_rlanRadialPolygon,
					RADIAL_POLY,
					fixedHeightAMSL);
				break;
			default:
				throw std::runtime_error(ErrStream()
							 << "ERROR: INVALID "
							    "_rlanUncertaintyRegionType = "
							 << _rlanUncertaintyRegionType);
				break;
		}
		/**************************************************************************************/

		double rlanRegionSize = _rlanRegion->getMaxDist();
		centerLon = _rlanRegion->getCenterLongitude();
		centerLat = _rlanRegion->getCenterLatitude();

		minLat = centerLat -
			 ((_maxRadius + rlanRegionSize) / CConst::earthRadius) * 180.0 / M_PI;
		maxLat = centerLat +
			 ((_maxRadius + rlanRegionSize) / CConst::earthRadius) * 180.0 / M_PI;

		double maxAbsLat = std::max(fabs(minLat), fabs(maxLat));
		minLon = centerLon - ((_maxRadius + rlanRegionSize) /
				      (CConst::earthRadius * cos(maxAbsLat * M_PI / 180.0))) *
					     180.0 / M_PI;
		maxLon = centerLon + ((_maxRadius + rlanRegionSize) /
				      (CConst::earthRadius * cos(maxAbsLat * M_PI / 180.0))) *
					     180.0 / M_PI;

		if (_pathLossModel == CConst::FCC6GHzReportAndOrderPathLossModel) {
			double maxDistBldg;
			if (_rlanITMTxClutterMethod == CConst::BldgDataITMCLutterMethod) {
				maxDistBldg = _maxRadius;
			} else {
				maxDistBldg = _closeInDist;
			}

			minLatBldg = centerLat -
				     ((maxDistBldg + rlanRegionSize) / CConst::earthRadius) *
					     180.0 / M_PI;
			maxLatBldg = centerLat +
				     ((maxDistBldg + rlanRegionSize) / CConst::earthRadius) *
					     180.0 / M_PI;

			double maxAbsLatBldg = std::max(fabs(minLatBldg), fabs(maxLatBldg));
			minLonBldg = centerLon -
				     ((maxDistBldg + rlanRegionSize) /
				      (CConst::earthRadius * cos(maxAbsLatBldg * M_PI / 180.0))) *
					     180.0 / M_PI;
			maxLonBldg = centerLon +
				     ((maxDistBldg + rlanRegionSize) /
				      (CConst::earthRadius * cos(maxAbsLatBldg * M_PI / 180.0))) *
					     180.0 / M_PI;
		} else {
			minLatBldg = minLat;
			maxLatBldg = maxLat;
			minLonBldg = minLon;
			maxLonBldg = maxLon;
		}
	} else if (_analysisType == "ExclusionZoneAnalysis") {
		readULSData(_ulsDatabaseList,
			    (PopGridClass *)NULL,
			    0,
			    ulsMinFreq,
			    ulsMaxFreq,
			    _removeMobile,
			    CConst::FixedSimulation,
			    0.0,
			    0.0,
			    0.0,
			    0.0);
		readDeniedRegionData(_deniedRegionFile);
		if (_ulsList->getSize() == 0) {
		} else if (_ulsList->getSize() > 1) {
		}
		double centerLat = (*_ulsList)[0]->getRxLatitudeDeg();
		double centerLon = (*_ulsList)[0]->getRxLongitudeDeg();

		minLat = centerLat - ((_maxRadius) / CConst::earthRadius) * 180.0 / M_PI;
		maxLat = centerLat + ((_maxRadius) / CConst::earthRadius) * 180.0 / M_PI;

		double maxAbsLat = std::max(fabs(minLat), fabs(maxLat));
		minLon = centerLon -
			 ((_maxRadius) / (CConst::earthRadius * cos(maxAbsLat * M_PI / 180.0))) *
				 180.0 / M_PI;
		maxLon = centerLon +
			 ((_maxRadius) / (CConst::earthRadius * cos(maxAbsLat * M_PI / 180.0))) *
				 180.0 / M_PI;

		if (_pathLossModel == CConst::FCC6GHzReportAndOrderPathLossModel) {
			minLatBldg = centerLat -
				     ((_closeInDist) / CConst::earthRadius) * 180.0 / M_PI;
			maxLatBldg = centerLat +
				     ((_closeInDist) / CConst::earthRadius) * 180.0 / M_PI;

			double maxAbsLatBldg = std::max(fabs(minLatBldg), fabs(maxLatBldg));
			minLonBldg = centerLon -
				     ((_closeInDist) /
				      (CConst::earthRadius * cos(maxAbsLatBldg * M_PI / 180.0))) *
					     180.0 / M_PI;
			maxLonBldg = centerLon +
				     ((_closeInDist) /
				      (CConst::earthRadius * cos(maxAbsLatBldg * M_PI / 180.0))) *
					     180.0 / M_PI;
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
		minLon = _heatmapMinLon -
			 (_maxRadius / (CConst::earthRadius * cos(maxAbsLat * M_PI / 180.0))) *
				 180.0 / M_PI;
		maxLon = _heatmapMaxLon +
			 (_maxRadius / (CConst::earthRadius * cos(maxAbsLat * M_PI / 180.0))) *
				 180.0 / M_PI;

		if (_pathLossModel == CConst::FCC6GHzReportAndOrderPathLossModel) {
			minLatBldg = _heatmapMinLat -
				     (_closeInDist / CConst::earthRadius) * 180.0 / M_PI;
			maxLatBldg = _heatmapMaxLat +
				     (_closeInDist / CConst::earthRadius) * 180.0 / M_PI;

			double maxAbsLatBldg = std::max(fabs(minLatBldg), fabs(maxLatBldg));
			minLonBldg = _heatmapMinLon -
				     (_closeInDist /
				      (CConst::earthRadius * cos(maxAbsLatBldg * M_PI / 180.0))) *
					     180.0 / M_PI;
			maxLonBldg = _heatmapMaxLon +
				     (_closeInDist /
				      (CConst::earthRadius * cos(maxAbsLatBldg * M_PI / 180.0))) *
					     180.0 / M_PI;
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
		throw std::runtime_error(QString("Invalid analysis type: %1")
						 .arg(QString::fromStdString(_analysisType))
						 .toStdString());
	}

	/**************************************************************************************/
	/* Setup Terrain data                                                                    */
	/**************************************************************************************/
	// The following counters are used to understand the percentage of holes in terrain data
	// that exist (mostly for debugging)
	UlsMeasurementAnalysis::numInvalidSRTM =
		0; // Define counters for invalid heights returned by SRTM
	UlsMeasurementAnalysis::numSRTM = 0; // Define counters for all SRTM calls

	_maxLidarRegionLoadVal = 70;

	_terrainDataModel = new TerrainClass(_lidarDir,
					     _cdsmDir,
					     _srtmDir,
					     _depDir,
					     _globeDir,
					     minLat,
					     minLon,
					     maxLat,
					     maxLon,
					     minLatBldg,
					     minLonBldg,
					     maxLatBldg,
					     maxLonBldg,
					     _maxLidarRegionLoadVal);

	_terrainDataModel->setSourceName(CConst::HeightSourceEnum::unknownHeightSource, "UNKNOWN");
	_terrainDataModel->setSourceName(CConst::HeightSourceEnum::globalHeightSource, "GLOBE");
	_terrainDataModel->setSourceName(CConst::HeightSourceEnum::depHeightSource,
					 "3DEP 1 arcsec");
	_terrainDataModel->setSourceName(CConst::HeightSourceEnum::srtmHeightSource, "SRTM");
	if (_useBDesignFlag) {
		_terrainDataModel->setSourceName(CConst::HeightSourceEnum::lidarHeightSource,
						 "B3D-3DEP");
	} else if (_useLiDAR) {
		_terrainDataModel->setSourceName(CConst::HeightSourceEnum::lidarHeightSource,
						 "LiDAR");
	}

	if (_pathLossModel == CConst::ITMBldgPathLossModel) {
		if (_terrainDataModel->getNumLidarRegion() == 0) {
			throw std::runtime_error(ErrStream() << "Path loss model set to ITM_BLDG, "
								"but no building data found within "
								"the analysis region.");
		}
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Setup ITU data                                                                    */
	/**************************************************************************************/
	_ituData = new ITUDataClass(_radioClimateFile, _surfRefracFile);
	LOGGER_INFO(logger) << "Reading ITU data files: " << _radioClimateFile << " and "
			    << _surfRefracFile;
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
	_prTable = new PRTABLEClass(_prTableFile);
	/**************************************************************************************/

	/**************************************************************************************/
	/* Read NLCD data or Polulation Density data depending on propEnvMethod               */
	/**************************************************************************************/
	if (_propEnvMethod == CConst::nlcdPointPropEnvMethod) {
		/**********************************************************************************/
		/* Setup NLCD data                                                                */
		/**********************************************************************************/
		if (_nlcdFile.empty()) {
			throw std::runtime_error("AfcManager::initializeDatabases(): _nlcdFile not "
						 "defined.");
		}
		std::string nlcdDirectory;
		auto nlcdFileInfo = QFileInfo(QString::fromStdString(_nlcdFile));
		std::unique_ptr<GdalNameMapperBase> nameMapper(nullptr);
		// NLCD specification in config may be of various flavors
		if (nlcdFileInfo.fileName().contains('{')) {
			// Name part contains template - tiled NLCD. Sample config:
			// "nlcdFile":
			// "rat_transfer/tiled_nlcd/nlcd_production/nlcd_production_{latHem:ns}{latDegCeil:02}{lonHem:ew}{lonDegFloor:03}.tif"
			// See comments in GdalNameMapper.h for formatting details
			nlcdDirectory = nlcdFileInfo.dir().path().toStdString();
			nameMapper = GdalNameMapperPattern::make_unique(
				nlcdFileInfo.fileName().toStdString(),
				nlcdDirectory);
		} else if (nlcdFileInfo.isDir()) {
			// Path is directory - set of files. Sample config:
			// "nlcdFile":
			// "rat_transfer/nlcd/nlcd_production"
			nlcdDirectory = _nlcdFile;
			nameMapper = GdalNameMapperDirect::make_unique("*", _nlcdFile);
		} else {
			// Otherwise path supposed to be a file. Sample config:
			// "nlcdFile":
			// "rat_transfer/nlcd/nlcd_production/nlcd_2019_land_cover_l48_20210604_resample.tif"
			nlcdDirectory = nlcdFileInfo.dir().path().toStdString();
			nameMapper = GdalNameMapperDirect::make_unique(
				nlcdFileInfo.fileName().toStdString(),
				nlcdDirectory);
		}
		cgNlcd.reset(new CachedGdal<uint8_t>(nlcdDirectory, "nlcd", std::move(nameMapper)));
		cgNlcd->setNoData(0);
		/**********************************************************************************/
	} else if (_propEnvMethod == CConst::popDensityMapPropEnvMethod) {
		if (!(_rainForestFile.empty())) {
			std::vector<PolygonClass *> polyList =
				PolygonClass::readMultiGeometry(_rainForestFile,
								_regionPolygonResolution);
			_rainForestPolygon = PolygonClass::combinePolygons(polyList);

			_rainForestPolygon->name = "Rain Forest";

			for (int polyIdx = 0; polyIdx < (int)polyList.size(); ++polyIdx) {
				PolygonClass *poly = polyList[polyIdx];
				delete poly;
			}
		}

		_popGrid = new PopGridClass(_worldPopulationFile,
					    _regionPolygonList,
					    _regionPolygonResolution,
					    _densityThrUrban,
					    _densityThrSuburban,
					    _densityThrRural,
					    minLat,
					    minLon,
					    maxLat,
					    maxLon);
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Read ULS data                                                                      */
	/**************************************************************************************/
	if (_analysisType == "HeatmapAnalysis" || _analysisType == "AP-AFC" ||
	    _analysisType == "ScanAnalysis") {
		readULSData(_ulsDatabaseList,
			    _popGrid,
			    0,
			    ulsMinFreq,
			    ulsMaxFreq,
			    _removeMobile,
			    CConst::FixedSimulation,
			    minLat,
			    maxLat,
			    minLon,
			    maxLon);
		readDeniedRegionData(_deniedRegionFile);
	} else if (_analysisType == "ExclusionZoneAnalysis") {
		fixFSTerrain();
#if DEBUG_AFC
	} else if (_analysisType == "test_itm") {
		// Do nothing
	} else if (_analysisType == "test_winner2") {
		// Do nothing
#endif
	} else {
		throw std::runtime_error(QString("Invalid analysis type: %1")
						 .arg(QString::fromStdString(_analysisType))
						 .toStdString());
	}
	/**************************************************************************************/

	splitFrequencyRanges();

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

	for (int antIdx = 0; antIdx < (int)_antennaList.size(); antIdx++) {
		delete _antennaList[antIdx];
	}

	for (int drIdx = 0; drIdx < (int)_deniedRegionList.size(); drIdx++) {
		delete _deniedRegionList[drIdx];
	}

	if (_popGrid) {
		delete _popGrid;
		_popGrid = (PopGridClass *)NULL;
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

		_heatmapIToNDB = (double **)NULL;
		_heatmapNumPtsLon = 0;
		_heatmapNumPtsLat = 0;
	}

	if (_nearFieldAdjFlag) {
		delete _nfa;
		_nfa = (NFAClass *)NULL;
	}

	if (_passiveRepeaterFlag) {
		delete _prTable;
		_prTable = (PRTABLEClass *)NULL;
	}

	for (int regionIdx = 0; regionIdx < _numRegion; ++regionIdx) {
		delete _regionPolygonList[regionIdx];
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

	for (ulsIdx = 0; ulsIdx <= _ulsList->getSize() - 1; ulsIdx++) {
		uls = (*_ulsList)[ulsIdx];
		delete uls;
	}
	_ulsList->reset();
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AfcManager::importGUIjson                                              ****/
/******************************************************************************************/
void AfcManager::importGUIjson(const std::string &inputJSONpath)
{
	// Read input parameters from GUI in JSON file
	QJsonDocument jsonDoc;
	// Use SearchPaths::forReading("data", ..., true) to ensure that the input file exists
	// before reading it in
	QString fName = QString(inputJSONpath.c_str());
	QByteArray data;

	if (AfcManager::_dataIf->readFile(fName, data)) {
		jsonDoc = QJsonDocument::fromJson(data);
	} else {
		throw std::runtime_error("AfcManager::importGUIjson(): Failed to open JSON file "
					 "specifying user's input parameters.");
	}

	// Print entirety of imported JSON file to debug log
	LOGGER_DEBUG(logger) << "Contents of imported JSON file: " << std::endl
			     << jsonDoc.toJson().toStdString() << std::endl;

	// Read JSON from file
	QJsonObject jsonObj = jsonDoc.object();

	if (jsonObj.contains("version") && !jsonObj["version"].isUndefined()) {
		_guiJsonVersion = jsonObj["version"].toString();
	} else {
		_guiJsonVersion = QString("1.4");
	}

	if (_guiJsonVersion == "1.4") {
		importGUIjsonVersion1_4(jsonObj);
	} else {
		LOGGER_WARN(logger) << "VERSION NOT SUPPORTED: GUI JSON FILE \"" << inputJSONpath
				    << "\": version: " << _guiJsonVersion;
		_responseCode = CConst::versionNotSupportedResponseCode;
		return;
	}

	return;
}

void AfcManager::importGUIjsonVersion1_4(const QJsonObject &jsonObj)
{
	QString errMsg;

	if ((_analysisType == "AP-AFC") || (_analysisType == "ScanAnalysis") ||
	    (_analysisType == "test_itm") || (_analysisType == "HeatmapAnalysis")) {
		QStringList requiredParams;
		QStringList optionalParams;

		/**********************************************************************/
		/* AvailableSpectrumInquiryRequestMessage Object (Table 5)            */
		/**********************************************************************/
		requiredParams = QStringList() << "availableSpectrumInquiryRequests"
					       << "version";
		optionalParams = QStringList() << "vendorExtensions";
		for (auto &key : jsonObj.keys()) {
			int rIdx = requiredParams.indexOf(key);
			if (rIdx != -1) {
				requiredParams.erase(requiredParams.begin() + rIdx);
			} else {
				int oIdx = optionalParams.indexOf(key);
				if (oIdx != -1) {
					optionalParams.erase(optionalParams.begin() + oIdx);
				} else {
					_unexpectedParams << key;
				}
			}
		}
		_missingParams << requiredParams;

		QJsonObject requestObj;
		if (!requiredParams.contains("availableSpectrumInquiryRequests")) {
			QJsonArray requestArray =
				jsonObj["availableSpectrumInquiryRequests"].toArray();
			if (requestArray.size() != 1) {
				LOGGER_WARN(logger) << "GENERAL FAILURE: afc-engine only processes "
						       "a single request, "
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
		requiredParams = QStringList() << "requestId"
					       << "deviceDescriptor"
					       << "location";
		optionalParams = QStringList() << "inquiredFrequencyRange"
					       << "inquiredChannels"
					       << "minDesiredPower"
					       << "vendorExtensions";
		for (auto &key : requestObj.keys()) {
			int rIdx = requiredParams.indexOf(key);
			if (rIdx != -1) {
				requiredParams.erase(requiredParams.begin() + rIdx);
			} else {
				int oIdx = optionalParams.indexOf(key);
				if (oIdx != -1) {
					optionalParams.erase(optionalParams.begin() + oIdx);
				} else {
					_unexpectedParams << key;
				}
			}
		}
		_missingParams << requiredParams;

		/* The requestId is required for any response so keep it */
		if (!requiredParams.contains("requestId")) {
			_requestId = requestObj["requestId"].toString();
		}

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
			inquiredFrequencyRangeArray =
				requestObj["inquiredFrequencyRange"].toArray();
		}

		QJsonArray inquiredChannelsArray;
		if (!optionalParams.contains("inquiredChannels")) {
			inquiredChannelsArray = requestObj["inquiredChannels"].toArray();
		}

		double minDesiredPower;
		if (!optionalParams.contains("minDesiredPower")) {
			minDesiredPower = requestObj["minDesiredPower"].toDouble();
		} else {
			minDesiredPower = quietNaN;
		}

		QJsonArray vendorExtensionArray;
		if (!optionalParams.contains("vendorExtensions")) {
			vendorExtensionArray = requestObj["vendorExtensions"].toArray();
		}

		/**********************************************************************/

		/**********************************************************************/
		/* DeviceDescriptor Object (Table 7)                                  */
		/**********************************************************************/
		requiredParams = QStringList() << "serialNumber"
					       << "certificationId";
		optionalParams = QStringList();
		for (auto &key : deviceDescriptorObj.keys()) {
			int rIdx = requiredParams.indexOf(key);
			if (rIdx != -1) {
				requiredParams.erase(requiredParams.begin() + rIdx);
			} else {
				int oIdx = optionalParams.indexOf(key);
				if (oIdx != -1) {
					optionalParams.erase(optionalParams.begin() + oIdx);
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

		if (!requiredParams.contains("serialNumber")) {
			_serialNumber = deviceDescriptorObj["serialNumber"].toString();
		}

		/**********************************************************************/

		/**********************************************************************/
		/* CertificationID Object (Table 8)                                   */
		/**********************************************************************/
		for (auto certificationIDVal : certificationIdArray) {
			auto certificationIDObj = certificationIDVal.toObject();
			requiredParams = QStringList() << "rulesetId"
						       << "id";
			optionalParams = QStringList();
			for (auto &key : certificationIDObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin() + rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin() + oIdx);
					} else {
						_unexpectedParams << key;
					}
				}
			}
			_missingParams << requiredParams;
			if (!requiredParams.contains("rulesetId")) {
				_rulesetId = certificationIdArray.at(0)["rulesetId"].toString();
			}
		}
		/**********************************************************************/

		/**********************************************************************/
		/* Location Object (Table 9)                                          */
		/**********************************************************************/
		requiredParams = QStringList() << "elevation";
		optionalParams = QStringList() << "ellipse"
					       << "linearPolygon"
					       << "radialPolygon"
					       << "indoorDeployment";
		for (auto &key : locationObj.keys()) {
			int rIdx = requiredParams.indexOf(key);
			if (rIdx != -1) {
				requiredParams.erase(requiredParams.begin() + rIdx);
			} else {
				int oIdx = optionalParams.indexOf(key);
				if (oIdx != -1) {
					optionalParams.erase(optionalParams.begin() + oIdx);
				} else {
					_unexpectedParams << key;
				}
			}
		}
		_missingParams << requiredParams;

		int hasIndoorDeploymentFlag = (optionalParams.contains("indoorDeployment") ? 0 : 1);

		int hasEllipseFlag = (optionalParams.contains("ellipse") ? 0 : 1);
		int hasLinearPolygonFlag = (optionalParams.contains("linearPolygon") ? 0 : 1);
		int hasRadialPolygonFlag = (optionalParams.contains("radialPolygon") ? 0 : 1);

		int n = hasEllipseFlag + hasLinearPolygonFlag + hasRadialPolygonFlag;

		if (n != 1) {
			LOGGER_WARN(logger)
				<< "GENERAL FAILURE: location object must contain exactly one "
				   "instance of ellipse, linearPolygon, or radialPolygon, total of "
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
			requiredParams = QStringList() << "center"
						       << "majorAxis"
						       << "minorAxis"
						       << "orientation";
			optionalParams = QStringList();
			for (auto &key : ellipseObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin() + rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin() + oIdx);
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
			for (auto &key : linearPolygonObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin() + rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin() + oIdx);
					} else {
						_unexpectedParams << key;
					}
				}
			}
			_missingParams << requiredParams;

			if (!requiredParams.contains("outerBoundary")) {
				outerBoundaryPointArray =
					linearPolygonObj["outerBoundary"].toArray();
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
			requiredParams = QStringList() << "center"
						       << "outerBoundary";
			optionalParams = QStringList();
			for (auto &key : radialPolygonObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin() + rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin() + oIdx);
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
				outerBoundaryVectorArray =
					radialPolygonObj["outerBoundary"].toArray();
				hasOuterBoundaryVectorArrayFlag = true;
			}
		}
		/**********************************************************************/

		/**********************************************************************/
		/* ElevationPolygon Object (Table 13)                                 */
		/**********************************************************************/
		requiredParams = QStringList() << "height"
					       << "heightType"
					       << "verticalUncertainty";
		optionalParams = QStringList();
		for (auto &key : elevationObj.keys()) {
			int rIdx = requiredParams.indexOf(key);
			if (rIdx != -1) {
				requiredParams.erase(requiredParams.begin() + rIdx);
			} else {
				int oIdx = optionalParams.indexOf(key);
				if (oIdx != -1) {
					optionalParams.erase(optionalParams.begin() + oIdx);
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
			requiredParams = QStringList() << "longitude"
						       << "latitude";
			optionalParams = QStringList();
			for (auto &key : centerObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin() + rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin() + oIdx);
					} else {
						_unexpectedParams << key;
					}
				}
			}
			_missingParams << requiredParams;
		}

		if (hasOuterBoundaryPointArrayFlag) {
			for (auto outerBoundaryPointVal : outerBoundaryPointArray) {
				auto outerBoundaryPointObj = outerBoundaryPointVal.toObject();
				requiredParams = QStringList() << "longitude"
							       << "latitude";
				optionalParams = QStringList();
				for (auto &key : outerBoundaryPointObj.keys()) {
					int rIdx = requiredParams.indexOf(key);
					if (rIdx != -1) {
						requiredParams.erase(requiredParams.begin() + rIdx);
					} else {
						int oIdx = optionalParams.indexOf(key);
						if (oIdx != -1) {
							optionalParams.erase(
								optionalParams.begin() + oIdx);
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
			for (auto outerBoundaryVectorVal : outerBoundaryVectorArray) {
				auto outerBoundaryVectorObj = outerBoundaryVectorVal.toObject();
				requiredParams = QStringList() << "length"
							       << "angle";
				optionalParams = QStringList();
				for (auto &key : outerBoundaryVectorObj.keys()) {
					int rIdx = requiredParams.indexOf(key);
					if (rIdx != -1) {
						requiredParams.erase(requiredParams.begin() + rIdx);
					} else {
						int oIdx = optionalParams.indexOf(key);
						if (oIdx != -1) {
							optionalParams.erase(
								optionalParams.begin() + oIdx);
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
		for (auto inquiredFrequencyRangeVal : inquiredFrequencyRangeArray) {
			auto inquiredFrequencyRangeObj = inquiredFrequencyRangeVal.toObject();
			requiredParams = QStringList() << "lowFrequency"
						       << "highFrequency";
			optionalParams = QStringList();
			for (auto &key : inquiredFrequencyRangeObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin() + rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin() + oIdx);
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
		for (auto inquiredChannelsVal : inquiredChannelsArray) {
			auto inquiredChannelsObj = inquiredChannelsVal.toObject();
			requiredParams = QStringList() << "globalOperatingClass";
			optionalParams = QStringList() << "channelCfi";
			for (auto &key : inquiredChannelsObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin() + rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin() + oIdx);
					} else {
						_unexpectedParams << key;
					}
				}
			}
			_missingParams << requiredParams;
		}
		/**********************************************************************/

		/**********************************************************************/
		/* VendorExtension Object (Table 23)                                  */
		/**********************************************************************/
		for (auto vendorExtensionVal : vendorExtensionArray) {
			auto vendorExtensionObj = vendorExtensionVal.toObject();
			requiredParams = QStringList() << "extensionId"
						       << "parameters";
			optionalParams = QStringList();
			for (auto &key : vendorExtensionObj.keys()) {
				int rIdx = requiredParams.indexOf(key);
				if (rIdx != -1) {
					requiredParams.erase(requiredParams.begin() + rIdx);
				} else {
					int oIdx = optionalParams.indexOf(key);
					if (oIdx != -1) {
						optionalParams.erase(optionalParams.begin() + oIdx);
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
		if (certificationIdArray.size() == 0) {
			_responseCode = CConst::invalidValueResponseCode;
			_invalidParams << "certificationId";
			return;
		}

		if (hasIndoorDeploymentFlag) {
			int indoorDeploymentVal = locationObj["indoorDeployment"].toInt();
			switch (indoorDeploymentVal) {
				case 0: /* Unknown */
					if (_certifiedIndoor) {
						_rlanType = RLANType::RLAN_INDOOR;
					} else {
						_rlanType = RLANType::RLAN_OUTDOOR;
					}
					break;
				case 1: /* Indoor */
					if (_rulesetId ==
					    QString::fromStdString("US_47_CFR_PART_15_SUBPART_E")) {
						/* US */
						if (_certifiedIndoor) {
							_rlanType = RLANType::RLAN_INDOOR;
						} else {
							LOGGER_INFO(logger)
								<< _serialNumber.toStdString()
								<< " indicated as deployed indoor "
								<< "but is not certified as "
								   "Indoor. "
								<< "So it is analyzed as outdoor";

							_rlanType = RLANType::RLAN_OUTDOOR;
						}
					} else {
						/* Everywhere else including Canada */
						_rlanType = RLANType::RLAN_INDOOR;
					}
					break;
				case 2:
					_rlanType = RLANType::RLAN_OUTDOOR;
					break;
				default:
					_invalidParams << "indoorDeployment";
			}
		} else { /* not specified, treated as unknown */
			if (_certifiedIndoor) {
				_rlanType = RLANType::RLAN_INDOOR;
			} else {
				_rlanType = RLANType::RLAN_OUTDOOR;
			}
		}

		if (_rlanType == RLANType::RLAN_INDOOR) {
			_bodyLossDB = _bodyLossIndoorDB;
			if ((!std::isnan(minDesiredPower)) &&
			    (minDesiredPower > _minEIRPIndoor_dBm)) {
				_minEIRP_dBm = minDesiredPower;
			} else {
				_minEIRP_dBm = _minEIRPIndoor_dBm;
			}
		} else {
			_buildingType = CConst::noBuildingType;
			_confidenceBldg2109 = 0.0;
			_fixedBuildingLossFlag = false;
			_fixedBuildingLossValue = 0.0;
			_bodyLossDB = _bodyLossOutdoorDB;
			if ((!std::isnan(minDesiredPower)) &&
			    (minDesiredPower > _minEIRPOutdoor_dBm)) {
				_minEIRP_dBm = minDesiredPower;
			} else {
				_minEIRP_dBm = _minEIRPOutdoor_dBm;
			}
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
		} else if (verticalUncertainty > _maxVerticalUncertainty) {
			LOGGER_WARN(logger)
				<< "GENERAL FAILURE: verticalUncertainty = " << verticalUncertainty
				<< " exceeds max value of " << _maxVerticalUncertainty;
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

			if ((centerLatitude < -90.0) || (centerLatitude > 90.0)) {
				_invalidParams << "latitude";
			}
			if ((centerLongitude < -180.0) || (centerLongitude > 180.0)) {
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
					LOGGER_WARN(logger) << "GENERAL FAILURE: majorAxis < 0";
					_invalidParams << "majorAxis";
				} else if (2 * majorAxis > _maxHorizontalUncertaintyDistance) {
					LOGGER_WARN(logger)
						<< "GENERAL FAILURE: 2*majorAxis = "
						<< 2 * majorAxis << " exceeds max value of "
						<< _maxHorizontalUncertaintyDistance;
					_invalidParams << "majorAxis";
				}
			}

			if ((orientation < 0.0) || (orientation > 180.0)) {
				_invalidParams << "orientation";
			}

			_rlanLLA = std::make_tuple(centerLatitude, centerLongitude, centerHeight);
			_rlanUncerts_m = std::make_tuple(minorAxis, majorAxis, verticalUncertainty);
			_rlanOrientation_deg = orientation;
		} else if (hasLinearPolygonFlag) {
			_rlanUncertaintyRegionType = RLANBoundary::LINEAR_POLY;

			if ((outerBoundaryPointArray.size() < 3) ||
			    (outerBoundaryPointArray.size() > 15)) {
				_invalidParams << "outerBoundary";
			}

			for (auto outerBoundaryPointVal : outerBoundaryPointArray) {
				auto outerBoundaryPointObj = outerBoundaryPointVal.toObject();
				double latitude = outerBoundaryPointObj["latitude"].toDouble();
				double longitude = outerBoundaryPointObj["longitude"].toDouble();

				if ((latitude < -90.0) || (latitude > 90.0)) {
					_invalidParams << "latitude";
				}
				if ((longitude < -180.0) || (longitude > 180.0)) {
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
			for (i = 0; i < (int)_rlanLinearPolygon.size(); i++) {
				sumLon += _rlanLinearPolygon[i].second;
				sumLat += _rlanLinearPolygon[i].first;
			}
			centerLongitude = sumLon / _rlanLinearPolygon.size();
			centerLatitude = sumLat / _rlanLinearPolygon.size();

			double cosLat = cos(centerLatitude * M_PI / 180.0);
			double maxHorizDistSq = 0.0;
			for (i = 0; i < (int)_rlanLinearPolygon.size(); i++) {
				double lon_i = _rlanLinearPolygon[i].second;
				double lat_i = _rlanLinearPolygon[i].first;
				for (int j = i + 1; j < (int)_rlanLinearPolygon.size() + 1; j++) {
					double jj = j % _rlanLinearPolygon.size();
					double lon_j = _rlanLinearPolygon[jj].second;
					double lat_j = _rlanLinearPolygon[jj].first;
					double deltaX = (lon_j - lon_i) * (M_PI / 180.0) * cosLat *
							CConst::earthRadius;
					double deltaY = (lat_j - lat_i) * (M_PI / 180.0) *
							CConst::earthRadius;
					double distSq = deltaX * deltaX + deltaY * deltaY;
					if (distSq > maxHorizDistSq) {
						maxHorizDistSq = distSq;
					}
				}
			}

			if (maxHorizDistSq >
			    _maxHorizontalUncertaintyDistance * _maxHorizontalUncertaintyDistance) {
				LOGGER_WARN(logger)
					<< "GENERAL FAILURE: Linear polygon contains vertices with "
					   "separation distance that exceeds max value of "
					<< _maxHorizontalUncertaintyDistance;
				_invalidParams << "outerBoundary";
			}

			_rlanLLA = std::make_tuple(centerLatitude, centerLongitude, centerHeight);
			_rlanUncerts_m = std::make_tuple(quietNaN, quietNaN, verticalUncertainty);
		} else if (hasRadialPolygonFlag) {
			_rlanUncertaintyRegionType = RLANBoundary::RADIAL_POLY;

			if ((outerBoundaryVectorArray.size() < 3) ||
			    (outerBoundaryVectorArray.size() > 15)) {
				_invalidParams << "outerBoundary";
			}

			for (auto outerBoundaryVectorVal : outerBoundaryVectorArray) {
				auto outerBoundaryVectorObj = outerBoundaryVectorVal.toObject();
				double angle = outerBoundaryVectorObj["angle"].toDouble();
				double length = outerBoundaryVectorObj["length"].toDouble();

				if (length < 0.0) {
					_invalidParams << "length";
				}
				if ((angle < 0.0) || (angle > 360.0)) {
					_invalidParams << "angle";
				}

				_rlanRadialPolygon.push_back(std::make_pair(angle, length));
			}

			double centerLatitude = centerObj["latitude"].toDouble();
			double centerLongitude = centerObj["longitude"].toDouble();

			double maxHorizDistSq = 0.0;
			for (int i = 0; i < (int)_rlanRadialPolygon.size(); i++) {
				double angle_i = _rlanRadialPolygon[i].second;
				double length_i = _rlanRadialPolygon[i].first;
				double x_i = length_i * sin(angle_i * M_PI / 360.0);
				double y_i = length_i * cos(angle_i * M_PI / 360.0);
				for (int j = i + 1; j < (int)_rlanRadialPolygon.size() + 1; j++) {
					double jj = j % _rlanRadialPolygon.size();
					double angle_j = _rlanRadialPolygon[jj].second;
					double length_j = _rlanRadialPolygon[jj].first;
					double x_j = length_j * sin(angle_j * M_PI / 360.0);
					double y_j = length_j * cos(angle_j * M_PI / 360.0);

					double deltaX = x_j - x_i;
					double deltaY = y_j - y_i;
					double distSq = deltaX * deltaX + deltaY * deltaY;
					if (distSq > maxHorizDistSq) {
						maxHorizDistSq = distSq;
					}
				}
			}

			if (maxHorizDistSq >
			    _maxHorizontalUncertaintyDistance * _maxHorizontalUncertaintyDistance) {
				LOGGER_WARN(logger)
					<< "GENERAL FAILURE: Radial polygon contains vertices with "
					   "separation distance that exceeds max value of "
					<< _maxHorizontalUncertaintyDistance;
				_invalidParams << "outerBoundary";
			}

			_rlanLLA = std::make_tuple(centerLatitude, centerLongitude, centerHeight);
			_rlanUncerts_m = std::make_tuple(quietNaN, quietNaN, verticalUncertainty);
		}

		for (auto inquiredChannelsVal : inquiredChannelsArray) {
			auto inquiredChannelsObj = inquiredChannelsVal.toObject();
			auto chanClass = std::make_pair<int, std::vector<int>>(
				inquiredChannelsObj["globalOperatingClass"].toInt(),
				std::vector<int>());
			if (inquiredChannelsObj.contains("channelCfi")) {
				for (const QJsonValue &chanIdx :
				     inquiredChannelsObj["channelCfi"].toArray())
					chanClass.second.push_back(chanIdx.toInt());
			}
			// TODO: are we handling the case where they want all?

			_inquiredChannels.push_back(chanClass);
		}

		for (auto inquiredFrequencyRangeVal : inquiredFrequencyRangeArray) {
			auto inquiredFrequencyRangeObj = inquiredFrequencyRangeVal.toObject();
			_inquiredFrequencyRangesMHz.push_back(
				std::make_pair(inquiredFrequencyRangeObj["lowFrequency"].toInt(),
					       inquiredFrequencyRangeObj["highFrequency"].toInt()));
		}

		bool hasRLANAntenna = false;
		for (auto vendorExtensionVal : vendorExtensionArray) {
			auto vendorExtensionObj = vendorExtensionVal.toObject();
			auto parametersObj = vendorExtensionObj["parameters"].toObject();
			if (parametersObj.contains("type") &&
			    !parametersObj["type"].isUndefined()) {
				std::string type = parametersObj["type"].toString().toStdString();
				if (type == "rlanAntenna") {
					if (hasRLANAntenna) {
						LOGGER_WARN(logger) << "GENERAL FAILURE: multiple "
								       "RLAN antennas specified";
						_responseCode = CConst::generalFailureResponseCode;
						return;
					}
					std::string antennaName = parametersObj["antennaModel"]
									  .toString()
									  .toStdString();
					_rlanAzimuthPointing =
						parametersObj["azimuthPointing"].toDouble();
					_rlanElevationPointing =
						parametersObj["elevationPointing"].toDouble();
					int numAntennaAOB = parametersObj["numAOB"].toInt();
					QJsonArray gainArray;
					gainArray = parametersObj["discriminationGainDB"].toArray();

					if (gainArray.size() != numAntennaAOB) {
						LOGGER_WARN(logger)
							<< "GENERAL FAILURE: numAntennaAOB = "
							<< numAntennaAOB
							<< " discriminationGainDB has "
							<< gainArray.size() << " elements";
						_responseCode = CConst::generalFailureResponseCode;
						return;
					}

					std::vector<std::tuple<double, double>> sampledData;
					std::tuple<double, double> pt;
					for (int aobIdx = 0; aobIdx < numAntennaAOB; ++aobIdx) {
						std::get<0>(pt) = (((double)aobIdx) /
								   (numAntennaAOB - 1)) *
								  M_PI;
						std::get<1>(pt) = gainArray.at(aobIdx).toDouble();
						sampledData.push_back(pt);
					}
					_rlanAntenna =
						new AntennaClass(CConst::antennaLUT_Boresight,
								 antennaName.c_str());
					LinInterpClass *gainTable = new LinInterpClass(sampledData);
					_rlanAntenna->setBoresightGainTable(gainTable);

					hasRLANAntenna = true;
				} else if (type == "heatmap") {
					requiredParams = QStringList() << "IndoorOutdoorStr"
								       << "MinLon"
								       << "MaxLon"
								       << "MinLat"
								       << "MaxLat"
								       << "RLANSpacing"
								       << "inquiredChannel"
								       << "analysis"
								       << "type"
								       << "fsIdType";
					optionalParams = QStringList()
							 << "RLANOutdoorEIRPDBm"
							 << "RLANOutdoorHeight"
							 << "RLANOutdoorHeightType"
							 << "RLANOutdoorHeightUncertainty"
							 << "RLANIndoorEIRPDBm"
							 << "RLANIndoorHeight"
							 << "RLANIndoorHeightType"
							 << "RLANIndoorHeightUncertainty"
							 << "fsId";

					for (auto &key : parametersObj.keys()) {
						int rIdx = requiredParams.indexOf(key);
						if (rIdx != -1) {
							requiredParams.erase(
								requiredParams.begin() + rIdx);
						} else {
							int oIdx = optionalParams.indexOf(key);
							if (oIdx != -1) {
								optionalParams.erase(
									optionalParams.begin() +
									oIdx);
							} else {
								_unexpectedParams << key;
							}
						}
					}
					_missingParams << requiredParams;

					if (_missingParams.size()) {
						_responseCode = CConst::missingParamResponseCode;
						return;
					} else if (_unexpectedParams.size()) {
						_responseCode = CConst::unexpectedParamResponseCode;
						return;
					}

					_heatmapMinLon = parametersObj["MinLon"].toDouble();
					_heatmapMaxLon = parametersObj["MaxLon"].toDouble();
					_heatmapMinLat = parametersObj["MinLat"].toDouble();
					_heatmapMaxLat = parametersObj["MaxLat"].toDouble();
					_heatmapRLANSpacing =
						parametersObj["RLANSpacing"].toDouble();
					_heatmapIndoorOutdoorStr = parametersObj["IndoorOutdoorStr"]
									   .toString()
									   .toStdString();

					bool outdoorFlag = false;
					bool indoorFlag = false;
					if (_heatmapIndoorOutdoorStr == "Outdoor") {
						outdoorFlag = true;
					} else if (_heatmapIndoorOutdoorStr == "Indoor") {
						indoorFlag = true;
					} else if (_heatmapIndoorOutdoorStr == "Database") {
						outdoorFlag = true;
						indoorFlag = true;
					} else {
						LOGGER_WARN(logger) << "GENERAL FAILURE: heatmap "
								       "IndoorOutdoorStr = "
								    << _heatmapIndoorOutdoorStr
								    << " illegal value";
						_responseCode = CConst::generalFailureResponseCode;
						return;
					}

					_heatmapAnalysisStr =
						parametersObj["analysis"].toString().toStdString();

					bool itonFlag = false;
					if (_heatmapAnalysisStr == "iton") {
						itonFlag = true;
					} else if (_heatmapAnalysisStr == "availability") {
						itonFlag = false;
					} else {
						LOGGER_WARN(logger)
							<< "GENERAL FAILURE: heatmap analysis = "
							<< _heatmapAnalysisStr << " illegal value";
						_responseCode = CConst::generalFailureResponseCode;
						return;
					}

					if (indoorFlag) {
						if (!optionalParams.contains("RLANIndoorHeightTyp"
									     "e")) {
							_heatmapRLANIndoorHeightType =
								parametersObj["RLANIndoorHeightTyp"
									      "e"]
									.toString()
									.toStdString();
						} else {
							_missingParams << "RLANIndoorHeightType";
						}
						if (!optionalParams.contains("RLANIndoorHeight")) {
							_heatmapRLANIndoorHeight =
								parametersObj["RLANIndoorHeight"]
									.toDouble();
						} else {
							LOGGER_WARN(logger)
								<< "GENERAL FAILURE: heatmap "
								   "RLANIndoorHeight not specified";
							_responseCode =
								CConst::generalFailureResponseCode;
							return;
						}
						if (!optionalParams.contains("RLANIndoorHeightUncer"
									     "tainty")) {
							_heatmapRLANIndoorHeightUncertainty =
								parametersObj["RLANIndoorHeightUnce"
									      "rtainty"]
									.toDouble();
						} else {
							LOGGER_WARN(logger)
								<< "GENERAL FAILURE: heatmap "
								   "RLANIndoorHeightUncertainty "
								   "not specified";
							_responseCode =
								CConst::generalFailureResponseCode;
							return;
						}

						if (itonFlag) {
							if (!optionalParams.contains("RLANIndoorEIR"
										     "PDBm")) {
								_heatmapRLANIndoorEIRPDBm =
									parametersObj["RLANIndoorEI"
										      "RPDBm"]
										.toDouble();
							} else {
								LOGGER_WARN(logger)
									<< "GENERAL FAILURE: "
									   "heatmap "
									   "RLANIndoorEIRPDBm not "
									   "specified";
								_responseCode = CConst::
									generalFailureResponseCode;
								return;
							}
						}
					}

					if (outdoorFlag) {
						if (!optionalParams.contains("RLANOutdoorHeightTyp"
									     "e")) {
							_heatmapRLANOutdoorHeightType =
								parametersObj["RLANOutdoorHeightTyp"
									      "e"]
									.toString()
									.toStdString();
						} else {
							LOGGER_WARN(logger)
								<< "GENERAL FAILURE: heatmap "
								   "RLANOutdoorHeightType not "
								   "specified";
							_responseCode =
								CConst::generalFailureResponseCode;
							return;
						}
						if (!optionalParams.contains("RLANOutdoorHeight")) {
							_heatmapRLANOutdoorHeight =
								parametersObj["RLANOutdoorHeight"]
									.toDouble();
						} else {
							LOGGER_WARN(logger)
								<< "GENERAL FAILURE: heatmap "
								   "RLANOutdoorHeight not "
								   "specified";
							_responseCode =
								CConst::generalFailureResponseCode;
							return;
						}
						if (!optionalParams.contains("RLANOutdoorHeightUnce"
									     "rtainty")) {
							_heatmapRLANOutdoorHeightUncertainty =
								parametersObj["RLANOutdoorHeightUnc"
									      "ertainty"]
									.toDouble();
						} else {
							LOGGER_WARN(logger)
								<< "GENERAL FAILURE: heatmap "
								   "RLANOutdoorHeightUncertainty "
								   "not specified";
							_responseCode =
								CConst::generalFailureResponseCode;
							return;
						}

						if (itonFlag) {
							if (!optionalParams.contains("RLANOutdoorEI"
										     "RPDBm")) {
								_heatmapRLANOutdoorEIRPDBm =
									parametersObj["RLANOutdoorE"
										      "IRPDBm"]
										.toDouble();
							} else {
								LOGGER_WARN(logger)
									<< "GENERAL FAILURE: "
									   "heatmap "
									   "RLANOutdoorEIRPDBm not "
									   "specified";
								_responseCode = CConst::
									generalFailureResponseCode;
								return;
							}
						}
					}

					std::string fsidTypeStr =
						parametersObj["fsIdType"].toString().toStdString();

					_heatmapFSID = -1;
					if (fsidTypeStr == "All") {
					} else if (fsidTypeStr == "Single") {
						if (!optionalParams.contains("fsId")) {
							_heatmapFSID =
								parametersObj["fsId"].toInt();
						} else {
							LOGGER_WARN(logger)
								<< "GENERAL FAILURE: heatmap fsId "
								   "not specified";
							_responseCode =
								CConst::generalFailureResponseCode;
							return;
						}
					} else {
						LOGGER_WARN(logger)
							<< "GENERAL FAILURE: heatmap fsIdType = "
							<< fsidTypeStr << " illegal value";
						_responseCode = CConst::generalFailureResponseCode;
						return;
					}

					auto inquiredChannelObj =
						parametersObj["inquiredChannel"].toObject();

					requiredParams = QStringList() << "globalOperatingClass"
								       << "channelCfi";
					optionalParams = QStringList();

					for (auto &key : inquiredChannelObj.keys()) {
						int rIdx = requiredParams.indexOf(key);
						if (rIdx != -1) {
							requiredParams.erase(
								requiredParams.begin() + rIdx);
						} else {
							int oIdx = optionalParams.indexOf(key);
							if (oIdx != -1) {
								optionalParams.erase(
									optionalParams.begin() +
									oIdx);
							} else {
								_unexpectedParams << key;
							}
						}
					}
					_missingParams << requiredParams;

					if (_missingParams.size()) {
						_responseCode = CConst::missingParamResponseCode;
						return;
					} else if (_unexpectedParams.size()) {
						_responseCode = CConst::unexpectedParamResponseCode;
						return;
					}

					_inquiredFrequencyRangesMHz.clear();
					_inquiredChannels.clear();

					auto chanClass = std::make_pair<int, std::vector<int>>(
						inquiredChannelObj["globalOperatingClass"].toInt(),
						std::vector<int>());
					chanClass.second.push_back(
						inquiredChannelObj["channelCfi"].toInt());

					_inquiredChannels.push_back(chanClass);

					_analysisType = "HeatmapAnalysis";

					_mapDataGeoJsonFile = "mapData.json.gz";
				}
			}

			std::string extensionId =
				vendorExtensionObj["extensionId"].toString().toStdString();
			if (extensionId == "openAfc.overrideAfcConfig") {
				_ulsDatabaseList.clear();
				std::string dbfile =
					SearchPaths::forReading(
						"data",
						parametersObj["fsDatabaseFile"].toString(),
						true)
						.toStdString();
				_ulsDatabaseList.push_back(std::make_tuple("FSDATA", dbfile));
			}
		}

		for (auto chanClass : _inquiredChannels) {
			LOGGER_INFO(logger)
				<< (chanClass.second.empty() ?
					    "ALL" :
					    std::to_string(chanClass.second.size()))
				<< " channels requested in operating class " << chanClass.first;
		}
		LOGGER_INFO(logger) << _inquiredChannels.size() << " operating class(es) requested";
		LOGGER_INFO(logger)
			<< _inquiredFrequencyRangesMHz.size() << " frequency range(s) requested";

		if (_inquiredChannels.size() + _inquiredFrequencyRangesMHz.size() == 0) {
			LOGGER_WARN(logger) << "GENERAL FAILURE: must specify either "
					       "inquiredChannels or inquiredFrequencies";
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
		throw std::runtime_error(QString("Invalid analysis type for version 1.1: %1")
						 .arg(QString::fromStdString(_analysisType))
						 .toStdString());
	}

	return;
}

// Support command line interface with AFC Engine
void AfcManager::setCmdLineParams(std::string &inputFilePath,
				  std::string &configFilePath,
				  std::string &outputFilePath,
				  std::string &tempDir,
				  std::string &logLevel,
				  int argc,
				  char **argv)
{
	// Declare the supported options
	po::options_description optDescript {"Allowed options"};
	// Create command line options
	optDescript.add_options()("help,h",
				  "Use input-file-path, config-file-path, or output-file-path.")(
		"request-type,r",
		po::value<std::string>()->default_value("AP-AFC"),
		"set request-type (AP-AFC, HeatmapAnalysis, ExclusionZoneAnalysis)")(
		"state-root,s",
		po::value<std::string>()->default_value("/var/lib/fbrat"),
		"set fbrat state root directory")("mnt-path,s",
						  po::value<std::string>()->default_value("/mnt/"
											  "nfs"),
						  "set share with GeoData and config data")(
		"input-file-path,i",
		po::value<std::string>()->default_value("inputFile.json"),
		"set input-file-path level")("config-file-path,c",
					     po::value<std::string>()->default_value("configFile."
										     "json"),
					     "set config-file-path level")(
		"output-file-path,o",
		po::value<std::string>()->default_value("outputFile.json"),
		"set output-file-path level")("temp-dir,t",
					      po::value<std::string>()->default_value(""),
					      "set temp-dir level")(
		"log-level,l",
		po::value<std::string>()->default_value("debug"),
		"set log-level")("runtime_opt,u",
				 po::value<uint32_t>()->default_value(3),
				 "bit 0: create 'fast' debug files; bit 1: create kmz and progress "
				 "files; bit 2: interpret file pathes as URLs; bit 4: create "
				 "'slow' debug files");

	po::variables_map cmdLineArgs;
	po::store(po::parse_command_line(argc, argv, optDescript),
		  cmdLineArgs); // ac and av are parameters passed into main
	po::notify(cmdLineArgs);

	// Check whether "help" argument was specified
	if (cmdLineArgs.count("help")) {
		std::cout << optDescript << std::endl;
		exit(0); // Terminate program, indicating it completed successfully
	}
	// Check whether "request-type(r)" was specified
	if (cmdLineArgs.count("request-type")) {
		_analysisType = cmdLineArgs["request-type"].as<std::string>();
	} else // Must be specified
	{
		throw std::runtime_error("AfcManager::setCmdLineParams(): request-type(r) command "
					 "line argument was not set.");
	}
	// Check whether "state-root(s)" was specified
	if (cmdLineArgs.count("state-root")) {
		_stateRoot = cmdLineArgs["state-root"].as<std::string>();
	} else // Must be specified
	{
		throw std::runtime_error("AfcManager::setCmdLineParams(): state-root(s) command "
					 "line argument was not set.");
	}
	// Check whether "mnt-path(s)" was specified
	if (cmdLineArgs.count("mnt-path")) {
		_mntPath = cmdLineArgs["mnt-path"].as<std::string>();
	} else // Must be specified
	{
		throw std::runtime_error("AfcManager::setCmdLineParams(): mnt-path(s) command line "
					 "argument was not set.");
	}
	// Check whether "input-file-path(i)" was specified
	if (cmdLineArgs.count("input-file-path")) {
		inputFilePath = cmdLineArgs["input-file-path"].as<std::string>();
	} else {
		throw std::runtime_error("AfcManager::setCmdLineParams(): input-file-path(i) "
					 "command line argument was not set.");
	}
	// Check whether "config-file-path(c)" was specified
	if (cmdLineArgs.count("config-file-path")) { // This is a work in progress
		configFilePath = cmdLineArgs["config-file-path"].as<std::string>();
	} else {
		throw std::runtime_error("AfcManager::setCmdLineParams(): config-file-path(c) "
					 "command line argument was not set.");
	}
	// Check whether "output-file-path(o)" was specified
	if (cmdLineArgs.count("output-file-path")) {
		outputFilePath = cmdLineArgs["output-file-path"].as<std::string>();
	} else {
		throw std::runtime_error("AfcManager::setCmdLineParams(): output-file-path(o) "
					 "command line argument was not set.");
	}
	// Check whether "temp-dir(t)" was specified
	if (cmdLineArgs.count("temp-dir")) {
		tempDir = cmdLineArgs["temp-dir"].as<std::string>();
	} else {
		throw std::runtime_error("AfcManager::setCmdLineParams(): temp-dir command line "
					 "argument was not set.");
	}
	// Check whether "log-level(l)" was specified
	if (cmdLineArgs.count("log-level")) {
		logLevel = cmdLineArgs["log-level"].as<std::string>();
	} else {
		throw std::runtime_error("AfcManager::setCmdLineParams(): log-level command line "
					 "argument was not set.");
	}
	if (cmdLineArgs.count("runtime_opt")) {
		uint32_t tmp = cmdLineArgs["runtime_opt"].as<uint32_t>();
		if (tmp & RUNTIME_OPT_ENABLE_DBG) {
			AfcManager::_createDebugFiles = true;
		}
		if (tmp & RUNTIME_OPT_ENABLE_GUI) {
			AfcManager::_createKmz = true;
		}

		if (tmp & RUNTIME_OPT_CERT_ID) {
			AfcManager::_certifiedIndoor = true;
		}
		AfcManager::_dataIf = new AfcDataIf(tmp & RUNTIME_OPT_URL);
		if (tmp & RUNTIME_OPT_ENABLE_SLOW_DBG) {
			AfcManager::_createSlowDebugFiles = true;
		}
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
		throw std::runtime_error("AfcManager::importConfigAFCjson(): Failed to open JSON "
					 "file specifying configuration parameters.");
	}

	// Print raw contents of input JSON file
	LOGGER_DEBUG(logger) << "Raw contents of input JSON file provided by the GUI: "
			     << jsonDoc.toJson().toStdString();

	// Read JSON from file
	QJsonObject jsonObj = jsonDoc.object();

	/**********************************************************************/
	/* Setup Paths                                                        */
	/**********************************************************************/
	if (jsonObj.contains("globeDir") && !jsonObj["globeDir"].isUndefined()) {
		_globeDir = SearchPaths::forReading("data", jsonObj["globeDir"].toString(), true)
				    .toStdString();
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): globeDir is missing.");
	}

	if (jsonObj.contains("srtmDir") && !jsonObj["srtmDir"].isUndefined()) {
		_srtmDir = SearchPaths::forReading("data", jsonObj["srtmDir"].toString(), true)
				   .toStdString();
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): srtmDir is missing.");
	}

	if (jsonObj.contains("cdsmDir") && !jsonObj["cdsmDir"].isUndefined()) {
		if (jsonObj["cdsmDir"].toString().isEmpty()) {
			_cdsmDir = "";
		} else {
			_cdsmDir = SearchPaths::forReading("data",
							   jsonObj["cdsmDir"].toString(),
							   true)
					   .toStdString();
		}
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): cdsmDir is missing.");
	}

	if (jsonObj.contains("depDir") && !jsonObj["depDir"].isUndefined()) {
		_depDir = SearchPaths::forReading("data", jsonObj["depDir"].toString(), true)
				  .toStdString();
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): depDir is missing.");
	}

	if (jsonObj.contains("lidarDir") && !jsonObj["lidarDir"].isUndefined()) {
		if (jsonObj["lidarDir"].toString().isEmpty()) {
			_lidarDir = "";
		} else {
			_lidarDir = SearchPaths::forReading("data",
							    jsonObj["lidarDir"].toString(),
							    true)
					    .toStdString();
		}
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): lidarDir is missing.");
	}

	if (jsonObj.contains("regionDir") && !jsonObj["regionDir"].isUndefined()) {
		_regionDir = jsonObj["regionDir"].toString().toStdString();
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): regionDir is "
					 "missing.");
	}

	if (jsonObj.contains("worldPopulationFile") &&
	    !jsonObj["worldPopulationFile"].isUndefined()) {
		_worldPopulationFile =
			SearchPaths::forReading("data",
						jsonObj["worldPopulationFile"].toString(),
						true)
				.toStdString();
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): worldPopulationFile "
					 "is missing.");
	}

	if (jsonObj.contains("nlcdFile") && !jsonObj["nlcdFile"].isUndefined()) {
		// Name part may contain template, it should not be searched for
		auto nlcdFileInfo = QFileInfo(jsonObj["nlcdFile"].toString());
		auto nlcdDir = SearchPaths::forReading("data", nlcdFileInfo.dir().path(), true);
		_nlcdFile = QDir::cleanPath(nlcdDir + "/" + nlcdFileInfo.fileName()).toStdString();
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): nlcdFile is missing.");
	}

	if (jsonObj.contains("rainForestFile") && !jsonObj["rainForestFile"].isUndefined()) {
		if (jsonObj["rainForestFile"].toString().isEmpty()) {
			_rainForestFile = "";
		} else {
			_rainForestFile =
				SearchPaths::forReading("data",
							jsonObj["rainForestFile"].toString(),
							true)
					.toStdString();
		}
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): rainForestFile is "
					 "missing.");
	}

	if (jsonObj.contains("nfaTableFile") && !jsonObj["nfaTableFile"].isUndefined()) {
		if (jsonObj["nfaTableFile"].toString().isEmpty()) {
			_nfaTableFile = "";
		} else {
			_nfaTableFile = SearchPaths::forReading("data",
								jsonObj["nfaTableFile"].toString(),
								true)
						.toStdString();
		}
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): nfaTableFile is "
					 "missing.");
	}

	if (jsonObj.contains("prTableFile") && !jsonObj["prTableFile"].isUndefined()) {
		if (jsonObj["prTableFile"].toString().isEmpty()) {
			_prTableFile = "";
		} else {
			_prTableFile = SearchPaths::forReading("data",
							       jsonObj["prTableFile"].toString(),
							       true)
					       .toStdString();
		}
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): prTableFile is "
					 "missing.");
	}

	if (jsonObj.contains("radioClimateFile") && !jsonObj["radioClimateFile"].isUndefined()) {
		_radioClimateFile = SearchPaths::forReading("data",
							    jsonObj["radioClimateFile"].toString(),
							    true)
					    .toStdString();
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): radioClimateFile is "
					 "missing.");
	}

	if (jsonObj.contains("surfRefracFile") && !jsonObj["surfRefracFile"].isUndefined()) {
		_surfRefracFile = SearchPaths::forReading("data",
							  jsonObj["surfRefracFile"].toString(),
							  true)
					  .toStdString();
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): surfRefracFile is "
					 "missing.");
	}

	if (jsonObj.contains("fsDatabaseFileList") &&
	    !jsonObj["fsDatabaseFileList"].isUndefined()) {
		QJsonArray fsDatabaseFileArray = jsonObj["fsDatabaseFileList"].toArray();
		for (QJsonValue fsDatabaseFileVal : fsDatabaseFileArray) {
			QJsonObject fsDatabaseFileObj = fsDatabaseFileVal.toObject();
			std::string name = fsDatabaseFileObj["name"].toString().toStdString();
			std::string dbfile = SearchPaths::forReading(
						     "data",
						     fsDatabaseFileObj["fsDatabaseFile"].toString(),
						     true)
						     .toStdString();
			_ulsDatabaseList.push_back(std::make_tuple(name, dbfile));
		}
	} else if (jsonObj.contains("fsDatabaseFile") && !jsonObj["fsDatabaseFile"].isUndefined()) {
		std::string dbfile = SearchPaths::forReading("data",
							     jsonObj["fsDatabaseFile"].toString(),
							     true)
					     .toStdString();
		_ulsDatabaseList.push_back(std::make_tuple("FSDATA", dbfile));
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): fsDatabaseFile not "
					 "specified.");
	}

	/**********************************************************************/

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
			errMsg = QString("ERROR: Freq Band %1 Invalid, startFreqMHz = %2, "
					 "stopFreqMHz = %3.  Require startFreqMHz < stopFreqMHz")
					 .arg(QString::fromStdString(name))
					 .arg(startFreqMHz)
					 .arg(stopFreqMHz);
			throw std::runtime_error(errMsg.toStdString());
		} else if ((stopFreqMHz <= _wlanMinFreqMHz) || (startFreqMHz >= _wlanMaxFreqMHz)) {
			errMsg = QString("ERROR: Freq Band %1 Invalid, startFreqMHz = %2, "
					 "stopFreqMHz = %3.  Has no overlap with band [%4,%5].")
					 .arg(QString::fromStdString(name))
					 .arg(startFreqMHz)
					 .arg(stopFreqMHz)
					 .arg(_wlanMinFreqMHz)
					 .arg(_wlanMaxFreqMHz);
			throw std::runtime_error(errMsg.toStdString());
		}

		_allowableFreqBandList.push_back(FreqBandClass(name, startFreqMHz, stopFreqMHz));
	}
	if (!_allowableFreqBandList.size()) {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): ERROR: no allowable "
					 "frequency bands defined.");
	}
	/**********************************************************************/

	// These are created for ease of copying
	QJsonObject antenna = jsonObj["antenna"].toObject();
	QJsonObject buildingLoss = jsonObj["buildingPenetrationLoss"].toObject();
	QJsonObject propModel = jsonObj["propagationModel"].toObject();

	// Input parameters stored in the AfcManager object
	if (jsonObj.contains("regionStr") && !jsonObj["regionStr"].isUndefined()) {
		_regionStr = jsonObj["regionStr"].toString().toStdString();
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): regionStr is "
					 "missing.");
	}

	_regionPolygonFileList = SearchPaths::forReading("data",
							 QString::fromStdString(_regionDir + "/" +
										_regionStr) +
								 ".kml",
							 true)
					 .toStdString();

	if (jsonObj.contains("cdsmLOSThr") && !jsonObj["cdsmLOSThr"].isUndefined()) {
		_cdsmLOSThr = jsonObj["cdsmLOSThr"].toDouble();
	} else {
		_cdsmLOSThr = 0.5;
	}

	if (jsonObj.contains("fsAnalysisListFile") &&
	    !jsonObj["fsAnalysisListFile"].isUndefined()) {
		_fsAnalysisListFile = QDir(QString::fromStdString(tempDir))
					      .filePath(jsonObj["fsAnalysisListFile"].toString())
					      .toStdString();
	} else {
		_fsAnalysisListFile = QDir(QString::fromStdString(tempDir))
					      .filePath("fs_analysis_list.csv")
					      .toStdString();
	}

	// ***********************************
	// If this flag is set, indoor rlan's have a fixed AMSL height over the uncertainty region
	// (with no height uncertainty). By default, this flag is false in which case the AGL height
	// is fixed over the uncertainty region (with no height uncertainty).
	// ***********************************
	if (jsonObj.contains("indoorFixedHeightAMSL") &&
	    !jsonObj["indoorFixedHeightAMSL"].isUndefined()) {
		_indoorFixedHeightAMSL = jsonObj["indoorFixedHeightAMSL"].toBool();
	} else {
		_indoorFixedHeightAMSL = false;
	}

	// ***********************************
	//
	// ***********************************
	if (jsonObj.contains("scanPointBelowGroundMethod") &&
	    !jsonObj["scanPointBelowGroundMethod"].isUndefined()) {
		std::string scanPointBelowGroundMethodStr =
			jsonObj["scanPointBelowGroundMethod"].toString().toStdString();
		if (scanPointBelowGroundMethodStr == "discard") {
			_scanPointBelowGroundMethod = CConst::DiscardScanPointBelowGroundMethod;
		} else if (scanPointBelowGroundMethodStr == "truncate") {
			_scanPointBelowGroundMethod = CConst::TruncateScanPointBelowGroundMethod;
		} else {
			throw std::runtime_error("AfcManager::importConfigAFCjson(): Invalid "
						 "scanPointBelowGroundMethod specified.");
		}
	} else {
		_scanPointBelowGroundMethod = CConst::TruncateScanPointBelowGroundMethod;
	}

	// ITM Parameters
	_itmEpsDielect = jsonObj["ITMParameters"].toObject()["dielectricConst"].toDouble();
	_itmSgmConductivity = jsonObj["ITMParameters"].toObject()["conductivity"].toDouble();
	_itmPolarization =
		jsonObj["ITMParameters"].toObject()["polarization"].toString().toStdString() ==
		"Vertical";

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
			throw std::runtime_error("AfcManager::importConfigAFCjson(): Invalid "
						 "scanPointBelowGroundMethod specified.");
		}
	} else {
		_scanRegionMethod = CConst::latLonAlignGridScanRegionMethod;
	}

	// AP uncertainty scanning resolution
	_scanres_xy = -1.0;
	_scanres_points_per_degree = -1;
	switch (_scanRegionMethod) {
		case CConst::xyAlignRegionNorthEastScanRegionMethod:
		case CConst::xyAlignRegionMajorMinorScanRegionMethod:
			if (uncertaintyObj.contains("horizontal") &&
			    !uncertaintyObj["horizontal"].isUndefined()) {
				_scanres_xy = uncertaintyObj["horizontal"].toDouble();
			} else {
				_scanres_xy = 30.0;
			}
			break;
		case CConst::latLonAlignGridScanRegionMethod:
			if (uncertaintyObj.contains("points_per_degree") &&
			    !uncertaintyObj["points_per_degree"].isUndefined()) {
				_scanres_points_per_degree =
					uncertaintyObj["points_per_degree"].toInt();
			} else {
				_scanres_points_per_degree = 3600; // Default 1 arcsec
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

	if (uncertaintyObj.contains("maxVerticalUncertainty") &&
	    !uncertaintyObj["maxVerticalUncertainty"].isUndefined()) {
		_maxVerticalUncertainty = uncertaintyObj["maxVerticalUncertainty"].toDouble();
	} else {
		_maxVerticalUncertainty = 50.0;
	}

	if (uncertaintyObj.contains("maxHorizontalUncertaintyDistance") &&
	    !uncertaintyObj["maxHorizontalUncertaintyDistance"].isUndefined()) {
		_maxHorizontalUncertaintyDistance =
			uncertaintyObj["maxHorizontalUncertaintyDistance"].toDouble();
	} else {
		_maxHorizontalUncertaintyDistance = 650.0;
	}

	if (jsonObj.contains("minEIRPIndoor") && !jsonObj["minEIRPIndoor"].isUndefined()) {
		_minEIRPIndoor_dBm = jsonObj["minEIRPIndoor"].toDouble();
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): minEIRPIndoor is "
					 "missing.");
	}

	if (jsonObj.contains("minEIRPOutdoor") && !jsonObj["minEIRPOutdoor"].isUndefined()) {
		_minEIRPOutdoor_dBm = jsonObj["minEIRPOutdoor"].toDouble();
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): minEIRPOutdoor is "
					 "missing.");
	}

	_maxEIRP_dBm = jsonObj["maxEIRP"].toDouble();

	if (jsonObj.contains("minPSD") && !jsonObj["minPSD"].isUndefined()) {
		_minPSD_dBmPerMHz = jsonObj["minPSD"].toDouble();
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): minPSD is missing.");
	}

	bool reportUnavailableSpectrumFlag;
	if (jsonObj.contains("reportUnavailableSpectrum") &&
	    !jsonObj["reportUnavailableSpectrum"].isUndefined()) {
		reportUnavailableSpectrumFlag = jsonObj["reportUnavailableSpectrum"].toBool();
	} else {
		reportUnavailableSpectrumFlag = false;
	}

	if (reportUnavailableSpectrumFlag) {
		if (jsonObj.contains("reportUnavailPSDdBPerMHz") &&
		    !jsonObj["reportUnavailPSDdBPerMHz"].isUndefined()) {
			_reportUnavailPSDdBmPerMHz = jsonObj["reportUnavailPSDdBPerMHz"].toDouble();
		} else {
			throw std::runtime_error("AfcManager::importConfigAFCjson(): "
						 "reportUnavailableSpectrum set but "
						 "reportUnavailPSDdBPerMHz is missing.");
		}
	} else {
		_reportUnavailPSDdBmPerMHz = quietNaN;
	}

	if (jsonObj.contains("inquiredFrequencyMaxPSD_dBmPerMHz") &&
	    !jsonObj["inquiredFrequencyMaxPSD_dBmPerMHz"].isUndefined()) {
		_inquiredFrequencyMaxPSD_dBmPerMHz =
			jsonObj["inquiredFrequencyMaxPSD_dBmPerMHz"].toInt();
	} else {
		_inquiredFrequencyMaxPSD_dBmPerMHz =
			_maxEIRP_dBm - 10.0 * log10(20.0); // Default maxEIRP over 20 MHz
	}

	_IoverN_threshold_dB = jsonObj["threshold"].toDouble();
	_maxRadius = jsonObj["maxLinkDistance"].toDouble() * 1000.0; // convert from km to m
	_bodyLossIndoorDB = jsonObj["bodyLoss"].toObject()["valueIndoor"].toDouble();
	_bodyLossOutdoorDB = jsonObj["bodyLoss"].toObject()["valueOutdoor"].toDouble();
	_polarizationLossDB = jsonObj["polarizationMismatchLoss"].toObject()["value"].toDouble();
	_buildingLossModel = (buildingLoss["kind"]).toString();

	int validFlag;
	if (jsonObj.contains("propagationEnv") && !jsonObj["propagationEnv"].isUndefined()) {
		_propEnvMethod = (CConst::PropEnvMethodEnum)
					 CConst::strPropEnvMethodList->str_to_type(
						 jsonObj["propagationEnv"].toString().toStdString(),
						 validFlag,
						 0);
		if (!validFlag) {
			throw std::runtime_error("ERROR: Invalid propagationEnv");
		}
	} else {
		_propEnvMethod = CConst::nlcdPointPropEnvMethod;
	}

	if (jsonObj.contains("densityThrUrban") && !jsonObj["densityThrUrban"].isUndefined()) {
		_densityThrUrban = jsonObj["densityThrUrban"].toDouble();
	} else {
		_densityThrUrban = 486.75e-6;
	}

	if (jsonObj.contains("densityThrSuburban") &&
	    !jsonObj["densityThrSuburban"].isUndefined()) {
		_densityThrSuburban = jsonObj["densityThrSuburban"].toDouble();
	} else {
		_densityThrSuburban = 211.205e-6;
	}

	if (jsonObj.contains("densityThrRural") && !jsonObj["densityThrRural"].isUndefined()) {
		_densityThrRural = jsonObj["densityThrRural"].toDouble();
	} else {
		_densityThrRural = 57.1965e-6;
	}
	// ***********************************

	// ***********************************
	// Feeder loss parameters
	// ***********************************
	QJsonObject receiverFeederLossObj = jsonObj["receiverFeederLoss"].toObject();

	if (receiverFeederLossObj.contains("IDU") && !receiverFeederLossObj["IDU"].isUndefined()) {
		_rxFeederLossDBIDU = receiverFeederLossObj["IDU"].toDouble();
	} else {
		_rxFeederLossDBIDU = 3.0;
	}
	if (receiverFeederLossObj.contains("ODU") && !receiverFeederLossObj["ODU"].isUndefined()) {
		_rxFeederLossDBODU = receiverFeederLossObj["ODU"].toDouble();
	} else {
		_rxFeederLossDBODU = 0.0;
	}
	if (receiverFeederLossObj.contains("UNKNOWN") &&
	    !receiverFeederLossObj["UNKNOWN"].isUndefined()) {
		_rxFeederLossDBUnknown = receiverFeederLossObj["UNKNOWN"].toDouble();
	} else {
		_rxFeederLossDBUnknown = 0.0;
	}
	// ***********************************

	// ***********************************
	// Noise Figure parameters
	// ***********************************
	auto ulsRecieverNoise = jsonObj["fsReceiverNoise"].toObject();

	QJsonArray freqList = ulsRecieverNoise["freqList"].toArray();
	for (QJsonValue freqVal : freqList) {
		double freqValHz = freqVal.toDouble() * 1.0e6; // Convert MHz to Hz
		_noisePSDFreqList.push_back(freqValHz);
	}

	QJsonArray noisePSDList = ulsRecieverNoise["noiseFloorList"].toArray();
	for (QJsonValue noisePSDVal : noisePSDList) {
		double noisePSDValdBWPerHz = noisePSDVal.toDouble() -
					     90.0; // Convert dbM/MHz to dBW/Hz
		_noisePSDList.push_back(noisePSDValdBWPerHz);
	}

	if (_noisePSDList.size() != _noisePSDFreqList.size() + 1) {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): Invalid "
					 "fsReceiverNoise specification.");
	}
	for (int i = 1; i < (int)_noisePSDFreqList.size(); ++i) {
		if (!(_noisePSDFreqList[i] > _noisePSDFreqList[i - 1])) {
			throw std::runtime_error("AfcManager::importConfigAFCjson(): "
						 "fsReceiverNoise freqList not monotonically "
						 "increasing.");
		}
	}

	// ***********************************

	// ***********************************
	// apply clutter bool
	// ***********************************
	if (jsonObj.contains("clutterAtFS") && !jsonObj["clutterAtFS"].isUndefined()) {
		_applyClutterFSRxFlag = jsonObj["clutterAtFS"].toBool();
	} else {
		_applyClutterFSRxFlag = false;
	}

	if (jsonObj.contains("allowRuralFSClutter") &&
	    !jsonObj["allowRuralFSClutter"].isUndefined()) {
		_allowRuralFSClutterFlag = jsonObj["allowRuralFSClutter"].toBool();
	} else {
		_allowRuralFSClutterFlag = false;
	}

	if (jsonObj.contains("fsClutterModel") && !jsonObj["fsClutterModel"].isUndefined()) {
		QJsonObject fsClutterModel = jsonObj["fsClutterModel"].toObject();
		if (fsClutterModel.contains("p2108Confidence") &&
		    !fsClutterModel["p2108Confidence"].isUndefined()) {
			_fsConfidenceClutter2108 = fsClutterModel["p2108Confidence"].toDouble() /
						   100.0;
		} else {
			throw std::runtime_error("AfcManager::importConfigAFCjson(): "
						 "fsClutterModel[p2108Confidence] missing.");
		}
		if (fsClutterModel.contains("maxFsAglHeight") &&
		    !fsClutterModel["maxFsAglHeight"].isUndefined()) {
			_maxFsAglHeight = fsClutterModel["maxFsAglHeight"].toDouble();
		} else {
			throw std::runtime_error("AfcManager::importConfigAFCjson(): "
						 "fsClutterModel[maxFsAglHeight] missing.");
		}
	} else {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): fsClutterModel "
					 "missing.");
	}

	if (jsonObj.contains("rlanITMTxClutterMethod") &&
	    !jsonObj["rlanITMTxClutterMethod"].isUndefined()) {
		_rlanITMTxClutterMethod =
			(CConst::ITMClutterMethodEnum)CConst::strITMClutterMethodList->str_to_type(
				jsonObj["rlanITMTxClutterMethod"].toString().toStdString(),
				validFlag,
				0);
		if (!validFlag) {
			throw std::runtime_error("ERROR: Invalid rlanITMTxClutterMethod");
		}
	} else {
		_rlanITMTxClutterMethod = CConst::ForceTrueITMClutterMethod;
	}
	// ***********************************

	if (jsonObj.contains("ulsDefaultAntennaType") &&
	    !jsonObj["ulsDefaultAntennaType"].isUndefined()) {
		_ulsDefaultAntennaType =
			(CConst::ULSAntennaTypeEnum)CConst::strULSAntennaTypeList->str_to_type(
				jsonObj["ulsDefaultAntennaType"].toString().toStdString(),
				validFlag,
				0);
		if (!validFlag) {
			throw std::runtime_error("ERROR: Invalid ulsDefaultAntennaType");
		}
	}

	// ***********************************
	// As of Feb-2022, it was agreed to include all possible Mobile links (e.g. those marked as
	// Mobile or TP radio service) in UNII-5 and UNII-7 for AFC analysis.
	// ***********************************
	if (jsonObj.contains("removeMobile") && !jsonObj["removeMobile"].isUndefined()) {
		_removeMobile = jsonObj["removeMobile"].toBool();
	} else {
		_removeMobile = false;
	}

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
		_buildingType = CConst::noBuildingType;
		_confidenceBldg2109 = 0.0;
	} else {
		throw std::runtime_error("ERROR: Invalid buildingLoss[\"kind\"]");
	}

	/**************************************************************************************/
	/* Path Loss Model Parmeters                                                       ****/
	/**************************************************************************************/
	_pathLossClampFSPL = false;

	// Set pathLossModel
	_pathLossModel = (CConst::PathLossModelEnum)CConst::strPathLossModelList->str_to_type(
		propModel["kind"].toString().toStdString(),
		validFlag,
		0);
	if (!validFlag) {
		throw std::runtime_error("ERROR: Invalid propagationModel[\"kind\"]");
	}

	if ((_pathLossModel == CConst::CustomPathLossModel) ||
	    (_pathLossModel == CConst::ISEDDBS06PathLossModel) ||
	    (_pathLossModel == CConst::BrazilPathLossModel) ||
	    (_pathLossModel == CConst::OfcomPathLossModel)) {
		_pathLossModel = CConst::FCC6GHzReportAndOrderPathLossModel;
	}

	switch (_pathLossModel) {
		case CConst::unknownPathLossModel:
			throw std::runtime_error("ERROR: Invalid propagationModel[\"kind\"]");
			break;
		case CConst::FSPLPathLossModel:
			// FSPL
			break;
		case CConst::ITMBldgPathLossModel:
			// ITM model using Building data as terrain
			// GUI gets these values as percentiles from 0-100, convert to probabilities
			// 0-1
			_winner2ProbLOSThr = propModel["win2ProbLosThreshold"].toDouble() / 100.0;
			_confidenceITM = propModel["itmConfidence"].toDouble() / 100.0;
			_confidenceClutter2108 = propModel["p2108Confidence"].toDouble() / 100.0;
			_useBDesignFlag = propModel["buildingSource"].toString() == "B-Design3D";
			_useLiDAR = propModel["buildingSource"].toString() == "LiDAR";
			_use3DEP = true;
			_winner2LOSOption = CConst::UnknownLOSOption;
			_winner2UnknownLOSMethod = CConst::PLOSThresholdWinner2UnknownLOSMethod;
			break;
		case CConst::CoalitionOpt6PathLossModel:
			// No buildings, Winner II, ITM, P.456 Clutter
			// GUI gets these values as percentiles from 0-100, convert to probabilities
			// 0-1
			_winner2ProbLOSThr = propModel["win2ProbLosThreshold"].toDouble() / 100.0;
			_confidenceITM = propModel["itmConfidence"].toDouble() / 100.0;
			_confidenceClutter2108 = propModel["p2108Confidence"].toDouble() / 100.0;

			_use3DEP = propModel["terrainSource"].toString() == "3DEP (30m)";

			_winner2LOSOption = CConst::UnknownLOSOption;
			_winner2UnknownLOSMethod = CConst::PLOSThresholdWinner2UnknownLOSMethod;
			break;
		case CConst::FCC6GHzReportAndOrderPathLossModel:
			// FCC_6GHZ_REPORT_AND_ORDER path loss model
			// GUI gets these values as percentiles from 0-100, convert to probabilities
			// 0-1
			_winner2ProbLOSThr = quietNaN;
			_confidenceITM = propModel["itmConfidence"].toDouble() / 100.0;
			_confidenceClutter2108 = propModel["p2108Confidence"].toDouble() / 100.0;

			_useBDesignFlag = propModel["buildingSource"].toString() == "B-Design3D";
			_useLiDAR = propModel["buildingSource"].toString() == "LiDAR";

			// In the GUI, force this to be true for US and CA.
			_use3DEP = propModel["terrainSource"].toString() == "3DEP (30m)";

			_winner2LOSOption = ((_useBDesignFlag || _useLiDAR) ?
						     CConst::BldgDataReqTxRxLOSOption :
					     (!_cdsmDir.empty()) ? CConst::CdsmLOSOption :
								   CConst::UnknownLOSOption);
			_winner2UnknownLOSMethod = CConst::PLOSCombineWinner2UnknownLOSMethod;

			_pathLossClampFSPL = true;
			break;
		default:
			CORE_DUMP;
			break;
	}

	if (_use3DEP) {
		if (_depDir.empty()) {
			throw std::runtime_error("AfcManager::importConfigAFCjson(): Specified "
						 "path loss model requires 3DEP data, but depDir "
						 "is empty.");
		}
	} else {
		if (!_depDir.empty()) {
			LOGGER_WARN(logger) << "3DEP data defined, but selected path loss model "
					       "doesn't use 3DEP data.  No 3DEP data will be used";
			_depDir = "";
		}
	}

	if (_useBDesignFlag || _useLiDAR) {
		if (_lidarDir.empty()) {
			throw std::runtime_error("AfcManager::importConfigAFCjson(): Specified "
						 "path loss model requires building data, but "
						 "lidarDir is empty.");
		}
	} else {
		if (!_lidarDir.empty()) {
			LOGGER_WARN(logger)
				<< "Building data defined, but selected path loss model doesn't "
				   "use building data.  No building data will be used";

			_lidarDir = "";
		}
	}

	if (propModel.contains("win2Confidence") && !propModel["win2Confidence"].isUndefined()) {
		throw std::runtime_error("AfcManager::importConfigAFCjson(): Outdated afc_config, "
					 "win2Confidence not supported parameter.");
	}

	if (propModel.contains("win2ConfidenceCombined") &&
	    !propModel["win2ConfidenceCombined"].isUndefined()) {
		_confidenceWinner2Combined = propModel["win2ConfidenceCombined"].toDouble() / 100.0;
	} else {
		_confidenceWinner2Combined = 0.5;
	}

	if (propModel.contains("win2ConfidenceLOS") &&
	    !propModel["win2ConfidenceLOS"].isUndefined()) {
		_confidenceWinner2LOS = propModel["win2ConfidenceLOS"].toDouble() / 100.0;
	} else {
		_confidenceWinner2LOS = 0.5;
	}

	if (propModel.contains("win2ConfidenceNLOS") &&
	    !propModel["win2ConfidenceNLOS"].isUndefined()) {
		_confidenceWinner2NLOS = propModel["win2ConfidenceNLOS"].toDouble() / 100.0;
	} else {
		_confidenceWinner2NLOS = 0.5;
	}

	if (propModel.contains("win2UseGroundDistance") &&
	    !propModel["win2UseGroundDistance"].isUndefined()) {
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

	if (propModel.contains("fsplUseGroundDistance") &&
	    !propModel["fsplUseGroundDistance"].isUndefined()) {
		_fsplUseGroundDistanceFlag = propModel["fsplUseGroundDistance"].toBool();
	} else {
		_fsplUseGroundDistanceFlag = false;
	}

	if (propModel.contains("itmReliability") && !propModel["itmReliability"].isUndefined()) {
		_reliabilityITM = propModel["itmReliability"].toDouble() / 100.0;
	} else {
		_reliabilityITM = 0.5;
	}

	if (propModel.contains("winner2LOSOption") &&
	    !propModel["winner2LOSOption"].isUndefined()) {
		_winner2LOSOption = (CConst::LOSOptionEnum)CConst::strLOSOptionList->str_to_type(
			propModel["winner2LOSOption"].toString().toStdString(),
			validFlag,
			1);
	}

	// ***********************************
	// If this flag is set, map data geojson is generated
	// ***********************************
	_mapDataGeoJsonFile = "";
	if (jsonObj.contains("enableMapInVirtualAp") &&
	    !jsonObj["enableMapInVirtualAp"].isUndefined() && AfcManager::_createKmz) {
		if (jsonObj["enableMapInVirtualAp"].toBool()) {
			_mapDataGeoJsonFile = "mapData.json.gz";
		}
	}

	if (jsonObj.contains("channelResponseAlgorithm") &&
	    !jsonObj["channelResponseAlgorithm"].isUndefined()) {
		std::string strval = jsonObj["channelResponseAlgorithm"].toString().toStdString();
		_channelResponseAlgorithm =
			(CConst::SpectralAlgorithmEnum)
				CConst::strSpectralAlgorithmList->str_to_type(strval, validFlag, 1);
	} else {
		_channelResponseAlgorithm = CConst::pwrSpectralAlgorithm;
	}

	if (jsonObj.contains("visibilityThreshold") &&
	    !jsonObj["visibilityThreshold"].isUndefined()) {
		_visibilityThreshold = jsonObj["visibilityThreshold"].toDouble();
	} else {
		_visibilityThreshold = -10000.0;
	}

	if (jsonObj.contains("printSkippedLinksFlag") &&
	    !jsonObj["printSkippedLinksFlag"].isUndefined()) {
		_printSkippedLinksFlag = jsonObj["printSkippedLinksFlag"].toBool();
	} else {
		_printSkippedLinksFlag = false;
	}

	if (jsonObj.contains("roundPSDEIRPFlag") && !jsonObj["roundPSDEIRPFlag"].isUndefined()) {
		_roundPSDEIRPFlag = jsonObj["roundPSDEIRPFlag"].toBool();
	} else {
		_roundPSDEIRPFlag = true;
	}

	if (jsonObj.contains("allowScanPtsInUncReg") &&
	    !jsonObj["allowScanPtsInUncReg"].isUndefined()) {
		_allowScanPtsInUncRegFlag = jsonObj["allowScanPtsInUncReg"].toBool();
	} else {
		_allowScanPtsInUncRegFlag = false;
	}

	// ***********************************
	// If this flag is set, FS RX antenna will have near field adjustment applied to antenna
	// gain
	// ***********************************
	if (jsonObj.contains("nearFieldAdjFlag") && !jsonObj["nearFieldAdjFlag"].isUndefined()) {
		_nearFieldAdjFlag = jsonObj["nearFieldAdjFlag"].toBool();
	} else {
		_nearFieldAdjFlag = true;
	}

	if (_nearFieldAdjFlag) {
		if (_nfaTableFile.empty()) {
			throw std::runtime_error("AfcManager::importConfigAFCjson(): "
						 "nearFieldAdjFlag set to true, but nfaTableFile "
						 "not specified.");
		}
	} else {
		if (!_nfaTableFile.empty()) {
			LOGGER_WARN(logger) << "nearFieldAdjFlag set to false, but nfaTableFile "
					       "has been specified.  Ignoring nfaTableFile.";
			_nfaTableFile = "";
		}
	}
	// ***********************************

	// ***********************************
	// If this flag is set, compute passive repeaters, otherwise ignore passive repeaters
	// ***********************************
	if (jsonObj.contains("passiveRepeaterFlag") &&
	    !jsonObj["passiveRepeaterFlag"].isUndefined()) {
		_passiveRepeaterFlag = jsonObj["passiveRepeaterFlag"].toBool();
	} else {
		_passiveRepeaterFlag = true;
	}

	if (_passiveRepeaterFlag) {
		if (_prTableFile.empty()) {
			throw std::runtime_error("AfcManager::importConfigAFCjson(): "
						 "passiveRepeaterFlag set to true, but prTableFile "
						 "not specified.");
		}
	} else {
		if (!_prTableFile.empty()) {
			LOGGER_WARN(logger) << "passiveRepeaterFlag set to false, but prTableFile "
					       "has been specified.  Ignoring prTableFile.";
			_prTableFile = "";
		}
	}
	// ***********************************

	// ***********************************
	// If this flag is set, report an error when all scan points are below
	// _minRlanHeightAboveTerrain
	// ***********************************
	if (jsonObj.contains("reportErrorRlanHeightLowFlag") &&
	    !jsonObj["reportErrorRlanHeightLowFlag"].isUndefined()) {
		_reportErrorRlanHeightLowFlag = jsonObj["reportErrorRlanHeightLowFlag"].toBool();
	} else {
		_reportErrorRlanHeightLowFlag = false;
	}
	// ***********************************

	// ***********************************
	// If this flag is set, include ACI in channel I/N calculations
	// ***********************************
	if (jsonObj.contains("aciFlag") && !jsonObj["aciFlag"].isUndefined()) {
		_aciFlag = jsonObj["aciFlag"].toBool();
	} else {
		_aciFlag = true;
	}
	// ***********************************

	if (jsonObj.contains("deniedRegionFile") && !jsonObj["deniedRegionFile"].isUndefined()) {
		_deniedRegionFile = SearchPaths::forReading("data",
							    jsonObj["deniedRegionFile"].toString(),
							    true)
					    .toStdString();
	}
}

QJsonArray generateStatusMessages(const std::vector<std::string> &messages)
{
	auto array = QJsonArray();
	for (auto &m : messages)
		array.append(QJsonValue(QString::fromStdString(m)));
	return array;
}

void AfcManager::addBuildingDatabaseTiles(OGRLayer *layer)
{
	// add building database raster boundary if loaded
	if (_terrainDataModel->getNumLidarRegion()) {
		LOGGER_DEBUG(logger) << "adding raster bounds";
		for (QRectF b : _terrainDataModel->getBounds()) {
			LOGGER_DEBUG(logger) << "adding tile";
			// Instantiate cone object
			std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> boxFeature(
				OGRFeature::CreateFeature(layer->GetLayerDefn()));

			// Intantiate unique-pointers to OGRPolygon and OGRLinearRing for storing
			// the beam coverage
			GdalHelpers::GeomUniquePtr<OGRPolygon> boundBox(
				GdalHelpers::createGeometry<
					OGRPolygon>()); // Use GdalHelpers.h templates to have
							// unique pointers create these on the heap
			GdalHelpers::GeomUniquePtr<OGRLinearRing> boxPoints(
				GdalHelpers::createGeometry<OGRLinearRing>());

			// Create OGRPoints to store the coordinates of the beam triangle
			// ***IMPORTANT NOTE: Coordinates stored as (Lon,Lat) here, required by
			// geoJSON specifications
			GdalHelpers::GeomUniquePtr<OGRPoint> topLeft(
				GdalHelpers::createGeometry<OGRPoint>());
			topLeft->setY(b.top());
			topLeft->setX(b.left()); // Must set points manually since cannot construct
						 // while pointing at with unique pointer
			GdalHelpers::GeomUniquePtr<OGRPoint> topRight(
				GdalHelpers::createGeometry<OGRPoint>());
			topRight->setY(b.top());
			topRight->setX(b.right());
			GdalHelpers::GeomUniquePtr<OGRPoint> botLeft(
				GdalHelpers::createGeometry<OGRPoint>());
			botLeft->setY(b.bottom());
			botLeft->setX(b.left());
			GdalHelpers::GeomUniquePtr<OGRPoint> botRight(
				GdalHelpers::createGeometry<OGRPoint>());
			botRight->setY(b.bottom());
			botRight->setX(b.right());

			// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
			boxPoints->addPoint(topLeft.get()); // Using .get() gives access to the
							    // pointer without giving up ownership
			boxPoints->addPoint(topRight.get());
			boxPoints->addPoint(botRight.get());
			boxPoints->addPoint(botLeft.get());
			boxPoints->addPoint(topLeft.get());
			boundBox->addRingDirectly(
				boxPoints.release()); // Points unique-pointer to null and gives up
						      // ownership of exteriorOfCone to beamCone

			// Add properties to the geoJSON features
			boxFeature->SetField("kind", "BLDB"); // BLDB = building bounds

			// Add geometry to feature
			boxFeature->SetGeometryDirectly(boundBox.release());

			if (layer->CreateFeature(boxFeature.release()) != OGRERR_NONE) {
				throw std::runtime_error("Could not add bound feature in layer of "
							 "output data source");
			}
		}
	}
}

void AfcManager::addDeniedRegions(OGRLayer *layer)
{
	// add denied regions
	LOGGER_DEBUG(logger) << "adding denied regions";
	int drIdx;
	for (drIdx = 0; drIdx < (int)_deniedRegionList.size(); ++drIdx) {
		DeniedRegionClass *dr = _deniedRegionList[drIdx];

		DeniedRegionClass::TypeEnum drType = dr->getType();

		std::string pfx;
		switch (drType) {
			case DeniedRegionClass::RASType:
				pfx = "RAS_";
				break;
			case DeniedRegionClass::userSpecifiedType:
				pfx = "USER_SPEC_";
				break;
			default:
				CORE_DUMP;
				break;
		}
		std::string name = pfx + std::to_string(dr->getID());

		LOGGER_DEBUG(logger) << "adding denied region " << name;

		double maxRLANHeightAGL;
		if (_analysisType == "HeatmapAnalysis") {
			maxRLANHeightAGL = _heatmapMaxRLANHeightAGL;
		} else {
			maxRLANHeightAGL = _rlanRegion->getMaxHeightAGL();
		}

		int numPtsCircle = 32;
		double rectLonStart, rectLonStop, rectLatStart, rectLatStop;
		double circleRadius, longitudeCenter, latitudeCenter;
		double drTerrainHeight, drBldgHeight, drHeightAGL;
		Vector3 drCenterPosn;
		Vector3 drUpVec;
		Vector3 drEastVec;
		Vector3 drNorthVec;
		QString dr_coords;
		MultibandRasterClass::HeightResult drLidarHeightResult;
		CConst::HeightSourceEnum drHeightSource;
		DeniedRegionClass::GeometryEnum drGeometry = dr->getGeometry();
		int numGeom;
		bool rectFlag;
		switch (drGeometry) {
			case DeniedRegionClass::rectGeometry:
			case DeniedRegionClass::rect2Geometry:
				numGeom = ((RectDeniedRegionClass *)dr)->getNumRect();
				rectFlag = true;
				break;
			case DeniedRegionClass::circleGeometry:
			case DeniedRegionClass::horizonDistGeometry:
				numGeom = 1;
				rectFlag = false;
				break;
			default:
				CORE_DUMP;
				break;
		}
		for (int geomIdx = 0; geomIdx < numGeom; ++geomIdx) {
			GdalHelpers::GeomUniquePtr<OGRLinearRing> ptList(
				GdalHelpers::createGeometry<OGRLinearRing>());
			if (rectFlag) {
				std::tie(rectLonStart, rectLonStop, rectLatStart, rectLatStop) =
					((RectDeniedRegionClass *)dr)->getRect(geomIdx);
				GdalHelpers::GeomUniquePtr<OGRPoint> topLeft(
					GdalHelpers::createGeometry<OGRPoint>());
				topLeft->setY(rectLatStop);
				topLeft->setX(rectLonStart);
				GdalHelpers::GeomUniquePtr<OGRPoint> topRight(
					GdalHelpers::createGeometry<OGRPoint>());
				topRight->setY(rectLatStop);
				topRight->setX(rectLonStop);
				GdalHelpers::GeomUniquePtr<OGRPoint> botLeft(
					GdalHelpers::createGeometry<OGRPoint>());
				botLeft->setY(rectLatStart);
				botLeft->setX(rectLonStart);
				GdalHelpers::GeomUniquePtr<OGRPoint> botRight(
					GdalHelpers::createGeometry<OGRPoint>());
				botRight->setY(rectLatStart);
				botRight->setX(rectLonStop);

				ptList->addPoint(
					topLeft.get()); // Using .get() gives access to the pointer
							// without giving up ownership
				ptList->addPoint(topRight.get());
				ptList->addPoint(botRight.get());
				ptList->addPoint(botLeft.get());
				ptList->addPoint(topLeft.get());
			} else {
				OGRPoint *ogrPtList[numPtsCircle];
				circleRadius = ((CircleDeniedRegionClass *)dr)
						       ->computeRadius(maxRLANHeightAGL);
				longitudeCenter =
					((CircleDeniedRegionClass *)dr)->getLongitudeCenter();
				latitudeCenter =
					((CircleDeniedRegionClass *)dr)->getLatitudeCenter();
				drHeightAGL = dr->getHeightAGL();
				_terrainDataModel->getTerrainHeight(longitudeCenter,
								    latitudeCenter,
								    drTerrainHeight,
								    drBldgHeight,
								    drLidarHeightResult,
								    drHeightSource);
				drCenterPosn = EcefModel::geodeticToEcef(
					latitudeCenter,
					longitudeCenter,
					(drTerrainHeight + drHeightAGL) / 1000.0);
				drUpVec = drCenterPosn.normalized();
				drEastVec = (Vector3(-drUpVec.y(), drUpVec.x(), 0.0)).normalized();
				drNorthVec = drUpVec.cross(drEastVec);
				for (int ptIdx = 0; ptIdx < numPtsCircle; ++ptIdx) {
					ogrPtList[ptIdx] = GdalHelpers::createGeometry<OGRPoint>();
					double phi = 2 * M_PI * ptIdx / numPtsCircle;
					Vector3 circlePtPosn = drCenterPosn +
							       (circleRadius / 1000) *
								       (drEastVec * cos(phi) +
									drNorthVec * sin(phi));

					GeodeticCoord circlePtPosnGeodetic =
						EcefModel::ecefToGeodetic(circlePtPosn);

					ogrPtList[ptIdx]->setX(circlePtPosnGeodetic.longitudeDeg);
					ogrPtList[ptIdx]->setY(circlePtPosnGeodetic.latitudeDeg);

					ptList->addPoint(ogrPtList[ptIdx]);
				}
				ptList->addPoint(ogrPtList[0]);
			}

			// Intantiate unique-pointers to OGRPolygon and OGRLinearRing for denied
			// region
			GdalHelpers::GeomUniquePtr<OGRPolygon> boundBox(
				GdalHelpers::createGeometry<OGRPolygon>());
			// Use GdalHelpers.h templates to have unique pointers create these on the
			// heap

			// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
			boundBox->addRingDirectly(ptList.release());

			// Add properties to the geoJSON features
			std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> regionFeature(
				OGRFeature::CreateFeature(layer->GetLayerDefn()));
			regionFeature->SetField("kind", "DR"); // DR = denied region

			// Add geometry to feature
			regionFeature->SetGeometryDirectly(boundBox.release());

			if (layer->CreateFeature(regionFeature.release()) != OGRERR_NONE) {
				throw std::runtime_error("Could not add denied region feature in "
							 "layer of output data source");
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
	for (auto &freqRange : psdFreqRangeList) {
		for (int i = 0; i < (int)freqRange.psd_dBm_MHzList.size(); i++) {
			if (!std::isnan(freqRange.psd_dBm_MHzList.at(i))) {
				spectrumInfos.append(QJsonObject {
					{"frequencyRange",
					 QJsonObject {{"lowFrequency", freqRange.freqMHzList.at(i)},
						      {"highFrequency",
						       freqRange.freqMHzList.at(i + 1)}}},
					{"maxPsd", freqRange.psd_dBm_MHzList.at(i)}});
			}
		}
	}

	QJsonArray channelInfos = QJsonArray();
	QJsonArray blackChannelInfos = QJsonArray();
	QJsonArray redChannelInfos = QJsonArray();
	if (_responseCode == CConst::successResponseCode) {
		for (const auto &group : _inquiredChannels) {
			auto operatingClass = group.first;
			auto indexArray = QJsonArray();
			auto blackIndexArray = QJsonArray();
			auto redIndexArray = QJsonArray();
			auto eirpArray = QJsonArray();

			for (const auto &chan : _channelList) {
				if ((chan.type == ChannelType::INQUIRED_CHANNEL) &&
				    (chan.operatingClass == operatingClass)) {
					ChannelColor chanColor = std::get<2>(chan.segList[0]);
					if (chanColor == BLACK) {
						blackIndexArray.append(chan.index);
					} else if (chanColor == RED) {
						redIndexArray.append(chan.index);
					} else {
						indexArray.append(chan.index);
						double eirpVal =
							std::min(std::get<0>(chan.segList[0]),
								 std::get<1>(chan.segList[0]));
						if (_roundPSDEIRPFlag) {
							// EIRP value rounded down to nearest
							// multiple of 0.1 dB
							eirpVal = std::floor(eirpVal * 10) / 10.0;
						}
						eirpArray.append(eirpVal);
					}
				}
			}

			auto channelGroup = QJsonObject {{"globalOperatingClass",
							  QJsonValue(operatingClass)},
							 {"channelCfi", indexArray},
							 {"maxEirp", eirpArray}};
			auto blackChannelGroup = QJsonObject {{"globalOperatingClass",
							       QJsonValue(operatingClass)},
							      {"channelCfi", blackIndexArray}};
			auto redChannelGroup = QJsonObject {{"globalOperatingClass",
							     QJsonValue(operatingClass)},
							    {"channelCfi", redIndexArray}};
			channelInfos.append(channelGroup);
			blackChannelInfos.append(blackChannelGroup);
			redChannelInfos.append(redChannelGroup);
		}
	}

	std::string shortDescription;

	switch (_responseCode) {
		case CConst::generalFailureResponseCode:
			shortDescription = "General Failure";
			break;
		case CConst::successResponseCode:
			shortDescription = "Success";
			break;
		case CConst::versionNotSupportedResponseCode:
			shortDescription = "Version Not Supported";
			break;
		case CConst::deviceDisallowedResponseCode:
			shortDescription = "Device Disallowed";
			break;
		case CConst::missingParamResponseCode:
			shortDescription = "Missing Param";
			break;
		case CConst::invalidValueResponseCode:
			shortDescription = "Invalid Value";
			break;
		case CConst::unexpectedParamResponseCode:
			shortDescription = "Unexpected Param";
			break;
		case CConst::unsupportedSpectrumResponseCode:
			shortDescription = "Unsupported Spectrum";
			break;

		default:
			CORE_DUMP;
			break;
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
			for (auto &param : _missingParams) {
				paramsArray.append(param);
			}
			paramObj.insert("missingParams", paramsArray);
		}

		if (_invalidParams.size()) {
			paramsArray = QJsonArray();
			for (auto &param : _invalidParams) {
				paramsArray.append(param);
			}
			paramObj.insert("invalidParams", paramsArray);
		}

		if (_unexpectedParams.size()) {
			paramsArray = QJsonArray();
			for (auto &param : _unexpectedParams) {
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
		availableSpectrumInquiryResponse.insert("availabilityExpireTime",
							ISO8601TimeUTC(1));
	}

	QJsonObject extensionParameterObj;
	if (blackChannelInfos.size()) {
		extensionParameterObj.insert("blackChannelInfo", blackChannelInfos);
	}
	if (redChannelInfos.size()) {
		extensionParameterObj.insert("redChannelInfo", redChannelInfos);
	}

	QJsonObject vendorExtensionObj;
	vendorExtensionObj.insert("extensionId", "openAfc.redBlackData");
	vendorExtensionObj.insert("parameters", extensionParameterObj);

	QJsonArray vendorExtensionArray;
	vendorExtensionArray.append(vendorExtensionObj);

	availableSpectrumInquiryResponse.insert("vendorExtensions", vendorExtensionArray);

	availableSpectrumInquiryResponse.insert("response", responseObj);

	QJsonObject responses {{"version", _guiJsonVersion},
			       {"availableSpectrumInquiryResponses",
				QJsonArray {availableSpectrumInquiryResponse}}};

	return QJsonDocument(responses);
}

QJsonDocument AfcManager::generateExclusionZoneJson()
{
	QTemporaryDir tempDir;
	if (!tempDir.isValid()) {
		throw std::runtime_error("AfcManager::generateExclusionZone(): Failed to create a "
					 "temporary directory to store output of GeoJSON driver");
	}

	const QString tempOutFileName = "output.tmp";
	const QString tempOutFilePath = tempDir.filePath(tempOutFileName);

	GDALDataset *dataSet;
	OGRLayer *exclusionLayer = createGeoJSONLayer(tempOutFilePath.toStdString().c_str(),
						      &dataSet);

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

	if (exclusionLayer->CreateField(&objKind) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::generateExclusionZone(): Could not create "
					 "'kind' field in layer of the output data source");
	}
	if (exclusionLayer->CreateField(&fsid) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::generateExclusionZone(): Could not create "
					 "'fsid' field in layer of the output data source");
	}
	if (exclusionLayer->CreateField(&terrainHeight) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::generateExclusionZone(): Could not create "
					 "'terrainHeight' field in layer of the output data "
					 "source");
	}
	if (exclusionLayer->CreateField(&height) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::generateExclusionZone(): Could not create "
					 "'height' field in layer of the output data source");
	}
	if (exclusionLayer->CreateField(&lat) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::generateExclusionZone(): Could not create "
					 "'lat' field in layer of the output data source");
	}
	if (exclusionLayer->CreateField(&lon) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::generateExclusionZone(): Could not create "
					 "'lon' field in layer of the output data source");
	}

	// Instantiate polygon object
	std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> exclusionZoneFeature(
		OGRFeature::CreateFeature(exclusionLayer->GetLayerDefn()));

	// Intantiate unique-pointers to OGRPolygon and OGRLinearRing for storing the beam coverage
	GdalHelpers::GeomUniquePtr<OGRPolygon> exZone(
		GdalHelpers::createGeometry<OGRPolygon>()); // Use GdalHelpers.h templates to have
							    // unique pointers create these on the
							    // heap
	GdalHelpers::GeomUniquePtr<OGRLinearRing> exteriorOfZone(
		GdalHelpers::createGeometry<OGRLinearRing>());

	int ulsIdx;
	ULSClass *uls = findULSID(_exclusionZoneFSID, 0, ulsIdx);
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
	exteriorOfZone->addPoint(startPoint.get()); // Using .get() gives access to the pointer
						    // without giving up ownership

	for (const auto &point : _exclusionZone) {
		// Create OGRPoints to store the coordinates of the beam triangle
		// ***IMPORTANT NOTE: Coordinates stored as (Lon,Lat) here, required by geoJSON
		// specifications
		GdalHelpers::GeomUniquePtr<OGRPoint> posPoint(
			GdalHelpers::createGeometry<OGRPoint>());
		posPoint->setX(point.first);
		posPoint->setY(point.second);

		// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
		exteriorOfZone->addPoint(posPoint.get());
	}

	// Add exterior boundary of cone's polygon
	exZone->addRingDirectly(
		exteriorOfZone.release()); // Points unique-pointer to null and gives up ownership
					   // of exteriorOfCone to beamCone

	// Add geometry to feature
	exclusionZoneFeature->SetGeometryDirectly(exZone.release());

	if (exclusionLayer->CreateFeature(exclusionZoneFeature.release()) != OGRERR_NONE) {
		throw std::runtime_error("Could not add cone feature in layer of output data "
					 "source");
	}

	// Allocation clean-up
	GDALClose(dataSet); // Remove the reference to the dataset

	// Create file to be written to (creates a file, a json object, and a json document in order
	// to store)
	std::ifstream tempFileStream(tempOutFilePath.toStdString(), std::ifstream::in);
	const std::string geoJsonCollection = slurp(tempFileStream);

	QJsonDocument geoJsonDoc = QJsonDocument::fromJson(
		QString::fromStdString(geoJsonCollection).toUtf8());
	QJsonObject geoJsonObj = geoJsonDoc.object();

	QJsonObject analysisJsonObj = {{"geoJson", geoJsonObj},
				       {"statusMessageList",
					generateStatusMessages(statusMessageList)}};

	return QJsonDocument(analysisJsonObj);
}

OGRLayer *AfcManager::createGeoJSONLayer(const char *tmpPath, GDALDataset **dataSet)
{
	GDALDriver *driver = (GDALDriver *)GDALGetDriverByName("GeoJSON");
	if (!driver) {
		throw std::runtime_error("GDALGetDriverByName() error");
	}

	/* create empty dataset */
	*dataSet = driver->Create(tmpPath, 0, 0, 0, GDT_Unknown, NULL);
	if (!(*dataSet)) {
		throw std::runtime_error("GDALOpenEx() error");
	}

	OGRSpatialReference
		spatialRefWGS84; // Set the desired spatial reference (WGS84 in this case)
	spatialRefWGS84.SetWellKnownGeogCS("WGS84");

	OGRLayer *layer = (*dataSet)->CreateLayer("Temp_Output",
						  &spatialRefWGS84,
						  wkbPolygon,
						  NULL);
	if (!layer) {
		throw std::runtime_error("GDALDataset::CreateLayer() error");
	}

	return layer;
}

void AfcManager::addHeatmap(OGRLayer *layer)
{
	// add heatmap
	LOGGER_DEBUG(logger) << "adding heatmap";

	bool itonFlag = (_heatmapAnalysisStr == "iton");

	std::string valueStr = (itonFlag ? "ItoN" : "eirpLimit");

	OGRFieldDefn objFill("fill", OFTString);
	OGRFieldDefn objOpacity("fill-opacity", OFTReal);
	OGRFieldDefn valueField(valueStr.c_str(), OFTReal);
	OGRFieldDefn indoor("indoor", OFTString);
	objFill.SetWidth(8);
	objOpacity.SetWidth(32);
	valueField.SetWidth(32);
	indoor.SetWidth(32);

	if (layer->CreateField(&objFill) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::addHeatmap(): Could not create 'fill' field "
					 "in layer of the output data source");
	}
	if (layer->CreateField(&objOpacity) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::addHeatmap(): Could not create "
					 "'fill-opacity' field in layer of the output data source");
	}
	if (layer->CreateField(&valueField) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::addHeatmap(): Could not create value field "
					 "in layer of the output data source");
	}
	if (layer->CreateField(&indoor) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::addHeatmap(): Could not create 'indoor' "
					 "field in layer of the output data source");
	}

	double latDel = 0.5 * (_heatmapMaxLat - _heatmapMinLat) /
			_heatmapNumPtsLat; // distance from center point to top/bot side of square
	double lonDel =
		0.5 * (_heatmapMaxLon - _heatmapMinLon) /
		_heatmapNumPtsLon; // distance from center point to left/right side of square
	LOGGER_DEBUG(logger) << "generating heatmap: " << _heatmapNumPtsLon << "x"
			     << _heatmapNumPtsLat;
	for (int lonIdx = 0; lonIdx < _heatmapNumPtsLon; lonIdx++) {
		for (int latIdx = 0; latIdx < _heatmapNumPtsLat; latIdx++) {
			double lon = (_heatmapMinLon * (2 * _heatmapNumPtsLon - 2 * lonIdx - 1) +
				      _heatmapMaxLon * (2 * lonIdx + 1)) /
				     (2 * _heatmapNumPtsLon);
			double lat = (_heatmapMinLat * (2 * _heatmapNumPtsLat - 2 * latIdx - 1) +
				      _heatmapMaxLat * (2 * latIdx + 1)) /
				     (2 * _heatmapNumPtsLat);
			// Instantiate polygon object
			std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> heatmapFeature(
				OGRFeature::CreateFeature(layer->GetLayerDefn()));

			// Intantiate unique-pointers to OGRPolygon and OGRLinearRing for storing
			// the beam coverage
			GdalHelpers::GeomUniquePtr<OGRPolygon> mapZone(
				GdalHelpers::createGeometry<
					OGRPolygon>()); // Use GdalHelpers.h templates to have
							// unique pointers create these on the heap
			GdalHelpers::GeomUniquePtr<OGRLinearRing> mapExt(
				GdalHelpers::createGeometry<OGRLinearRing>());

			_minEIRP_dBm = (_heatmapIsIndoor[lonIdx][latIdx] ? _minEIRPIndoor_dBm :
									   _minEIRPOutdoor_dBm);

			std::string color = getHeatmapColor(_heatmapIToNDB[lonIdx][latIdx],
							    _heatmapIsIndoor[lonIdx][latIdx],
							    true);

			double value;
			double showValueFlag = true;
			if (itonFlag) {
				value = _heatmapIToNDB[lonIdx][latIdx];
			} else {
				value = _maxEIRP_dBm + _IoverN_threshold_dB -
					_heatmapIToNDB[lonIdx][latIdx];
				if (value > _maxEIRP_dBm) {
					value = _maxEIRP_dBm;
				} else if (value < _minEIRP_dBm) {
					value = _minEIRP_dBm;
					showValueFlag = false;
				}
			}

			// Add properties to the geoJSON features
			heatmapFeature->SetField("kind", "HMAP");
			heatmapFeature->SetField("fill", color.c_str());
			heatmapFeature->SetField("fill-opacity", 0.5);
			if (showValueFlag) {
				heatmapFeature->SetField(valueStr.c_str(), value);
			}
			heatmapFeature->SetField("indoor",
						 _heatmapIsIndoor[lonIdx][latIdx] ? "Y" : "N");

			// Create OGRPoints to store the coordinates of the heatmap box
			// ***IMPORTANT NOTE: Coordinates stored as (Lon,Lat) here, required by
			// geoJSON specifications
			GdalHelpers::GeomUniquePtr<OGRPoint> tl(
				GdalHelpers::createGeometry<OGRPoint>());
			tl->setX(lon - lonDel);
			tl->setY(lat + latDel);
			// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
			mapExt->addPoint(tl.get());

			GdalHelpers::GeomUniquePtr<OGRPoint> tr(
				GdalHelpers::createGeometry<OGRPoint>());
			tr->setX(lon + lonDel);
			tr->setY(lat + latDel);
			// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
			mapExt->addPoint(tr.get());

			GdalHelpers::GeomUniquePtr<OGRPoint> br(
				GdalHelpers::createGeometry<OGRPoint>());
			br->setX(lon + lonDel);
			br->setY(lat - latDel);
			// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
			mapExt->addPoint(br.get());

			GdalHelpers::GeomUniquePtr<OGRPoint> bl(
				GdalHelpers::createGeometry<OGRPoint>());
			bl->setX(lon - lonDel);
			bl->setY(lat - latDel);
			// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
			mapExt->addPoint(bl.get());

			// add end point again
			mapExt->addPoint(tl.get());

			// Add exterior boundary of cone's polygon
			mapZone->addRingDirectly(
				mapExt.release()); // Points unique-pointer to null and gives up
						   // ownership of exteriorOfCone to beamCone

			// Add geometry to feature
			heatmapFeature->SetGeometryDirectly(mapZone.release());

			if (layer->CreateFeature(heatmapFeature.release()) != OGRERR_NONE) {
				throw std::runtime_error("Could not add heat map tile feature in "
							 "layer of output data source");
			}
		}
	}
}

void AfcManager::exportGUIjson(const QString &exportJsonPath, const std::string &tempDir)
{
	QJsonDocument outputDocument;
	if (_analysisType == "APAnalysis") {
		// if the request type is PAWS then we only return the spectrum data
		// outputDocument = QJsonDocument(jsonSpectrumData(_channelList, _deviceDesc,
		// _wlanMinFreq));

		// Write PAWS outputs to JSON file
		// QFile outputAnalysisFile(exportJsonPath);
		// outputAnalysisFile.open(QFile::WriteOnly);
		// outputAnalysisFile.write(outputDocument.toJson());
		// outputAnalysisFile.close();
		// LOGGER_DEBUG(logger) << "PAWS response saved to the following JSON file: " <<
		// outputAnalysisFile.fileName().toStdString(); return; // Short circuit since
		// export is complete now
	} else if ((_analysisType == "AP-AFC") || (_analysisType == "HeatmapAnalysis")) {
		// temporarily return PAWS until we write new generator function
		// outputDocument = QJsonDocument(jsonSpectrumData(_rlanBWList, _numChan,
		// _channelData, _deviceDesc, _wlanMinFreq));
		outputDocument = generateRatAfcJson();

		if ((_responseCode == CConst::successResponseCode) &&
		    (!_mapDataGeoJsonFile.empty())) {
			generateMapDataGeoJson(tempDir);
		}
	} else if (_analysisType == "ExclusionZoneAnalysis") {
		outputDocument = generateExclusionZoneJson();
	} else if (_analysisType == "ScanAnalysis") {
		outputDocument = QJsonDocument();
#if DEBUG_AFC
	} else if (_analysisType == "test_itm") {
		// Do nothing
	} else if (_analysisType == "test_winner2") {
		// Do nothing
#endif
	} else {
		throw std::runtime_error(ErrStream() << "ERROR: Unrecognized analysis type = \""
						     << _analysisType << "\"");
	}

	// Write analysis outputs to JSON file
	QByteArray data = outputDocument.toJson();
	if (!AfcManager::_dataIf->gzipAndWriteFile(exportJsonPath, data)) {
		throw std::runtime_error("Error writing output file");
	}
	LOGGER_DEBUG(logger) << "Output file written to " << exportJsonPath.toStdString();
}

void AfcManager::generateMapDataGeoJson(const std::string &tempDir)
{
	QJsonDocument outputDocument;

	QTemporaryDir geoTempDir;
	if (!geoTempDir.isValid()) {
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Failed to create a "
					 "temporary directory to store output of GeoJSON driver");
	}

	const QString tempOutFileName = "output.tmp";
	const QString tempOutFilePath = geoTempDir.filePath(tempOutFileName);

	GDALDataset *dataSet;
	OGRLayer *coneLayer = createGeoJSONLayer(tempOutFilePath.toStdString().c_str(), &dataSet);

	OGRFieldDefn objKind("kind", OFTString);
	OGRFieldDefn dbNameField("DBNAME", OFTString);
	OGRFieldDefn fsidField("FSID", OFTInteger);
	OGRFieldDefn segmentField("SEGMENT", OFTInteger);
	OGRFieldDefn startFreq("startFreq", OFTReal);
	OGRFieldDefn stopFreq("stopFreq", OFTReal);
	objKind.SetWidth(64); /* fsLonField.SetWidth(64); fsLatField.SetWidth(64);*/
	fsidField.SetWidth(32); /* fsLonField.SetWidth(64); fsLatField.SetWidth(64);*/
	segmentField.SetWidth(32); /* fsLonField.SetWidth(64); fsLatField.SetWidth(64);*/
	startFreq.SetWidth(32);
	stopFreq.SetWidth(32);

	if (coneLayer->CreateField(&objKind) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Could not create "
					 "'kind' field in layer of the output data source");
	}
	if (coneLayer->CreateField(&dbNameField) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Could not create "
					 "'DBNAME' field in layer of the output data source");
	}
	if (coneLayer->CreateField(&fsidField) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Could not create "
					 "'FSID' field in layer of the output data source");
	}
	if (coneLayer->CreateField(&segmentField) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Could not create "
					 "'SEGMENT' field in layer of the output data source");
	}
	if (coneLayer->CreateField(&startFreq) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Could not create "
					 "'startFreq' field in layer of the output data source");
	}
	if (coneLayer->CreateField(&stopFreq) != OGRERR_NONE) {
		throw std::runtime_error("AfcManager::generateMapDataGeoJson(): Could not create "
					 "'stopFreq' field in layer of the output data source");
	}

	// Calculate the cones in iterative loop
	LatLon FSLatLonVal, posPointLatLon, negPointLatLon;
	for (const auto &ulsIdx : _ulsIdxList) {
		ULSClass *uls = (*_ulsList)[ulsIdx];

		// Grab FSID for storing with coverage polygon
		int FSID = uls->getID();

		std::string dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);

		int numPR = uls->getNumPR();
		for (int segIdx = 0; segIdx < numPR + 1; ++segIdx) {
			// Instantiate cone object
			std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> coneFeature(
				OGRFeature::CreateFeature(coneLayer->GetLayerDefn()));

			Vector3 ulsTxPosn = (segIdx == 0 ? uls->getTxPosition() :
							   uls->getPR(segIdx - 1).positionTx);
			double ulsTxLongitude = (segIdx == 0 ? uls->getTxLongitudeDeg() :
							       uls->getPR(segIdx - 1).longitudeDeg);
			double ulsTxLatitude = (segIdx == 0 ? uls->getTxLatitudeDeg() :
							      uls->getPR(segIdx - 1).latitudeDeg);
			// double ulsTxHeight = (segIdx == 0 ? uls->getTxHeightAMSL() :
			// uls->getPR(segIdx-1).heightAMSLTx);

			Vector3 ulsRxPosn = (segIdx == numPR ? uls->getRxPosition() :
							       uls->getPR(segIdx).positionRx);
			double ulsRxLongitude = (segIdx == numPR ? uls->getRxLongitudeDeg() :
								   uls->getPR(segIdx).longitudeDeg);
			double ulsRxLatitude = (segIdx == numPR ? uls->getRxLatitudeDeg() :
								  uls->getPR(segIdx).latitudeDeg);
			// double ulsRxHeight = (segIdx == numPR ? uls->getRxHeightAMSL() :
			// uls->getPR(segIdx).heightAMSLRx);

			bool txLocFlag = (!std::isnan(ulsTxPosn.x())) &&
					 (!std::isnan(ulsTxPosn.y())) &&
					 (!std::isnan(ulsTxPosn.z()));

			double linkDistKm;
			if (!txLocFlag) {
				linkDistKm = 1.0;
				Vector3 segPointing = (segIdx == numPR ?
							       uls->getAntennaPointing() :
							       uls->getPR(segIdx).pointing);
				ulsTxPosn = ulsRxPosn + linkDistKm * segPointing;

				GeodeticCoord ulsTxPosnGeodetic = EcefModel::ecefToGeodetic(
					ulsTxPosn);
				ulsTxLongitude = ulsTxPosnGeodetic.longitudeDeg;
				ulsTxLatitude = ulsTxPosnGeodetic.latitudeDeg;
			} else {
				linkDistKm = (ulsTxPosn - ulsRxPosn).len();
			}

			LatLon ulsRxLatLonVal = std::make_pair(ulsRxLatitude, ulsRxLongitude);
			LatLon ulsTxLatLonVal = std::make_pair(ulsTxLatitude, ulsTxLongitude);

			// Compute the beam coordinates and store into DoublePairs
			std::tie(FSLatLonVal, posPointLatLon, negPointLatLon) =
				computeBeamConeLatLon(uls, ulsRxLatLonVal, ulsTxLatLonVal);

			// Intantiate unique-pointers to OGRPolygon and OGRLinearRing for storing
			// the beam coverage
			GdalHelpers::GeomUniquePtr<OGRPolygon> beamCone(
				GdalHelpers::createGeometry<
					OGRPolygon>()); // Use GdalHelpers.h templates to have
							// unique pointers create these on the heap
			GdalHelpers::GeomUniquePtr<OGRLinearRing> exteriorOfCone(
				GdalHelpers::createGeometry<OGRLinearRing>());

			// Create OGRPoints to store the coordinates of the beam triangle
			// ***IMPORTANT NOTE: Coordinates stored as (Lon,Lat) here, required by
			// geoJSON specifications
			GdalHelpers::GeomUniquePtr<OGRPoint> FSPoint(
				GdalHelpers::createGeometry<OGRPoint>());
			FSPoint->setX(FSLatLonVal.second);
			FSPoint->setY(
				FSLatLonVal
					.first); // Must set points manually since cannot construct
						 // while pointing at with unique pointer
			GdalHelpers::GeomUniquePtr<OGRPoint> posPoint(
				GdalHelpers::createGeometry<OGRPoint>());
			posPoint->setX(posPointLatLon.second);
			posPoint->setY(posPointLatLon.first);
			GdalHelpers::GeomUniquePtr<OGRPoint> negPoint(
				GdalHelpers::createGeometry<OGRPoint>());
			negPoint->setX(negPointLatLon.second);
			negPoint->setY(negPointLatLon.first);

			// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
			exteriorOfCone->addPoint(
				FSPoint.get()); // Using .get() gives access to the pointer without
						// giving up ownership
			exteriorOfCone->addPoint(posPoint.get());
			exteriorOfCone->addPoint(negPoint.get());
			exteriorOfCone->addPoint(
				FSPoint.get()); // Adds this again just so that the polygon closes
						// where it starts (at FS location)

			// Add exterior boundary of cone's polygon
			beamCone->addRingDirectly(
				exteriorOfCone
					.release()); // Points unique-pointer to null and gives up
						     // ownership of exteriorOfCone to beamCone

			// Add properties to the geoJSON features
			coneFeature->SetField("FSID", FSID);
			coneFeature->SetField("SEGMENT", segIdx);
			coneFeature->SetField("DBNAME", dbName.c_str());
			coneFeature->SetField("kind", "FS");
			coneFeature->SetField("startFreq", uls->getStartFreq() / 1.0e6);
			coneFeature->SetField("stopFreq", uls->getStopFreq() / 1.0e6);
			/* coneFeature->SetField("FS Lon", FSLatLonVal.second);
			 * coneFeature->SetField("FS lat", FSLatLonVal.first);*/

			// Add geometry to feature
			coneFeature->SetGeometryDirectly(beamCone.release());

			if (coneLayer->CreateFeature(coneFeature.release()) != OGRERR_NONE) {
				throw std::runtime_error("Could not add cone feature in layer of "
							 "output data source");
			}
		}
	}
	if (_rlanAntenna) {
		// Instantiate cone object
		std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> coneFeature(
			OGRFeature::CreateFeature(coneLayer->GetLayerDefn()));

		double rlanAntennaArrowLength = 1000.0;
		Vector3 centerPosn;
		double centerLon, centerLat;
		if (_analysisType == "HeatmapAnalysis") {
			centerPosn = _heatmapRLANCenterPosn;
			centerLon = _heatmapRLANCenterLon;
			centerLat = _heatmapRLANCenterLat;
		} else {
			centerPosn = _rlanRegion->getCenterPosn();
			centerLon = _rlanRegion->getCenterLongitude();
			centerLat = _rlanRegion->getCenterLatitude();
		}
		Vector3 ptgPosn = centerPosn + (rlanAntennaArrowLength / 1000.0) * _rlanPointing;

		GeodeticCoord ptgPosnGeodetic = EcefModel::ecefToGeodetic(ptgPosn);

		LatLon rlanLatLonVal = std::make_pair(centerLat, centerLon);
		LatLon ptgLatLonVal = std::make_pair(ptgPosnGeodetic.latitudeDeg,
						     ptgPosnGeodetic.longitudeDeg);

		double cosVal = cos(_rlanRegion->getCenterLatitude() * M_PI / 180.0);
		double cvgTheta = 2.0 * M_PI / 180.0;
		double deltaLat = ptgLatLonVal.first - rlanLatLonVal.first;
		double deltaLon = ptgLatLonVal.second - rlanLatLonVal.second;
		posPointLatLon = std::make_pair(rlanLatLonVal.first + deltaLat * cos(cvgTheta) +
							deltaLon * cosVal * sin(cvgTheta),
						rlanLatLonVal.second +
							(deltaLon * cosVal * cos(cvgTheta) -
							 deltaLat * sin(cvgTheta)) /
								cosVal);
		negPointLatLon = std::make_pair(rlanLatLonVal.first + deltaLat * cos(cvgTheta) -
							deltaLon * cosVal * sin(cvgTheta),
						rlanLatLonVal.second +
							(deltaLon * cosVal * cos(cvgTheta) +
							 deltaLat * sin(cvgTheta)) /
								cosVal);

		// Intantiate unique-pointers to OGRPolygon and OGRLinearRing for storing the beam
		// coverage
		GdalHelpers::GeomUniquePtr<OGRPolygon> beamCone(
			GdalHelpers::createGeometry<OGRPolygon>()); // Use GdalHelpers.h templates
								    // to have unique pointers
								    // create these on the heap
		GdalHelpers::GeomUniquePtr<OGRLinearRing> exteriorOfCone(
			GdalHelpers::createGeometry<OGRLinearRing>());

		// Create OGRPoints to store the coordinates of the beam triangle
		// ***IMPORTANT NOTE: Coordinates stored as (Lon,Lat) here, required by geoJSON
		// specifications
		GdalHelpers::GeomUniquePtr<OGRPoint> rlanPoint(
			GdalHelpers::createGeometry<OGRPoint>());
		rlanPoint->setX(rlanLatLonVal.second);
		rlanPoint->setY(
			rlanLatLonVal.first); // Must set points manually since cannot construct
					      // while pointing at with unique pointer
		GdalHelpers::GeomUniquePtr<OGRPoint> posPoint(
			GdalHelpers::createGeometry<OGRPoint>());
		posPoint->setX(posPointLatLon.second);
		posPoint->setY(posPointLatLon.first);
		GdalHelpers::GeomUniquePtr<OGRPoint> negPoint(
			GdalHelpers::createGeometry<OGRPoint>());
		negPoint->setX(negPointLatLon.second);
		negPoint->setY(negPointLatLon.first);

		// Adding the polygon vertices to a OGRLinearRing object for geoJSON export
		exteriorOfCone->addPoint(rlanPoint.get()); // Using .get() gives access to the
							   // pointer without giving up ownership
		exteriorOfCone->addPoint(posPoint.get());
		exteriorOfCone->addPoint(negPoint.get());
		exteriorOfCone->addPoint(
			rlanPoint.get()); // Adds this again just so that the polygon closes where
					  // it starts (at FS location)

		// Add exterior boundary of cone's polygon
		beamCone->addRingDirectly(
			exteriorOfCone.release()); // Points unique-pointer to null and gives up
						   // ownership of exteriorOfCone to beamCone

		// Add properties to the geoJSON features
		coneFeature->SetField("FSID", 0);
		coneFeature->SetField("SEGMENT", 0);
		coneFeature->SetField("DBNAME", "0");
		coneFeature->SetField("kind", "RLAN");
		coneFeature->SetField("startFreq", 0.0);
		coneFeature->SetField("stopFreq", 0.0);

		// Add geometry to feature
		coneFeature->SetGeometryDirectly(beamCone.release());

		if (coneLayer->CreateFeature(coneFeature.release()) != OGRERR_NONE) {
			throw std::runtime_error("Could not add cone feature in layer of output "
						 "data source");
		}
	}

	// add building database raster boundary if loaded
	addBuildingDatabaseTiles(coneLayer);

	// add denied regions
	addDeniedRegions(coneLayer);

	if (_analysisType == "HeatmapAnalysis") {
		addHeatmap(coneLayer);
	}

	// Allocation clean-up
	GDALClose(dataSet); // Remove the reference to the dataset

	// Create file to be written to (creates a file, a json object, and a json document in order
	// to store)
	std::ifstream tempFileStream(tempOutFilePath.toStdString(), std::ifstream::in);
	const std::string geoJsonCollection = slurp(tempFileStream);

	QJsonDocument geoJsonDoc = QJsonDocument::fromJson(
		QString::fromStdString(geoJsonCollection).toUtf8());
	QJsonObject geoJsonObj = geoJsonDoc.object();

	QJsonObject analysisJsonObj = {{"geoJson", geoJsonObj}};

	outputDocument = QJsonDocument(analysisJsonObj);

	// Write map data GEOJSON file
	std::string fullPathMapDataFile = QDir(QString::fromStdString(tempDir))
						  .filePath(QString::fromStdString(
							  _mapDataGeoJsonFile))
						  .toStdString();
	auto mapDataFile = FileHelpers::open(QString::fromStdString(fullPathMapDataFile),
					     QIODevice::WriteOnly);
	auto gzip_writer = new GzipStream(mapDataFile.get());
	if (!gzip_writer->open(QIODevice::WriteOnly)) {
		throw std::runtime_error("Gzip failed to open.");
	}
	gzip_writer->write(outputDocument.toJson());
	gzip_writer->close();
	LOGGER_DEBUG(logger) << "Output file written to " << mapDataFile->fileName().toStdString();
}

#if 0
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
		for (auto& band : rlanBWList)
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
	channelNameStart[320] = 31;
	std::map<int, int> channelNameStep;
	channelNameStep[20] = 4;
	channelNameStep[40] = 8;
	channelNameStep[80] = 16;
	channelNameStep[160] = 32;
	channelNameStep[320] = 32;

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
#endif

#if 0
// Converts spectrum data into a valid PAWS response
QJsonObject jsonSpectrumData(const std::vector<ChannelStruct> &channelList, const QJsonObject &deviceDesc, const double &startFreq)
{
	// get a list of the bandwidths used and group the channels
	auto initialPair = std::make_pair(channelList.front().bandwidth(), std::vector<ChannelStruct>());
	auto rlanBWList = std::vector<std::pair<int, std::vector<ChannelStruct>>> { initialPair };
	for (const ChannelStruct &channel : channelList)
	{
		bool found = false;
		for (auto& band : rlanBWList)
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
#endif

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
	if ((dashPosn1 == std::string::npos) || (dashPosn1 == 0)) {
		// Angle is in decimal format, not DMS
		angleDeg = strtod(dmsStr.c_str(), &chptr);
	} else {
		if (!error) {
			dashPosn2 = dmsStr.find('-', dashPosn1 + 1);
			if (dashPosn2 == std::string::npos) {
				error = true;
			}
		}

		double dVal, mVal, sVal;
		if (!error) {
			letterPosn = dmsStr.find_first_of("NEWS", dashPosn2 + 1);

			std::string dStr = dmsStr.substr(0, dashPosn1);
			std::string mStr = dmsStr.substr(dashPosn1 + 1, dashPosn2 - dashPosn1 - 1);
			std::string sStr = ((letterPosn == std::string::npos) ?
						    dmsStr.substr(dashPosn2 + 1) :
						    dmsStr.substr(dashPosn2 + 1,
								  letterPosn - dashPosn2 - 1));

			dVal = strtod(dStr.c_str(), &chptr);
			mVal = strtod(mStr.c_str(), &chptr);
			sVal = strtod(sStr.c_str(), &chptr);
		}

		if (error) {
			throw std::runtime_error(ErrStream() << "ERROR: Unable to convert DMS "
								"string to angle, DMS string = \""
							     << dmsStr << "\"");
		}

		angleDeg = dVal + (mVal + sVal / 60.0) / 60.0;

		if (letterPosn != std::string::npos) {
			if ((dmsStr.at(letterPosn) == 'W') || (dmsStr.at(letterPosn) == 'S')) {
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

	for (int aIdx = 0; (aIdx < (int)_antennaList.size()) && (!found); aIdx++) {
		if (std::string(_antennaList[aIdx]->get_strid()) == strval) {
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
ULSClass *AfcManager::findULSID(int ulsID, int dbIdx, int &ulsIdx)
{
	int ulsIdxA, ulsIdxB;
	int id, db, idA, idB, dbIdxA, dbIdxB;
	ULSClass *uls = (ULSClass *)NULL;
	ulsIdx = -1;

	bool found = false;

	ulsIdxA = 0;
	idA = (*_ulsList)[ulsIdxA]->getID();
	dbIdxA = (*_ulsList)[ulsIdxA]->getDBIdx();
	if ((idA == ulsID) && (dbIdxA == dbIdx)) {
		found = true;
		ulsIdx = ulsIdxA;
		uls = (*_ulsList)[ulsIdx];
	} else if ((dbIdx < dbIdxA) || ((dbIdx == dbIdxA) && (ulsID < idA))) {
		throw std::runtime_error(ErrStream() << "ERROR: Invalid DBIDX = " << dbIdx
						     << " FSID = " << ulsID);
	}

	ulsIdxB = _ulsList->getSize() - 1;
	;
	idB = (*_ulsList)[ulsIdxB]->getID();
	dbIdxB = (*_ulsList)[ulsIdxB]->getDBIdx();
	if ((idB == ulsID) && (dbIdxB == dbIdx)) {
		found = true;
		ulsIdx = ulsIdxB;
		uls = (*_ulsList)[ulsIdx];
	} else if ((dbIdx > dbIdxB) || ((dbIdx == dbIdxB) && (ulsID > idB))) {
		throw std::runtime_error(ErrStream() << "ERROR: Invalid DBIDX = " << dbIdx
						     << " FSID = " << ulsID);
	}

	while ((ulsIdxA + 1 < ulsIdxB) && (!found)) {
		ulsIdx = (ulsIdxA + ulsIdxB) / 2;
		id = (*_ulsList)[ulsIdx]->getID();
		db = (*_ulsList)[ulsIdx]->getDBIdx();
		if ((ulsID == id) && (dbIdx == db)) {
			found = true;
			uls = (*_ulsList)[ulsIdx];
		} else if ((dbIdx > db) || ((dbIdx == db) && (ulsID > id))) {
			ulsIdxA = ulsIdx;
			idA = id;
		} else {
			ulsIdxB = ulsIdx;
			idB = id;
		}
	}

	if (!found) {
		uls = (ULSClass *)NULL;
		ulsIdx = -1;
	}

	return (uls);
}
/******************************************************************************************/

/******************************************************************************************/
/**** AfcManager::computeBeamConeLatLon                                                ****/
/******************************************************************************************/
// Calculates and stores the beam cone coordinates
std::tuple<LatLon, LatLon, LatLon> AfcManager::computeBeamConeLatLon(ULSClass *uls,
								     LatLon rxLatLonVal,
								     LatLon txLatLonVal)
{
	// Store lat/lon from ULS class
	LatLon FSLatLonVal = rxLatLonVal;

	std::make_pair(uls->getRxLatitudeDeg(), uls->getRxLongitudeDeg());

	// Obtain the angle in radians for 3 dB attenuation
	double theta_rad = uls->computeBeamWidth(3.0) * M_PI / 180;
	double cosTheta = cos(theta_rad);
	double sinTheta = sin(theta_rad);

	double cosVal = cos(rxLatLonVal.first * M_PI / 180.0);
	double deltaX = (txLatLonVal.second - rxLatLonVal.second) * cosVal;
	double deltaY = txLatLonVal.first - rxLatLonVal.first;

	double deltaLon1 = (deltaX * cosTheta + deltaY * sinTheta) / cosVal;
	double deltaLat1 = (deltaY * cosTheta - deltaX * sinTheta);

	double deltaLon2 = (deltaX * cosTheta - deltaY * sinTheta) / cosVal;
	double deltaLat2 = (deltaY * cosTheta + deltaX * sinTheta);

	// Store in DoublePairs
	LatLon posPointLatLon = std::make_pair(rxLatLonVal.first + deltaLat1,
					       rxLatLonVal.second + deltaLon1);
	LatLon negPointLatLon = std::make_pair(rxLatLonVal.first + deltaLat2,
					       rxLatLonVal.second + deltaLon2);

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

	if (fabsMHz <= BMHz / 2) {
		overlap = fabsMHz;
	} else {
		overlap = BMHz / 2;
	}

	// LOGGER_INFO(logger) << "ACIFN: overlap_0 = " << overlap;

	if (fabsMHz > BMHz / 2) {
		if (fabsMHz <= BMHz / 2 + 1.0) {
			overlap += (1.0 - exp(log(10.0) * (BMHz - 2 * fabsMHz))) / (2 * log(10.0));
		} else {
			overlap += 0.99 / (2 * log(10.0));
		}
	}

	// LOGGER_INFO(logger) << "ACIFN: overlap_1 = " << overlap;

	if (fabsMHz > BMHz / 2 + 1.0) {
		if (fabsMHz <= BMHz) {
			overlap += exp(log(10.0) * (-6 * BMHz + 28.0) / (5 * BMHz - 10.0)) *
				   (exp(log(10.0) * ((-8.0) / (5 * BMHz - 10.0)) *
					(BMHz / 2 + 1.0)) -
				    exp(log(10.0) * ((-8.0 * fabsMHz) / (5 * BMHz - 10.0)))) /
				   ((8.0 * log(10.0)) / (5 * BMHz - 10.0));
		} else {
			overlap += exp(log(10.0) * (-6 * BMHz + 28.0) / (5 * BMHz - 10.0)) *
				   (exp(log(10.0) * ((-8.0) / (5 * BMHz - 10.0)) *
					(BMHz / 2 + 1.0)) -
				    exp(log(10.0) * ((-8.0 * BMHz) / (5 * BMHz - 10.0)))) /
				   ((8.0 * log(10.0)) / (5 * BMHz - 10.0));
		}
	}

	// LOGGER_INFO(logger) << "ACIFN: overlap_2 = " << overlap;

	if (fabsMHz > BMHz) {
		if (fabsMHz <= 3 * BMHz / 2) {
			overlap += exp(-log(10.0) * 0.4) *
				   (exp(log(10.0) * (-2.4)) -
				    exp(log(10.0) * (-2.4 * fabsMHz / BMHz))) /
				   ((2.4 * log(10.0) / BMHz));
		} else {
			overlap += exp(-log(10.0) * 0.4) *
				   (exp(log(10.0) * (-2.4)) - exp(log(10.0) * (-3.6))) /
				   ((2.4 * log(10.0) / BMHz));
		}
	}

	// LOGGER_INFO(logger) << "ACIFN: overlap_3 = " << overlap;

	overlap = sign * overlap / BMHz;

	// LOGGER_INFO(logger) << "ACIFN: overlap = " << overlap;
	// LOGGER_INFO(logger) << "ACIFN: ==========================";

	// printf("ACIFN: %12.10f %12.10f %12.10f\n", fMHz, BMHz, overlap);

	return overlap;
}
/******************************************************************************************/

/******************************************************************************************/
/**** AfcManager::computeSpectralOverlapLoss                                           ****/
/******************************************************************************************/
bool AfcManager::computeSpectralOverlapLoss(double *spectralOverlapLossDBptr,
					    double sigStartFreq,
					    double sigStopFreq,
					    double rxStartFreq,
					    double rxStopFreq,
					    bool aciFlag,
					    CConst::SpectralAlgorithmEnum spectralAlgorithm) const
{
	bool hasOverlap;

	if (!aciFlag) {
		if ((sigStopFreq <= rxStartFreq) || (sigStartFreq >= rxStopFreq)) {
			hasOverlap = false;
			if (spectralOverlapLossDBptr) {
				*spectralOverlapLossDBptr =
					-std::numeric_limits<double>::infinity();
			}
		} else {
			hasOverlap = true;
			if (spectralOverlapLossDBptr) {
				double f1 = (sigStartFreq < rxStartFreq ? rxStartFreq :
									  sigStartFreq);
				double f2 = (sigStopFreq > rxStopFreq ? rxStopFreq : sigStopFreq);
				double overlap;
				if (spectralAlgorithm == CConst::pwrSpectralAlgorithm) {
					overlap = (f2 - f1) / (sigStopFreq - sigStartFreq);
				} else {
					overlap = (rxStopFreq - rxStartFreq) /
						  (sigStopFreq - sigStartFreq);
				}
				*spectralOverlapLossDBptr = -10.0 * log(overlap) / log(10.0);
			}
		}
	} else {
		if ((2 * sigStopFreq - sigStartFreq <= rxStartFreq) ||
		    (2 * sigStartFreq - sigStopFreq >= rxStopFreq)) {
			hasOverlap = false;
			if (spectralOverlapLossDBptr) {
				*spectralOverlapLossDBptr =
					-std::numeric_limits<double>::infinity();
			}
		} else {
			hasOverlap = true;
			if (spectralOverlapLossDBptr) {
				double BMHz = (sigStopFreq - sigStartFreq) * 1.0e-6;
				double fStartMHz = (rxStartFreq -
						    (sigStartFreq + sigStopFreq) / 2) *
						   1.0e-6;
				double fStopMHz = (rxStopFreq - (sigStartFreq + sigStopFreq) / 2) *
						  1.0e-6;
				if (spectralAlgorithm == CConst::pwrSpectralAlgorithm) {
					double overlap = aciFn(fStopMHz, BMHz) -
							 aciFn(fStartMHz, BMHz);
					*spectralOverlapLossDBptr = -10.0 * log(overlap) /
								    log(10.0);
				} else {
					double fCrit;
					if ((fStartMHz <= 0.0) && (fStopMHz >= 0.0)) {
						fCrit = 0.0;
					} else {
						fCrit = std::min(fabs(fStartMHz), fabs(fStopMHz));
					}
					double psdDB;
					if (fCrit < BMHz / 2) {
						psdDB = 0.0;
					} else if (fCrit < BMHz / 2 + 1.0) {
						psdDB = -20.0 * (fCrit - BMHz / 2);
					} else if (fCrit < BMHz) {
						psdDB = (-20.0 * (BMHz - fCrit) -
							 28.0 * (fCrit - BMHz / 2 - 1.0)) /
							(BMHz / 2 - 1.0);
					} else {
						psdDB = (-28.0 * (3 * BMHz / 2 - fCrit) -
							 40.0 * (fCrit - BMHz)) /
							(BMHz / 2);
					}
					double overlap = (rxStopFreq - rxStartFreq) /
							 (sigStopFreq - sigStartFreq);
					*spectralOverlapLossDBptr = -psdDB -
								    10.0 * log(overlap) / log(10.0);
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
void AfcManager::readULSData(
	const std::vector<std::tuple<std::string, std::string>> &ulsDatabaseList,
	PopGridClass *popGridVal,
	int linkDirection,
	double minFreq,
	double maxFreq,
	bool removeMobileFlag,
	CConst::SimulationEnum simulationFlag,
	const double &minLat,
	const double &maxLat,
	const double &minLon,
	const double &maxLon)
{
	AnomalyGzipCsv anomGc(_fsAnomFile);

	int prIdx;
	int dbIdx;
	bool prevGdalDirectMode = (_terrainDataModel ? _terrainDataModel->setGdalDirectMode(true) :
						       false);
	for (dbIdx = 0; dbIdx < (int)ulsDatabaseList.size(); ++dbIdx) {
		std::string name = std::get<0>(ulsDatabaseList[dbIdx]);
		std::string filename = std::get<1>(ulsDatabaseList[dbIdx]);

		LOGGER_INFO(logger) << "Reading " << name << " ULS Database: " << filename;
		int linenum;
		// char *chptr;
		std::string str;
		std::string reasonIgnored;
		// std::size_t strpos;

		bool fixAnomalousEntries = false;

		int numIgnoreInvalid = 0;
		int numIgnoreOutOfBand = 0;
		int numIgnoreOutsideSimulationRegion = 0;
		int numIgnoreMobile = 0;
		int numFixed = 0;
		int numValid = 0;

		int fsid = -1;
		std::string callsign;
		int pathNumber;
		std::string radioService;
		std::string entityName;
		std::string rxCallsign;
		int rxAntennaNumber;
		int numPR;
		std::string frequencyAssigned;
		double startFreq, stopFreq;
		double rxLatitudeDeg, rxLongitudeDeg;
		double rxGroundElevation;
		double rxHeightAboveTerrain;
		double txLatitudeDeg, txLongitudeDeg;
		double txGroundElevation;
		std::string txPolarization;
		double txHeightAboveTerrain;
		double azimuthAngleToTx;
		double elevationAngleToTx;
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

		if (filename.empty()) {
			throw std::runtime_error("ERROR: No ULS data file specified");
		}

		int numUrbanULS = 0;
		int numSuburbanULS = 0;
		int numRuralULS = 0;
		int numBarrenULS = 0;

		LOGGER_INFO(logger) << "Analysis Band: [" << minFreq << ", " << maxFreq << "]";

		linenum = 0;

		std::vector<UlsRecord> rows = std::vector<UlsRecord>();
		if (_analysisType == "ExclusionZoneAnalysis") {
			// can also use UlsDatabase::getFSById(QString, int) to get a single record
			// only by Id
			ulsDatabase->loadFSById(QString::fromStdString(filename),
						_deniedRegionList,
						_antennaList,
						rows,
						_exclusionZoneFSID);
		} else if ((_analysisType == "HeatmapAnalysis") && (_heatmapFSID != -1)) {
			ulsDatabase->loadFSById(QString::fromStdString(filename),
						_deniedRegionList,
						_antennaList,
						rows,
						_heatmapFSID);
			if (rows.size() == 0) {
				LOGGER_WARN(logger) << "GENERAL FAILURE: invalid FSID specified "
						       "for heatmap analysis: "
						    << _heatmapFSID;
				_responseCode = CConst::generalFailureResponseCode;
				return;
			}
		} else {
			ulsDatabase->loadUlsData(QString::fromStdString(filename),
						 _deniedRegionList,
						 _antennaList,
						 rows,
						 minLat,
						 maxLat,
						 minLon,
						 maxLon);
		}
		// Distributing FS TX by 1x1 degree squares to minimize GDAL reopening
		std::sort(rows.begin(), rows.end(), [](const UlsRecord &rl, const UlsRecord &rr) {
			if (std::isnan(rl.txLatitudeDeg) || std::isnan(rl.txLongitudeDeg)) {
				return false;
			} else if (std::isnan(rr.txLatitudeDeg) || std::isnan(rr.txLongitudeDeg)) {
				return true;
			} else {
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
			}
		});

		for (UlsRecord row : rows) {
			linenum++;
			bool ignoreFlag = false;
			bool fixedFlag = false;
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
			/* callsign, rxCallsign                                                   */
			/**************************************************************************/
			callsign = row.callsign;
			rxCallsign = row.rxCallsign;
			pathNumber = row.pathNumber;
			/**************************************************************************/

			/**************************************************************************/
			/* rxAntennaNumber                                                        */
			/**************************************************************************/
			rxAntennaNumber = row.rxAntennaNumber;
			/**************************************************************************/

			/**************************************************************************/
			/* frequencyAssigned => startFreq, stopFreq                               */
			/**************************************************************************/

			if (!_filterSimRegionOnly) {
				if (!ignoreFlag) {
					startFreq = row.startFreq * 1.0e6;
					stopFreq = row.stopFreq * 1.0e6;

					if (stopFreq < startFreq) {
						throw std::runtime_error(
							ErrStream()
							<< "ERROR reading ULS data: FSID = " << fsid
							<< ", startFreq = " << startFreq
							<< ", stopFreq = " << stopFreq
							<< ", must have startFreq < stopFreq");
					}
				}

				if (!ignoreFlag) {
					if ((stopFreq <= minFreq) || (startFreq >= maxFreq)) {
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
			if ((!_filterSimRegionOnly) && (removeMobileFlag) && (!ignoreFlag)) {
				if ((row.mobile || (radioService == "TP") ||
				     ((startFreq < 6525.0e6) && (stopFreq > 6425.0e6)))) {
					ignoreFlag = true;
					reasonIgnored = "Mobile ULS entry";
					numIgnoreMobile++;
				}
			}
			/**************************************************************************/

			/**************************************************************************/
			/* rxLatCoords => rxLatitudeDeg                                           */
			/**************************************************************************/
			if (!ignoreFlag) {
				rxLatitudeDeg = row.rxLatitudeDeg;

				if (rxLatitudeDeg == 0.0) {
					if ((linkDirection == 0) || (linkDirection == 2)) {
						ignoreFlag = true;
						reasonIgnored = "RX Latitude has value 0";
						numIgnoreInvalid++;
					} else if (linkDirection == 1) {
						reasonIgnored = "Ignored: Rx Latitude has value 0";
						ignoreFlag = true;
						numIgnoreInvalid++;
					} else {
						throw std::runtime_error(ErrStream()
									 << "ERROR reading ULS "
									    "data: linkDirection = "
									 << linkDirection
									 << " INVALID value");
					}
				}
			}
			/**************************************************************************/

			/**************************************************************************/
			/* rxLongCoords => rxLongitudeDeg                                         */
			/**************************************************************************/
			if (!ignoreFlag) {
				rxLongitudeDeg = row.rxLongitudeDeg;

				if (rxLongitudeDeg == 0.0) {
					if ((linkDirection == 0) || (linkDirection == 2)) {
						ignoreFlag = true;
						reasonIgnored = "RX Longitude has value 0";
					} else if (linkDirection == 1) {
						reasonIgnored = "Ignored: Rx Longitude has value 0";
						ignoreFlag = true;
						numIgnoreInvalid++;
					}
					numIgnoreInvalid++;
				}
			}
			/**************************************************************************/

			/**************************************************************************/
			/* Check rxLatitude and rxLongitude region defined by popGrid (SIMULATION
			 * REGION)     */
			/**************************************************************************/
			if ((!ignoreFlag) && ((linkDirection == 0) || (linkDirection == 2)) &&
			    (popGridVal)) {
				int lonIdx;
				int latIdx;
				int regionIdx;
				popGridVal->findDeg(rxLongitudeDeg,
						    rxLatitudeDeg,
						    lonIdx,
						    latIdx,
						    rxPropEnv,
						    regionIdx);
				if ((rxPropEnv == 0) || (rxPropEnv == 'X')) {
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
			if (!_filterSimRegionOnly) {
				rxHeightAboveTerrain = row.rxHeightAboveTerrain;
				if (!ignoreFlag) {
					if (std::isnan(rxHeightAboveTerrain)) {
						bool fixedMissingRxHeight = false;
						if (fixAnomalousEntries) {
							if (!std::isnan(row.txHeightAboveTerrain)) {
								txHeightAboveTerrain =
									row.txHeightAboveTerrain;
								if (txHeightAboveTerrain > 0.0) {
									rxHeightAboveTerrain =
										row.txHeightAboveTerrain;
									fixedStr += "Fixed: "
										    "missing Rx "
										    "Height above "
										    "Terrain set "
										    "to Tx Height "
										    "above Terrain";
									fixedMissingRxHeight = true;
									fixedFlag = true;
								} else if (txHeightAboveTerrain ==
									   0.0) {
									rxHeightAboveTerrain = 0.1;
									fixedStr +=
										"Fixed: missing Rx "
										"Height above "
										"Terrain set to " +
										std::to_string(
											rxHeightAboveTerrain);
									fixedMissingRxHeight = true;
									fixedFlag = true;
								}
							} else {
								if (radioService == "CF") {
									rxHeightAboveTerrain = 39.3;
									fixedStr +=
										"Fixed: missing Rx "
										"Height above "
										"Terrain for " +
										radioService +
										" set to " +
										std::to_string(
											rxHeightAboveTerrain);
									fixedMissingRxHeight = true;
									fixedFlag = true;
								} else if (radioService == "MG") {
									rxHeightAboveTerrain = 41.0;
									fixedStr +=
										"Fixed: missing Rx "
										"Height above "
										"Terrain for " +
										radioService +
										" set to " +
										std::to_string(
											rxHeightAboveTerrain);
									fixedMissingRxHeight = true;
									fixedFlag = true;
								} else if (radioService == "MW") {
									rxHeightAboveTerrain = 39.9;
									fixedStr +=
										"Fixed: missing Rx "
										"Height above "
										"Terrain for " +
										radioService +
										" set to " +
										std::to_string(
											rxHeightAboveTerrain);
									fixedMissingRxHeight = true;
									fixedFlag = true;
								} else if (radioService == "TI") {
									rxHeightAboveTerrain = 41.8;
									fixedStr +=
										"Fixed: missing Rx "
										"Height above "
										"Terrain for " +
										radioService +
										" set to " +
										std::to_string(
											rxHeightAboveTerrain);
									fixedMissingRxHeight = true;
									fixedFlag = true;
								} else if (radioService == "TP") {
									rxHeightAboveTerrain = 30.0;
									fixedStr +=
										"Fixed: missing Rx "
										"Height above "
										"Terrain for " +
										radioService +
										" set to " +
										std::to_string(
											rxHeightAboveTerrain);
									fixedMissingRxHeight = true;
									fixedFlag = true;
								} else if (radioService == "TS") {
									rxHeightAboveTerrain = 41.5;
									fixedStr +=
										"Fixed: missing Rx "
										"Height above "
										"Terrain for " +
										radioService +
										" set to " +
										std::to_string(
											rxHeightAboveTerrain);
									fixedMissingRxHeight = true;
									fixedFlag = true;
								} else if (radioService == "TT") {
									rxHeightAboveTerrain = 42.1;
									fixedStr +=
										"Fixed: missing Rx "
										"Height above "
										"Terrain for " +
										radioService +
										" set to " +
										std::to_string(
											rxHeightAboveTerrain);
									fixedMissingRxHeight = true;
									fixedFlag = true;
								}
							}
						}

						if (!fixedMissingRxHeight) {
							ignoreFlag = true;
							reasonIgnored = "missing Rx Height above "
									"Terrain";
							numIgnoreInvalid++;
						}
					}
				}

				if (!ignoreFlag) {
					if (rxHeightAboveTerrain < 3.0) {
						if (fixAnomalousEntries) {
							rxHeightAboveTerrain = 3.0;
							fixedStr += "Fixed: Rx Height above "
								    "Terrain < 3.0 set to 3.0";
							fixedFlag = true;
						} else {
							LOGGER_WARN(logger)
								<< "WARNING: ULS data for FSID = "
								<< fsid
								<< ", rxHeightAboveTerrain = "
								<< rxHeightAboveTerrain
								<< " is < 3.0";
						}
					}
				}
			}
			/**************************************************************************/

			/**************************************************************************/
			/* txLatCoords => txLatitudeDeg                                           */
			/**************************************************************************/
			txLatitudeDeg = row.txLatitudeDeg;
			/**************************************************************************/

			/**************************************************************************/
			/* txLongCoords => txLongitudeDeg                                         */
			/**************************************************************************/
			txLongitudeDeg = row.txLongitudeDeg;
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
			txHeightAboveTerrain = row.txHeightAboveTerrain;
			/**************************************************************************/

			/**************************************************************************/
			/* txLocFlag: true if the TX location is known, and false otherwise.      */
			/* Note that Cadada does not provode TX location.                         */
			/**************************************************************************/
			bool txLocFlag = !(std::isnan(txLongitudeDeg) ||
					   std::isnan(txLatitudeDeg) ||
					   std::isnan(txHeightAboveTerrain));
			/**************************************************************************/

			/**************************************************************************/
			/* Check txLatitude and txLongitude region defined by popGrid (SIMULATION
			 * REGION)     */
			/**************************************************************************/
			if (txLocFlag && (!ignoreFlag) &&
			    ((linkDirection == 1) || (linkDirection == 2)) && popGridVal) {
				int lonIdx;
				int latIdx;
				int regionIdx;
				popGridVal->findDeg(txLongitudeDeg,
						    txLatitudeDeg,
						    lonIdx,
						    latIdx,
						    txPropEnv,
						    regionIdx);
				if ((txPropEnv == 0) || (txPropEnv == 'X')) {
					ignoreFlag = true;
					reasonIgnored = "TX outside SIMULATION REGION";
					numIgnoreOutsideSimulationRegion++;
				}
			}
			/**************************************************************************/

			/**************************************************************************/
			/* azimuthAngleToTx, elevationAngleToTx                                   */
			/**************************************************************************/
			azimuthAngleToTx = row.azimuthAngleToTx;
			elevationAngleToTx = row.elevationAngleToTx;
			/**************************************************************************/

			/**************************************************************************/
			/* txPtgFlag: true if pointing to TX location is known, false otherwise.  */
			/* Note that Cadada specifies these parameters, US does not.              */
			/**************************************************************************/
			bool txPtgFlag = !(std::isnan(azimuthAngleToTx) ||
					   std::isnan(elevationAngleToTx));
			/**************************************************************************/

			/**************************************************************************/
			/* If Tx location not provided and pointing to TX not provided, ignore    */
			/**************************************************************************/
			if ((!txLocFlag) && (!txPtgFlag)) {
				ignoreFlag = true;
				reasonIgnored = "TX location and pointing both not specified";
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
			if ((!_filterSimRegionOnly) && (!ignoreFlag)) {
				for (prIdx = 0; prIdx < numPR; ++prIdx) {
					/******************************************************************/
					/* prLongitudeDeg */
					/******************************************************************/
					if (std::isnan(row.prLongitudeDeg[prIdx]) ||
					    (row.prLongitudeDeg[prIdx] == 0.0)) {
						reasonIgnored = "Ignored: PR Longitude has value "
								"nan or 0";
						ignoreFlag = true;
						numIgnoreInvalid++;
					}
					/******************************************************************/

					/******************************************************************/
					/* prLatitudeDeg */
					/******************************************************************/
					if (std::isnan(row.prLatitudeDeg[prIdx]) ||
					    (row.prLatitudeDeg[prIdx] == 0.0)) {
						reasonIgnored = "Ignored: PR Latitude has value "
								"nan or 0";
						ignoreFlag = true;
						numIgnoreInvalid++;
					}
					/******************************************************************/

					/******************************************************************/
					/* prHeightAboveTerrain */
					/******************************************************************/
					if (std::isnan(row.prHeightAboveTerrainRx[prIdx])) {
						ignoreFlag = true;
						reasonIgnored = "missing PR Height above Terrain";
						numIgnoreInvalid++;
					}

					if (!ignoreFlag) {
						if (row.prHeightAboveTerrainRx[prIdx] <= 0.0) {
							LOGGER_WARN(logger)
								<< "WARNING: ULS data for FSID = "
								<< fsid << ", Passive Repeater "
								<< (prIdx + 1)
								<< ", prHeightAboveTerrainRx = "
								<< row.prHeightAboveTerrainRx[prIdx]
								<< " is < 0.0";
						}
					}

					if (std::isnan(row.prHeightAboveTerrainTx[prIdx])) {
						ignoreFlag = true;
						reasonIgnored = "missing PR Height above Terrain";
						numIgnoreInvalid++;
					}

					if (!ignoreFlag) {
						if (row.prHeightAboveTerrainTx[prIdx] <= 0.0) {
							LOGGER_WARN(logger)
								<< "WARNING: ULS data for FSID = "
								<< fsid << ", Passive Repeater "
								<< (prIdx + 1)
								<< ", prHeightAboveTerrainTx = "
								<< row.prHeightAboveTerrainTx[prIdx]
								<< " is < 0.0";
						}
					}
					/******************************************************************/
				}
			}
			/**************************************************************************/

			if (!_filterSimRegionOnly) {
				if ((linkDirection == 0) || (linkDirection == 2) ||
				    (simulationFlag == CConst::MobileSimulation)) {
					/**************************************************************************/
					/* rxGain */
					/**************************************************************************/
					rxGain = row.rxGain;
					if (!ignoreFlag) {
						if (std::isnan(rxGain)) {
							if (fixAnomalousEntries) {
								if (radioService == "CF") {
									rxGain = 39.3;
									fixedStr +=
										"Fixed: missing Rx "
										"Gain for " +
										radioService +
										" gain set to " +
										std::to_string(
											rxGain);
									fixedFlag = true;
								} else if (radioService == "MG") {
									rxGain = 41.0;
									fixedStr +=
										"Fixed: missing Rx "
										"Gain for " +
										radioService +
										" gain set to " +
										std::to_string(
											rxGain);
									fixedFlag = true;
								} else if (radioService == "MW") {
									rxGain = 39.9;
									fixedStr +=
										"Fixed: missing Rx "
										"Gain for " +
										radioService +
										" gain set to " +
										std::to_string(
											rxGain);
									fixedFlag = true;
								} else if (radioService == "TI") {
									rxGain = 41.8;
									fixedStr +=
										"Fixed: missing Rx "
										"Gain for " +
										radioService +
										" gain set to " +
										std::to_string(
											rxGain);
									fixedFlag = true;
								} else if (radioService == "TP") {
									rxGain = 30.0;
									fixedStr +=
										"Fixed: missing Rx "
										"Gain for " +
										radioService +
										" gain set to " +
										std::to_string(
											rxGain);
									fixedFlag = true;
								} else if (radioService == "TS") {
									rxGain = 41.5;
									fixedStr +=
										"Fixed: missing Rx "
										"Gain for " +
										radioService +
										" gain set to " +
										std::to_string(
											rxGain);
									fixedFlag = true;
								} else if (radioService == "TT") {
									rxGain = 42.1;
									fixedStr +=
										"Fixed: missing Rx "
										"Gain for " +
										radioService +
										" gain set to " +
										std::to_string(
											rxGain);
									fixedFlag = true;
								} else if (radioService == "TB") {
									rxGain = 40.7;
									fixedStr +=
										"Fixed: missing Rx "
										"Gain for " +
										radioService +
										" gain set to " +
										std::to_string(
											rxGain);
									fixedFlag = true;
								} else {
									ignoreFlag = true;
									reasonIgnored = "missing "
											"Rx Gain";
									numIgnoreInvalid++;
								}
							} else {
								ignoreFlag = true;
								reasonIgnored = "missing Rx Gain";
								numIgnoreInvalid++;
							}
						} else if ((callsign == "WQUY451") &&
							   (rxGain == 1.8)) {
							if (fixAnomalousEntries) {
								rxGain = 39.3;
								fixedStr += "Fixed: anomalous Rx "
									    "Gain for " +
									    callsign +
									    " changed from 1.8 "
									    "to " +
									    std::to_string(rxGain);
								fixedFlag = true;
							}
						}
					}

					if (!ignoreFlag) {
						if (rxGain < 10.0) {
							if (fixAnomalousEntries) {
								if (radioService == "CF") {
									rxGain = 39.3;
									fixedStr +=
										"Fixed: invalid Rx "
										"Gain " +
										std::to_string(
											rxGain) +
										" for " +
										radioService +
										" set to 39.3";
									fixedFlag = true;
								} else if (radioService == "MG") {
									rxGain = 41.0;
									fixedStr +=
										"Fixed: invalid Rx "
										"Gain " +
										std::to_string(
											rxGain) +
										" for " +
										radioService +
										" set to 41.0";
									fixedFlag = true;
								} else if (radioService == "MW") {
									rxGain = 39.9;
									fixedStr +=
										"Fixed: invalid Rx "
										"Gain " +
										std::to_string(
											rxGain) +
										" for " +
										radioService +
										" set to 39.9";
									fixedFlag = true;
								} else if (radioService == "TI") {
									rxGain = 41.8;
									fixedStr +=
										"Fixed: invalid Rx "
										"Gain " +
										std::to_string(
											rxGain) +
										" for " +
										radioService +
										" set to 41.8";
									fixedFlag = true;
								} else if (radioService == "TS") {
									rxGain = 41.5;
									fixedStr +=
										"Fixed: invalid Rx "
										"Gain " +
										std::to_string(
											rxGain) +
										" for " +
										radioService +
										" set to 41.5";
									fixedFlag = true;
								} else {
									ignoreFlag = true;
									reasonIgnored = "invalid "
											"Rx Gain";
									numIgnoreInvalid++;
								}
							} else {
								// DO NOTHING

								// ignoreFlag = true;
								// reasonIgnored = "invalid Rx
								// Gain"; numIgnoreInvalid++;
							}
						}
					}
					/**************************************************************************/

					/**************************************************************************/
					/* rxAntennaDiameter */
					/**************************************************************************/
					rxAntennaDiameter = row.rxAntennaDiameter;
					/**************************************************************************/

					/**************************************************************************/
					/* rxNearFieldAntDiameter; */
					/**************************************************************************/
					rxNearFieldAntDiameter = row.rxNearFieldAntDiameter;
					/**************************************************************************/

					/**************************************************************************/
					/* rxNearFieldDistLimit; */
					/**************************************************************************/
					rxNearFieldDistLimit = row.rxNearFieldDistLimit;
					/**************************************************************************/

					/**************************************************************************/
					/* rxNearFieldAntEfficiency; */
					/**************************************************************************/
					rxNearFieldAntEfficiency = row.rxNearFieldAntEfficiency;
					/**************************************************************************/

					/**************************************************************************/
					/* rxAntennaCategory */
					/**************************************************************************/
					rxAntennaCategory = row.rxAntennaCategory;
					/**************************************************************************/

					/**************************************************************************/
					/* rxAntenna */
					/**************************************************************************/
					if (!ignoreFlag) {
						rxAntenna = row.rxAntenna;
						if (rxAntenna) {
							LOGGER_DEBUG(logger)
								<< "Antenna Found " << fsid << ": "
								<< rxAntenna->get_strid();
							rxAntennaType = CConst::LUTAntennaType;
						} else {
							std::string strval = row.rxAntennaModelName;
							int validFlag;
							rxAntennaType =
								(CConst::ULSAntennaTypeEnum)CConst::
									strULSAntennaTypeList
										->str_to_type(
											strval,
											validFlag,
											0);
							if (!validFlag) {
								// std::ostringstream errStr;
								// errStr << "Invalid ULS data for
								// FSID = " << fsid
								// 	<< ", Unknown Rx Antenna \""
								// << strval
								// 	<< "\" using " <<
								// CConst::strULSAntennaTypeList->type_to_str(_ulsDefaultAntennaType);
								// LOGGER_WARN(logger) <<
								// errStr.str();
								// statusMessageList.push_back(errStr.str());

								rxAntennaType =
									_ulsDefaultAntennaType;
							}
						}
					}
					/**************************************************************************/

					/**************************************************************************/
					/* hasDiversity */
					/**************************************************************************/
					hasDiversity = row.hasDiversity;
					/**************************************************************************/

					if (hasDiversity) {
						/**********************************************************************/
						/* diversityHeightAboveTerrain */
						/**********************************************************************/
						diversityHeightAboveTerrain =
							row.diversityHeightAboveTerrain;
						if (!ignoreFlag) {
							if (std::isnan(
								    diversityHeightAboveTerrain)) {
								ignoreFlag = true;
								reasonIgnored = "missing Rx "
										"Diversity Height "
										"above Terrain";
								numIgnoreInvalid++;
							}
						}

						if (!ignoreFlag) {
							if (diversityHeightAboveTerrain < 3.0) {
								LOGGER_WARN(logger)
									<< "WARNING: ULS data for "
									   "FSID = "
									<< fsid
									<< ", "
									   "diversityHeightAboveTer"
									   "rain = "
									<< diversityHeightAboveTerrain
									<< " is < 3.0";
							}
						}
						/**********************************************************************/

						/**********************************************************************/
						/* diversityGain */
						/**********************************************************************/
						diversityGain = row.diversityGain;
						if (!ignoreFlag) {
							if (std::isnan(diversityGain)) {
								ignoreFlag = true;
								reasonIgnored = "missing Rx "
										"Diversity Gain";
								numIgnoreInvalid++;
							}
						}
						/**********************************************************************/

						/**********************************************************************/
						/* rxAntennaDiameter */
						/**********************************************************************/
						diversityAntennaDiameter =
							row.diversityAntennaDiameter;
						/**********************************************************************/
					}

					/**************************************************************************/
					/* fadeMargin */
					/**************************************************************************/

					// ULS Db does not have fadeMargin field so set this as
					// default
					fadeMarginDB = -1.0;

					/**************************************************************************/
				}
			}

			if (!_filterSimRegionOnly) {
				if ((linkDirection == 1) || (linkDirection == 2) ||
				    (simulationFlag == CConst::MobileSimulation) ||
				    (simulationFlag == CConst::RLANSensingSimulation) ||
				    (simulationFlag == CConst::showFSPwrAtRLANSimulation)) {
					/**************************************************************************/
					/* txGain */
					/**************************************************************************/
					txGain = row.txGain;
					if (!ignoreFlag) {
						if (std::isnan(txGain)) {
							if (fixAnomalousEntries) {
								if (radioService == "CF") {
									txGain = 39.3;
									fixedStr +=
										"Fixed: missing Tx "
										"Gain for " +
										radioService +
										" gain set to " +
										std::to_string(
											txGain);
									fixedFlag = true;
								} else {
									ignoreFlag = true;
									reasonIgnored = "missing "
											"Tx Gain";
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
					/* txEIRP */
					/**************************************************************************/
					txEIRP = row.txEIRP;
					if (!ignoreFlag) {
						if (std::isnan(txEIRP)) {
							if (fixAnomalousEntries &&
							    (radioService == "CF")) {
								txEIRP = 66;
								fixedStr += "Fixed: missing txEIRP "
									    "set to " +
									    std::to_string(txEIRP) +
									    " dBm";
								fixedFlag = true;
							} else {
								ignoreFlag = true;
								reasonIgnored = "missing Tx EIRP";
								numIgnoreInvalid++;
							}
						}
					}

					if (!ignoreFlag) {
						txEIRP = txEIRP - 30; // Convert dBm to dBW

						if (txEIRP >= 80.0) {
							if (fixAnomalousEntries) {
								txEIRP = 39.3;
								fixedStr += "Fixed: Tx EIRP > 80 "
									    "dBW set to 39.3 dBW";
								fixedFlag = true;
							} else {
								LOGGER_WARN(logger)
									<< "WARNING: ULS data for "
									   "FSID = "
									<< fsid
									<< ", txEIRP = " << txEIRP
									<< " (dBW) is >= 80.0";
							}
						}
					}
					/**************************************************************************/

					/**************************************************************************/
					/* txAntenna */
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
					for (int segIdx = 0; segIdx < numPR + 1; ++segIdx) {
						Vector3 txLon =
							(segIdx == 0 ?
								 txLongitudeDeg :
								 row.prLongitudeDeg[segIdx - 1]);
						Vector3 txLat =
							(segIdx == 0 ?
								 txLatitudeDeg :
								 row.prLatitudeDeg[segIdx - 1]);
						Vector3 rxLon =
							(segIdx == numPR ?
								 rxLongitudeDeg :
								 row.prLongitudeDeg[segIdx]);
						Vector3 rxLat = (segIdx == numPR ?
									 rxLatitudeDeg :
									 row.prLatitudeDeg[segIdx]);

						if ((rxLat == txLat) && (rxLon == txLon)) {
							reasonIgnored = "Ignored: RX and TX "
									"LON/LAT values are "
									"identical for segment " +
									std::to_string(segIdx);
							ignoreFlag = true;
							numIgnoreInvalid++;
						}
					}
				}

				if (!ignoreFlag) {
					if (rxGain > 80.0) {
						if (fixAnomalousEntries) {
							rxGain = 30.0;
							fixedStr += "Fixed: RX Gain > 80 dB: set "
								    "to 30 dB";
							fixedFlag = true;
						} else {
							LOGGER_WARN(logger)
								<< "WARNING: ULS data for FSID = "
								<< fsid << ", rxGain = " << rxGain
								<< " is > 80.0";
						}
					}
				}
			}

			if ((!ignoreFlag) && (!fixedFlag)) {
				numValid++;
			} else if ((!ignoreFlag) && (fixedFlag)) {
				numFixed++;
			}

			if (!ignoreFlag) {
				bool unii5Flag =
					computeSpectralOverlapLoss((double *)NULL,
								   startFreq,
								   stopFreq,
								   5925.0e6,
								   6425.0e6,
								   false,
								   CConst::psdSpectralAlgorithm);

				bool unii7Flag =
					computeSpectralOverlapLoss((double *)NULL,
								   startFreq,
								   stopFreq,
								   6525.0e6,
								   6875.0e6,
								   false,
								   CConst::psdSpectralAlgorithm);

				double rxAntennaFeederLossDB = row.rxLineLoss;
				if (std::isnan(rxAntennaFeederLossDB)) {
					// R2-AIP-10: set feeder loss according to txArchitecture
					// R2-AIP-10-CAN: for canada sim, set _rxFeederLossDBIDU =
					// _rxFeederLossDBODU = _rxFeederLossDBUnknown = 0.0
					if (row.txArchitecture == "IDU") {
						rxAntennaFeederLossDB = _rxFeederLossDBIDU;
					} else if (row.txArchitecture == "ODU") {
						rxAntennaFeederLossDB = _rxFeederLossDBODU;
					} else {
						rxAntennaFeederLossDB = _rxFeederLossDBUnknown;
					}
				}
				double noisePSD;
				double centerFreq = (startFreq + stopFreq) / 2;
				bool found = false;
				for (int i = 0; (i < (int)_noisePSDFreqList.size()) && (!found);
				     ++i) {
					if (centerFreq <= _noisePSDFreqList[i]) {
						noisePSD = _noisePSDList[i];
						found = true;
					}
				}
				if (!found) {
					noisePSD = _noisePSDList[_noisePSDFreqList.size()];
				}
				double antennaCenterFreq;
				if (row.region == "US") {
					if (unii5Flag && unii7Flag) {
						antennaCenterFreq = centerFreq;
					} else if (unii5Flag) {
						antennaCenterFreq = (CConst::unii5StartFreqMHz +
								     CConst::unii5StopFreqMHz) *
								    0.5e6;
					} else if (unii7Flag) {
						antennaCenterFreq = (CConst::unii7StartFreqMHz +
								     CConst::unii7StopFreqMHz) *
								    0.5e6;
					} else {
						antennaCenterFreq = centerFreq;
					}
				} else {
					antennaCenterFreq = centerFreq;
				}
				double lambda = CConst::c / antennaCenterFreq;
				double rxDlambda = rxAntennaDiameter / lambda;
				double noiseBandwidth = stopFreq - startFreq;

				uls = new ULSClass(this, fsid, dbIdx, numPR, row.region);
				_ulsList->append(uls);
				uls->setCallsign(callsign);
				uls->setPathNumber(pathNumber);
				uls->setRxCallsign(rxCallsign);
				uls->setRxAntennaNumber(rxAntennaNumber);
				uls->setRadioService(radioService);
				uls->setEntityName(entityName);
				uls->setStartFreq(startFreq);
				uls->setStopFreq(stopFreq);
				uls->setNoiseBandwidth(noiseBandwidth);
				uls->setRxGroundElevation(rxGroundElevation);
				uls->setRxLatitudeDeg(rxLatitudeDeg);
				uls->setRxLongitudeDeg(rxLongitudeDeg);
				uls->setTxGroundElevation(txGroundElevation);
				uls->setTxPolarization(txPolarization);
				uls->setTxLatitudeDeg(txLatitudeDeg);
				uls->setTxLongitudeDeg(txLongitudeDeg);
				uls->setAzimuthAngleToTx(azimuthAngleToTx);
				uls->setElevationAngleToTx(elevationAngleToTx);
				uls->setRxGain(rxGain);
				uls->setRxDlambda(rxDlambda);
				uls->setRxNearFieldAntDiameter(rxNearFieldAntDiameter);
				uls->setRxNearFieldDistLimit(rxNearFieldDistLimit);
				uls->setRxNearFieldAntEfficiency(rxNearFieldAntEfficiency);
				uls->setRxAntennaModel(row.rxAntennaModelName);
				uls->setRxAntennaType(rxAntennaType);
				uls->setTxAntennaType(txAntennaType);
				uls->setRxAntenna(rxAntenna);
				uls->setTxAntenna(txAntenna);
				uls->setRxAntennaCategory(rxAntennaCategory);
				uls->setTxGain(txGain);
				uls->setTxEIRP(txEIRP);
				uls->setRxAntennaFeederLossDB(rxAntennaFeederLossDB);
				uls->setFadeMarginDB(fadeMarginDB);
				uls->setStatus(status);

				uls->setHasDiversity(hasDiversity);
				if (hasDiversity) {
					uls->setDiversityGain(diversityGain);

					double diversityDlambda = diversityAntennaDiameter / lambda;
					uls->setDiversityDlambda(diversityDlambda);
				}

				if (simulationFlag == CConst::MobileSimulation) {
					throw std::invalid_argument("Mobile simulation not "
								    "supported");
				}

				bool rxTerrainHeightFlag, txTerrainHeightFlag;
				double terrainHeight;
				double bldgHeight;
				MultibandRasterClass::HeightResult lidarHeightResult;
				CConst::HeightSourceEnum rxHeightSource;
				CConst::HeightSourceEnum txHeightSource;
				CConst::HeightSourceEnum prHeightSource;
				Vector3 rxPosition, txPosition, prPosition, diversityPosition;

				if ((_terrainDataModel)) {
					_terrainDataModel->getTerrainHeight(rxLongitudeDeg,
									    rxLatitudeDeg,
									    terrainHeight,
									    bldgHeight,
									    lidarHeightResult,
									    rxHeightSource);
					rxTerrainHeightFlag = true;
				} else {
					rxTerrainHeightFlag = false;
					terrainHeight = 0.0;
				}
				double rxHeight = rxHeightAboveTerrain + terrainHeight;

				uls->setRxTerrainHeightFlag(rxTerrainHeightFlag);
				uls->setRxTerrainHeight(terrainHeight);
				uls->setRxHeightAboveTerrain(rxHeightAboveTerrain);
				uls->setRxHeightAMSL(rxHeight);
				uls->setRxHeightSource(rxHeightSource);

				rxPosition = EcefModel::geodeticToEcef(rxLatitudeDeg,
								       rxLongitudeDeg,
								       rxHeight / 1000.0);
				uls->setRxPosition(rxPosition);

				if (hasDiversity) {
					double diversityHeight = diversityHeightAboveTerrain +
								 terrainHeight;
					uls->setDiversityHeightAboveTerrain(
						diversityHeightAboveTerrain);
					uls->setDiversityHeightAMSL(diversityHeight);

					diversityPosition =
						EcefModel::geodeticToEcef(rxLatitudeDeg,
									  rxLongitudeDeg,
									  diversityHeight / 1000.0);
					uls->setDiversityPosition(diversityPosition);
				}

				if (txLocFlag) {
					if ((_terrainDataModel)) {
						_terrainDataModel->getTerrainHeight(
							txLongitudeDeg,
							txLatitudeDeg,
							terrainHeight,
							bldgHeight,
							lidarHeightResult,
							txHeightSource);
						txTerrainHeightFlag = true;
					} else {
						txTerrainHeightFlag = false;
						terrainHeight = 0.0;
					}
					double txHeight = txHeightAboveTerrain + terrainHeight;

					uls->setTxTerrainHeightFlag(txTerrainHeightFlag);
					uls->setTxTerrainHeight(terrainHeight);
					uls->setTxHeightAboveTerrain(txHeightAboveTerrain);
					uls->setTxHeightSource(txHeightSource);
					uls->setTxHeightAMSL(txHeight);

					txPosition = EcefModel::geodeticToEcef(txLatitudeDeg,
									       txLongitudeDeg,
									       txHeight / 1000.0);
					uls->setTxPosition(txPosition);
				} else {
					uls->setTxTerrainHeightFlag(true);
					uls->setTxTerrainHeight(quietNaN);
					uls->setTxHeightAboveTerrain(quietNaN);
					uls->setTxHeightSource(CConst::unknownHeightSource);
					uls->setTxHeightAMSL(quietNaN);
					Vector3 nanVector3 = Vector3(quietNaN, quietNaN, quietNaN);
					uls->setTxPosition(nanVector3);
				}

				for (prIdx = 0; prIdx < numPR; ++prIdx) {
					PRClass &pr = uls->getPR(prIdx);

					int validFlag;
					pr.type = (CConst::PRTypeEnum)CConst::strPRTypeList
							  ->str_to_type(row.prType[prIdx],
									validFlag,
									1);
					pr.longitudeDeg = row.prLongitudeDeg[prIdx];
					pr.latitudeDeg = row.prLatitudeDeg[prIdx];
					pr.heightAboveTerrainRx = row.prHeightAboveTerrainRx[prIdx];
					pr.heightAboveTerrainTx = row.prHeightAboveTerrainTx[prIdx];

					if ((_terrainDataModel)) {
						_terrainDataModel->getTerrainHeight(
							pr.longitudeDeg,
							pr.latitudeDeg,
							terrainHeight,
							bldgHeight,
							lidarHeightResult,
							prHeightSource);
						pr.terrainHeightFlag = true;
					} else {
						pr.terrainHeightFlag = false;
						terrainHeight = 0.0;
					}

					pr.terrainHeight = terrainHeight;
					pr.heightAMSLRx = pr.heightAboveTerrainRx +
							  pr.terrainHeight;
					pr.heightAMSLTx = pr.heightAboveTerrainTx +
							  pr.terrainHeight;
					pr.heightSource = prHeightSource;

					pr.positionRx = EcefModel::geodeticToEcef(pr.latitudeDeg,
										  pr.longitudeDeg,
										  pr.heightAMSLRx /
											  1000.0);
					pr.positionTx = EcefModel::geodeticToEcef(pr.latitudeDeg,
										  pr.longitudeDeg,
										  pr.heightAMSLTx /
											  1000.0);

					if (row.prType[prIdx] == "Ant") {
						pr.type = CConst::backToBackAntennaPRType;
						pr.txGain = row.prTxGain[prIdx];
						pr.txDlambda = row.prTxAntennaDiameter[prIdx] /
							       lambda;
						pr.rxGain = row.prRxGain[prIdx];
						pr.rxDlambda = row.prRxAntennaDiameter[prIdx] /
							       lambda;
						pr.antCategory = row.prAntCategory[prIdx];
						pr.antModel = row.prAntModelName[prIdx];

						/**************************************************************************/
						/* Passive Repeater Antenna Pattern */
						/**************************************************************************/
						if (!ignoreFlag) {
							pr.antenna = row.prAntenna[prIdx];
							if (pr.antenna) {
								LOGGER_DEBUG(logger)
									<< "Passive Repeater "
									   "Antenna Found "
									<< fsid << ": "
									<< pr.antenna->get_strid();
								pr.antennaType =
									CConst::LUTAntennaType;
							} else {
								std::string strval =
									row.prAntModelName[prIdx];
								pr.antennaType =
									(CConst::ULSAntennaTypeEnum)
										CConst::strULSAntennaTypeList
											->str_to_type(
												strval,
												validFlag,
												0);
								if (!validFlag) {
									// std::ostringstream
									// errStr; errStr <<
									// "Invalid ULS data for
									// FSID = " << fsid
									// 	<< ", Unknown
									// Passive Repeater Antenna
									// \"" << strval
									// 	<< "\" using " <<
									// CConst::strULSAntennaTypeList->type_to_str(_ulsDefaultAntennaType);
									// LOGGER_WARN(logger) <<
									// errStr.str();
									// statusMessageList.push_back(errStr.str());

									pr.antennaType =
										_ulsDefaultAntennaType;
								}
							}
						}
						/**************************************************************************/
					} else if (row.prType[prIdx] == "Ref") {
						pr.type = CConst::billboardReflectorPRType;
						pr.reflectorHeightLambda =
							row.prReflectorHeight[prIdx] / lambda;
						pr.reflectorWidthLambda =
							row.prReflectorWidth[prIdx] / lambda;

					} else {
						CORE_DUMP;
					}
				}

				for (int segIdx = 0; segIdx <= numPR; ++segIdx) {
					Vector3 segTxPosn =
						(segIdx == 0 ? txPosition :
							       uls->getPR(segIdx - 1).positionTx);
					Vector3 segRxPosn = (segIdx == numPR ?
								     rxPosition :
								     uls->getPR(segIdx).positionRx);
					Vector3 pointing;
					double segDist;

					if (txLocFlag || (segIdx > 0)) {
						pointing = (segTxPosn - segRxPosn).normalized();
						segDist = (segTxPosn - segRxPosn).len() * 1000.0;
					} else {
						Vector3 upVec = segRxPosn.normalized();
						Vector3 zVec = Vector3(0.0, 0.0, 1.0);
						Vector3 eastVec = zVec.cross(upVec).normalized();
						Vector3 northVec = upVec.cross(eastVec);

						double ca = cos(row.azimuthAngleToTx * M_PI /
								180.0);
						double sa = sin(row.azimuthAngleToTx * M_PI /
								180.0);
						double ce = cos(row.elevationAngleToTx * M_PI /
								180.0);
						double se = sin(row.elevationAngleToTx * M_PI /
								180.0);

						pointing = northVec * ca * ce + eastVec * sa * ce +
							   upVec * se;
						segDist = -1.0;
					}

					if (segIdx == numPR) {
						uls->setAntennaPointing(
							pointing); // Pointing of Rx antenna
						uls->setLinkDistance(segDist);
						if (hasDiversity) {
							pointing = (segTxPosn - diversityPosition)
									   .normalized();
							uls->setDiversityAntennaPointing(
								pointing); // Pointing of Rx
									   // Diversity antenna
						}
					} else {
						uls->getPR(segIdx).pointing =
							pointing; // Pointing of Passive Receiver
						uls->getPR(segIdx).segmentDistance = segDist;
					}
				}

				/******************************************************************/
				/* Calculate PR parameters:                                       */
				/*     Orthonormal basis                                          */
				/*     thetaIN                                                    */
				/*     alphaAZ                                                    */
				/*     alphaEL                                                    */
				/******************************************************************/
				for (prIdx = numPR - 1; prIdx >= 0; prIdx--) {
					PRClass &pr = uls->getPR(prIdx);
					double nextSegDist =
						((prIdx == numPR - 1) ?
							 uls->getLinkDistance() :
							 uls->getPR(prIdx + 1).segmentDistance);
					double nextSegFSPL = 20.0 *
							     log((4 * M_PI * centerFreq *
								  nextSegDist) /
								 CConst::c) /
							     log(10.0);

					if (pr.type == CConst::backToBackAntennaPRType) {
						pr.pathSegGain = pr.rxGain + pr.txGain -
								 nextSegFSPL;
					} else if (pr.type == CConst::billboardReflectorPRType) {
						Vector3 pointingA = pr.pointing;
						Vector3 pointingB = -(
							prIdx == numPR - 1 ?
								uls->getAntennaPointing() :
								uls->getPR(prIdx + 1).pointing);
						pr.reflectorZ =
							(pointingA + pointingB)
								.normalized(); // Perpendicular to
									       // reflector surface
						Vector3 upVec = pr.positionRx.normalized();
						pr.reflectorX = (upVec.cross(pr.reflectorZ))
									.normalized(); // Horizontal
						pr.reflectorY = (pr.reflectorZ.cross(pr.reflectorX))
									.normalized();

						double Ax = pointingA.dot(pr.reflectorX);
						double Ay = pointingA.dot(pr.reflectorY);
						double Az = pointingA.dot(pr.reflectorZ);

						double cosThetaIN = pointingA.dot(pr.reflectorZ);

						// Spec was changed from:
						// s = if ((alphaEL <= alphaAZ))
						// reflectorWidthLambda*cosThetaIN else
						// pr.reflectorHeightLambda*cosThetaIN to s =
						// MAX(reflectorWidthLambda,
						// reflectorHeightLambda)*cosThetaIN

						bool conditionW;
						if (0) {
							// previous spec
							double alphaAZ = (180.0 / M_PI) *
									 fabs(atan(Ax / Az));
							double alphaEL = (180.0 / M_PI) *
									 fabs(atan(Ay / Az));
							conditionW = (alphaEL <= alphaAZ);
						} else {
							// current spec
							conditionW = (pr.reflectorWidthLambda >=
								      pr.reflectorHeightLambda);
						}

						if (conditionW) {
							pr.reflectorSLambda =
								pr.reflectorWidthLambda *
								cosThetaIN;
						} else {
							pr.reflectorSLambda =
								pr.reflectorHeightLambda *
								cosThetaIN;
						}
						pr.reflectorTheta1 =
							(180.0 / M_PI) *
							asin(1.0 / (2 * pr.reflectorSLambda));
						double Ks = 4 * pr.reflectorWidthLambda *
							    pr.reflectorHeightLambda * cosThetaIN *
							    lambda / (M_PI * nextSegDist);
						double alpha_n = 20 * log10(M_PI * Ks / 4);

						double Q = -1.0;
						if ((Ks <= 0.4) ||
						    ((prIdx < numPR - 1) &&
						     (uls->getPR(prIdx + 1).type ==
						      CConst::billboardReflectorPRType))) {
							pr.pathSegGain = std::min(3.0, alpha_n);
						} else {
							double nextDlambda =
								((prIdx == numPR - 1) ?
									 uls->getRxDlambda() :
									 uls->getPR(prIdx + 1)
										 .rxDlambda);
							Q = nextDlambda *
							    sqrt(M_PI /
								 (4 * pr.reflectorWidthLambda *
								  pr.reflectorHeightLambda *
								  cosThetaIN));
							pr.pathSegGain =
								_prTable->computePRTABLE(Q,
											 1.0 / Ks);
						}

						pr.reflectorThetaIN = acos(cosThetaIN) * 180.0 /
								      M_PI;
						pr.reflectorKS = Ks;
						pr.reflectorQ = Q;
					}
					pr.effectiveGain =
						(prIdx == numPR - 1 ?
							 uls->getRxGain() :
							 uls->getPR(prIdx + 1).effectiveGain) +
						pr.pathSegGain;
				}
				/******************************************************************/

				double noiseLevelDBW = noisePSD + 10.0 * log10(noiseBandwidth);
				uls->setNoiseLevelDBW(noiseLevelDBW);
				if (fixedFlag && anomGc) {
					anomGc.fsid = fsid;
					anomGc.dbName = name;
					anomGc.callsign = callsign;
					anomGc.rxLatitudeDeg = rxLatitudeDeg;
					anomGc.rxLongitudeDeg = rxLongitudeDeg;
					anomGc.anomaly = fixedStr;
					anomGc.completeRow();
				}
			} else if (anomGc) {
				anomGc.fsid = fsid;
				anomGc.dbName = name;
				anomGc.callsign = callsign;
				anomGc.rxLatitudeDeg = rxLatitudeDeg;
				anomGc.rxLongitudeDeg = rxLongitudeDeg;
				anomGc.anomaly = reasonIgnored;
				anomGc.completeRow();
			}
		}

		LOGGER_INFO(logger) << "TOTAL NUM VALID ULS: " << numValid;
		LOGGER_INFO(logger) << "TOTAL NUM IGNORE ULS (invalid data):" << numIgnoreInvalid;
		LOGGER_INFO(logger) << "TOTAL NUM IGNORE ULS (out of band): " << numIgnoreOutOfBand;
		LOGGER_INFO(logger) << "TOTAL NUM IGNORE ULS (out of SIMULATION REGION): "
				    << numIgnoreOutsideSimulationRegion;
		LOGGER_INFO(logger) << "TOTAL NUM IGNORE ULS (Mobile): " << numIgnoreMobile;
		LOGGER_INFO(logger) << "TOTAL NUM FIXED ULS: " << numFixed;
		LOGGER_INFO(logger) << "TOTAL NUM VALID ULS IN SIMULATION (VALID + FIXED): "
				    << _ulsList->getSize();
		if (linkDirection == 0) {
			LOGGER_INFO(logger)
				<< "NUM URBAN ULS: " << numUrbanULS << " = "
				<< ((double)numUrbanULS / _ulsList->getSize()) * 100.0 << " %";
			LOGGER_INFO(logger)
				<< "NUM SUBURBAN ULS: " << numSuburbanULS << " = "
				<< ((double)numSuburbanULS / _ulsList->getSize()) * 100.0 << " %";
			LOGGER_INFO(logger)
				<< "NUM RURAL ULS: " << numRuralULS << " = "
				<< ((double)numRuralULS / _ulsList->getSize()) * 100.0 << " %";
			LOGGER_INFO(logger)
				<< "NUM BARREN ULS: " << numBarrenULS << " = "
				<< ((double)numBarrenULS / _ulsList->getSize()) * 100.0 << " %";
		}

		if (_filterSimRegionOnly) {
			exit(1);
		}

		delete ulsDatabase;
	}
	if (_terrainDataModel) {
		_terrainDataModel->setGdalDirectMode(prevGdalDirectMode);
	}

	// _ulsList is expected to be sorted by ID (used in findULSID() )
	_ulsList->sort([](ULSClass *const &l, ULSClass *const &r) {
		return l->getID() < r->getID();
	});

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** AfcManager::readDeniedRegionData()                                               ****/
/******************************************************************************************/
void AfcManager::readDeniedRegionData(std::string filename)
{
	if (filename.empty()) {
		LOGGER_INFO(logger) << "No denied region file specified";
		return;
	}

	int linenum, fIdx;
	std::string line, strval;
	char *chptr;
	FILE *fp = (FILE *)NULL;
	std::string str;
	std::string reasonIgnored;
	std::ostringstream errStr;

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

	std::vector<int *> fieldIdxList;
	std::vector<std::string> fieldLabelList;
	fieldIdxList.push_back(&startFreqFieldIdx);
	fieldLabelList.push_back("Start Freq (MHz)");
	fieldIdxList.push_back(&stopFreqFieldIdx);
	fieldLabelList.push_back("Stop Freq (MHz)");
	fieldIdxList.push_back(&stopFreqFieldIdx);
	fieldLabelList.push_back("End Freq (MHz)");
	fieldIdxList.push_back(&exclusionZoneTypeFieldIdx);
	fieldLabelList.push_back("Exclusion Zone");

	fieldIdxList.push_back(&lat1Rect1FieldIdx);
	fieldLabelList.push_back("Rectangle1 Lat 1");
	fieldIdxList.push_back(&lat2Rect1FieldIdx);
	fieldLabelList.push_back("Rectangle1 Lat 2");
	fieldIdxList.push_back(&lon1Rect1FieldIdx);
	fieldLabelList.push_back("Rectangle1 Lon 1");
	fieldIdxList.push_back(&lon2Rect1FieldIdx);
	fieldLabelList.push_back("Rectangle1 Lon 2");

	fieldIdxList.push_back(&lat1Rect2FieldIdx);
	fieldLabelList.push_back("Rectangle2 Lat 1");
	fieldIdxList.push_back(&lat2Rect2FieldIdx);
	fieldLabelList.push_back("Rectangle2 Lat 2");
	fieldIdxList.push_back(&lon1Rect2FieldIdx);
	fieldLabelList.push_back("Rectangle2 Lon 1");
	fieldIdxList.push_back(&lon2Rect2FieldIdx);
	fieldLabelList.push_back("Rectangle2 Lon 2");

	fieldIdxList.push_back(&radiusFieldIdx);
	fieldLabelList.push_back("Circle Radius (km)");
	fieldIdxList.push_back(&latCircleFieldIdx);
	fieldLabelList.push_back("Circle center Lat");
	fieldIdxList.push_back(&lonCircleFieldIdx);
	fieldLabelList.push_back("Circle center Lon");

	fieldIdxList.push_back(&heightAGLFieldIdx);
	fieldLabelList.push_back("Antenna AGL height (m)");

	int drid;
	double startFreq, stopFreq;
	DeniedRegionClass::GeometryEnum exclusionZoneType;
	double lat1Rect1, lat2Rect1, lon1Rect1, lon2Rect1;
	double lat1Rect2, lat2Rect2, lon1Rect2, lon2Rect2;
	double radius;
	double latCircle, lonCircle;
	bool horizonDistFlag;
	double heightAGL;

	int fieldIdx;

	LOGGER_INFO(logger) << "Reading denied region Datafile: " << filename;

	if (!(fp = fopen(filename.c_str(), "rb"))) {
		str = std::string("ERROR: Unable to open Denied Region Data File \"") + filename +
		      std::string("\"\n");
		throw std::runtime_error(str);
	}

	enum LineTypeEnum { labelLineType, dataLineType, ignoreLineType, unknownLineType };

	LineTypeEnum lineType;

	DeniedRegionClass *dr;

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
			if (fIdx == (int)std::string::npos) {
				if (fieldList.size() == 1) {
					lineType = ignoreLineType;
				}
			} else {
				if (fieldList[0].at(fIdx) == '#') {
					lineType = ignoreLineType;
				}
			}
		}

		if ((lineType == unknownLineType) && (!foundLabelLine)) {
			lineType = labelLineType;
			foundLabelLine = 1;
		}
		if ((lineType == unknownLineType) && (foundLabelLine)) {
			lineType = dataLineType;
		}
		/**************************************************************************/

		/**************************************************************************/
		/**** Process Line                                                     ****/
		/**************************************************************************/
		bool found;
		std::string field;
		switch (lineType) {
			case labelLineType:
				for (fieldIdx = 0; fieldIdx < (int)fieldList.size(); fieldIdx++) {
					field = fieldList.at(fieldIdx);

					// std::cout << "FIELD: \"" << field << "\"" << std::endl;

					found = false;
					for (fIdx = 0;
					     (fIdx < (int)fieldLabelList.size()) && (!found);
					     fIdx++) {
						if (field == fieldLabelList.at(fIdx)) {
							*fieldIdxList.at(fIdx) = fieldIdx;
							found = true;
						}
					}
				}

				for (fIdx = 0; fIdx < (int)fieldIdxList.size(); fIdx++) {
					if (*fieldIdxList.at(fIdx) == -1) {
						errStr << "ERROR: Invalid Denied Region Data file "
							  "\""
						       << filename << "\" label line missing \""
						       << fieldLabelList.at(fIdx) << "\""
						       << std::endl;
						throw std::runtime_error(errStr.str());
					}
				}

				break;
			case dataLineType:
				/**************************************************************************/
				/* startFreq */
				/**************************************************************************/
				strval = fieldList.at(startFreqFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid Denied Region Data file \""
					       << filename << "\" line " << linenum
					       << " missing Start Freq" << std::endl;
					throw std::runtime_error(errStr.str());
				}
				startFreq = std::strtod(strval.c_str(), &chptr) *
					    1.0e6; // Convert MHz to Hz
				/**************************************************************************/

				/**************************************************************************/
				/* stopFreq */
				/**************************************************************************/
				strval = fieldList.at(stopFreqFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid Denied Region Data file \""
					       << filename << "\" line " << linenum
					       << " missing Stop Freq" << std::endl;
					throw std::runtime_error(errStr.str());
				}
				stopFreq = std::strtod(strval.c_str(), &chptr) *
					   1.0e6; // Convert MHz to Hz
				/**************************************************************************/

				/**************************************************************************/
				/* exclusionZoneType */
				/**************************************************************************/
				strval = fieldList.at(exclusionZoneTypeFieldIdx);

				if (strval == "One Rectangle") {
					exclusionZoneType = DeniedRegionClass::rectGeometry;
				} else if (strval == "Two Rectangles") {
					exclusionZoneType = DeniedRegionClass::rect2Geometry;
				} else if (strval == "Circle") {
					exclusionZoneType = DeniedRegionClass::circleGeometry;
				} else if (strval == "Horizon Distance") {
					exclusionZoneType = DeniedRegionClass::horizonDistGeometry;
				} else {
					errStr << "ERROR: Invalid Denied Region Data file \""
					       << filename << "\" line " << linenum
					       << " exclusion zone set to unrecognized value "
					       << strval << std::endl;
					throw std::runtime_error(errStr.str());
				}
				/**************************************************************************/

				drid = (_deniedRegionList.size() ?
						_deniedRegionList[_deniedRegionList.size() - 1]
								->getID() +
							1 :
						0);

				switch (exclusionZoneType) {
					case DeniedRegionClass::rectGeometry:
					case DeniedRegionClass::rect2Geometry:
						dr = (DeniedRegionClass *)new RectDeniedRegionClass(
							drid);

						strval = fieldList.at(lat1Rect1FieldIdx);
						lat1Rect1 = getAngleFromDMS(strval);
						strval = fieldList.at(lat2Rect1FieldIdx);
						lat2Rect1 = getAngleFromDMS(strval);
						strval = fieldList.at(lon1Rect1FieldIdx);
						lon1Rect1 = getAngleFromDMS(strval);
						strval = fieldList.at(lon2Rect1FieldIdx);
						lon2Rect1 = getAngleFromDMS(strval);
						((RectDeniedRegionClass *)dr)
							->addRect(lon1Rect1,
								  lon2Rect1,
								  lat1Rect1,
								  lat2Rect1);

						if (exclusionZoneType ==
						    DeniedRegionClass::rect2Geometry) {
							strval = fieldList.at(lat1Rect2FieldIdx);
							lat1Rect2 = getAngleFromDMS(strval);
							strval = fieldList.at(lat2Rect2FieldIdx);
							lat2Rect2 = getAngleFromDMS(strval);
							strval = fieldList.at(lon1Rect2FieldIdx);
							lon1Rect2 = getAngleFromDMS(strval);
							strval = fieldList.at(lon2Rect2FieldIdx);
							lon2Rect2 = getAngleFromDMS(strval);
							((RectDeniedRegionClass *)dr)
								->addRect(lon1Rect2,
									  lon2Rect2,
									  lat1Rect2,
									  lat2Rect2);
						}
						break;
					case DeniedRegionClass::circleGeometry:
					case DeniedRegionClass::horizonDistGeometry:
						strval = fieldList.at(lonCircleFieldIdx);
						lonCircle = getAngleFromDMS(strval);
						strval = fieldList.at(latCircleFieldIdx);
						latCircle = getAngleFromDMS(strval);

						horizonDistFlag =
							(exclusionZoneType ==
							 DeniedRegionClass::horizonDistGeometry);

						dr = (DeniedRegionClass *)new CircleDeniedRegionClass(
							drid,
							horizonDistFlag);

						((CircleDeniedRegionClass *)dr)
							->setLongitudeCenter(lonCircle);
						((CircleDeniedRegionClass *)dr)
							->setLatitudeCenter(latCircle);

						if (!horizonDistFlag) {
							strval = fieldList.at(radiusFieldIdx);
							if (strval.empty()) {
								errStr << "ERROR: Invalid Denied "
									  "Region Data file \""
								       << filename << "\" line "
								       << linenum
								       << " missing Circle Radius"
								       << std::endl;
								throw std::runtime_error(
									errStr.str());
							}
							radius = std::strtod(strval.c_str(),
									     &chptr) *
								 1.0e3; // Convert km to m
							((CircleDeniedRegionClass *)dr)
								->setRadius(radius);
						} else {
							/**************************************************************************/
							/* heightAGL */
							/**************************************************************************/
							strval = fieldList.at(heightAGLFieldIdx);
							if (strval.empty()) {
								errStr << "ERROR: Invalid Denied "
									  "Region Data file \""
								       << filename << "\" line "
								       << linenum
								       << " missing Antenna AGL "
									  "Height"
								       << std::endl;
								throw std::runtime_error(
									errStr.str());
							}
							heightAGL = std::strtod(strval.c_str(),
										&chptr);
							dr->setHeightAGL(heightAGL);
							/**************************************************************************/
						}

						break;
					default:
						CORE_DUMP;
						break;
				}

				dr->setStartFreq(startFreq);
				dr->setStopFreq(stopFreq);
				dr->setType(DeniedRegionClass::userSpecifiedType);
				_deniedRegionList.push_back(dr);

				break;
			case ignoreLineType:
			case unknownLineType:
				// do nothing
				break;
			default:
				CORE_DUMP;
				break;
		}
	}

	if (fp) {
		fclose(fp);
	}

	LOGGER_INFO(logger) << "TOTAL NUM DENIED REGION: " << _deniedRegionList.size();

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
			_terrainDataModel->getTerrainHeight(uls->getRxLongitudeDeg(),
							    uls->getRxLatitudeDeg(),
							    terrainHeight,
							    bldgHeight,
							    lidarHeightResult,
							    heightSource);
			updateFlag = true;
			uls->setRxTerrainHeightFlag(true);
			double rxHeight = uls->getRxHeightAboveTerrain() + terrainHeight;
			Vector3 rxPosition = EcefModel::geodeticToEcef(uls->getRxLatitudeDeg(),
								       uls->getRxLongitudeDeg(),
								       rxHeight / 1000.0);
			uls->setRxPosition(rxPosition);
			uls->setRxTerrainHeight(terrainHeight);
			uls->setRxHeightAMSL(rxHeight);
			uls->setRxHeightSource(heightSource);
		}
		if (!uls->getTxTerrainHeightFlag()) {
			_terrainDataModel->getTerrainHeight(uls->getTxLongitudeDeg(),
							    uls->getTxLatitudeDeg(),
							    terrainHeight,
							    bldgHeight,
							    lidarHeightResult,
							    heightSource);
			updateFlag = true;
			uls->setTxTerrainHeightFlag(true);
			double txHeight = uls->getTxHeightAboveTerrain() + terrainHeight;
			Vector3 txPosition = EcefModel::geodeticToEcef(uls->getTxLatitudeDeg(),
								       uls->getTxLongitudeDeg(),
								       txHeight / 1000.0);
			uls->setTxPosition(txPosition);
			uls->setTxTerrainHeight(terrainHeight);
			uls->setTxHeightAMSL(txHeight);
			uls->setTxHeightSource(heightSource);
		}
		for (int prIdx = 0; prIdx < uls->getNumPR(); ++prIdx) {
			PRClass &pr = uls->getPR(prIdx);

			if (!pr.terrainHeightFlag) {
				_terrainDataModel->getTerrainHeight(pr.longitudeDeg,
								    pr.latitudeDeg,
								    terrainHeight,
								    bldgHeight,
								    lidarHeightResult,
								    heightSource);
				updateFlag = true;
				pr.terrainHeightFlag = true;

				double prHeightRx = pr.heightAboveTerrainRx + terrainHeight;
				double prHeightTx = pr.heightAboveTerrainTx + terrainHeight;
				pr.positionRx = EcefModel::geodeticToEcef(pr.latitudeDeg,
									  pr.longitudeDeg,
									  prHeightRx / 1000.0);
				pr.positionTx = EcefModel::geodeticToEcef(pr.latitudeDeg,
									  pr.longitudeDeg,
									  prHeightTx / 1000.0);
				pr.terrainHeight = terrainHeight;
				pr.heightAMSLRx = prHeightRx;
				pr.heightAMSLTx = prHeightTx;
				pr.heightSource = heightSource;
			}
		}

		if (updateFlag) {
			for (int segIdx = 0; segIdx < uls->getNumPR() + 1; ++segIdx) {
				Vector3 segTxPosn = (segIdx == 0 ?
							     uls->getTxPosition() :
							     uls->getPR(segIdx - 1).positionTx);
				Vector3 segRxPosn = (segIdx == uls->getNumPR() ?
							     uls->getRxPosition() :
							     uls->getPR(segIdx).positionRx);

				Vector3 pointing =
					(segTxPosn - segRxPosn)
						.normalized(); // boresight pointing direction of RX
				double segDist = (segTxPosn - segRxPosn).len() * 1000.0;

				bool txLocFlag = (!std::isnan(segTxPosn.x())) &&
						 (!std::isnan(segTxPosn.y())) &&
						 (!std::isnan(segTxPosn.z()));

				if (txLocFlag) {
					pointing = (segTxPosn - segRxPosn).normalized();
					segDist = (segTxPosn - segRxPosn).len() * 1000.0;
				} else {
					double azimuthAngleToTx = uls->getAzimuthAngleToTx();
					double elevationAngleToTx = uls->getElevationAngleToTx();

					Vector3 upVec = segRxPosn.normalized();
					Vector3 zVec = Vector3(0.0, 0.0, 1.0);
					Vector3 eastVec = zVec.cross(upVec).normalized();
					Vector3 northVec = upVec.cross(eastVec);

					double ca = cos(azimuthAngleToTx * M_PI / 180.0);
					double sa = sin(azimuthAngleToTx * M_PI / 180.0);
					double ce = cos(elevationAngleToTx * M_PI / 180.0);
					double se = sin(elevationAngleToTx * M_PI / 180.0);

					pointing = northVec * ca * ce + eastVec * sa * ce +
						   upVec * se;
					segDist = -1.0;
				}

				if (segIdx == uls->getNumPR()) {
					uls->setAntennaPointing(pointing); // Pointing of Rx antenna
					uls->setLinkDistance(segDist);
				} else {
					uls->getPR(segIdx).pointing = pointing;
					uls->getPR(segIdx).segmentDistance = segDist;
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
double AfcManager::computeBuildingPenetration(CConst::BuildingTypeEnum buildingType,
					      double elevationAngleDeg,
					      double frequency,
					      std::string &buildingPenetrationModelStr,
					      double &buildingPenetrationCDF) const
{
	double r, s, t, u, v, w, x, y, z;
	double A, B, C;
	double mA, sA, mB, sB;

	if (_fixedBuildingLossFlag) {
		buildingPenetrationModelStr = "FIXED VALUE";
		buildingPenetrationCDF = 0.5;
		return (_fixedBuildingLossValue);
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
	} else if (buildingType == CConst::thermallyEfficientBuildingType) {
		r = 28.19;
		s = -3.00;
		t = 8.48;
		u = 13.5;
		v = 3.8;
		w = 27.8;
		x = -2.9;
		y = 9.4;
		z = -2.1;
	} else {
		throw std::runtime_error("ERROR in computeBuildingPenetration(), Invalid building "
					 "type");
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
	gauss[0] = _zbldg2109;

	A = gauss[0] * sA + mA;
	B = gauss[0] * sB + mB;
	C = -3.0;

	double lossDB = 10.0 *
			log(exp(A * log(10.0) / 10.0) + exp(B * log(10.0) / 10.0) +
			    exp(C * log(10.0) / 10.0)) /
			log(10.0);
	buildingPenetrationCDF = q(-gauss[0]);

	return (lossDB);
}
/******************************************************************************************/

/******************************************************************************************/
/**** AfcManager::computePathLoss                                                      ****/
/******************************************************************************************/
void AfcManager::computePathLoss(CConst::PathLossModelEnum pathLossModel,
				 bool itmFSPLFlag,
				 CConst::PropEnvEnum propEnv,
				 CConst::PropEnvEnum propEnvRx,
				 CConst::NLCDLandCatEnum nlcdLandCatTx,
				 CConst::NLCDLandCatEnum nlcdLandCatRx,
				 double distKm,
				 double fsplDistKm,
				 double win2DistKm,
				 double frequency,
				 double txLongitudeDeg,
				 double txLatitudeDeg,
				 double txHeightM,
				 double elevationAngleTxDeg,
				 double rxLongitudeDeg,
				 double rxLatitudeDeg,
				 double rxHeightM,
				 double elevationAngleRxDeg,
				 double &pathLoss,
				 double &pathClutterTxDB,
				 double &pathClutterRxDB,
				 std::string &pathLossModelStr,
				 double &pathLossCDF,
				 std::string &pathClutterTxModelStr,
				 double &pathClutterTxCDF,
				 std::string &pathClutterRxModelStr,
				 double &pathClutterRxCDF,
				 std::string *txClutterStrPtr,
				 std::string *rxClutterStrPtr,
				 double **ITMProfilePtr,
				 double **isLOSProfilePtr,
				 double *isLOSSurfaceFracPtr
#if DEBUG_AFC
				 ,
				 std::vector<std::string> &ITMHeightType
#endif
) const
{
	double frequencyGHz = frequency * 1.0e-9;

	if (txClutterStrPtr) {
		*txClutterStrPtr = "";
	}

	if (rxClutterStrPtr) {
		*rxClutterStrPtr = "";
	}

	pathLossModelStr = "";
	pathClutterTxModelStr = "";
	pathClutterTxCDF = -1.0;
	pathClutterRxModelStr = "";
	pathClutterRxCDF = -1.0;

	if (pathLossModel == CConst::ITMBldgPathLossModel) {
		if ((propEnv == CConst::urbanPropEnv) || (propEnv == CConst::suburbanPropEnv)) {
			if (win2DistKm * 1000 < _closeInDist) {
				if (_closeInPathLossModel == "WINNER2") {
					int winner2LOSValue =
						0; // 1: Force LOS, 2: Force NLOS, 0: Compute
						   // probLOS, then select or combine.
					if (win2DistKm * 1000 <= 50.0) {
						winner2LOSValue = 1;
					} else if ((_winner2LOSOption ==
						    CConst::BldgDataLOSOption) ||
						   (_winner2LOSOption ==
						    CConst::BldgDataReqTxLOSOption) ||
						   (_winner2LOSOption ==
						    CConst::BldgDataReqRxLOSOption) ||
						   (_winner2LOSOption ==
						    CConst::BldgDataReqTxRxLOSOption) ||
						   (_winner2LOSOption == CConst::CdsmLOSOption)) {
						double terrainHeight;
						double bldgHeight;
						MultibandRasterClass::HeightResult
							lidarHeightResult;
						CConst::HeightSourceEnum txHeightSource,
							rxHeightSource;
						_terrainDataModel->getTerrainHeight(
							txLongitudeDeg,
							txLatitudeDeg,
							terrainHeight,
							bldgHeight,
							lidarHeightResult,
							txHeightSource,
							false);
						_terrainDataModel->getTerrainHeight(
							rxLongitudeDeg,
							rxLatitudeDeg,
							terrainHeight,
							bldgHeight,
							lidarHeightResult,
							rxHeightSource,
							false);

						bool reqTx = (_winner2LOSOption ==
							      CConst::BldgDataReqTxLOSOption) ||
							     (_winner2LOSOption ==
							      CConst::BldgDataReqTxRxLOSOption);
						bool reqRx = (_winner2LOSOption ==
							      CConst::BldgDataReqRxLOSOption) ||
							     (_winner2LOSOption ==
							      CConst::BldgDataReqTxRxLOSOption);

						if (((!reqTx) || (txHeightSource ==
								  CConst::lidarHeightSource)) &&
						    ((!reqRx) || (rxHeightSource ==
								  CConst::lidarHeightSource))) {
							int numPts = std::min(
								((int)floor(distKm * 1000 /
									    _itmMinSpacing)) +
									1,
								_itmMaxNumPts);
							bool losFlag =
								UlsMeasurementAnalysis::isLOS(
									_terrainDataModel,
									QPointF(txLatitudeDeg,
										txLongitudeDeg),
									txHeightM,
									QPointF(rxLatitudeDeg,
										rxLongitudeDeg),
									rxHeightM,
									distKm,
									numPts,
									isLOSProfilePtr,
									isLOSSurfaceFracPtr);
							if (losFlag) {
								if ((_winner2LOSOption ==
								     CConst::CdsmLOSOption) &&
								    (*isLOSSurfaceFracPtr <
								     _cdsmLOSThr)) {
									winner2LOSValue = 0;
								} else {
									winner2LOSValue = 1;
								}
							} else {
								winner2LOSValue = 2;
							}
						}
					} else if (_winner2LOSOption == CConst::ForceLOSLOSOption) {
						winner2LOSValue = 1;
					} else if (_winner2LOSOption ==
						   CConst::ForceNLOSLOSOption) {
						winner2LOSValue = 2;
					}

					double sigma, probLOS;
					if (propEnv == CConst::urbanPropEnv) {
						// Winner2 C2: urban
						pathLoss = Winner2_C2urban(1000 * win2DistKm,
									   rxHeightM,
									   txHeightM,
									   frequency,
									   sigma,
									   pathLossModelStr,
									   pathLossCDF,
									   probLOS,
									   winner2LOSValue);
					} else if (propEnv == CConst::suburbanPropEnv) {
						// Winner2 C1: suburban
						pathLoss = Winner2_C1suburban(1000 * win2DistKm,
									      rxHeightM,
									      txHeightM,
									      frequency,
									      sigma,
									      pathLossModelStr,
									      pathLossCDF,
									      probLOS,
									      winner2LOSValue);
					}
				} else {
					throw std::runtime_error(ErrStream()
								 << "ERROR: Invalid close in path "
								    "loss model = "
								 << _closeInPathLossModel);
				}
			} else if (itmFSPLFlag) {
				pathLoss = 20.0 *
					   log((4 * M_PI * frequency * fsplDistKm * 1000) /
					       CConst::c) /
					   log(10.0);
				pathLossModelStr = "FSPL";
				pathLossCDF = 0.5;
			} else {
				// Terrain propagation: Terrain + ITM
				double frequencyMHz = 1.0e-6 * frequency;
				// std::cerr << "PATHLOSS," << txLatitudeDeg << "," <<
				// txLongitudeDeg << "," << rxLatitudeDeg << "," << rxLongitudeDeg
				// << std::endl;
				int numPts = std::min(((int)floor(distKm * 1000 / _itmMinSpacing)) +
							      1,
						      _itmMaxNumPts);

				int radioClimate = _ituData->getRadioClimateValue(txLatitudeDeg,
										  txLongitudeDeg);
				int radioClimateTmp =
					_ituData->getRadioClimateValue(rxLatitudeDeg,
								       rxLongitudeDeg);
				if (radioClimateTmp < radioClimate) {
					radioClimate = radioClimateTmp;
				}
				double surfaceRefractivity = _ituData->getSurfaceRefractivityValue(
					(txLatitudeDeg + rxLatitudeDeg) / 2,
					(txLongitudeDeg + rxLongitudeDeg) / 2);

				/******************************************************************************************/
				/**** NOTE: ITM based on signal loss, so higher confidence
				 * corresponds to higher loss. ****/
				/******************************************************************************************/
				double u = _confidenceITM;

				pathLoss = UlsMeasurementAnalysis::runPointToPoint(
					_terrainDataModel,
					true,
					QPointF(txLatitudeDeg, txLongitudeDeg),
					txHeightM,
					QPointF(rxLatitudeDeg, rxLongitudeDeg),
					rxHeightM,
					distKm,
					_itmEpsDielect,
					_itmSgmConductivity,
					surfaceRefractivity,
					frequencyMHz,
					radioClimate,
					_itmPolarization,
					u,
					_reliabilityITM,
					numPts,
					NULL,
					ITMProfilePtr);
				pathLossModelStr = "ITM_BLDG";
				pathLossCDF = _confidenceITM;
			}
		} else if ((propEnv == CConst::ruralPropEnv) ||
			   (propEnv == CConst::barrenPropEnv)) {
			if (itmFSPLFlag) {
				pathLoss = 20.0 *
					   log((4 * M_PI * frequency * fsplDistKm * 1000) /
					       CConst::c) /
					   log(10.0);
				pathLossModelStr = "FSPL";
				pathLossCDF = 0.5;
			} else {
				// Terrain propagation: Terrain + ITM
				double frequencyMHz = 1.0e-6 * frequency;
				int numPts = std::min(((int)floor(distKm * 1000 / _itmMinSpacing)) +
							      1,
						      _itmMaxNumPts);
				int radioClimate = _ituData->getRadioClimateValue(txLatitudeDeg,
										  txLongitudeDeg);
				int radioClimateTmp =
					_ituData->getRadioClimateValue(rxLatitudeDeg,
								       rxLongitudeDeg);
				if (radioClimateTmp < radioClimate) {
					radioClimate = radioClimateTmp;
				}
				double surfaceRefractivity = _ituData->getSurfaceRefractivityValue(
					(txLatitudeDeg + rxLatitudeDeg) / 2,
					(txLongitudeDeg + rxLongitudeDeg) / 2);
				double u = _confidenceITM;
				pathLoss = UlsMeasurementAnalysis::runPointToPoint(
					_terrainDataModel,
					true,
					QPointF(txLatitudeDeg, txLongitudeDeg),
					txHeightM,
					QPointF(rxLatitudeDeg, rxLongitudeDeg),
					rxHeightM,
					distKm,
					_itmEpsDielect,
					_itmSgmConductivity,
					surfaceRefractivity,
					frequencyMHz,
					radioClimate,
					_itmPolarization,
					u,
					_reliabilityITM,
					numPts,
					NULL,
					ITMProfilePtr);
				pathLossModelStr = "ITM_BLDG";
				pathLossCDF = _confidenceITM;

				pathLossModelStr = "ITM_BLDG";
				pathLossCDF = _confidenceITM;
			}
		} else {
			throw std::runtime_error(ErrStream() << "ERROR reading ULS data: propEnv = "
							     << propEnv << " INVALID value");
		}
		pathClutterTxDB = 0.0;
		pathClutterTxModelStr = "NONE";
		pathClutterTxCDF = 0.5;
		pathClutterRxDB = 0.0;
		pathClutterRxModelStr = "NONE";
		pathClutterRxCDF = 0.5;
	} else if (pathLossModel == CConst::CoalitionOpt6PathLossModel) {
#if 1
		// As of 2021.12.03 this path loss model is no longer supported for AFC.
		throw std::runtime_error(ErrStream()
					 << "ERROR: unsupported path loss model selected");
#else
		// Path Loss Model selected for 6 GHz Coalition
		// Option 6 in PropagationModelOptions Winner-II 20171004.pptx

		if ((propEnv == CConst::urbanPropEnv) || (propEnv == CConst::suburbanPropEnv)) {
			if (distKm * 1000 < _closeInDist) {
				if (_closeInPathLossModel == "WINNER2") {
					int winner2LOSValue =
						0; // 1: Force LOS, 2: Force NLOS, 0: Compute
						   // probLOS, then select or combine.
					if (distKm * 1000 <= 50.0) {
						winner2LOSValue = 1;
					} else if (_winner2LOSOption == CConst::BldgDataLOSOption) {
						double terrainHeight;
						double bldgHeight;
						MultibandRasterClass::HeightResult
							lidarHeightResult;
						CConst::HeightSourceEnum txHeightSource,
							rxHeightSource;
						_terrainDataModel->getTerrainHeight(
							txLongitudeDeg,
							txLatitudeDeg,
							terrainHeight,
							bldgHeight,
							lidarHeightResult,
							txHeightSource);
						_terrainDataModel->getTerrainHeight(
							rxLongitudeDeg,
							rxLatitudeDeg,
							terrainHeight,
							bldgHeight,
							lidarHeightResult,
							rxHeightSource);

						if ((txHeightSource == CConst::lidarHeightSource) &&
						    (rxHeightSource == CConst::lidarHeightSource)) {
							int numPts = std::min(
								((int)floor(distKm * 1000 /
									    _itmMinSpacing)) +
									1,
								_itmMaxNumPts);
							bool losFlag =
								UlsMeasurementAnalysis::isLOS(
									_terrainDataModel,
									QPointF(txLatitudeDeg,
										txLongitudeDeg),
									txHeightM,
									QPointF(rxLatitudeDeg,
										rxLongitudeDeg),
									rxHeightM,
									distKm,
									numPts,
									isLOSProfilePtr);
							winner2LOSValue = (losFlag ? 1 : 2);
						}
					} else if (_winner2LOSOption == CConst::ForceLOSLOSOption) {
						winner2LOSValue = 1;
					} else if (_winner2LOSOption ==
						   CConst::ForceNLOSLOSOption) {
						winner2LOSValue = 2;
					}

					double sigma, probLOS;
					if (propEnv == CConst::urbanPropEnv) {
						// Winner2 C2: urban
						pathLoss = Winner2_C2urban(1000 * distKm,
									   rxHeightM,
									   txHeightM,
									   frequency,
									   sigma,
									   pathLossModelStr,
									   pathLossCDF,
									   probLOS,
									   winner2LOSValue);
					} else if (propEnv == CConst::suburbanPropEnv) {
						// Winner2 C1: suburban
						pathLoss = Winner2_C1suburban(1000 * distKm,
									      rxHeightM,
									      txHeightM,
									      frequency,
									      sigma,
									      pathLossModelStr,
									      pathLossCDF,
									      probLOS,
									      winner2LOSValue);
					}
				} else {
					throw std::runtime_error(ErrStream()
								 << "ERROR: Invalid close in path "
								    "loss model = "
								 << _closeInPathLossModel);
				}
				pathClutterTxDB = 0.0;
				pathClutterTxModelStr = "NONE";
				pathClutterTxCDF = 0.5;
			} else {
				if (itmFSPLFlag) {
					pathLoss = 20.0 *
						   log((4 * M_PI * frequency * fsplDistKm * 1000) /
						       CConst::c) /
						   log(10.0);
					pathLossModelStr = "FSPL";
					pathLossCDF = 0.5;
				} else {
					// Terrain propagation: Terrain + ITM
					double frequencyMHz = 1.0e-6 * frequency;
					// std::cerr << "PATHLOSS," << txLatitudeDeg << "," <<
					// txLongitudeDeg << "," << rxLatitudeDeg << "," <<
					// rxLongitudeDeg << std::endl;
					int numPts = std::min(
						((int)floor(distKm * 1000 / _itmMinSpacing)) + 1,
						_itmMaxNumPts);
					int radioClimate =
						_ituData->getRadioClimateValue(txLatitudeDeg,
									       txLongitudeDeg);
					int radioClimateTmp =
						_ituData->getRadioClimateValue(rxLatitudeDeg,
									       rxLongitudeDeg);
					if (radioClimateTmp < radioClimate) {
						radioClimate = radioClimateTmp;
					}
					double surfaceRefractivity =
						_ituData->getSurfaceRefractivityValue(
							(txLatitudeDeg + rxLatitudeDeg) / 2,
							(txLongitudeDeg + rxLongitudeDeg) / 2);

					/******************************************************************************************/
					/**** NOTE: ITM based on signal loss, so higher confidence
					 * corresponds to higher loss. ****/
					/******************************************************************************************/
					double u = _confidenceITM;

					pathLoss = UlsMeasurementAnalysis::runPointToPoint(
						_terrainDataModel,
						false,
						QPointF(txLatitudeDeg, txLongitudeDeg),
						txHeightM,
						QPointF(rxLatitudeDeg, rxLongitudeDeg),
						rxHeightM,
						distKm,
						_itmEpsDielect,
						_itmSgmConductivity,
						surfaceRefractivity,
						frequencyMHz,
						radioClimate,
						_itmPolarization,
						u,
						_reliabilityITM,
						numPts,
						NULL,
						ITMProfilePtr);
					pathLossModelStr = "ITM";
					pathLossCDF = _confidenceITM;
				}

				// ITU-R P.[CLUTTER] sec 3.2
				double Ll = 23.5 + 9.6 * log(frequencyGHz) / log(10.0);
				double Ls = 32.98 + 23.9 * log(distKm) / log(10.0) +
					    3.0 * log(frequencyGHz) / log(10.0);

				arma::vec gauss(1);
				gauss[0] = _zclutter2108;

				double Lctt = -5.0 *
						      log(exp(-0.2 * Ll * log(10.0)) +
							  exp(-0.2 * Ls * log(10.0))) /
						      log(10.0) +
					      6.0 * gauss[0];
				pathClutterTxDB = Lctt;

				pathClutterTxModelStr = "P.2108";
				pathClutterTxCDF = q(-gauss[0]);
				if (_applyClutterFSRxFlag && (rxHeightM <= 10.0) &&
				    (distKm >= 1.0)) {
					pathClutterRxDB = pathClutterTxDB;
					pathClutterRxModelStr = pathClutterTxModelStr;
					pathClutterRxCDF = pathClutterTxCDF;
				} else {
					pathClutterRxDB = 0.0;
					pathClutterRxModelStr = "NONE";
					pathClutterRxCDF = 0.5;
				}
			}
		} else if ((propEnv == CConst::ruralPropEnv) ||
			   (propEnv == CConst::barrenPropEnv)) {
			if (itmFSPLFlag) {
				pathLoss = 20.0 *
					   log((4 * M_PI * frequency * fsplDistKm * 1000) /
					       CConst::c) /
					   log(10.0);
				pathLossModelStr = "FSPL";
				pathLossCDF = 0.5;
			} else {
				// Terrain propagation: Terrain + ITM
				double frequencyMHz = 1.0e-6 * frequency;
				int numPts = std::min(((int)floor(distKm * 1000 / _itmMinSpacing)) +
							      1,
						      _itmMaxNumPts);
				int radioClimate = _ituData->getRadioClimateValue(txLatitudeDeg,
										  txLongitudeDeg);
				int radioClimateTmp =
					_ituData->getRadioClimateValue(rxLatitudeDeg,
								       rxLongitudeDeg);
				if (radioClimateTmp < radioClimate) {
					radioClimate = radioClimateTmp;
				}
				double surfaceRefractivity = _ituData->getSurfaceRefractivityValue(
					(txLatitudeDeg + rxLatitudeDeg) / 2,
					(txLongitudeDeg + rxLongitudeDeg) / 2);
				double u = _confidenceITM;
				pathLoss = UlsMeasurementAnalysis::runPointToPoint(
					_terrainDataModel,
					false,
					QPointF(txLatitudeDeg, txLongitudeDeg),
					txHeightM,
					QPointF(rxLatitudeDeg, rxLongitudeDeg),
					rxHeightM,
					distKm,
					_itmEpsDielect,
					_itmSgmConductivity,
					surfaceRefractivity,
					frequencyMHz,
					radioClimate,
					_itmPolarization,
					u,
					_reliabilityITM,
					numPts,
					NULL,
					ITMProfilePtr);
				pathLossModelStr = "ITM";
				pathLossCDF = _confidenceITM;
			}

			// ITU-R p.452 Clutter loss function
			pathClutterTxDB = AfcManager::computeClutter452HtEl(txHeightM,
									    distKm,
									    elevationAngleTxDeg);
			pathClutterTxModelStr = "452_HT_ELANG";
			pathClutterTxCDF = 0.5;

			if (_applyClutterFSRxFlag && (rxHeightM <= 10.0) && (distKm >= 1.0)) {
				pathClutterRxDB =
					AfcManager::computeClutter452HtEl(rxHeightM,
									  distKm,
									  elevationAngleRxDeg);
				pathClutterRxModelStr = "452_HT_ELANG";
				pathClutterRxCDF = 0.5;
			} else {
				pathClutterRxDB = 0.0;
				pathClutterRxModelStr = "NONE";
				pathClutterRxCDF = 0.5;
			}
		} else {
			throw std::runtime_error(ErrStream() << "ERROR: propEnv = " << propEnv
							     << " INVALID value");
		}
#endif
	} else if (pathLossModel == CConst::FCC6GHzReportAndOrderPathLossModel) {
		// Path Loss Model used in FCC Report and Order

		if (fsplDistKm * 1000 < 30.0) {
			pathLoss = 20.0 *
				   log((4 * M_PI * frequency * fsplDistKm * 1000) / CConst::c) /
				   log(10.0);
			pathLossModelStr = "FSPL";
			pathLossCDF = 0.5;

			pathClutterTxDB = 0.0;
			pathClutterTxModelStr = "NONE";
			pathClutterTxCDF = 0.5;
		} else if (win2DistKm * 1000 < _closeInDist) {
			int winner2LOSValue = 0; // 1: Force LOS, 2: Force NLOS, 0: Compute probLOS,
						 // then select or combine.
			if (win2DistKm * 1000 <= 50.0) {
				winner2LOSValue = 1;
			} else if ((_winner2LOSOption == CConst::BldgDataLOSOption) ||
				   (_winner2LOSOption == CConst::BldgDataReqTxLOSOption) ||
				   (_winner2LOSOption == CConst::BldgDataReqRxLOSOption) ||
				   (_winner2LOSOption == CConst::BldgDataReqTxRxLOSOption) ||
				   (_winner2LOSOption == CConst::CdsmLOSOption)) {
				double terrainHeight;
				double bldgHeight;
				MultibandRasterClass::HeightResult lidarHeightResult;
				CConst::HeightSourceEnum txHeightSource, rxHeightSource;
				_terrainDataModel->getTerrainHeight(txLongitudeDeg,
								    txLatitudeDeg,
								    terrainHeight,
								    bldgHeight,
								    lidarHeightResult,
								    txHeightSource);
				_terrainDataModel->getTerrainHeight(rxLongitudeDeg,
								    rxLatitudeDeg,
								    terrainHeight,
								    bldgHeight,
								    lidarHeightResult,
								    rxHeightSource);

				bool reqTx = (_winner2LOSOption ==
					      CConst::BldgDataReqTxLOSOption) ||
					     (_winner2LOSOption ==
					      CConst::BldgDataReqTxRxLOSOption);
				bool reqRx = (_winner2LOSOption ==
					      CConst::BldgDataReqRxLOSOption) ||
					     (_winner2LOSOption ==
					      CConst::BldgDataReqTxRxLOSOption);

				if (((!reqTx) || (txHeightSource == CConst::lidarHeightSource)) &&
				    ((!reqRx) || (rxHeightSource == CConst::lidarHeightSource))) {
					int numPts = std::min(
						((int)floor(distKm * 1000 / _itmMinSpacing)) + 1,
						_itmMaxNumPts);
					bool losFlag = UlsMeasurementAnalysis::isLOS(
						_terrainDataModel,
						QPointF(txLatitudeDeg, txLongitudeDeg),
						txHeightM,
						QPointF(rxLatitudeDeg, rxLongitudeDeg),
						rxHeightM,
						distKm,
						numPts,
						isLOSProfilePtr,
						isLOSSurfaceFracPtr);
					if (losFlag) {
						if ((_winner2LOSOption == CConst::CdsmLOSOption) &&
						    (*isLOSSurfaceFracPtr < _cdsmLOSThr)) {
							winner2LOSValue = 0;
						} else {
							winner2LOSValue = 1;
						}
					} else {
						winner2LOSValue = 2;
					}
				}
			} else if (_winner2LOSOption == CConst::ForceLOSLOSOption) {
				winner2LOSValue = 1;
			} else if (_winner2LOSOption == CConst::ForceNLOSLOSOption) {
				winner2LOSValue = 2;
			}

			double sigma, probLOS;
			if (propEnv == CConst::urbanPropEnv) {
				// Winner2 C2: urban
				pathLoss = Winner2_C2urban(1000 * win2DistKm,
							   rxHeightM,
							   txHeightM,
							   frequency,
							   sigma,
							   pathLossModelStr,
							   pathLossCDF,
							   probLOS,
							   winner2LOSValue);
			} else if (propEnv == CConst::suburbanPropEnv) {
				// Winner2 C1: suburban
				pathLoss = Winner2_C1suburban(1000 * win2DistKm,
							      rxHeightM,
							      txHeightM,
							      frequency,
							      sigma,
							      pathLossModelStr,
							      pathLossCDF,
							      probLOS,
							      winner2LOSValue);
			} else if ((propEnv == CConst::ruralPropEnv) ||
				   (propEnv == CConst::barrenPropEnv)) {
				// Winner2 D1: rural
				pathLoss = Winner2_D1rural(1000 * win2DistKm,
							   rxHeightM,
							   txHeightM,
							   frequency,
							   sigma,
							   pathLossModelStr,
							   pathLossCDF,
							   probLOS,
							   winner2LOSValue);
			} else {
				throw std::runtime_error(ErrStream()
							 << "ERROR: propEnv = " << propEnv
							 << " INVALID value");
			}
			if (_winner2LOSOption == CConst::CdsmLOSOption) {
				pathLossModelStr += " cdsmFrac = " +
						    std::to_string(*isLOSSurfaceFracPtr);
			}
			pathClutterTxModelStr = "NONE";
			pathClutterTxDB = 0.0;
			pathClutterTxCDF = 0.5;
		} else {
			bool rlanHasClutter;
			switch (_rlanITMTxClutterMethod) {
				case CConst::ForceTrueITMClutterMethod:
					rlanHasClutter = true;
					break;
				case CConst::ForceFalseITMClutterMethod:
					rlanHasClutter = false;
					break;
				case CConst::BldgDataITMCLutterMethod: {
					int numPts = std::min(
						((int)floor(distKm * 1000 / _itmMinSpacing)) + 1,
						_itmMaxNumPts);
					bool losFlag = UlsMeasurementAnalysis::isLOS(
						_terrainDataModel,
						QPointF(txLatitudeDeg, txLongitudeDeg),
						txHeightM,
						QPointF(rxLatitudeDeg, rxLongitudeDeg),
						rxHeightM,
						distKm,
						numPts,
						isLOSProfilePtr,
						isLOSSurfaceFracPtr);
					rlanHasClutter = !losFlag;
				} break;
			}

			if ((propEnv == CConst::urbanPropEnv) ||
			    (propEnv == CConst::suburbanPropEnv)) {
				if (itmFSPLFlag) {
					pathLoss = 20.0 *
						   log((4 * M_PI * frequency * fsplDistKm * 1000) /
						       CConst::c) /
						   log(10.0);
					pathLossModelStr = "FSPL";
					pathLossCDF = 0.5;
				} else {
					// Terrain propagation: SRTM + ITM
					double frequencyMHz = 1.0e-6 * frequency;
					int numPts = std::min(
						((int)floor(distKm * 1000 / _itmMinSpacing)) + 1,
						_itmMaxNumPts);
					int radioClimate =
						_ituData->getRadioClimateValue(txLatitudeDeg,
									       txLongitudeDeg);
					int radioClimateTmp =
						_ituData->getRadioClimateValue(rxLatitudeDeg,
									       rxLongitudeDeg);
					if (radioClimateTmp < radioClimate) {
						radioClimate = radioClimateTmp;
					}
					double surfaceRefractivity =
						_ituData->getSurfaceRefractivityValue(
							(txLatitudeDeg + rxLatitudeDeg) / 2,
							(txLongitudeDeg + rxLongitudeDeg) / 2);
					double u = _confidenceITM;
					pathLoss = UlsMeasurementAnalysis::runPointToPoint(
						_terrainDataModel,
						false,
						QPointF(txLatitudeDeg, txLongitudeDeg),
						txHeightM,
						QPointF(rxLatitudeDeg, rxLongitudeDeg),
						rxHeightM,
						distKm,
						_itmEpsDielect,
						_itmSgmConductivity,
						surfaceRefractivity,
						frequencyMHz,
						radioClimate,
						_itmPolarization,
						u,
						_reliabilityITM,
						numPts,
						NULL,
						ITMProfilePtr);
					pathLossModelStr = "ITM";
					pathLossCDF = _confidenceITM;
				}

				if (rlanHasClutter) {
					// ITU-R P.[CLUTTER] sec 3.2
					double Ll = 23.5 + 9.6 * log(frequencyGHz) / log(10.0);
					double Ls = 32.98 + 23.9 * log(distKm) / log(10.0) +
						    3.0 * log(frequencyGHz) / log(10.0);

					arma::vec gauss(1);
					gauss[0] = _zclutter2108;

					double Lctt = -5.0 *
							      log(exp(-0.2 * Ll * log(10.0)) +
								  exp(-0.2 * Ls * log(10.0))) /
							      log(10.0) +
						      6.0 * gauss[0];

					pathClutterTxDB = Lctt;
					pathClutterTxModelStr = "P.2108";
					pathClutterTxCDF = q(-gauss[0]);
				} else {
					pathClutterTxModelStr = "NONE";
					pathClutterTxDB = 0.0;
					pathClutterTxCDF = 0.5;
				}

			} else if ((propEnv == CConst::ruralPropEnv) ||
				   (propEnv == CConst::barrenPropEnv)) {
				if (itmFSPLFlag) {
					pathLoss = 20.0 *
						   log((4 * M_PI * frequency * fsplDistKm * 1000) /
						       CConst::c) /
						   log(10.0);
					pathLossModelStr = "FSPL";
					pathLossCDF = 0.5;
				} else {
					// Terrain propagation: SRTM + ITM
					double frequencyMHz = 1.0e-6 * frequency;
					double u = _confidenceITM;
					int numPts = std::min(
						((int)floor(distKm * 1000 / _itmMinSpacing)) + 1,
						_itmMaxNumPts);
					int radioClimate =
						_ituData->getRadioClimateValue(txLatitudeDeg,
									       txLongitudeDeg);
					int radioClimateTmp =
						_ituData->getRadioClimateValue(rxLatitudeDeg,
									       rxLongitudeDeg);
					if (radioClimateTmp < radioClimate) {
						radioClimate = radioClimateTmp;
					}
					double surfaceRefractivity =
						_ituData->getSurfaceRefractivityValue(
							(txLatitudeDeg + rxLatitudeDeg) / 2,
							(txLongitudeDeg + rxLongitudeDeg) / 2);
					pathLoss = UlsMeasurementAnalysis::runPointToPoint(
						_terrainDataModel,
						false,
						QPointF(txLatitudeDeg, txLongitudeDeg),
						txHeightM,
						QPointF(rxLatitudeDeg, rxLongitudeDeg),
						rxHeightM,
						distKm,
						_itmEpsDielect,
						_itmSgmConductivity,
						surfaceRefractivity,
						frequencyMHz,
						radioClimate,
						_itmPolarization,
						u,
						_reliabilityITM,
						numPts,
						NULL,
						ITMProfilePtr);
					pathLossModelStr = "ITM";
					pathLossCDF = _confidenceITM;
				}

				if ((rlanHasClutter) &&
				    (nlcdLandCatTx == CConst::noClutterNLCDLandCat)) {
					rlanHasClutter = false;
				}

				if (rlanHasClutter) {
					double ha, dk;
					switch (nlcdLandCatTx) {
						case CConst::deciduousTreesNLCDLandCat:
							ha = 15.0;
							dk = 0.05;
							if (txClutterStrPtr) {
								*txClutterStrPtr = "DECIDUOUS_"
										   "TREES";
							}
							break;
						case CConst::coniferousTreesNLCDLandCat:
							ha = 20.0;
							dk = 0.05;
							if (txClutterStrPtr) {
								*txClutterStrPtr = "CONIFEROUS_"
										   "TREES";
							}
							break;
						case CConst::highCropFieldsNLCDLandCat:
							ha = 4.0;
							dk = 0.1;
							if (txClutterStrPtr) {
								*txClutterStrPtr = "HIGH_CROP_"
										   "FIELDS";
							}
							break;
						case CConst::villageCenterNLCDLandCat:
						case CConst::unknownNLCDLandCat:
							ha = 5.0;
							dk = 0.07;
							if (txClutterStrPtr) {
								*txClutterStrPtr = "VILLAGE_CENTER";
							}
							break;
						case CConst::tropicalRainForestNLCDLandCat:
							ha = 20.0;
							dk = 0.03;
							if (txClutterStrPtr) {
								*txClutterStrPtr = "TROPICAL_RAIN_"
										   "FOREST";
							}
							break;
						default:
							ha = quietNaN;
							dk = quietNaN;
							CORE_DUMP;
							break;
					}

					if (distKm < 10 * dk) {
						pathClutterTxDB = 0.0;
					} else {
						double elevationAngleThresholdDeg =
							std::atan((ha - txHeightM) /
								  (dk * 1000.0)) *
							180.0 / M_PI;
						if (elevationAngleTxDeg >
						    elevationAngleThresholdDeg) {
							pathClutterTxDB = 0.0;
						} else {
							const double Ffc =
								0.25 +
								0.375 * (1 +
									 std::tanh(7.5 *
										   (frequencyGHz -
										    0.5)));
							double result = 10.25 * Ffc * exp(-1 * dk);
							result *= 1 -
								  std::tanh(6 * (txHeightM / ha -
										 0.625));
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
			} else if ((propEnvRx == CConst::urbanPropEnv) ||
				   (propEnvRx == CConst::suburbanPropEnv)) {
				// ITU-R P.[CLUTTER] sec 3.2
				double Ll = 23.5 + 9.6 * log(frequencyGHz) / log(10.0);
				double Ls = 32.98 + 23.9 * log(distKm) / log(10.0) +
					    3.0 * log(frequencyGHz) / log(10.0);

				arma::vec gauss(1);
				gauss[0] = _fsZclutter2108;

				double Lctt = -5.0 *
						      log(exp(-0.2 * Ll * log(10.0)) +
							  exp(-0.2 * Ls * log(10.0))) /
						      log(10.0) +
					      6.0 * gauss[0];

				pathClutterRxDB = Lctt;
				pathClutterRxModelStr = "P.2108";
				pathClutterRxCDF = q(-gauss[0]);
			} else if ((propEnvRx == CConst::ruralPropEnv) ||
				   (propEnvRx == CConst::barrenPropEnv)) {
				bool clutterFlag = _allowRuralFSClutterFlag &&
						   (nlcdLandCatRx == CConst::noClutterNLCDLandCat ?
							    false :
							    true);

				if (clutterFlag) {
					double ha, dk;
					switch (nlcdLandCatRx) {
						case CConst::deciduousTreesNLCDLandCat:
							ha = 15.0;
							dk = 0.05;
							if (rxClutterStrPtr) {
								*rxClutterStrPtr = "DECIDUOUS_"
										   "TREES";
							}
							break;
						case CConst::coniferousTreesNLCDLandCat:
							ha = 20.0;
							dk = 0.05;
							if (rxClutterStrPtr) {
								*rxClutterStrPtr = "CONIFEROUS_"
										   "TREES";
							}
							break;
						case CConst::highCropFieldsNLCDLandCat:
							ha = 4.0;
							dk = 0.1;
							if (rxClutterStrPtr) {
								*rxClutterStrPtr = "HIGH_CROP_"
										   "FIELDS";
							}
							break;
						case CConst::villageCenterNLCDLandCat:
						case CConst::unknownNLCDLandCat:
							ha = 5.0;
							dk = 0.07;
							if (rxClutterStrPtr) {
								*rxClutterStrPtr = "VILLAGE_CENTER";
							}
							break;
						default:
							CORE_DUMP;
							break;
					}

					if (distKm < 10 * dk) {
						pathClutterRxDB = 0.0;
					} else {
						double elevationAngleThresholdDeg =
							std::atan((ha - rxHeightM) /
								  (dk * 1000.0)) *
							180.0 / M_PI;
						if (elevationAngleRxDeg >
						    elevationAngleThresholdDeg) {
							pathClutterRxDB = 0.0;
						} else {
							const double Ffc =
								0.25 +
								0.375 * (1 +
									 std::tanh(7.5 *
										   (frequencyGHz -
										    0.5)));
							double result = 10.25 * Ffc * exp(-1 * dk);
							result *= 1 -
								  std::tanh(6 * (rxHeightM / ha -
										 0.625));
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
				throw std::runtime_error(
					ErrStream() << "ERROR: Invalid morphology for location "
						    << rxLongitudeDeg << " " << rxLatitudeDeg);
			}
		} else {
			pathClutterRxDB = 0.0;
			pathClutterRxModelStr = "NONE";
			pathClutterRxCDF = 0.5;
		}
	} else if (pathLossModel == CConst::FSPLPathLossModel) {
		pathLoss = 20.0 * log((4 * M_PI * frequency * fsplDistKm * 1000) / CConst::c) /
			   log(10.0);
		pathLossModelStr = "FSPL";
		pathLossCDF = 0.5;

		pathClutterTxDB = 0.0;
		pathClutterTxModelStr = "NONE";
		pathClutterTxCDF = 0.5;

		pathClutterRxDB = 0.0;
		pathClutterRxModelStr = "NONE";
		pathClutterRxCDF = 0.5;
	} else {
		throw std::runtime_error(ErrStream() << "ERROR reading ULS data: pathLossModel = "
						     << pathLossModel << " INVALID value");
	}

	if (_pathLossClampFSPL) {
		double fspl = 20.0 * log((4 * M_PI * frequency * fsplDistKm * 1000) / CConst::c) /
			      log(10.0);
		if (pathLoss < fspl) {
			pathLossModelStr += "_" + std::to_string(pathLoss) + "_CLAMPFSPL";
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
double AfcManager::Winner2_C1suburban_LOS(double distance,
					  double hBS,
					  double hMS,
					  double frequency,
					  double zval,
					  double &sigma,
					  double &pathLossCDF) const
{
	double retval;

	double dBP = 4 * hBS * hMS * frequency / CConst::c;

	if (distance < 30.0) {
		// FSPL
		sigma = 0.0;
		retval = -(20.0 * log10(CConst::c / (4 * M_PI * frequency * distance)));
	} else if (distance < dBP) {
		sigma = 4.0;
		retval = 23.8 * log10(distance) + 41.2 + 20 * log10(frequency * 1.0e-9 / 5);
	} else {
		sigma = 6.0;
		retval = 40.0 * log10(distance) + 11.65 - 16.2 * log10(hBS) - 16.2 * log10(hMS) +
			 3.8 * log10(frequency * 1.0e-9 / 5);
	}

	arma::vec gauss(1);
	gauss[0] = zval;

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
double AfcManager::Winner2_C1suburban_NLOS(double distance,
					   double hBS,
					   double /* hMS */,
					   double frequency,
					   double zval,
					   double &sigma,
					   double &pathLossCDF) const
{
	double retval;

	sigma = 8.0;
	retval = (44.9 - 6.55 * log10(hBS)) * log10(distance) + 31.46 + 5.83 * log10(hBS) +
		 23.0 * log10(frequency * 1.0e-9 / 5);

	arma::vec gauss(1);
	gauss[0] = zval;

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
double AfcManager::Winner2_C1suburban(double distance,
				      double hBS,
				      double hMS,
				      double frequency,
				      double &sigma,
				      std::string &pathLossModelStr,
				      double &pathLossCDF,
				      double &probLOS,
				      int losValue) const
{
	double retval = quietNaN;

	if ((losValue == 0) && (_closeInHgtFlag) && (hMS > _closeInHgtLOS)) {
		losValue = 1;
		probLOS = 1.0;
	} else {
		probLOS = exp(-distance / 200);
	}

	if (losValue == 0) {
		if (_winner2UnknownLOSMethod == CConst::PLOSCombineWinner2UnknownLOSMethod) {
			double sigmaLOS, sigmaNLOS;
			double plLOS = Winner2_C1suburban_LOS(distance,
							      hBS,
							      hMS,
							      frequency,
							      0.0,
							      sigmaLOS,
							      pathLossCDF);
			double plNLOS = Winner2_C1suburban_NLOS(distance,
								hBS,
								hMS,
								frequency,
								0.0,
								sigmaNLOS,
								pathLossCDF);
			retval = probLOS * plLOS + (1.0 - probLOS) * plNLOS;
			sigma = sqrt(probLOS * probLOS * sigmaLOS * sigmaLOS +
				     (1.0 - probLOS) * (1.0 - probLOS) * sigmaNLOS * sigmaNLOS);

			arma::vec gauss(1);
			gauss[0] = _zwinner2Combined;

			retval += sigma * gauss[0];
			pathLossCDF = q(-(gauss[0]));

			pathLossModelStr = "W2C1_SUBURBAN_COMB";
		} else if (_winner2UnknownLOSMethod ==
			   CConst::PLOSThresholdWinner2UnknownLOSMethod) {
			if (probLOS > _winner2ProbLOSThr) {
				retval = Winner2_C1suburban_LOS(distance,
								hBS,
								hMS,
								frequency,
								_zwinner2LOS,
								sigma,
								pathLossCDF);
				pathLossModelStr = "W2C1_SUBURBAN_LOS";
			} else {
				retval = Winner2_C1suburban_NLOS(distance,
								 hBS,
								 hMS,
								 frequency,
								 _zwinner2NLOS,
								 sigma,
								 pathLossCDF);
				pathLossModelStr = "W2C1_SUBURBAN_NLOS";
			}
		} else {
			CORE_DUMP;
		}
	} else if (losValue == 1) {
		retval = Winner2_C1suburban_LOS(distance,
						hBS,
						hMS,
						frequency,
						_zwinner2LOS,
						sigma,
						pathLossCDF);
		pathLossModelStr = "W2C1_SUBURBAN_LOS";
	} else if (losValue == 2) {
		retval = Winner2_C1suburban_NLOS(distance,
						 hBS,
						 hMS,
						 frequency,
						 _zwinner2NLOS,
						 sigma,
						 pathLossCDF);
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
double AfcManager::Winner2_C2urban_LOS(double distance,
				       double hBS,
				       double hMS,
				       double frequency,
				       double zval,
				       double &sigma,
				       double &pathLossCDF) const
{
	double retval;

	double dBP = 4 * (hBS - 1.0) * (hMS - 1.0) * frequency / CConst::c;

	if (distance < 10.0) {
		// FSPL
		sigma = 0.0;
		retval = -(20.0 * log10(CConst::c / (4 * M_PI * frequency * distance)));
	} else if (distance < dBP) {
		sigma = 4.0;
		retval = 26.0 * log10(distance) + 39.0 + 20 * log10(frequency * 1.0e-9 / 5);
	} else {
		sigma = 6.0;
		retval = 40.0 * log10(distance) + 13.47 - 14.0 * log10(hBS - 1) -
			 14.0 * log10(hMS - 1) + 6.0 * log10(frequency * 1.0e-9 / 5);
	}

	arma::vec gauss(1);
	gauss[0] = zval;

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
double AfcManager::Winner2_C2urban_NLOS(double distance,
					double hBS,
					double /* hMS */,
					double frequency,
					double zval,
					double &sigma,
					double &pathLossCDF) const
{
	double retval;

	sigma = 8.0;
	retval = (44.9 - 6.55 * log10(hBS)) * log10(distance) + 34.46 + 5.83 * log10(hBS) +
		 23.0 * log10(frequency * 1.0e-9 / 5);

	arma::vec gauss(1);
	gauss[0] = zval;

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
double AfcManager::Winner2_C2urban(double distance,
				   double hBS,
				   double hMS,
				   double frequency,
				   double &sigma,
				   std::string &pathLossModelStr,
				   double &pathLossCDF,
				   double &probLOS,
				   int losValue) const
{
	double retval = quietNaN;

	if ((losValue == 0) && (_closeInHgtFlag) && (hMS > _closeInHgtLOS)) {
		losValue = 1;
		probLOS = 1.0;
	} else {
		probLOS = (distance > 18.0 ? 18.0 / distance : 1.0) *
				  (1.0 - exp(-distance / 63.0)) +
			  exp(-distance / 63.0);
	}

	if (losValue == 0) {
		if (_winner2UnknownLOSMethod == CConst::PLOSCombineWinner2UnknownLOSMethod) {
			double sigmaLOS, sigmaNLOS;
			double plLOS = Winner2_C2urban_LOS(distance,
							   hBS,
							   hMS,
							   frequency,
							   0.0,
							   sigmaLOS,
							   pathLossCDF);
			double plNLOS = Winner2_C2urban_NLOS(distance,
							     hBS,
							     hMS,
							     frequency,
							     0.0,
							     sigmaNLOS,
							     pathLossCDF);
			retval = probLOS * plLOS + (1.0 - probLOS) * plNLOS;
			sigma = sqrt(probLOS * probLOS * sigmaLOS * sigmaLOS +
				     (1.0 - probLOS) * (1.0 - probLOS) * sigmaNLOS * sigmaNLOS);

			arma::vec gauss(1);
			gauss[0] = _zwinner2Combined;

			retval += sigma * gauss[0];
			pathLossCDF = q(-(gauss[0]));

			pathLossModelStr = "W2C2_URBAN_COMB";
		} else if (_winner2UnknownLOSMethod ==
			   CConst::PLOSThresholdWinner2UnknownLOSMethod) {
			if (probLOS > _winner2ProbLOSThr) {
				retval = Winner2_C2urban_LOS(distance,
							     hBS,
							     hMS,
							     frequency,
							     _zwinner2LOS,
							     sigma,
							     pathLossCDF);
				pathLossModelStr = "W2C2_URBAN_LOS";
			} else {
				retval = Winner2_C2urban_NLOS(distance,
							      hBS,
							      hMS,
							      frequency,
							      _zwinner2NLOS,
							      sigma,
							      pathLossCDF);
				pathLossModelStr = "W2C2_URBAN_NLOS";
			}
		} else {
			CORE_DUMP;
		}
	} else if (losValue == 1) {
		retval = Winner2_C2urban_LOS(distance,
					     hBS,
					     hMS,
					     frequency,
					     _zwinner2LOS,
					     sigma,
					     pathLossCDF);
		pathLossModelStr = "W2C2_URBAN_LOS";
	} else if (losValue == 2) {
		retval = Winner2_C2urban_NLOS(distance,
					      hBS,
					      hMS,
					      frequency,
					      _zwinner2NLOS,
					      sigma,
					      pathLossCDF);
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
double AfcManager::Winner2_D1rural_LOS(double distance,
				       double hBS,
				       double hMS,
				       double frequency,
				       double zval,
				       double &sigma,
				       double &pathLossCDF) const
{
	double retval;

	double dBP = 4 * (hBS) * (hMS)*frequency / CConst::c;

	if (distance < 10.0) {
		// FSPL
		sigma = 0.0;
		retval = -(20.0 * log10(CConst::c / (4 * M_PI * frequency * distance)));
	} else if (distance < dBP) {
		sigma = 4.0;
		retval = 21.5 * log10(distance) + 44.2 + 20 * log10(frequency * 1.0e-9 / 5);
	} else {
		sigma = 6.0;
		retval = 40.0 * log10(distance) + 10.5 - 18.5 * log10(hBS) - 18.5 * log10(hMS) +
			 1.5 * log10(frequency * 1.0e-9 / 5);
	}

	arma::vec gauss(1);
	gauss[0] = zval;

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
double AfcManager::Winner2_D1rural_NLOS(double distance,
					double hBS,
					double hMS,
					double frequency,
					double zval,
					double &sigma,
					double &pathLossCDF) const
{
	double retval;

	sigma = 8.0;
	retval = 25.1 * log10(distance) + 55.4 - 0.13 * (hBS - 25) * log10(distance / 100) -
		 0.9 * (hMS - 1.5) + 21.3 * log10(frequency * 1.0e-9 / 5);

	arma::vec gauss(1);
	gauss[0] = zval;

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
double AfcManager::Winner2_D1rural(double distance,
				   double hBS,
				   double hMS,
				   double frequency,
				   double &sigma,
				   std::string &pathLossModelStr,
				   double &pathLossCDF,
				   double &probLOS,
				   int losValue) const
{
	double retval = quietNaN;

	if ((losValue == 0) && (_closeInHgtFlag) && (hMS > _closeInHgtLOS)) {
		losValue = 1;
		probLOS = 1.0;
	} else {
		probLOS = exp(-distance / 1000);
	}

	if (losValue == 0) {
		if (_winner2UnknownLOSMethod == CConst::PLOSCombineWinner2UnknownLOSMethod) {
			double sigmaLOS, sigmaNLOS;
			double plLOS = Winner2_D1rural_LOS(distance,
							   hBS,
							   hMS,
							   frequency,
							   0.0,
							   sigmaLOS,
							   pathLossCDF);
			double plNLOS = Winner2_D1rural_NLOS(distance,
							     hBS,
							     hMS,
							     frequency,
							     0.0,
							     sigmaNLOS,
							     pathLossCDF);
			retval = probLOS * plLOS + (1.0 - probLOS) * plNLOS;
			sigma = sqrt(probLOS * probLOS * sigmaLOS * sigmaLOS +
				     (1.0 - probLOS) * (1.0 - probLOS) * sigmaNLOS * sigmaNLOS);

			arma::vec gauss(1);
			gauss[0] = _zwinner2Combined;

			retval += sigma * gauss[0];
			pathLossCDF = q(-(gauss[0]));

			pathLossModelStr = "W2D1_RURAL_COMB";
		} else if (_winner2UnknownLOSMethod ==
			   CConst::PLOSThresholdWinner2UnknownLOSMethod) {
			if (probLOS > _winner2ProbLOSThr) {
				retval = Winner2_D1rural_LOS(distance,
							     hBS,
							     hMS,
							     frequency,
							     _zwinner2LOS,
							     sigma,
							     pathLossCDF);
				pathLossModelStr = "W2D1_RURAL_LOS";
			} else {
				retval = Winner2_D1rural_NLOS(distance,
							      hBS,
							      hMS,
							      frequency,
							      _zwinner2NLOS,
							      sigma,
							      pathLossCDF);
				pathLossModelStr = "W2D1_RURAL_NLOS";
			}
		} else {
			CORE_DUMP;
		}
	} else if (losValue == 1) {
		retval = Winner2_D1rural_LOS(distance,
					     hBS,
					     hMS,
					     frequency,
					     _zwinner2LOS,
					     sigma,
					     pathLossCDF);
		pathLossModelStr = "W2D1_RURAL_LOS";
	} else if (losValue == 2) {
		retval = Winner2_D1rural_NLOS(distance,
					      hBS,
					      hMS,
					      frequency,
					      _zwinner2NLOS,
					      sigma,
					      pathLossCDF);
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
	for (auto &channel : _channelList) {
		for (int freqSegIdx = 0; freqSegIdx < channel.segList.size(); ++freqSegIdx) {
			double segEIRP;
			if (channel.type == INQUIRED_FREQUENCY) {
				segEIRP = _inquiredFrequencyMaxPSD_dBmPerMHz +
					  10.0 * log10((double)channel.bandwidth(freqSegIdx));
			} else {
				segEIRP = _maxEIRP_dBm;
			}
			std::get<0>(channel.segList[freqSegIdx]) = segEIRP;
			std::get<1>(channel.segList[freqSegIdx]) = segEIRP;
		}
	}

	if (_analysisType == "AP-AFC") {
		runPointAnalysis();
	} else if (_analysisType == "ScanAnalysis") {
		runScanAnalysis();
	} else if (_analysisType == "ExclusionZoneAnalysis") {
		runExclusionZoneAnalysis();
	} else if (_analysisType == "HeatmapAnalysis") {
		runHeatmapAnalysis();
#if DEBUG_AFC
	} else if (_analysisType == "test_itm") {
		runTestITM("path_trace_afc.csv");
	} else if (_analysisType == "test_winner2") {
		runTestWinner2("w2_alignment.csv", "w2_alignment_afc.csv");
	} else if (_analysisType == "test_aciFn") {
		double fStartMHz = -5.0;
		double fStopMHz = 25.0;
		double BMHz = 40.0;
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
		throw std::runtime_error(ErrStream() << "ERROR: Unrecognized analysis type = \""
						     << _analysisType << "\"");
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
	std::vector<int> fsidTraceList {24175};
	std::string pathTraceFile = "path_trace.csv.gz";
#endif

	LOGGER_INFO(logger) << "Executing AfcManager::runPointAnalysis()";

	_rlanRegion->configure(_rlanHeightType, _terrainDataModel);

	_rlanPointing = _rlanRegion->computePointing(_rlanAzimuthPointing, _rlanElevationPointing);

	/**************************************************************************************/
	/* Get Uncertainty Region Scan Points                                                 */
	/* scanPointList: scan points in horizontal plane (lon/lat)                           */
	/* For scan point scanPtIdx, numRlanHt[scanPtIdx] heights are considered.             */
	/**************************************************************************************/
	std::vector<LatLon> scanPointList = _rlanRegion->getScan(_scanRegionMethod,
								 _scanres_xy,
								 _scanres_points_per_degree);

	// Sorting ULS most-interferable-first
	std::vector<ULSClass *> sortedUlsList(getSortedUls());

	double heightUncertainty = _rlanRegion->getHeightUncertainty();
	int NHt = (int)ceil(heightUncertainty / _scanres_ht);
	Vector3 rlanPosnList[scanPointList.size()][2 * NHt + 1];
	GeodeticCoord rlanCoordList[scanPointList.size()][2 * NHt + 1];
	int numRlanHt[scanPointList.size()];
	double rlanTerrainHeight[scanPointList.size()];
	CConst::HeightSourceEnum rlanHeightSource[scanPointList.size()];
	CConst::PropEnvEnum rlanPropEnv[scanPointList.size()];
	CConst::NLCDLandCatEnum rlanNlcdLandCat[scanPointList.size()];

	int scanPtIdx, rlanHtIdx;
	for (scanPtIdx = 0; scanPtIdx < (int)scanPointList.size(); scanPtIdx++) {
		LatLon scanPt = scanPointList[scanPtIdx];

		double bldgHeight;
		MultibandRasterClass::HeightResult lidarHeightResult;
		_terrainDataModel->getTerrainHeight(scanPt.second,
						    scanPt.first,
						    rlanTerrainHeight[scanPtIdx],
						    bldgHeight,
						    lidarHeightResult,
						    rlanHeightSource[scanPtIdx]);

		double height0;
		if (_rlanRegion->getFixedHeightAMSL()) {
			height0 = _rlanRegion->getCenterHeightAMSL();
		} else {
			height0 = _rlanRegion->getCenterHeightAMSL() -
				  _rlanRegion->getCenterTerrainHeight() +
				  rlanTerrainHeight[scanPtIdx];
		}

		int htIdx;
		numRlanHt[scanPtIdx] = 0;
		bool lowHeightFlag = false;
		for (htIdx = 0; (htIdx <= 2 * NHt) && (!lowHeightFlag); ++htIdx) {
			double heightAMSL = height0 +
					    (NHt ? (NHt - htIdx) * heightUncertainty / NHt :
						   0.0); // scan from top down
			double heightAGL = heightAMSL - rlanTerrainHeight[scanPtIdx];
			bool useFlag;
			if (heightAGL < _minRlanHeightAboveTerrain) {
				switch (_scanPointBelowGroundMethod) {
					case CConst::DiscardScanPointBelowGroundMethod:
						useFlag = false;
						break;
					case CConst::TruncateScanPointBelowGroundMethod:
						heightAMSL = rlanTerrainHeight[scanPtIdx] +
							     _minRlanHeightAboveTerrain;
						useFlag = true;
						break;
					default:
						CORE_DUMP;
						break;
				}
				lowHeightFlag = true;
			} else {
				useFlag = true;
			}
			if (useFlag) {
				rlanCoordList[scanPtIdx][htIdx] =
					GeodeticCoord::fromLatLon(scanPt.first,
								  scanPt.second,
								  heightAMSL / 1000.0);
				rlanPosnList[scanPtIdx][htIdx] = EcefModel::fromGeodetic(
					rlanCoordList[scanPtIdx][htIdx]);
				numRlanHt[scanPtIdx]++;
			}
		}

		rlanPropEnv[scanPtIdx] = computePropEnv(scanPt.second,
							scanPt.first,
							rlanNlcdLandCat[scanPtIdx]);
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Create excThrFile, useful for debugging                                            */
	/**************************************************************************************/
	ExThrGzipCsv *excthrGc = (ExThrGzipCsv *)NULL;
	if (!_excThrFile.empty()) {
		excthrGc = new ExThrGzipCsv(_excThrFile);
	}

	EirpGzipCsv eirpGc(_eirpGcFile);
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
		fkml->writeTextElement("name", "AFC");
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
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
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
		fkml->writeAttribute("id", "bluePoly");
		fkml->writeStartElement("LineStyle");
		fkml->writeTextElement("width", "1.5");
		fkml->writeEndElement(); // LineStyle
		fkml->writeStartElement("PolyStyle");
		fkml->writeTextElement("color", "ffff0000");
		fkml->writeEndElement(); // PolyStyle
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
		fkml->writeTextElement("href",
				       "http://maps.google.com/mapfiles/kml/shapes/"
				       "placemark_circle.png");
		fkml->writeEndElement(); // Icon
		fkml->writeEndElement(); // IconStyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "redPlacemark");
		fkml->writeStartElement("IconStyle");
		fkml->writeTextElement("color", "ff0000ff");
		fkml->writeStartElement("Icon");
		fkml->writeTextElement("href",
				       "http://maps.google.com/mapfiles/kml/pushpin/"
				       "ylw-pushpin.png");
		fkml->writeEndElement(); // Icon
		fkml->writeEndElement(); // IconStyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "yellowPlacemark");
		fkml->writeStartElement("IconStyle");
		fkml->writeTextElement("color", "ff00ffff");
		fkml->writeStartElement("Icon");
		fkml->writeTextElement("href",
				       "http://maps.google.com/mapfiles/kml/pushpin/"
				       "ylw-pushpin.png");
		fkml->writeEndElement(); // Icon
		fkml->writeEndElement(); // IconStyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "greenPlacemark");
		fkml->writeStartElement("IconStyle");
		fkml->writeTextElement("color", "ff00ff00");
		fkml->writeStartElement("Icon");
		fkml->writeTextElement("href",
				       "http://maps.google.com/mapfiles/kml/pushpin/"
				       "ylw-pushpin.png");
		fkml->writeEndElement(); // Icon
		fkml->writeEndElement(); // IconStyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "blackPlacemark");
		fkml->writeStartElement("IconStyle");
		fkml->writeTextElement("color", "ff000000");
		fkml->writeStartElement("Icon");
		fkml->writeTextElement("href",
				       "http://maps.google.com/mapfiles/kml/pushpin/"
				       "ylw-pushpin.png");
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
		fkml->writeTextElement("coordinates",
				       QString::asprintf("%.10f,%.10f,%.2f",
							 rlanCenterPtGeo.longitudeDeg,
							 rlanCenterPtGeo.latitudeDeg,
							 rlanCenterPtGeo.heightKm * 1000.0));
		fkml->writeEndElement(); // Point
		fkml->writeEndElement(); // Placemark
		/**********************************************************************************/

		/**********************************************************************************/
		/* TOP                                                                            */
		/**********************************************************************************/
		fkml->writeStartElement("Placemark");
		fkml->writeTextElement("name", "TOP");
		fkml->writeTextElement("visibility", "1");
		fkml->writeTextElement("styleUrl", "#transBluePoly");
		fkml->writeStartElement("Polygon");
		fkml->writeTextElement("extrude", "0");
		fkml->writeTextElement("tessellate", "0");
		fkml->writeTextElement("altitudeMode", "absolute");
		fkml->writeStartElement("outerBoundaryIs");
		fkml->writeStartElement("LinearRing");

		QString top_coords = QString();
		for (ptIdx = 0; ptIdx <= (int)ptList.size(); ptIdx++) {
			GeodeticCoord pt = ptList[ptIdx % ptList.size()];
			top_coords.append(
				QString::asprintf("%.10f,%.10f,%.2f\n",
						  pt.longitudeDeg,
						  pt.latitudeDeg,
						  pt.heightKm * 1000.0 +
							  _rlanRegion->getHeightUncertainty()));
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
		fkml->writeTextElement("styleUrl", "#transBluePoly");
		fkml->writeStartElement("Polygon");
		fkml->writeTextElement("extrude", "0");
		fkml->writeTextElement("tessellate", "0");
		fkml->writeTextElement("altitudeMode", "absolute");
		fkml->writeStartElement("outerBoundaryIs");
		fkml->writeStartElement("LinearRing");

		QString bottom_coords = QString();
		for (ptIdx = 0; ptIdx <= (int)ptList.size(); ptIdx++) {
			GeodeticCoord pt = ptList[ptIdx % ptList.size()];
			bottom_coords.append(
				QString::asprintf("%.10f,%.10f,%.2f\n",
						  pt.longitudeDeg,
						  pt.latitudeDeg,
						  pt.heightKm * 1000.0 -
							  _rlanRegion->getHeightUncertainty()));
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
		for (ptIdx = 0; ptIdx < (int)ptList.size(); ptIdx++) {
			fkml->writeStartElement("Placemark");
			fkml->writeTextElement("name", QString::asprintf("S_%d", ptIdx));
			fkml->writeTextElement("visibility", "1");
			fkml->writeTextElement("styleUrl", "#transBluePoly");
			fkml->writeStartElement("Polygon");
			fkml->writeTextElement("extrude", "0");
			fkml->writeTextElement("tessellate", "0");
			fkml->writeTextElement("altitudeMode", "absolute");
			fkml->writeStartElement("outerBoundaryIs");
			fkml->writeStartElement("LinearRing");

			GeodeticCoord pt1 = ptList[ptIdx];
			GeodeticCoord pt2 = ptList[(ptIdx + 1) % ptList.size()];
			QString side_coords;
			side_coords.append(
				QString::asprintf("%.10f,%.10f,%.2f\n",
						  pt1.longitudeDeg,
						  pt1.latitudeDeg,
						  pt1.heightKm * 1000.0 -
							  _rlanRegion->getHeightUncertainty()));
			side_coords.append(
				QString::asprintf("%.10f,%.10f,%.2f\n",
						  pt1.longitudeDeg,
						  pt1.latitudeDeg,
						  pt1.heightKm * 1000.0 +
							  _rlanRegion->getHeightUncertainty()));
			side_coords.append(
				QString::asprintf("%.10f,%.10f,%.2f\n",
						  pt2.longitudeDeg,
						  pt2.latitudeDeg,
						  pt2.heightKm * 1000.0 +
							  _rlanRegion->getHeightUncertainty()));
			side_coords.append(
				QString::asprintf("%.10f,%.10f,%.2f\n",
						  pt2.longitudeDeg,
						  pt2.latitudeDeg,
						  pt2.heightKm * 1000.0 -
							  _rlanRegion->getHeightUncertainty()));
			side_coords.append(
				QString::asprintf("%.10f,%.10f,%.2f\n",
						  pt1.longitudeDeg,
						  pt1.latitudeDeg,
						  pt1.heightKm * 1000.0 -
							  _rlanRegion->getHeightUncertainty()));

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

		for (scanPtIdx = 0; scanPtIdx < (int)scanPointList.size(); scanPtIdx++) {
			LatLon scanPt = scanPointList[scanPtIdx];

			for (rlanHtIdx = 0; rlanHtIdx < numRlanHt[scanPtIdx]; ++rlanHtIdx) {
				double heightAMSL = rlanCoordList[scanPtIdx][rlanHtIdx].heightKm *
						    1000;

				fkml->writeStartElement("Placemark");
				fkml->writeTextElement("name",
						       QString::asprintf("SCAN_POINT_%d_%d",
									 scanPtIdx,
									 rlanHtIdx));
				fkml->writeTextElement("visibility", "1");
				fkml->writeTextElement("styleUrl", "#dotStyle");
				fkml->writeStartElement("Point");
				fkml->writeTextElement("altitudeMode", "absolute");
				fkml->writeTextElement("coordinates",
						       QString::asprintf("%.10f,%.10f,%.2f",
									 scanPt.second,
									 scanPt.first,
									 heightAMSL));
				fkml->writeEndElement(); // Point
				fkml->writeEndElement(); // Placemark
			}
		}

		fkml->writeEndElement(); // Scan Points
		/**********************************************************************************/

		if (_rlanAntenna) {
			double rlanAntennaArrowLength = 1000.0;
			Vector3 centerPosn = _rlanRegion->getCenterPosn();

			Vector3 zvec = _rlanPointing;
			Vector3 xvec = (Vector3(zvec.y(), -zvec.x(), 0.0)).normalized();
			Vector3 yvec = zvec.cross(xvec);

			int numCvgPoints = 32;

			std::vector<GeodeticCoord> ptgPtList;
			double cvgTheta = 2.0 * M_PI / 180.0;
			int cvgPhiIdx;
			for (cvgPhiIdx = 0; cvgPhiIdx < numCvgPoints; ++cvgPhiIdx) {
				double cvgPhi = 2 * M_PI * cvgPhiIdx / numCvgPoints;
				Vector3 cvgIntPosn = centerPosn +
						     (rlanAntennaArrowLength / 1000.0) *
							     (zvec * cos(cvgTheta) +
							      (xvec * cos(cvgPhi) +
							       yvec * sin(cvgPhi)) *
								      sin(cvgTheta));

				GeodeticCoord cvgIntPosnGeodetic = EcefModel::ecefToGeodetic(
					cvgIntPosn);
				ptgPtList.push_back(cvgIntPosnGeodetic);
			}

			if (true) {
				fkml->writeStartElement("Folder");
				fkml->writeTextElement("name", "DIR_ANTENNA");

				for (cvgPhiIdx = 0; cvgPhiIdx < numCvgPoints; ++cvgPhiIdx) {
					fkml->writeStartElement("Placemark");
					fkml->writeTextElement("name",
							       QString::asprintf("p%d", cvgPhiIdx));
					fkml->writeTextElement("styleUrl", "#bluePoly");
					fkml->writeTextElement("visibility", "1");
					fkml->writeStartElement("Polygon");
					fkml->writeTextElement("extrude", "0");
					fkml->writeTextElement("altitudeMode", "absolute");
					fkml->writeStartElement("outerBoundaryIs");
					fkml->writeStartElement("LinearRing");

					QString more_coords = QString::asprintf(
						"%.10f,%.10f,%.2f\n",
						_rlanRegion->getCenterLongitude(),
						_rlanRegion->getCenterLatitude(),
						_rlanRegion->getCenterHeightAMSL());

					GeodeticCoord pt = ptgPtList[cvgPhiIdx];
					more_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n",
									     pt.longitudeDeg,
									     pt.latitudeDeg,
									     pt.heightKm * 1000.0));

					pt = ptgPtList[(cvgPhiIdx + 1) % numCvgPoints];
					more_coords.append(QString::asprintf("%.10f,%.10f,%.2f\n",
									     pt.longitudeDeg,
									     pt.latitudeDeg,
									     pt.heightKm * 1000.0));

					more_coords.append(QString::asprintf(
						"%.10f,%.10f,%.2f\n",
						_rlanRegion->getCenterLongitude(),
						_rlanRegion->getCenterLatitude(),
						_rlanRegion->getCenterHeightAMSL()));

					fkml->writeTextElement("coordinates", more_coords);
					fkml->writeEndElement(); // LinearRing
					fkml->writeEndElement(); // outerBoundaryIs
					fkml->writeEndElement(); // Polygon
					fkml->writeEndElement(); // Placemark
				}
				fkml->writeEndElement(); // Beamcone
			}
		}

		std::vector<GeodeticCoord> bdyPtList = _rlanRegion->getBoundaryPolygon(
			_terrainDataModel);

		if (bdyPtList.size()) {
			/**********************************************************************************/
			/* TOP BOUNDARY */
			/**********************************************************************************/
			fkml->writeStartElement("Placemark");
			fkml->writeTextElement("name", "TOP BOUNDARY");
			fkml->writeTextElement("visibility", "1");
			fkml->writeTextElement("styleUrl", "#transGrayPoly");
			fkml->writeStartElement("Polygon");
			fkml->writeTextElement("extrude", "0");
			fkml->writeTextElement("tessellate", "0");
			fkml->writeTextElement("altitudeMode", "absolute");
			fkml->writeStartElement("outerBoundaryIs");
			fkml->writeStartElement("LinearRing");

			QString top_bdy_coords = QString();
			for (ptIdx = 0; ptIdx <= (int)bdyPtList.size(); ptIdx++) {
				GeodeticCoord pt = bdyPtList[ptIdx % bdyPtList.size()];
				top_bdy_coords.append(
					QString::asprintf("%.10f,%.10f,%.2f\n",
							  pt.longitudeDeg,
							  pt.latitudeDeg,
							  _rlanRegion->getMaxHeightAMSL()));
			}

			fkml->writeTextElement("coordinates", top_bdy_coords);
			fkml->writeEndElement(); // LinearRing
			fkml->writeEndElement(); // outerBoundaryIs
			fkml->writeEndElement(); // Polygon
			fkml->writeEndElement(); // Placemark
			/**********************************************************************************/

			/**********************************************************************************/
			/* BOTTOM BOUNDARY */
			/**********************************************************************************/
			fkml->writeStartElement("Placemark");
			fkml->writeTextElement("name", "BOTTOM BOUNDARY");
			fkml->writeTextElement("visibility", "1");
			fkml->writeTextElement("styleUrl", "#transGrayPoly");
			fkml->writeStartElement("Polygon");
			fkml->writeTextElement("extrude", "0");
			fkml->writeTextElement("tessellate", "0");
			fkml->writeTextElement("altitudeMode", "absolute");
			fkml->writeStartElement("outerBoundaryIs");
			fkml->writeStartElement("LinearRing");

			QString bottom_bdy_coords = QString();
			for (ptIdx = 0; ptIdx <= (int)bdyPtList.size(); ptIdx++) {
				GeodeticCoord pt = bdyPtList[ptIdx % bdyPtList.size()];
				bottom_bdy_coords.append(
					QString::asprintf("%.10f,%.10f,%.2f\n",
							  pt.longitudeDeg,
							  pt.latitudeDeg,
							  _rlanRegion->getMinHeightAMSL()));
			}

			fkml->writeTextElement("coordinates", bottom_bdy_coords);
			fkml->writeEndElement(); // LinearRing
			fkml->writeEndElement(); // outerBoundaryIs
			fkml->writeEndElement(); // Polygon
			fkml->writeEndElement(); // Placemark
			/**********************************************************************************/
		}

		fkml->writeEndElement(); // Folder

		fkml->writeStartElement("Folder");
		fkml->writeTextElement("name", "Denied Region");
		int drIdx;
		for (drIdx = 0; drIdx < (int)_deniedRegionList.size(); ++drIdx) {
			DeniedRegionClass *dr = _deniedRegionList[drIdx];
			DeniedRegionClass::TypeEnum drType = dr->getType();
			std::string pfx;
			switch (drType) {
				case DeniedRegionClass::RASType:
					pfx = "RAS_";
					break;
				case DeniedRegionClass::userSpecifiedType:
					pfx = "USER_SPEC_";
					break;
				default:
					CORE_DUMP;
					break;
			}

			fkml->writeStartElement("Folder");
			fkml->writeTextElement("name",
					       QString::fromStdString(pfx) +
						       QString::number(dr->getID()));

			int numPtsCircle = 32;
			int rectIdx, numRect;
			double rectLonStart, rectLonStop, rectLatStart, rectLatStop;
			double circleRadius, longitudeCenter, latitudeCenter;
			double drTerrainHeight, drBldgHeight, drHeightAGL;
			Vector3 drCenterPosn;
			Vector3 drUpVec;
			Vector3 drEastVec;
			Vector3 drNorthVec;
			QString dr_coords;
			MultibandRasterClass::HeightResult drLidarHeightResult;
			CConst::HeightSourceEnum drHeightSource;
			DeniedRegionClass::GeometryEnum drGeometry = dr->getGeometry();
			switch (drGeometry) {
				case DeniedRegionClass::rectGeometry:
				case DeniedRegionClass::rect2Geometry:
					numRect = ((RectDeniedRegionClass *)dr)->getNumRect();
					for (rectIdx = 0; rectIdx < numRect; rectIdx++) {
						std::tie(rectLonStart,
							 rectLonStop,
							 rectLatStart,
							 rectLatStop) =
							((RectDeniedRegionClass *)dr)
								->getRect(rectIdx);

						fkml->writeStartElement("Placemark");
						fkml->writeTextElement("name",
								       QString("RECT_") +
									       QString::number(
										       rectIdx));
						fkml->writeTextElement("visibility", "1");
						fkml->writeTextElement("styleUrl",
								       "#transBluePoly");
						fkml->writeStartElement("Polygon");
						fkml->writeTextElement("extrude", "0");
						fkml->writeTextElement("tessellate", "0");
						fkml->writeTextElement("altitudeMode",
								       "clampToGround");
						fkml->writeStartElement("outerBoundaryIs");
						fkml->writeStartElement("LinearRing");

						dr_coords = QString();
						dr_coords.append(QString::asprintf("%.10f,%.10f,%."
										   "2f\n",
										   rectLonStart,
										   rectLatStart,
										   0.0));
						dr_coords.append(QString::asprintf("%.10f,%.10f,%."
										   "2f\n",
										   rectLonStop,
										   rectLatStart,
										   0.0));
						dr_coords.append(QString::asprintf("%.10f,%.10f,%."
										   "2f\n",
										   rectLonStop,
										   rectLatStop,
										   0.0));
						dr_coords.append(QString::asprintf("%.10f,%.10f,%."
										   "2f\n",
										   rectLonStart,
										   rectLatStop,
										   0.0));
						dr_coords.append(QString::asprintf("%.10f,%.10f,%."
										   "2f\n",
										   rectLonStart,
										   rectLatStart,
										   0.0));

						fkml->writeTextElement("coordinates", dr_coords);
						fkml->writeEndElement(); // LinearRing
						fkml->writeEndElement(); // outerBoundaryIs
						fkml->writeEndElement(); // Polygon
						fkml->writeEndElement(); // Placemark
					}
					break;
				case DeniedRegionClass::circleGeometry:
				case DeniedRegionClass::horizonDistGeometry:
					circleRadius =
						((CircleDeniedRegionClass *)dr)
							->computeRadius(
								_rlanRegion->getMaxHeightAGL());
					longitudeCenter = ((CircleDeniedRegionClass *)dr)
								  ->getLongitudeCenter();
					latitudeCenter = ((CircleDeniedRegionClass *)dr)
								 ->getLatitudeCenter();
					drHeightAGL = dr->getHeightAGL();
					_terrainDataModel->getTerrainHeight(longitudeCenter,
									    latitudeCenter,
									    drTerrainHeight,
									    drBldgHeight,
									    drLidarHeightResult,
									    drHeightSource);

					drCenterPosn = EcefModel::geodeticToEcef(
						latitudeCenter,
						longitudeCenter,
						(drTerrainHeight + drHeightAGL) / 1000.0);
					drUpVec = drCenterPosn.normalized();
					drEastVec = (Vector3(-drUpVec.y(), drUpVec.x(), 0.0))
							    .normalized();
					drNorthVec = drUpVec.cross(drEastVec);

					fkml->writeStartElement("Placemark");
					fkml->writeTextElement("name", QString("CIRCLE"));
					fkml->writeTextElement("visibility", "1");
					fkml->writeTextElement("styleUrl", "#transBluePoly");
					fkml->writeStartElement("Polygon");
					fkml->writeTextElement("extrude", "0");
					fkml->writeTextElement("tessellate", "0");
					fkml->writeTextElement("altitudeMode", "clampToGround");
					fkml->writeStartElement("outerBoundaryIs");
					fkml->writeStartElement("LinearRing");

					dr_coords = QString();
					for (ptIdx = 0; ptIdx <= numPtsCircle; ++ptIdx) {
						double phi = 2 * M_PI * ptIdx / numPtsCircle;
						Vector3 circlePtPosn = drCenterPosn +
								       (circleRadius / 1000) *
									       (drEastVec *
											cos(phi) +
										drNorthVec *
											sin(phi));

						GeodeticCoord circlePtPosnGeodetic =
							EcefModel::ecefToGeodetic(circlePtPosn);

						dr_coords.append(QString::asprintf(
							"%.10f,%.10f,%.2f\n",
							circlePtPosnGeodetic.longitudeDeg,
							circlePtPosnGeodetic.latitudeDeg,
							0.0));
					}

					fkml->writeTextElement("coordinates", dr_coords);
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
	TraceGzipCsv pathTraceGc(pathTraceFile);
	/**************************************************************************************/
#endif

	/**************************************************************************************/
	/* Compute Channel Availability                                                       */
	/**************************************************************************************/
	const double exclusionDistKmSquared = (_exclusionDist / 1000.0) * (_exclusionDist / 1000.0);
	const double maxRadiusKmSquared = (_maxRadius / 1000.0) * (_maxRadius / 1000.0);

	if ((_rlanRegion->getMaxHeightAGL() < _minRlanHeightAboveTerrain) &&
	    (_reportErrorRlanHeightLowFlag)) {
		LOGGER_WARN(logger)
			<< std::string("ERROR: Point Analysis: Invalid RLAN parameter settings.")
			<< std::endl
			<< std::string("RLAN uncertainty region has a max AGL height of ")
			<< _rlanRegion->getMaxHeightAGL() << std::string(", which is < ")
			<< _minRlanHeightAboveTerrain << std::endl;
		_responseCode = CConst::invalidValueResponseCode;
		_invalidParams << "heightType"
			       << "height"
			       << "verticalUncertainty";
		return;
	}

#if DEBUG_AFC
	char *tstr;

	time_t tStartDR = time(NULL);
	tstr = strdup(ctime(&tStartDR));
	strtok(tstr, "\n");

	std::cout << "Begin Processing Denied Regions " << tstr << std::endl << std::flush;

	free(tstr);
#endif

	int drIdx;
	for (drIdx = 0; drIdx < (int)_deniedRegionList.size(); ++drIdx) {
		DeniedRegionClass *dr = _deniedRegionList[drIdx];
		bool foundScanPointInDR = false;
		for (scanPtIdx = 0;
		     (scanPtIdx < (int)scanPointList.size()) && (!foundScanPointInDR);
		     scanPtIdx++) {
			if (numRlanHt[scanPtIdx]) {
				rlanHtIdx = 0;

				GeodeticCoord rlanCoord = rlanCoordList[scanPtIdx][rlanHtIdx];
				double rlanHeightAGL = (rlanCoord.heightKm * 1000) -
						       rlanTerrainHeight[scanPtIdx];

				if (dr->intersect(rlanCoord.longitudeDeg,
						  rlanCoord.latitudeDeg,
						  0.0,
						  rlanHeightAGL)) {
					int chanIdx;
					for (chanIdx = 0; chanIdx < (int)_channelList.size();
					     ++chanIdx) {
						ChannelStruct *channel = &(_channelList[chanIdx]);
						for (int freqSegIdx = 0;
						     freqSegIdx < channel->segList.size();
						     ++freqSegIdx) {
							if (std::get<2>(
								    channel->segList[freqSegIdx]) !=
							    BLACK) {
								double chanStartFreq =
									channel->freqMHzList
										[freqSegIdx] *
									1.0e6;
								double chanStopFreq =
									channel->freqMHzList
										[freqSegIdx + 1] *
									1.0e6;
								bool hasOverlap = computeSpectralOverlapLoss(
									(double *)NULL,
									chanStartFreq,
									chanStopFreq,
									dr->getStartFreq(),
									dr->getStopFreq(),
									false,
									CConst::psdSpectralAlgorithm);
								if (hasOverlap) {
									channel->segList
										[freqSegIdx] = std::make_tuple(
										-std::numeric_limits<
											double>::
											infinity(),
										-std::numeric_limits<
											double>::
											infinity(),
										BLACK);
								}
							}
						}
					}
					foundScanPointInDR = true;
				}
			}
		}
	}

#if DEBUG_AFC
	time_t tEndDR = time(NULL);
	tstr = strdup(ctime(&tEndDR));
	strtok(tstr, "\n");

	int elapsedTimeDR = (int)(tEndDR - tStartDR);

	int etDR = elapsedTimeDR;
	int elapsedTimeDRSec = etDR % 60;
	etDR = etDR / 60;
	int elapsedTimeDRMin = etDR % 60;
	etDR = etDR / 60;
	int elapsedTimeDRHour = etDR % 24;
	etDR = etDR / 24;
	int elapsedTimeDRDay = etDR;

	std::cout << "End Processing Denied Regions " << tstr
		  << " Elapsed time = " << (tEndDR - tStartDR) << " sec = " << elapsedTimeDRDay
		  << " days " << elapsedTimeDRHour << " hours " << elapsedTimeDRMin << " min "
		  << elapsedTimeDRSec << " sec." << std::endl
		  << std::flush;

	free(tstr);

#endif

#if DEBUG_AFC
	time_t tStartULS = time(NULL);
	tstr = strdup(ctime(&tStartULS));
	strtok(tstr, "\n");

	std::cout << "Begin Processing ULS RX's " << tstr << std::endl << std::flush;

	free(tstr);

	int traceIdx = 0;
#endif

	/**************************************************************************************/
	/* Create _fsAnalysisListFile                                                         */
	/**************************************************************************************/
	FILE *fFSList;
	if (_fsAnalysisListFile.empty()) {
		fFSList = (FILE *)NULL;
	} else {
		if (!(fFSList = fopen(_fsAnalysisListFile.c_str(), "wb"))) {
			errStr << std::string("ERROR: Unable to open fsAnalysisListFile \"") +
					  _fsAnalysisListFile + std::string("\"\n");
			throw std::runtime_error(errStr.str());
		}
	}
	if (fFSList) {
		fprintf(fFSList,
			"FSID"
			",RX_CALLSIGN"
			",TX_CALLSIGN"
			",NUM_PASSIVE_REPEATER"
			",IS_DIVERSITY_LINK"
			",SEGMENT_IDX"
			",START_FREQ (MHz)"
			",STOP_FREQ (MHz)"
			",SEGMENT_RX_LONGITUDE (deg)"
			",SEGMENT_RX_LATITUDE (deg)"
			",SEGMENT_RX_HEIGHT_AGL (m)"
			",SEGMENT_DISTANCE (m)"
			",PR_REF_THETA_IN (deg)"
			",PR_REF_KS"
			",PR_REF_Q"
			",PR_PATH_SEGMENT_GAIN (dB)"
			",PR_EFFECTIVE_GAIN (dB)"
			"\n");
	}
	/**************************************************************************************/

	int ulsIdx;
	double *eirpLimitList = (double *)malloc(sortedUlsList.size() * sizeof(double));
	bool *ulsFlagList = (bool *)malloc(sortedUlsList.size() * sizeof(bool));
	for (ulsIdx = 0; ulsIdx < (int)sortedUlsList.size(); ++ulsIdx) {
		eirpLimitList[ulsIdx] = _maxEIRP_dBm;
		ulsFlagList[ulsIdx] = false;
	}

	int totNumProc = (int)sortedUlsList.size();

	int numPct = 100;

	if (numPct > totNumProc) {
		numPct = totNumProc;
	}

	bool cont = true;
	int numProc = 0;
	for (ulsIdx = 0; (ulsIdx < (int)sortedUlsList.size()) && (cont); ++ulsIdx) {
		LOGGER_DEBUG(logger)
			<< "considering ULSIdx: " << ulsIdx << '/' << sortedUlsList.size();
		ULSClass *uls = sortedUlsList[ulsIdx];

#if 0
		// For debugging, identifies anomalous ULS entries
		if (uls->getLinkDistance() == -1) {
			std::cout << uls->getID() << std::endl;
		}
#endif
		if (true) {
#if DEBUG_AFC
			bool traceFlag = false;
			if (traceIdx < (int)fsidTraceList.size()) {
				if (uls->getID() == fsidTraceList[traceIdx]) {
					traceFlag = true;
				}
			}
#endif

			int numPR = uls->getNumPR();
			int numDiversity = (uls->getHasDiversity() ? 2 : 1);

			int segStart = (_passiveRepeaterFlag ? 0 : numPR);

			for (int segIdx = segStart; segIdx < numPR + 1; ++segIdx) {
				for (int divIdx = 0; divIdx < numDiversity; ++divIdx) {
					Vector3 ulsRxPos =
						(segIdx == numPR ?
							 (divIdx == 0 ?
								  uls->getRxPosition() :
								  uls->getDiversityPosition()) :
							 uls->getPR(segIdx).positionRx);
					double ulsRxLongitude =
						(segIdx == numPR ? uls->getRxLongitudeDeg() :
								   uls->getPR(segIdx).longitudeDeg);
					double ulsRxLatitude =
						(segIdx == numPR ? uls->getRxLatitudeDeg() :
								   uls->getPR(segIdx).latitudeDeg);

					Vector3 lineOfSightVectorKm = ulsRxPos -
								      _rlanRegion->getCenterPosn();
					double distKmSquared =
						(lineOfSightVectorKm).dot(lineOfSightVectorKm);

					if (distKmSquared < maxRadiusKmSquared) {
#if DEBUG_AFC
						time_t t1 = time(NULL);
#endif

						ulsFlagList[ulsIdx] = true;

						double ulsRxHeightAGL =
							(segIdx == numPR ?
								 (divIdx == 0 ?
									  uls->getRxHeightAboveTerrain() :
									  uls->getDiversityHeightAboveTerrain()) :
								 uls->getPR(segIdx)
									 .heightAboveTerrainRx);
						double ulsRxHeightAMSL =
							(segIdx == numPR ?
								 (divIdx == 0 ?
									  uls->getRxHeightAMSL() :
									  uls->getDiversityHeightAMSL()) :
								 uls->getPR(segIdx).heightAMSLRx);
						double ulsSegmentDistance =
							(segIdx == numPR ?
								 uls->getLinkDistance() :
								 uls->getPR(segIdx)
									 .segmentDistance);

						/**************************************************************************************/
						/* Determine propagation environment of FS segment
						 * RX, if needed.                     */
						/**************************************************************************************/
						char ulsRxPropEnv = ' ';
						CConst::NLCDLandCatEnum nlcdLandCatRx;
						CConst::PropEnvEnum fsPropEnv;
						if ((_applyClutterFSRxFlag) &&
						    (ulsRxHeightAGL <= _maxFsAglHeight)) {
							fsPropEnv = computePropEnv(ulsRxLongitude,
										   ulsRxLatitude,
										   nlcdLandCatRx);
							switch (fsPropEnv) {
								case CConst::urbanPropEnv:
									ulsRxPropEnv = 'U';
									break;
								case CConst::suburbanPropEnv:
									ulsRxPropEnv = 'S';
									break;
								case CConst::ruralPropEnv:
									ulsRxPropEnv = 'R';
									break;
								case CConst::barrenPropEnv:
									ulsRxPropEnv = 'B';
									break;
								case CConst::unknownPropEnv:
									ulsRxPropEnv = 'X';
									break;
								default:
									CORE_DUMP;
							}
						} else {
							fsPropEnv = CConst::unknownPropEnv;
							ulsRxPropEnv = ' ';
						}
						/**************************************************************************************/

						LatLon ulsRxLatLon =
							std::pair<double, double>(ulsRxLatitude,
										  ulsRxLongitude);
						bool contains2D, contains3D;

						// If contains2D is set, FS lon/lat is inside
						// 2D-uncertainty region, depending on height may be
						// above, below, or actually inside uncertainty
						// region. If contains3D is set, FS is inside
						// 3D-uncertainty
						LatLon closestLatLon =
							_rlanRegion->closestPoint(ulsRxLatLon,
										  contains2D);
						if ((!contains2D) &&
						    (fabs(closestLatLon.first - ulsRxLatLon.first) <
						     2.0 / _scanres_points_per_degree) &&
						    (fabs(closestLatLon.second -
							  ulsRxLatLon.second) <
						     2.0 / _scanres_points_per_degree)) {
							double adjFSRxLongitude =
								(std::floor(
									 ulsRxLongitude *
									 _scanres_points_per_degree) +
								 0.5) /
								_scanres_points_per_degree;
							double adjFSRxLatitude =
								(std::floor(
									 ulsRxLatitude *
									 _scanres_points_per_degree) +
								 0.5) /
								_scanres_points_per_degree;

							for (scanPtIdx = 0;
							     (scanPtIdx <
							      (int)scanPointList.size()) &&
							     (!contains2D);
							     scanPtIdx++) {
								LatLon scanPt =
									scanPointList[scanPtIdx];
								if ((fabs(scanPt.second -
									  adjFSRxLongitude) <
								     1.0e-10) &&
								    (fabs(scanPt.first -
									  adjFSRxLatitude) <
								     1.0e-10)) {
									contains2D = true;
								}
							}
						}

						contains3D = false;
						if (contains2D) {
							if ((ulsRxHeightAMSL >=
							     _rlanRegion->getMinHeightAMSL()) &&
							    (ulsRxHeightAMSL <=
							     _rlanRegion->getMaxHeightAMSL())) {
								contains3D = true;
								LOGGER_INFO(logger)
									<< "FSID = " << uls->getID()
									<< (divIdx ? " DIVERSITY "
										     "LINK" :
										     "")
									<< (segIdx == numPR ?
										    " RX" :
										    " PR " +
											    std::to_string(
												    segIdx +
												    1))
									<< " inside uncertainty "
									   "volume";
							}
							if (!contains3D) {
								LOGGER_INFO(logger)
									<< "FSID = " << uls->getID()
									<< (divIdx ? " DIVERSITY "
										     "LINK" :
										     "")
									<< (segIdx == numPR ?
										    " RX" :
										    " PR " +
											    std::to_string(
												    segIdx +
												    1))
									<< " inside uncertainty "
									   "footprint, not in "
									   "volume";
							}
						}

						double minRLANDist = -1.0;
						if (distKmSquared <= exclusionDistKmSquared) {
							LOGGER_INFO(logger)
								<< "FSID = " << uls->getID()
								<< (segIdx == numPR ?
									    " RX" :
									    " PR " +
										    std::to_string(
											    segIdx))
								<< " is inside exclusion distance.";
							minRLANDist = sqrt(distKmSquared) * 1000.0;
						} else if ((contains3D) &&
							   (!_allowScanPtsInUncRegFlag)) {
							int chanIdx;
							for (chanIdx = 0;
							     chanIdx < (int)_channelList.size();
							     ++chanIdx) {
								ChannelStruct *channel = &(
									_channelList[chanIdx]);
								ChannelType channelType =
									channel->type;
								bool useACI =
									(channelType == INQUIRED_FREQUENCY ?
										 false :
										 _aciFlag);
								for (int freqSegIdx = 0;
								     freqSegIdx <
								     channel->segList.size();
								     ++freqSegIdx) {
									ChannelColor chanColor = std::get<
										2>(
										channel->segList
											[freqSegIdx]);
									if ((chanColor != BLACK)) {
										double chanStartFreq =
											channel->freqMHzList
												[freqSegIdx] *
											1.0e6;
										double chanStopFreq =
											channel->freqMHzList
												[freqSegIdx +
												 1] *
											1.0e6;
										bool hasOverlap = computeSpectralOverlapLoss(
											(double *)
												NULL,
											chanStartFreq,
											chanStopFreq,
											uls->getStartFreq(),
											uls->getStopFreq(),
											useACI,
											CConst::psdSpectralAlgorithm);

										if (hasOverlap) {
											double eirpLimit_dBm;
											if (std::isnan(
												    _reportUnavailPSDdBmPerMHz)) {
												eirpLimit_dBm =
													-std::numeric_limits<
														double>::
														infinity();
												std::get<
													2>(
													channel->segList
														[freqSegIdx]) =
													BLACK;
											} else {
												double bwMHz =
													(double)channel
														->bandwidth(
															freqSegIdx);
												eirpLimit_dBm =
													_reportUnavailPSDdBmPerMHz +
													10 * log10(bwMHz);
											}
											std::get<0>(
												channel->segList
													[freqSegIdx]) =
												eirpLimit_dBm;
											std::get<1>(
												channel->segList
													[freqSegIdx]) =
												eirpLimit_dBm;

											if ((eirpLimit_dBm <
											     eirpLimitList
												     [ulsIdx])) {
												eirpLimitList
													[ulsIdx] =
														eirpLimit_dBm;
											}
										}
									}
								}
							}
							minRLANDist = 0.0;
						} else {
							Vector3 ulsAntennaPointing =
								(segIdx == numPR ?
									 (divIdx == 0 ?
										  uls->getAntennaPointing() :
										  uls->getDiversityAntennaPointing()) :
									 uls->getPR(segIdx)
										 .pointing);

							double minAngleOffBoresightDeg = 0.0;
							if (contains2D) {
								double minAOBLon, minAOBLat,
									minAOBHeghtAMSL;
								minAngleOffBoresightDeg =
									_rlanRegion->calcMinAOB(
										ulsRxLatLon,
										ulsAntennaPointing,
										ulsRxHeightAMSL,
										minAOBLon,
										minAOBLat,
										minAOBHeghtAMSL);
								LOGGER_INFO(logger)
									<< std::setprecision(15)
									<< "FSID = " << uls->getID()
									<< " MIN_AOB = "
									<< minAngleOffBoresightDeg
									<< " at LON = " << minAOBLon
									<< " LAT = " << minAOBLat
									<< " HEIGHT_AMSL = "
									<< minAOBHeghtAMSL;
							}

							for (scanPtIdx = 0;
							     scanPtIdx < (int)scanPointList.size();
							     scanPtIdx++) {
								LatLon scanPt =
									scanPointList[scanPtIdx];

								// Use Haversine formula with
								// average earth radius of 6371 km
								double groundDistanceKm;
								{
									double lon1Rad =
										scanPt.second *
										M_PI / 180.0;
									double lat1Rad =
										scanPt.first *
										M_PI / 180.0;
									double lon2Rad =
										ulsRxLongitude *
										M_PI / 180.0;
									double lat2Rad =
										ulsRxLatitude *
										M_PI / 180.0;
									double slat = sin(
										(lat2Rad -
										 lat1Rad) /
										2);
									double slon = sin(
										(lon2Rad -
										 lon1Rad) /
										2);
									groundDistanceKm =
										2 *
										CConst::averageEarthRadius *
										asin(sqrt(
											slat * slat +
											cos(lat1Rad) *
												cos(lat2Rad) *
												slon *
												slon)) *
										1.0e-3;
								}

								for (rlanHtIdx = 0;
								     rlanHtIdx <
								     numRlanHt[scanPtIdx];
								     ++rlanHtIdx) {
									Vector3 rlanPosn =
										rlanPosnList
											[scanPtIdx]
											[rlanHtIdx];
									GeodeticCoord rlanCoord =
										rlanCoordList
											[scanPtIdx]
											[rlanHtIdx];
									lineOfSightVectorKm =
										ulsRxPos - rlanPosn;
									double distKm =
										lineOfSightVectorKm
											.len();
									double win2DistKm;
									if (_winner2UseGroundDistanceFlag) {
										win2DistKm =
											groundDistanceKm;
									} else {
										win2DistKm = distKm;
									}
									double fsplDistKm;
									if (_fsplUseGroundDistanceFlag) {
										fsplDistKm =
											groundDistanceKm;
									} else {
										fsplDistKm = distKm;
									}
									double dAP = rlanPosn.len();
									double duls =
										ulsRxPos.len();
									double elevationAngleTxDeg =
										90.0 -
										acos(rlanPosn.dot(
											     lineOfSightVectorKm) /
										     (dAP *
										      distKm)) *
											180.0 /
											M_PI;
									double elevationAngleRxDeg =
										90.0 -
										acos(ulsRxPos.dot(
											     -lineOfSightVectorKm) /
										     (duls *
										      distKm)) *
											180.0 /
											M_PI;

									double rlanAngleOffBoresightRad;
									double rlanDiscriminationGainDB;
									if (_rlanAntenna) {
										double cosAOB =
											_rlanPointing
												.dot(lineOfSightVectorKm) /
											distKm;
										if (cosAOB > 1.0) {
											cosAOB =
												1.0;
										} else if (cosAOB <
											   -1.0) {
											cosAOB =
												-1.0;
										}
										rlanAngleOffBoresightRad =
											acos(cosAOB);
										rlanDiscriminationGainDB =
											_rlanAntenna->gainDB(
												rlanAngleOffBoresightRad);
									} else {
										rlanAngleOffBoresightRad =
											0.0;
										rlanDiscriminationGainDB =
											0.0;
									}
									double rlanHtAboveTerrain =
										rlanCoord.heightKm *
											1000.0 -
										rlanTerrainHeight
											[scanPtIdx];

									if ((minRLANDist == -1.0) ||
									    (distKm * 1000.0 <
									     minRLANDist)) {
										minRLANDist =
											distKm *
											1000.0;
									}

									int chanIdx;
									std::vector<int> itmSegList;
									std::vector<double>
										RxPowerDBW_0PLList
											[2];
									std::vector<
										ExcThrParamClass>
										excThrParamList[2];
									for (chanIdx = 0;
									     chanIdx <
									     (int)_channelList
										     .size();
									     ++chanIdx) {
										ChannelStruct *channel =
											&(_channelList
												  [chanIdx]);
										ChannelType channelType =
											channel->type;
										itmSegList.clear();
										RxPowerDBW_0PLList[0]
											.clear();
										RxPowerDBW_0PLList[1]
											.clear();
										excThrParamList[0]
											.clear();
										excThrParamList[1]
											.clear();
										// First set state =
										// 0.  When state =
										// 0, FSPL is used
										// in place of ITM.
										// In state = 0, go
										// through all freq
										// segments and if
										// state=0 does not
										// limit EIRP those
										// segments are
										// done. Identity
										// list of segments
										// for which state =
										// 0 (FSPL in place
										// of ITM) does
										// limit EIRP and
										// full Path loss
										// calculation is
										// required. Next
										// set state = 1.
										// When state = 1,
										// the full path
										// loss model is
										// used. For the
										// range of
										// frequencies where
										// Path loss
										// calculation is
										// required,
										// calculate path
										// loss at min and
										// max frequencies,
										// then use linear
										// interpoplation
										// for intermediate
										// frequencies.
										bool contFlag =
											true;
										int state = 0;
										int freqSegIdx = 0;
										int itmSegIdx = -1;
										double itmStartPathLoss =
											quietNaN;
										double itmStopPathLoss =
											quietNaN;
										double itmStartFreqMHz =
											quietNaN;
										double itmStopFreqMHz =
											quietNaN;

										while (contFlag) {
											if (state ==
											    1) {
												freqSegIdx = itmSegList
													[itmSegIdx];
											}

											ChannelColor chanColor = std::get<
												2>(
												channel->segList
													[freqSegIdx]);
											if ((chanColor !=
											     BLACK) &&
											    (chanColor !=
											     RED)) {
												int chanStartFreqMHz =
													channel->freqMHzList
														[freqSegIdx];
												int chanStopFreqMHz =
													channel->freqMHzList
														[freqSegIdx +
														 1];

												bool useACI =
													(channelType == INQUIRED_FREQUENCY ?
														 false :
														 _aciFlag);
												CConst::SpectralAlgorithmEnum spectralAlgorithm =
													(channelType == INQUIRED_FREQUENCY ?
														 CConst::psdSpectralAlgorithm :
														 _channelResponseAlgorithm);
												// LOGGER_INFO(logger) << "COMPUTING SPECTRAL OVERLAP FOR FSID = " << uls->getID();
												double spectralOverlapLossDB;
												bool hasOverlap = computeSpectralOverlapLoss(
													&spectralOverlapLossDB,
													chanStartFreqMHz *
														1.0e6,
													chanStopFreqMHz *
														1.0e6,
													uls->getStartFreq(),
													uls->getStopFreq(),
													useACI,
													spectralAlgorithm);
												if (hasOverlap) {
													double rxPowerDBW_0PL
														[2];
													ExcThrParamClass excThrParam
														[2];
													double eirpLimit_dBm
														[2];
													double bandwidthMHz =
														(double)channel
															->bandwidth(
																freqSegIdx);
													int numBandEdge =
														(channelType == INQUIRED_FREQUENCY ?
															 2 :
															 1);

													double maxEIRPdBm;
													if (channelType ==
													    INQUIRED_FREQUENCY) {
														maxEIRPdBm =
															_inquiredFrequencyMaxPSD_dBmPerMHz +
															10.0 * log10(bandwidthMHz);
													} else {
														maxEIRPdBm =
															_maxEIRP_dBm;
													}

													for (int bandEdgeIdx =
														     0;
													     bandEdgeIdx <
													     numBandEdge;
													     ++bandEdgeIdx) {
														double evalFreqMHz;
														if (channelType ==
														    INQUIRED_FREQUENCY) {
															evalFreqMHz =
																(bandEdgeIdx == 0 ?
																	 chanStartFreqMHz :
																	 chanStopFreqMHz);
														} else {
															evalFreqMHz =
																(chanStartFreqMHz +
																 chanStopFreqMHz) /
																2.0;
														}
														double evalFreqHz =
															evalFreqMHz *
															1.0e6;

														if ((state ==
														     0) &&
														    (fsplDistKm ==
														     0)) {
															// Possible if FSPL distance is horizontal. This should be extremely rare
															continue;
														}

														double pathLoss;
														double rxPowerDBW;
														double I2NDB;
														double marginDB;

														if (state ==
														    0) {
															std::string
																buildingPenetrationModelStr;
															double buildingPenetrationCDF;
															double buildingPenetrationDB;

															std::string
																txClutterStr;
															std::string
																rxClutterStr;
															std::string
																pathLossModelStr;
															double pathLossCDF;
															std::string
																pathClutterTxModelStr;
															double pathClutterTxCDF;
															double pathClutterTxDB;
															std::string
																pathClutterRxModelStr;
															double pathClutterRxCDF;
															double pathClutterRxDB;
															double rxGainDB;
															double discriminationGain;
															std::string
																rxAntennaSubModelStr;
															double angleOffBoresightDeg;
															double nearFieldOffsetDB;
															double nearField_xdb;
															double nearField_u;
															double nearField_eff;
															double reflectorD0;
															double reflectorD1;

															computePathLoss(
																contains2D ?
																	CConst::FSPLPathLossModel :
																	_pathLossModel,
																true,
																rlanPropEnv
																	[scanPtIdx],
																fsPropEnv,
																rlanNlcdLandCat
																	[scanPtIdx],
																nlcdLandCatRx,
																distKm,
																fsplDistKm,
																win2DistKm,
																evalFreqHz,
																rlanCoord
																	.longitudeDeg,
																rlanCoord
																	.latitudeDeg,
																rlanHtAboveTerrain,
																elevationAngleTxDeg,
																ulsRxLongitude,
																ulsRxLatitude,
																ulsRxHeightAGL,
																elevationAngleRxDeg,
																pathLoss,
																pathClutterTxDB,
																pathClutterRxDB,
																pathLossModelStr,
																pathLossCDF,
																pathClutterTxModelStr,
																pathClutterTxCDF,
																pathClutterRxModelStr,
																pathClutterRxCDF,
																&txClutterStr,
																&rxClutterStr,
																&(uls->ITMHeightProfile),
																&(uls->isLOSHeightProfile),
																&(uls->isLOSSurfaceFrac)
#if DEBUG_AFC
																	,
																uls->ITMHeightType
#endif
															);
															buildingPenetrationDB = computeBuildingPenetration(
																_buildingType,
																elevationAngleTxDeg,
																evalFreqHz,
																buildingPenetrationModelStr,
																buildingPenetrationCDF);

															if (contains2D) {
																angleOffBoresightDeg =
																	minAngleOffBoresightDeg;
															} else {
																angleOffBoresightDeg =
																	acos(ulsAntennaPointing
																		     .dot(-(lineOfSightVectorKm
																				    .normalized()))) *
																	180.0 /
																	M_PI;
															}
															if (segIdx ==
															    numPR) {
																rxGainDB = uls->computeRxGain(
																	angleOffBoresightDeg,
																	elevationAngleRxDeg,
																	evalFreqHz,
																	rxAntennaSubModelStr,
																	divIdx);
															} else {
																discriminationGain =
																	uls->getPR(segIdx)
																		.computeDiscriminationGain(
																			angleOffBoresightDeg,
																			elevationAngleRxDeg,
																			evalFreqHz,
																			reflectorD0,
																			reflectorD1);
																rxGainDB =
																	uls->getPR(segIdx)
																		.effectiveGain +
																	discriminationGain;
															}

															nearFieldOffsetDB =
																0.0;
															nearField_xdb =
																quietNaN;
															nearField_u =
																quietNaN;
															nearField_eff =
																quietNaN;
															if (segIdx ==
															    numPR) {
																if (_nearFieldAdjFlag &&
																    (distKm *
																	     1000.0 <
																     uls->getRxNearFieldDistLimit()) &&
																    (angleOffBoresightDeg <
																     90.0)) {
																	bool unii5Flag = computeSpectralOverlapLoss(
																		(double *)
																			NULL,
																		uls->getStartFreq(),
																		uls->getStopFreq(),
																		5925.0e6,
																		6425.0e6,
																		false,
																		CConst::psdSpectralAlgorithm);
																	double Fc;
																	if (unii5Flag) {
																		Fc = 6175.0e6;
																	} else {
																		Fc = 6700.0e6;
																	}
																	nearField_eff =
																		uls->getRxNearFieldAntEfficiency();
																	double D =
																		uls->getRxNearFieldAntDiameter();

																	nearField_xdb =
																		10.0 *
																		log10(CConst::c *
																		      distKm *
																		      1000.0 /
																		      (2 *
																		       Fc *
																		       D *
																		       D));
																	nearField_u =
																		(Fc *
																		 D *
																		 sin(angleOffBoresightDeg *
																		     M_PI /
																		     180.0) /
																		 CConst::c);

																	nearFieldOffsetDB = _nfa->computeNFA(
																		nearField_xdb,
																		nearField_u,
																		nearField_eff);
																}
															}

															rxPowerDBW_0PL[bandEdgeIdx] =
																(maxEIRPdBm -
																 30.0) +
																rlanDiscriminationGainDB -
																_bodyLossDB -
																buildingPenetrationDB -
																pathClutterTxDB -
																pathClutterRxDB +
																rxGainDB +
																nearFieldOffsetDB -
																spectralOverlapLossDB -
																_polarizationLossDB -
																uls->getRxAntennaFeederLossDB();
															if (excthrGc) {
																excThrParam[bandEdgeIdx]
																	.rlanDiscriminationGainDB =
																	rlanDiscriminationGainDB;
																excThrParam[bandEdgeIdx]
																	.bodyLossDB =
																	_bodyLossDB;
																excThrParam[bandEdgeIdx]
																	.buildingPenetrationModelStr =
																	buildingPenetrationModelStr;
																excThrParam[bandEdgeIdx]
																	.buildingPenetrationCDF =
																	buildingPenetrationCDF;
																excThrParam[bandEdgeIdx]
																	.buildingPenetrationDB =
																	buildingPenetrationDB;
																excThrParam[bandEdgeIdx]
																	.angleOffBoresightDeg =
																	angleOffBoresightDeg;
																excThrParam[bandEdgeIdx]
																	.pathLossModelStr =
																	pathLossModelStr;
																excThrParam[bandEdgeIdx]
																	.pathLossCDF =
																	pathLossCDF;
																excThrParam[bandEdgeIdx]
																	.pathClutterTxModelStr =
																	pathClutterTxModelStr;
																excThrParam[bandEdgeIdx]
																	.pathClutterTxCDF =
																	pathClutterTxCDF;
																excThrParam[bandEdgeIdx]
																	.pathClutterTxDB =
																	pathClutterTxDB;
																excThrParam[bandEdgeIdx]
																	.txClutterStr =
																	txClutterStr;
																excThrParam[bandEdgeIdx]
																	.pathClutterRxModelStr =
																	pathClutterRxModelStr;
																excThrParam[bandEdgeIdx]
																	.pathClutterRxCDF =
																	pathClutterRxCDF;
																excThrParam[bandEdgeIdx]
																	.pathClutterRxDB =
																	pathClutterRxDB;
																excThrParam[bandEdgeIdx]
																	.rxClutterStr =
																	rxClutterStr;
																excThrParam[bandEdgeIdx]
																	.rxGainDB =
																	rxGainDB;
																excThrParam[bandEdgeIdx]
																	.discriminationGain =
																	discriminationGain;
																excThrParam[bandEdgeIdx]
																	.rxAntennaSubModelStr =
																	rxAntennaSubModelStr;
																excThrParam[bandEdgeIdx]
																	.nearFieldOffsetDB =
																	nearFieldOffsetDB;
																excThrParam[bandEdgeIdx]
																	.spectralOverlapLossDB =
																	spectralOverlapLossDB;
																excThrParam[bandEdgeIdx]
																	.polarizationLossDB =
																	_polarizationLossDB;
																excThrParam[bandEdgeIdx]
																	.rxAntennaFeederLossDB =
																	uls->getRxAntennaFeederLossDB();
																excThrParam[bandEdgeIdx]
																	.nearField_xdb =
																	nearField_xdb;
																excThrParam[bandEdgeIdx]
																	.nearField_u =
																	nearField_u;
																excThrParam[bandEdgeIdx]
																	.nearField_eff =
																	nearField_eff;
																excThrParam[bandEdgeIdx]
																	.reflectorD0 =
																	reflectorD0;
																excThrParam[bandEdgeIdx]
																	.reflectorD1 =
																	reflectorD1;
															}
														} else {
															pathLoss =
																(itmStartPathLoss *
																	 (itmStopFreqMHz -
																	  evalFreqMHz) +
																 itmStopPathLoss *
																	 (evalFreqMHz -
																	  itmStartFreqMHz)) /
																(itmStopFreqMHz -
																 itmStartFreqMHz);
															rxPowerDBW_0PL[bandEdgeIdx] = RxPowerDBW_0PLList
																[bandEdgeIdx]
																[itmSegIdx];
															if (excthrGc) {
																excThrParam[bandEdgeIdx] = excThrParamList
																	[bandEdgeIdx]
																	[itmSegIdx];
															}
														}

														rxPowerDBW =
															rxPowerDBW_0PL
																[bandEdgeIdx] -
															pathLoss;

														I2NDB = rxPowerDBW -
															uls->getNoiseLevelDBW();

														marginDB =
															_IoverN_threshold_dB -
															I2NDB;

														eirpLimit_dBm[bandEdgeIdx] =
															maxEIRPdBm +
															marginDB;

														if (eirpGc) {
															eirpGc.callsign =
																uls->getCallsign();
															eirpGc.pathNum =
																uls->getPathNumber();
															eirpGc.ulsId =
																uls->getID();
															eirpGc.segIdx =
																segIdx;
															eirpGc.divIdx =
																divIdx;
															eirpGc.scanLat =
																rlanCoord
																	.latitudeDeg;
															eirpGc.scanLon =
																rlanCoord
																	.longitudeDeg;
															eirpGc.scanAgl =
																rlanHtAboveTerrain;
															eirpGc.scanAmsl =
																rlanCoord
																	.heightKm *
																1000.0;
															eirpGc.scanPtIdx =
																scanPtIdx;
															eirpGc.distKm =
																distKm;
															eirpGc.elevationAngleTx =
																elevationAngleTxDeg;
															eirpGc.channel =
																channel->index;
															eirpGc.chanStartMhz =
																channel->freqMHzList
																	[freqSegIdx];
															eirpGc.chanEndMhz =
																channel->freqMHzList
																	[freqSegIdx +
																	 1];
															eirpGc.chanBwMhz =
																bandwidthMHz;
															eirpGc.chanType =
																channelType;
															eirpGc.eirpLimit = eirpLimit_dBm
																[bandEdgeIdx];
															eirpGc.fspl =
																(state ==
																 0);
															eirpGc.pathLossDb =
																pathLoss;
															eirpGc.configPathLossModel =
																_pathLossModel;
															eirpGc.resultedPathLossModel =
																excThrParam[bandEdgeIdx]
																	.pathLossModelStr;
															eirpGc.buildingPenetrationDb =
																excThrParam[bandEdgeIdx]
																	.buildingPenetrationDB;
															eirpGc.offBoresight =
																excThrParam[bandEdgeIdx]
																	.angleOffBoresightDeg;
															eirpGc.rxGainDb =
																excThrParam[bandEdgeIdx]
																	.rxGainDB;
															eirpGc.discriminationGainDb =
																excThrParam[bandEdgeIdx]
																	.rlanDiscriminationGainDB;
															eirpGc.txPropEnv = rlanPropEnv
																[scanPtIdx];
															eirpGc.nlcdTx = rlanNlcdLandCat
																[scanPtIdx];
															eirpGc.pathClutterTxModel =
																excThrParam[bandEdgeIdx]
																	.pathClutterTxModelStr;
															eirpGc.pathClutterTxDb =
																excThrParam[bandEdgeIdx]
																	.pathClutterTxDB;
															eirpGc.txClutter =
																excThrParam[bandEdgeIdx]
																	.txClutterStr;
															eirpGc.rxPropEnv =
																fsPropEnv;
															eirpGc.nlcdRx =
																nlcdLandCatRx;
															eirpGc.pathClutterRxModel =
																excThrParam[bandEdgeIdx]
																	.pathClutterRxModelStr;
															eirpGc.pathClutterRxDb =
																excThrParam[bandEdgeIdx]
																	.pathClutterRxDB;
															eirpGc.rxClutter =
																excThrParam[bandEdgeIdx]
																	.rxClutterStr;
															eirpGc.nearFieldOffsetDb =
																excThrParam[bandEdgeIdx]
																	.nearFieldOffsetDB;
															eirpGc.spectralOverlapLossDb =
																spectralOverlapLossDB;
															eirpGc.ulsAntennaFeederLossDb =
																uls->getRxAntennaFeederLossDB();
															eirpGc.rxPowerDbW =
																rxPowerDBW;
															eirpGc.ulsNoiseLevelDbW =
																uls->getNoiseLevelDBW();
															eirpGc.completeRow();
														}

														// Link budget calculations are written to the exceed threshold file if I2NDB exceeds _visibilityThreshold or
														// when link distance is less than _closeInDist.  If state==1, these links are always written.  If state==0.
														// these links are written only if _printSkippedLinksFlag is set.
														if (excthrGc &&
														    (((state ==
														       0) &&
														      (_printSkippedLinksFlag)) ||
														     (state ==
														      1)) &&
														    (std::isnan(
															     rxPowerDBW) ||
														     (I2NDB >
														      _visibilityThreshold) ||
														     (distKm *
															      1000 <
														      _closeInDist))) {
															double d1;
															double d2;
															double pathDifference;
															double fresnelIndex =
																-1.0;
															double ulsWavelength =
																CConst::c /
																((uls->getStartFreq() +
																  uls->getStopFreq()) /
																 2);
															if (ulsSegmentDistance !=
															    -1.0) {
																const Vector3 ulsTxPos =
																	(segIdx ?
																		 uls->getPR(segIdx -
																			    1)
																			 .positionTx :
																		 uls->getTxPosition());
																d1 = (ulsRxPos -
																      rlanPosn)
																	     .len() *
																     1000;
																d2 = (ulsTxPos -
																      rlanPosn)
																	     .len() *
																     1000;
																pathDifference =
																	d1 +
																	d2 -
																	ulsSegmentDistance;
																fresnelIndex =
																	pathDifference /
																	(ulsWavelength /
																	 2);
															} else {
																d1 = (ulsRxPos -
																      rlanPosn)
																	     .len() *
																     1000;
																d2 = -1.0;
																pathDifference =
																	-1.0;
															}

															std::string
																rxAntennaTypeStr;
															if (segIdx ==
															    numPR) {
																CConst::ULSAntennaTypeEnum ulsRxAntennaType =
																	uls->getRxAntennaType();
																if (ulsRxAntennaType ==
																    CConst::LUTAntennaType) {
																	rxAntennaTypeStr = std::string(
																		uls->getRxAntenna()
																			->get_strid());
																} else {
																	rxAntennaTypeStr =
																		std::string(
																			CConst::strULSAntennaTypeList
																				->type_to_str(
																					ulsRxAntennaType)) +
																		excThrParam[bandEdgeIdx]
																			.rxAntennaSubModelStr;
																}
															} else {
																if (uls->getPR(segIdx)
																	    .type ==
																    CConst::backToBackAntennaPRType) {
																	CConst::ULSAntennaTypeEnum ulsRxAntennaType =
																		uls->getPR(segIdx)
																			.antennaType;
																	if (ulsRxAntennaType ==
																	    CConst::LUTAntennaType) {
																		rxAntennaTypeStr = std::string(
																			uls->getPR(segIdx)
																				.antenna
																				->get_strid());
																	} else {
																		rxAntennaTypeStr =
																			std::string(
																				CConst::strULSAntennaTypeList
																					->type_to_str(
																						ulsRxAntennaType)) +
																			excThrParam[bandEdgeIdx]
																				.rxAntennaSubModelStr;
																	}
																} else {
																	rxAntennaTypeStr =
																		"";
																}
															}

															std::string bldgTypeStr =
																(_fixedBuildingLossFlag ?
																	 "INDOOR_FIXED" :
																 _buildingType ==
																		 CConst::noBuildingType ?
																	 "OUTDOOR" :
																 _buildingType ==
																		 CConst::traditionalBuildingType ?
																	 "TRADITIONAL" :
																	 "THERMALLY_EFFICIENT");

															excthrGc->fsid =
																uls->getID();
															excthrGc->region =
																uls->getRegion();
															excthrGc->dbName = std::get<
																0>(
																_ulsDatabaseList
																	[uls->getDBIdx()]);
															excthrGc->rlanPosnIdx =
																rlanHtIdx;
															excthrGc->callsign =
																uls->getCallsign();
															excthrGc->fsLon =
																uls->getRxLongitudeDeg();
															excthrGc->fsLat =
																uls->getRxLatitudeDeg();
															excthrGc->fsAgl =
																divIdx == 0 ?
																	uls->getRxHeightAboveTerrain() :
																	uls->getDiversityHeightAboveTerrain();
															excthrGc->fsTerrainHeight =
																uls->getRxTerrainHeight();
															excthrGc->fsTerrainSource =
																_terrainDataModel
																	->getSourceName(
																		uls->getRxHeightSource());
															excthrGc->fsPropEnv =
																ulsRxPropEnv;
															excthrGc->numPr =
																uls->getNumPR();
															excthrGc->divIdx =
																divIdx;
															excthrGc->segIdx =
																segIdx;
															excthrGc->segRxLon =
																ulsRxLongitude;
															excthrGc->segRxLat =
																ulsRxLatitude;

															if ((segIdx <
															     numPR) &&
															    (uls->getPR(segIdx)
																     .type ==
															     CConst::billboardReflectorPRType)) {
																PRClass &pr = uls->getPR(
																	segIdx);
																excthrGc->refThetaIn =
																	pr.reflectorThetaIN;
																excthrGc->refKs =
																	pr.reflectorKS;
																excthrGc->refQ =
																	pr.reflectorQ;
																excthrGc->refD0 =
																	excThrParam[bandEdgeIdx]
																		.reflectorD0;
																excthrGc->refD1 =
																	excThrParam[bandEdgeIdx]
																		.reflectorD1;
															}

															excthrGc->rlanLon =
																rlanCoord
																	.longitudeDeg;
															excthrGc->rlanLat =
																rlanCoord
																	.latitudeDeg;
															excthrGc->rlanAgl =
																rlanCoord.heightKm *
																	1000.0 -
																rlanTerrainHeight
																	[scanPtIdx];
															excthrGc->rlanTerrainHeight = rlanTerrainHeight
																[scanPtIdx];
															excthrGc->rlanTerrainSource =
																_terrainDataModel
																	->getSourceName(
																		rlanHeightSource
																			[scanPtIdx]);
															excthrGc->rlanPropEnv =
																CConst::strPropEnvList
																	->type_to_str(
																		rlanPropEnv
																			[scanPtIdx]);
															excthrGc->rlanFsDist =
																distKm;
															excthrGc->rlanFsGroundDist =
																groundDistanceKm;
															excthrGc->rlanElevAngle =
																elevationAngleTxDeg;
															excthrGc->boresightAngle =
																excThrParam[bandEdgeIdx]
																	.angleOffBoresightDeg;
															excthrGc->rlanTxEirp =
																maxEIRPdBm;
															if (_rlanAntenna) {
																excthrGc->rlanAntennaModel =
																	_rlanAntenna
																		->get_strid();
																excthrGc->rlanAOB =
																	rlanAngleOffBoresightRad *
																	180.0 /
																	M_PI;
															} else {
																excthrGc->rlanAntennaModel =
																	"";
																excthrGc->rlanAOB =
																	-1.0;
															}
															excthrGc->rlanDiscriminationGainDB =
																rlanDiscriminationGainDB;
															excthrGc->bodyLoss =
																_bodyLossDB;
															excthrGc->rlanClutterCategory =
																excThrParam[bandEdgeIdx]
																	.txClutterStr;
															excthrGc->fsClutterCategory =
																excThrParam[bandEdgeIdx]
																	.rxClutterStr;
															excthrGc->buildingType =
																bldgTypeStr;
															excthrGc->buildingPenetration =
																excThrParam[bandEdgeIdx]
																	.buildingPenetrationDB;
															excthrGc->buildingPenetrationModel =
																excThrParam[bandEdgeIdx]
																	.buildingPenetrationModelStr;
															excthrGc->buildingPenetrationCdf =
																excThrParam[bandEdgeIdx]
																	.buildingPenetrationCDF;
															excthrGc->pathLoss =
																pathLoss;
															excthrGc->pathLossModel =
																excThrParam[bandEdgeIdx]
																	.pathLossModelStr;
															excthrGc->pathLossCdf =
																excThrParam[bandEdgeIdx]
																	.pathLossCDF;
															excthrGc->pathClutterTx =
																excThrParam[bandEdgeIdx]
																	.pathClutterTxDB;
															excthrGc->pathClutterTxMode =
																excThrParam[bandEdgeIdx]
																	.pathClutterTxModelStr;
															excthrGc->pathClutterTxCdf =
																excThrParam[bandEdgeIdx]
																	.pathClutterTxCDF;
															excthrGc->pathClutterRx =
																excThrParam[bandEdgeIdx]
																	.pathClutterRxDB;
															excthrGc->pathClutterRxMode =
																excThrParam[bandEdgeIdx]
																	.pathClutterRxModelStr;
															excthrGc->pathClutterRxCdf =
																excThrParam[bandEdgeIdx]
																	.pathClutterRxCDF;
															excthrGc->rlanBandwidth =
																bandwidthMHz;
															excthrGc->rlanStartFreq =
																chanStartFreqMHz;
															excthrGc->rlanStopFreq =
																chanStopFreqMHz;
															excthrGc->ulsStartFreq =
																uls->getStartFreq() *
																1.0e-6;
															excthrGc->ulsStopFreq =
																uls->getStopFreq() *
																1.0e-6;
															excthrGc->antType =
																rxAntennaTypeStr;
															excthrGc->antCategory =
																CConst::strAntennaCategoryList
																	->type_to_str(
																		segIdx == numPR ?
																			uls->getRxAntennaCategory() :
																			uls->getPR(segIdx)
																				.antCategory);
															excthrGc->antGainPeak =
																divIdx == 0 ?
																	uls->getRxGain() :
																	uls->getDiversityGain();

															if (segIdx !=
															    numPR) {
																excthrGc->prType =
																	CConst::strPRTypeList
																		->type_to_str(
																			uls->getPR(segIdx)
																				.type);
																excthrGc->prEffectiveGain =
																	uls->getPR(segIdx)
																		.effectiveGain;
																excthrGc->prDiscrinminationGain =
																	excThrParam[bandEdgeIdx]
																		.discriminationGain;
															}

															excthrGc->fsGainToRlan =
																excThrParam[bandEdgeIdx]
																	.rxGainDB;
															if (!std::isnan(
																    excThrParam[bandEdgeIdx]
																	    .nearField_xdb)) {
																excthrGc->fsNearFieldXdb =
																	excThrParam[bandEdgeIdx]
																		.nearField_xdb;
															}
															if (!std::isnan(
																    excThrParam[bandEdgeIdx]
																	    .nearField_u)) {
																excthrGc->fsNearFieldU =
																	excThrParam[bandEdgeIdx]
																		.nearField_u;
															}
															if (!std::isnan(
																    excThrParam[bandEdgeIdx]
																	    .nearField_eff)) {
																excthrGc->fsNearFieldEff =
																	excThrParam[bandEdgeIdx]
																		.nearField_eff;
															}
															excthrGc->fsNearFieldOffset =
																excThrParam[bandEdgeIdx]
																	.nearFieldOffsetDB;
															excthrGc->spectralOverlapLoss =
																spectralOverlapLossDB;
															excthrGc->polarizationLoss =
																_polarizationLossDB;
															excthrGc->fsRxFeederLoss =
																uls->getRxAntennaFeederLossDB();
															excthrGc->fsRxPwr =
																rxPowerDBW;
															excthrGc->fsIN =
																I2NDB;
															excthrGc->eirpLimit = eirpLimit_dBm
																[bandEdgeIdx];
															excthrGc->fsSegDist =
																ulsSegmentDistance;
															excthrGc->rlanCenterFreq =
																evalFreqMHz;
															excthrGc->fsTxToRlanDist =
																d2;
															excthrGc->pathDifference =
																pathDifference;
															excthrGc->ulsWavelength =
																ulsWavelength *
																1000;
															excthrGc->fresnelIndex =
																fresnelIndex;

															excthrGc->completeRow();
														}
													}

													// Trying Free Space Path Loss then (if not skipped) - configured Path Loss.
													bool skip;

													if (state ==
													    0) {
														// Skipping further computation if Free Space path loss
														// EIRP is larger than already established (hence
														// configured path loss will be even larger),
														// otherwise proceeding with (potentially slow)
														// configured path loss computation

														// 1dB allowance to accommodate for amplifying clutters and other artifacts
														if (contains2D) {
															skip = false;
														} else if (
															((eirpLimit_dBm
																  [0] -
															  1) <
															 std::get<
																 0>(
																 channel->segList
																	 [freqSegIdx])) ||
															((numBandEdge ==
															  2) &&
															 ((eirpLimit_dBm
																   [1] -
															   1) <
															  std::get<
																  1>(
																  channel->segList
																	  [freqSegIdx])))) {
															itmSegList
																.push_back(
																	freqSegIdx);
															for (int bandEdgeIdx =
																     0;
															     bandEdgeIdx <
															     numBandEdge;
															     ++bandEdgeIdx) {
																RxPowerDBW_0PLList[bandEdgeIdx]
																	.push_back(
																		rxPowerDBW_0PL
																			[bandEdgeIdx]);
																if (excthrGc) {
																	excThrParamList[bandEdgeIdx]
																		.push_back(
																			excThrParam
																				[bandEdgeIdx]);
																}
															}
															skip = true;
														} else {
															skip = true;
														}
													} else {
														skip = false;
													}

													// When _printSkippedLinksFlag set, links analyzed with FSPL that are skipped are still inserted into the exc_thr file.
													// This is useful for testing and debugging.  Note that the extra printing impacts execution speed.  When _printSkippedLinksFlag is
													// not set, skipped links are no inserted in the exc_thr file, so execution speed is not impacted.
													// if ((!_printSkippedLinksFlag) && (skip)) {
													// 	continue;
													// }

													if (!skip) {
														if ((contains2D) &&
														    (!std::isnan(
															    _reportUnavailPSDdBmPerMHz))) {
															double clipeirpLimit_dBm =
																_reportUnavailPSDdBmPerMHz +
																10 * log10(bandwidthMHz);
															for (int bandEdgeIdx =
																     0;
															     bandEdgeIdx <
															     numBandEdge;
															     ++bandEdgeIdx) {
																if (eirpLimit_dBm
																	    [bandEdgeIdx] <
																    clipeirpLimit_dBm) {
																	eirpLimit_dBm
																		[bandEdgeIdx] =
																			clipeirpLimit_dBm;
																}
															}
														}
														if (channelType ==
														    ChannelType::
															    INQUIRED_CHANNEL) {
															if ((std::get<
																     2>(
																     channel->segList
																	     [freqSegIdx]) !=
															     RED) &&
															    (eirpLimit_dBm
																     [0] <
															     std::get<
																     0>(
																     channel->segList
																	     [freqSegIdx]))) {
																if (eirpLimit_dBm
																	    [0] <
																    _minEIRP_dBm) {
																	channel->segList
																		[freqSegIdx] =
																		std::make_tuple(
																			_minEIRP_dBm,
																			_minEIRP_dBm,
																			RED);
																} else {
																	std::get<
																		0>(
																		channel->segList
																			[freqSegIdx]) = eirpLimit_dBm
																		[0];
																	std::get<
																		1>(
																		channel->segList
																			[freqSegIdx]) = eirpLimit_dBm
																		[0];
																}
															}
															if ((eirpLimit_dBm
																     [0] <
															     eirpLimitList
																     [ulsIdx])) {
																eirpLimitList[ulsIdx] = eirpLimit_dBm
																	[0];
															}
														} else {
															// INQUIRED_FREQUENCY
															if (std::get<
																    2>(
																    channel->segList
																	    [freqSegIdx]) !=
															    RED) {
																bool redFlag =
																	true;
																for (int bandEdgeIdx =
																	     0;
																     bandEdgeIdx <
																     numBandEdge;
																     ++bandEdgeIdx) {
																	double bandEdgeVal =
																		((bandEdgeIdx ==
																		  0) ?
																			 std::get<
																				 0>(
																				 channel->segList
																					 [freqSegIdx]) :
																			 std::get<
																				 1>(
																				 channel->segList
																					 [freqSegIdx]));
																	if (eirpLimit_dBm
																		    [bandEdgeIdx] <
																	    bandEdgeVal) {
																		if (bandEdgeIdx ==
																		    0) {
																			std::get<
																				0>(
																				channel->segList
																					[freqSegIdx]) = eirpLimit_dBm
																				[bandEdgeIdx];
																		} else {
																			std::get<
																				1>(
																				channel->segList
																					[freqSegIdx]) = eirpLimit_dBm
																				[bandEdgeIdx];
																		}
																		bandEdgeVal = eirpLimit_dBm
																			[bandEdgeIdx];
																	}
																	double psd =
																		bandEdgeVal -
																		10.0 * log(bandwidthMHz) /
																			log(10.0);
																	if (psd >
																	    _minPSD_dBmPerMHz) {
																		redFlag =
																			false;
																	}
																}
																if (redFlag) {
																	double eirp_dBm =
																		_minPSD_dBmPerMHz +
																		10.0 * log10(bandwidthMHz);
																	channel->segList
																		[freqSegIdx] =
																		std::make_tuple(
																			eirp_dBm,
																			eirp_dBm,
																			RED);
																}
															}
														}
													}
												}
											}

											if (state ==
											    0) {
												freqSegIdx++;
												if (freqSegIdx ==
												    channel->segList
													    .size()) {
													std::string
														txClutterStr;
													std::string
														rxClutterStr;
													std::string
														pathLossModelStr;
													double pathLossCDF;
													std::string
														pathClutterTxModelStr;
													double pathClutterTxCDF;
													double pathClutterTxDB;
													std::string
														pathClutterRxModelStr;
													double pathClutterRxCDF;
													double pathClutterRxDB;

													int numItmSeg =
														itmSegList
															.size();
													if ((numItmSeg >
													     1) ||
													    ((numItmSeg >
													      0) &&
													     (channelType ==
													      ChannelType::
														      INQUIRED_FREQUENCY))) {
														if (channelType ==
														    ChannelType::
															    INQUIRED_FREQUENCY) {
															itmStartFreqMHz =
																channel->freqMHzList
																	[itmSegList
																		 [0]];
															itmStopFreqMHz =
																channel->freqMHzList
																	[itmSegList
																		 [numItmSeg -
																		  1] +
																	 1];
														} else {
															itmStartFreqMHz =
																(channel->freqMHzList
																	 [itmSegList
																		  [0]] +
																 channel->freqMHzList
																	 [itmSegList
																		  [0] +
																	  1]) /
																2.0;
															itmStopFreqMHz =
																(channel->freqMHzList
																	 [itmSegList
																		  [numItmSeg]] +
																 channel->freqMHzList
																	 [itmSegList
																		  [numItmSeg] +
																	  1]) /
																2.0;
														}

														computePathLoss(
															contains2D ?
																CConst::FSPLPathLossModel :
																_pathLossModel,
															false,
															rlanPropEnv
																[scanPtIdx],
															fsPropEnv,
															rlanNlcdLandCat
																[scanPtIdx],
															nlcdLandCatRx,
															distKm,
															fsplDistKm,
															win2DistKm,
															itmStartFreqMHz *
																1.0e6,
															rlanCoord
																.longitudeDeg,
															rlanCoord
																.latitudeDeg,
															rlanHtAboveTerrain,
															elevationAngleTxDeg,
															ulsRxLongitude,
															ulsRxLatitude,
															ulsRxHeightAGL,
															elevationAngleRxDeg,
															itmStartPathLoss,
															pathClutterTxDB,
															pathClutterRxDB,
															pathLossModelStr,
															pathLossCDF,
															pathClutterTxModelStr,
															pathClutterTxCDF,
															pathClutterRxModelStr,
															pathClutterRxCDF,
															&txClutterStr,
															&rxClutterStr,
															&(uls->ITMHeightProfile),
															&(uls->isLOSHeightProfile),
															&(uls->isLOSSurfaceFrac)
#if DEBUG_AFC
																,
															uls->ITMHeightType
#endif
														);

														computePathLoss(
															contains2D ?
																CConst::FSPLPathLossModel :
																_pathLossModel,
															false,
															rlanPropEnv
																[scanPtIdx],
															fsPropEnv,
															rlanNlcdLandCat
																[scanPtIdx],
															nlcdLandCatRx,
															distKm,
															fsplDistKm,
															win2DistKm,
															itmStopFreqMHz *
																1.0e6,
															rlanCoord
																.longitudeDeg,
															rlanCoord
																.latitudeDeg,
															rlanHtAboveTerrain,
															elevationAngleTxDeg,
															ulsRxLongitude,
															ulsRxLatitude,
															ulsRxHeightAGL,
															elevationAngleRxDeg,
															itmStopPathLoss,
															pathClutterTxDB,
															pathClutterRxDB,
															pathLossModelStr,
															pathLossCDF,
															pathClutterTxModelStr,
															pathClutterTxCDF,
															pathClutterRxModelStr,
															pathClutterRxCDF,
															&txClutterStr,
															&rxClutterStr,
															&(uls->ITMHeightProfile),
															&(uls->isLOSHeightProfile),
															&(uls->isLOSSurfaceFrac)
#if DEBUG_AFC
																,
															uls->ITMHeightType
#endif
														);
														state = 1;
														itmSegIdx =
															0;
													} else if (
														numItmSeg ==
														1) {
														itmStartFreqMHz =
															channel->freqMHzList
																[itmSegList
																	 [0]];
														itmStopFreqMHz =
															channel->freqMHzList
																[itmSegList
																	 [0] +
																 1];

														computePathLoss(
															contains2D ?
																CConst::FSPLPathLossModel :
																_pathLossModel,
															false,
															rlanPropEnv
																[scanPtIdx],
															fsPropEnv,
															rlanNlcdLandCat
																[scanPtIdx],
															nlcdLandCatRx,
															distKm,
															fsplDistKm,
															win2DistKm,
															(itmStartFreqMHz +
															 itmStopFreqMHz) *
																0.5e6,
															rlanCoord
																.longitudeDeg,
															rlanCoord
																.latitudeDeg,
															rlanHtAboveTerrain,
															elevationAngleTxDeg,
															ulsRxLongitude,
															ulsRxLatitude,
															ulsRxHeightAGL,
															elevationAngleRxDeg,
															itmStartPathLoss,
															pathClutterTxDB,
															pathClutterRxDB,
															pathLossModelStr,
															pathLossCDF,
															pathClutterTxModelStr,
															pathClutterTxCDF,
															pathClutterRxModelStr,
															pathClutterRxCDF,
															&txClutterStr,
															&rxClutterStr,
															&(uls->ITMHeightProfile),
															&(uls->isLOSHeightProfile),
															&(uls->isLOSSurfaceFrac)
#if DEBUG_AFC
																,
															uls->ITMHeightType
#endif
														);

														itmStopPathLoss =
															itmStartPathLoss;

														state = 1;
														itmSegIdx =
															0;
													} else {
														contFlag =
															false;
													}
												}
											} else {
												itmSegIdx++;
												if (itmSegIdx ==
												    itmSegList
													    .size()) {
													contFlag =
														false;
												}
											}
										}
									}

#if DEBUG_AFC
									if (traceFlag &&
									    (!contains2D)) {
										if (uls->ITMHeightProfile) {
											double lon1Rad =
												rlanCoord
													.longitudeDeg *
												M_PI /
												180.0;
											double lat1Rad =
												rlanCoord
													.latitudeDeg *
												M_PI /
												180.0;
											int N = ((int)uls->ITMHeightProfile
													 [0]) +
												1;
											for (int ptIdx =
												     0;
											     ptIdx <
											     N;
											     ptIdx++) {
												Vector3 losPathPosn =
													(((double)(N -
														   1 -
														   ptIdx)) /
													 (N -
													  1)) * rlanPosn +
													(((double)ptIdx) /
													 (N -
													  1)) * ulsRxPos;
												GeodeticCoord losPathPosnGeodetic =
													EcefModel::ecefToGeodetic(
														losPathPosn);

												double ptLon =
													losPathPosnGeodetic
														.longitudeDeg;
												double ptLat =
													losPathPosnGeodetic
														.latitudeDeg;
												double losPathHeight =
													losPathPosnGeodetic
														.heightKm *
													1000.0;

												double lon2Rad =
													ptLon *
													M_PI /
													180.0;
												double lat2Rad =
													ptLat *
													M_PI /
													180.0;
												double slat = sin(
													(lat2Rad -
													 lat1Rad) /
													2);
												double slon = sin(
													(lon2Rad -
													 lon1Rad) /
													2);
												double ptDistKm =
													2 *
													CConst::averageEarthRadius *
													asin(sqrt(
														slat * slat +
														cos(lat1Rad) *
															cos(lat2Rad) *
															slon *
															slon)) *
													1.0e-3;

												pathTraceGc
													.ptId =
													(boost::format(
														 "PT_%d") %
													 ptIdx)
														.str();
												pathTraceGc
													.lon =
													ptLon;
												pathTraceGc
													.lat =
													ptLat;
												pathTraceGc
													.dist =
													ptDistKm;
												pathTraceGc
													.amsl =
													uls->ITMHeightProfile
														[2 +
														 ptIdx];
												pathTraceGc
													.losAmsl =
													losPathHeight;
												pathTraceGc
													.fsid =
													uls->getID();
												pathTraceGc
													.divIdx =
													divIdx;
												pathTraceGc
													.segIdx =
													segIdx;
												pathTraceGc
													.scanPtIdx =
													scanPtIdx;
												pathTraceGc
													.rlanHtIdx =
													rlanHtIdx;
												pathTraceGc
													.completeRow();
											}
										}
									}
#endif
								}

								if (uls->ITMHeightProfile) {
									free(uls->ITMHeightProfile);
									uls->ITMHeightProfile =
										(double *)NULL;
								}
								if (uls->isLOSHeightProfile) {
									free(uls->isLOSHeightProfile);
									uls->isLOSHeightProfile =
										(double *)NULL;
								}
							}
						}

						if (fFSList) {
							fprintf(fFSList, "%d", uls->getID());
							fprintf(fFSList,
								",%s,%s",
								uls->getRxCallsign().c_str(),
								uls->getCallsign().c_str());
							fprintf(fFSList,
								",%d,%d,%d",
								numPR,
								divIdx,
								segIdx);
							fprintf(fFSList,
								",%.1f,%.1f",
								uls->getStartFreq() * 1.0e-6,
								uls->getStopFreq() * 1.0e-6);
							fprintf(fFSList,
								",%.6f,%.6f",
								ulsRxLongitude,
								ulsRxLatitude);
							fprintf(fFSList, ",%.1f", ulsRxHeightAGL);
							fprintf(fFSList,
								",%.1f",
								ulsSegmentDistance);

							if (segIdx < numPR) {
								PRClass &pr = uls->getPR(segIdx);
								if (pr.type ==
								    CConst::billboardReflectorPRType) {
									fprintf(fFSList,
										",%.5f",
										pr.reflectorThetaIN);
									fprintf(fFSList,
										",%.5f",
										pr.reflectorKS);
									fprintf(fFSList,
										",%.5f",
										pr.reflectorQ);
								} else {
									fprintf(fFSList, ",,,");
								}
								fprintf(fFSList,
									",%.3f",
									pr.pathSegGain);
								fprintf(fFSList,
									",%.3f",
									pr.effectiveGain);
							} else {
								fprintf(fFSList, ",,,,,");
							}

							fprintf(fFSList, "\n");
						}

#if DEBUG_AFC
						time_t t2 = time(NULL);
						tstr = strdup(ctime(&t2));
						strtok(tstr, "\n");

						LOGGER_DEBUG(logger)
							<< numProc << " [" << ulsIdx + 1 << " / "
							<< sortedUlsList.size()
							<< "] FSID = " << uls->getID()
							<< " DIV_IDX = " << divIdx
							<< " SEG_IDX = " << segIdx << " " << tstr
							<< " Elapsed Time = " << (t2 - t1);

						free(tstr);

						if (false && (numProc == 10)) {
							cont = false;
						}

#endif

					} else {
#if DEBUG_AFC
						// uls is not included in calculations
						LOGGER_DEBUG(logger)
							<< "FSID: " << uls->getID()
							<< " DIV_IDX = " << divIdx
							<< " SEG_IDX = " << segIdx
							<< ", distKm: " << sqrt(distKmSquared)
							<< ", Outside MAX_RADIUS";
#endif
					}
				}
			}

#if DEBUG_AFC
			if (traceFlag) {
				traceIdx++;
			}
#endif
		}

		numProc++;
	}

	for (int colorIdx = 0; (colorIdx < 3) && (fkml); ++colorIdx) {
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

		for (ulsIdx = 0; ulsIdx < (int)sortedUlsList.size(); ulsIdx++) {
			bool useFlag = ulsFlagList[ulsIdx];

			if (useFlag) {
				if (colorIdx == 0) {
					useFlag = ((eirpLimitList[ulsIdx] < _minEIRP_dBm) ? true :
											    false);
				} else if (colorIdx == 1) {
					useFlag = (((eirpLimitList[ulsIdx] < _maxEIRP_dBm) &&
						    (eirpLimitList[ulsIdx] >= _minEIRP_dBm)) ?
							   true :
							   false);
				} else if (colorIdx == 2) {
					useFlag = ((eirpLimitList[ulsIdx] >= _maxEIRP_dBm) ? true :
											     false);
				}
			}
			if ((useFlag) && (fkml)) {
				ULSClass *uls = sortedUlsList[ulsIdx];
				std::string dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);

				fkml->writeStartElement("Folder");
				fkml->writeTextElement("name",
						       QString::fromStdString(
							       dbName + "_" +
							       std::to_string(uls->getID())));
				// fkml->writeTextElement("name",
				// QString::fromStdString(uls->getCallsign()));

				int numPR = uls->getNumPR();
				for (int segIdx = 0; segIdx < numPR + 1; ++segIdx) {
					Vector3 ulsTxPosn =
						(segIdx == 0 ? uls->getTxPosition() :
							       uls->getPR(segIdx - 1).positionTx);
					double ulsTxLongitude =
						(segIdx == 0 ? uls->getTxLongitudeDeg() :
							       uls->getPR(segIdx - 1).longitudeDeg);
					double ulsTxLatitude =
						(segIdx == 0 ? uls->getTxLatitudeDeg() :
							       uls->getPR(segIdx - 1).latitudeDeg);
					double ulsTxHeight =
						(segIdx == 0 ? uls->getTxHeightAMSL() :
							       uls->getPR(segIdx - 1).heightAMSLTx);

					Vector3 ulsRxPosn = (segIdx == numPR ?
								     uls->getRxPosition() :
								     uls->getPR(segIdx).positionRx);
					double ulsRxLongitude =
						(segIdx == numPR ? uls->getRxLongitudeDeg() :
								   uls->getPR(segIdx).longitudeDeg);
					double ulsRxLatitude =
						(segIdx == numPR ? uls->getRxLatitudeDeg() :
								   uls->getPR(segIdx).latitudeDeg);
					double ulsRxHeight =
						(segIdx == numPR ? uls->getRxHeightAMSL() :
								   uls->getPR(segIdx).heightAMSLRx);

					bool txLocFlag = (!std::isnan(ulsTxPosn.x())) &&
							 (!std::isnan(ulsTxPosn.y())) &&
							 (!std::isnan(ulsTxPosn.z()));

					double linkDistKm;
					if (!txLocFlag) {
						linkDistKm = 1.0;
						Vector3 segPointing =
							(segIdx == numPR ?
								 uls->getAntennaPointing() :
								 uls->getPR(segIdx).pointing);
						ulsTxPosn = ulsRxPosn + linkDistKm * segPointing;
					} else {
						linkDistKm = (ulsTxPosn - ulsRxPosn).len();
					}

					if ((segIdx == 0) && (addPlacemarks) && (txLocFlag)) {
						fkml->writeStartElement("Placemark");
						fkml->writeTextElement(
							"name",
							QString::asprintf("%s %s_%d",
									  "TX",
									  dbName.c_str(),
									  uls->getID()));
						fkml->writeTextElement("visibility", "1");
						fkml->writeTextElement("styleUrl",
								       placemarkStyleStr.c_str());
						fkml->writeStartElement("Point");
						fkml->writeTextElement("altitudeMode", "absolute");
						fkml->writeTextElement(
							"coordinates",
							QString::asprintf("%.10f,%.10f,%.2f",
									  ulsTxLongitude,
									  ulsTxLatitude,
									  ulsTxHeight));
						fkml->writeEndElement(); // Point
						fkml->writeEndElement(); // Placemark
					}

					double beamWidthDeg = uls->computeBeamWidth(3.0);
					double beamWidthRad = beamWidthDeg * (M_PI / 180.0);

					Vector3 zvec = (ulsTxPosn - ulsRxPosn).normalized();
					Vector3 xvec =
						(Vector3(zvec.y(), -zvec.x(), 0.0)).normalized();
					Vector3 yvec = zvec.cross(xvec);

					int numCvgPoints = 32;

					std::vector<GeodeticCoord> ptList;
					double cvgTheta = beamWidthRad;
					int cvgPhiIdx;
					for (cvgPhiIdx = 0; cvgPhiIdx < numCvgPoints; ++cvgPhiIdx) {
						double cvgPhi = 2 * M_PI * cvgPhiIdx / numCvgPoints;
						Vector3 cvgIntPosn =
							ulsRxPosn +
							linkDistKm * (zvec * cos(cvgTheta) +
								      (xvec * cos(cvgPhi) +
								       yvec * sin(cvgPhi)) *
									      sin(cvgTheta));

						GeodeticCoord cvgIntPosnGeodetic =
							EcefModel::ecefToGeodetic(cvgIntPosn);
						ptList.push_back(cvgIntPosnGeodetic);
					}

					if (addPlacemarks) {
						std::string nameStr;
						if (segIdx == numPR) {
							nameStr = "RX";
						} else {
							nameStr = "PR " +
								  std::to_string(segIdx + 1);
							;
						}
						fkml->writeStartElement("Placemark");
						fkml->writeTextElement(
							"name",
							QString::asprintf("%s %s_%d",
									  nameStr.c_str(),
									  dbName.c_str(),
									  uls->getID()));
						fkml->writeTextElement("visibility", "1");
						fkml->writeTextElement("styleUrl",
								       placemarkStyleStr.c_str());
						fkml->writeStartElement("Point");
						fkml->writeTextElement("altitudeMode", "absolute");
						fkml->writeTextElement(
							"coordinates",
							QString::asprintf("%.10f,%.10f,%.2f",
									  ulsRxLongitude,
									  ulsRxLatitude,
									  ulsRxHeight));
						fkml->writeEndElement(); // Point
						fkml->writeEndElement(); // Placemark
					}

					if (true) {
						fkml->writeStartElement("Folder");
						fkml->writeTextElement(
							"name",
							QString::asprintf("Beamcone_%d",
									  segIdx + 1));

						for (cvgPhiIdx = 0; cvgPhiIdx < numCvgPoints;
						     ++cvgPhiIdx) {
							fkml->writeStartElement("Placemark");
							fkml->writeTextElement(
								"name",
								QString::asprintf("p%d",
										  cvgPhiIdx));
							fkml->writeTextElement(
								"styleUrl",
								polyStyleStr.c_str());
							fkml->writeTextElement(
								"visibility",
								visibilityStr.c_str());
							fkml->writeStartElement("Polygon");
							fkml->writeTextElement("extrude", "0");
							fkml->writeTextElement("altitudeMode",
									       "absolute");
							fkml->writeStartElement("outerBoundaryIs");
							fkml->writeStartElement("LinearRing");

							QString more_coords = QString::asprintf(
								"%.10f,%.10f,%.2f\n",
								ulsRxLongitude,
								ulsRxLatitude,
								ulsRxHeight);

							GeodeticCoord pt = ptList[cvgPhiIdx];
							more_coords.append(QString::asprintf(
								"%.10f,%.10f,%.2f\n",
								pt.longitudeDeg,
								pt.latitudeDeg,
								pt.heightKm * 1000.0));

							pt = ptList[(cvgPhiIdx + 1) % numCvgPoints];
							more_coords.append(QString::asprintf(
								"%.10f,%.10f,%.2f\n",
								pt.longitudeDeg,
								pt.latitudeDeg,
								pt.heightKm * 1000.0));

							more_coords.append(
								QString::asprintf("%.10f,%.10f,%."
										  "2f\n",
										  ulsRxLongitude,
										  ulsRxLatitude,
										  ulsRxHeight));

							fkml->writeTextElement("coordinates",
									       more_coords);
							fkml->writeEndElement(); // LinearRing
							fkml->writeEndElement(); // outerBoundaryIs
							fkml->writeEndElement(); // Polygon
							fkml->writeEndElement(); // Placemark
						}
						fkml->writeEndElement(); // Beamcone
					}
				}
				fkml->writeEndElement(); // Folder
			}
		}
		fkml->writeEndElement(); // Folder
	}

	for (ulsIdx = 0; ulsIdx < (int)sortedUlsList.size(); ulsIdx++) {
		if (ulsFlagList[ulsIdx]) {
			_ulsIdxList.push_back(
				ulsIdx); // Store the ULS indices that are used in analysis
		}
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

#if DEBUG_AFC
	time_t tEndULS = time(NULL);
	tstr = strdup(ctime(&tEndULS));
	strtok(tstr, "\n");

	int elapsedTime = (int)(tEndULS - tStartULS);

	int et = elapsedTime;
	int elapsedTimeSec = et % 60;
	et = et / 60;
	int elapsedTimeMin = et % 60;
	et = et / 60;
	int elapsedTimeHour = et % 24;
	et = et / 24;
	int elapsedTimeDay = et;

	std::cout << "End Processing ULS RX's " << tstr
		  << " Elapsed time = " << (tEndULS - tStartULS) << " sec = " << elapsedTimeDay
		  << " days " << elapsedTimeHour << " hours " << elapsedTimeMin << " min "
		  << elapsedTimeSec << " sec." << std::endl
		  << std::flush;

	free(tstr);

#endif
	/**************************************************************************************/

	_terrainDataModel->printStats();

	int chanIdx;
	for (chanIdx = 0; chanIdx < (int)_channelList.size(); ++chanIdx) {
		ChannelStruct *channel = &(_channelList[chanIdx]);
		for (int freqSegIdx = 0; freqSegIdx < channel->segList.size(); ++freqSegIdx) {
			ChannelColor chanColor = std::get<2>(channel->segList[freqSegIdx]);
			if ((channel->type == ChannelType::INQUIRED_CHANNEL) &&
			    (chanColor != BLACK) && (chanColor != RED)) {
				double chanEirpLimit =
					std::min(std::get<0>(channel->segList[freqSegIdx]),
						 std::get<1>(channel->segList[freqSegIdx]));
				if (chanEirpLimit == _maxEIRP_dBm) {
					std::get<2>(channel->segList[freqSegIdx]) = GREEN;
				} else if (chanEirpLimit >= _minEIRP_dBm) {
					std::get<2>(channel->segList[freqSegIdx]) = YELLOW;
				} else {
					std::get<2>(channel->segList[freqSegIdx]) = RED;
				}
			}
		}
	}

	free(eirpLimitList);
	free(ulsFlagList);

	if (excthrGc) {
		delete excthrGc;
	}
}

// Returns _ulsList content, sorted in by decreasing of crude interference
// (computed from free-space path loss and off-bearing gain only)
std::vector<ULSClass *> AfcManager::getSortedUls()
{
	std::vector<ULSClass *> ret;
	for (auto ulsIdx = 0; ulsIdx < _ulsList->getSize(); ++ulsIdx) {
		ret.push_back((*_ulsList)[ulsIdx]);
	}

	// AP ECEF coordinates
	Vector3 apEcef = EcefModel::fromGeodetic(
		GeodeticCoord::fromLatLon(_rlanRegion->getCenterLatitude(),
					  _rlanRegion->getCenterLongitude(),
					  _rlanRegion->getCenterHeightAMSL() / 1000.));
	// Maps ULS IDs to sort keys
	std::map<int, double> sortKeys;
	for (auto &uls : ret) {
		auto ulsRxEcef = EcefModel::fromGeodetic(
			GeodeticCoord::fromLatLon(uls->getRxLatitudeDeg(),
						  uls->getRxLongitudeDeg(),
						  0));
		auto ulsCenterFreqHz = (uls->getStartFreq() + uls->getStopFreq()) / 2;
		auto ulsAntennaPointing = uls->getAntennaPointing();
		auto lineOfSightVectorKm = ulsRxEcef - apEcef;
		double distKm = lineOfSightVectorKm.len();

		// The more pathloss the less the interference
		double pathLoss = 20.0 *
				  log((4 * M_PI * ulsCenterFreqHz * distKm * 1000) / CConst::c) /
				  log(10.0);
		auto interferenceScore = pathLoss;

		// The more discrimination gain the more the interference
		double angleOffBoresightDeg = acos(ulsAntennaPointing.dot(
						      -(lineOfSightVectorKm.normalized()))) *
					      180.0 / M_PI;
		std::string rxAntennaSubModelStrDummy;
		double discriminationGainDb = uls->computeRxGain(angleOffBoresightDeg,
								 0,
								 ulsCenterFreqHz,
								 rxAntennaSubModelStrDummy,
								 0);
		interferenceScore -= discriminationGainDb;

		sortKeys[uls->getID()] = interferenceScore;
	}
	std::sort(ret.begin(), ret.end(), [sortKeys](ULSClass *const &l, ULSClass *const &r) {
		return sortKeys.at(l->getID()) < sortKeys.at(r->getID());
	});
	return ret;
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
	if (!(fscan = fopen(scanFileName.c_str(), "wb"))) {
		errStr << std::string("ERROR: Unable to open scanFile \"") + "/tmp/scan.csv" +
				  std::string("\"\n");
		throw std::runtime_error(errStr.str());
	}
	fprintf(fscan,
		"SCAN_PT_IDX,HEIGHT_IDX,RLAN_LONGITUDE (deg)"
		",RLAN_LATITUDE (deg)"
		",RLAN_HEIGHT_ABOVE_TERRAIN (m)"
		",RLAN_TERRAIN_HEIGHT (m)"
		",RLAN_TERRAIN_SOURCE"
		",RLAN_PROP_ENV");

	// List all bandwidths covered by op-classes, merge duplicates
	for (auto &opClass : _opClass) {
		int bw = opClass.bandWidth;
		bw_index_map[bw] = 0;
	}

	int numBW = 0;
	for (auto &map : bw_index_map) {
		int bw = map.first;
		fprintf(fscan,
			",NUM_CHAN_BLACK_%d_MHZ,NUM_CHAN_RED_%d_MHZ,NUM_CHAN_YELLOW_%d_MHZ,NUM_"
			"CHAN_GREEN_%d_MHZ",
			bw,
			bw,
			bw,
			bw);
		// Note down the index of bandwidth in the map
		map.second = numBW++;
	}
	fprintf(fscan, "\n");
	fflush(fscan);

	_rlanRegion->configure(_rlanHeightType, _terrainDataModel);

	double heightUncertainty = _rlanRegion->getHeightUncertainty();
	int NHt = (int)ceil(heightUncertainty / _scanres_ht);
	Vector3 rlanPosnList[2 * NHt + 1];
	GeodeticCoord rlanCoordList[2 * NHt + 1];

	int bwIdx;
	int numBlack[numBW];
	int numGreen[numBW];
	int numYellow[numBW];
	int numRed[numBW];

	/**************************************************************************************/
	/* Get Uncertainty Region Scan Points                                                 */
	/**************************************************************************************/
	std::vector<LatLon> scanPointList = _rlanRegion->getScan(_scanRegionMethod,
								 _scanres_xy,
								 _scanres_points_per_degree);
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
		fkml->writeTextElement("name", "AFC");
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
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
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
		fkml->writeTextElement("href",
				       "http://maps.google.com/mapfiles/kml/shapes/"
				       "placemark_circle.png");
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
		fkml->writeTextElement("href",
				       "http://maps.google.com/mapfiles/kml/pushpin/"
				       "ylw-pushpin.png");
		fkml->writeEndElement(); // Icon
		fkml->writeEndElement(); // IconStyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "yellowPlacemark");
		fkml->writeStartElement("IconStyle");
		fkml->writeTextElement("color", "ff00ffff");
		fkml->writeStartElement("Icon");
		fkml->writeTextElement("href",
				       "http://maps.google.com/mapfiles/kml/pushpin/"
				       "ylw-pushpin.png");
		fkml->writeEndElement(); // Icon
		fkml->writeEndElement(); // IconStyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "greenPlacemark");
		fkml->writeStartElement("IconStyle");
		fkml->writeTextElement("color", "ff00ff00");
		fkml->writeStartElement("Icon");
		fkml->writeTextElement("href",
				       "http://maps.google.com/mapfiles/kml/pushpin/"
				       "ylw-pushpin.png");
		fkml->writeEndElement(); // Icon
		fkml->writeEndElement(); // IconStyle
		fkml->writeEndElement(); // Style

		fkml->writeStartElement("Style");
		fkml->writeAttribute("id", "blackPlacemark");
		fkml->writeStartElement("IconStyle");
		fkml->writeTextElement("color", "ff000000");
		fkml->writeStartElement("Icon");
		fkml->writeTextElement("href",
				       "http://maps.google.com/mapfiles/kml/pushpin/"
				       "ylw-pushpin.png");
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
		fkml->writeTextElement("coordinates",
				       QString::asprintf("%.10f,%.10f,%.2f",
							 rlanCenterPtGeo.longitudeDeg,
							 rlanCenterPtGeo.latitudeDeg,
							 rlanCenterPtGeo.heightKm * 1000.0));
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
		for (ptIdx = 0; ptIdx <= (int)ptList.size(); ptIdx++) {
			GeodeticCoord pt = ptList[ptIdx % ptList.size()];
			top_coords.append(
				QString::asprintf("%.10f,%.10f,%.2f\n",
						  pt.longitudeDeg,
						  pt.latitudeDeg,
						  pt.heightKm * 1000.0 +
							  _rlanRegion->getHeightUncertainty()));
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
		for (ptIdx = 0; ptIdx <= (int)ptList.size(); ptIdx++) {
			GeodeticCoord pt = ptList[ptIdx % ptList.size()];
			bottom_coords.append(
				QString::asprintf("%.10f,%.10f,%.2f\n",
						  pt.longitudeDeg,
						  pt.latitudeDeg,
						  pt.heightKm * 1000.0 -
							  _rlanRegion->getHeightUncertainty()));
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
		for (ptIdx = 0; ptIdx < (int)ptList.size(); ptIdx++) {
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
			GeodeticCoord pt2 = ptList[(ptIdx + 1) % ptList.size()];
			QString side_coords;
			side_coords.append(
				QString::asprintf("%.10f,%.10f,%.2f\n",
						  pt1.longitudeDeg,
						  pt1.latitudeDeg,
						  pt1.heightKm * 1000.0 -
							  _rlanRegion->getHeightUncertainty()));
			side_coords.append(
				QString::asprintf("%.10f,%.10f,%.2f\n",
						  pt1.longitudeDeg,
						  pt1.latitudeDeg,
						  pt1.heightKm * 1000.0 +
							  _rlanRegion->getHeightUncertainty()));
			side_coords.append(
				QString::asprintf("%.10f,%.10f,%.2f\n",
						  pt2.longitudeDeg,
						  pt2.latitudeDeg,
						  pt2.heightKm * 1000.0 +
							  _rlanRegion->getHeightUncertainty()));
			side_coords.append(
				QString::asprintf("%.10f,%.10f,%.2f\n",
						  pt2.longitudeDeg,
						  pt2.latitudeDeg,
						  pt2.heightKm * 1000.0 -
							  _rlanRegion->getHeightUncertainty()));
			side_coords.append(
				QString::asprintf("%.10f,%.10f,%.2f\n",
						  pt1.longitudeDeg,
						  pt1.latitudeDeg,
						  pt1.heightKm * 1000.0 -
							  _rlanRegion->getHeightUncertainty()));

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

		for (ptIdx = 0; ptIdx < (int)scanPointList.size(); ptIdx++) {
			LatLon scanPt = scanPointList[ptIdx];

			double rlanTerrainHeight, bldgHeight;
			MultibandRasterClass::HeightResult lidarHeightResult;
			CConst::HeightSourceEnum rlanHeightSource;
			_terrainDataModel->getTerrainHeight(scanPt.second,
							    scanPt.first,
							    rlanTerrainHeight,
							    bldgHeight,
							    lidarHeightResult,
							    rlanHeightSource);

			double height0;
			if (_rlanRegion->getFixedHeightAMSL()) {
				height0 = _rlanRegion->getCenterHeightAMSL();
			} else {
				height0 = _rlanRegion->getCenterHeightAMSL() -
					  _rlanRegion->getCenterTerrainHeight() + rlanTerrainHeight;
			}

			int htIdx;
			for (htIdx = 0; htIdx <= 2 * NHt; ++htIdx) {
				double heightAMSL = height0 +
						    (NHt ? (htIdx - NHt) * heightUncertainty / NHt :
							   0.0);

				fkml->writeStartElement("Placemark");
				fkml->writeTextElement("name",
						       QString::asprintf("SCAN_POINT_%d_%d",
									 ptIdx,
									 htIdx));
				fkml->writeTextElement("visibility", "1");
				fkml->writeTextElement("styleUrl", "#dotStyle");
				fkml->writeStartElement("Point");
				fkml->writeTextElement("altitudeMode", "absolute");
				fkml->writeTextElement("coordinates",
						       QString::asprintf("%.10f,%.10f,%.2f",
									 scanPt.second,
									 scanPt.first,
									 heightAMSL));
				fkml->writeEndElement(); // Point
				fkml->writeEndElement(); // Placemark
			}
		}

		fkml->writeEndElement(); // Scan Points
		/**********************************************************************************/

		fkml->writeEndElement(); // Folder

		fkml->writeStartElement("Folder");
		fkml->writeTextElement("name", "Denied Region");
		int drIdx;
		for (drIdx = 0; drIdx < (int)_deniedRegionList.size(); ++drIdx) {
			DeniedRegionClass *dr = _deniedRegionList[drIdx];

			fkml->writeStartElement("Folder");
			fkml->writeTextElement("name",
					       QString("DR_") + QString::number(dr->getID()));

			int numPtsCircle = 32;
			int rectIdx, numRect;
			double rectLonStart, rectLonStop, rectLatStart, rectLatStop;
			double circleRadius, longitudeCenter, latitudeCenter;
			double drTerrainHeight, drBldgHeight, drHeightAGL;
			Vector3 drCenterPosn;
			Vector3 drUpVec;
			Vector3 drEastVec;
			Vector3 drNorthVec;
			QString dr_coords;
			MultibandRasterClass::HeightResult drLidarHeightResult;
			CConst::HeightSourceEnum drHeightSource;
			DeniedRegionClass::GeometryEnum drGeometry = dr->getGeometry();
			switch (drGeometry) {
				case DeniedRegionClass::rectGeometry:
				case DeniedRegionClass::rect2Geometry:
					numRect = ((RectDeniedRegionClass *)dr)->getNumRect();
					for (rectIdx = 0; rectIdx < numRect; rectIdx++) {
						std::tie(rectLonStart,
							 rectLonStop,
							 rectLatStart,
							 rectLatStop) =
							((RectDeniedRegionClass *)dr)
								->getRect(rectIdx);

						fkml->writeStartElement("Placemark");
						fkml->writeTextElement("name",
								       QString("RECT_") +
									       QString::number(
										       rectIdx));
						fkml->writeTextElement("visibility", "1");
						fkml->writeTextElement("styleUrl",
								       "#transBluePoly");
						fkml->writeStartElement("Polygon");
						fkml->writeTextElement("extrude", "0");
						fkml->writeTextElement("tessellate", "0");
						fkml->writeTextElement("altitudeMode",
								       "clampToGround");
						fkml->writeStartElement("outerBoundaryIs");
						fkml->writeStartElement("LinearRing");

						dr_coords = QString();
						dr_coords.append(QString::asprintf("%.10f,%.10f,%."
										   "2f\n",
										   rectLonStart,
										   rectLatStart,
										   0.0));
						dr_coords.append(QString::asprintf("%.10f,%.10f,%."
										   "2f\n",
										   rectLonStop,
										   rectLatStart,
										   0.0));
						dr_coords.append(QString::asprintf("%.10f,%.10f,%."
										   "2f\n",
										   rectLonStop,
										   rectLatStop,
										   0.0));
						dr_coords.append(QString::asprintf("%.10f,%.10f,%."
										   "2f\n",
										   rectLonStart,
										   rectLatStop,
										   0.0));
						dr_coords.append(QString::asprintf("%.10f,%.10f,%."
										   "2f\n",
										   rectLonStart,
										   rectLatStart,
										   0.0));

						fkml->writeTextElement("coordinates", dr_coords);
						fkml->writeEndElement(); // LinearRing
						fkml->writeEndElement(); // outerBoundaryIs
						fkml->writeEndElement(); // Polygon
						fkml->writeEndElement(); // Placemark
					}
					break;
				case DeniedRegionClass::circleGeometry:
				case DeniedRegionClass::horizonDistGeometry:
					circleRadius =
						((CircleDeniedRegionClass *)dr)
							->computeRadius(
								_rlanRegion->getMaxHeightAGL());
					longitudeCenter = ((CircleDeniedRegionClass *)dr)
								  ->getLongitudeCenter();
					latitudeCenter = ((CircleDeniedRegionClass *)dr)
								 ->getLatitudeCenter();
					drHeightAGL = dr->getHeightAGL();
					_terrainDataModel->getTerrainHeight(longitudeCenter,
									    latitudeCenter,
									    drTerrainHeight,
									    drBldgHeight,
									    drLidarHeightResult,
									    drHeightSource);

					drCenterPosn = EcefModel::geodeticToEcef(
						latitudeCenter,
						longitudeCenter,
						(drTerrainHeight + drHeightAGL) / 1000.0);
					drUpVec = drCenterPosn.normalized();
					drEastVec = (Vector3(-drUpVec.y(), drUpVec.x(), 0.0))
							    .normalized();
					drNorthVec = drUpVec.cross(drEastVec);

					fkml->writeStartElement("Placemark");
					fkml->writeTextElement("name",
							       QString("RECT_") +
								       QString::number(rectIdx));
					fkml->writeTextElement("visibility", "1");
					fkml->writeTextElement("styleUrl", "#transBluePoly");
					fkml->writeStartElement("Polygon");
					fkml->writeTextElement("extrude", "0");
					fkml->writeTextElement("tessellate", "0");
					fkml->writeTextElement("altitudeMode", "clampToGround");
					fkml->writeStartElement("outerBoundaryIs");
					fkml->writeStartElement("LinearRing");

					dr_coords = QString();
					for (ptIdx = 0; ptIdx <= numPtsCircle; ++ptIdx) {
						double phi = 2 * M_PI * ptIdx / numPtsCircle;
						Vector3 circlePtPosn = drCenterPosn +
								       (circleRadius / 1000) *
									       (drEastVec *
											cos(phi) +
										drNorthVec *
											sin(phi));

						GeodeticCoord circlePtPosnGeodetic =
							EcefModel::ecefToGeodetic(circlePtPosn);

						dr_coords.append(QString::asprintf(
							"%.10f,%.10f,%.2f\n",
							circlePtPosnGeodetic.longitudeDeg,
							circlePtPosnGeodetic.latitudeDeg,
							0.0));
					}

					fkml->writeTextElement("coordinates", dr_coords);
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
	}
	delete kml_writer;

	/**************************************************************************************/
	/* Compute Channel Availability                                                       */
	/**************************************************************************************/
	const double exclusionDistKmSquared = (_exclusionDist / 1000.0) * (_exclusionDist / 1000.0);
	const double maxRadiusKmSquared = (_maxRadius / 1000.0) * (_maxRadius / 1000.0);

	if (_rlanRegion->getMinHeightAGL() < _minRlanHeightAboveTerrain) {
		throw std::runtime_error(
			ErrStream()
			<< std::string("ERROR: Point Analysis: Invalid RLAN parameter settings.")
			<< std::endl
			<< std::string("RLAN Min Height above terrain = ")
			<< _rlanRegion->getMinHeightAGL() << std::endl
			<< std::string("RLAN must be more than ") << _minRlanHeightAboveTerrain
			<< " meters above terrain" << std::endl);
	}

	int scanPtIdx;
	for (scanPtIdx = 0; scanPtIdx < (int)scanPointList.size(); scanPtIdx++) {
		LatLon scanPt = scanPointList[scanPtIdx];

		/**************************************************************************************/
		/* Determine propagation environment of RLAN using scan point */
		/**************************************************************************************/
		CConst::NLCDLandCatEnum nlcdLandCatTx;
		CConst::PropEnvEnum rlanPropEnv = computePropEnv(scanPt.second,
								 scanPt.first,
								 nlcdLandCatTx);
		/**************************************************************************************/

		double rlanTerrainHeight, bldgHeight;
		MultibandRasterClass::HeightResult lidarHeightResult;
		CConst::HeightSourceEnum rlanHeightSource;
		_terrainDataModel->getTerrainHeight(scanPt.second,
						    scanPt.first,
						    rlanTerrainHeight,
						    bldgHeight,
						    lidarHeightResult,
						    rlanHeightSource);

		double height0;
		if (_rlanRegion->getFixedHeightAMSL()) {
			height0 = _rlanRegion->getCenterHeightAMSL();
		} else {
			height0 = _rlanRegion->getCenterHeightAMSL() -
				  _rlanRegion->getCenterTerrainHeight() + rlanTerrainHeight;
		}

		int rlanPosnIdx;
		int htIdx;
		for (htIdx = 0; htIdx <= 2 * NHt; ++htIdx) {
			rlanCoordList[htIdx] = GeodeticCoord::fromLatLon(
				scanPt.first,
				scanPt.second,
				(height0 + (NHt ? (htIdx - NHt) * heightUncertainty / NHt : 0.0)) /
					1000.0);
			rlanPosnList[htIdx] = EcefModel::fromGeodetic(rlanCoordList[htIdx]);
		}

		int numRlanPosn = 2 * NHt + 1;

		for (rlanPosnIdx = 0; rlanPosnIdx < numRlanPosn; ++rlanPosnIdx) {
			Vector3 rlanPosn = rlanPosnList[rlanPosnIdx];
			GeodeticCoord rlanCoord = rlanCoordList[rlanPosnIdx];

			/**************************************************************************************/
			/* Initialize eirpLimit_dBm to _maxEIRP_dBm for all channels */
			/**************************************************************************************/
			for (auto &channel : _channelList) {
				for (int freqSegIdx = 0; freqSegIdx < channel.segList.size();
				     ++freqSegIdx) {
					channel.segList[freqSegIdx] = std::make_tuple(_maxEIRP_dBm,
										      _maxEIRP_dBm,
										      GREEN);
				}
			}
			/**************************************************************************************/

			double rlanHtAboveTerrain = rlanCoord.heightKm * 1000.0 - rlanTerrainHeight;

			int drIdx;
			for (drIdx = 0; drIdx < (int)_deniedRegionList.size(); ++drIdx) {
				DeniedRegionClass *dr = _deniedRegionList[drIdx];
				if (dr->intersect(rlanCoord.longitudeDeg,
						  rlanCoord.latitudeDeg,
						  0.0,
						  rlanHtAboveTerrain)) {
					for (auto &channel : _channelList) {
						for (int freqSegIdx = 0;
						     freqSegIdx < channel.segList.size();
						     ++freqSegIdx) {
							if (std::get<2>(
								    channel.segList[freqSegIdx]) !=
							    BLACK) {
								double chanStartFreq =
									channel.freqMHzList
										[freqSegIdx] *
									1.0e6;
								double chanStopFreq =
									channel.freqMHzList
										[freqSegIdx + 1] *
									1.0e6;
								bool hasOverlap = computeSpectralOverlapLoss(
									(double *)NULL,
									chanStartFreq,
									chanStopFreq,
									dr->getStartFreq(),
									dr->getStopFreq(),
									false,
									CConst::psdSpectralAlgorithm);
								if (hasOverlap) {
									channel.segList
										[freqSegIdx] = std::make_tuple(
										-std::numeric_limits<
											double>::
											infinity(),
										-std::numeric_limits<
											double>::
											infinity(),
										BLACK);
								}
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
				double distKmSquared =
					(lineOfSightVectorKm).dot(lineOfSightVectorKm);
				double distKm = lineOfSightVectorKm.len();
				double dAP = rlanPosn.len();
				double duls = ulsRxPos.len();
				double elevationAngleTxDeg =
					90.0 -
					acos(rlanPosn.dot(lineOfSightVectorKm) / (dAP * distKm)) *
						180.0 / M_PI;
				double elevationAngleRxDeg =
					90.0 -
					acos(ulsRxPos.dot(-lineOfSightVectorKm) / (duls * distKm)) *
						180.0 / M_PI;

				// Use Haversine formula with average earth radius of 6371 km
				double lon1Rad = rlanCoord.longitudeDeg * M_PI / 180.0;
				double lat1Rad = rlanCoord.latitudeDeg * M_PI / 180.0;
				double lon2Rad = uls->getRxLongitudeDeg() * M_PI / 180.0;
				double lat2Rad = uls->getRxLatitudeDeg() * M_PI / 180.0;
				double slat = sin((lat2Rad - lat1Rad) / 2);
				double slon = sin((lon2Rad - lon1Rad) / 2);
				double groundDistanceKm = 2 * CConst::averageEarthRadius *
							  asin(sqrt(slat * slat +
								    cos(lat1Rad) * cos(lat2Rad) *
									    slon * slon)) *
							  1.0e-3;

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

				if ((distKmSquared < maxRadiusKmSquared) &&
				    (distKmSquared > exclusionDistKmSquared)) {
					/**************************************************************************************/
					/* Determine propagation environment of FS, if needed. */
					/**************************************************************************************/
					CConst::NLCDLandCatEnum nlcdLandCatRx;
					CConst::PropEnvEnum fsPropEnv;
					if ((_applyClutterFSRxFlag) &&
					    (uls->getRxHeightAboveTerrain() <= _maxFsAglHeight)) {
						fsPropEnv = computePropEnv(uls->getRxLongitudeDeg(),
									   uls->getRxLatitudeDeg(),
									   nlcdLandCatRx);
					} else {
						fsPropEnv = CConst::unknownPropEnv;
					}
					/**************************************************************************************/

					for (auto &channel : _channelList) {
						for (int freqSegIdx = 0;
						     freqSegIdx < channel.segList.size();
						     ++freqSegIdx) {
							if (std::get<2>(
								    channel.segList[freqSegIdx]) !=
							    BLACK) {
								double chanStartFreq =
									channel.freqMHzList
										[freqSegIdx] *
									1.0e6;
								double chanStopFreq =
									channel.freqMHzList
										[freqSegIdx + 1] *
									1.0e6;

								bool useACI =
									(channel.type == INQUIRED_FREQUENCY ?
										 false :
										 _aciFlag);
								CConst::SpectralAlgorithmEnum spectralAlgorithm =
									(channel.type == INQUIRED_FREQUENCY ?
										 CConst::psdSpectralAlgorithm :
										 _channelResponseAlgorithm);
								// LOGGER_INFO(logger) << "COMPUTING
								// SPECTRAL OVERLAP FOR FSID = " <<
								// uls->getID();
								double spectralOverlapLossDB;
								bool hasOverlap =
									computeSpectralOverlapLoss(
										&spectralOverlapLossDB,
										chanStartFreq,
										chanStopFreq,
										uls->getStartFreq(),
										uls->getStopFreq(),
										useACI,
										spectralAlgorithm);

								if (hasOverlap) {
									// double bandwidth =
									// chanStopFreq -
									// chanStartFreq;
									double chanCenterFreq =
										(chanStartFreq +
										 chanStopFreq) /
										2;

									std::string
										buildingPenetrationModelStr;
									double buildingPenetrationCDF;
									double buildingPenetrationDB = computeBuildingPenetration(
										_buildingType,
										elevationAngleTxDeg,
										chanCenterFreq,
										buildingPenetrationModelStr,
										buildingPenetrationCDF);

									std::string txClutterStr;
									std::string rxClutterStr;
									std::string
										pathLossModelStr;
									double pathLossCDF;
									double pathLoss;
									std::string
										pathClutterTxModelStr;
									double pathClutterTxCDF;
									double pathClutterTxDB;
									std::string
										pathClutterRxModelStr;
									double pathClutterRxCDF;
									double pathClutterRxDB;

									computePathLoss(
										_pathLossModel,
										false,
										rlanPropEnv,
										fsPropEnv,
										nlcdLandCatTx,
										nlcdLandCatRx,
										distKm,
										fsplDistKm,
										win2DistKm,
										chanCenterFreq,
										rlanCoord
											.longitudeDeg,
										rlanCoord
											.latitudeDeg,
										rlanHtAboveTerrain,
										elevationAngleTxDeg,
										uls->getRxLongitudeDeg(),
										uls->getRxLatitudeDeg(),
										uls->getRxHeightAboveTerrain(),
										elevationAngleRxDeg,
										pathLoss,
										pathClutterTxDB,
										pathClutterRxDB,
										pathLossModelStr,
										pathLossCDF,
										pathClutterTxModelStr,
										pathClutterTxCDF,
										pathClutterRxModelStr,
										pathClutterRxCDF,
										&txClutterStr,
										&rxClutterStr,
										&(uls->ITMHeightProfile),
										&(uls->isLOSHeightProfile),
										&(uls->isLOSSurfaceFrac)
#if DEBUG_AFC
											,
										uls->ITMHeightType
#endif
									);

									std::string
										rxAntennaSubModelStr;
									double angleOffBoresightDeg =
										acos(uls->getAntennaPointing()
											     .dot(-(lineOfSightVectorKm
													    .normalized()))) *
										180.0 / M_PI;
									double rxGainDB = uls->computeRxGain(
										angleOffBoresightDeg,
										elevationAngleRxDeg,
										chanCenterFreq,
										rxAntennaSubModelStr,
										0);

									double rxPowerDBW =
										(_maxEIRP_dBm -
										 30.0) -
										_bodyLossDB -
										buildingPenetrationDB -
										pathLoss -
										pathClutterTxDB -
										pathClutterRxDB +
										rxGainDB -
										spectralOverlapLossDB -
										_polarizationLossDB -
										uls->getRxAntennaFeederLossDB();

									double I2NDB =
										rxPowerDBW -
										uls->getNoiseLevelDBW();

									double marginDB =
										_IoverN_threshold_dB -
										I2NDB;

									double eirpLimit_dBm =
										_maxEIRP_dBm +
										marginDB;

									if (eirpLimit_dBm <
									    std::min(
										    std::get<0>(
											    channel.segList
												    [freqSegIdx]),
										    std::get<1>(
											    channel.segList
												    [freqSegIdx]))) {
										std::get<0>(
											channel.segList
												[freqSegIdx]) =
											eirpLimit_dBm;
										std::get<1>(
											channel.segList
												[freqSegIdx]) =
											eirpLimit_dBm;
									}
								}
							}
						}
					}
				}

				if (uls->ITMHeightProfile) {
					free(uls->ITMHeightProfile);
					uls->ITMHeightProfile = (double *)NULL;
				}
				if (uls->isLOSHeightProfile) {
					free(uls->isLOSHeightProfile);
					uls->isLOSHeightProfile = (double *)NULL;
				}
			}

			for (bwIdx = 0; bwIdx < numBW; ++bwIdx) {
				numBlack[bwIdx] = 0;
				numGreen[bwIdx] = 0;
				numYellow[bwIdx] = 0;
				numRed[bwIdx] = 0;
			}

			for (auto &channel : _channelList) {
				for (int freqSegIdx = 0; freqSegIdx < channel.segList.size();
				     ++freqSegIdx) {
					if (channel.type == ChannelType::INQUIRED_CHANNEL) {
						bwIdx = bw_index_map[channel.bandwidth(freqSegIdx)];
						double channelEirp = std::min(
							std::get<0>(channel.segList[freqSegIdx]),
							std::get<1>(channel.segList[freqSegIdx]));
						if (std::get<2>(channel.segList[freqSegIdx]) ==
						    BLACK) {
							numBlack[bwIdx]++;
						} else if (channelEirp == _maxEIRP_dBm) {
							numGreen[bwIdx]++;
						} else if (channelEirp >= _minEIRP_dBm) {
							numYellow[bwIdx]++;
						} else {
							numRed[bwIdx]++;
						}
					}
				}
			}

			fprintf(fscan,
				"%d,%d,%.6f,%.6f,%.1f,%.1f,%s,%s",
				scanPtIdx,
				rlanPosnIdx,
				rlanCoord.longitudeDeg,
				rlanCoord.latitudeDeg,
				rlanHtAboveTerrain,
				rlanTerrainHeight,
				_terrainDataModel->getSourceName(rlanHeightSource).c_str(),
				CConst::strPropEnvList->type_to_str(rlanPropEnv));

			for (bwIdx = 0; bwIdx < numBW; ++bwIdx) {
				fprintf(fscan,
					",%d,%d,%d,%d",
					numBlack[bwIdx],
					numRed[bwIdx],
					numYellow[bwIdx],
					numGreen[bwIdx]);
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
	std::vector<int> fsidTraceList {66423};
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

	int ulsIdx;
	ULSClass *uls = findULSID(_exclusionZoneFSID, 0, ulsIdx);
	std::string dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);

	ChannelStruct channel = _channelList[_exclusionZoneRLANChanIdx];
	int freqSegIdx = 0;

	double bandwidth = (channel.bandwidth(freqSegIdx)) * 1.0e6;

	double chanStartFreq = channel.freqMHzList[freqSegIdx] * 1.0e6;
	double chanStopFreq = channel.freqMHzList[freqSegIdx + 1] * 1.0e6;
	bool useACI = (channel.type == INQUIRED_FREQUENCY ? false : _aciFlag);
	CConst::SpectralAlgorithmEnum spectralAlgorithm = (channel.type == INQUIRED_FREQUENCY ?
								   CConst::psdSpectralAlgorithm :
								   _channelResponseAlgorithm);
	// LOGGER_INFO(logger) << "COMPUTING SPECTRAL OVERLAP FOR FSID = " << uls->getID();
	double spectralOverlapLossDB;
	bool hasOverlap = computeSpectralOverlapLoss(&spectralOverlapLossDB,
						     chanStartFreq,
						     chanStopFreq,
						     uls->getStartFreq(),
						     uls->getStopFreq(),
						     useACI,
						     spectralAlgorithm);
	double chanCenterFreq = (chanStartFreq + chanStopFreq) / 2;

	if (!hasOverlap) {
		throw std::runtime_error(
			ErrStream()
			<< "ERROR: Specified RLAN spectrum does not overlap FS spectrum. FSID: "
			<< _exclusionZoneFSID << " goes from " << uls->getStartFreq() / 1.0e6
			<< " MHz to " << uls->getStopFreq() / 1.0e6 << " MHz");
	}
	LOGGER_INFO(logger) << "FSID = " << _exclusionZoneFSID << " found";
	LOGGER_INFO(logger) << "LON: " << uls->getRxLongitudeDeg();
	LOGGER_INFO(logger) << "LAT: " << uls->getRxLatitudeDeg();
	LOGGER_INFO(logger) << "SPECTRAL_OVERLAP_LOSS (dB) = " << spectralOverlapLossDB;

	/**************************************************************************************/
	/* Create excThrFile, useful for debugging                                            */
	/**************************************************************************************/
	ExThrGzipCsv excthrGc(_excThrFile);

	/**************************************************************************************/

#if DEBUG_AFC
	/**************************************************************************************/
	/* Create pathTraceFile, for debugging                                                */
	/**************************************************************************************/
	TraceGzipCsv pathTraceGc(pathTraceFile);
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
	double pathLossDB = (_exclusionZoneRLANEIRPDBm - 30.0) - _bodyLossDB + uls->getRxGain() -
			    spectralOverlapLossDB - _polarizationLossDB -
			    uls->getRxAntennaFeederLossDB() - uls->getNoiseLevelDBW() -
			    _IoverN_threshold_dB;

	double dFSPL = exp(pathLossDB * log(10.0) / 20.0) * CConst::c / (4 * M_PI * chanCenterFreq);
	/******************************************************************************/

	double initialD0 = (dFSPL * 180.0 /
			    (CConst::earthRadius * M_PI *
			     cos(uls->getRxLatitudeDeg() * M_PI / 180.0)));

	const double minPossibleRadius = 10.0;
	double minPossibleD = (minPossibleRadius * 180.0 / (CConst::earthRadius * M_PI));

	double distKm0, distKm1, distKmM;
	int exclPtIdx;

	for (exclPtIdx = 0; exclPtIdx < numContourPoints; exclPtIdx++) {
		LOGGER_DEBUG(logger)
			<< "computing exlPtIdx: " << exclPtIdx << '/' << numContourPoints;
		double cc = cos(exclPtIdx * 2 * M_PI / numContourPoints);
		double ss = sin(exclPtIdx * 2 * M_PI / numContourPoints);

		bool cont;

		/******************************************************************************/
		/* Step 1: Compute margin at dFSPL, If this margin is not positive something  */
		/* is seriously wrong.                                                        */
		/******************************************************************************/
		double margin0;
		double d0 = initialD0;
		do {
			margin0 = computeIToNMargin(d0,
						    cc,
						    ss,
						    uls,
						    chanCenterFreq,
						    bandwidth,
						    chanStartFreq,
						    chanStopFreq,
						    spectralOverlapLossDB,
						    ulsRxPropEnv,
						    distKm0,
						    "",
						    nullptr);

			if (margin0 < 0.0) {
				d0 *= 1.1;
				cont = true;
				printf("DBNAME = %s FSID = %d, EXCL_PT_IDX = %d, dFSPL = %.1f DIST "
				       "= %.1f margin = %.3f\n",
				       dbName.c_str(),
				       uls->getID(),
				       exclPtIdx,
				       dFSPL,
				       1000 * distKm0,
				       margin0);
			} else {
				cont = false;
			}
		} while (cont);
		/******************************************************************************/
		// printf("exclPtIdx = %d, dFSPL = %.3f margin = %.3f\n", exclPtIdx, dFSPL,
		// margin0);

		bool minRadiusFlag = false;
		/******************************************************************************/
		/* Step 2: Bound position for which margin = 0                                */
		/******************************************************************************/
		double d1, margin1;
		cont = true;
		do {
			d1 = d0 * 0.95;
			margin1 = computeIToNMargin(d1,
						    cc,
						    ss,
						    uls,
						    chanCenterFreq,
						    bandwidth,
						    chanStartFreq,
						    chanStopFreq,
						    spectralOverlapLossDB,
						    ulsRxPropEnv,
						    distKm1,
						    "",
						    nullptr);

			if (d1 <= minPossibleD) {
				d0 = d1;
				margin0 = margin1;
				distKm0 = distKm1;
				minRadiusFlag = true;
				cont = false;
			} else if (margin1 >= 0.0) {
				d0 = d1;
				margin0 = margin1;
				distKm0 = distKm1;
			} else {
				cont = false;
			}
		} while (cont);
		// printf("Position Bounded [%.10f, %.10f]\n", d1, d0);
		/**********************************************************************************/

		if (!minRadiusFlag) {
			/******************************************************************************/
			/* Step 3: Shrink interval to find where margin = 0 */
			/******************************************************************************/
			while (d0 - d1 > 1.0e-6) {
				double dm = (d1 + d0) / 2;
				double marginM = computeIToNMargin(dm,
								   cc,
								   ss,
								   uls,
								   chanCenterFreq,
								   bandwidth,
								   chanStartFreq,
								   chanStopFreq,
								   spectralOverlapLossDB,
								   ulsRxPropEnv,
								   distKmM,
								   "",
								   nullptr);
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

		margin1 = computeIToNMargin(d1,
					    cc,
					    ss,
					    uls,
					    chanCenterFreq,
					    bandwidth,
					    chanStartFreq,
					    chanStopFreq,
					    spectralOverlapLossDB,
					    ulsRxPropEnv,
					    distKm1,
					    "Above Thr",
					    &excthrGc);
		margin0 = computeIToNMargin(d0,
					    cc,
					    ss,
					    uls,
					    chanCenterFreq,
					    bandwidth,
					    chanStartFreq,
					    chanStopFreq,
					    spectralOverlapLossDB,
					    ulsRxPropEnv,
					    distKm0,
					    "Below Thr",
					    &excthrGc);

		double rlanLon = uls->getRxLongitudeDeg() + d0 * cc;
		double rlanLat = uls->getRxLatitudeDeg() + d0 * ss;

		_exclusionZone[exclPtIdx] = std::make_pair(rlanLon, rlanLat);

		// LOGGER_DEBUG(logger) << std::setprecision(15) << exclusionZonePtLon << " " <<
		// exclusionZonePtLat;
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
double AfcManager::computeIToNMargin(double d,
				     double cc,
				     double ss,
				     ULSClass *uls,
				     double chanCenterFreq,
				     double bandwidth,
				     double chanStartFreq,
				     double chanStopFreq,
				     double spectralOverlapLossDB,
				     char ulsRxPropEnv,
				     double &distKmM,
				     std::string comment,
				     ExThrGzipCsv *excthrGc)
{
	Vector3 rlanPosnList[3];
	GeodeticCoord rlanCoordList[3];

	double rlanLon, rlanLat, rlanHeight;
	rlanLon = uls->getRxLongitudeDeg() + d * cc;
	rlanLat = uls->getRxLatitudeDeg() + d * ss;

	double fsHeight = uls->getRxTerrainHeight() + uls->getRxHeightAboveTerrain();

	double rlanHeightInput = std::get<2>(_rlanLLA);
	double heightUncertainty = std::get<2>(_rlanUncerts_m);

	double rlanTerrainHeight, bldgHeight;
	MultibandRasterClass::HeightResult lidarHeightResult;
	CConst::HeightSourceEnum rlanHeightSource;
	_terrainDataModel->getTerrainHeight(rlanLon,
					    rlanLat,
					    rlanTerrainHeight,
					    bldgHeight,
					    lidarHeightResult,
					    rlanHeightSource);
	if (_rlanHeightType == CConst::AMSLHeightType) {
		rlanHeight = rlanHeightInput;
	} else if (_rlanHeightType == CConst::AGLHeightType) {
		rlanHeight = rlanHeightInput + rlanTerrainHeight;
	} else {
		throw std::runtime_error(ErrStream() << "ERROR: INVALID_VALUE _rlanHeightType = "
						     << _rlanHeightType);
	}

	if (rlanHeight - heightUncertainty - rlanTerrainHeight < _minRlanHeightAboveTerrain) {
		throw std::runtime_error(
			ErrStream()
			<< std::string("ERROR: ItoN: Invalid RLAN parameter settings.") << std::endl
			<< std::string("RLAN Height = ") << rlanHeight << std::endl
			<< std::string("Height Uncertainty = ") << heightUncertainty << std::endl
			<< std::string("Terrain Height at RLAN Location = ") << rlanTerrainHeight
			<< std::endl
			<< std::string("RLAN is ")
			<< rlanHeight - heightUncertainty - rlanTerrainHeight
			<< " meters above terrain" << std::endl
			<< std::string("RLAN must be more than ") << _minRlanHeightAboveTerrain
			<< " meters above terrain" << std::endl);
	}

	Vector3 rlanCenterPosn = EcefModel::geodeticToEcef(rlanLat, rlanLon, rlanHeight / 1000.0);

	Vector3 rlanPosn0;
	if ((rlanHeight - heightUncertainty < fsHeight) &&
	    (rlanHeight + heightUncertainty > fsHeight)) {
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
		fsPropEnv = computePropEnv(uls->getRxLongitudeDeg(),
					   uls->getRxLatitudeDeg(),
					   nlcdLandCatRx);
	} else {
		fsPropEnv = CConst::unknownPropEnv;
	}
	/**************************************************************************************/

	Vector3 upVec = rlanCenterPosn.normalized();
	const Vector3 ulsRxPos = uls->getRxPosition();

	rlanPosnList[0] = rlanPosn0; // RLAN Position
	rlanPosnList[1] = rlanCenterPosn +
			  (heightUncertainty / 1000.0) *
				  upVec; // RLAN Position raised by height uncertainty
	rlanPosnList[2] = rlanCenterPosn -
			  (heightUncertainty / 1000.0) *
				  upVec; // RLAN Position lowered by height uncertainty

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
		double elevationAngleTxDeg = 90.0 - acos(rlanPosn.dot(lineOfSightVectorKm) /
							 (dAP * distKm)) *
							    180.0 / M_PI;
		double elevationAngleRxDeg = 90.0 - acos(ulsRxPos.dot(-lineOfSightVectorKm) /
							 (duls * distKm)) *
							    180.0 / M_PI;

		// Use Haversine formula with average earth radius of 6371 km
		double lon1Rad = rlanCoord.longitudeDeg * M_PI / 180.0;
		double lat1Rad = rlanCoord.latitudeDeg * M_PI / 180.0;
		double lon2Rad = uls->getRxLongitudeDeg() * M_PI / 180.0;
		double lat2Rad = uls->getRxLatitudeDeg() * M_PI / 180.0;
		double slat = sin((lat2Rad - lat1Rad) / 2);
		double slon = sin((lon2Rad - lon1Rad) / 2);
		double groundDistanceKm = 2 * CConst::averageEarthRadius *
					  asin(sqrt(slat * slat +
						    cos(lat1Rad) * cos(lat2Rad) * slon * slon)) *
					  1.0e-3;

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
		double buildingPenetrationDB =
			computeBuildingPenetration(_buildingType,
						   elevationAngleTxDeg,
						   chanCenterFreq,
						   buildingPenetrationModelStr,
						   buildingPenetrationCDF);

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

		computePathLoss(_pathLossModel,
				false,
				(rlanPropEnv == CConst::unknownPropEnv ? CConst::barrenPropEnv :
									 rlanPropEnv),
				fsPropEnv,
				nlcdLandCatTx,
				nlcdLandCatRx,
				distKm,
				fsplDistKm,
				win2DistKm,
				chanCenterFreq,
				rlanCoord.longitudeDeg,
				rlanCoord.latitudeDeg,
				rlanHtAboveTerrain,
				elevationAngleTxDeg,
				uls->getRxLongitudeDeg(),
				uls->getRxLatitudeDeg(),
				uls->getRxHeightAboveTerrain(),
				elevationAngleRxDeg,
				pathLoss,
				pathClutterTxDB,
				pathClutterRxDB,
				pathLossModelStr,
				pathLossCDF,
				pathClutterTxModelStr,
				pathClutterTxCDF,
				pathClutterRxModelStr,
				pathClutterRxCDF,
				&txClutterStr,
				&rxClutterStr,
				&(uls->ITMHeightProfile),
				&(uls->isLOSHeightProfile),
				&(uls->isLOSSurfaceFrac)
#if DEBUG_AFC
					,
				uls->ITMHeightType
#endif
		);

		std::string rxAntennaSubModelStr;
		double angleOffBoresightDeg = acos(uls->getAntennaPointing().dot(
						      -(lineOfSightVectorKm.normalized()))) *
					      180.0 / M_PI;
		double rxGainDB = uls->computeRxGain(angleOffBoresightDeg,
						     elevationAngleRxDeg,
						     chanCenterFreq,
						     rxAntennaSubModelStr,
						     0);

		double rxPowerDBW = (_exclusionZoneRLANEIRPDBm - 30.0) - _bodyLossDB -
				    buildingPenetrationDB - pathLoss - pathClutterTxDB -
				    pathClutterRxDB + rxGainDB - spectralOverlapLossDB -
				    _polarizationLossDB - uls->getRxAntennaFeederLossDB();

		double I2NDB = rxPowerDBW - uls->getNoiseLevelDBW();

		double marginDB = _IoverN_threshold_dB - I2NDB;

		if ((rlanPosnIdx == 0) || (marginDB < minMarginDB)) {
			minMarginDB = marginDB;
			distKmM = distKm;
		}

		if (excthrGc && *excthrGc) {
			double d1;
			double d2;
			double pathDifference;
			double fresnelIndex = -1.0;
			double ulsLinkDistance = uls->getLinkDistance();
			double ulsWavelength = CConst::c /
					       ((uls->getStartFreq() + uls->getStopFreq()) / 2);
			if (ulsLinkDistance != -1.0) {
				int numPR = uls->getNumPR();
				const Vector3 ulsTxPos = (numPR ? uls->getPR(numPR - 1).positionTx :
								  uls->getTxPosition());
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
			if (ulsRxAntennaType == CConst::LUTAntennaType) {
				rxAntennaTypeStr = std::string(uls->getRxAntenna()->get_strid());
			} else {
				rxAntennaTypeStr =
					std::string(CConst::strULSAntennaTypeList->type_to_str(
						ulsRxAntennaType)) +
					rxAntennaSubModelStr;
			}

			std::string bldgTypeStr = (_fixedBuildingLossFlag ? "INDOOR_FIXED" :
						   _buildingType == CConst::noBuildingType ?
									    "OUTDOOR" :
						   _buildingType ==
								   CConst::traditionalBuildingType ?
									    "TRADITIONAL" :
									    "THERMALLY_EFFICIENT");

			excthrGc->fsid = uls->getID();
			excthrGc->dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);
			excthrGc->rlanPosnIdx = rlanPosnIdx;
			excthrGc->callsign = uls->getCallsign();
			excthrGc->fsLon = uls->getRxLongitudeDeg();
			excthrGc->fsLat = uls->getRxLatitudeDeg();
			excthrGc->fsAgl = uls->getRxHeightAboveTerrain();
			excthrGc->fsTerrainHeight = uls->getRxTerrainHeight();
			excthrGc->fsTerrainSource = _terrainDataModel->getSourceName(
				uls->getRxHeightSource());
			excthrGc->fsPropEnv = ulsRxPropEnv;
			excthrGc->numPr = uls->getNumPR();
			excthrGc->rlanLon = rlanCoord.longitudeDeg;
			excthrGc->rlanLat = rlanCoord.latitudeDeg;
			excthrGc->rlanAgl = rlanCoord.heightKm * 1000.0 - rlanTerrainHeight;
			excthrGc->rlanTerrainHeight = rlanTerrainHeight;
			excthrGc->rlanTerrainSource = _terrainDataModel->getSourceName(
				rlanHeightSource);
			excthrGc->rlanPropEnv = CConst::strPropEnvList->type_to_str(rlanPropEnv);
			excthrGc->rlanFsDist = distKm;
			excthrGc->rlanFsGroundDist = groundDistanceKm;
			excthrGc->rlanElevAngle = elevationAngleTxDeg;
			excthrGc->boresightAngle = angleOffBoresightDeg;
			excthrGc->rlanTxEirp = _exclusionZoneRLANEIRPDBm;
			excthrGc->bodyLoss = _bodyLossDB;
			excthrGc->rlanClutterCategory = txClutterStr;
			excthrGc->fsClutterCategory = rxClutterStr;
			excthrGc->buildingType = bldgTypeStr;
			excthrGc->buildingPenetration = buildingPenetrationDB;
			excthrGc->buildingPenetrationModel = buildingPenetrationModelStr;
			excthrGc->buildingPenetrationCdf = buildingPenetrationCDF;
			excthrGc->pathLoss = pathLoss;
			excthrGc->pathLossModel = pathLossModelStr;
			excthrGc->pathLossCdf = pathLossCDF;
			excthrGc->pathClutterTx = pathClutterTxDB;
			excthrGc->pathClutterTxMode = pathClutterTxModelStr;
			excthrGc->pathClutterTxCdf = pathClutterTxCDF;
			excthrGc->pathClutterRx = pathClutterRxDB;
			excthrGc->pathClutterRxMode = pathClutterRxModelStr;
			excthrGc->pathClutterRxCdf = pathClutterRxCDF;
			excthrGc->rlanBandwidth = bandwidth * 1.0e-6;
			excthrGc->rlanStartFreq = chanStartFreq * 1.0e-6;
			excthrGc->rlanStopFreq = chanStopFreq * 1.0e-6;
			excthrGc->ulsStartFreq = uls->getStartFreq() * 1.0e-6;
			excthrGc->ulsStopFreq = uls->getStopFreq() * 1.0e-6;
			excthrGc->antType = rxAntennaTypeStr;
			excthrGc->antGainPeak = uls->getRxGain();
			excthrGc->fsGainToRlan = rxGainDB;
			excthrGc->spectralOverlapLoss = spectralOverlapLossDB;
			excthrGc->polarizationLoss = _polarizationLossDB;
			excthrGc->fsRxFeederLoss = uls->getRxAntennaFeederLossDB();
			excthrGc->fsRxPwr = rxPowerDBW;
			excthrGc->fsIN = rxPowerDBW - uls->getNoiseLevelDBW();
			excthrGc->ulsLinkDist = ulsLinkDistance;
			excthrGc->rlanCenterFreq = chanCenterFreq;
			excthrGc->fsTxToRlanDist = d2;
			excthrGc->pathDifference = pathDifference;
			excthrGc->ulsWavelength = ulsWavelength * 1000;
			excthrGc->fresnelIndex = fresnelIndex;
			excthrGc->comment = comment;

			excthrGc->completeRow();
		}
	}

	if (uls->ITMHeightProfile) {
		free(uls->ITMHeightProfile);
		uls->ITMHeightProfile = (double *)NULL;
	}
	if (uls->isLOSHeightProfile) {
		free(uls->isLOSHeightProfile);
		uls->isLOSHeightProfile = (double *)NULL;
	}
	/**************************************************************************************/

	return (minMarginDB);
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::writeKML()                                                                 */
/******************************************************************************************/
void AfcManager::writeKML()
{
	int ulsIdx;
	ULSClass *uls = findULSID(_exclusionZoneFSID, 0, ulsIdx);
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
	fkml->writeTextElement("name", "AFC");
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
	fkml->writeTextElement(
		"name",
		QString("FSID : %1_%s").arg(QString::fromStdString(dbName)).arg(uls->getID()));
	fkml->writeTextElement("visibility", "0");
	fkml->writeStartElement("Point");
	fkml->writeTextElement("extrude", "1");
	fkml->writeTextElement("altitudeMode", "absolute");
	fkml->writeTextElement("coordinates",
			       QString::asprintf("%12.10f,%12.10f,%5.3f",
						 uls->getRxLongitudeDeg(),
						 uls->getRxLatitudeDeg(),
						 _exclusionZoneFSTerrainHeight +
							 _exclusionZoneHeightAboveTerrain));
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
	for (exclPtIdx = 0; exclPtIdx < (int)_exclusionZone.size(); ++exclPtIdx) {
		double rlanLon = std::get<0>(_exclusionZone[exclPtIdx]);
		double rlanLat = std::get<1>(_exclusionZone[exclPtIdx]);
		double rlanHeight;
		double rlanTerrainHeight, bldgHeight;
		MultibandRasterClass::HeightResult lidarHeightResult;
		CConst::HeightSourceEnum heightSource;
		_terrainDataModel->getTerrainHeight(rlanLon,
						    rlanLat,
						    rlanTerrainHeight,
						    bldgHeight,
						    lidarHeightResult,
						    heightSource);
		if (_rlanHeightType == CConst::AMSLHeightType) {
			rlanHeight = rlanHeightInput;
		} else if (_rlanHeightType == CConst::AGLHeightType) {
			rlanHeight = rlanHeightInput + rlanTerrainHeight;
		} else {
			throw std::runtime_error(ErrStream() << "ERROR: INVALID _rlanHeightType = "
							     << _rlanHeightType);
		}
		excls_coords.append(
			QString::asprintf("%.10f,%.10f,%.5f\n", rlanLon, rlanLat, rlanHeight));
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

inline int mkcolor(int r, int g, int b)
{
	if ((r < 0) || (r > 255)) {
		CORE_DUMP;
	}
	if ((g < 0) || (g > 255)) {
		CORE_DUMP;
	}
	if ((b < 0) || (b > 255)) {
		CORE_DUMP;
	}

	int color = (r << 16) | (g << 8) | b;

	return (color);
}

/******************************************************************************************/
/* AfcManager::defineHeatmapColors()                                                      */
/******************************************************************************************/
void AfcManager::defineHeatmapColors()
{
	bool itonFlag = (_heatmapAnalysisStr == "iton");

	int BlackColor = mkcolor(0, 0, 0);
	int WhiteColor = mkcolor(255, 255, 255);
	int LightGrayColor = mkcolor(211, 211, 211);
	int DarkGrayColor = mkcolor(169, 169, 169);
	int BlueColor = mkcolor(0, 0, 255);
	int DarkBlueColor = mkcolor(0, 0, 139);
	int GreenColor = mkcolor(0, 128, 0);
	int DarkGreenColor = mkcolor(0, 100, 0);
	int YellowColor = mkcolor(255, 255, 0);
	int OrangeColor = mkcolor(255, 165, 0);
	int RedColor = mkcolor(255, 0, 0);
	int MaroonColor = mkcolor(128, 0, 0);

	/**************************************************************************************/
	/* Define color scheme                                                                */
	/**************************************************************************************/
	_heatmapColorList.push_back(BlackColor);
	_heatmapColorList.push_back(WhiteColor);

	if (itonFlag) {
		_heatmapColorList.push_back(LightGrayColor);
		_heatmapIndoorThrList.push_back(_IoverN_threshold_dB - 20.0);
		_heatmapColorList.push_back(DarkGrayColor);
		_heatmapIndoorThrList.push_back(_IoverN_threshold_dB);
		_heatmapColorList.push_back(BlueColor);
		_heatmapIndoorThrList.push_back(_IoverN_threshold_dB + 3.0);
		_heatmapColorList.push_back(DarkBlueColor);
		_heatmapIndoorThrList.push_back(_IoverN_threshold_dB + 6.0);
		_heatmapColorList.push_back(GreenColor);
		_heatmapIndoorThrList.push_back(_IoverN_threshold_dB + 9.0);
		_heatmapColorList.push_back(DarkGreenColor);
		_heatmapIndoorThrList.push_back(_IoverN_threshold_dB + 12.0);
		_heatmapColorList.push_back(YellowColor);
		_heatmapIndoorThrList.push_back(_IoverN_threshold_dB + 15.0);
		_heatmapColorList.push_back(OrangeColor);
		_heatmapIndoorThrList.push_back(_IoverN_threshold_dB + 18.0);
		_heatmapColorList.push_back(RedColor);
		_heatmapIndoorThrList.push_back(_IoverN_threshold_dB + 21.0);
		_heatmapColorList.push_back(MaroonColor);

		_heatmapOutdoorThrList = _heatmapIndoorThrList;
	} else {
		_heatmapColorList.push_back(GreenColor);
		_heatmapIndoorThrList.push_back(_IoverN_threshold_dB);
		_heatmapOutdoorThrList.push_back(_IoverN_threshold_dB);
		_heatmapColorList.push_back(YellowColor);
		_heatmapIndoorThrList.push_back(_IoverN_threshold_dB + _maxEIRP_dBm -
						_minEIRPIndoor_dBm);
		_heatmapOutdoorThrList.push_back(_IoverN_threshold_dB + _maxEIRP_dBm -
						 _minEIRPOutdoor_dBm);
		_heatmapColorList.push_back(RedColor);
	}
	/**************************************************************************************/
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::getHeatmapColor()                                                          */
/******************************************************************************************/
std::string AfcManager::getHeatmapColor(double itonVal, bool indoorFlag, bool hexFlag)
{
	std::vector<double> *thrList = (indoorFlag ? &_heatmapIndoorThrList :
						     &_heatmapOutdoorThrList);

	int n;
	if (itonVal == std::numeric_limits<double>::infinity()) {
		n = 0;
	} else if (itonVal == -std::numeric_limits<double>::infinity()) {
		n = 1;
	} else {
		int numThr = thrList->size();
		int k;
		if (itonVal < (*thrList)[0]) {
			k = 0;
		} else if (itonVal >= (*thrList)[numThr - 1]) {
			k = numThr;
		} else {
			int k0 = 0;
			int k1 = numThr - 1;
			while (k1 > k0 + 1) {
				int km = (k0 + k1) / 2;
				if (itonVal < (*thrList)[km]) {
					k1 = km;
				} else {
					k0 = km;
				}
			}
			k = k1;
		}
		n = k + 2;
	}

	int color = _heatmapColorList[n];
	std::string colorStr;

	if (hexFlag) {
		char hexstr[7];
		sprintf(hexstr, "%06x", color);
		colorStr = std::string("#") + hexstr;
	} else {
		colorStr = to_string((color >> 16) & 0xFF) + " " + to_string((color >> 8) & 0xFF) +
			   " " + to_string(color & 0xFF);
	}

	return (colorStr);
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::runHeatmapAnalysis()                                                       */
/******************************************************************************************/
void AfcManager::runHeatmapAnalysis()
{
	std::ostringstream errStr;

	LOGGER_INFO(logger) << "Executing AfcManager::runHeatmapAnalysis()";

	if (_channelList.size() != 1) {
		throw std::runtime_error(ErrStream()
					 << "ERROR: Number of channels = " << _channelList.size()
					 << " require exactly 1 channel for heatmap analysis");
	}

	ChannelStruct *channel = &(_channelList[0]);
	int freqSegIdx = 0;

	double chanStartFreq = channel->freqMHzList[freqSegIdx] * 1.0e6;
	double chanStopFreq = channel->freqMHzList[freqSegIdx + 1] * 1.0e6;
	double chanCenterFreq = (chanStartFreq + chanStopFreq) / 2;
	double chanBandwidth = chanStopFreq - chanStartFreq;
	bool useACI = (channel->type == INQUIRED_FREQUENCY ? false : _aciFlag);
	CConst::SpectralAlgorithmEnum spectralAlgorithm = (channel->type == INQUIRED_FREQUENCY ?
								   CConst::psdSpectralAlgorithm :
								   _channelResponseAlgorithm);

	_heatmapNumPtsLat = ceil((_heatmapMaxLat - _heatmapMinLat) * M_PI / 180.0 *
				 CConst::earthRadius / _heatmapRLANSpacing);

	double minAbsLat;
	if ((_heatmapMinLat < 0.0) && (_heatmapMaxLat > 0.0)) {
		minAbsLat = 0.0;
	} else {
		minAbsLat = std::min(fabs(_heatmapMinLat), fabs(_heatmapMaxLat));
	}

	_heatmapNumPtsLon = ceil((_heatmapMaxLon - _heatmapMinLon) * M_PI / 180.0 *
				 CConst::earthRadius * cos(minAbsLat * M_PI / 180.0) /
				 _heatmapRLANSpacing);

	int totNumProc = _heatmapNumPtsLon * _heatmapNumPtsLat;
	LOGGER_INFO(logger) << "NUM_PTS_LON: " << _heatmapNumPtsLon;
	LOGGER_INFO(logger) << "NUM_PTS_LAT: " << _heatmapNumPtsLat;
	LOGGER_INFO(logger) << "TOT_PTS: " << totNumProc;

	_heatmapIToNThresholdDB = _IoverN_threshold_dB;

	/**************************************************************************************/
	/* Create List of FS's that have spectral overlap                                     */
	/**************************************************************************************/
	int ulsIdx;
	for (ulsIdx = 0; ulsIdx < _ulsList->getSize(); ulsIdx++) {
		ULSClass *uls = (*_ulsList)[ulsIdx];
		double spectralOverlapLossDB;
		bool hasOverlap = computeSpectralOverlapLoss(&spectralOverlapLossDB,
							     chanStartFreq,
							     chanStopFreq,
							     uls->getStartFreq(),
							     uls->getStopFreq(),
							     useACI,
							     spectralAlgorithm);
		if (hasOverlap) {
			_ulsIdxList.push_back(
				ulsIdx); // Store the ULS indices that are used in analysis
		}
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Allocate / Initialize heatmap                                                      */
	/**************************************************************************************/
	_heatmapIToNDB = (double **)malloc(_heatmapNumPtsLon * sizeof(double *));
	_heatmapIsIndoor = (bool **)malloc(_heatmapNumPtsLon * sizeof(bool *));

	int lonIdx, latIdx;
	for (lonIdx = 0; lonIdx < _heatmapNumPtsLon; ++lonIdx) {
		_heatmapIToNDB[lonIdx] = (double *)malloc(_heatmapNumPtsLat * sizeof(double));
		_heatmapIsIndoor[lonIdx] = (bool *)malloc(_heatmapNumPtsLat * sizeof(bool));
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Create excThrFile, useful for debugging                                            */
	/**************************************************************************************/
	ExThrGzipCsv excthrGc(_excThrFile);
	/**************************************************************************************/

	/**************************************************************************************/
	/* Compute Heatmap                                                                    */
	/**************************************************************************************/
	const double exclusionDistKmSquared = (_exclusionDist / 1000.0) * (_exclusionDist / 1000.0);
	const double maxRadiusKmSquared = (_maxRadius / 1000.0) * (_maxRadius / 1000.0);

	Vector3 rlanPosnList[3];
	GeodeticCoord rlanCoordList[3];
	_heatmapMaxRLANHeightAGL = quietNaN;

#if DEBUG_AFC
	char *tstr;

	time_t tStartHeatmap = time(NULL);
	tstr = strdup(ctime(&tStartHeatmap));
	strtok(tstr, "\n");

	LOGGER_INFO(logger) << "Begin Heatmap Scan: " << tstr;

	free(tstr);

#endif

	int numPct = 100;

	if (numPct > totNumProc) {
		numPct = totNumProc;
	}

	bool itonFlag = (_heatmapAnalysisStr == "iton");

	bool initFlag = false;
	int numInvalid = 0;
	int numProc = 0;
	for (lonIdx = 0; lonIdx < _heatmapNumPtsLon; ++lonIdx) {
		double rlanLon = (_heatmapMinLon * (2 * _heatmapNumPtsLon - 2 * lonIdx - 1) +
				  _heatmapMaxLon * (2 * lonIdx + 1)) /
				 (2 * _heatmapNumPtsLon);
		for (latIdx = 0; latIdx < _heatmapNumPtsLat; ++latIdx) {
			double rlanLat = (_heatmapMinLat *
						  (2 * _heatmapNumPtsLat - 2 * latIdx - 1) +
					  _heatmapMaxLat * (2 * latIdx + 1)) /
					 (2 * _heatmapNumPtsLat);
			// LOGGER_DEBUG(logger) << "Heatmap point: (" << lonIdx << ", " << latIdx <<
			// ")";

#if DEBUG_AFC
			auto t1 = std::chrono::high_resolution_clock::now();
#endif

			double rlanHeight;
			double rlanTerrainHeight, bldgHeight;
			MultibandRasterClass::HeightResult lidarHeightResult;
			CConst::HeightSourceEnum rlanHeightSource;
			_terrainDataModel->getTerrainHeight(rlanLon,
							    rlanLat,
							    rlanTerrainHeight,
							    bldgHeight,
							    lidarHeightResult,
							    rlanHeightSource);

			if (_heatmapIndoorOutdoorStr == "Outdoor") {
				_buildingType = CConst::noBuildingType;
			} else if (_heatmapIndoorOutdoorStr == "Indoor") {
				_buildingType = CConst::traditionalBuildingType;
			} else if (_heatmapIndoorOutdoorStr == "Database") {
				if (lidarHeightResult ==
				    MultibandRasterClass::HeightResult::BUILDING) {
					_buildingType = CConst::traditionalBuildingType;
				} else {
					_buildingType = CConst::noBuildingType;
				}
			}

			double rlanEIRP_dBm = _maxEIRP_dBm;
			double rlanHeightInput, heightUncertainty;
			std::string rlanHeightType;
			if (_buildingType == CConst::noBuildingType) {
				if (itonFlag) {
					rlanEIRP_dBm = _heatmapRLANOutdoorEIRPDBm;
				}
				rlanHeightInput = _heatmapRLANOutdoorHeight;
				heightUncertainty = _heatmapRLANOutdoorHeightUncertainty;
				rlanHeightType = _heatmapRLANOutdoorHeightType;
				_bodyLossDB = _bodyLossOutdoorDB;
			} else {
				if (itonFlag) {
					rlanEIRP_dBm = _heatmapRLANIndoorEIRPDBm;
				}
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
				throw std::runtime_error(ErrStream()
							 << "ERROR: INVALID_VALUE rlanHeightType = "
							 << rlanHeightType);
			}

			if (rlanHeight - heightUncertainty - rlanTerrainHeight <
			    _minRlanHeightAboveTerrain) {
				throw std::runtime_error(
					ErrStream()
					<< std::string("ERROR: Heat Map: Invalid RLAN parameter "
						       "settings.")
					<< std::endl
					<< std::string("RLAN Height = ") << rlanHeight << std::endl
					<< std::string("Height Uncertainty = ") << heightUncertainty
					<< std::endl
					<< std::string("Terrain Height at RLAN Location = ")
					<< rlanTerrainHeight << std::endl
					<< std::string("RLAN is ")
					<< rlanHeight - heightUncertainty - rlanTerrainHeight
					<< " meters above terrain" << std::endl
					<< std::string("RLAN must be more than ")
					<< _minRlanHeightAboveTerrain << " meters above terrain"
					<< std::endl);
			}

			CConst::NLCDLandCatEnum nlcdLandCatTx;
			CConst::PropEnvEnum rlanPropEnv = computePropEnv(rlanLon,
									 rlanLat,
									 nlcdLandCatTx);

			rlanCoordList[0] = GeodeticCoord::fromLatLon(
				rlanLat,
				rlanLon,
				(rlanHeight + heightUncertainty) / 1000.0);
			rlanCoordList[1] = GeodeticCoord::fromLatLon(rlanLat,
								     rlanLon,
								     rlanHeight / 1000.0);
			rlanCoordList[2] = GeodeticCoord::fromLatLon(
				rlanLat,
				rlanLon,
				(rlanHeight - heightUncertainty) / 1000.0);

			rlanPosnList[0] = EcefModel::fromGeodetic(rlanCoordList[0]);
			rlanPosnList[1] = EcefModel::fromGeodetic(rlanCoordList[1]);
			rlanPosnList[2] = EcefModel::fromGeodetic(rlanCoordList[2]);

			Vector3 rlanCenterPosn = rlanPosnList[1];
			if ((lonIdx == _heatmapNumPtsLon / 2) &&
			    (latIdx == _heatmapNumPtsLat / 2)) {
				_heatmapRLANCenterPosn = rlanCenterPosn;
				_heatmapRLANCenterLon = rlanLon;
				_heatmapRLANCenterLat = rlanLat;
			}

			int numRlanPosn = ((heightUncertainty == 0.0) ? 1 : 3);

			double maxIToNDB = -std::numeric_limits<double>::infinity();
			channel->segList[freqSegIdx] =
				std::make_tuple(std::numeric_limits<double>::infinity(),
						std::numeric_limits<double>::infinity(),
						GREEN);

			if (numRlanPosn) {
				GeodeticCoord rlanCoord = rlanCoordList[0];
				double rlanHeightAGL = (rlanCoord.heightKm * 1000) -
						       rlanTerrainHeight;

				if (std::isnan(_heatmapMaxRLANHeightAGL) ||
				    (rlanHeightAGL > _heatmapMaxRLANHeightAGL)) {
					_heatmapMaxRLANHeightAGL = rlanHeightAGL;
				}

				int drIdx;
				for (drIdx = 0; drIdx < (int)_deniedRegionList.size(); ++drIdx) {
					DeniedRegionClass *dr = _deniedRegionList[drIdx];
					if (dr->intersect(rlanCoord.longitudeDeg,
							  rlanCoord.latitudeDeg,
							  0.0,
							  rlanHeightAGL)) {
						if (std::get<2>(channel->segList[freqSegIdx]) !=
						    BLACK) {
							bool hasOverlap = computeSpectralOverlapLoss(
								(double *)NULL,
								chanStartFreq,
								chanStopFreq,
								dr->getStartFreq(),
								dr->getStopFreq(),
								false,
								CConst::psdSpectralAlgorithm);
							if (hasOverlap) {
								channel->segList[freqSegIdx] =
									std::make_tuple(
										-std::numeric_limits<
											double>::
											infinity(),
										-std::numeric_limits<
											double>::
											infinity(),
										BLACK);
								maxIToNDB = std::numeric_limits<
									double>::infinity();
							}
						}
					}
				}
			}

			int uIdx;
			for (uIdx = 0; (uIdx < (int)_ulsIdxList.size()) &&
				       (maxIToNDB != std::numeric_limits<double>::infinity());
			     uIdx++) {
				ulsIdx = _ulsIdxList[uIdx];
				ULSClass *uls = (*_ulsList)[ulsIdx];

				int numPR = uls->getNumPR();
				int numDiversity = (uls->getHasDiversity() ? 2 : 1);

				int segStart = (_passiveRepeaterFlag ? 0 : numPR);

				for (int segIdx = segStart; segIdx < numPR + 1; ++segIdx) {
					for (int divIdx = 0; divIdx < numDiversity; ++divIdx) {
						Vector3 ulsRxPos =
							(segIdx == numPR ?
								 (divIdx == 0 ?
									  uls->getRxPosition() :
									  uls->getDiversityPosition()) :
								 uls->getPR(segIdx).positionRx);
						double ulsRxLongitude =
							(segIdx == numPR ?
								 uls->getRxLongitudeDeg() :
								 uls->getPR(segIdx).longitudeDeg);
						double ulsRxLatitude =
							(segIdx == numPR ?
								 uls->getRxLatitudeDeg() :
								 uls->getPR(segIdx).latitudeDeg);

						Vector3 lineOfSightVectorKm = ulsRxPos -
									      rlanCenterPosn;
						double distKmSquared =
							(lineOfSightVectorKm)
								.dot(lineOfSightVectorKm);

#if 0
						// For debugging, identifies anomalous ULS entries
						if (uls->getLinkDistance() == -1) {
							std::string dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);
							std::cout << dbName << "_" << uls->getID() << std::endl;
						}
#endif

						if (distKmSquared <= exclusionDistKmSquared) {
							channel->segList[freqSegIdx] =
								std::make_tuple(
									-std::numeric_limits<
										double>::infinity(),
									-std::numeric_limits<
										double>::infinity(),
									BLACK);
							maxIToNDB = std::numeric_limits<
								double>::infinity();
						} else if (distKmSquared < maxRadiusKmSquared) {
							double ulsRxHeightAGL =
								(segIdx == numPR ?
									 (divIdx == 0 ?
										  uls->getRxHeightAboveTerrain() :
										  uls->getDiversityHeightAboveTerrain()) :
									 uls->getPR(segIdx)
										 .heightAboveTerrainRx);
							double ulsRxHeightAMSL =
								(segIdx == numPR ?
									 (divIdx == 0 ?
										  uls->getRxHeightAMSL() :
										  uls->getDiversityHeightAMSL()) :
									 uls->getPR(segIdx)
										 .heightAMSLRx);
							double ulsSegmentDistance =
								(segIdx == numPR ?
									 uls->getLinkDistance() :
									 uls->getPR(segIdx)
										 .segmentDistance);

							/**************************************************************************************/
							/* Determine propagation environment of FS
							 * segment RX, if needed. */
							/**************************************************************************************/
							char ulsRxPropEnv = ' ';
							CConst::NLCDLandCatEnum nlcdLandCatRx;
							CConst::PropEnvEnum fsPropEnv;
							if ((_applyClutterFSRxFlag) &&
							    (ulsRxHeightAGL <= _maxFsAglHeight)) {
								fsPropEnv = computePropEnv(
									ulsRxLongitude,
									ulsRxLatitude,
									nlcdLandCatRx);
								switch (fsPropEnv) {
									case CConst::urbanPropEnv:
										ulsRxPropEnv = 'U';
										break;
									case CConst::
										suburbanPropEnv:
										ulsRxPropEnv = 'S';
										break;
									case CConst::ruralPropEnv:
										ulsRxPropEnv = 'R';
										break;
									case CConst::barrenPropEnv:
										ulsRxPropEnv = 'B';
										break;
									case CConst::unknownPropEnv:
										ulsRxPropEnv = 'X';
										break;
									default:
										CORE_DUMP;
								}
							} else {
								fsPropEnv = CConst::unknownPropEnv;
								ulsRxPropEnv = ' ';
							}
							/**************************************************************************************/

							Vector3 ulsAntennaPointing =
								(segIdx == numPR ?
									 (divIdx == 0 ?
										  uls->getAntennaPointing() :
										  uls->getDiversityAntennaPointing()) :
									 uls->getPR(segIdx)
										 .pointing);

							// Use Haversine formula with average earth
							// radius of 6371 km
							double groundDistanceKm;
							{
								double lon1Rad = rlanLon * M_PI /
										 180.0;
								double lat1Rad = rlanLat * M_PI /
										 180.0;
								double lon2Rad = ulsRxLongitude *
										 M_PI / 180.0;
								double lat2Rad = ulsRxLatitude *
										 M_PI / 180.0;
								double slat = sin(
									(lat2Rad - lat1Rad) / 2);
								double slon = sin(
									(lon2Rad - lon1Rad) / 2);
								groundDistanceKm =
									2 *
									CConst::averageEarthRadius *
									asin(sqrt(
										slat * slat +
										cos(lat1Rad) *
											cos(lat2Rad) *
											slon *
											slon)) *
									1.0e-3;
							}

							for (int rlanHtIdx = 0;
							     rlanHtIdx < numRlanPosn;
							     ++rlanHtIdx) {
								Vector3 rlanPosn =
									rlanPosnList[rlanHtIdx];
								GeodeticCoord rlanCoord =
									rlanCoordList[rlanHtIdx];
								lineOfSightVectorKm = ulsRxPos -
										      rlanPosn;
								double distKm =
									lineOfSightVectorKm.len();
								double win2DistKm;
								if (_winner2UseGroundDistanceFlag) {
									win2DistKm =
										groundDistanceKm;
								} else {
									win2DistKm = distKm;
								}
								double fsplDistKm;
								if (_fsplUseGroundDistanceFlag) {
									fsplDistKm =
										groundDistanceKm;
								} else {
									fsplDistKm = distKm;
								}

								double dAP = rlanPosn.len();
								double duls = ulsRxPos.len();
								double elevationAngleTxDeg =
									90.0 -
									acos(rlanPosn.dot(
										     lineOfSightVectorKm) /
									     (dAP * distKm)) *
										180.0 / M_PI;
								double elevationAngleRxDeg =
									90.0 -
									acos(ulsRxPos.dot(
										     -lineOfSightVectorKm) /
									     (duls * distKm)) *
										180.0 / M_PI;

								double rlanAngleOffBoresightRad;
								double rlanDiscriminationGainDB;
								if (_rlanAntenna) {
									double cosAOB =
										_rlanPointing.dot(
											lineOfSightVectorKm) /
										distKm;
									if (cosAOB > 1.0) {
										cosAOB = 1.0;
									} else if (cosAOB < -1.0) {
										cosAOB = -1.0;
									}
									rlanAngleOffBoresightRad =
										acos(cosAOB);
									rlanDiscriminationGainDB =
										_rlanAntenna->gainDB(
											rlanAngleOffBoresightRad);
								} else {
									rlanAngleOffBoresightRad =
										0.0;
									rlanDiscriminationGainDB =
										0.0;
								}

								double spectralOverlapLossDB;
								bool hasOverlap =
									computeSpectralOverlapLoss(
										&spectralOverlapLossDB,
										chanStartFreq,
										chanStopFreq,
										uls->getStartFreq(),
										uls->getStopFreq(),
										useACI,
										spectralAlgorithm);
								if (hasOverlap) {
									std::string
										buildingPenetrationModelStr;
									double buildingPenetrationCDF;
									double buildingPenetrationDB = computeBuildingPenetration(
										_buildingType,
										elevationAngleTxDeg,
										chanCenterFreq,
										buildingPenetrationModelStr,
										buildingPenetrationCDF);

									std::string txClutterStr;
									std::string rxClutterStr;
									std::string
										pathLossModelStr;
									double pathLossCDF;
									double pathLoss;
									std::string
										pathClutterTxModelStr;
									double pathClutterTxCDF;
									double pathClutterTxDB;
									std::string
										pathClutterRxModelStr;
									double pathClutterRxCDF;
									double pathClutterRxDB;
									double rxGainDB;
									double discriminationGain;
									std::string
										rxAntennaSubModelStr;
									double angleOffBoresightDeg;
									double rxPowerDBW;
									double I2NDB;
									double marginDB;
									double eirpLimit_dBm;
									double nearFieldOffsetDB;
									double nearField_xdb;
									double nearField_u;
									double nearField_eff;
									double reflectorD0;
									double reflectorD1;

									double rlanHtAboveTerrain =
										rlanCoord.heightKm *
											1000.0 -
										rlanTerrainHeight;

									computePathLoss(
										_pathLossModel,
										false,
										rlanPropEnv,
										fsPropEnv,
										nlcdLandCatTx,
										nlcdLandCatRx,
										distKm,
										fsplDistKm,
										win2DistKm,
										chanCenterFreq,
										rlanCoord
											.longitudeDeg,
										rlanCoord
											.latitudeDeg,
										rlanHtAboveTerrain,
										elevationAngleTxDeg,
										uls->getRxLongitudeDeg(),
										uls->getRxLatitudeDeg(),
										uls->getRxHeightAboveTerrain(),
										elevationAngleRxDeg,
										pathLoss,
										pathClutterTxDB,
										pathClutterRxDB,
										pathLossModelStr,
										pathLossCDF,
										pathClutterTxModelStr,
										pathClutterTxCDF,
										pathClutterRxModelStr,
										pathClutterRxCDF,
										&txClutterStr,
										&rxClutterStr,
										&(uls->ITMHeightProfile),
										&(uls->isLOSHeightProfile),
										&(uls->isLOSSurfaceFrac)
#if DEBUG_AFC
											,
										uls->ITMHeightType
#endif
									);

									angleOffBoresightDeg =
										acos(uls->getAntennaPointing()
											     .dot(-(lineOfSightVectorKm
													    .normalized()))) *
										180.0 / M_PI;
									if (segIdx == numPR) {
										rxGainDB = uls->computeRxGain(
											angleOffBoresightDeg,
											elevationAngleRxDeg,
											chanCenterFreq,
											rxAntennaSubModelStr,
											divIdx);
									} else {
										discriminationGain =
											uls->getPR(segIdx)
												.computeDiscriminationGain(
													angleOffBoresightDeg,
													elevationAngleRxDeg,
													chanCenterFreq,
													reflectorD0,
													reflectorD1);
										rxGainDB =
											uls->getPR(segIdx)
												.effectiveGain +
											discriminationGain;
									}

									nearFieldOffsetDB = 0.0;
									nearField_xdb = quietNaN;
									nearField_u = quietNaN;
									nearField_eff = quietNaN;
									if (segIdx == numPR) {
										if (_nearFieldAdjFlag &&
										    (distKm *
											     1000.0 <
										     uls->getRxNearFieldDistLimit()) &&
										    (angleOffBoresightDeg <
										     90.0)) {
											bool unii5Flag = computeSpectralOverlapLoss(
												(double *)
													NULL,
												uls->getStartFreq(),
												uls->getStopFreq(),
												5925.0e6,
												6425.0e6,
												false,
												CConst::psdSpectralAlgorithm);
											double Fc;
											if (unii5Flag) {
												Fc = 6175.0e6;
											} else {
												Fc = 6700.0e6;
											}
											nearField_eff =
												uls->getRxNearFieldAntEfficiency();
											double D =
												uls->getRxNearFieldAntDiameter();

											nearField_xdb =
												10.0 *
												log10(CConst::c *
												      distKm *
												      1000.0 /
												      (2 *
												       Fc *
												       D *
												       D));
											nearField_u =
												(Fc *
												 D *
												 sin(angleOffBoresightDeg *
												     M_PI /
												     180.0) /
												 CConst::c);

											nearFieldOffsetDB = _nfa->computeNFA(
												nearField_xdb,
												nearField_u,
												nearField_eff);
										}
									}

									rxPowerDBW =
										(rlanEIRP_dBm -
										 30.0) +
										rlanDiscriminationGainDB -
										_bodyLossDB -
										buildingPenetrationDB -
										pathLoss -
										pathClutterTxDB -
										pathClutterRxDB +
										rxGainDB +
										nearFieldOffsetDB -
										spectralOverlapLossDB -
										_polarizationLossDB -
										uls->getRxAntennaFeederLossDB();

									I2NDB = rxPowerDBW -
										uls->getNoiseLevelDBW();

									if ((maxIToNDB ==
									     -std::numeric_limits<
										     double>::
										     infinity()) ||
									    (I2NDB > maxIToNDB)) {
										maxIToNDB = I2NDB;
										_heatmapIsIndoor[lonIdx][latIdx] =
											(_buildingType ==
													 CConst::noBuildingType ?
												 false :
												 true);
									}

									if (excthrGc &&
									    (std::isnan(
										     rxPowerDBW) ||
									     (I2NDB >
									      _visibilityThreshold) ||
									     (distKm * 1000 <
									      _closeInDist))) {
										double d1;
										double d2;
										double pathDifference;
										double fresnelIndex =
											-1.0;
										double ulsLinkDistance =
											uls->getLinkDistance();
										double ulsWavelength =
											CConst::c /
											((uls->getStartFreq() +
											  uls->getStopFreq()) /
											 2);
										if (ulsSegmentDistance !=
										    -1.0) {
											const Vector3 ulsTxPos =
												(segIdx ?
													 uls->getPR(segIdx -
														    1)
														 .positionTx :
													 uls->getTxPosition());
											d1 = (ulsRxPos -
											      rlanPosn)
												     .len() *
											     1000;
											d2 = (ulsTxPos -
											      rlanPosn)
												     .len() *
											     1000;
											pathDifference =
												d1 +
												d2 -
												ulsSegmentDistance;
											fresnelIndex =
												pathDifference /
												(ulsWavelength /
												 2);
										} else {
											d1 = (ulsRxPos -
											      rlanPosn)
												     .len() *
											     1000;
											d2 = -1.0;
											pathDifference =
												-1.0;
										}

										std::string
											rxAntennaTypeStr;
										if (segIdx ==
										    numPR) {
											CConst::ULSAntennaTypeEnum ulsRxAntennaType =
												uls->getRxAntennaType();
											if (ulsRxAntennaType ==
											    CConst::LUTAntennaType) {
												rxAntennaTypeStr = std::string(
													uls->getRxAntenna()
														->get_strid());
											} else {
												rxAntennaTypeStr =
													std::string(
														CConst::strULSAntennaTypeList
															->type_to_str(
																ulsRxAntennaType)) +
													rxAntennaSubModelStr;
											}
										} else {
											if (uls->getPR(segIdx)
												    .type ==
											    CConst::backToBackAntennaPRType) {
												CConst::ULSAntennaTypeEnum ulsRxAntennaType =
													uls->getPR(segIdx)
														.antennaType;
												if (ulsRxAntennaType ==
												    CConst::LUTAntennaType) {
													rxAntennaTypeStr = std::string(
														uls->getPR(segIdx)
															.antenna
															->get_strid());
												} else {
													rxAntennaTypeStr =
														std::string(
															CConst::strULSAntennaTypeList
																->type_to_str(
																	ulsRxAntennaType)) +
														rxAntennaSubModelStr;
												}
											} else {
												rxAntennaTypeStr =
													"";
											}
										}

										std::string bldgTypeStr =
											(_fixedBuildingLossFlag ?
												 "I"
												 "N"
												 "D"
												 "O"
												 "O"
												 "R"
												 "_"
												 "F"
												 "I"
												 "X"
												 "E"
												 "D" :
											 _buildingType ==
													 CConst::noBuildingType ?
												 "O"
												 "U"
												 "T"
												 "D"
												 "O"
												 "O"
												 "R" :
											 _buildingType ==
													 CConst::traditionalBuildingType ?
												 "T"
												 "R"
												 "A"
												 "D"
												 "I"
												 "T"
												 "I"
												 "O"
												 "N"
												 "A"
												 "L" :
												 "T"
												 "H"
												 "E"
												 "R"
												 "M"
												 "A"
												 "L"
												 "L"
												 "Y"
												 "_"
												 "E"
												 "F"
												 "F"
												 "I"
												 "C"
												 "I"
												 "E"
												 "N"
												 "T");

										excthrGc.fsid =
											uls->getID();
										excthrGc.region =
											uls->getRegion();
										excthrGc.dbName = std::get<
											0>(
											_ulsDatabaseList
												[uls->getDBIdx()]);
										excthrGc.rlanPosnIdx =
											rlanHtIdx;
										excthrGc.callsign =
											uls->getCallsign();
										excthrGc.fsLon =
											uls->getRxLongitudeDeg();
										excthrGc.fsLat =
											uls->getRxLatitudeDeg();
										excthrGc.fsAgl =
											divIdx == 0 ?
												uls->getRxHeightAboveTerrain() :
												uls->getDiversityHeightAboveTerrain();
										excthrGc.fsTerrainHeight =
											uls->getRxTerrainHeight();
										excthrGc.fsTerrainSource =
											_terrainDataModel
												->getSourceName(
													uls->getRxHeightSource());
										excthrGc.fsPropEnv =
											ulsRxPropEnv;
										excthrGc.numPr =
											uls->getNumPR();
										excthrGc.divIdx =
											divIdx;
										excthrGc.segIdx =
											segIdx;
										excthrGc.segRxLon =
											ulsRxLongitude;
										excthrGc.segRxLat =
											ulsRxLatitude;

										if ((segIdx <
										     numPR) &&
										    (uls->getPR(segIdx)
											     .type ==
										     CConst::billboardReflectorPRType)) {
											PRClass &pr = uls->getPR(
												segIdx);
											excthrGc.refThetaIn =
												pr.reflectorThetaIN;
											excthrGc.refKs =
												pr.reflectorKS;
											excthrGc.refQ =
												pr.reflectorQ;
											excthrGc.refD0 =
												reflectorD0;
											excthrGc.refD1 =
												reflectorD1;
										}

										excthrGc.rlanLon =
											rlanCoord
												.longitudeDeg;
										excthrGc.rlanLat =
											rlanCoord
												.latitudeDeg;
										excthrGc.rlanAgl =
											rlanCoord.heightKm *
												1000.0 -
											rlanTerrainHeight;
										excthrGc.rlanTerrainHeight =
											rlanTerrainHeight;
										excthrGc.rlanTerrainSource =
											_terrainDataModel
												->getSourceName(
													rlanHeightSource);
										excthrGc.rlanPropEnv =
											CConst::strPropEnvList
												->type_to_str(
													rlanPropEnv);
										excthrGc.rlanFsDist =
											distKm;
										excthrGc.rlanFsGroundDist =
											groundDistanceKm;
										excthrGc.rlanElevAngle =
											elevationAngleTxDeg;
										excthrGc.boresightAngle =
											angleOffBoresightDeg;
										excthrGc.rlanTxEirp =
											rlanEIRP_dBm;
										if (_rlanAntenna) {
											excthrGc.rlanAntennaModel =
												_rlanAntenna
													->get_strid();
											excthrGc.rlanAOB =
												rlanAngleOffBoresightRad *
												180.0 /
												M_PI;
										} else {
											excthrGc.rlanAntennaModel =
												"";
											excthrGc.rlanAOB =
												-1.0;
										}
										excthrGc.rlanDiscriminationGainDB =
											rlanDiscriminationGainDB;
										excthrGc.bodyLoss =
											_bodyLossDB;
										excthrGc.rlanClutterCategory =
											txClutterStr;
										excthrGc.fsClutterCategory =
											rxClutterStr;
										excthrGc.buildingType =
											bldgTypeStr;
										excthrGc.buildingPenetration =
											buildingPenetrationDB;
										excthrGc.buildingPenetrationModel =
											buildingPenetrationModelStr;
										excthrGc.buildingPenetrationCdf =
											buildingPenetrationCDF;
										excthrGc.pathLoss =
											pathLoss;
										excthrGc.pathLossModel =
											pathLossModelStr;
										excthrGc.pathLossCdf =
											pathLossCDF;
										excthrGc.pathClutterTx =
											pathClutterTxDB;
										excthrGc.pathClutterTxMode =
											pathClutterTxModelStr;
										excthrGc.pathClutterTxCdf =
											pathClutterTxCDF;
										excthrGc.pathClutterRx =
											pathClutterRxDB;
										excthrGc.pathClutterRxMode =
											pathClutterRxModelStr;
										excthrGc.pathClutterRxCdf =
											pathClutterRxCDF;
										excthrGc.rlanBandwidth =
											(chanStopFreq -
											 chanStartFreq) *
											1.0e-6;
										excthrGc.rlanStartFreq =
											chanStartFreq *
											1.0e-6;
										excthrGc.rlanStopFreq =
											chanStopFreq *
											1.0e-6;
										excthrGc.ulsStartFreq =
											uls->getStartFreq() *
											1.0e-6;
										excthrGc.ulsStopFreq =
											uls->getStopFreq() *
											1.0e-6;
										excthrGc.antType =
											rxAntennaTypeStr;
										excthrGc.antCategory =
											CConst::strAntennaCategoryList
												->type_to_str(
													segIdx == numPR ?
														uls->getRxAntennaCategory() :
														uls->getPR(segIdx)
															.antCategory);
										excthrGc.antGainPeak =
											uls->getRxGain();

										if (segIdx !=
										    numPR) {
											excthrGc.prType =
												CConst::strPRTypeList
													->type_to_str(
														uls->getPR(segIdx)
															.type);
											excthrGc.prEffectiveGain =
												uls->getPR(segIdx)
													.effectiveGain;
											excthrGc.prDiscrinminationGain =
												discriminationGain;
										}

										excthrGc.fsGainToRlan =
											rxGainDB;
										if (!std::isnan(
											    nearField_xdb)) {
											excthrGc.fsNearFieldXdb =
												nearField_xdb;
										}
										if (!std::isnan(
											    nearField_u)) {
											excthrGc.fsNearFieldU =
												nearField_u;
										}
										if (!std::isnan(
											    nearField_eff)) {
											excthrGc.fsNearFieldEff =
												nearField_eff;
										}
										excthrGc.fsNearFieldOffset =
											nearFieldOffsetDB;
										excthrGc.spectralOverlapLoss =
											spectralOverlapLossDB;
										excthrGc.polarizationLoss =
											_polarizationLossDB;
										excthrGc.fsRxFeederLoss =
											uls->getRxAntennaFeederLossDB();
										excthrGc.fsRxPwr =
											rxPowerDBW;
										excthrGc.fsIN =
											I2NDB;
										// excthrGc.eirpLimit
										// = eirpLimit_dBm;
										excthrGc.fsSegDist =
											ulsSegmentDistance;
										excthrGc.rlanCenterFreq =
											chanCenterFreq;
										excthrGc.fsTxToRlanDist =
											d2;
										excthrGc.pathDifference =
											pathDifference;
										excthrGc.ulsWavelength =
											ulsWavelength *
											1000;
										excthrGc.fresnelIndex =
											fresnelIndex;

										excthrGc.completeRow();
									}
								}
							}

							if (uls->ITMHeightProfile) {
								free(uls->ITMHeightProfile);
								uls->ITMHeightProfile = (double *)
									NULL;
							}
							if (uls->isLOSHeightProfile) {
								free(uls->isLOSHeightProfile);
								uls->isLOSHeightProfile = (double *)
									NULL;
							}
						}
					}
				}
			}
			_heatmapIToNDB[lonIdx][latIdx] = maxIToNDB;

			if (maxIToNDB == -std::numeric_limits<double>::infinity()) {
				numInvalid++;
				if (numInvalid <= 100) {
					errStr << "At position LON = " << rlanLon
					       << " LAT = " << rlanLat
					       << " there are no FS receivers within "
					       << (_maxRadius / 1000)
					       << " Km of RLAN that have spectral overlap with "
						  "RLAN";
					LOGGER_INFO(logger) << errStr.str();
					errStr.str("");
					errStr.clear();
				}
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

#if DEBUG_AFC
			auto t2 = std::chrono::high_resolution_clock::now();

			std::cout << " [" << numProc << " / " << totNumProc << "] "
				  << " Elapsed Time = " << std::setprecision(6)
				  << std::chrono::duration_cast<std::chrono::duration<double>>(t2 -
											       t1)
					     .count()
				  << std::endl
				  << std::flush;

#endif
		}
	}

	if (numInvalid) {
		errStr << "There were a total of " << numInvalid
		       << " RLAN locations for which there are no FS receivers within "
		       << (_maxRadius / 1000) << " Km that have nonzero spectral overlap"
		       << std::endl;
		LOGGER_WARN(logger) << errStr.str();
		statusMessageList.push_back(errStr.str());
		errStr.str("");
		errStr.clear();
	}

#if DEBUG_AFC
	time_t tEndHeatmap = time(NULL);
	tstr = strdup(ctime(&tEndHeatmap));
	strtok(tstr, "\n");

	int elapsedTime = (int)(tEndHeatmap - tStartHeatmap);

	int et = elapsedTime;
	int elapsedTimeSec = et % 60;
	et = et / 60;
	int elapsedTimeMin = et % 60;
	et = et / 60;
	int elapsedTimeHour = et % 24;
	et = et / 24;
	int elapsedTimeDay = et;

	LOGGER_INFO(logger) << "End Processing Heatmap " << tstr
			    << " Elapsed time = " << (tEndHeatmap - tStartHeatmap)
			    << " sec = " << elapsedTimeDay << " days " << elapsedTimeHour
			    << " hours " << elapsedTimeMin << " min " << elapsedTimeSec << " sec.";

	free(tstr);
#endif
	/**************************************************************************************/

	/**************************************************************************************/
	/* Open KML File and write header                                                     */
	/**************************************************************************************/
	FILE *fkml = (FILE *)NULL;
	if (!(fkml = fopen("/tmp/doc.kml", "wb"))) {
		throw std::runtime_error("ERROR");
	}

	if (fkml) {
		fprintf(fkml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		fprintf(fkml, "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n");
		fprintf(fkml, "<Document>\n");
		fprintf(fkml, "<name>AFC Heatmap</name>\n");
		fprintf(fkml, "<open>1</open>\n");
		fprintf(fkml, "<description>Display Heatmap Analysis Results</description>\n");
		fprintf(fkml, "\n");
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* KML Header                                                                         */
	/**************************************************************************************/
	if (fkml) {
		fprintf(fkml, "        <Style id=\"transGrayPoly\">\n");
		fprintf(fkml, "            <LineStyle>\n");
		fprintf(fkml, "                <width>1.5</width>\n");
		fprintf(fkml, "            </LineStyle>\n");
		fprintf(fkml, "            <PolyStyle>\n");
		fprintf(fkml, "                <color>7d7f7f7f</color>\n");
		fprintf(fkml, "            </PolyStyle>\n");
		fprintf(fkml, "        </Style>\n");

		fprintf(fkml, "        <Style id=\"transBluePoly\">\n");
		fprintf(fkml, "            <LineStyle>\n");
		fprintf(fkml, "                <width>1.5</width>\n");
		fprintf(fkml, "            </LineStyle>\n");
		fprintf(fkml, "            <PolyStyle>\n");
		fprintf(fkml, "                <color>7dff0000</color>\n");
		fprintf(fkml, "            </PolyStyle>\n");
		fprintf(fkml, "        </Style>\n");

		fprintf(fkml, "        <Style id=\"redPoly\">\n");
		fprintf(fkml, "            <LineStyle>\n");
		fprintf(fkml, "                <color>ff0000ff</color>\n");
		fprintf(fkml, "                <width>1.5</width>\n");
		fprintf(fkml, "            </LineStyle>\n");
		fprintf(fkml, "            <PolyStyle>\n");
		fprintf(fkml, "                <color>7d0000ff</color>\n");
		fprintf(fkml, "            </PolyStyle>\n");
		fprintf(fkml, "        </Style>\n");

		fprintf(fkml, "        <Style id=\"yellowPoly\">\n");
		fprintf(fkml, "            <LineStyle>\n");
		fprintf(fkml, "                <color>ff00ffff</color>\n");
		fprintf(fkml, "                <width>1.5</width>\n");
		fprintf(fkml, "            </LineStyle>\n");
		fprintf(fkml, "            <PolyStyle>\n");
		fprintf(fkml, "                <color>7d00ffff</color>\n");
		fprintf(fkml, "            </PolyStyle>\n");
		fprintf(fkml, "        </Style>\n");

		fprintf(fkml, "        <Style id=\"greenPoly\">\n");
		fprintf(fkml, "            <LineStyle>\n");
		fprintf(fkml, "                <color>ff00ff00</color>\n");
		fprintf(fkml, "                <width>1.5</width>\n");
		fprintf(fkml, "            </LineStyle>\n");
		fprintf(fkml, "            <PolyStyle>\n");
		fprintf(fkml, "                <color>7d00ff00</color>\n");
		fprintf(fkml, "            </PolyStyle>\n");
		fprintf(fkml, "        </Style>\n");

		fprintf(fkml, "        <Style id=\"bluePoly\">\n");
		fprintf(fkml, "            <LineStyle>\n");
		fprintf(fkml, "                <width>1.5</width>\n");
		fprintf(fkml, "            </LineStyle>\n");
		fprintf(fkml, "            <PolyStyle>\n");
		fprintf(fkml, "                <color>ffff0000</color>\n");
		fprintf(fkml, "            </PolyStyle>\n");
		fprintf(fkml, "        </Style>\n");

		fprintf(fkml, "        <Style id=\"blackPoly\">\n");
		fprintf(fkml, "            <LineStyle>\n");
		fprintf(fkml, "                <color>ff000000</color>\n");
		fprintf(fkml, "                <width>1.5</width>\n");
		fprintf(fkml, "            </LineStyle>\n");
		fprintf(fkml, "            <PolyStyle>\n");
		fprintf(fkml, "                <color>7d000000</color>\n");
		fprintf(fkml, "            </PolyStyle>\n");
		fprintf(fkml, "        </Style>\n");

		fprintf(fkml, "        <Style id=\"redPlacemark\">\n");
		fprintf(fkml, "            <IconStyle>\n");
		fprintf(fkml, "                <color>ff0000ff</color>\n");
		fprintf(fkml, "                <Icon>\n");
		fprintf(fkml,
			"                    "
			"<href>http://maps.google.com/mapfiles/kml/pushpin/ylw-pushpin.png</"
			"href>\n");
		fprintf(fkml, "                </Icon>\n");
		fprintf(fkml, "            </IconStyle>\n");
		fprintf(fkml, "        </Style>\n");

		fprintf(fkml, "        <Style id=\"yellowPlacemark\">\n");
		fprintf(fkml, "            <IconStyle>\n");
		fprintf(fkml, "                <color>ff00ffff</color>\n");
		fprintf(fkml, "                <Icon>\n");
		fprintf(fkml,
			"                    "
			"<href>http://maps.google.com/mapfiles/kml/pushpin/ylw-pushpin.png</"
			"href>\n");
		fprintf(fkml, "                </Icon>\n");
		fprintf(fkml, "            </IconStyle>\n");
		fprintf(fkml, "        </Style>\n");

		fprintf(fkml, "        <Style id=\"greenPlacemark\">\n");
		fprintf(fkml, "            <IconStyle>\n");
		fprintf(fkml, "                <color>ff00ff00</color>\n");
		fprintf(fkml, "                <Icon>\n");
		fprintf(fkml,
			"                    "
			"<href>http://maps.google.com/mapfiles/kml/pushpin/ylw-pushpin.png</"
			"href>\n");
		fprintf(fkml, "                </Icon>\n");
		fprintf(fkml, "            </IconStyle>\n");
		fprintf(fkml, "        </Style>\n");

		fprintf(fkml, "        <Style id=\"blackPlacemark\">\n");
		fprintf(fkml, "            <IconStyle>\n");
		fprintf(fkml, "                <color>ff000000</color>\n");
		fprintf(fkml, "                <Icon>\n");
		fprintf(fkml,
			"                    "
			"<href>http://maps.google.com/mapfiles/kml/pushpin/ylw-pushpin.png</"
			"href>\n");
		fprintf(fkml, "                </Icon>\n");
		fprintf(fkml, "            </IconStyle>\n");
		fprintf(fkml, "        </Style>\n");
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* KML Show FS                                                                        */
	/**************************************************************************************/
	if (fkml) {
		fprintf(fkml, "        <Folder>\n");
		fprintf(fkml, "            <name>FS</name>\n");
		for (int uIdx = 0; uIdx < (int)_ulsIdxList.size(); uIdx++) {
			ulsIdx = _ulsIdxList[uIdx];
			ULSClass *uls = (*_ulsList)[ulsIdx];
			std::string dbName = std::get<0>(_ulsDatabaseList[uls->getDBIdx()]);
			int addPlacemarks = 1;
			std::string placemarkStyleStr = "#yellowPlacemark";
			std::string polyStyleStr = "#yellowPoly";
			std::string visibilityStr = "1";

			fprintf(fkml, "        <Folder>\n");
			fprintf(fkml,
				"            <name>%s_%d</name>\n",
				dbName.c_str(),
				uls->getID());

			int numPR = uls->getNumPR();
			for (int segIdx = 0; segIdx < numPR + 1; ++segIdx) {
				Vector3 ulsTxPosn = (segIdx == 0 ?
							     uls->getTxPosition() :
							     uls->getPR(segIdx - 1).positionTx);
				double ulsTxLongitude =
					(segIdx == 0 ? uls->getTxLongitudeDeg() :
						       uls->getPR(segIdx - 1).longitudeDeg);
				double ulsTxLatitude = (segIdx == 0 ?
								uls->getTxLatitudeDeg() :
								uls->getPR(segIdx - 1).latitudeDeg);
				double ulsTxHeight = (segIdx == 0 ?
							      uls->getTxHeightAMSL() :
							      uls->getPR(segIdx - 1).heightAMSLTx);

				Vector3 ulsRxPosn = (segIdx == numPR ?
							     uls->getRxPosition() :
							     uls->getPR(segIdx).positionRx);
				double ulsRxLongitude = (segIdx == numPR ?
								 uls->getRxLongitudeDeg() :
								 uls->getPR(segIdx).longitudeDeg);
				double ulsRxLatitude = (segIdx == numPR ?
								uls->getRxLatitudeDeg() :
								uls->getPR(segIdx).latitudeDeg);
				double ulsRxHeight = (segIdx == numPR ?
							      uls->getRxHeightAMSL() :
							      uls->getPR(segIdx).heightAMSLRx);

				bool txLocFlag = (!std::isnan(ulsTxPosn.x())) &&
						 (!std::isnan(ulsTxPosn.y())) &&
						 (!std::isnan(ulsTxPosn.z()));

				double linkDistKm;
				if (!txLocFlag) {
					linkDistKm = 1.0;
					Vector3 segPointing = (segIdx == numPR ?
								       uls->getAntennaPointing() :
								       uls->getPR(segIdx).pointing);
					ulsTxPosn = ulsRxPosn + linkDistKm * segPointing;
				} else {
					linkDistKm = (ulsTxPosn - ulsRxPosn).len();
				}

				if ((segIdx == 0) && (addPlacemarks) && (txLocFlag)) {
					fprintf(fkml, "            <Placemark>\n");
					fprintf(fkml,
						"                <name>%s %s_%d</name>\n",
						"TX",
						dbName.c_str(),
						uls->getID());
					fprintf(fkml,
						"                <visibility>1</visibility>\n");
					fprintf(fkml,
						"                <styleUrl>%s</styleUrl>\n",
						placemarkStyleStr.c_str());
					fprintf(fkml, "                <Point>\n");
					fprintf(fkml,
						"                    "
						"<altitudeMode>absolute</altitudeMode>\n");
					fprintf(fkml,
						"                    "
						"<coordinates>%.10f,%.10f,%.2f</coordinates>\n",
						ulsTxLongitude,
						ulsTxLatitude,
						ulsTxHeight);
					fprintf(fkml, "                </Point>\n");
					fprintf(fkml, "            </Placemark>\n");
				}

				double beamWidthDeg = uls->computeBeamWidth(3.0);
				double beamWidthRad = beamWidthDeg * (M_PI / 180.0);

				Vector3 zvec = (ulsTxPosn - ulsRxPosn).normalized();
				Vector3 xvec = (Vector3(zvec.y(), -zvec.x(), 0.0)).normalized();
				Vector3 yvec = zvec.cross(xvec);

				int numCvgPoints = 32;

				std::vector<GeodeticCoord> ptList;
				double cvgTheta = beamWidthRad;
				int cvgPhiIdx;
				for (cvgPhiIdx = 0; cvgPhiIdx < numCvgPoints; ++cvgPhiIdx) {
					double cvgPhi = 2 * M_PI * cvgPhiIdx / numCvgPoints;
					Vector3 cvgIntPosn = ulsRxPosn +
							     linkDistKm * (zvec * cos(cvgTheta) +
									   (xvec * cos(cvgPhi) +
									    yvec * sin(cvgPhi)) *
										   sin(cvgTheta));

					GeodeticCoord cvgIntPosnGeodetic =
						EcefModel::ecefToGeodetic(cvgIntPosn);
					ptList.push_back(cvgIntPosnGeodetic);
				}
				if (addPlacemarks) {
					std::string nameStr;
					if (segIdx == numPR) {
						nameStr = "RX";
					} else {
						nameStr = "PR " + std::to_string(segIdx + 1);
						;
					}
					fprintf(fkml, "            <Placemark>\n");
					fprintf(fkml,
						"                <name>%s %s_%d</name>\n",
						nameStr.c_str(),
						dbName.c_str(),
						uls->getID());
					fprintf(fkml,
						"                <visibility>1</visibility>\n");
					fprintf(fkml,
						"                <styleUrl>%s</styleUrl>\n",
						placemarkStyleStr.c_str());
					fprintf(fkml, "                <Point>\n");
					fprintf(fkml,
						"                    "
						"<altitudeMode>absolute</altitudeMode>\n");
					fprintf(fkml,
						"                    "
						"<coordinates>%.10f,%.10f,%.2f</coordinates>\n",
						ulsRxLongitude,
						ulsRxLatitude,
						ulsRxHeight);
					fprintf(fkml, "                </Point>\n");
					fprintf(fkml, "            </Placemark>\n");
				}

				if (true) {
					fprintf(fkml, "            <Folder>\n");
					fprintf(fkml,
						"            <name>Beamcone_%d</name>/n",
						segIdx + 1);

					for (cvgPhiIdx = 0; cvgPhiIdx < numCvgPoints; ++cvgPhiIdx) {
						fprintf(fkml, "            <Placemark>\n");
						fprintf(fkml,
							"                <name>p%d</name>\n",
							cvgPhiIdx);
						fprintf(fkml,
							"                "
							"<visibility>%s</visibility>\n",
							visibilityStr.c_str());
						fprintf(fkml,
							"                <styleUrl>%s</styleUrl>\n",
							placemarkStyleStr.c_str());
						fprintf(fkml, "                <Polygon>\n");
						fprintf(fkml,
							"                    "
							"<extrude>0</extrude>\n");
						fprintf(fkml,
							"                    "
							"<altitudeMode>absolute</altitudeMode>\n");
						fprintf(fkml,
							"                    <outerBoundaryIs>\n");
						fprintf(fkml,
							"                        <LinearRing>\n");
						fprintf(fkml,
							"                            "
							"<coordinates>\n");

						fprintf(fkml,
							"%.10f,%.10f,%.2f\n",
							ulsRxLongitude,
							ulsRxLatitude,
							ulsRxHeight);

						GeodeticCoord pt = ptList[cvgPhiIdx];
						fprintf(fkml,
							"%.10f,%.10f,%.2f\n",
							pt.longitudeDeg,
							pt.latitudeDeg,
							pt.heightKm * 1000.0);

						pt = ptList[(cvgPhiIdx + 1) % numCvgPoints];
						fprintf(fkml,
							"%.10f,%.10f,%.2f\n",
							pt.longitudeDeg,
							pt.latitudeDeg,
							pt.heightKm * 1000.0);

						fprintf(fkml,
							"%.10f,%.10f,%.2f\n",
							ulsRxLongitude,
							ulsRxLatitude,
							ulsRxHeight);

						fprintf(fkml,
							"                            "
							"</coordinates>\n");
						fprintf(fkml,
							"                        </LinearRing>\n");
						fprintf(fkml,
							"                    </outerBoundaryIs>\n");
						fprintf(fkml, "                </Polygon>\n");
						fprintf(fkml, "            </Placemark>\n");
					}
					fprintf(fkml, "            </Folder>\n");
				}
			}
			fprintf(fkml, "        </Folder>\n");
		}
		fprintf(fkml, "        </Folder>\n");
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* KML Show Denied Regions                                                            */
	/**************************************************************************************/
	if (fkml) {
		fprintf(fkml, "        <Folder>\n");
		fprintf(fkml, "            <name>Denied Region</name>\n");

		int drIdx;
		for (drIdx = 0; drIdx < (int)_deniedRegionList.size(); ++drIdx) {
			DeniedRegionClass *dr = _deniedRegionList[drIdx];
			DeniedRegionClass::TypeEnum drType = dr->getType();
			std::string pfx;
			switch (drType) {
				case DeniedRegionClass::RASType:
					pfx = "RAS_";
					break;
				case DeniedRegionClass::userSpecifiedType:
					pfx = "USER_SPEC_";
					break;
				default:
					CORE_DUMP;
					break;
			}

			fprintf(fkml, "            <Folder>\n");
			fprintf(fkml,
				"                <name>%s_%d</name>\n",
				pfx.c_str(),
				dr->getID());

			int numPtsCircle = 32;
			int rectIdx, numRect;
			double rectLonStart, rectLonStop, rectLatStart, rectLatStop;
			double circleRadius, longitudeCenter, latitudeCenter;
			double drTerrainHeight, drBldgHeight, drHeightAGL;
			Vector3 drCenterPosn;
			Vector3 drUpVec;
			Vector3 drEastVec;
			Vector3 drNorthVec;
			QString dr_coords;
			MultibandRasterClass::HeightResult drLidarHeightResult;
			CConst::HeightSourceEnum drHeightSource;
			DeniedRegionClass::GeometryEnum drGeometry = dr->getGeometry();
			switch (drGeometry) {
				case DeniedRegionClass::rectGeometry:
				case DeniedRegionClass::rect2Geometry:
					numRect = ((RectDeniedRegionClass *)dr)->getNumRect();
					for (rectIdx = 0; rectIdx < numRect; rectIdx++) {
						std::tie(rectLonStart,
							 rectLonStop,
							 rectLatStart,
							 rectLatStop) =
							((RectDeniedRegionClass *)dr)
								->getRect(rectIdx);

						fprintf(fkml, "                <Placemark>\n");
						fprintf(fkml,
							"                    "
							"<name>RECT_%d</name>\n",
							rectIdx);
						fprintf(fkml,
							"                    "
							"<visibility>1</visibility>\n");
						fprintf(fkml,
							"                    "
							"<styleUrl>#transBluePoly</styleUrl>\n");
						fprintf(fkml, "                    <Polygon>\n");
						fprintf(fkml,
							"                        "
							"<extrude>0</extrude>\n");
						fprintf(fkml,
							"                        "
							"<tessellate>0</tessellate>\n");
						fprintf(fkml,
							"                        "
							"<altitudeMode>clampToGround</"
							"altitudeMode>\n");
						fprintf(fkml,
							"                        "
							"<outerBoundaryIs>\n");
						fprintf(fkml,
							"                            "
							"<LinearRing>\n");
						fprintf(fkml,
							"                                "
							"<coordinates>\n");

						fprintf(fkml,
							"%.10f,%.10f,%.2f\n",
							rectLonStart,
							rectLatStart,
							0.0);
						fprintf(fkml,
							"%.10f,%.10f,%.2f\n",
							rectLonStop,
							rectLatStart,
							0.0);
						fprintf(fkml,
							"%.10f,%.10f,%.2f\n",
							rectLonStop,
							rectLatStop,
							0.0);
						fprintf(fkml,
							"%.10f,%.10f,%.2f\n",
							rectLonStart,
							rectLatStop,
							0.0);
						fprintf(fkml,
							"%.10f,%.10f,%.2f\n",
							rectLonStart,
							rectLatStart,
							0.0);

						fprintf(fkml,
							"                                "
							"</coordinates>\n");
						fprintf(fkml,
							"                            "
							"</LinearRing>\n");
						fprintf(fkml,
							"                        "
							"</outerBoundaryIs>\n");
						fprintf(fkml, "                    </Polygon>\n");
						fprintf(fkml, "                </Placemark>\n");
					}
					break;
				case DeniedRegionClass::circleGeometry:
				case DeniedRegionClass::horizonDistGeometry:
					circleRadius = ((CircleDeniedRegionClass *)dr)
							       ->computeRadius(
								       _heatmapMaxRLANHeightAGL);
					longitudeCenter = ((CircleDeniedRegionClass *)dr)
								  ->getLongitudeCenter();
					latitudeCenter = ((CircleDeniedRegionClass *)dr)
								 ->getLatitudeCenter();
					drHeightAGL = dr->getHeightAGL();
					_terrainDataModel->getTerrainHeight(longitudeCenter,
									    latitudeCenter,
									    drTerrainHeight,
									    drBldgHeight,
									    drLidarHeightResult,
									    drHeightSource);

					drCenterPosn = EcefModel::geodeticToEcef(
						latitudeCenter,
						longitudeCenter,
						(drTerrainHeight + drHeightAGL) / 1000.0);
					drUpVec = drCenterPosn.normalized();
					drEastVec = (Vector3(-drUpVec.y(), drUpVec.x(), 0.0))
							    .normalized();
					drNorthVec = drUpVec.cross(drEastVec);

					fprintf(fkml, "                <Placemark>\n");
					fprintf(fkml, "                    <name>CIRCLE</name>\n");
					fprintf(fkml,
						"                    <visibility>1</visibility>\n");
					fprintf(fkml,
						"                    "
						"<styleUrl>#transBluePoly</styleUrl>\n");
					fprintf(fkml, "                    <Polygon>\n");
					fprintf(fkml,
						"                        <extrude>0</extrude>\n");
					fprintf(fkml,
						"                        "
						"<tessellate>0</tessellate>\n");
					fprintf(fkml,
						"                        "
						"<altitudeMode>clampToGround</altitudeMode>\n");
					fprintf(fkml,
						"                        <outerBoundaryIs>\n");
					fprintf(fkml, "                            <LinearRing>\n");
					fprintf(fkml,
						"                                <coordinates>\n");

					for (int ptIdx = 0; ptIdx <= numPtsCircle; ++ptIdx) {
						double phi = 2 * M_PI * ptIdx / numPtsCircle;
						Vector3 circlePtPosn = drCenterPosn +
								       (circleRadius / 1000) *
									       (drEastVec *
											cos(phi) +
										drNorthVec *
											sin(phi));

						GeodeticCoord circlePtPosnGeodetic =
							EcefModel::ecefToGeodetic(circlePtPosn);

						fprintf(fkml,
							"%.10f,%.10f,%.2f\n",
							circlePtPosnGeodetic.longitudeDeg,
							circlePtPosnGeodetic.latitudeDeg,
							0.0);
					}

					fprintf(fkml,
						"                                </coordinates>\n");
					fprintf(fkml,
						"                            </LinearRing>\n");
					fprintf(fkml,
						"                        </outerBoundaryIs>\n");
					fprintf(fkml, "                    </Polygon>\n");
					fprintf(fkml, "                </Placemark>\n");

					break;
				default:
					CORE_DUMP;
					break;
			}
			fprintf(fkml, "            </Folder>\n");
		}
		fprintf(fkml, "        </Folder>\n");
	}
	/**************************************************************************************/

	int lonRegionIdx;
	int latRegionIdx;
	int interp = 9; // must be odd
	int numRegionLon = (int)floor(((double)_heatmapNumPtsLon) * interp / 500.0) + 1;
	int numRegionLat = (int)floor(((double)_heatmapNumPtsLat) * interp / 500.0) + 1;

	/**************************************************************************************/
	/* KML Heatmap                                                                        */
	/**************************************************************************************/
	if (fkml) {
		int startLonIdx, stopLonIdx;
		int startLatIdx, stopLatIdx;
		int lonN = (_heatmapNumPtsLon - 1) / numRegionLon;
		int lonq = (_heatmapNumPtsLon - 1) % numRegionLon;
		int latN = (_heatmapNumPtsLat - 1) / numRegionLat;
		int latq = (_heatmapNumPtsLat - 1) % numRegionLat;

		fprintf(fkml, "        <Folder>\n");
		fprintf(fkml, "            <name>Heatmap</name>\n");

		for (lonRegionIdx = 0; lonRegionIdx < numRegionLon; lonRegionIdx++) {
			if (lonRegionIdx < lonq) {
				startLonIdx = (lonN + 1) * lonRegionIdx;
				stopLonIdx = (lonN + 1) * (lonRegionIdx + 1);
			} else {
				startLonIdx = lonN * lonRegionIdx + lonq;
				stopLonIdx = lonN * (lonRegionIdx + 1) + lonq;
			}

			for (latRegionIdx = 0; latRegionIdx < numRegionLat; latRegionIdx++) {
				if (latRegionIdx < latq) {
					startLatIdx = (latN + 1) * latRegionIdx;
					stopLatIdx = (latN + 1) * (latRegionIdx + 1);
				} else {
					startLatIdx = latN * latRegionIdx + latq;
					stopLatIdx = latN * (latRegionIdx + 1) + latq;
				}

				/**************************************************************************************/
				/* Create PPM File */
				/**************************************************************************************/
				FILE *fppm;
				if (!(fppm = fopen("/tmp/image.ppm", "wb"))) {
					throw std::runtime_error("ERROR");
				}
				fprintf(fppm, "P3\n");
				fprintf(fppm,
					"%d %d %d\n",
					(stopLonIdx - startLonIdx + 1),
					(stopLatIdx - startLatIdx + 1),
					255);

				for (latIdx = stopLatIdx; latIdx >= startLatIdx; --latIdx) {
					for (lonIdx = startLonIdx; lonIdx <= stopLonIdx; ++lonIdx) {
						if (lonIdx) {
							fprintf(fppm, " ");
						}
						fprintf(fppm,
							"%s",
							getHeatmapColor(
								_heatmapIToNDB[lonIdx][latIdx],
								_heatmapIsIndoor[lonIdx][latIdx],
								false)
								.c_str());
					}
					fprintf(fppm, "\n");
				}

				fclose(fppm);
				/**************************************************************************************/

				std::string pngFile = "/tmp/image_" + std::to_string(lonRegionIdx) +
						      "_" + std::to_string(latRegionIdx) + ".png";

				std::string command = "convert /tmp/image.ppm " + pngFile;
				std::cout << "COMMAND: " << command << std::endl;
				system(command.c_str());

				/**************************************************************************************/
				/* Write to KML File */
				/**************************************************************************************/
				fprintf(fkml, "<GroundOverlay>\n");
				fprintf(fkml,
					"    <name>Region: %d_%d</name>\n",
					lonRegionIdx,
					latRegionIdx);
				fprintf(fkml, "    <visibility>%d</visibility>\n", 1);
				fprintf(fkml, "    <color>80ffffff</color>\n");
				fprintf(fkml, "    <Icon>\n");
				fprintf(fkml,
					"        <href>image_%d_%d.png</href>\n",
					lonRegionIdx,
					latRegionIdx);
				fprintf(fkml, "    </Icon>\n");
				fprintf(fkml, "    <LatLonBox>\n");
				fprintf(fkml,
					"        <north>%.8f</north>\n",
					(_heatmapMinLat * (_heatmapNumPtsLat - 1 - stopLatIdx) +
					 _heatmapMaxLat * (stopLatIdx + 1)) /
						_heatmapNumPtsLat);
				fprintf(fkml,
					"        <south>%.8f</south>\n",
					(_heatmapMinLat * (_heatmapNumPtsLat - startLatIdx) +
					 _heatmapMaxLat * startLatIdx) /
						_heatmapNumPtsLat);
				fprintf(fkml,
					"        <east>%.8f</east>\n",
					(_heatmapMinLon * (_heatmapNumPtsLon - 1 - stopLonIdx) +
					 _heatmapMaxLon * (stopLonIdx + 1)) /
						_heatmapNumPtsLon);
				fprintf(fkml,
					"        <west>%.8f</west>\n",
					(_heatmapMinLon * (_heatmapNumPtsLon - startLonIdx) +
					 _heatmapMaxLon * startLonIdx) /
						_heatmapNumPtsLon);
				fprintf(fkml, "    </LatLonBox>\n");
				fprintf(fkml, "</GroundOverlay>\n");
				/**************************************************************************************/
			}
		}
		fprintf(fkml, "        </Folder>\n");
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Close KML File                                                                     */
	/**************************************************************************************/
	if (fkml) {
		fprintf(fkml, "</Document>\n");
		fprintf(fkml, "</kml>\n");

		fclose(fkml);
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Zip files into output KMZ file                                                     */
	/**************************************************************************************/
	if (fkml) {
		std::string command = "zip -j " + _kmlFile + " /tmp/doc.kml";
		for (lonRegionIdx = 0; lonRegionIdx < numRegionLon; lonRegionIdx++) {
			for (latRegionIdx = 0; latRegionIdx < numRegionLat; latRegionIdx++) {
				command += " /tmp/image_" + std::to_string(lonRegionIdx) + "_" +
					   std::to_string(latRegionIdx) + ".png";
			}
		}
		// std::cout << "COMMAND: " << command.c_str() << std::endl;
		system(command.c_str());
	}
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
	GzipCsv inputGc(_userInputsFile);

	double lat, lon, alt, minor, major, height_uncert;
	std::tie(lat, lon, alt) = _rlanLLA;
	std::tie(minor, major, height_uncert) = _rlanUncerts_m;

	auto f2s = [](double f) {
		return boost::lexical_cast<std::string>(f);
	};

	if (inputGc) {
		inputGc.writeRow({"ANALYSIS_TYPE", _analysisType});
		inputGc.writeRow({"SERIAL_NUMBER", _serialNumber.toStdString()});
		inputGc.writeRow({"LATITUDE (DEG)", f2s(lat)});
		inputGc.writeRow({"LONGITUDE (DEG)", f2s(lon)});
		inputGc.writeRow({"ANTENNA_HEIGHT (M)", f2s(alt)});
		inputGc.writeRow({"SEMI-MAJOR_AXIS (M)", f2s(major)});
		inputGc.writeRow({"SEMI-MINOR_AXIS (M)", f2s(minor)});
		inputGc.writeRow({"HEIGHT_UNCERTAINTY (M)", f2s(height_uncert)});
		inputGc.writeRow({"ORIENTATION (DEG)", f2s(_rlanOrientation_deg)});
		inputGc.writeRow({"HEIGHT_TYPE",
				  (_rlanHeightType == CConst::AMSLHeightType ? "AMSL" :
				   _rlanHeightType == CConst::AGLHeightType  ? "AGL" :
									       "INVALID")});
		inputGc.writeRow({"ALLOW_SCAN_PTS_IN_UNC_REG",
				  (_allowScanPtsInUncRegFlag ? "true" : "false")});
		inputGc.writeRow({"INDOOR/OUTDOOR",
				  (_rlanType == RLAN_INDOOR  ? "indoor" :
				   _rlanType == RLAN_OUTDOOR ? "outdoor" :
							       "error")});
		inputGc.writeRow({"CERT_INDOOR", (_certifiedIndoor ? "True" : "False")});
		inputGc.writeRow(
			{"CHANNEL_RESPONSE_ALGORITHM",
			 CConst::strSpectralAlgorithmList->type_to_str(_channelResponseAlgorithm)});

		// inputGc.writeRow({ "ULS_DATABASE", _inputULSDatabaseStr } );
		inputGc.writeRow({"AP/CLIENT_PROPAGATION_ENVIRO",
				  CConst::strPropEnvMethodList->type_to_str(_propEnvMethod)});
		inputGc.writeRow({"AP/CLIENT_MIN_EIRP_INDOOR (DBM)", f2s(_minEIRPIndoor_dBm)});
		inputGc.writeRow({"AP/CLIENT_MIN_EIRP_OUTDOOR (DBM)", f2s(_minEIRPOutdoor_dBm)});
		inputGc.writeRow({"AP/CLIENT_MAX_EIRP (DBM)", f2s(_maxEIRP_dBm)});

		inputGc.writeRow(
			{"BUILDING_PENETRATION_LOSS_MODEL", _buildingLossModel.toStdString()});
		inputGc.writeRow(
			{"BUILDING_TYPE",
			 (_buildingType == CConst::traditionalBuildingType ? "traditional" :
			  _buildingType == CConst::thermallyEfficientBuildingType ?
									     "thermally efficient" :
									     "no building type")});
		inputGc.writeRow({"BUILDING_PENETRATION_CONFIDENCE", f2s(_confidenceBldg2109)});
		inputGc.writeRow({"BUILDING_PENETRATION_LOSS_FIXED_VALUE (DB)",
				  f2s(_fixedBuildingLossValue)});
		inputGc.writeRow({"POLARIZATION_LOSS (DB)", f2s(_polarizationLossDB)});
		inputGc.writeRow({"RLAN_BODY_LOSS_INDOOR (DB)", f2s(_bodyLossIndoorDB)});
		inputGc.writeRow({"RLAN_BODY_LOSS_OUTDOOR (DB)", f2s(_bodyLossOutdoorDB)});
		inputGc.writeRow({"I/N_THRESHOLD", f2s(_IoverN_threshold_dB)});
		inputGc.writeRow(
			{"FS_RECEIVER_DEFAULT_ANTENNA",
			 CConst::strULSAntennaTypeList->type_to_str(_ulsDefaultAntennaType)});
		inputGc.writeRow(
			{"RLAN_ITM_TX_CLUTTER_METHOD",
			 CConst::strITMClutterMethodList->type_to_str(_rlanITMTxClutterMethod)});

		inputGc.writeRow({"PROPAGATION_MODEL",
				  CConst::strPathLossModelList->type_to_str(_pathLossModel)});
		inputGc.writeRow({"WINNER_II_PROB_LOS_THRESHOLD", f2s(_winner2ProbLOSThr)});
		inputGc.writeRow({"WINNER_II_LOS_CONFIDENCE", f2s(_confidenceWinner2LOS)});
		inputGc.writeRow({"WINNER_II_NLOS_CONFIDENCE", f2s(_confidenceWinner2NLOS)});
		inputGc.writeRow(
			{"WINNER_II_COMBINED_CONFIDENCE", f2s(_confidenceWinner2Combined)});
		inputGc.writeRow({"WINNER_II_HGT_FLAG", (_closeInHgtFlag ? "true" : "false")});
		inputGc.writeRow({"WINNER_II_HGT_LOS", f2s(_closeInHgtLOS)});
		inputGc.writeRow({"ITM_CONFIDENCE", f2s(_confidenceITM)});
		inputGc.writeRow({"ITM_RELIABILITY", f2s(_reliabilityITM)});
		inputGc.writeRow({"P.2108_CONFIDENCE", f2s(_confidenceClutter2108)});
		inputGc.writeRow({"WINNER_II_USE_GROUND_DISTANCE",
				  (_winner2UseGroundDistanceFlag ? "true" : "false")});
		inputGc.writeRow({"FSPL_USE_GROUND_DISTANCE",
				  (_fsplUseGroundDistanceFlag ? "true" : "false")});
		inputGc.writeRow(
			{"PASSIVE_REPEATER_FLAG", (_passiveRepeaterFlag ? "true" : "false")});
		inputGc.writeRow({"RX ANTENNA FEEDER LOSS IDU (DB)", f2s(_rxFeederLossDBIDU)});
		inputGc.writeRow({"RX ANTENNA FEEDER LOSS ODU (DB)", f2s(_rxFeederLossDBODU)});
		inputGc.writeRow(
			{"RX ANTENNA FEEDER LOSS UNKNOWN (DB)", f2s(_rxFeederLossDBUnknown)});
		inputGc.writeRow({"RAIN_FOREST_FILE", _rainForestFile});
		inputGc.writeRow({"SRTM DIRECTORY", _srtmDir});
		inputGc.writeRow({"DEP  DIRECTORY", _depDir});
		inputGc.writeRow({"CDSM  DIRECTORY", _cdsmDir});
		inputGc.writeRow({"CDSM  LOS THREHOLD", f2s(_cdsmLOSThr)});
		if (_analysisType == "ExclusionZoneAnalysis") {
			double chanCenterFreq = _wlanMinFreq + (_exclusionZoneRLANChanIdx + 0.5) *
								       _exclusionZoneRLANBWHz;

			inputGc.writeRow({"EXCLUSION_ZONE_FSID", f2s(_exclusionZoneFSID)});
			inputGc.writeRow(
				{"EXCLUSION_ZONE_RLAN_BW (Hz)", f2s(_exclusionZoneRLANBWHz)});
			inputGc.writeRow(
				{"EXCLUSION_ZONE_RLAN_CENTER_FREQ (Hz)", f2s(chanCenterFreq)});
			inputGc.writeRow(
				{"EXCLUSION_ZONE_RLAN_EIRP (dBm)", f2s(_exclusionZoneRLANEIRPDBm)});

		} else if (_analysisType == "HeatmapAnalysis") {
			if (_inquiredChannels.size() != 1) {
				throw std::runtime_error(ErrStream()
							 << "ERROR: Number of channels = "
							 << _inquiredChannels.size()
							 << " require exactly 1 channel for "
							    "heatmap analysis");
			}
			if (_inquiredChannels[0].second.size() != 1) {
				throw std::runtime_error(ErrStream()
							 << "ERROR: Number of channels = "
							 << _inquiredChannels.size()
							 << " require exactly 1 channel for "
							    "heatmap analysis");
			}
			int operatingClass = _inquiredChannels[0].first;
			int channelCfi = _inquiredChannels[0].second[0];

			inputGc.writeRow({"HEATMAP_CHANNEL_OPERATING_CLASS",
					  std::to_string(operatingClass)});
			inputGc.writeRow({"HEATMAP_CHANNEL_CFI", std::to_string(channelCfi)});
			inputGc.writeRow({"HEATMAP_MIN_LON (DEG)", f2s(_heatmapMinLon)});
			inputGc.writeRow({"HEATMAP_MIN_LAT (DEG)", f2s(_heatmapMaxLon)});
			inputGc.writeRow({"HEATMAP_RLAN_SPACING (m)", f2s(_heatmapRLANSpacing)});
			inputGc.writeRow({"HEATMAP_INDOOR_OUTDOOR_STR", _heatmapIndoorOutdoorStr});

			inputGc.writeRow(
				{"HEATMAP_RLAN_INDOOR_EIRP (dBm)", f2s(_heatmapRLANIndoorEIRPDBm)});
			inputGc.writeRow(
				{"HEATMAP_RLAN_INDOOR_HEIGHT_TYPE", _heatmapRLANIndoorHeightType});
			inputGc.writeRow(
				{"HEATMAP_RLAN_INDOOR_HEIGHT (m)", f2s(_heatmapRLANIndoorHeight)});
			inputGc.writeRow({"HEATMAP_RLAN_INDOOR_HEIGHT_UNCERTAINTY (m)",
					  f2s(_heatmapRLANIndoorHeightUncertainty)});

			inputGc.writeRow({"HEATMAP_RLAN_OUTDOOR_EIRP (dBm)",
					  f2s(_heatmapRLANOutdoorEIRPDBm)});
			inputGc.writeRow({"HEATMAP_RLAN_OUTDOOR_HEIGHT_TYPE",
					  _heatmapRLANOutdoorHeightType});
			inputGc.writeRow({"HEATMAP_RLAN_OUTDOOR_HEIGHT (m)",
					  f2s(_heatmapRLANOutdoorHeight)});
			inputGc.writeRow({"HEATMAP_RLAN_OUTDOOR_HEIGHT_UNCERTAINTY (m)",
					  f2s(_heatmapRLANOutdoorHeightUncertainty)});
		}
		inputGc.writeRow({"REPORT_UNAVAILABLE_SPECTRUM",
				  (std::isnan(_reportUnavailPSDdBmPerMHz) ? "false" : "true")});
		if (!std::isnan(_reportUnavailPSDdBmPerMHz)) {
			inputGc.writeRow({"REPORT_UNAVAIL_PSD_DBM_PER_MHZ",
					  f2s(_reportUnavailPSDdBmPerMHz)});
		}
		inputGc.writeRow({"INQUIRED_FREQUENCY_MAX_PSD_DBM_PER_MHZ",
				  f2s(_inquiredFrequencyMaxPSD_dBmPerMHz)});
		inputGc.writeRow({"VISIBILITY_THRESHOLD", f2s(_visibilityThreshold)});
		inputGc.writeRow(
			{"PRINT_SKIPPED_LINKS_FLAG", (_printSkippedLinksFlag ? "true" : "false")});
		inputGc.writeRow({"ROUND_PSD_EIRP_FLAG", (_roundPSDEIRPFlag ? "true" : "false")});
		inputGc.writeRow({"ACI_FLAG", (_aciFlag ? "true" : "false")});
	}
	LOGGER_DEBUG(logger) << "User inputs written to userInputs.csv";
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::computePropEnv                                                             */
/******************************************************************************************/
CConst::PropEnvEnum AfcManager::computePropEnv(double lonDeg,
					       double latDeg,
					       CConst::NLCDLandCatEnum &nlcdLandCat,
					       bool errorFlag) const
{
	// If user uses a density map versus a constant environmental input
	int lonIdx;
	int latIdx;
	CConst::PropEnvEnum propEnv;
	nlcdLandCat = CConst::unknownNLCDLandCat;
	if (_propEnvMethod == CConst::nlcdPointPropEnvMethod) {
		unsigned int landcat = cgNlcd->valueAt(latDeg, lonDeg);

		switch (landcat) {
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
	} else if (_propEnvMethod == CConst::popDensityMapPropEnvMethod) {
		int regionIdx;
		char propEnvChar;
		_popGrid->findDeg(lonDeg, latDeg, lonIdx, latIdx, propEnvChar, regionIdx);

		switch (propEnvChar) {
			case 'U':
				propEnv = CConst::urbanPropEnv;
				break;
			case 'S':
				propEnv = CConst::suburbanPropEnv;
				break;
			case 'R':
				propEnv = CConst::ruralPropEnv;
				nlcdLandCat = CConst::villageCenterNLCDLandCat;
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

		if (_rainForestPolygon) {
			int xIdx = (int)floor(lonDeg / _regionPolygonResolution + 0.5);
			int yIdx = (int)floor(latDeg / _regionPolygonResolution + 0.5);

			if (_rainForestPolygon->in_bdy_area(xIdx, yIdx)) {
				nlcdLandCat = CConst::tropicalRainForestNLCDLandCat;
			}
		}

		// For constant set environments:
	} else if (_propEnvMethod == CConst::urbanPropEnvMethod) {
		propEnv = CConst::urbanPropEnv;
	} else if (_propEnvMethod == CConst::suburbanPropEnvMethod) {
		propEnv = CConst::suburbanPropEnv;
	} else if (_propEnvMethod == CConst::ruralPropEnvMethod) {
		propEnv = CConst::ruralPropEnv;
	} else {
		throw std::runtime_error("Error in selecting propagation environment");
	}

	if ((propEnv == CConst::unknownPropEnv) && (errorFlag)) {
		throw std::runtime_error(
			ErrStream()
			<< "ERROR: RLAN Location LAT = " << latDeg << " LON = " << lonDeg
			<< " outside Simulation Region defined by population density file");
	}

	return (propEnv);
}
/******************************************************************************************/

/******************************************************************************************/
/* AfcManager::computeClutter452HtEl                                                      */
/* See ITU-R p.452-16, Section 4.5.3                                                      */
/******************************************************************************************/
double AfcManager::computeClutter452HtEl(double txHeightM,
					 double distKm,
					 double elevationAngleDeg) const
{
	// Input values
	double d_k = 0.07; // Distance (km) from nominal clutter point to antenna
	double h_a = 5.0; // Nominal clutter height (m) above local ground level

	// double distKm = 1; Test case
	// double frequency = 6; // GHz per ITU-R p.452; this is unused if using F_fc=1

	// Calculate elevationAngleClutterLimitDeg
	double tanVal = (h_a - txHeightM) /
			(d_k * 1e3); // Tan value of elevation AngleClutterLimitDeg
	double elevationAngleClutterLimitDeg =
		atan(tanVal) * 180 / M_PI; // Returns elevationAngleClutterLimit in degrees

	// Calculate calculate clutter loss
	double htanVal = 6 * (txHeightM / h_a - 0.625); // htan angle
	// double F_fc = 0.25 + 0.375 * (1 + tanh(7.5 * (frequency - 0.5))); // Use this equation
	// for non-6GHz frequencies
	double F_fc = 1; // Near 6GHz frequency, this is always approximately 1
	double A_h = 10.25 * F_fc * exp(-1 * d_k) * (1 - tanh(htanVal)) - 0.33; // Clutter loss

	if ((elevationAngleDeg <= elevationAngleClutterLimitDeg) &&
	    (distKm > d_k * 10)) // If signal is below clutter
		return (A_h); // Set path clutter loss
	else
		return (0.0); // Otherwise, no clutter loss
}
/**************************************************************************************/

/**************************************************************************************/
/* AfcManager::setConstInputs()                                                       */
/**************************************************************************************/
void AfcManager::setConstInputs(const std::string &tempDir)
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
	RlanRegionClass::minRlanHeightAboveTerrain = _minRlanHeightAboveTerrain;

	_maxRadius = 150.0e3;
	_exclusionDist = 1.0;

	_illuminationEfficiency = 1.0;
	_closeInDist = 1.0e3; // Radius in which close in path loss model is used
	_closeInPathLossModel = "WINNER2"; // Close in path loss model is used

	_wlanMinFreqMHz = 5925;
	_wlanMaxFreqMHz = 7125;

	// Hardcode to US for now.
	// When multiple countries are supported this need to come from AFC Configuration
	for (auto &opClass : OpClass::USOpClass) {
		_opClass.push_back(opClass);
	}

	for (auto &opClass : OpClass::PSDOpClassList) {
		_psdOpClassList.push_back(opClass);
	}

	_wlanMinFreq = _wlanMinFreqMHz * 1.0e6;
	_wlanMaxFreq = _wlanMaxFreqMHz * 1.0e6;

	_filterSimRegionOnly = false;

	// _rlanBWStr = "20.0e6,40.0e6,80.0e6,160.0e6"; // Channel bandwidths in Hz

	_regionPolygonResolution = 1.0e-5;

	if (AfcManager::_createDebugFiles) {
		_excThrFile = QDir(QString::fromStdString(tempDir))
				      .filePath("exc_thr.csv.gz")
				      .toStdString();
		_fsAnomFile = QDir(QString::fromStdString(tempDir))
				      .filePath("fs_anom.csv.gz")
				      .toStdString();
		_userInputsFile = QDir(QString::fromStdString(tempDir))
					  .filePath("userInputs.csv.gz")
					  .toStdString();
	} else {
		_excThrFile = "";
		_fsAnomFile = "";
		_userInputsFile = "";
	}
	if (AfcManager::_createSlowDebugFiles) {
		_eirpGcFile =
			QDir(QString::fromStdString(tempDir)).filePath("eirp.csv.gz").toStdString();
	}
	if (AfcManager::_createKmz) {
		_kmlFile =
			QDir(QString::fromStdString(tempDir)).filePath("results.kmz").toStdString();
	}
	/**************************************************************************************/
}
/**************************************************************************************/

/**************************************************************************************/
/* AfcManager::computeInquiredFreqRangesPSD()                                         */
/**************************************************************************************/
void AfcManager::computeInquiredFreqRangesPSD(std::vector<psdFreqRangeClass> &psdFreqRangeList)
{
	for (auto &channel : _channelList) {
		if (channel.type == INQUIRED_FREQUENCY) {
			psdFreqRangeClass psdFreqRange;
			for (int freqSegIdx = 0; freqSegIdx < channel.segList.size();
			     ++freqSegIdx) {
				if (freqSegIdx == 0) {
					psdFreqRange.freqMHzList.push_back(channel.freqMHzList[0]);
				}
				int freqSegStartFreqMHz = channel.freqMHzList[freqSegIdx];
				int freqSegStopFreqMHz = channel.freqMHzList[freqSegIdx + 1];
				ChannelColor chanColor = std::get<2>(channel.segList[freqSegIdx]);
				if ((chanColor != BLACK) && (chanColor != RED)) {
					double segEIRPLimitA = std::get<0>(
						channel.segList[freqSegIdx]);
					double segEIRPLimitB = std::get<1>(
						channel.segList[freqSegIdx]);
					double psdA = segEIRPLimitA -
						      10.0 * log10((double)channel.bandwidth(
								     freqSegIdx));
					double psdB = segEIRPLimitB -
						      10.0 * log10((double)channel.bandwidth(
								     freqSegIdx));

					if (_roundPSDEIRPFlag) {
						// PSD value rounded down to nearest multiple of 0.1
						// dB
						psdA = std::floor(psdA * 10) / 10.0;
						psdB = std::floor(psdB * 10) / 10.0;
						double prevPSD = psdA;
						int prevFreq = freqSegStartFreqMHz;
						while (prevFreq < freqSegStopFreqMHz) {
							if (prevPSD == psdB) {
								psdFreqRange.freqMHzList.push_back(
									freqSegStopFreqMHz);
								psdFreqRange.psd_dBm_MHzList
									.push_back(psdB);
								prevPSD = psdB;
								prevFreq = freqSegStopFreqMHz;
							} else {
								int n = (int)floor(
									fabs(psdB - prevPSD) / 0.1 +
									0.5);
								int m = freqSegStopFreqMHz -
									prevFreq;
								double freqMHzVal = prevFreq +
										    (m + n - 1) / n;
								double psdVal =
									(psdA * (freqSegStopFreqMHz -
										 freqMHzVal) +
									 psdB * (freqMHzVal -
										 freqSegStartFreqMHz)) /
									(freqSegStopFreqMHz -
									 freqSegStartFreqMHz);
								psdVal = std::floor(psdVal * 10) /
									 10.0;
								double minPSD = std::min(prevPSD,
											 psdVal);
								psdFreqRange.freqMHzList.push_back(
									freqMHzVal);
								psdFreqRange.psd_dBm_MHzList
									.push_back(minPSD);
								prevPSD = psdVal;
								prevFreq = freqMHzVal;
							}
						}
					} else {
						double prevPSD = psdA;
						if (prevPSD == psdB) {
							psdFreqRange.freqMHzList.push_back(
								freqSegStopFreqMHz);
							psdFreqRange.psd_dBm_MHzList.push_back(
								psdB);
							prevPSD = psdB;
						} else {
							for (int freqMHzVal =
								     freqSegStartFreqMHz + 1;
							     freqMHzVal <= freqSegStopFreqMHz;
							     ++freqMHzVal) {
								double psdVal =
									(psdA * (freqSegStopFreqMHz -
										 freqMHzVal) +
									 psdB * (freqMHzVal -
										 freqSegStartFreqMHz)) /
									(freqSegStopFreqMHz -
									 freqSegStartFreqMHz);
								double minPSD = std::min(prevPSD,
											 psdVal);
								psdFreqRange.freqMHzList.push_back(
									freqMHzVal);
								psdFreqRange.psd_dBm_MHzList
									.push_back(minPSD);
								prevPSD = psdVal;
							}
						}
					}
				} else {
					psdFreqRange.freqMHzList.push_back(freqSegStopFreqMHz);
					psdFreqRange.psd_dBm_MHzList.push_back(quietNaN);
				}
			}

			int segIdx;
			for (segIdx = psdFreqRange.psd_dBm_MHzList.size() - 2; segIdx >= 0;
			     --segIdx) {
				if (psdFreqRange.psd_dBm_MHzList[segIdx] ==
				    psdFreqRange.psd_dBm_MHzList[segIdx + 1]) {
					psdFreqRange.psd_dBm_MHzList.erase(
						psdFreqRange.psd_dBm_MHzList.begin() + segIdx + 1);
					psdFreqRange.freqMHzList.erase(
						psdFreqRange.freqMHzList.begin() + segIdx + 1);
				}
			}

			psdFreqRangeList.push_back(psdFreqRange);
		}
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

	/**********************************************************************************/
	/* Check that inquired frequency ranges are valid and translate into sorted       */
	/* list of disjoint segments.                                                     */
	/**********************************************************************************/
	std::vector<std::pair<int, int>> freqSegmentList;
	for (auto &freqRange : _inquiredFrequencyRangesMHz) {
		auto inquiredStartFreqMHz = freqRange.first;
		auto inquiredStopFreqMHz = freqRange.second;

		if (inquiredStopFreqMHz <= inquiredStartFreqMHz) {
			LOGGER_DEBUG(logger) << "Inquired Freq Range INVALID: ["
					     << inquiredStartFreqMHz << ", " << inquiredStopFreqMHz
					     << "] stop freq must be > start freq" << std::endl;
			_responseCode = CConst::unsupportedSpectrumResponseCode;
			return;
		}

		std::vector<std::pair<int, int>> overlapBandList =
			calculateOverlapBandList(_allowableFreqBandList,
						 inquiredStartFreqMHz,
						 inquiredStopFreqMHz);

		if (overlapBandList.size()) {
			for (int segIdx = 0; segIdx < (int)overlapBandList.size(); ++segIdx) {
				int startFreq = overlapBandList[segIdx].first;
				int stopFreq = overlapBandList[segIdx].second;

				int idxA = 0;
				bool overlapA;
				while ((idxA < freqSegmentList.size()) &&
				       (startFreq > freqSegmentList[idxA].second)) {
					idxA++;
				}
				overlapA = ((idxA < freqSegmentList.size()) &&
					    (startFreq >= freqSegmentList[idxA].first));

				int idxB = 0;
				bool overlapB;
				while ((idxB < freqSegmentList.size()) &&
				       (stopFreq > freqSegmentList[idxB].second)) {
					idxB++;
				}
				overlapB = ((idxB < freqSegmentList.size()) &&
					    (stopFreq >= freqSegmentList[idxB].first));

				int start = overlapA ? freqSegmentList[idxA].first : startFreq;
				int stop = overlapB ? freqSegmentList[idxB].second : stopFreq;
				int delStart = idxA;
				int delStop = overlapB ? idxB : idxB - 1;

				if (delStop >= delStart) {
					freqSegmentList.erase(freqSegmentList.begin() + delStart,
							      freqSegmentList.begin() + delStop +
								      1);
				}
				freqSegmentList.insert(freqSegmentList.begin() + delStart,
						       std::make_pair(start, stop));
			}
		} else {
			// the start/stop frequencies are not valid
			LOGGER_DEBUG(logger)
				<< "Inquired Freq Range INVALID: [" << inquiredStartFreqMHz << ", "
				<< inquiredStopFreqMHz << "]" << std::endl;
			_responseCode = CConst::unsupportedSpectrumResponseCode;
			return;
		}
	}
	/**********************************************************************************/

#if DEBUG_AFC && 0
	std::cout << "freqSegmentList contains:" << std::endl;
	for (int i = 0; i < freqSegmentList.size(); i++)
		std::cout << " [" << freqSegmentList[i].first << "," << freqSegmentList[i].second
			  << "]" << std::endl;
	std::cout << '\n';
#endif

	if (freqSegmentList.size()) {
		ChannelStruct channel;
		channel.operatingClass = -1;
		channel.index = -1;
		channel.type = INQUIRED_FREQUENCY;

		for (int segIdx = 0; segIdx < (int)freqSegmentList.size(); ++segIdx) {
			channel.freqMHzList.push_back(freqSegmentList[segIdx].first);
			channel.freqMHzList.push_back(freqSegmentList[segIdx].second);

			if (segIdx) {
				channel.segList.push_back(std::make_tuple(
					0.0,
					0.0,
					BLACK)); // Between segments not used, initialized to BLACK
			}
			channel.segList.push_back(
				std::make_tuple(0.0,
						0.0,
						GREEN)); // Everything initialized to GREEN
		}
		_channelList.push_back(channel);
	}

	for (auto &channelPair : _inquiredChannels) {
		LOGGER_DEBUG(logger)
			<< "creating channels for operating class " << channelPair.first;

		int numChan;
		numChan = 0;

		// Iterate each operating classes and add all channels of given operating class
		for (auto &opClass : _opClass) {
			// Skip of classes of not in inquired channel list
			if (opClass.opClass != channelPair.first) {
				continue;
			}

			for (auto &cfi : opClass.channels) {
				bool includeChannel;

				includeChannel = false;

				// If channel indexes are provided check for channel validity.
				// If channel indexes are not provided then include all channels of
				// given operating class.
				if (channelPair.second.size() != 0) {
					for (auto inquired_cfi : channelPair.second) {
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
					int startFreqMHz = (opClass.startFreq + 5 * cfi) -
							   (opClass.bandWidth >> 1);
					int stopFreqMHz = startFreqMHz + opClass.bandWidth;
					channel.freqMHzList.push_back(startFreqMHz);
					channel.freqMHzList.push_back(stopFreqMHz);

					// Include channel if it is within the frequency bands in
					// AFC config
					if (containsChannel(_allowableFreqBandList,
							    startFreqMHz,
							    stopFreqMHz)) {
						channel.operatingClass = opClass.opClass;
						channel.index = cfi;
						channel.segList.push_back(std::make_tuple(
							0.0,
							0.0,
							GREEN)); // Everything initialized to GREEN
						channel.type = ChannelType::INQUIRED_CHANNEL;
						_channelList.push_back(channel);
						numChan++;
						totalNumChan++;
						LOGGER_DEBUG(logger)
							<< "added " << numChan << " channels";
					}
				}
			}
			if (numChan == 0) {
				LOGGER_DEBUG(logger) << "Inquired Channel INVALID Operating Class: "
						     << channelPair.first << std::endl;
				_responseCode = CConst::unsupportedSpectrumResponseCode;
				return;
			}
		}

		if (totalNumChan == 0) {
			LOGGER_DEBUG(logger)
				<< "Missing valid Inquired channel and frequency " << std::endl;
			_responseCode = CConst::missingParamResponseCode;
			return;
		}
	}
}
/**************************************************************************************/

/**************************************************************************************/
/* AfcManager::splitFrequencyRanges()                                                 */
/**************************************************************************************/
void AfcManager::splitFrequencyRanges()
{
	std::set<int> fsChannelEdgesMhz;
	for (int ulsIdx = 0; ulsIdx < _ulsList->getSize(); ++ulsIdx) {
		auto uls = (*_ulsList)[ulsIdx];
		fsChannelEdgesMhz.insert((int)floor((uls->getStartFreq() + 1.0) * 1.0e-6));
		fsChannelEdgesMhz.insert((int)ceil((uls->getStopFreq() - 1.0) * 1.0e-6));
	}

	int drIdx;
	for (drIdx = 0; drIdx < (int)_deniedRegionList.size(); ++drIdx) {
		DeniedRegionClass *dr = _deniedRegionList[drIdx];
		fsChannelEdgesMhz.insert((int)floor((dr->getStartFreq() + 1.0) * 1.0e-6));
		fsChannelEdgesMhz.insert((int)ceil((dr->getStopFreq() - 1.0) * 1.0e-6));
	}

	int numFsFreq = fsChannelEdgesMhz.size();

	if (numFsFreq == 0) {
		return;
	}
	std::set<int>::iterator fsChannelEdgesMhzIt = fsChannelEdgesMhz.begin();
	int minFsFreq = *std::next(fsChannelEdgesMhzIt, 0);
	int maxFsFreq = *std::next(fsChannelEdgesMhzIt, numFsFreq - 1);

	for (int chanIdx = 0; chanIdx < (int)_channelList.size(); ++chanIdx) {
		ChannelStruct *channel = &(_channelList[chanIdx]);
		if (channel->type == INQUIRED_FREQUENCY) {
			int segIdx = 0;
			while (segIdx < channel->segList.size()) {
				ChannelColor segColor = std::get<2>(channel->segList[segIdx]);
				if (segColor != BLACK) {
					int chanStartFreqMHz = channel->freqMHzList[segIdx];
					int chanStopFreqMHz = channel->freqMHzList[segIdx + 1];

					int aIdx = -2;
					int bIdx = -2;

					if ((maxFsFreq <= chanStartFreqMHz) ||
					    (minFsFreq >= chanStopFreqMHz)) {
						// Do nothing
					} else {
						aIdx = -1;
						std::set<int>::iterator aIt = fsChannelEdgesMhzIt;
						while (*aIt <= chanStartFreqMHz) {
							aIdx++;
							aIt++;
						};
						bIdx = numFsFreq;
						std::set<int>::reverse_iterator bIt =
							fsChannelEdgesMhz.rbegin();
						while (*bIt >= chanStopFreqMHz) {
							bIdx--;
							bIt++;
						}
						int numIns = bIdx - aIdx - 1;

						if (numIns > 0) {
							for (int insIdx = 0; insIdx < numIns;
							     ++insIdx) {
								channel->freqMHzList.insert(
									channel->freqMHzList
											.begin() +
										segIdx + 1 + insIdx,
									*aIt);
								channel->segList.insert(
									channel->segList.begin() +
										segIdx + insIdx,
									std::make_tuple(0.0,
											0.0,
											GREEN));
								// Everything initialized to GREEN
								aIt++;
							}
						}
						segIdx += numIns;
					}
				}
				segIdx++;
			}

#if DEBUG_AFC && 0
			std::cout << "NUM_SEG: " << channel->segList.size() << std::endl;
			for (segIdx = 0; segIdx < channel->segList.size(); ++segIdx) {
				ChannelColor segColor = std::get<2>(channel->segList[segIdx]);
				std::string colorStr;
				switch (segColor) {
					case BLACK:
						colorStr = "BLACK";
						break;
					case RED:
						colorStr = "RED";
						break;
					case YELLOW:
						colorStr = "YELLOW";
						break;
					case GREEN:
						colorStr = "GREEN";
						break;
					default:
						CORE_DUMP;
						break;
				}
				std::cout << "SEG " << segIdx << ": "
					  << channel->freqMHzList[segIdx] << " - "
					  << channel->freqMHzList[segIdx + 1] << " " << colorStr
					  << std::endl;
			}
#endif
		}
	}
}
/**************************************************************************************/

/**************************************************************************************/
/* AfcManager::containsChannel()                                                      */
/**************************************************************************************/
bool AfcManager::containsChannel(const std::vector<FreqBandClass> &freqBandList,
				 int chanStartFreqMHz,
				 int chanStopFreqMHz)
{
	auto segmentList = std::vector<std::pair<int, int>> {
		std::make_pair(chanStartFreqMHz, chanStopFreqMHz)};

	for (auto &freqBand : freqBandList) {
		int bandStart = freqBand.getStartFreqMHz();
		int bandStop = freqBand.getStopFreqMHz();
		int segIdx = 0;
		while (segIdx < (int)segmentList.size()) {
			int segStart = segmentList[segIdx].first;
			int segStop = segmentList[segIdx].second;

			if ((bandStop <= segStart) || (bandStart >= segStop)) {
				// No overlap, do nothing
				segIdx++;
			} else if ((bandStart <= segStart) && (bandStop >= segStop)) {
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
				segmentList.insert(segmentList.begin() + segIdx + 1,
						   std::make_pair(bandStop, segStop));
				segIdx += 2;
			}
		}
	}

	bool containsFlag = (segmentList.size() == 0);

	return (containsFlag);
}
/**************************************************************************************/

/**************************************************************************************/
/* AfcManager::calculateOverlapBandList()                                                      */
/**************************************************************************************/
std::vector<std::pair<int, int>> AfcManager::calculateOverlapBandList(
	const std::vector<FreqBandClass> &freqBandList,
	int chanStartFreqMHz,
	int chanStopFreqMHz)
{
	std::vector<std::pair<int, int>> overlapBandList;

	for (auto &freqBand : freqBandList) {
		int bandStart = freqBand.getStartFreqMHz();
		int bandStop = freqBand.getStopFreqMHz();
		int overlapStart = std::max(bandStart, chanStartFreqMHz);
		int overlapStop = std::min(bandStop, chanStopFreqMHz);

		if (overlapStart < overlapStop) {
			int segIdxA = -1;
			int segIdxB = -1;
			for (int segIdx = overlapBandList.size() - 1;
			     (segIdx >= 0) && (segIdxA == -1);
			     --segIdx) {
				if (overlapBandList[segIdx].first <= overlapStart) {
					segIdxA = segIdx;
				}
			}
			for (int segIdx = 0;
			     segIdx < ((int)overlapBandList.size()) && (segIdxB == -1);
			     ++segIdx) {
				if (overlapBandList[segIdx].second >= overlapStop) {
					segIdxB = segIdx;
				}
			}
			if ((segIdxA == -1) && (segIdxB == -1)) {
				overlapBandList.clear();
				overlapBandList.push_back(
					std::make_pair(overlapStart, overlapStop));
			} else if (segIdxA == -1) {
				if (segIdxB) {
					overlapBandList.erase(overlapBandList.begin(),
							      overlapBandList.begin() + segIdxB);
				}
				if (overlapStop < overlapBandList[0].first) {
					overlapBandList.insert(overlapBandList.begin(),
							       std::make_pair(overlapStart,
									      overlapStop));
				} else {
					overlapBandList[0].first = overlapStart;
				}
			} else if (segIdxB == -1) {
				if (segIdxA + 1 < (int)overlapBandList.size()) {
					overlapBandList.erase(overlapBandList.begin() + segIdxA + 1,
							      overlapBandList.end());
				}
				if (overlapStart > overlapBandList[segIdxA].second) {
					overlapBandList.push_back(
						std::make_pair(overlapStart, overlapStop));
				} else {
					overlapBandList[segIdxA].second = overlapStop;
				}
			} else if (segIdxA < segIdxB) {
				int startEraseIdx;
				int stopEraseIdx;
				if (overlapStart > overlapBandList[segIdxA].second) {
					startEraseIdx = segIdxA + 1;
				} else {
					startEraseIdx = segIdxA;
					overlapStart = overlapBandList[segIdxA].first;
				}
				if (overlapStop < overlapBandList[segIdxB].first) {
					stopEraseIdx = segIdxB - 1;
				} else {
					stopEraseIdx = segIdxB;
					overlapStop = overlapBandList[segIdxB].second;
				}
				overlapBandList.erase(overlapBandList.begin() + startEraseIdx,
						      overlapBandList.begin() + stopEraseIdx + 1);
				overlapBandList.insert(overlapBandList.begin() + startEraseIdx,
						       std::make_pair(overlapStart, overlapStop));
			}
		}
	}

	return (overlapBandList);
}
/**************************************************************************************/

#if DEBUG_AFC
/******************************************************************************************/
/* AfcManager::runTestITM(std::string inputFile)                                          */
/******************************************************************************************/
void AfcManager::runTestITM(std::string inputFile)
{
	LOGGER_INFO(logger) << "Executing AfcManager::runTestITM()";

	#if 1
	extern void point_to_point(double elev[],
				   double tht_m,
				   double rht_m,
				   double eps_dielect,
				   double sgm_conductivity,
				   double eno_ns_surfref,
				   double frq_mhz,
				   int radio_climate,
				   int pol,
				   double conf,
				   double rel,
				   double &dbloss,
				   std::string &strmode,
				   int &errnum);
	#endif

	int linenum, fIdx;
	std::string line, strval;
	char *chptr;
	std::string str;
	std::string reasonIgnored;
	std::ostringstream errStr;

	double rlanLon = -91.43291667;
	double rlanLat = 41.4848611111;

	double fsLon = -91.74102778;
	double fsLat = 41.96444444;
	double fsHeightAGL = 108.5;

	double frequencyMHz = 6175.0;

	double groundDistanceKm;
	{
		double lon1Rad = rlanLon * M_PI / 180.0;
		double lat1Rad = rlanLat * M_PI / 180.0;
		double lon2Rad = fsLon * M_PI / 180.0;
		double lat2Rad = fsLat * M_PI / 180.0;

		double slat = sin((lat2Rad - lat1Rad) / 2);
		double slon = sin((lon2Rad - lon1Rad) / 2);
		groundDistanceKm = 2 * CConst::averageEarthRadius *
				   asin(sqrt(slat * slat +
					     cos(lat1Rad) * cos(lat2Rad) * slon * slon)) *
				   1.0e-3;
	}

	int terrainHeightFieldIdx = -1;

	std::vector<int *> fieldIdxList;
	std::vector<std::string> fieldLabelList;
	fieldIdxList.push_back(&terrainHeightFieldIdx);
	fieldLabelList.push_back("Terrain Height (m)");
	fieldIdxList.push_back(&terrainHeightFieldIdx);
	fieldLabelList.push_back("TERRAIN_HEIGHT_AMSL (m)");

	double terrainHeight;

	std::vector<double> heightList;

	int fieldIdx;

	if (inputFile.empty()) {
		throw std::runtime_error("ERROR: No ITM Test File specified");
	}

	LOGGER_INFO(logger) << "Reading Winner2 Test File: " << inputFile;

	FILE *fin;
	if (!(fin = fopen(inputFile.c_str(), "rb"))) {
		str = std::string("ERROR: Unable to open ITM Test File \"") + inputFile +
		      std::string("\"\n");
		throw std::runtime_error(str);
	}

	enum LineTypeEnum { labelLineType, dataLineType, ignoreLineType, unknownLineType };

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
			if (fIdx == (int)std::string::npos) {
				if (fieldList.size() == 1) {
					lineType = ignoreLineType;
				}
			} else {
				if (fieldList[0].at(fIdx) == '#') {
					lineType = ignoreLineType;
				}
			}
		}

		if ((lineType == unknownLineType) && (!foundLabelLine)) {
			lineType = labelLineType;
			foundLabelLine = 1;
		}
		if ((lineType == unknownLineType) && (foundLabelLine)) {
			lineType = dataLineType;
		}
		/**************************************************************************/

		/**************************************************************************/
		/**** Process Line                                                     ****/
		/**************************************************************************/
		bool found;
		std::string field;
		switch (lineType) {
			case labelLineType:
				for (fieldIdx = 0; fieldIdx < (int)fieldList.size(); fieldIdx++) {
					field = fieldList.at(fieldIdx);

					// std::cout << "FIELD: \"" << field << "\"" << std::endl;

					found = false;
					for (fIdx = 0;
					     (fIdx < (int)fieldLabelList.size()) && (!found);
					     fIdx++) {
						if (field == fieldLabelList.at(fIdx)) {
							*fieldIdxList.at(fIdx) = fieldIdx;
							found = true;
						}
					}
				}

				for (fIdx = 0; fIdx < (int)fieldIdxList.size(); fIdx++) {
					if (*fieldIdxList.at(fIdx) == -1) {
						errStr << "ERROR: Invalid Winner2 Test Input file "
							  "\""
						       << inputFile << "\" label line missing \""
						       << fieldLabelList.at(fIdx) << "\""
						       << std::endl;
						throw std::runtime_error(errStr.str());
					}
				}

				break;
			case dataLineType:
				/**************************************************************************/
				/* terrainHeight */
				/**************************************************************************/
				strval = fieldList.at(terrainHeightFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid ITM Test Input file \""
					       << inputFile << "\" line " << linenum
					       << " missing Terrain Height (m)" << std::endl;
					throw std::runtime_error(errStr.str());
				}
				terrainHeight = std::strtod(strval.c_str(), &chptr);
				/**************************************************************************/

				heightList.push_back(terrainHeight);

				break;
			case ignoreLineType:
			case unknownLineType:
				// do nothing
				break;
			default:
				CORE_DUMP;
				break;
		}
	}

	int numpts = heightList.size();
	double *heightProfile = (double *)calloc(sizeof(double), numpts + 2);

	heightProfile[0] = numpts - 1;
	heightProfile[1] = (groundDistanceKm / (numpts - 1)) * 1000.0;

	double eps_dielect = _itmEpsDielect;
	double sgm_conductivity = _itmSgmConductivity;
	double surfaceRefractivity = _ituData->getSurfaceRefractivityValue((rlanLat + fsLat) / 2,
									   (rlanLon + fsLon) / 2);

	int radioClimate = _ituData->getRadioClimateValue(rlanLat, rlanLon);
	int radioClimateTmp = _ituData->getRadioClimateValue(fsLat, fsLon);
	if (radioClimateTmp < radioClimate) {
		radioClimate = radioClimateTmp;
	}

	int pol = _itmPolarization;
	double conf = _confidenceITM;
	double rel = _reliabilityITM;

	int errnum;
	double pathLoss;
	std::string strmode;
	// char strmode[50];

	std::cout << "PATH_PROFILE: " << inputFile << std::endl;
	std::cout << "GROUND DISTANCE (Km): " << groundDistanceKm << std::endl;
	std::cout << "NUMPTS: " << numpts << std::endl;
	std::cout << "DELTA (m): " << heightProfile[1] << std::endl;

	std::cout << "RLAN LON (deg): " << rlanLon << std::endl;
	std::cout << "RLAN LAT (deg): " << rlanLat << std::endl;
	std::cout << "FS   LON (deg): " << fsLon << std::endl;
	std::cout << "FS   LAT (deg): " << fsLat << std::endl;
	std::cout << "FS   HEIGHT AGL (m) : " << fsHeightAGL << std::endl;

	std::cout << "eps_dielect: " << eps_dielect << std::endl;
	std::cout << "sgm_conductivity: " << sgm_conductivity << std::endl;
	std::cout << "surfaceRefractivity: " << surfaceRefractivity << std::endl;
	std::cout << "frequencyMHz: " << frequencyMHz << std::endl;
	std::cout << "radioClimate: " << radioClimate << std::endl;
	std::cout << "pol: " << pol << std::endl;
	std::cout << "conf: " << conf << std::endl;
	std::cout << "rel: " << rel << std::endl;

	std::cout << std::endl;

	int numRlanHeight = 2;
	double rlanHeightAGLStart = 10.0;
	double rlanHeightAGLStop = 11.0;

	for (bool reverseFlag : {false}) {
		for (int rlanHeightIdx = 0; rlanHeightIdx < numRlanHeight; ++rlanHeightIdx) {
			double rlanHeightAGL = (rlanHeightAGLStart *
							(numRlanHeight - 1 - rlanHeightIdx) +
						rlanHeightAGLStop * rlanHeightIdx) /
					       (numRlanHeight - 1);
			std::cout << "RLAN HEIGHT AGL (m) : " << rlanHeightAGL << std::endl;

			for (int i = 0; i < numpts; ++i) {
				heightProfile[2 + i] = heightList[reverseFlag ? numpts - 1 - i : i];
			}

			double txHeightAGL;
			double rxHeightAGL;

			if (reverseFlag) {
				txHeightAGL = fsHeightAGL;
				rxHeightAGL = rlanHeightAGL;
			} else {
				txHeightAGL = rlanHeightAGL;
				rxHeightAGL = fsHeightAGL;
			}

			point_to_point(heightProfile,
				       txHeightAGL,
				       rxHeightAGL,
				       eps_dielect,
				       sgm_conductivity,
				       surfaceRefractivity,
				       frequencyMHz,
				       radioClimate,
				       pol,
				       conf,
				       rel,
				       pathLoss,
				       strmode,
				       errnum);

			std::cout << "REVERSE_FLAG: " << reverseFlag << std::endl;
			std::cout << "MODE: " << strmode << std::endl;
			std::cout << "ERRNUM: " << errnum << std::endl;
			std::cout << "PATH_LOSS (DB): " << pathLoss << std::endl;
			std::cout << "PT," << inputFile << "," << rlanHeightAGL << "," << pathLoss
				  << std::endl;
		}
		std::cout << "PT,,," << std::endl;
	}

	return;
}
/******************************************************************************************/
#endif

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

	std::vector<int *> fieldIdxList;
	std::vector<std::string> fieldLabelList;
	fieldIdxList.push_back(&regionFieldIdx);
	fieldLabelList.push_back("Region");
	fieldIdxList.push_back(&distanceFieldIdx);
	fieldLabelList.push_back("Distance (m)");
	fieldIdxList.push_back(&hbFieldIdx);
	fieldLabelList.push_back("hb (m)");
	fieldIdxList.push_back(&hmFieldIdx);
	fieldLabelList.push_back("hm (m)");
	fieldIdxList.push_back(&frequencyFieldIdx);
	fieldLabelList.push_back("Frequency (GHz)");
	fieldIdxList.push_back(&confidenceFieldIdx);
	fieldLabelList.push_back("Confidence");

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
	if (!(fin = fopen(inputFile.c_str(), "rb"))) {
		str = std::string("ERROR: Unable to open Winner2 Test File \"") + inputFile +
		      std::string("\"\n");
		throw std::runtime_error(str);
	}

	FILE *fout;
	if (!(fout = fopen(outputFile.c_str(), "wb"))) {
		errStr << std::string("ERROR: Unable to open Winner2 Test Output File \"") +
				  outputFile + std::string("\"\n");
		throw std::runtime_error(errStr.str());
	}

	enum LineTypeEnum { labelLineType, dataLineType, ignoreLineType, unknownLineType };

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
			if (fIdx == (int)std::string::npos) {
				if (fieldList.size() == 1) {
					lineType = ignoreLineType;
				}
			} else {
				if (fieldList[0].at(fIdx) == '#') {
					lineType = ignoreLineType;
				}
			}
		}

		if ((lineType == unknownLineType) && (!foundLabelLine)) {
			lineType = labelLineType;
			foundLabelLine = 1;
		}
		if ((lineType == unknownLineType) && (foundLabelLine)) {
			lineType = dataLineType;
		}
		/**************************************************************************/

		/**************************************************************************/
		/**** Process Line                                                     ****/
		/**************************************************************************/
		bool found;
		std::string field;
		switch (lineType) {
			case labelLineType:
				for (fieldIdx = 0; fieldIdx < (int)fieldList.size(); fieldIdx++) {
					field = fieldList.at(fieldIdx);

					// std::cout << "FIELD: \"" << field << "\"" << std::endl;

					found = false;
					for (fIdx = 0;
					     (fIdx < (int)fieldLabelList.size()) && (!found);
					     fIdx++) {
						if (field == fieldLabelList.at(fIdx)) {
							*fieldIdxList.at(fIdx) = fieldIdx;
							found = true;
						}
					}
				}

				for (fIdx = 0; fIdx < (int)fieldIdxList.size(); fIdx++) {
					if (*fieldIdxList.at(fIdx) == -1) {
						errStr << "ERROR: Invalid Winner2 Test Input file "
							  "\""
						       << inputFile << "\" label line missing \""
						       << fieldLabelList.at(fIdx) << "\""
						       << std::endl;
						throw std::runtime_error(errStr.str());
					}
				}

				fprintf(fout,
					"%s,%s,%s,%s,%s\n",
					line.c_str(),
					"afc_probLOS",
					"afc_plLOS",
					"afc_plNLOS",
					"afc_plCombined");

				break;
			case dataLineType:
				/**************************************************************************/
				/* REGION (propEnv) */
				/**************************************************************************/
				strval = fieldList.at(regionFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid Winner2 Test Input file \""
					       << inputFile << "\" line " << linenum
					       << " missing REGION" << std::endl;
					throw std::runtime_error(errStr.str());
				}

				propEnv = (CConst::PropEnvEnum)CConst::strPropEnvList
						  ->str_to_type(strval, validFlag);

				if (!validFlag) {
					errStr << "ERROR: Invalid Winner2 Test Input file \""
					       << inputFile << "\" line " << linenum
					       << " INVALID REGION = " << strval << std::endl;
					throw std::runtime_error(errStr.str());
				}
				/**************************************************************************/

				/**************************************************************************/
				/* distance */
				/**************************************************************************/
				strval = fieldList.at(distanceFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid Winner2 Test Input file \""
					       << inputFile << "\" line " << linenum
					       << " missing Distance (m)" << std::endl;
					throw std::runtime_error(errStr.str());
				}
				distance = std::strtod(strval.c_str(), &chptr);
				/**************************************************************************/

				/**************************************************************************/
				/* hb */
				/**************************************************************************/
				strval = fieldList.at(hbFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid Winner2 Test Input file \""
					       << inputFile << "\" line " << linenum
					       << " missing hb" << std::endl;
					throw std::runtime_error(errStr.str());
				}
				hb = std::strtod(strval.c_str(), &chptr);
				/**************************************************************************/

				/**************************************************************************/
				/* hm */
				/**************************************************************************/
				strval = fieldList.at(hmFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid Winner2 Test Input file \""
					       << inputFile << "\" line " << linenum
					       << " missing hm" << std::endl;
					throw std::runtime_error(errStr.str());
				}
				hm = std::strtod(strval.c_str(), &chptr);
				/**************************************************************************/

				/**************************************************************************/
				/* frequency */
				/**************************************************************************/
				strval = fieldList.at(frequencyFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid Winner2 Test Input file \""
					       << inputFile << "\" line " << linenum
					       << " missing Frequency (GHz)" << std::endl;
					throw std::runtime_error(errStr.str());
				}
				frequency = std::strtod(strval.c_str(), &chptr) *
					    1.0e9; // Convert GHz to Hz
				/**************************************************************************/

				/**************************************************************************/
				/* confidence */
				/**************************************************************************/
				strval = fieldList.at(confidenceFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Invalid Winner2 Test Input file \""
					       << inputFile << "\" line " << linenum
					       << " missing Confidence" << std::endl;
					throw std::runtime_error(errStr.str());
				}
				confidence = std::strtod(strval.c_str(), &chptr);
				/**************************************************************************/

				zval = -qerfi(confidence);
				_zwinner2Combined = zval;
				if (propEnv == CConst::urbanPropEnv) {
					plLOS = Winner2_C2urban_LOS(distance,
								    hb,
								    hm,
								    frequency,
								    zval,
								    sigmaLOS,
								    pathLossCDF);
					plNLOS = Winner2_C2urban_NLOS(distance,
								      hb,
								      hm,
								      frequency,
								      zval,
								      sigmaNLOS,
								      pathLossCDF);
					plCombined = Winner2_C2urban(distance,
								     hb,
								     hm,
								     frequency,
								     sigmaCombined,
								     pathLossModelStr,
								     pathLossCDF,
								     probLOS,
								     winner2LOSValue);
				} else if (propEnv == CConst::suburbanPropEnv) {
					plLOS = Winner2_C1suburban_LOS(distance,
								       hb,
								       hm,
								       frequency,
								       zval,
								       sigmaLOS,
								       pathLossCDF);
					plNLOS = Winner2_C1suburban_NLOS(distance,
									 hb,
									 hm,
									 frequency,
									 zval,
									 sigmaNLOS,
									 pathLossCDF);
					plCombined = Winner2_C1suburban(distance,
									hb,
									hm,
									frequency,
									sigmaCombined,
									pathLossModelStr,
									pathLossCDF,
									probLOS,
									winner2LOSValue);
				} else if (propEnv == CConst::ruralPropEnv) {
					plLOS = Winner2_D1rural_LOS(distance,
								    hb,
								    hm,
								    frequency,
								    zval,
								    sigmaLOS,
								    pathLossCDF);
					plNLOS = Winner2_D1rural_NLOS(distance,
								      hb,
								      hm,
								      frequency,
								      zval,
								      sigmaNLOS,
								      pathLossCDF);
					plCombined = Winner2_D1rural(distance,
								     hb,
								     hm,
								     frequency,
								     sigmaCombined,
								     pathLossModelStr,
								     pathLossCDF,
								     probLOS,
								     winner2LOSValue);
				} else {
					CORE_DUMP;
				}

				if (distance < 50.0) {
					probLOS = 1.0;
					plCombined = plLOS;
				}

				fprintf(fout,
					"%s,%.3f,%3f,%.3f,%.3f\n",
					line.c_str(),
					probLOS,
					plLOS,
					plNLOS,
					plCombined);

				break;
			case ignoreLineType:
			case unknownLineType:
				// do nothing
				break;
			default:
				CORE_DUMP;
				break;
		}
	}

	if (fin) {
		fclose(fin);
	}
	if (fout) {
		fclose(fout);
	}

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

	FILE *fkml = (FILE *)NULL;
	/**************************************************************************************/
	/* Open KML File and write header                                                     */
	/**************************************************************************************/
	if (1) {
		if (!(fkml = fopen("/tmp/doc.kml", "wb"))) {
			errStr << std::string("ERROR: Unable to open kmlFile \"") + "/tmp/doc.kml" +
					  std::string("\"\n");
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
		fprintf(fkml,
			"        <description>%s : Show NLCD categories.</description>\n",
			"TEST");
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
	std::cout << "    NLCD_TOP_RIGHT: " << nlcdBr.lonDegMax << " " << nlcdBr.latDegMax
		  << std::endl;
	std::cout << "    NLCD_BOTTOM_LEFT: " << nlcdBr.lonDegMin << " " << nlcdBr.latDegMin
		  << std::endl;

	std::vector<std::string> colorList;

	int i;
	for (i = 0; i < 255; i++) {
		std::string color;
		switch (i) {
			case 21:
				color = "221 201 201";
				break;
			case 22:
				color = "216 147 130";
				break;
			case 23:
				color = "237   0   0";
				break;
			case 31:
				color = "178 173 163";
				break;
			case 32:
				color = "249 249 249";
				break;
			case 41:
				color = "104 170  99";
				break;
			case 42:
				color = " 28  99  48";
				break;
			case 43:
				color = "181 201 142";
				break;
			case 52:
				color = "204,186,124";
				break;

			case 1:
				color = "  0 249   0";
				break;
			case 11:
				color = " 71 107 160";
				break;
			case 12:
				color = "209 221 249";
				break;
			case 24:
				color = "170   0   0";
				break;
			case 51:
				color = "165 140  48";
				break;
			case 71:
				color = "226 226 193";
				break;
			case 72:
				color = "201 201 119";
				break;
			case 73:
				color = "153 193  71";
				break;
			case 74:
				color = "119 173 147";
				break;
			case 81:
				color = "219 216  61";
				break;
			case 82:
				color = "170 112  40";
				break;
			case 90:
				color = "186 216 234";
				break;
			case 91:
				color = "181 211 229";
				break;
			case 92:
				color = "181 211 229";
				break;
			case 93:
				color = "181 211 229";
				break;
			case 94:
				color = "181 211 229";
				break;
			case 95:
				color = "112 163 186";
				break;

			default:
				color = "255 255 255";
				break;
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
	double resolutionLon = (30.0 / CConst::earthRadius) * 180.0 / M_PI;
	double resolutionLat = (30.0 / CConst::earthRadius) * 180.0 / M_PI;

	std::vector<std::string> imageFileList;

	/**************************************************************************************/
	/* Define regions                                                                     */
	/**************************************************************************************/
	int maxPtsPerRegion = 5000;

	double longitudeDegStart = _popGrid->getMinLonDeg();
	double latitudeDegStart = _popGrid->getMinLatDeg();

	int numLon = (_popGrid->getMaxLonDeg() - longitudeDegStart) / resolutionLon;
	int numLat = (_popGrid->getMaxLatDeg() - latitudeDegStart) / resolutionLat;

	int numRegionLon = (numLon + maxPtsPerRegion - 1) / maxPtsPerRegion;
	int numRegionLat = (numLat + maxPtsPerRegion - 1) / maxPtsPerRegion;

	int lonRegionIdx;
	int latRegionIdx;
	int startLonIdx, stopLonIdx;
	int startLatIdx, stopLatIdx;
	int lonN = numLon / numRegionLon;
	int lonq = numLon % numRegionLon;
	int latN = numLat / numRegionLat;
	int latq = numLat % numRegionLat;
	/**************************************************************************************/

	std::cout << "    NUM_REGION_LON: " << numRegionLon << std::endl;
	std::cout << "    NUM_REGION_LAT: " << numRegionLat << std::endl;

	int interpolationFactor = 1;
	int interpLon, interpLat;

	fprintf(fkml, "        <Folder>\n");
	fprintf(fkml, "            <name>%s</name>\n", "NLCD");
	fprintf(fkml, "            <visibility>1</visibility>\n");

	for (lonRegionIdx = 0; lonRegionIdx < numRegionLon; lonRegionIdx++) {
		if (lonRegionIdx < lonq) {
			startLonIdx = (lonN + 1) * lonRegionIdx;
			stopLonIdx = (lonN + 1) * lonRegionIdx + lonN;
		} else {
			startLonIdx = lonN * lonRegionIdx + lonq;
			stopLonIdx = lonN * lonRegionIdx + lonq + lonN - 1;
		}

		for (latRegionIdx = 0; latRegionIdx < numRegionLat; latRegionIdx++) {
			if (latRegionIdx < latq) {
				startLatIdx = (latN + 1) * latRegionIdx;
				stopLatIdx = (latN + 1) * latRegionIdx + latN;
			} else {
				startLatIdx = latN * latRegionIdx + latq;
				stopLatIdx = latN * latRegionIdx + latq + latN - 1;
			}

			/**************************************************************************************/
			/* Create PPM File */
			/**************************************************************************************/
			FILE *fppm;
			if (!(fppm = fopen("/tmp/image.ppm", "wb"))) {
				throw std::runtime_error("ERROR");
			}
			fprintf(fppm, "P3\n");
			fprintf(fppm,
				"%d %d %d\n",
				(stopLonIdx - startLonIdx + 1) * interpolationFactor,
				(stopLatIdx - startLatIdx + 1) * interpolationFactor,
				255);

			int latIdx, lonIdx;
			for (latIdx = stopLatIdx; latIdx >= startLatIdx; --latIdx) {
				double latDeg = latitudeDegStart + (latIdx + 0.5) * resolutionLon;
				for (interpLat = interpolationFactor - 1; interpLat >= 0;
				     --interpLat) {
					for (lonIdx = startLonIdx; lonIdx <= stopLonIdx; ++lonIdx) {
						double lonDeg = longitudeDegStart +
								(lonIdx + 0.5) * resolutionLon;

						unsigned int landcat =
							(unsigned int)cgNlcd->valueAt(latDeg,
										      lonDeg);

						// printf("LON = %.6f LAT = %.6f LANDCAT = %d\n",
						// lonDeg, latDeg, landcat);

						std::string colorStr = colorList[landcat];

						for (interpLon = 0; interpLon < interpolationFactor;
						     interpLon++) {
							if (lonIdx || interpLon) {
								fprintf(fppm, " ");
							}
							fprintf(fppm, "%s", colorStr.c_str());
						}
					}
					fprintf(fppm, "\n");
				}
			}

			fclose(fppm);
			/**************************************************************************************/

			std::string pngFile = std::string("/tmp/image") + "_" +
					      std::to_string(lonRegionIdx) + "_" +
					      std::to_string(latRegionIdx) + ".png";

			imageFileList.push_back(pngFile);

			std::string command = "convert /tmp/image.ppm -transparent white " +
					      pngFile;
			std::cout << "COMMAND: " << command << std::endl;
			system(command.c_str());

			/**************************************************************************************/
			/* Write to KML File */
			/**************************************************************************************/
			fprintf(fkml, "<GroundOverlay>\n");
			fprintf(fkml,
				"    <name>Region: %d_%d</name>\n",
				lonRegionIdx,
				latRegionIdx);
			fprintf(fkml, "    <visibility>%d</visibility>\n", 1);
			fprintf(fkml, "    <color>C0ffffff</color>\n");
			fprintf(fkml, "    <Icon>\n");
			fprintf(fkml,
				"        <href>image_%d_%d.png</href>\n",
				lonRegionIdx,
				latRegionIdx);
			fprintf(fkml, "    </Icon>\n");
			fprintf(fkml, "    <LatLonBox>\n");
			fprintf(fkml,
				"        <north>%.8f</north>\n",
				latitudeDegStart + (stopLatIdx + 1) * resolutionLat);
			fprintf(fkml,
				"        <south>%.8f</south>\n",
				latitudeDegStart + (startLatIdx)*resolutionLat);
			fprintf(fkml,
				"        <east>%.8f</east>\n",
				longitudeDegStart + (stopLonIdx + 1) * resolutionLon);
			fprintf(fkml,
				"        <west>%.8f</west>\n",
				longitudeDegStart + (startLonIdx)*resolutionLon);
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

		for (int imageIdx = 0; imageIdx < ((int)imageFileList.size()); imageIdx++) {
			command += " " + imageFileList[imageIdx];
		}
		std::cout << "COMMAND: " << command.c_str() << std::endl;
		system(command.c_str());
	}
	/**************************************************************************************/

	delete _popGrid;

	_popGrid = (PopGridClass *)NULL;
}
/******************************************************************************************/
#endif
