/*
 *      This data structure stores the data related to a Back To Back Passive Repeater Data and Billboard
 *      Passive Repeader Data for Canada (CA).
 *      There is a field in this for each relevent column in the ISED database, regardless of whether or not it
 *      is populated.
 *
 */

#ifndef PASSIVE_REPEATER_CA_H
#define PASSIVE_REPEATER_CA_H

class PassiveRepeaterCAClass {
public:
    /**************************************************************************************/
    /**** PRType                                                                       ****/
    /**************************************************************************************/
    enum PRTypeEnum
    {
        backToBackAntennaPRType,
        billboardReflectorPRType,
        unknownPRType
    };
    /**************************************************************************************/

    std::string authorizationNumber;
    double latitudeDeg;
    double longitudeDeg;
    double groundElevation;
    double heightAGLA;
    double heightAGLB;

    PRTypeEnum type;

    // Back to Back Antenna Parameters
    double antennaGainA;
    double antennaGainB;
    std::string antennaModelA;
    std::string antennaModelB;
    double azimuthPtgA;
    double azimuthPtgB;
    double elevationPtgA;
    double elevationPtgB;
    Vector3 positionA;
    Vector3 pointingVecA;
    Vector3 positionB;
    Vector3 pointingVecB;

    // Billboard Reflector Parameters
    double reflectorHeight;
    double reflectorWidth;
    Vector3 reflectorPosition;
    Vector3 reflectorPointingVec;
};

class BackToBackPassiveRepeaterCAClass {
public:
    std::string authorizationNumber;
    double latitudeDeg;
    double longitudeDeg;
    double groundElevation;
    double heightAGL;
    double antennaGain;
    std::string antennaModel;
    double azimuthPtg;
    double elevationPtg;
};

class ReflectorPassiveRepeaterCAClass {
public:
    std::string authorizationNumber;
    double latitudeDeg;
    double longitudeDeg;
    double groundElevation;
    double heightAGL;
    double reflectorHeight;
    double reflectorWidth;
    double azimuthPtg;
    double elevationPtg;
};

#endif
