#include "CsvWriter.h"
#include "UlsFileReader.h"
#include <QDebug>
#include <QStringList>
#include <limits>
#include <math.h>
#include <iostream>

#define VERSION "1.3.0"

bool removeMobile = false;

const double speedOfLight = 2.99792458e8;
const double unii5StartFreqMHz = 5925.0;
const double unii5StopFreqMHz  = 6425.0;
const double unii7StartFreqMHz = 6525.0;
const double unii7StopFreqMHz  = 6875.0;

namespace {
QString makeNumber(const double &d) {
  if (std::isnan(d))
    return "";
  else
    return QString::number(d, 'f', 15);
}

QString makeNumber(const int &i) { return QString::number(i); }

QString charString(char c) {
  if (c < 32) {
    return "";
  }
  return QString(c);
}

double emissionDesignatorToBandwidth(const QString &emDesig) {
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
    return -1;
  }

  QString num = frqPart.replace(unitS, ".");

  double number = num.toDouble() * multi;

  return number / 1e6; // Convert to MHz
}


// Ensures that all the necessary fields are available to determine if this goes in the regular ULS or anomalous file
// returns a blank string if valid, otherwise a string indicating what failed. 
QString hasNecessaryFields(const UlsEmission &e, UlsPath path, UlsLocation rxLoc, UlsLocation txLoc, UlsAntenna rxAnt, UlsAntenna txAnt, UlsHeader txHeader, QList<UlsLocation> prLocList, QList<UlsAntenna> prAntList) {
  QString failReason = "";
  // check lat/lon degree for rx
  if (isnan(rxLoc.latitudeDeg) || isnan(rxLoc.longitudeDeg)) {
    failReason.append( "Invalid rx lat degree or long degree, "); 
  } 
  // check lat/lon degree for rx
  if (isnan(txLoc.latitudeDeg) || isnan(txLoc.longitudeDeg)) {
    failReason.append( "Invalid tx lat degree or long degree, "); 
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

  // check recievers height to center RAAT
  if(isnan(rxAnt.heightToCenterRAAT) || rxAnt.heightToCenterRAAT <= 0) {
    failReason.append( "Invalid rx height to center RAAT, "); 
  }
  if(rxAnt.heightToCenterRAAT < 3) {
    failReason.append( "rx height to center RAAT is < 3m, "); 
  }
  if(isnan(txAnt.heightToCenterRAAT) || txAnt.heightToCenterRAAT <= 0) {
    failReason.append( "Invalid tx height to center RAAT, "); 
  }
  if(txAnt.heightToCenterRAAT < 3) {
    failReason.append( "tx height to center RAAT is < 3m, "); 
  }

  // check recievers gain 
  if(isnan(rxAnt.gain) || rxAnt.gain < 0 ) {
    failReason.append("Invalid rx gain value, "); 
  } 
  if(rxAnt.gain > 80 ) {
    failReason.append("rx gain > 80 dBi, "); 
  }
  if(rxAnt.gain < 10 ) {
    failReason.append("rx gain < 10 dBi, "); 
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

bool SegmentCompare(const UlsSegment& segA, const UlsSegment& segB)
{
    return segA.segmentNumber < segB.segmentNumber;
}

QStringList getCSVHeader(int numPR)
{
    QStringList header;
    header << "Callsign";
    header << "Status";
    header << "Radio Service";
    header << "Entity Name";
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
    header << "Tx Model";
    header << "Tx Location Name";
    header << "Tx Lat Coords";
    header << "Tx Long Coords";
    header << "Tx Ground Elevation (m)";
    header << "Tx Polarization";
    header << "Tx Azimuth Angle (deg)";
    header << "Tx Elevation Angle (deg)";
    header << "Tx Ant Manufacturer";
    header << "Tx Ant Model";
    header << "Tx Height to Center RAAT (m)";
    header << "Tx Beamwidth";
    header << "Tx Gain (dBi)";
    header << "Rx Location Name";
    header << "Rx Lat Coords";
    header << "Rx Long Coords";
    header << "Rx Ground Elevation (m)";
    header << "Rx Manufacturer";
    header << "Rx Model";
    header << "Rx Ant Manufacturer";
    header << "Rx Ant Model";
    header << "Rx Height to Center RAAT (m)";
    header << "Rx Gain (dBi)";
    header << "Rx Ant Diameter (m)";
    header << "Rx Diversity Height (m)";
    header << "Rx Diversity Gain (dBi)";
    header << "Num Passive Repeater";
    for(int prIdx=1; prIdx<=numPR; ++prIdx) {
        header << "Passive Repeater " + QString::number(prIdx) + " Location Name";
        header << "Passive Repeater " + QString::number(prIdx) + " Lat Coords";
        header << "Passive Repeater " + QString::number(prIdx) + " Long Coords";
        header << "Passive Repeater " + QString::number(prIdx) + " Ground Elevation (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Polarization";
        header << "Passive Repeater " + QString::number(prIdx) + " Azimuth Angle (deg)";
        header << "Passive Repeater " + QString::number(prIdx) + " Elevation Angle (deg)";
        header << "Passive Repeater " + QString::number(prIdx) + " Reflector Height (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Reflector Width (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Line Loss (dB)";
        header << "Passive Repeater " + QString::number(prIdx) + " Height to Center RAAT (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Beamwidth";
        header << "Passive Repeater " + QString::number(prIdx) + " Back-to-Back Gain Tx (dBi)";
        header << "Passive Repeater " + QString::number(prIdx) + " Back-to-Back Gain Rx (dBi)";
        header << "Segment " + QString::number(prIdx+1) + " Length (Km)";
    }

    return header;
}

/******************************************************************************************/
/**** computeSpectralOverlap                                                           ****/
/******************************************************************************************/
double computeSpectralOverlap(double sigStartFreq, double sigStopFreq, double rxStartFreq, double rxStopFreq)
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


}; // namespace

int main(int argc, char **argv)
{
  setvbuf(stdout, NULL, _IONBF, 0);

  if (strcmp(argv[1], "--version") == 0) {
    printf("Coalition ULS Processing Tool Version %s\n", VERSION);
    printf("Copyright 2019 (C) RKF Engineering Solutions\n");
    printf("Compatible with ULS Database Version 4\n");
    printf("Spec: "
           "https://www.fcc.gov/sites/default/files/"
           "public_access_database_definitions_v4.pdf\n");
    return 0;
  }
  printf("Coalition ULS Processing Tool Version %s\n", VERSION);
  printf("Copyright 2019 (C) RKF Engineering Solutions\n");
  if (argc < 3 || argc > 3) {
    fprintf(stderr, "Syntax: %s [ULS file.csv] [Output File.csv]\n", argv[0]);
    return -1;
  }

    char *tstr;

    time_t t1 = time(NULL);
    tstr = strdup(ctime(&t1));
    strtok(tstr, "\n");
    std::cout << tstr << " : Begin processing." << std::endl;
    free(tstr);

  UlsFileReader r(argv[1]);

    int maxNumSegment = 0;
    std::string maxNumSegmentCallsign = "";

    foreach (const UlsFrequency &freq, r.frequencies()) {

        UlsPath path;
        bool pathFound = false;

        foreach (const UlsPath &p, r.pathsMap(freq.callsign)) {
            if (strcmp(p.callsign, freq.callsign) == 0) {
                if (freq.locationNumber == p.txLocationNumber &&
                    freq.antennaNumber == p.txAntennaNumber) {
                    path = p;
                    pathFound = true;
                    break;
                }
            }
        }

        if (pathFound == false) {
            continue;
        }

        /// Find the emissions information.
        UlsEmission txEm;
        bool txEmFound = false;
        bool hasMultipleTxEm = false;
        QList<UlsEmission> allTxEm;
        foreach (const UlsEmission &e, r.emissionsMap(freq.callsign)) {
            if (strcmp(e.callsign, freq.callsign) == 0 &&
                e.locationId == freq.locationNumber &&
                e.antennaId == freq.antennaNumber &&
                e.frequencyId == freq.frequencyNumber) {
                allTxEm << e;
            }
        }

        /// Find the header.
        UlsHeader txHeader;
        bool txHeaderFound = false;
        foreach (const UlsHeader &h, r.headersMap(path.callsign)) {
            if (strcmp(h.callsign, path.callsign) == 0) {
                txHeader = h;
                txHeaderFound = true;
                break;
            }
        }

        if (!txHeaderFound) {
            continue;
        } else if (txHeader.licenseStatus != 'A' && txHeader.licenseStatus != 'L') {
            continue;
        }

        // std::cout << freq.callsign << ": " << allTxEm.size() << " emissions" << std::endl;
        foreach (const UlsEmission &e, allTxEm) {
            bool invalidFlag = false;
            double bwMhz = std::numeric_limits<double>::quiet_NaN();
            double lowFreq, highFreq;
            bwMhz = emissionDesignatorToBandwidth(e.desig);
            if (bwMhz == -1) {
                invalidFlag = true;
            }
            if (bwMhz == 0) {
                invalidFlag = true;
            }

            if (!invalidFlag) {
                if (freq.frequencyUpperBand > freq.frequencyAssigned) {
                    lowFreq  = freq.frequencyAssigned;  // Lower Band (MHz)
                    highFreq = freq.frequencyUpperBand; // Upper Band (MHz)
                    // Here Frequency Assigned should be low + high / 2
                } else {
                    lowFreq  = freq.frequencyAssigned - bwMhz / 2.0; // Lower Band (MHz)
                    highFreq = freq.frequencyAssigned + bwMhz / 2.0; // Upper Band (MHz)
                }
                // skip if no overlap UNII5 and 7
                if (lowFreq >= 6875 || highFreq <= 5925) {
                    invalidFlag = true;
                } else if (lowFreq >= 6425 && highFreq <= 6525) {
                    invalidFlag = true;
                }
            } 
            if (!invalidFlag) {
                foreach (const UlsSegment &segment, r.segmentsMap(freq.callsign)) {
                    int segmentNumber = segment.segmentNumber;
                    if (segmentNumber > maxNumSegment) {
                        maxNumSegment = segmentNumber;
                        maxNumSegmentCallsign = std::string(segment.callsign);
                    }
                }
            }
        }
    }

    int maxNumPassiveRepeater = maxNumSegment - 1;

  qDebug() << "DATA statistics:";
  qDebug() << "paths" << r.paths().count();
  qDebug() << "emissions" << r.emissions().count();
  qDebug() << "antennas" << r.antennas().count();
  qDebug() << "frequencies" << r.frequencies().count();
  qDebug() << "locations" << r.locations().count();
  qDebug() << "headers" << r.headers().count();
  qDebug() << "market freqs" << r.marketFrequencies().count();
  qDebug() << "entities" << r.entities().count();
  qDebug() << "control points" << r.controlPoints().count();
  qDebug() << "segments" << r.segments().count();
  qDebug() << "maxNumPassiveRepeater" << maxNumPassiveRepeater << " callsign: " << QString::fromStdString(maxNumSegmentCallsign);

  int prIdx;

    CsvWriter wt(argv[2]);
    {
        QStringList header = getCSVHeader(maxNumPassiveRepeater);
        wt.writeRow(header);
    }

    CsvWriter anomalous("anomalous_uls.csv");
    {
        QStringList header = getCSVHeader(maxNumPassiveRepeater);
        header << "Fixed";
        header << "Anomalous Reason";
        anomalous.writeRow(header);
    }

    FILE *fwarn;
    std::string warningFile = "warning_uls.txt";
    if ( !(fwarn = fopen(warningFile.c_str(), "wb")) ) {
        std::cout << std::string("WARNING: Unable to open warningFile \"") + warningFile + std::string("\"\n");
    }

    qDebug() << "--- Beginning path processing";

    int cnt = 0;
    int numRecs = 0;

  foreach (const UlsFrequency &freq, r.frequencies()) {
    // qDebug() << "processing frequency " << cnt << "/" << r.frequencies().count()
            //  << " callsign " << freq.callsign;
    bool pathFound = false;
    bool hasRepeater = false;
    QString anomalousReason = ""; 
    QString fixedReason = ""; 

    QList<UlsPath> pathList;

    foreach (const UlsPath &p, r.pathsMap(freq.callsign)) {
        if (strcmp(p.callsign, freq.callsign) == 0) {
            if (    (freq.locationNumber == p.txLocationNumber)
                 && (freq.antennaNumber == p.txAntennaNumber) ) {
                pathList << p;
            }
        }
    }

    if ((pathList.size() == 0) && (fwarn)) {
        fprintf(fwarn, "CALLSIGN: %s, Unable to find path matching TX_LOCATION_NUM = %d TX_ANTENNA_NUM = %d\n",
            freq.callsign, freq.locationNumber, freq.antennaNumber);
    }

    foreach (const UlsPath &path, pathList) {
        if (path.passiveReceiver == 'Y') {
            hasRepeater = true;
        }

        cnt++;

        /// Find the associated transmit location.
        UlsLocation txLoc;
        bool locFound = false;
        foreach (const UlsLocation &loc, r.locationsMap(path.callsign)) {
            if (strcmp(loc.callsign, path.callsign) == 0) {
                if (path.txLocationNumber == loc.locationNumber) {
                    txLoc = loc;
                    locFound = true;
                    break;
                }
            }
        }

        if (locFound == false) {
            if (fwarn) {
                fprintf(fwarn, "CALLSIGN: %s, Unable to find txLoc matching LOCATION_NUM = %d\n",
                    freq.callsign, path.txLocationNumber);
            }
            continue;
        }

        /// Find the associated transmit antenna.
        UlsAntenna txAnt;
        bool txAntFound = false;
        foreach (const UlsAntenna &ant, r.antennasMap(path.callsign)) {
            if (strcmp(ant.callsign, path.callsign) == 0) {
                // Implicitly matches with an Antenna record with locationClass 'T'
                if (    (ant.locationNumber == txLoc.locationNumber)
                     && (ant.antennaNumber == path.txAntennaNumber)
                     && (ant.pathNumber == path.pathNumber)
                   ) {
                    txAnt = ant;
                    txAntFound = true;
                    break;
                }
            }
        }

        if (txAntFound == false) {
            if (fwarn) {
                fprintf(fwarn, "CALLSIGN: %s, Unable to find txAnt matching LOCATION_NUM = %d ANTENNA_NUM = %d PATH_NUM = %d\n",
                    freq.callsign, txLoc.locationNumber, path.txAntennaNumber, path.pathNumber);
            }
            continue;
        }

        /// Find the associated frequency.
        const UlsFrequency &txFreq = freq;

        /// Find the RX location.
        UlsLocation rxLoc;
        bool rxLocFound = false;
        foreach (const UlsLocation &loc, r.locationsMap(path.callsign)) {
            if (strcmp(loc.callsign, path.callsign) == 0) {
                if (loc.locationNumber == path.rxLocationNumber) {
                    rxLoc = loc;
                    rxLocFound = true;
                    break;
                }
            }
        }

        if (rxLocFound == false) {
            if (fwarn) {
                fprintf(fwarn, "CALLSIGN: %s, Unable to find rxLoc matching LOCATION_NUM = %d\n",
                    freq.callsign, path.rxLocationNumber);
            }
            continue;
        }

        /// Find the RX antenna.
        UlsAntenna rxAnt;
        bool rxAntFound = false;
        foreach (const UlsAntenna &ant, r.antennasMap(path.callsign)) {
            if (strcmp(ant.callsign, path.callsign) == 0) {
                // Implicitly matches with an Antenna record with locationClass 'R'
                if (    (ant.locationNumber == rxLoc.locationNumber)
                     && (ant.antennaNumber == path.rxAntennaNumber)
                     && (ant.pathNumber == path.pathNumber)
                   ) {
                    rxAnt = ant;
                    rxAntFound = true;
                    break;
                }
            }
        }

        if (rxAntFound == false) {
            if (fwarn) {
                fprintf(fwarn, "CALLSIGN: %s, Unable to find rxAnt matching LOCATION_NUM = %d ANTENNA_NUM = %d PATH_NUM = %d\n",
                    freq.callsign, rxLoc.locationNumber, path.rxAntennaNumber, path.pathNumber);
            }
            continue;
        }

        // Only assigned if hasRepeater is True
        QList<UlsLocation> prLocList;
        QList<UlsAntenna> prAntList;

        /// Create list of segments in link.
        QList<UlsSegment> segList;
        foreach (const UlsSegment &s, r.segmentsMap(path.callsign)) {
            if (s.pathNumber == path.pathNumber) {
                segList << s;
            }
        }
        qSort(segList.begin(), segList.end(), SegmentCompare);

        int prevSegRxLocationId = -1;
        for(int segIdx=0; segIdx<segList.size(); ++segIdx) {
            const UlsSegment &s = segList[segIdx];
            if (s.segmentNumber != segIdx+1) {
                anomalousReason.append("Segments missing, ");
                qDebug() << "callsign " << path.callsign << " path " << path.pathNumber << " has missing segments.";
                break;
            }
            if (segIdx == 0) {
                if (s.txLocationId != txLoc.locationNumber) {
                    anomalousReason.append("First segment not at TX, ");
                    qDebug() << "callsign " << path.callsign << " path " << path.pathNumber << " first segment not at TX.";
                    break;
                }
            }
            if (segIdx == segList.size()-1) {
                if (s.rxLocationId != rxLoc.locationNumber) {
                    anomalousReason.append("Last segment not at RX, ");
                    qDebug() << "callsign " << path.callsign << " path " << path.pathNumber << " last segment not at RX.";
                    break;
                }
            }
            if (segIdx) {
                if (s.txLocationId != prevSegRxLocationId) {
                    anomalousReason.append("Segments do not form a path, ");
                    qDebug() << "callsign " << path.callsign << " path " << path.pathNumber << " segments do not form a path.";
                    break;
                }
                bool found;
                found = false;
                foreach (const UlsLocation &loc, r.locationsMap(path.callsign)) {
                    if (loc.locationNumber == s.txLocationId) {
                        prLocList << loc;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    anomalousReason.append("Segment location not found, ");
                    qDebug() << "callsign " << path.callsign << " path " << path.pathNumber << " segment location not found.";
                    break;
                }
                found = false;
                foreach (const UlsAntenna &ant, r.antennasMap(path.callsign)) {
                    if (    (ant.antennaType == 'P')
                         && (ant.locationNumber == prLocList.last().locationNumber)
                         && (ant.pathNumber == path.pathNumber) ) {
                        prAntList << ant;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    anomalousReason.append("Segment antenna not found, ");
                    qDebug() << "callsign " << path.callsign << " path " << path.pathNumber << " segment antenna not found.";
                    break;
                }
            }
            prevSegRxLocationId = s.rxLocationId;
        }

        UlsSegment txSeg;
        bool txSegFound = false;
        foreach (const UlsSegment &s, r.segmentsMap(path.callsign)) {
            if (strcmp(s.callsign, path.callsign) == 0) {
                if (s.pathNumber == path.pathNumber) {
                    if (s.segmentNumber < 2) {
                        txSeg = s;
                        txSegFound = true;
                        break;
                    }
                }
            }
        }

        /// Find the emissions information.
        UlsEmission txEm;
        bool txEmFound = false;
        bool hasMultipleTxEm = false;
        QList<UlsEmission> allTxEm;
        foreach (const UlsEmission &e, r.emissionsMap(path.callsign)) {
            if (strcmp(e.callsign, path.callsign) == 0) {
                if (e.locationId == txLoc.locationNumber &&
                    e.antennaId == txAnt.antennaNumber &&
                    e.frequencyId == txFreq.frequencyNumber) {
                if (txEmFound) {
                    hasMultipleTxEm = true;
                    allTxEm << e;
                    continue;
                } else {
                    allTxEm << e;
                    txEm = e;
                    txEmFound = true;
                }
                // break;
                }
            }
        }
        if (!txEmFound)
          allTxEm << txEm; // Make sure at least one emission.

        // if (hasMultipleTxEm) {
        //   qDebug() << "callsign " << path.callsign
        //            << " has multiple emission designators.";
        // }

        /// Find the header.
        UlsHeader txHeader;
        bool txHeaderFound = false;
        foreach (const UlsHeader &h, r.headersMap(path.callsign)) {
            if (strcmp(h.callsign, path.callsign) == 0) {
                txHeader = h;
                txHeaderFound = true;
                break;
            }
        }

        if (!txHeaderFound) {
            // qDebug() << "Unable to locate header data for" << path.callsign << "path"
            //  << path.pathNumber;
            continue;
        } else if (txHeader.licenseStatus != 'A' && txHeader.licenseStatus != 'L') {
            // qDebug() << "Skipping non-Active tx for" << path.callsign << "path" << path.pathNumber;
            continue;
        }

        /// Find the entity
        UlsEntity txEntity;
        bool txEntityFound = false;
        foreach (const UlsEntity &e, r.entitiesMap(path.callsign)) {
            if (strcmp(e.callsign, path.callsign) == 0) {
                txEntity = e;
                txEntityFound = true;
                break;
            }
        }

        if (!txEntityFound) {
            // qDebug() << "Unable to locate entity data for" << path.callsign << "path"
            //          << path.pathNumber;
            continue;
        }

        /// Find the control point.
        UlsControlPoint txControlPoint;
        bool txControlPointFound = false;
        foreach (const UlsControlPoint &ucp, r.controlPointsMap(path.callsign)) {
            if (strcmp(ucp.callsign, path.callsign) == 0) {
                txControlPoint = ucp;
                txControlPointFound = true;
                break;
            }
        }

    /// Build the actual output.
    foreach (const UlsEmission &e, allTxEm) {
      double bwMhz = std::numeric_limits<double>::quiet_NaN();
      double lowFreq, highFreq;
      if (txEmFound) {
        bwMhz = emissionDesignatorToBandwidth(e.desig);
        if (bwMhz == -1) {
          anomalousReason.append("Emission designator contains no or invalid order of magnitude, ");
        }
        if (bwMhz == 0) {
          anomalousReason.append("Emission designator contains no or invalid order of magnitude, ");
        }
      }
      if (txFreq.frequencyUpperBand > txFreq.frequencyAssigned) {
        lowFreq = txFreq.frequencyAssigned;  // Lower Band (MHz)
        highFreq = txFreq.frequencyUpperBand; // Upper Band (MHz)
        // Here Frequency Assigned should be low + high / 2
      } else {
        lowFreq = txFreq.frequencyAssigned -
                          bwMhz / 2.0; // Lower Band (MHz)
        highFreq = txFreq.frequencyAssigned +
                          bwMhz / 2.0; // Upper Band (MHz)
      }

      double rxAntennaDiameter = 0.0;
      if(isnan(lowFreq) || isnan(highFreq)) {
          anomalousReason.append("NaN frequency value, ");
      } else {
          double overlapUnii5 = computeSpectralOverlap(lowFreq, highFreq, unii5StartFreqMHz, unii5StopFreqMHz);
          double overlapUnii7 = computeSpectralOverlap(lowFreq, highFreq, unii7StartFreqMHz, unii7StopFreqMHz);

          if ((overlapUnii5 == 0.0) && (overlapUnii7 == 0.0)) {
              continue;
          } else if ((overlapUnii5 > 0.0) && (overlapUnii7 > 0.0)) {
              anomalousReason.append("Band overlaps both Unii5 and Unii7, ");
          } else {

              /************************************************************************************/
              /* Fix RX Antenna Gain/Diameter according to WINNF-21-I-00132                       */
              /************************************************************************************/
              double centerFreqMHz = (lowFreq + highFreq)/2;
              if (std::isnan(rxAnt.gain)) {
                  if (overlapUnii5 > 0.0) {
                      fixedReason.append("Rx Gain missing, set to UNII-5 value 38.8");
                      rxAnt.gain = 38.8;
                  } else {
                      fixedReason.append("Rx Gain missing, set to UNII-7 value 39.5");
                      rxAnt.gain = 39.5;
                  }
                  rxAntennaDiameter = 1.83;
              } else {
                  double G;
                  if (rxAnt.gain < 32.0) {
                      fixedReason.append(QString("Rx Gain = %1 < 32, Rx Gain set to 32").arg(rxAnt.gain));
                      G = 32.0;
                  } else if (rxAnt.gain > 48.0) {
                      fixedReason.append(QString("Rx Gain = %1 > 48, Rx Gain set to 48").arg(rxAnt.gain));
                      G = 48.0;
                  } else {
                      G = rxAnt.gain;
                  }
                  double Fc;
                  if (overlapUnii5 > 0.0) {
                      Fc = 6175.0e6;
                  } else {
                      Fc = 6700.0e6;
                  }
                  double oneOverSqrtK = 1.0/sqrt(0.54);
                  rxAntennaDiameter = (speedOfLight/(M_PI*Fc))*exp(log(10.0)*G/20)*oneOverSqrtK;

                  // rxAnt.gain = xxxxxxx Need clarification from WINNF
              }
              /************************************************************************************/
          }
      }

      // now that we have everything, ensure that inputs have what we need
      anomalousReason.append(hasNecessaryFields(e, path, rxLoc, txLoc, rxAnt, txAnt, txHeader, prLocList, prAntList));

      QStringList row;
      row << path.callsign;                   // Callsign
      row << QString(txHeader.licenseStatus); // Status
      row << txHeader.radioServiceCode;       // Radio Service
      row << txEntity.entityName;             // Entity Name
      row << txHeader.grantDate;              // Grant
      row << txHeader.expiredDate;            // Expiration
      row << txHeader.effectiveDate;          // Effective
      if (txControlPointFound) {
        row << txControlPoint.controlPointAddress; // Address
        row << txControlPoint.controlPointCity;    // City
        row << txControlPoint.controlPointCounty;  // County
        row << txControlPoint.controlPointState;   // State
      } else {
        row << ""
            << ""
            << ""
            << "";
      }
      row << charString(txHeader.commonCarrier);    // Common Carrier
      row << charString(txHeader.nonCommonCarrier); // Non Common Carrier
      row << charString(txHeader.privateCarrier);   // Private Comm
      row << charString(txHeader.fixed);            // Fixed
      row << charString(txHeader.mobile);           // Mobile
      row << charString(txHeader.radiolocation);    // Radiolocation
      row << charString(txHeader.satellite);        // Satellite
      row << charString(txHeader.developmental); // Developmental or STA or Demo
      row << charString(txHeader.interconnected); // Interconnected
      row << makeNumber(path.pathNumber);         // Path Number
      row << makeNumber(path.txLocationNumber);   // Tx Location Number
      row << makeNumber(path.txAntennaNumber);    // Tx Antenna Number
      row << path.rxCallsign;                     // Rx Callsign
      row << makeNumber(path.rxLocationNumber);   // Rx Location Number
      row << makeNumber(path.rxAntennaNumber);    // Rx Antenna Number
      row << makeNumber(txFreq.frequencyNumber);  // Frequency Number
      if (txSegFound) {
        row << makeNumber(txSeg.segmentLength); // 1st Segment Length (km)
      } else {
        row << "";
      }

      {

        QString _freq;
        // If this is true, frequencyAssigned IS the lower band
        if (txFreq.frequencyUpperBand > txFreq.frequencyAssigned) {
          _freq = QString("%1")
                     .arg( (txFreq.frequencyAssigned + txFreq.frequencyUpperBand) / 2);
        } else {
          _freq = QString("%1").arg(txFreq.frequencyAssigned);
        }
        row << _freq; // Center Frequency (MHz)
      }
      
      {
        if (txEmFound) {
          row << makeNumber(bwMhz); // Bandiwdth (MHz)
        } else {
          row << "";
        }
        row << makeNumber(lowFreq); // Lower Band (MHz)
        row << makeNumber(highFreq); // Upper Band (MHz)
      }

      row << makeNumber(txFreq.tolerance);               // Tolerance (%)
      row << makeNumber(txFreq.EIRP);                    // Tx EIRP (dBm)
      row << charString(txFreq.transmitterPowerControl); // Auto Tx Pwr Control
      if (txEmFound) {
        row << QString(e.desig);      // Emissions Designator
        row << makeNumber(e.modRate); // Digital Mod Rate
        row << QString(e.modCode);    // Digital Mod Type
      } else {
        row << ""
            << ""
            << "";
      }

      row << txFreq.transmitterMake;  // Tx Manufacturer
      row << txFreq.transmitterModel; // Tx Model
      row << txLoc.locationName;      // Tx Location Name
      row << makeNumber(txLoc.latitude);
      // QString("%1-%2-%3
      // %4").arg(txLoc.latitudeDeg).arg(txLoc.latitudeMinutes).arg(txLoc.latitudeSeconds).arg(txLoc.latitudeDirection);
      // // Tx Lat Coords
      row << makeNumber(txLoc.longitude);
      // QString("%1-%2-%3
      // %4").arg(txLoc.longitudeDeg).arg(txLoc.longitudeMinutes).arg(txLoc.longitudeSeconds).arg(txLoc.longitudeDirection);
      // // Tx Lon Coords
      row << makeNumber(txLoc.groundElevation); // Tx Ground Elevation (m)
      row << txAnt.polarizationCode;            // Tx Polarization
      row << makeNumber(txAnt.azimuth);         // Tx Azimuth Angle (deg)
      row << makeNumber(txAnt.tilt);            // Tx Elevation Angle (deg)
      row << txAnt.antennaMake;                 // Tx Ant Manufacturer
      row << txAnt.antennaModel;                // Tx Ant Model
      row << makeNumber(txAnt.heightToCenterRAAT);
      // Tx Height to Center RAAT (m)
      row << makeNumber(txAnt.beamwidth); // Tx Beamwidth
      row << makeNumber(txAnt.gain);      // Tx Gain (dBi)
      row << rxLoc.locationName;          // Rx Location Name
      row << makeNumber(rxLoc.latitude);
      // QString("%1-%2-%3
      // %4").arg(rxLoc.latitudeDeg).arg(rxLoc.latitudeMinutes).arg(rxLoc.latitudeSeconds).arg(rxLoc.latitudeDirection);
      // // Rx Lat Coords
      row << makeNumber(rxLoc.longitude);
      // QString("%1-%2-%3
      // %4").arg(rxLoc.longitudeDeg).arg(rxLoc.longitudeMinutes).arg(rxLoc.longitudeSeconds).arg(rxLoc.longitudeDirection);
      // // Rx Lon Coords
      row << makeNumber(rxLoc.groundElevation); // Rx Ground Elevation (m)
      row << "";                                // Rx Manufacturer
      row << "";                                // Rx Model
      row << rxAnt.antennaMake;                 // Rx Ant Manufacturer
      row << rxAnt.antennaModel;                // Rx Ant Model
      row << makeNumber(
          rxAnt.heightToCenterRAAT);            // Rx Height to Center RAAT (m)
      row << makeNumber(rxAnt.gain);            // Rx Gain (dBi)
      row << makeNumber(rxAntennaDiameter);     // Rx Ant Diameter (m)
      row << makeNumber(rxAnt.diversityHeight); // Rx Diveristy Height (m)
      row << makeNumber(rxAnt.diversityGain);   // Rx Diversity Gain (dBi)

      row << QString::number(prLocList.size());
      for(prIdx=1; prIdx<=maxNumPassiveRepeater; ++prIdx) {

          if (    (prIdx <= prLocList.size())
               && (prIdx <= prAntList.size()) ) {
              UlsLocation &prLoc = prLocList[prIdx-1];
              UlsAntenna &prAnt = prAntList[prIdx-1];
              UlsSegment &segment = segList[prIdx];

              row << prLoc.locationName;          // Passive Repeater Location Name
              row << makeNumber(prLoc.latitude);  // Passive Repeater Lat Coords
              row << makeNumber(prLoc.longitude); // Passive Repeater Lon Coords
              row << makeNumber(
                  prLoc.groundElevation);       // Passive Repeater Ground Elevation
              row << prAnt.polarizationCode;    // Passive Repeater Polarization
              row << makeNumber(prAnt.azimuth); // Passive Repeater Azimuth Angle
              row << makeNumber(prAnt.tilt);    // Passive Repeater Elevation Angle
              row << makeNumber(prAnt.reflectorHeight); // Passive Repeater Reflector Height
              row << makeNumber(prAnt.reflectorWidth);  // Passive Repeater Reflector Width
              row << makeNumber(prAnt.lineLoss);        // Passive Repeater Line Loss
              row << makeNumber(
                  prAnt.heightToCenterRAAT); // Passive Repeater Height to Center RAAT
              row << makeNumber(prAnt.beamwidth); // Passive Repeater Beamwidth
              row << makeNumber(
                  prAnt.backtobackTxGain); // Passive Repeater Back-To-Back Tx Gain
              row << makeNumber(
                  prAnt.backtobackRxGain); // Passive Repeater Back-To-Back Rx Gain
              row << makeNumber(segment.segmentLength); // Segment Length (km)
          } else {
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
          }
      }
      if(anomalousReason.length() > 0) {
        row << "0" << anomalousReason; 
        anomalous.writeRow(row);
        anomalousReason = "";
      } else {
        wt.writeRow(row);
        if (fixedReason.length() > 0) {
            row << "1" << fixedReason; 
            anomalous.writeRow(row);
        }
        numRecs++;
      }
      fixedReason = "";
      
    }
    }
  }

    if (fwarn) {
        fclose(fwarn);
    }

    std::cout<<"Processed " << r.frequencies().count()
             << " frequency records and output to file; a total of " << numRecs
             << " output"<<'\n';

    time_t t2 = time(NULL);
    tstr = strdup(ctime(&t2));
    strtok(tstr, "\n");
    std::cout << tstr << " : Completed processing." << std::endl;
    free(tstr);

    int elapsedTime = (int) (t2-t1);

    int et = elapsedTime;
    int elapsedTimeSec = et % 60;
    et = et / 60;
    int elapsedTimeMin = et % 60;
    et = et / 60;
    int elapsedTimeHour = et % 24;
    et = et / 24;
    int elapsedTimeDay = et;

    std::cout << "Elapsed time = " << (t2-t1) << " sec = ";
    if (elapsedTimeDay) {
        std::cout << elapsedTimeDay  << " days ";
    }
    if (elapsedTimeDay || elapsedTimeHour) {
        std::cout << elapsedTimeHour << " hours ";
    }
    std::cout << elapsedTimeMin  << " min ";
    std::cout << elapsedTimeSec  << " sec";
    std::cout << std::endl;
}
