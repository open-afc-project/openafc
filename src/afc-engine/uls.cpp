/******************************************************************************************/
/**** FILE : uls.cpp                                                                   ****/
/******************************************************************************************/

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <cstring>
#include <math.h>
#include <stdexcept>

#include <QPointF>

#include "uls.h"
#include "antenna.h"
#include "calcitu1245.h"
#include "calcitu699.h"
#include "calcitu1336_4.h"
#include "AfcManager.h"
#include "global_defines.h"
#include "local_defines.h"
#include "spline.h"
#include "list.h"
#include "UlsMeasurementAnalysis.h"
#include "EcefModel.h"

namespace
{
// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "ULSClass")
}

/******************************************************************************************/
/**** FUNCTION: ULSClass::ULSClass()                                                   ****/
/******************************************************************************************/
ULSClass::ULSClass(AfcManager *dataSetVal, int idVal, int dbIdxVal, int numPRVal, std::string regionVal) : dataSet(dataSetVal), id(idVal), dbIdx(dbIdxVal), numPR(numPRVal), region(regionVal)
{
	prList = new PRClass[numPR];

	location = (char *) NULL;
	ITMHeightProfile = (double *) NULL;
	isLOSHeightProfile = (double *) NULL;

	dataSet = (AfcManager *) NULL;

	startAllocFreq = quietNaN;
	stopAllocFreq = quietNaN;
	startUseFreq = quietNaN;
	stopUseFreq = quietNaN;
	bandwidth = quietNaN;
	rxAntennaNumber = -1;
	rxLatitudeDeg = quietNaN;
	rxLongitudeDeg = quietNaN;
	rxTerrainHeight = quietNaN;
	rxHeightAboveTerrain = quietNaN;
	rxHeightAMSL = quietNaN;
	rxGroundElevation = quietNaN;
	rxHeightSource = CConst::unknownHeightSource;
	rxLidarRegion = -1;
	rxTerrainHeightFlag = false;
	txLatitudeDeg = quietNaN;
	txLongitudeDeg = quietNaN;
	txGroundElevation = quietNaN;
	txTerrainHeight = quietNaN;
	txHeightAboveTerrain = quietNaN;
	txHeightAMSL = quietNaN;
	txHeightSource = CConst::unknownHeightSource;
	txCenterToRAATHeight = quietNaN;
	txTerrainHeightFlag = false;
	noiseLevelDBW = quietNaN;
	txGain = quietNaN;
	rxGain = quietNaN;
	rxDlambda = quietNaN;
	rxNearFieldAntDiameter = quietNaN;
	rxNearFieldDistLimit = quietNaN;
	rxNearFieldAntEfficiency = quietNaN;
	rxAntennaCategory = CConst::UnknownAntennaCategory;
	txEIRP = quietNaN;
	linkDistance = quietNaN;
	operatingRadius = quietNaN;
	rxSensitivity = quietNaN;
	mobileUnit = -1;
	operatingCenterLongitudeDeg = quietNaN;
	operatingCenterLatitudeDeg = quietNaN;
	propLoss = quietNaN;

	hasDiversity = false;
	diversityGain = quietNaN;
	diversityDlambda = quietNaN;
	diversityHeightAboveTerrain = quietNaN;
	diversityHeightAMSL = quietNaN;

	minPathLossDB = quietNaN;
	maxPathLossDB = quietNaN;
	antHeight = quietNaN;
	type = CConst::ESULSType;

	satellitePosnData = (ListClass<Vector3> *) NULL;
	mobilePopGrid = (PopGridClass *) NULL;
	rxAntennaType = CConst::UnknownAntennaType;
	txAntennaType = CConst::UnknownAntennaType;
	rxAntenna = (AntennaClass *) NULL;
	txAntenna = (AntennaClass *) NULL;
	rxAntennaFeederLossDB = quietNaN;
	fadeMarginDB = quietNaN;
	pairIdx = -1;
	numOutOfBandRLAN = 0;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ULSClass::~ULSClass()                                                  ****/
/******************************************************************************************/
ULSClass::~ULSClass()
{
	clearData();

	delete [] prList;
}
/******************************************************************************************/

/******************************************************************************************/
/**** ULSClass:: static members                                                        ****/
/******************************************************************************************/
double ULSClass::azPointing = 0.0;
CConst::AngleUnitEnum ULSClass::azPointingUnit = CConst::degreeAngleUnit;

double ULSClass::elPointing = 3.0*M_PI/180.0;
CConst::AngleUnitEnum ULSClass::elPointingUnit = CConst::degreeAngleUnit;

CConst::PathLossModelEnum ULSClass::pathLossModel = CConst::unknownPathLossModel;
/******************************************************************************************/

/******************************************************************************************/
/**** ULSClass:: SET/GET Functions                                                     ****/
/******************************************************************************************/
int ULSClass::getID() const {
	return(id);
}
int ULSClass::getDBIdx() const {
	return(dbIdx);
}
std::string ULSClass::getRegion() const {
	return(region);
}
Vector3 ULSClass::getRxPosition() {
	return(rxPosition);
}
Vector3 ULSClass::getTxPosition() {
	return(txPosition);
}
Vector3 ULSClass::getAntennaPointing() {
	return(antennaPointing);
}
CConst::ULSTypeEnum ULSClass::getType() {
	return(type);
}
ListClass<Vector3> *ULSClass::getSatellitePositionData() {
	return(satellitePosnData);
}
double ULSClass::getStartAllocFreq() {
	return(startAllocFreq);
}
double ULSClass::getStopAllocFreq() {
	return(stopAllocFreq);
}
double ULSClass::getStartUseFreq() {
	return(startUseFreq);
}
double ULSClass::getStopUseFreq() {
	return(stopUseFreq);
}
double ULSClass::getBandwidth() {
	return(bandwidth);
}
int ULSClass::getNumPR() {
	return(numPR);
}
std::string ULSClass::getRadioService() {
	return(radioService);
}
std::string ULSClass::getEntityName() {
	return(entityName);
}
std::string ULSClass::getCallsign() {
	return(callsign);
}
std::string ULSClass::getRxCallsign() {
	return(rxCallsign);
}
int ULSClass::getRxAntennaNumber() {
	return(rxAntennaNumber);
}
double ULSClass::getRxLongitudeDeg() const {
	return(rxLongitudeDeg);
}
double ULSClass::getRxLatitudeDeg() const {
	return(rxLatitudeDeg);
}
double ULSClass::getRxGroundElevation()      {
	return(rxGroundElevation);
}
double                   ULSClass::getRxTerrainHeight()       {
	return(rxTerrainHeight);
}
double                   ULSClass::getRxHeightAboveTerrain()  {
	return(rxHeightAboveTerrain);
}
double                   ULSClass::getRxHeightAMSL()          {
	return(rxHeightAMSL);
}
CConst::HeightSourceEnum ULSClass::getRxHeightSource()        {
	return(rxHeightSource);
}
double ULSClass::getTxLongitudeDeg() {
	return(txLongitudeDeg);
}
double ULSClass::getTxLatitudeDeg() {
	return(txLatitudeDeg);
}
std::string ULSClass::getTxPolarization()      {
	return(txPolarization);
}
double ULSClass::getTxGroundElevation()      {
	return(txGroundElevation);
}
double                   ULSClass::getTxTerrainHeight()       {
	return(txTerrainHeight);
}
double                   ULSClass::getTxHeightAboveTerrain()  {
	return(txHeightAboveTerrain);
}
double                   ULSClass::getTxHeightAMSL()          {
	return(txHeightAMSL);
}
CConst::HeightSourceEnum ULSClass::getTxHeightSource()        {
	return(txHeightSource);
}
double                   ULSClass::getAzimuthAngleToTx()          {
	return(azimuthAngleToTx);
}
double                   ULSClass::getElevationAngleToTx()          {
	return(elevationAngleToTx);
}


double ULSClass::getNoiseLevelDBW() {
	return(noiseLevelDBW);
}
double ULSClass::getRxGain()        {
	return(rxGain);
}
double ULSClass::getRxDlambda()        {
	return(rxDlambda);
}
double ULSClass::getRxNearFieldAntDiameter() {
	return(rxNearFieldAntDiameter);
}
double ULSClass::getRxNearFieldDistLimit() {
	return(rxNearFieldDistLimit);
}
double ULSClass::getRxNearFieldAntEfficiency() {
	return(rxNearFieldAntEfficiency);
}
CConst::AntennaCategoryEnum ULSClass::getRxAntennaCategory()        {
	return(rxAntennaCategory);
}
double ULSClass::getRxAntennaFeederLossDB() {
	return(rxAntennaFeederLossDB);
}
double ULSClass::getFadeMarginDB() {
	return(fadeMarginDB);
}
std::string ULSClass::getStatus() {
	return(status);
}
CConst::ULSAntennaTypeEnum ULSClass::getRxAntennaType() {
	return(rxAntennaType);
}
CConst::ULSAntennaTypeEnum ULSClass::getTxAntennaType() {
	return(txAntennaType);
}
AntennaClass *ULSClass::getRxAntenna() {
	return(rxAntenna);
}
AntennaClass *ULSClass::getTxAntenna() {
	return(txAntenna);
}
double ULSClass::getTxGain()        {
	return(txGain);
}
double ULSClass::getTxEIRP()        {
	return(txEIRP);
}
double ULSClass::getLinkDistance()  {
	return(linkDistance);
}
double ULSClass::getOperatingRadius()  {
	return(operatingRadius);
}
double ULSClass::getRxSensitivity()  {
	return(rxSensitivity);
}
double ULSClass::getOperatingCenterLongitudeDeg() {
	return(operatingCenterLongitudeDeg);
}
double ULSClass::getOperatingCenterLatitudeDeg() {
	return(operatingCenterLatitudeDeg);
}
double ULSClass::getPropLoss() {
	return(propLoss);
}
int ULSClass::getPairIdx() {
	return(pairIdx);
}
int ULSClass::getRxLidarRegion() {
	return(rxLidarRegion);
}
int ULSClass::getTxLidarRegion() {
	return(txLidarRegion);
}
bool ULSClass::getRxTerrainHeightFlag() {
	return(rxTerrainHeightFlag);
}
bool ULSClass::getTxTerrainHeightFlag() {
	return(txTerrainHeightFlag);
}
int ULSClass::getNumOutOfBandRLAN() {
	return(numOutOfBandRLAN);
}

void ULSClass::setSatellitePositionData(ListClass<Vector3> *spd) {
	satellitePosnData = spd;
	return;
}
void ULSClass::setRxPosition(Vector3 p) {
	rxPosition = p;
	return;
}
void ULSClass::setTxPosition(Vector3 p) {
	txPosition = p;
	return;
}
void ULSClass::setAntennaPointing(Vector3 p) {
	antennaPointing = p;
	return;
}
void ULSClass::setType(CConst::ULSTypeEnum typeVal) {
	type = typeVal;
	return;
}
void ULSClass::setStartAllocFreq(double f) {
	startAllocFreq = f;
	return;
}
void ULSClass::setStopAllocFreq(double f) {
	stopAllocFreq = f;
	return;
}
void ULSClass::setStartUseFreq(double f) {
	startUseFreq = f;
	return;
}
void ULSClass::setStopUseFreq(double f) {
	stopUseFreq = f;
	return;
}
void ULSClass::setBandwidth(double b) {
	bandwidth = b;
	return;
}
void ULSClass::setRadioService(std::string radioServiceVal) {
	radioService = radioServiceVal;
	return;
}
void ULSClass::setEntityName(std::string entityNameVal) {
	entityName = entityNameVal;
	return;
}
void ULSClass::setCallsign(std::string callsignVal) {
	callsign = callsignVal;
	return;
}
void ULSClass::setRxCallsign(std::string rxCallsignVal) {
	rxCallsign = rxCallsignVal;
	return;
}
void ULSClass::setRxAntennaNumber(int rxAntennaNumberVal) {
	rxAntennaNumber = rxAntennaNumberVal;
	return;
}
void ULSClass::setRxLatitudeDeg(double rxLatitudeDegVal) {
	rxLatitudeDeg = rxLatitudeDegVal;
	return;
}
void ULSClass::setRxLongitudeDeg(double rxLongitudeDegVal) {
	rxLongitudeDeg = rxLongitudeDegVal;
	return;
}
void ULSClass::setRxGroundElevation(double rxGroundElevationVal) {
	rxGroundElevation = rxGroundElevationVal;
	return;
}
void ULSClass::setRxTerrainHeight(double rxTerrainHeightVal) {
	rxTerrainHeight = rxTerrainHeightVal;
	return;
}
void ULSClass::setRxHeightAboveTerrain(double rxHeightAboveTerrainVal) {
	rxHeightAboveTerrain = rxHeightAboveTerrainVal;
	return;
}
void ULSClass::setRxHeightAMSL(double rxHeightAMSLVal) {
	rxHeightAMSL = rxHeightAMSLVal;
	return;
}
void ULSClass::setRxHeightSource(CConst::HeightSourceEnum rxHeightSourceVal) {
	rxHeightSource = rxHeightSourceVal;
	return;
}
void ULSClass::setTxLatitudeDeg(double txLatitudeDegVal) {
	txLatitudeDeg = txLatitudeDegVal;
	return;
}
void ULSClass::setTxLongitudeDeg(double txLongitudeDegVal) {
	txLongitudeDeg = txLongitudeDegVal;
	return;
}
void ULSClass::setTxPolarization(std::string txPolarizationVal) {
	txPolarization = txPolarizationVal;
	return;
}
void ULSClass::setTxGroundElevation(double txGroundElevationVal) {
	txGroundElevation = txGroundElevationVal;
	return;
}
void ULSClass::setTxTerrainHeight(double txTerrainHeightVal) {
	txTerrainHeight = txTerrainHeightVal;
	return;
}
void ULSClass::setTxHeightAboveTerrain(double txHeightAboveTerrainVal) {
	txHeightAboveTerrain = txHeightAboveTerrainVal;
	return;
}
void ULSClass::setTxHeightAMSL(double txHeightAMSLVal) {
	txHeightAMSL = txHeightAMSLVal;
	return;
}
void ULSClass::setTxHeightSource(CConst::HeightSourceEnum txHeightSourceVal) {
	txHeightSource = txHeightSourceVal;
	return;
}
void ULSClass::setAzimuthAngleToTx(double azimuthAngleToTxVal) {
	azimuthAngleToTx = azimuthAngleToTxVal;
	return;
}
void ULSClass::setElevationAngleToTx(double elevationAngleToTxVal) {
	elevationAngleToTx = elevationAngleToTxVal;
	return;
}
void ULSClass::setNoiseLevelDBW(double noiseLevelDBWVal) {
	noiseLevelDBW = noiseLevelDBWVal;
}
void ULSClass::setRxGain(double rxGainVal) {
	rxGain = rxGainVal;
	return;
}
void ULSClass::setRxDlambda(double rxDlambdaVal) {
	rxDlambda = rxDlambdaVal;
	return;
}
void ULSClass::setRxNearFieldAntDiameter(double rxNearFieldAntDiameterVal) {
	rxNearFieldAntDiameter = rxNearFieldAntDiameterVal;
	return;
}
void ULSClass::setRxNearFieldDistLimit(double rxNearFieldDistLimitVal) {
	rxNearFieldDistLimit = rxNearFieldDistLimitVal;
	return;
}
void ULSClass::setRxNearFieldAntEfficiency(double rxNearFieldAntEfficiencyVal) {
	rxNearFieldAntEfficiency = rxNearFieldAntEfficiencyVal;
	return;
}
void ULSClass::setRxAntennaCategory(CConst::AntennaCategoryEnum rxAntennaCategoryVal) {
	rxAntennaCategory = rxAntennaCategoryVal;
	return;
}
void ULSClass::setRxAntennaFeederLossDB(double rxAntennaFeederLossDBVal) {
	rxAntennaFeederLossDB = rxAntennaFeederLossDBVal;
	return;
}
void ULSClass::setFadeMarginDB(double fadeMarginDBVal) {
	fadeMarginDB = fadeMarginDBVal;
	return;
}
void ULSClass::setStatus(std::string statusVal) {
	status = statusVal;
	return;
}
void ULSClass::setRxAntennaType(CConst::ULSAntennaTypeEnum rxAntennaTypeVal) {
	rxAntennaType = rxAntennaTypeVal;
	return;
}
void ULSClass::setTxAntennaType(CConst::ULSAntennaTypeEnum txAntennaTypeVal) {
	txAntennaType = txAntennaTypeVal;
	return;
}
void ULSClass::setRxAntenna(AntennaClass *rxAntennaVal) {
	rxAntenna = rxAntennaVal;
	return;
}
void ULSClass::setTxAntenna(AntennaClass *txAntennaVal) {
	txAntenna = txAntennaVal;
	return;
}
void ULSClass::setTxGain(double txGainVal) {
	txGain = txGainVal;
	return;
}
void ULSClass::setTxEIRP(double txEIRPVal) {
	txEIRP = txEIRPVal;
	return;
}
void ULSClass::setLinkDistance(double linkDistanceVal) {
	linkDistance = linkDistanceVal;
	return;
}
void ULSClass::setOperatingRadius(double operatingRadiusVal) {
	operatingRadius = operatingRadiusVal;
	return;
}
void ULSClass::setRxSensitivity(double rxSensitivityVal) {
	rxSensitivity = rxSensitivityVal;
	return;
}
void ULSClass::setMobileUnit(int mobileUnitVal) {
	mobileUnit = mobileUnitVal;
	return;
}
void ULSClass::setOperatingCenterLongitudeDeg(double operatingCenterLongitudeDegVal) {
	operatingCenterLongitudeDeg = operatingCenterLongitudeDegVal;
	return;
}
void ULSClass::setOperatingCenterLatitudeDeg(double operatingCenterLatitudeDegVal) {
	operatingCenterLatitudeDeg = operatingCenterLatitudeDegVal;
	return;
}
void ULSClass::setPropLoss(double propLossVal) {
	propLoss = propLossVal;
	return;
}
void ULSClass::setPairIdx(int pairIdxVal) {
	pairIdx = pairIdxVal;
	return;
}
void ULSClass::setRxLidarRegion(int lidarRegionVal) {
	rxLidarRegion = lidarRegionVal;
	return;
}
void ULSClass::setTxLidarRegion(int lidarRegionVal) {
	txLidarRegion = lidarRegionVal;
	return;
}
void ULSClass::setRxTerrainHeightFlag(bool terrainHeightFlagVal) {
	rxTerrainHeightFlag = terrainHeightFlagVal;
	return;
}
void ULSClass::setTxTerrainHeightFlag(bool terrainHeightFlagVal) {
	txTerrainHeightFlag = terrainHeightFlagVal;
	return;
}
void ULSClass::setNumOutOfBandRLAN(int numOutOfBandRLANVal) {
	numOutOfBandRLAN = numOutOfBandRLANVal;
	return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ULSClass::clearData                                                    ****/
/******************************************************************************************/
void ULSClass::clearData()
{
	if (satellitePosnData) {
		delete satellitePosnData;
		satellitePosnData = (ListClass<Vector3> *) NULL;
	}

	if (location) {
		free(location);
		location = (char *) NULL;
	}
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ULSClass::setUseFrequency                                              ****/
/******************************************************************************************/
void ULSClass::setUseFrequency()
{
	std::ostringstream errStr;

	if (fabs(stopAllocFreq - startAllocFreq - bandwidth) < 1.0e-3) {
		startUseFreq = startAllocFreq;
		stopUseFreq = stopAllocFreq;
	} else if (stopAllocFreq - startAllocFreq > bandwidth) {
		if (1) {
			// std::cout << "ALLOC_BW_GT_BW: ID = " << getID() << " radioService = " << getRadioService()
			//           << " startAllocFreq = " << startAllocFreq << " stopAllocFreq = " << stopAllocFreq << " bandwidth = " << bandwidth << std::endl;

			// Randomly select channel
			int N = (int) floor((stopAllocFreq - startAllocFreq)/bandwidth);
			double r = ((double) rand() / (RAND_MAX+1.0));
			int k = (int) floor(r*N);
			startUseFreq = startAllocFreq + k*bandwidth;
			stopUseFreq = startUseFreq + bandwidth;
		} else if (0) {
			// Most conservative, aggregate interference over entire alloc band, consider noise only in bandwidth
			startUseFreq = startAllocFreq;
			stopUseFreq = stopAllocFreq;
		} else {
			// Aggregate interference over entire alloc band, consider noise over entire alloc band
			startUseFreq = startAllocFreq;
			stopUseFreq = stopAllocFreq;
			bandwidth = stopAllocFreq - startAllocFreq;
		}
	} else {
		errStr << "ERROR: Invalid frequency specification for Radio Service = " << radioService
		       << " startAllocFreq = " << startAllocFreq*1.0e-6
		       << " stopAllocFreq = "  << stopAllocFreq*1.0e-6
		       << " bandwidth = "      << bandwidth*1.0e-6
		       << std::endl;
		throw std::runtime_error(errStr.str());
	}
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ULSClass::computeRxGain                                                ****/
/******************************************************************************************/
double ULSClass::computeRxGain(double angleOffBoresightDeg, double elevationAngleDeg, double frequency, std::string &subModelStr, int divIdx)
{
	double rxGainDB;
	subModelStr = "";

	double maxGain = (divIdx == 0 ? rxGain    : diversityGain   );
	double Dlambda = (divIdx == 0 ? rxDlambda : diversityDlambda);

	switch(rxAntennaType) {
	case CConst::F1245AntennaType:
		rxGainDB = calcItu1245::CalcITU1245(angleOffBoresightDeg, maxGain, Dlambda);
		break;
	case CConst::F699AntennaType:
		rxGainDB = calcItu699::CalcITU699(angleOffBoresightDeg, maxGain, Dlambda);
		break;
	case CConst::F1336OmniAntennaType:
		rxGainDB = calcItu1336_4::CalcITU1336_omni_avg(elevationAngleDeg, maxGain, frequency);
		break;
	case CConst::R2AIP07AntennaType:
		rxGainDB = calcR2AIP07Antenna(angleOffBoresightDeg, frequency, rxAntennaModel, rxAntennaCategory, subModelStr, divIdx, maxGain, Dlambda);
		break;
	case CConst::OmniAntennaType:
		rxGainDB = 0.0;
		break;
	case CConst::LUTAntennaType:
		rxGainDB = rxAntenna->gainDB(angleOffBoresightDeg*M_PI/180.0) + rxGain;
		break;
	default:
		throw std::runtime_error(ErrStream() << "ERROR in ULSClass::computeRxGain: rxAntennaType = " << rxAntennaType << " INVALID value for FSID = " << id);
		break;
	}

	return(rxGainDB);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ULSClass::calcR2AIP07Antenna                                           ****/
/******************************************************************************************/
double ULSClass::calcR2AIP07Antenna(double angleOffBoresightDeg, double frequency, std::string antennaModel, CConst::AntennaCategoryEnum category,
	std::string &subModelStr, int divIdx, double maxGain, double Dlambda)
{
	// int freqIdx;
	double rxGainDB;

	if ((frequency >= 5925.0e6) && (frequency <= 6425.0e6)) {
		// freqIdx = 0;
	} else if ((frequency >= 6525.0e6) && (frequency <= 6875.0e6)) {
		// freqIdx = 1;
	} else {
		throw std::runtime_error(ErrStream() << "ERROR in ULSClass::calcR2AIP07Antenna: frequency = " << frequency << " INVALID value");
	}

	if (maxGain < 38) {
		if (angleOffBoresightDeg < 5) {
			subModelStr = ":F.699";
			rxGainDB = calcItu699::CalcITU699(angleOffBoresightDeg, maxGain, Dlambda);
		} else if (divIdx == 0) {
			// Table 2, Category B2
			double minSuppression;
			if (angleOffBoresightDeg < 10.0) {
				minSuppression = 15.0;
			} else if (angleOffBoresightDeg < 15.0) {
				minSuppression = 20.0;
			} else if (angleOffBoresightDeg < 20.0) {
				minSuppression = 23.0;
			} else if (angleOffBoresightDeg < 30.0) {
				minSuppression = 28.0;
			} else if (angleOffBoresightDeg < 100.0) {
				minSuppression = 29.0;
			} else {
				minSuppression = 60.0;
			}
			subModelStr = ":catB2";
			rxGainDB = maxGain - minSuppression;
		} else {
			// Table 2, Category B1
			double minSuppression;
			if (angleOffBoresightDeg < 10.0) {
				minSuppression = 21.0;
			} else if (angleOffBoresightDeg < 15.0) {
				minSuppression = 25.0;
			} else if (angleOffBoresightDeg < 20.0) {
				minSuppression = 29.0;
			} else if (angleOffBoresightDeg < 30.0) {
				minSuppression = 32.0;
			} else if (angleOffBoresightDeg < 100.0) {
				minSuppression = 35.0;
			} else if (angleOffBoresightDeg < 140.0) {
				minSuppression = 39.0;
			} else {
				minSuppression = 45.0;
			}
			subModelStr = ":catB1";
			rxGainDB = maxGain - minSuppression;
		}
	} else {
		if (angleOffBoresightDeg < 5) {
			subModelStr = ":F.699";
			rxGainDB = calcItu699::CalcITU699(angleOffBoresightDeg, maxGain, Dlambda);
		} else {
			bool antennaModelBlank = antennaModel.empty();
			bool categoryB1Flag = (category == CConst::B1AntennaCategory);
			bool knownHighPerformance = (category == CConst::HPAntennaCategory);

			if (antennaModelBlank || categoryB1Flag) {
				// Table 2, Category B1
				double minSuppression;
				if (angleOffBoresightDeg < 10.0) {
					minSuppression = 21.0;
				} else if (angleOffBoresightDeg < 15.0) {
					minSuppression = 25.0;
				} else if (angleOffBoresightDeg < 20.0) {
					minSuppression = 29.0;
				} else if (angleOffBoresightDeg < 30.0) {
					minSuppression = 32.0;
				} else if (angleOffBoresightDeg < 100.0) {
					minSuppression = 35.0;
				} else if (angleOffBoresightDeg < 140.0) {
					minSuppression = 39.0;
				} else {
					minSuppression = 45.0;
				}
				subModelStr = ":catB1";
				rxGainDB = maxGain - minSuppression;
			} else if (knownHighPerformance) {
				// Table 2, Category A
				double minSuppressionA;
				if (angleOffBoresightDeg < 10.0) {
					minSuppressionA = 25.0;
				} else if (angleOffBoresightDeg < 15.0) {
					minSuppressionA = 29.0;
				} else if (angleOffBoresightDeg < 20.0) {
					minSuppressionA = 33.0;
				} else if (angleOffBoresightDeg < 30.0) {
					minSuppressionA = 36.0;
				} else if (angleOffBoresightDeg < 100.0) {
					minSuppressionA = 42.0;
				} else {
					minSuppressionA = 55.0;
				}

				double descrimination699 = maxGain - calcItu699::CalcITU699(angleOffBoresightDeg, maxGain, Dlambda);

				double descriminationDB;
				if (descrimination699 >= minSuppressionA) {
					subModelStr = ":F.699";
					descriminationDB = descrimination699;
				} else {
					subModelStr = ":catA";
					descriminationDB = minSuppressionA;
				}

				rxGainDB = maxGain - descriminationDB;
			} else {
				// Table 2, Category A
				double minSuppressionA;
				if (angleOffBoresightDeg < 10.0) {
					minSuppressionA = 25.0;
				} else if (angleOffBoresightDeg < 15.0) {
					minSuppressionA = 29.0;
				} else if (angleOffBoresightDeg < 20.0) {
					minSuppressionA = 33.0;
				} else if (angleOffBoresightDeg < 30.0) {
					minSuppressionA = 36.0;
				} else if (angleOffBoresightDeg < 100.0) {
					minSuppressionA = 42.0;
				} else {
					minSuppressionA = 55.0;
				}

				subModelStr = ":catA";
				rxGainDB = maxGain - minSuppressionA;
			}
		}
	}

	return(rxGainDB);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ULSClass::computeBeamWidth                                             ****/
/******************************************************************************************/
double ULSClass::computeBeamWidth(double attnDB)
{
	double rxGainDB;
	std::ostringstream errStr;

	CConst::ULSAntennaTypeEnum ulsRxAntennaType = getRxAntennaType();

	double g0 = getRxGain();

	//std::cout << "ULS: " << getID() << " GAIN (DB) = " << g0 << std::endl;

	if (ulsRxAntennaType == CConst::F1336OmniAntennaType) {
		throw std::runtime_error(ErrStream() << "ERROR in ULSClass::computeBeamWidth: ulsRxAntennaType = F1336OmniAntennaType not supported");
	}

	double a1 = 0.0;
	double frequency = (startUseFreq + stopUseFreq)/2;

	double a2 = a1;
	double e2;
	do {
		if (a2 == 180.0) {
			errStr << "ERROR: Unable to compute " << attnDB << " dB beamwidth with GAIN (DB) = " << g0 << std::endl;
			throw std::runtime_error(errStr.str());
		}
		a2 += 2.0*exp(-g0*log(10.0)/20.0)*180.0/M_PI;
		if (a2 > 180.0) {
			a2 = 180.0;
		}
		double angleOffBoresightDeg = a2;

		std::string subModelStr;
		rxGainDB = computeRxGain(angleOffBoresightDeg, -1.0, frequency, subModelStr, 0);

		e2 = rxGainDB - g0 + attnDB;
	} while(e2 > 0.0);

	while (a2-a1 > 1.0e-8) {
		double a3 = (a1+a2)/2;
		double angleOffBoresightDeg = a3;

		std::string subModelStr;
		rxGainDB = computeRxGain(angleOffBoresightDeg, -1.0, frequency, subModelStr, 0);

		double e3 = rxGainDB - g0 + attnDB;

		if (e3 > 0.0) {
			a1 = a3;
		} else {
			a2 = a3;
			e2 = e3;
		}
	}

	double beamWidthDeg = a1;
	//std::cout << "ULS: " << getID() << " BEAMWIDTH (deg) = " << beamWidthDeg << std::endl;

	return(beamWidthDeg);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PRClass::PRClass()                                                     ****/
/******************************************************************************************/
PRClass::PRClass()
{
	pathSegGain = quietNaN;
	effectiveGain = quietNaN;

	type = CConst::unknownPRType;

	latitudeDeg = quietNaN;
	longitudeDeg = quietNaN;
	heightAboveTerrainRx = quietNaN;
	heightAboveTerrainTx = quietNaN;

	terrainHeight = quietNaN;
	heightAMSLRx = quietNaN;
	heightAMSLTx = quietNaN;
	heightSource = CConst::unknownHeightSource;
	lidarRegion = -1;
	terrainHeightFlag = false;
	positionRx = Vector3(quietNaN, quietNaN, quietNaN);
	positionTx = Vector3(quietNaN, quietNaN, quietNaN);
	pointing = Vector3(quietNaN, quietNaN, quietNaN);
	segmentDistance = quietNaN;

	txGain = quietNaN;
	txDlambda = quietNaN;
	rxGain = quietNaN;
	rxDlambda = quietNaN;
	antCategory = CConst::UnknownAntennaCategory;
	antModel = "";

	reflectorHeightLambda = quietNaN;
	reflectorWidthLambda = quietNaN;

	reflectorX = Vector3(quietNaN, quietNaN, quietNaN);
	reflectorY = Vector3(quietNaN, quietNaN, quietNaN);
	reflectorZ = Vector3(quietNaN, quietNaN, quietNaN);

	reflectorThetaIN = quietNaN;
	reflectorKS = quietNaN;
	reflectorQ = quietNaN;

	reflectorSLambda = quietNaN;
	reflectorTheta1 = quietNaN;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PRClass::~PRClass()                                                    ****/
/******************************************************************************************/
PRClass::~PRClass()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PRClass::computeDiscriminationGain                                     ****/
/******************************************************************************************/
double PRClass::computeDiscriminationGain(double angleOffBoresightDeg, double elevationAngleDeg, double frequency,
	double& reflectorD0, double& reflectorD1)
{
	double discriminationDB;

	switch(type) {
		case CConst::backToBackAntennaPRType:
			{
				std::string subModelStr;
				double rxGainDB = ULSClass::calcR2AIP07Antenna(angleOffBoresightDeg, frequency, antModel, antCategory, subModelStr, 0, rxGain, rxDlambda);
				discriminationDB = rxGainDB - rxGain;

				reflectorD0 = quietNaN;
				reflectorD1 = quietNaN;
			}
			break;
		case CConst::billboardReflectorPRType:
			{
				double D0 = -10.0*log10(4*M_PI*reflectorWidthLambda*reflectorHeightLambda*cos(reflectorThetaIN*M_PI/180.0));
				double D1;
				double u_over_PI = reflectorSLambda*sin(angleOffBoresightDeg*M_PI/180.0);

				if (angleOffBoresightDeg <= reflectorTheta1) {
					D1 = 20*log10(MathHelpers::sinc(u_over_PI));
				} else if (angleOffBoresightDeg <= 20.0) {
					D1 = -20*log10(fabs(M_PI*u_over_PI));
				} else {
					double u0_over_PI = reflectorSLambda*sin(20.0*M_PI/180.0);
					D1 = -20*log10(fabs(M_PI*u0_over_PI)) - 0.4165*(angleOffBoresightDeg - 20.0);
				}
				discriminationDB = std::max(D0, D1);

				reflectorD0 = D0;
				reflectorD1 = D1;
			}
			break;
		default:
			discriminationDB = quietNaN;
			CORE_DUMP;
			break;
	}

	return discriminationDB;
}
/******************************************************************************************/
