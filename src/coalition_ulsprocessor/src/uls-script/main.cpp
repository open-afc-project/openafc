#include "CsvWriter.h"
#include "UlsFileReader.h"
#include <QDebug>
#include <QStringList>
#include <limits>
#include <math.h>
#include <iostream>

#define VERSION "1.3.0"

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
    printf("WARNING: Emission designator contains no order of magnitude.\n");
    return -1;
  }

  QString num = frqPart.replace(unitS, ".");

  double number = num.toFloat() * multi;

  return number / 1e6; // Convert to MHz
}


// Ensures that all the necessary fields are available to determine if this goes in the regular ULS or anomalous file
// returns a blank string if valid, otherwise a string indicating what failed. 
QString hasNecessaryFields(const UlsEmission &e, UlsPath path, UlsLocation rxLoc, UlsLocation txLoc, UlsAntenna rxAnt, UlsAntenna txAnt, UlsHeader txHeader, UlsLocation prLoc, UlsAntenna prAnt) {
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
  if(txHeader.mobile == 'Y') {
    failReason.append("Mobile is Y, ");
  }

  // radio service code 
  if(strcmp(txHeader.radioServiceCode, "TP") == 0) {
    failReason.append("Radio service value of TP, ");
  }
  
  // check passive repeater
  if(path.passiveReceiver == 'Y') {
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

}; // namespace

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IONBF, 0);

  if (strcmp(argv[1], "--version") == 0) {
    printf("Coalition ULS Processing Tool Version %s\n", VERSION);
    printf("Compatible with ULS Database Version 4\n");
    printf("Spec: "
           "https://www.fcc.gov/sites/default/files/"
           "public_access_database_definitions_v4.pdf\n");
    return 0;
  }
  printf("Coalition ULS Processing Tool Version %s\n", VERSION);
  if (argc < 3 || argc > 3) {
    fprintf(stderr, "Syntax: %s [ULS file.csv] [Output File.csv]\n", argv[0]);
    return -1;
  }

  UlsFileReader r(argv[1]);

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

  CsvWriter wt(argv[2]);
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
    header << "Passive Receiver Indicator";
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
    header << "Rx Diversity Height (m)";
    header << "Rx Diversity Gain (dBi)";
    header << "Passive Repeater Location Name";
    header << "Passive Repeater Lat Coords";
    header << "Passive Repeater Long Coords";
    header << "Passive Repeater Ground Elevation (m)";
    header << "Passive Repeater Polarization";
    header << "Passive Repeater Azimuth Angle (deg)";
    header << "Passive Repeater Elevation Angle (deg)";
    header << "Passive Repeater Height to Center RAAT (m)";
    header << "Passive Repeater Beamwidth";
    header << "Passive Repeater Back-to-Back Gain Tx (dBi)";
    header << "Passive Repeater Back-to-Back Gain Rx (dBi)";
    wt.writeRow(header);
  }

  CsvWriter anomalous("anomalous_uls.csv");
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
    header << "Passive Receiver Indicator";
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
    header << "Rx Diversity Height (m)";
    header << "Rx Diversity Gain (dBi)";
    header << "Passive Repeater Location Name";
    header << "Passive Repeater Lat Coords";
    header << "Passive Repeater Long Coords";
    header << "Passive Repeater Ground Elevation (m)";
    header << "Passive Repeater Polarization";
    header << "Passive Repeater Azimuth Angle (deg)";
    header << "Passive Repeater Elevation Angle (deg)";
    header << "Passive Repeater Height to Center RAAT (m)";
    header << "Passive Repeater Beamwidth";
    header << "Passive Repeater Back-to-Back Gain Tx (dBi)";
    header << "Passive Repeater Back-to-Back Gain Rx (dBi)";
    header << "Anomalous Reason";
    anomalous.writeRow(header);
  }

  qDebug() << "--- Beginning path processing";

  int cnt = 0;
  int numRecs = 0;


  foreach (const UlsFrequency &freq, r.frequencies()) {
    // qDebug() << "processing frequency " << cnt << "/" << r.frequencies().count()
            //  << " callsign " << freq.callsign;
    UlsPath path;
    bool pathFound = false;
    bool hasRepeater = false;
    QString anomalousReason = ""; 

    foreach (const UlsPath &p, r.pathsMap(freq.callsign)) {
      if (strcmp(p.callsign, freq.callsign) == 0) {
        if (freq.locationNumber == p.txLocationNumber &&
            freq.antennaNumber == p.txAntennaNumber) {
          if (p.passiveReceiver == 'Y') {
            hasRepeater = true;
          }
          path = p;
          pathFound = true;
          break;
        }
      }
    }
    cnt++;

    if (pathFound == false) {
      // qDebug() << "no matching path found";
      continue;
    }

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
      // qDebug() << "Unable to find TX location for path" << path.callsign
      //          << "path ID" << path.pathNumber;
      continue;
    }

    /// Find the associated transmit antenna.
    UlsAntenna txAnt;
    bool txAntFound = false;
    foreach (const UlsAntenna &ant, r.antennasMap(path.callsign)) {
      if (strcmp(ant.callsign, path.callsign) == 0) {
        // Implicitly matches with an Antenna record with locationClass 'T'
        if (ant.locationNumber == txLoc.locationNumber) {
          if (ant.antennaNumber == path.txAntennaNumber) {
            txAnt = ant;
            txAntFound = true;
            break;
          }
        }
      }
    }

    if (txAntFound == false) {
      // qDebug() << "Unable to locate TX antenna data for callsign"
      //          << path.callsign << "path" << path.pathNumber;
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
      // qDebug() << "Unable to locate RX location data for callsign"
      //          << path.callsign << "path" << path.pathNumber;
      continue;
    }

    /// Find the RX antenna.
    UlsAntenna rxAnt;
    bool rxAntFound = false;
    foreach (const UlsAntenna &ant, r.antennasMap(path.callsign)) {
      if (strcmp(ant.callsign, path.callsign) == 0) {
        // Implicitly matches with an Antenna record with locationClass 'R'
        if (ant.locationNumber == rxLoc.locationNumber) {
          if (ant.antennaNumber == path.rxAntennaNumber) {
            rxAnt = ant;
            rxAntFound = true;
            break;
          }
        }
      }
    }
    if (rxAntFound == false) {
      // qDebug() << "Unable to locate RX antenna data for callsign"
      //          << path.callsign << "path" << path.pathNumber;
      continue;
    }

    // Only assigned if hasRepeater is True
    UlsLocation prLoc;
    UlsAntenna prAnt;

    if (hasRepeater) {
      /// Find the Passive Repeater Location
      foreach (const UlsLocation &loc, r.locationsMap(path.callsign)) {
        if (strcmp(loc.callsign, path.callsign) == 0) {
          if (loc.locationClass == 'P') {
            prLoc = loc;
            break;
          }
        }
      }

      /// Find the Passive Repeater Transmit Antenna
      foreach (const UlsAntenna &ant, r.antennasMap(path.callsign)) {
        if (strcmp(ant.callsign, path.callsign) == 0) {
          if (ant.antennaType == 'P' &&
              ant.locationNumber == prLoc.locationNumber) {
            prAnt = ant;
            break;
          }
        }
      }
    }

    /// Find the first segment.
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
      // skip if no overlap UNII5 and 7
      if (lowFreq >= 6875 || highFreq <= 5925) {
        continue;
      } 
      // skip if in restricted range 
      else if(lowFreq >= 6425 && highFreq <= 6525) {
        continue;
      }
      // entry is anomlous if either frequency value is NaN
      else if(isnan(lowFreq) || isnan(highFreq)) {
        anomalousReason.append("NaN frequency value, ");
      } 
      // skip if no overlap 10700 - 11700
      // if (lowFreq >= 11700 || highFreq <= 10700) {
      //   // qDebug() << "Skipping outside 10700-11700";
      //   continue;
      // }
      // else if(isnan(lowFreq) || isnan(highFreq)) {
      //   anomalousReason.append("NaN frequency value, ");
      // } 
      

      // now that we have everything, ensure that inputs have what we need
      anomalousReason.append(hasNecessaryFields(e, path, rxLoc, txLoc, rxAnt, txAnt, txHeader, prLoc, prAnt));



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
      row << charString(path.passiveReceiver);    // Passive Receiver Indicator
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
      row << makeNumber(rxAnt.diversityHeight); // Rx Diveristy Height (m)
      row << makeNumber(rxAnt.diversityGain);   // Rx Diversity Gain (dBi)
      if (hasRepeater) {
        row << prLoc.locationName;          // Passive Repeater Location Name
        row << makeNumber(prLoc.latitude);  // Passive Repeater Lat Coords
        row << makeNumber(prLoc.longitude); // Passive Repeater Lon Coords
        row << makeNumber(
            prLoc.groundElevation);       // Passive Repeater Ground Elevation
        row << prAnt.polarizationCode;    // Passive Repeater Polarization
        row << makeNumber(prAnt.azimuth); // Passive Repeater Azimuth Angle
        row << makeNumber(prAnt.tilt);    // Passive Repeater Elevation Angle
        row << makeNumber(
            prAnt.heightToCenterRAAT); // Passive Repeater Height to Center RAAT
        row << makeNumber(prAnt.beamwidth); // Passive Repeater Beamwidth
        row << makeNumber(
            prAnt.backtobackTxGain); // Passive Repeater Back-To-Back Tx Gain
        row << makeNumber(
            prAnt.backtobackRxGain); // Passive Repeater Back-To-Back Rx Gain
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
      }
      if(anomalousReason.length() > 0) {
        row << anomalousReason; 
        anomalous.writeRow(row);
        anomalousReason = "";
      } else {
        wt.writeRow(row);
        numRecs++;
      }
      
    }
  }
  qDebug() << "Processed" << r.frequencies().count()
           << "frequency records and output to file; a total of " << numRecs
           << "output";
  std::cout<<"Processed " << r.frequencies().count()
           << " frequency records and output to file; a total of " << numRecs
           << " output"<<'\n';
}
