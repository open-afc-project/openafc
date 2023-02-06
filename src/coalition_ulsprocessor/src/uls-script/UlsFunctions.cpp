#include <QtCore>
#include <limits>
#include <math.h>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "UlsFunctions.h"

/******************************************************************************************/
/**** Static Constants                                                                 ****/
/******************************************************************************************/
const double UlsFunctionsClass::speedOfLight = 2.99792458e8;
const double UlsFunctionsClass::earthRadius = 6378.137e3;
const double UlsFunctionsClass::unii5StartFreqMHz = 5925.0;
const double UlsFunctionsClass::unii5StopFreqMHz  = 6425.0;
const double UlsFunctionsClass::unii6StartFreqMHz = 6425.0;
const double UlsFunctionsClass::unii6StopFreqMHz  = 6525.0;
const double UlsFunctionsClass::unii7StartFreqMHz = 6525.0;
const double UlsFunctionsClass::unii7StopFreqMHz  = 6875.0;
const double UlsFunctionsClass::unii8StartFreqMHz = 6875.0;
const double UlsFunctionsClass::unii8StopFreqMHz  = 7125.0;
/******************************************************************************************/

/**************************************************************************/
/* UlsFunctionsClass::makeNumber()                                        */
/**************************************************************************/
QString UlsFunctionsClass::makeNumber(const double &d) {
    if (std::isnan(d)) {
        return "";
    } else {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(15) << d;
        return QString::fromStdString(stream.str());
    }
}

QString UlsFunctionsClass::makeNumber(const int &i)
{
    return QString::number(i);
}
/**************************************************************************/

/**************************************************************************/
/* UlsFunctionsClass::charString()                                        */
/**************************************************************************/
QString UlsFunctionsClass::charString(char c) {
    if (c < 32) {
        return "";
    }
    return QString(c);
}
/**************************************************************************/

/**************************************************************************/
/* UlsFunctionsClass::emissionDesignatorToBandwidth()                     */
/**************************************************************************/
double UlsFunctionsClass::emissionDesignatorToBandwidth(const QString &emDesig)
{
    QString frqPart = emDesig.left(4);
    double multi;
    QString unitS;

    if (frqPart.contains("H")) {
        multi = 1;
        unitS = "H";
    } else if (frqPart.contains("K")) {
        multi = 1000;
        unitS = "K";
    } else if (frqPart.contains("M")) {
        multi = 1e6;
        unitS = "M";
    } else if (frqPart.contains("G")) {
        multi = 1e9;
        unitS = "G";
    } else {
        return -1.0;
    }

    QString num = frqPart.replace(unitS, ".");

    double number = num.toDouble() * multi;

    return number / 1e6; // Convert to MHz
}
/**************************************************************************/

