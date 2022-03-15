/******************************************************************************************/
/**** FILE : rlan.cpp                                                                  ****/
/******************************************************************************************/

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <cstring>
#include <math.h>
#include <stdexcept>

#include "rlan.h"
#include "uls.h"
#include "list.h"
#include "global_defines.h"
#include "local_defines.h"
#include "global_fn.h"
#include "spline.h"
#include "uls.h"

/******************************************************************************************/
/**** static members                                                                   ****/
/******************************************************************************************/
CConst::LengthUnitEnum RLANClass::antHeightUnit     = CConst::mLengthUnit;
double RLANClass::strictTXPsdDB      = -136.6206;              // dBW/Hz (-100.6 dBW/4 KHz = -136.6206 dBW/Hz)
CConst::PSDDBUnitEnum RLANClass::strictTXPsdDBUnit  = CConst::WPerHzPSDDBUnit;
double RLANClass::relaxedTXPsdDB = -103.0;                     // dBW/Hz (-43 dBW/MHz = -103 dBW/Hz)
CConst::PSDDBUnitEnum RLANClass::relaxedTXPsdDBUnit = CConst::WPerMHzPSDDBUnit;
double RLANClass::cableLossDB = 3.0;                           // AP Cable Loss

double RLANClass::noiseLevelDBW = 0.0;
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: RLANClass::RLANClass()                                                 ****/
/******************************************************************************************/
RLANClass::RLANClass(int idVal)
{
    propEnv = CConst::unknownPropEnv;
    id = idVal;

    numFSVisible = 0;
    offTune = true;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: RLANClass::~RLANClass()                                                ****/
/******************************************************************************************/
RLANClass::~RLANClass()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** RLANClass:: SET/GET Functions                                                    ****/
/******************************************************************************************/
int                      RLANClass::getID()            { return(id);            }
Vector3                  RLANClass::getPosition()      { return(position);      }
double                   RLANClass::getLatitudeDeg()   { return(latitudeDeg);   }
double                   RLANClass::getLongitudeDeg()  { return(longitudeDeg);  }
double                   RLANClass::getHeight()        { return(height);        }
CConst::PropEnvEnum      RLANClass::getPropEnv()       { return(propEnv);       }
int                      RLANClass::getRegion()        { return(regionIdx);     }
double                   RLANClass::getStartFreq()     { return(startFreq);     }
double                   RLANClass::getStopFreq()      { return(stopFreq);      }
double                   RLANClass::getCenterFreq()    { return(centerFreq);    }
int                      RLANClass::getNumFSVisible()  { return(numFSVisible);  }
bool                     RLANClass::getOffTune()       { return(offTune);       }
double                   RLANClass::getNoiseLevelDBW() { return(noiseLevelDBW); }


void RLANClass::setPosition(Vector3 p) { position = p; return; }
void RLANClass::setLatitudeDeg(double latitudeDegVal)   { latitudeDeg = latitudeDegVal; return; }
void RLANClass::setLongitudeDeg(double longitudeDegVal)   { longitudeDeg = longitudeDegVal; return; }
void RLANClass::setHeight(double heightVal) { height = heightVal;  return; }
void RLANClass::setPropEnv(CConst::PropEnvEnum pe) { propEnv = pe;  return; }
void RLANClass::setRegion(int regionIdxVal) { regionIdx = regionIdxVal;  return; }
void RLANClass::setUserType(CConst::UserTypeEnum ut) { userType = ut;  return; }
void RLANClass::setMaxEirpDBW(double maxEirpDBWVal)      { maxEirpDBW = maxEirpDBWVal; return;      }
void RLANClass::setOffTune(bool offTuneVal)              { offTune = offTuneVal; return;      }

void RLANClass::setNoiseLevelDBW(double noiseLevelDBWVal) { noiseLevelDBW = noiseLevelDBWVal; }
/******************************************************************************************/

/******************************************************************************************/
/**** RLANClass::incrementNumFSVisible()                                               ****/
/******************************************************************************************/
void RLANClass::incrementNumFSVisible()
{
    numFSVisible++;
}
/******************************************************************************************/

