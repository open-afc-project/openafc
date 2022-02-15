/******************************************************************************************/
/**** FILE: cconst.cpp                                                                 ****/
/******************************************************************************************/

#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <cstdio>

#include "cconst.h"
#include "str_type.h"
#include "global_defines.h"

/******************************************************************************************/
/**** Static Constants                                                                 ****/
/******************************************************************************************/
const double CConst::c  = 2.99792458e8;
const double CConst::u0 = 4*M_PI*1.0e-7;
const double CConst::e0 = 1/(CConst::c * CConst::c * CConst::u0);
const double CConst::logTable[] = {0.0,
                                   log(2.0)/log(10.0),
                                   log(3.0)/log(10.0),
                                   log(4.0)/log(10.0),
                                   log(5.0)/log(10.0),
                                   log(6.0)/log(10.0),
                                   log(7.0)/log(10.0),
                                   log(8.0)/log(10.0),
                                   log(9.0)/log(10.0)};
const double CConst::earthRadius = 6378.137e3;
const double CConst::geoRadius = 42164.0e3;
const double CConst::boltzmannConstant = 1.3806488e-23;
const double CConst::T0                = 290;
const double CConst::atmPressurehPa    = 1013;
/******************************************************************************************/

/******************************************************************************************/
/**** Static StrTypeClass lists                                                        ****/
/******************************************************************************************/
const StrTypeClass CConst::strPathLossModelList[] = {
    {               CConst::unknownPathLossModel, "UNKNOWN"                   },
    {                  CConst::FSPLPathLossModel, "FSPL"                      },
    {               CConst::ITMBldgPathLossModel, "ITM with building data"    },
    { CConst::FCC6GHzReportAndOrderPathLossModel, "FCC 6GHz Report & Order"   },
    {         CConst::CoalitionOpt6PathLossModel, "ITM with no building data" },

    {                                    -1,  (char *) 0                      }
};

const StrTypeClass CConst::strWinner2LOSOptionList[] = {
    {          CConst::UnknownWinner2LOSOption, "UNKNOWN"                   },
    {         CConst::BldgDataWinner2LOSOption, "BLDG_DATA"                   },
    {         CConst::ForceLOSWinner2LOSOption, "FORCE_LOS"                   },
    {        CConst::ForceNLOSWinner2LOSOption, "FORCE_NLOS"                  },

    {                                    -1,  (char *) 0                      }
};


const StrTypeClass CConst::strPropEnvList[] = {
    {                CConst::unknownPropEnv, "UNKNOWN"       },
    {                  CConst::urbanPropEnv, "URBAN"         },
    {               CConst::suburbanPropEnv, "SUBURBAN"      },
    {                  CConst::ruralPropEnv, "RURAL"         },
    {                 CConst::barrenPropEnv, "BARREN"        },
    {                                    -1,  (char *) 0     }
};

const StrTypeClass CConst::strULSAntennaTypeList[] = {
    {      CConst::F1336OmniAntennaType, "F.1336 Omni"  },
    {          CConst::F1245AntennaType, "F.1245"  },
    {                                -1,  (char *) 0       }
};

const StrTypeClass CConst::strHeightSourceList[] = {
    {          CConst::unknownHeightSource, "UNKNOWN"        },
    {          CConst::globalHeightSource, "GLOBAL"          },
    {          CConst::depHeightSource, "3DEP"            },
    {          CConst::srtmHeightSource, "SRTM"              },
    {          CConst::lidarHeightSource, "LiDAR"            },
    {                                    -1,  (char *) 0     }
};
