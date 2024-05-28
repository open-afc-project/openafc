/******************************************************************************************/
/**** FILE : uls.h                                                                     ****/
/******************************************************************************************/

#ifndef ULS_H
#define ULS_H

#include "Vector3.h"
#include "cconst.h"
#include "pop_grid.h"

class GdalDataDir;
class WorldData;
class AfcManager;
class AntennaClass;

template<class T>
class ListClass;

/******************************************************************************************/
/**** CLASS: PRClass - Passive Repeater                                                ****/
/******************************************************************************************/
class PRClass
{
    public:
        PRClass();
        ~PRClass();

        double computeDiscriminationGain(
            double angleOffBoresightDeg, double elevationAngleDeg, double frequency, double &reflectorD0, double &reflectorD1);

        // Path segment gain as defined in R2-AIP-31
        double pathSegGain;
        double effectiveGain;

        CConst::PRTypeEnum type;

        double latitudeDeg;
        double longitudeDeg;
        double heightAboveTerrainRx;
        double heightAboveTerrainTx;

        double terrainHeight;
        double heightAMSLRx;
        double heightAMSLTx;
        CConst::HeightSourceEnum heightSource;
        int lidarRegion;
        bool terrainHeightFlag;
        Vector3 positionRx;
        Vector3 positionTx;
        Vector3 pointing;
        double segmentDistance;

        double txGain;
        double txDlambda;
        double rxGain;
        double rxDlambda;
        CConst::AntennaCategoryEnum antCategory;
        std::string antModel;
        CConst::ULSAntennaTypeEnum antennaType;
        AntennaClass *antenna;

        double reflectorHeightLambda;
        double reflectorWidthLambda;

        // Reflector 3D coordinate system:
        // X: horizontal vector on reflector surface in direction of width
        // Y: vector on reflector surface in direction of height.  Note that when reflector
        // is tilted, this is not vertical relative to the ground. X: vector perpendicular
        // to reflector surface X, Y, Z are orthonormal basis
        Vector3 reflectorX, reflectorY, reflectorZ;

        double reflectorThetaIN; // Inclusion angle between incident and reflected waves at
                                 // reflector is 2*thetaIN
        double reflectorKS;
        double reflectorQ;
        // double reflectorAlphaEL;  // Inclusion angle in elevation plane is 2*alphaEL
        // (relative to reflector orthonormal basis) double reflectorAlphaAZ;  // Inclusion
        // angle in azimuthal plane is 2*alphaAZ (relative to reflector orthonormal basis)

        double reflectorSLambda; // (s/lambda) used in calculation of discrimination gain
        double reflectorTheta1; // Theta1 used in calculation of discrimination gain
};
/******************************************************************************************/

/******************************************************************************************/
/**** CLASS: ULSClass                                                                  ****/
/******************************************************************************************/
class ULSClass
{
    public:
        ULSClass(AfcManager *dataSetVal, int idVal, int dbIdx, int numPRVal, std::string regionVal);
        ~ULSClass();

        int getID() const;
        int getDBIdx() const;
        std::string getRegion() const;
        CConst::ULSTypeEnum getType();
        ListClass<Vector3> *getSatellitePositionData();
        double getStartFreq();
        double getStopFreq();
        double getNoiseBandwidth();
        std::string getRadioService();
        std::string getEntityName();
        std::string getCallsign();
        int getPathNumber();
        std::string getRxCallsign();
        int getRxAntennaNumber();
        int getNumPR();
        PRClass &getPR(int prIdx)
        {
            return prList[prIdx];
        }
        Vector3 getRxPosition();
        Vector3 getTxPosition();
        Vector3 getAntennaPointing();

        double getRxLatitudeDeg() const;
        double getRxLongitudeDeg() const;
        double getRxGroundElevation();
        double getRxTerrainHeight();
        double getRxHeightAboveTerrain();
        double getRxHeightAMSL();
        CConst::HeightSourceEnum getRxHeightSource();

        double getTxLatitudeDeg();
        double getTxLongitudeDeg();
        double getTxGroundElevation();
        double getTxTerrainHeight();
        double getTxHeightAboveTerrain();
        double getTxHeightAMSL();
        CConst::HeightSourceEnum getTxHeightSource();
        double getAzimuthAngleToTx();
        double getElevationAngleToTx();

        std::string getTxPolarization();
        double getTxSrtmHeight();
        double getTxCenterToRAATHeight();
        double getNoiseLevelDBW();
        double getRxGain();
        double getRxDlambda();
        double getRxNearFieldAntDiameter();
        double getRxNearFieldDistLimit();
        double getRxNearFieldAntEfficiency();
        CConst::AntennaCategoryEnum getRxAntennaCategory();
        double getRxAntennaFeederLossDB();
        double getFadeMarginDB();
        std::string getStatus();
        double computeBeamWidth(double attnDB);
        double computeRxGain(double angleOffBoresightDeg, double elevationAngleDeg, double frequency, std::string &subModelStr, int divIdx);

