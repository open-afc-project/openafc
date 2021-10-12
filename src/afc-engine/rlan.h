/******************************************************************************************/
/**** FILE : rlan.h                                                                      ****/
/******************************************************************************************/

#ifndef RLAN_H
#define RLAN_H

#include "cconst.h"
#include "Vector3.h"

template<class T> class ListClass;
class AntennaClass;
class SplineClass;
class ULSClass;

/******************************************************************************************/
/**** CLASS: RLANClass                                                                 ****/
/******************************************************************************************/
class RLANClass
{
public:
    RLANClass(int idVal);
    ~RLANClass();
    void setPosition(Vector3 p);
    void setLatitudeDeg(double latitudeDegVal);
    void setLongitudeDeg(double longitudeDegVal);
    void setHeight(double heightVal);
    void setPropEnv(CConst::PropEnvEnum pe);
    void setRegion(int regionIdxVal);
    void setUserType(CConst::UserTypeEnum ut);
    void setMaxEirpDBW(double eirpDBWVal);
    void setOffTune(bool offTuneVal);
    static void setNoiseLevelDBW(double noiseLevelDBWVal);

    int getID();
    Vector3 getPosition();
    double getLatitudeDeg();
    double getLongitudeDeg();
    double getHeight();
    CConst::PropEnvEnum getPropEnv();
    int getRegion();
    double getStartFreq();
    double getStopFreq();
    double getCenterFreq();
    int getNumFSVisible();
    bool getOffTune();
    static double getNoiseLevelDBW();

    void incrementNumFSVisible();

    static CConst::LengthUnitEnum antHeightUnit;
    static double strictTXPsdDB;
    static CConst::PSDDBUnitEnum strictTXPsdDBUnit;
    static double relaxedTXPsdDB;
    static CConst::PSDDBUnitEnum relaxedTXPsdDBUnit;
    static double cableLossDB;

    static int useEirpPatternFile;
    static bool clipRLANOutdoorEIRPFlag;

    static double noiseLevelDBW;

private:
    int id;
    Vector3 position;
    double latitudeDeg;
    double longitudeDeg;
    double height;
    CConst::PropEnvEnum propEnv;
    CConst::UserTypeEnum userType;
    CConst::AntennaModelEnum antennaModel;

    double startFreq;
    double stopFreq;
    double centerFreq;
    double maxEirpDBW;
    double orientPhiRad;
    Vector3 pointingVec;
    int numFSVisible;
    bool offTune;
    int regionIdx;
};
/******************************************************************************************/

#endif