/**************************************************************************/
/* UlsFunctionsClass::hasNecessaryFields()                                */
/**************************************************************************/
QString UlsFunctionsClass::hasNecessaryFields(const UlsEmission &e, UlsPath path,
    UlsLocation rxLoc, UlsLocation txLoc, UlsAntenna rxAnt, UlsAntenna txAnt,
    UlsHeader txHeader, QList<UlsLocation> prLocList, QList<UlsAntenna> prAntList,
    bool removeMobile)
{
    QString failReason = "";
    // check lat/lon degree for rx
    if (isnan(rxLoc.latitude) || isnan(rxLoc.longitude)) {
        failReason.append( "Invalid rx lat degree or long degree, ");
    }
    // check lat/lon degree for rx
    if (isnan(txLoc.latitude) || isnan(txLoc.longitude)) {
        failReason.append( "Invalid tx lat degree or long degree, ");
    }
    // check tx and rx not at same position
    if (    (failReason == "")
         && (fabs(txLoc.longitude - rxLoc.longitude) <=1.0e-5)
         && (fabs(txLoc.latitude  - rxLoc.latitude ) <=1.0e-5) ) {
        failReason.append( "RX and TX at same location, ");
    }
    // check rx latitude/longitude direction
    if(rxLoc.latitudeDirection != 'N' && rxLoc.latitudeDirection != 'S') {
        failReason.append( "Invalid rx latitude direction, ");
    }
    if(rxLoc.longitudeDirection != 'E' && rxLoc.longitudeDirection != 'W') {
        failReason.append( "Invalid rx longitude direction, ");
    }
    // check tx latitude/longitude direction
    if(txLoc.latitudeDirection != 'N' && txLoc.latitudeDirection != 'S') {
        failReason.append( "Invalid tx latitude direction, ");
    }
    if(txLoc.longitudeDirection != 'E' && txLoc.longitudeDirection != 'W') {
        failReason.append( "Invalid tx longitude direction, ");
    }

    // mobile
    if ( removeMobile && (txHeader.mobile == 'Y') ) {
        failReason.append("Mobile is Y, ");
    }

    // radio service code
    if ( removeMobile &&(strcmp(txHeader.radioServiceCode, "TP") == 0) ) {
        failReason.append("Radio service value of TP, ");
    }

    int prIdx;
    for(prIdx=0; prIdx<std::min(prLocList.size(),prAntList.size()); ++prIdx) {
        const UlsLocation &prLoc = prLocList[prIdx];
        const UlsAntenna &prAnt = prAntList[prIdx];

        // check lat/lon degree for pr
        if (isnan(prLoc.latitudeDeg) || isnan(prLoc.longitudeDeg)) {
            failReason.append( "Invalid passive repeater lat degree or long degree, ");
        }
        // check pr latitude/longitude direction
        if(prLoc.latitudeDirection != 'N' && prLoc.latitudeDirection != 'S') {
            failReason.append( "Invalid passive repeater latitude direction, ");
        }
        if(prLoc.longitudeDirection != 'E' && prLoc.longitudeDirection != 'W') {
            failReason.append( "Invalid passive repeater longitude direction, ");
        }
        // check recievers height to center RAAT
        if(isnan(prAnt.heightToCenterRAAT) || prAnt.heightToCenterRAAT <= 0) {
            failReason.append( "Invalid passive repeater height to center RAAT, ");
        }
        if(prAnt.heightToCenterRAAT < 3) {
            failReason.append( "Passive repeater height to center RAAT is < 3m, ");
        }
    }

    return failReason;
}
/**************************************************************************/

/**************************************************************************/
/* UlsFunctionsClass::SegmentCompare()                                    */
/**************************************************************************/
bool UlsFunctionsClass::SegmentCompare(const UlsSegment& segA, const UlsSegment& segB)
{
    return segA.segmentNumber < segB.segmentNumber;
}
/**************************************************************************/