        static double calcR2AIP07Antenna(double angleOffBoresightDeg, double frequency, std::string antennaModel,
            CConst::AntennaCategoryEnum category, std::string &subModelStr, int divIdx, double maxGain, double Dlambda);
        static double calcR2AIP07CANAntenna(double angleOffBoresightDeg, double frequency, std::string antennaModel,
            CConst::AntennaCategoryEnum category, std::string &subModelStr, int divIdx, double maxGain, double Dlambda);

        std::string getRxAntennaModel()
        {
            return rxAntennaModel;
        }
        CConst::ULSAntennaTypeEnum getRxAntennaType();
        CConst::ULSAntennaTypeEnum getTxAntennaType();
        AntennaClass *getRxAntenna();
        AntennaClass *getTxAntenna();
        double getTxGain();
        double getTxEIRP();
        double getLinkDistance();
        double getPropLoss();
        int getPairIdx();
        int getRxLidarRegion();
        int getTxLidarRegion();
        bool getRxTerrainHeightFlag();
        bool getTxTerrainHeightFlag();
        int getNumOutOfBandRLAN();

        bool getHasDiversity()
        {
            return hasDiversity;
        }
        double getDiversityGain()
        {
            return diversityGain;
        }
        double getDiversityDlambda()
        {
            return diversityDlambda;
        }
        double getDiversityHeightAboveTerrain()
        {
            return diversityHeightAboveTerrain;
        }
        double getDiversityHeightAMSL()
        {
            return diversityHeightAMSL;
        }
        Vector3 getDiversityPosition()
        {
            return diversityPosition;
        }
        Vector3 getDiversityAntennaPointing()
        {
            return diversityAntennaPointing;
        }

        void setStartFreq(double f);
        void setStopFreq(double f);
        void setNoiseBandwidth(double b);
        void setRadioService(std::string radioServiceVal);
        void setEntityName(std::string entityNameVal);
        void setCallsign(std::string callsignVal);
        void setPathNumber(int pathNumberVal);

        void setRxCallsign(std::string rxCallsignVal);
        void setRxAntennaNumber(int rxAntennaNumberVal);
        void setRxLatitudeDeg(double latitudeDegVal);
        void setRxLongitudeDeg(double longitudeDegVal);
        void setRxGroundElevation(double rxGroundElevationVal);
        void setRxTerrainHeight(double rxTerrainHeightVal);
        void setRxHeightAboveTerrain(double rxHeightAboveTerrainVal);
        void setRxHeightAMSL(double rxHeightAMSLVal);
        void setRxHeightSource(CConst::HeightSourceEnum rxHeightSourceVal);

        void setTxLatitudeDeg(double latitudeDegVal);
        void setTxLongitudeDeg(double longitudeDegVal);
        void setTxGroundElevation(double txGroundElevationVal);
        void setTxTerrainHeight(double txTerrainHeightVal);
        void setTxHeightAboveTerrain(double txHeightAboveTerrainVal);
        void setTxHeightAMSL(double txHeightAMSLVal);
        void setTxHeightSource(CConst::HeightSourceEnum txHeightSourceVal);
        void setTxPolarization(std::string txPolarizationVal);
        void setAzimuthAngleToTx(double azimuthAngleToTxVal);
        void setElevationAngleToTx(double elevationAngleToTxVal);

        void setNoiseLevelDBW(double noiseLevelDBWVal);
        void setRxGain(double rxGainVal);
        void setRxDlambda(double rxDlambdaVal);
        void setRxNearFieldAntDiameter(double rxNearFieldAntDiameterVal);
        void setRxNearFieldDistLimit(double rxNearFieldDistLimitVal);
        void setRxNearFieldAntEfficiency(double rxNearFieldAntEfficiencyVal);
        void setRxAntennaCategory(CConst::AntennaCategoryEnum rxAntennaCategoryVal);
        void setRxAntennaFeederLossDB(double rxAntennaFeederLossDBVal);
        void setFadeMarginDB(double fadeMarginDBVal);
        void setStatus(std::string statusVal);
        void setRxAntennaModel(std::string rxAntennaModelVal)
        {
            rxAntennaModel = rxAntennaModelVal;
            return;
        }
        void setRxAntennaType(CConst::ULSAntennaTypeEnum rxAntennaTypeVal);
        void setTxAntennaType(CConst::ULSAntennaTypeEnum txAntennaTypeVal);
        void setRxAntenna(AntennaClass *rxAntennaVal);
        void setTxAntenna(AntennaClass *txAntennaVal);
        void setTxGain(double txGainVal);
        void setTxEIRP(double txEIRPVal);
        void setLinkDistance(double linkDistanceVal);
        void setPropLoss(double propLossVal);
        void setPairIdx(int pairIdxVal);
        void setRxLidarRegion(int lidarRegionVal);
        void setTxLidarRegion(int lidarRegionVal);
        void setRxTerrainHeightFlag(bool terrainHeightFlagVal);
        void setTxTerrainHeightFlag(bool terrainHeightFlagVal);
        void setNumOutOfBandRLAN(int numOutOfBandRLANVal);

