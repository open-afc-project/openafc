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

template<class T> class ListClass;

/******************************************************************************************/
/**** CLASS: ULSClass                                                                  ****/
/******************************************************************************************/
class ULSClass
{
public:
    ULSClass(AfcManager *dataSetVal, int idVal, int dbIdx);
    ~ULSClass();

    int getID() const;
    int getDBIdx() const;
    CConst::ULSTypeEnum getType();
    ListClass<Vector3> *getSatellitePositionData();
    double getStartAllocFreq();
    double getStopAllocFreq();
    double getStartUseFreq();
    double getStopUseFreq();
    double getBandwidth();
    std::string getRadioService();
    std::string getEntityName();
    std::string getCallsign();
    std::string getRxCallsign();
    int getRxAntennaNumber();
    bool getHasPR();
    Vector3 getRxPosition();
    Vector3 getTxPosition();
    Vector3 getPRPosition();
    Vector3 getAntennaPointing();

    double getRxLatitudeDeg();
    double getRxLongitudeDeg();
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

    double getPRLatitudeDeg();
    double getPRLongitudeDeg();
    double getPRGroundElevation();
    double getPRTerrainHeight();
    double getPRHeightAboveTerrain();
    double getPRHeightAMSL();
    CConst::HeightSourceEnum getPRHeightSource();

    std::string getTxPolarization();
    double getTxSrtmHeight();
    double getTxCenterToRAATHeight();
    double getNoiseLevelDBW();
    double getRxGain();
    double getRxAntennaFeederLossDB();
    double getFadeMarginDB();
    std::string getStatus();
    double computeBeamWidth(double attnDB);
    double computeRxGain(double angleOffBoresightDeg, double elevationAngleDeg, double frequency);
    CConst::ULSAntennaTypeEnum getRxAntennaType();
    CConst::ULSAntennaTypeEnum getTxAntennaType();
    AntennaClass *getRxAntenna();
    AntennaClass *getTxAntenna();
    double getTxGain();
    double getTxEIRP();
    double getLinkDistance();
    double getOperatingRadius();
    double getRxSensitivity();
    double getOperatingCenterLongitudeDeg();
    double getOperatingCenterLatitudeDeg();
    double getPropLoss();
    int getPairIdx();
    int getRxLidarRegion();
    int getTxLidarRegion();
    int getPRLidarRegion();
    bool getRxTerrainHeightFlag();
    bool getTxTerrainHeightFlag();
    bool getPRTerrainHeightFlag();
    int getNumOutOfBandRLAN();

    void setStartAllocFreq(double f);
    void setStopAllocFreq(double f);
    void setStartUseFreq(double f);
    void setStopUseFreq(double f);
    void setBandwidth(double b);
    void setRadioService(std::string radioServiceVal);
    void setEntityName(std::string entityNameVal);
    void setCallsign(std::string callsignVal);

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

    void setHasPR(bool hasPRVal);

    void setPRLatitudeDeg(double prLatitudeDegVal);
    void setPRLongitudeDeg(double prLongitudeDegVal);
    void setPRTerrainHeight(double prTerrainHeightVal);
    void setPRHeightAboveTerrain(double prHeightAboveTerrainVal);
    void setPRHeightAMSL(double prHeightAMSLVal);
    void setPRHeightSource(CConst::HeightSourceEnum prHeightSourceVal);

    void setUseFrequency();
    void setNoiseLevelDBW(double noiseLevelDBWVal);
    void setRxGain(double rxGainVal);
    void setRxAntennaFeederLossDB(double rxAntennaFeederLossDBVal);
    void setFadeMarginDB(double fadeMarginDBVal);
    void setStatus(std::string statusVal);
    void setRxAntennaType(CConst::ULSAntennaTypeEnum rxAntennaTypeVal);
    void setTxAntennaType(CConst::ULSAntennaTypeEnum txAntennaTypeVal);
    void setRxAntenna(AntennaClass *rxAntennaVal);
    void setTxAntenna(AntennaClass *txAntennaVal);
    void setTxGain(double txGainVal);
    void setTxEIRP(double txEIRPVal);
    void setLinkDistance(double linkDistanceVal);
    void setOperatingRadius(double operatingRadiusVal);
    void setRxSensitivity(double rxSensitivityVal);
    void setMobileUnit(int mobileUnitVal);
    void setOperatingCenterLongitudeDeg(double operatingCenterLongitudeDegVal);
    void setOperatingCenterLatitudeDeg(double operatingCenterLatitudeDegVal);
    void setPropLoss(double propLossVal);
    void setPairIdx(int pairIdxVal);
    void setRxLidarRegion(int lidarRegionVal);
    void setTxLidarRegion(int lidarRegionVal);
    void setPRLidarRegion(int lidarRegionVal);
    void setRxTerrainHeightFlag(bool terrainHeightFlagVal);
    void setTxTerrainHeightFlag(bool terrainHeightFlagVal);
    void setPRTerrainHeightFlag(bool terrainHeightFlagVal);
    void setNumOutOfBandRLAN(int numOutOfBandRLANVal);

    void setRxPosition(Vector3 p);
    void setTxPosition(Vector3 p);
    void setPRPosition(Vector3 p);
    void setAntennaPointing(Vector3 p);
    void setType(CConst::ULSTypeEnum typeVal);
    void setSatellitePositionData(ListClass<Vector3> *spd);

    void clearData();

    static const int numPtsPDF = 1000;
    static double azPointing, elPointing;
    static CConst::AngleUnitEnum azPointingUnit, elPointingUnit;
    static GdalDataDir *gdalDir;
    static WorldData *globeModel;
    static CConst::PathLossModelEnum pathLossModel;

    char *location;
    double *ITMHeightProfile;

#if DEBUG_AFC
    std::vector<std::string> ITMHeightType;
#endif

private:
    AfcManager *dataSet;

    int id;
    int dbIdx;
    double startAllocFreq;
    double stopAllocFreq;
    double startUseFreq;
    double stopUseFreq;
    double bandwidth;
    bool hasPR;
    std::string callsign;
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
    std::string txPolarization;
    double txCenterToRAATHeight;
    int txLidarRegion;
    bool txTerrainHeightFlag;
    double noiseLevelDBW;
    double txGain, rxGain;
    double txEIRP;
    double linkDistance;
    double operatingRadius;
    double rxSensitivity;
    int mobileUnit; // 0 = RX, 1 = TX
    double operatingCenterLongitudeDeg;
    double operatingCenterLatitudeDeg;
    double propLoss;

    double prLatitudeDeg;
    double prLongitudeDeg;
    double prGroundElevation;
    double prTerrainHeight;
    double prHeightAboveTerrain;
    double prHeightAMSL;
    double thresholdPFD;
    CConst::HeightSourceEnum prHeightSource;
    int prLidarRegion;
    bool prTerrainHeightFlag;

    double minPathLossDB, maxPathLossDB;
    Vector3 txPosition, rxPosition, prPosition;
    Vector3 antennaPointing;
    double antHeight;
    CConst::ULSTypeEnum type;

    ListClass<Vector3> *satellitePosnData;
    PopGridClass *mobilePopGrid; // Pop grid for mobile simulation
    CConst::ULSAntennaTypeEnum rxAntennaType;
    CConst::ULSAntennaTypeEnum txAntennaType;
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