/******************************************************************************************/
/* UlsFunctionsClass::getCSVHeader()                                                      */
/******************************************************************************************/
QStringList UlsFunctionsClass::getCSVHeader(int numPR)
{
    QStringList header;
    header << "Region";
    header << "Callsign";
    header << "Status";
    header << "Radio Service";
    header << "Entity Name";
    header << "FRN";
    header << "Grant";
    header << "Expiration";
    header << "Effective";
    header << "Address";
    header << "City";
    header << "County";
    header << "State";
    header << "Common Carrier";
    header << "Non Common Carrier";
    header << "Private Comm";
    header << "Fixed";
    header << "Mobile";
    header << "Radiolocation";
    header << "Satellite";
    header << "Developmental or STA or Demo";
    header << "Interconnected";
    header << "Path Number";
    header << "Tx Location Number";
    header << "Tx Antenna Number";
    header << "Rx Callsign";
    header << "Rx Location Number";
    header << "Rx Antenna Number";
    header << "Frequency Number";
    header << "1st Segment Length (km)";
    header << "Center Frequency (MHz)";
    header << "Bandwidth (MHz)";
    header << "Lower Band (MHz)";
    header << "Upper Band (MHz)";
    header << "Tolerance (%)";
    header << "Tx EIRP (dBm)";
    header << "Auto Tx Pwr Control";
    header << "Emissions Designator";
    header << "Digital Mod Rate";
    header << "Digital Mod Type";
    header << "Tx Manufacturer";
    header << "Tx Model ULS";
    header << "Tx Model Matched";
    header << "Tx Architecture";
    header << "Tx Location Name";
    header << "Tx Lat Coords";
    header << "Tx Long Coords";
    header << "Tx Ground Elevation (m)";
    header << "Tx Polarization";
    header << "Azimuth Angle Towards Tx (deg)";
    header << "Elevation Angle Towards Tx (deg)";
    header << "Tx Ant Manufacturer";
    header << "Tx Ant Model";
    header << "Tx Ant Model Name Matched";
    header << "Tx Ant Category";
    header << "Tx Ant Diameter (m)";
    header << "Tx Ant Midband Gain (dB)";
    header << "Tx Height to Center RAAT ULS (m)";
    header << "Tx Beamwidth";
    header << "Tx Gain ULS (dBi)";
    header << "Rx Location Name";
    header << "Rx Lat Coords";
    header << "Rx Long Coords";
    header << "Rx Ground Elevation (m)";
    header << "Rx Manufacturer";
    header << "Rx Model";
    header << "Rx Ant Manufacturer";
    header << "Rx Ant Model";
    header << "Rx Ant Model Name Matched";
    header << "Rx Ant Category";
    header << "Rx Ant Diameter (m)";
    header << "Rx Ant Midband Gain (dB)";
    header << "Rx Line Loss (dB)";
    header << "Rx Height to Center RAAT ULS (m)";
    header << "Rx Gain ULS (dBi)";
    header << "Rx Diversity Height to Center RAAT ULS (m)";
    header << "Rx Diversity Ant Diameter (m)";
    header << "Rx Diversity Gain ULS (dBi)";
    header << "Num Passive Repeater";
    for(int prIdx=1; prIdx<=numPR; ++prIdx) {
        header << "Passive Repeater " + QString::number(prIdx) + " Location Name";
        header << "Passive Repeater " + QString::number(prIdx) + " Lat Coords";
        header << "Passive Repeater " + QString::number(prIdx) + " Long Coords";
        header << "Passive Repeater " + QString::number(prIdx) + " Ground Elevation (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Polarization";
        header << "Passive Repeater " + QString::number(prIdx) + " Azimuth Angle (deg)";
        header << "Passive Repeater " + QString::number(prIdx) + " Elevation Angle (deg)";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Manufacturer";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Model";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Model Name Matched";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Type";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Category";
        header << "Passive Repeater " + QString::number(prIdx) + " ULS Back-to-Back Gain Tx (dBi)";
        header << "Passive Repeater " + QString::number(prIdx) + " ULS Back-to-Back Gain Rx (dBi)";
        header << "Passive Repeater " + QString::number(prIdx) + " ULS Reflector Height (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " ULS Reflector Width (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Model Diameter (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Model Midband Gain (dB)";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Model Reflector Height (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Model Reflector Width (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Line Loss (dB)";
        header << "Passive Repeater " + QString::number(prIdx) + " Height to Center RAAT Tx (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Height to Center RAAT Rx (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Beamwidth";
        header << "Segment " + QString::number(prIdx+1) + " Length (Km)";
    }

    return header;
}
/******************************************************************************************/

/******************************************************************************************/
/**** UlsFunctionsClass::computeSpectralOverlap()                                      ****/
/******************************************************************************************/
double UlsFunctionsClass::computeSpectralOverlap(double sigStartFreq, double sigStopFreq, double rxStartFreq, double rxStopFreq)
{
    double overlap;

    if ((sigStopFreq <= rxStartFreq) || (sigStartFreq >= rxStopFreq)) {
        overlap = 0.0;
    } else {
        double f1 = (sigStartFreq < rxStartFreq ? rxStartFreq : sigStartFreq);
        double f2 = (sigStopFreq  > rxStopFreq ? rxStopFreq : sigStopFreq);
        overlap = (f2-f1)/(sigStopFreq - sigStartFreq);
    }

    return(overlap);
}
/******************************************************************************************/

/******************************************************************************************/
/**** UlsFunctionsClass::computeHPointingVec()                                         ****/
/******************************************************************************************/
Vector3 UlsFunctionsClass::computeHPointingVec(Vector3 position, double azimuthPtg, double elevationPtg)
{
    Vector3 ptgVec;

    Vector3 upVec = position.normalized();
    Vector3 zVec = Vector3(0.0, 0.0, 1.0);
    Vector3 eastVec = zVec.cross(upVec).normalized();
    Vector3 northVec = upVec.cross(eastVec);

    double ca = cos(azimuthPtg*M_PI/180.0);
    double sa = sin(azimuthPtg*M_PI/180.0);
    double ce = cos(elevationPtg*M_PI/180.0);
    double se = sin(elevationPtg*M_PI/180.0);

    ptgVec = northVec*ca*ce + eastVec*sa*ce + upVec*se;

    return(ptgVec);
}
/******************************************************************************************/