        void setRxPosition(Vector3 &p);
        void setTxPosition(Vector3 &p);
        void setAntennaPointing(Vector3 &p);
        void setType(CConst::ULSTypeEnum typeVal);
        void setSatellitePositionData(ListClass<Vector3> *spd);

        void setHasDiversity(bool hasDiversityVal)
        {
            hasDiversity = hasDiversityVal;
        }
        void setDiversityGain(double diversityGainVal)
        {
            diversityGain = diversityGainVal;
        }
        void setDiversityDlambda(double diversityDlambdaVal)
        {
            diversityDlambda = diversityDlambdaVal;
        }
        void setDiversityHeightAboveTerrain(double diversityHeightAboveTerrainVal)
        {
            diversityHeightAboveTerrain = diversityHeightAboveTerrainVal;
        }
        void setDiversityHeightAMSL(double diversityHeightAMSLVal)
        {
            diversityHeightAMSL = diversityHeightAMSLVal;
        }
        void setDiversityPosition(Vector3 &diversityPositionVal)
        {
            diversityPosition = diversityPositionVal;
        }
        void setDiversityAntennaPointing(Vector3 &diversityAntennaPointingVal)
        {
            diversityAntennaPointing = diversityAntennaPointingVal;
        }

        void clearData();

        static const int numPtsPDF = 1000;
        static double azPointing, elPointing;
        static CConst::AngleUnitEnum azPointingUnit, elPointingUnit;
        static GdalDataDir *gdalDir;
        static WorldData *globeModel;
        static CConst::PathLossModelEnum pathLossModel;

        char *location;
        double *ITMHeightProfile;
        double *isLOSHeightProfile;
        double isLOSSurfaceFrac;

#if DEBUG_AFC
        std::vector<std::string> ITMHeightType;
#endif

    private:
        AfcManager *dataSet;

        int id;
        int dbIdx;
        int numPR;
        std::string region;
        double startFreq;
        double stopFreq;
        double noiseBandwidth;
        std::string callsign;
        int pathNumber;
        std::string rxCallsign;
        int rxAntennaNumber;
        std::string radioService;
        std::string entityName;
        double rxLatitudeDeg;
        double rxLongitudeDeg;
        double rxTerrainHeight;
        double rxHeightAboveTerrain;
        double rxHeightAMSL;
        double rxGroundElevation;
        CConst::HeightSourceEnum rxHeightSource;
        int rxLidarRegion;
        bool rxTerrainHeightFlag;
        double txLatitudeDeg;
        double txLongitudeDeg;
        double txGroundElevation;
        double txTerrainHeight;
        double txHeightAboveTerrain;
        double txHeightAMSL;
        CConst::HeightSourceEnum txHeightSource;
        double azimuthAngleToTx;
        double elevationAngleToTx;
        std::string txPolarization;
        double txCenterToRAATHeight;
        int txLidarRegion;
        bool txTerrainHeightFlag;
        double noiseLevelDBW;
        double txGain, rxGain, rxDlambda;
        double rxNearFieldAntDiameter;
        double rxNearFieldDistLimit;
        double rxNearFieldAntEfficiency;
        CConst::AntennaCategoryEnum rxAntennaCategory;
        double txEIRP;
        double linkDistance;
        double propLoss;

        bool hasDiversity;
        double diversityGain;
        double diversityDlambda;
        double diversityHeightAboveTerrain;
        double diversityHeightAMSL;
        Vector3 diversityPosition;

        Vector3 diversityAntennaPointing;

        PRClass *prList;

        double minPathLossDB, maxPathLossDB;
        Vector3 txPosition, rxPosition;
        Vector3 antennaPointing;
        double antHeight;
        CConst::ULSTypeEnum type;

        ListClass<Vector3> *satellitePosnData;
        PopGridClass *mobilePopGrid; // Pop grid for mobile simulation
        CConst::ULSAntennaTypeEnum rxAntennaType;
        CConst::ULSAntennaTypeEnum txAntennaType;
        std::string rxAntennaModel;
        AntennaClass *rxAntenna;
        AntennaClass *txAntenna;
        double rxAntennaFeederLossDB;
        double fadeMarginDB;
        std::string status;
        int pairIdx;
        int numOutOfBandRLAN;
};
/******************************************************************************************/

#endif
