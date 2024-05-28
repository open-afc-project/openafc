/*
 *      This data structure stores the data related to a Station Data for Canada (CA).
 *      There is a field in this for each relevent column in the ISED database, regardless of
 * whether or not it is populated.
 *
 */

#ifndef STATION_DATA_CA_H
#define STATION_DATA_CA_H

#include "Vector3.h"

class StationDataCAClass
{
    public:
        int service;
        int subService;
        std::string authorizationNumber;
        std::string callsign;
        double latitudeDeg;
        double longitudeDeg;
        double groundElevation;
        double antennaHeightAGL;
        std::string emissionsDesignator;
        double bandwidthMHz;
        double centerFreqMHz;
        double antennaGain;
        std::string antennaModel;
        std::string modulation;
        double azimuthPtg;
        double elevationPtg;
        double lineLoss;
        std::string inServiceDate;
        std::string stationLocation;
        std::string antennaManufacturer;

        Vector3 position;
        Vector3 pointingVec;
};

#endif
