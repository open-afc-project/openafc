/*
 *      This data structure stores the data related to a Radio Astronomy Station
 *
 */

#ifndef RAS_H
#define RAS_H

#include "Vector3.h"

class RASClass
{
    public:
        std::string region;
        std::string name;
        std::string location;
        double startFreqMHz;
        double stopFreqMHz;
        std::string exclusionZone;
        double rect1lat1;
        double rect1lat2;
        double rect1lon1;
        double rect1lon2;
        double rect2lat1;
        double rect2lat2;
        double rect2lon1;
        double rect2lon2;
        double radiusKm;
        double centerLat;
        double centerLon;
        double heightAGL;
};

#endif
