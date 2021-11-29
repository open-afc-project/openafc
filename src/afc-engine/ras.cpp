/******************************************************************************************/
/**** FILE : ras.cpp                                                                   ****/
/******************************************************************************************/

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <cstring>
#include <math.h>
#include <stdexcept>

#include <QPointF>

#include "ras.h"

#include "rkflogging/ErrStream.h"
#include "rkflogging/Logging.h"
#include "rkflogging/LoggingConfig.h"

namespace
{
    // Logger for all instances of class
    LOGGER_DEFINE_GLOBAL(logger, "RASClass")
}

/******************************************************************************************/
/**** FUNCTION: RASClass::RASClass()                                                   ****/
/******************************************************************************************/
RASClass::RASClass(int idVal) : id(idVal)
{
    startFreq = -1.0;
    stopFreq = -1.0;
    heightAGL = -1.0;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: RASClass::~RASClass()                                                  ****/
/******************************************************************************************/
RASClass::~RASClass()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: RectRASClass::RectRASClass()                                           ****/
/******************************************************************************************/
RectRASClass::RectRASClass(int idVal) : RASClass(idVal)
{
    rectList = std::vector<std::tuple<double, double, double, double> >(0);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: RectRASClass::~RectRASClass()                                          ****/
/******************************************************************************************/
RectRASClass::~RectRASClass()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: RectRASClass::RectRASClass()                                           ****/
/******************************************************************************************/
void RectRASClass::addRect(double lonStart, double lonStop, double latStart, double latStop)
{
    rectList.push_back(std::make_tuple(lonStart, lonStop, latStart, latStop));
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: RectRASClass::intersect()                                            ****/
/******************************************************************************************/
bool RectRASClass::intersect(double longitude, double latitude, double maxDist, double txHeightAGL) const
{
    int rectIdx, deltaLon, deltaLat, dist;
    bool intersectFlag = false;
    for(rectIdx=0; (rectIdx<rectList.size())&&(!intersectFlag); ++rectIdx) {
        double rectLonStart, rectLonStop, rectLatStart, rectLatStop;
        std::tie(rectLonStart, rectLonStop, rectLatStart, rectLatStop) = rectList[rectIdx];

        int sx = (longitude < rectLonStart ? -1 :
                  longitude > rectLonStop  ?  1 : 0);

        int sy = (latitude < rectLatStart ? -1 :
                  latitude > rectLatStop  ?  1 : 0);

        if ((sx == 0) && (sy == 0)) {
            intersectFlag = true;
        } else if (sx == 0) {
            deltaLat = (sy == -1 ? rectLatStart - latitude : latitude - rectLatStop);
            dist = deltaLat*CConst::earthRadius*M_PI/180.0;
            intersectFlag = (dist <= maxDist);
        } else if (sy == 0) {
            double cosVal = cos(latitude * M_PI/180.0);
            deltaLon = (sy == -1 ? rectLonStart - longitude : longitude - rectLonStop);
            dist = deltaLon*CConst::earthRadius*M_PI/180.0*cosVal;
            intersectFlag = (dist <= maxDist);
        } else {
            double cosVal = cos(latitude * M_PI/180.0);
            double cosSq = cosVal*cosVal;
            deltaLat = (sy == -1 ? rectLatStart - latitude : latitude - rectLatStop);
            deltaLon = (sy == -1 ? rectLonStart - longitude : longitude - rectLonStop);
            dist = CConst::earthRadius*M_PI/180.0*sqrt(deltaLat*deltaLat + deltaLon*deltaLon*cosSq);
            intersectFlag = (dist <= maxDist);
        }
    }

    return(intersectFlag);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: CircleRASClass::CircleRASClass()                                       ****/
/******************************************************************************************/
CircleRASClass::CircleRASClass(int idVal, bool horizonDistFlagVal) : RASClass(idVal), horizonDistFlag(horizonDistFlagVal)
{
    longitudeCenter = 0.0;
    latitudeCenter  = 0.0;
    radius          = 0.0;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: CircleRASClass::~CircleRASClass()                                      ****/
/******************************************************************************************/
CircleRASClass::~CircleRASClass()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: CircleRASClass::computeRadius()                                        ****/
/******************************************************************************************/
double CircleRASClass::computeRadius(double txHeightAGL) const
{
    double returnVal;

    if (!horizonDistFlag) {
        returnVal = radius;
    } else {
        returnVal = sqrt(2*CConst::earthRadius*4.0/3)*(sqrt(heightAGL) + sqrt(txHeightAGL));
    }

    return returnVal;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: CircleRASClass::intersect()                                            ****/
/******************************************************************************************/
bool CircleRASClass::intersect(double longitude, double latitude, double maxDist, double txHeightAGL) const
{
    double rasRadius = computeRadius(txHeightAGL);

    double deltaLon = longitudeCenter - longitude;
    double deltaLat = latitudeCenter - latitude;
    double cosVal = cos(latitude * M_PI/180.0);
    double cosSq = cosVal*cosVal;

    double dist = CConst::earthRadius*M_PI/180.0*( sqrt(deltaLat*deltaLat + deltaLon*deltaLon*cosSq) );

    bool retval = (dist <= rasRadius + maxDist);

    return(retval);
}
/******************************************************************************************/

