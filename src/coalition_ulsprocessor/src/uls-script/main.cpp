#include <QDebug>
#include <QStringList>
#include <limits>
#include <math.h>
#include <iostream>

#include "CsvWriter.h"
#include "UlsFileReader.h"
#include "AntennaModelMap.h"
#include "FreqAssignment.h"
#include "UlsFunctions.h"

#define VERSION "1.3.0"

bool removeMobile = false;
bool includeUnii8 = false;

void testAntennaModelMap(AntennaModelMapClass &antennaModelMap, std::string inputFile, std::string outputFile);
void processUS(UlsFileReader &r, int maxNumPassiveRepeater, CsvWriter &wt, CsvWriter &anomalous, FILE *fwarn, AntennaModelMapClass &antennaModelMap, FreqAssignmentClass &freqAssignment);
void processCA(UlsFileReader &r, int maxNumPassiveRepeater, CsvWriter &wt, CsvWriter &anomalous, FILE *fwarn, AntennaModelMapClass &antennaModelMap);
void makeLink(const StationDataCAClass &station, const QList<PassiveRepeaterCAClass> &prList, std::vector<int> &idxList, double &azimuthPtg, double &elevationPtg);

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
    if (argc != 7) {
        fprintf(stderr, "Syntax: %s [ULS file.csv] [Output File.csv] [AntModelListFile.csv] [AntModelMapFile.csv] [mode]\n", argv[0]);
        return -1;
    }

    char *tstr;

    time_t t1 = time(NULL);
    tstr = strdup(ctime(&t1));
    strtok(tstr, "\n");
    std::cout << tstr << " : Begin processing." << std::endl;
    free(tstr);

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];
    std::string antModelListFile = argv[3];
    std::string antModelMapFile = argv[4];
    std::string freqAssignmentFile = argv[5];
    std::string mode = argv[6];

    FILE *fwarn;
    std::string warningFile = "warning_uls.txt";
    if ( !(fwarn = fopen(warningFile.c_str(), "wb")) ) {
        std::cout << std::string("WARNING: Unable to open warningFile \"") + warningFile + std::string("\"\n");
    }

	AntennaModelMapClass antennaModelMap(antModelListFile, antModelMapFile);

	FreqAssignmentClass fccFreqAssignment(freqAssignmentFile);

    if (mode == "test_antenna_model_map") {
        testAntennaModelMap(antennaModelMap, inputFile, outputFile);
        return 0;
    } else if (mode == "proc_uls") {
        // Do nothing
    } else if (mode == "proc_uls_include_unii8") {
        includeUnii8 = true;
    } else {
        fprintf(stderr, "ERROR: Invalid mode: %s\n", mode.c_str());
        return -1;
    }

    UlsFileReader r(inputFile.c_str(), fwarn);

    int maxNumPRUS = r.computeStatisticsUS(fccFreqAssignment, includeUnii8);
    int maxNumPRCA = r.computeStatisticsCA(fwarn);
    int maxNumPassiveRepeater = (maxNumPRUS > maxNumPRCA ? maxNumPRUS : maxNumPRCA);

    std::cout << "US Max Num Passive Repeater: " << maxNumPRUS << std::endl;
    std::cout << "CA Max Num Passive Repeater: " << maxNumPRCA << std::endl;
    std::cout << "Max Num Passive Repeater: "    << maxNumPassiveRepeater << std::endl;

    CsvWriter wt(outputFile.c_str());
    {
        QStringList header = UlsFunctionsClass::getCSVHeader(maxNumPassiveRepeater);
        wt.writeRow(header);
    }

    CsvWriter anomalous("anomalous_uls.csv");
    {
        QStringList header = UlsFunctionsClass::getCSVHeader(maxNumPassiveRepeater);
        header << "Fixed";
        header << "Anomalous Reason";
        anomalous.writeRow(header);
    }

    processUS(r, maxNumPassiveRepeater, wt, anomalous, fwarn, antennaModelMap, fccFreqAssignment);

    processCA(r, maxNumPassiveRepeater, wt, anomalous, fwarn, antennaModelMap);

    if (fwarn) {
        fclose(fwarn);
    }

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
/******************************************************************************************/

/******************************************************************************************/
/**** processUS                                                                        ****/
/******************************************************************************************/
void processUS(UlsFileReader &r, int maxNumPassiveRepeater, CsvWriter &wt, CsvWriter &anomalous, FILE *fwarn, AntennaModelMapClass &antennaModelMap, FreqAssignmentClass &freqAssignment)
{
    int prIdx;

    qDebug() << "--- Beginning path processing";

    int cnt = 0;
    int numRecs = 0;

    int numAntMatch = 0;
    int numAntUnmatch = 0;
    int numMissingRxAntHeight = 0;
    int numMissingTxAntHeight = 0;


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
            qSort(segList.begin(), segList.end(), UlsFunctionsClass::SegmentCompare);

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
            if (!txEmFound) {
                allTxEm << txEm; // Make sure at least one emission.
            }

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
                double bwMHz = std::numeric_limits<double>::quiet_NaN();
                double lowFreq, highFreq;
                if (txEmFound) {
                    bwMHz = UlsFunctionsClass::emissionDesignatorToBandwidth(e.desig);
                    if ( (bwMHz == -1.0) || (bwMHz > 60.0) || (bwMHz == 0) ) {
                        bwMHz = freqAssignment.getBandwidth(txFreq.frequencyAssigned);
                    }
                    if (bwMHz == -1.0) {
                        anomalousReason.append("Unable to get bandwidth");
                    }
                }
                if (txFreq.frequencyUpperBand > txFreq.frequencyAssigned) {
                    lowFreq = txFreq.frequencyAssigned;  // Lower Band (MHz)
                    highFreq = txFreq.frequencyUpperBand; // Upper Band (MHz)
                    // Here Frequency Assigned should be low + high / 2
                } else {
                    lowFreq  = txFreq.frequencyAssigned - bwMHz / 2.0; // Lower Band (MHz)
                    highFreq = txFreq.frequencyAssigned + bwMHz / 2.0; // Upper Band (MHz)
                }

                AntennaModelClass *rxAntModel = antennaModelMap.find(std::string(rxAnt.antennaModel));

                AntennaModelClass::CategoryEnum rxAntennaCategory;
                double rxAntennaDiameter;
                double rxDiversityDiameter;
                double rxAntennaMidbandGain;
                std::string rxAntennaModelName;
                if (rxAntModel) {
                    numAntMatch++;
                    rxAntennaModelName = rxAntModel->name;
                    rxAntennaCategory = rxAntModel->category;
                    rxAntennaDiameter = rxAntModel->diameterM;
                    rxDiversityDiameter = rxAntModel->diameterM;
                    rxAntennaMidbandGain = rxAntModel->midbandGain;
                } else {
                    numAntUnmatch++;
                    rxAntennaModelName = "";
                    rxAntennaCategory = AntennaModelClass::UnknownCategory;
                    rxAntennaDiameter = -1.0;
                    rxDiversityDiameter = -1.0;
                    rxAntennaMidbandGain = std::numeric_limits<double>::quiet_NaN();
                    fixedReason.append("Rx Antenna Model Unmatched");
                }

                AntennaModelClass *txAntModel = antennaModelMap.find(std::string(txAnt.antennaModel));

                AntennaModelClass::CategoryEnum txAntennaCategory;
                double txAntennaDiameter;
                double txAntennaMidbandGain;
                std::string txAntennaModelName;
                if (txAntModel) {
                    numAntMatch++;
                    txAntennaModelName = txAntModel->name;
                    txAntennaCategory = txAntModel->category;
                    txAntennaDiameter = txAntModel->diameterM;
                    txAntennaMidbandGain = txAntModel->midbandGain;
                } else {
                    numAntUnmatch++;
                    txAntennaModelName = "";
                    txAntennaCategory = AntennaModelClass::UnknownCategory;
                    txAntennaDiameter = -1.0;
                    txAntennaMidbandGain = std::numeric_limits<double>::quiet_NaN();
                    fixedReason.append("Tx Antenna Model Unmatched");
                }

                if(isnan(lowFreq) || isnan(highFreq)) {
                    anomalousReason.append("NaN frequency value, ");
                } else {
                    bool overlapUnii5 = (highFreq > UlsFunctionsClass::unii5StartFreqMHz) && (lowFreq < UlsFunctionsClass::unii5StopFreqMHz);
                    bool overlapUnii7 = (highFreq > UlsFunctionsClass::unii7StartFreqMHz) && (lowFreq < UlsFunctionsClass::unii7StopFreqMHz);
                    bool overlapUnii8 = (highFreq > UlsFunctionsClass::unii8StartFreqMHz) && (lowFreq < UlsFunctionsClass::unii8StopFreqMHz);

                    if (!(overlapUnii5 || overlapUnii7 || (includeUnii8 && overlapUnii8))) {
                        continue;
                    } else if (overlapUnii5 && overlapUnii7) {
                        anomalousReason.append("Band overlaps both Unii5 and Unii7, ");
                    }
                }

                if (std::isnan(rxAnt.heightToCenterRAAT)) {
                    rxAnt.heightToCenterRAAT = -1.0;
                    numMissingRxAntHeight++;
                } else if (rxAnt.heightToCenterRAAT < 1.5) {
                    rxAnt.heightToCenterRAAT = 1.5;
                }

                if (std::isnan(txAnt.heightToCenterRAAT)) {
                    txAnt.heightToCenterRAAT = -1.0;
                    numMissingTxAntHeight++;
                } else if (txAnt.heightToCenterRAAT < 1.5) {
                    txAnt.heightToCenterRAAT = 1.5;
                }

                // now that we have everything, ensure that inputs have what we need
                anomalousReason.append(UlsFunctionsClass::hasNecessaryFields(e, path, rxLoc, txLoc, rxAnt, txAnt, txHeader, prLocList, prAntList, removeMobile));

                QStringList row;
                row << "US";                            // Region
                row << path.callsign;                   // Callsign
                row << QString(txHeader.licenseStatus); // Status
                row << txHeader.radioServiceCode;       // Radio Service
                row << txEntity.entityName;             // Entity Name
                row << txEntity.frn;                    // FRN: Fcc Registration Number
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
                row << UlsFunctionsClass::charString(txHeader.commonCarrier);    // Common Carrier
                row << UlsFunctionsClass::charString(txHeader.nonCommonCarrier); // Non Common Carrier
                row << UlsFunctionsClass::charString(txHeader.privateCarrier);   // Private Comm
                row << UlsFunctionsClass::charString(txHeader.fixed);            // Fixed
                row << UlsFunctionsClass::charString(txHeader.mobile);           // Mobile
                row << UlsFunctionsClass::charString(txHeader.radiolocation);    // Radiolocation
                row << UlsFunctionsClass::charString(txHeader.satellite);        // Satellite
                row << UlsFunctionsClass::charString(txHeader.developmental); // Developmental or STA or Demo
                row << UlsFunctionsClass::charString(txHeader.interconnected); // Interconnected
                row << UlsFunctionsClass::makeNumber(path.pathNumber);         // Path Number
                row << UlsFunctionsClass::makeNumber(path.txLocationNumber);   // Tx Location Number
                row << UlsFunctionsClass::makeNumber(path.txAntennaNumber);    // Tx Antenna Number
                row << path.rxCallsign;                     // Rx Callsign
                row << UlsFunctionsClass::makeNumber(path.rxLocationNumber);   // Rx Location Number
                row << UlsFunctionsClass::makeNumber(path.rxAntennaNumber);    // Rx Antenna Number
                row << UlsFunctionsClass::makeNumber(txFreq.frequencyNumber);  // Frequency Number
                if (txSegFound) {
                    row << UlsFunctionsClass::makeNumber(txSeg.segmentLength); // 1st Segment Length (km)
                } else {
                    row << "";
                }

                {
                    QString _freq;
                    // If this is true, frequencyAssigned IS the lower band
                    if (txFreq.frequencyUpperBand > txFreq.frequencyAssigned) {
                        _freq = QString("%1").arg( (txFreq.frequencyAssigned + txFreq.frequencyUpperBand) / 2);
                    } else {
                        _freq = QString("%1").arg(txFreq.frequencyAssigned);
                    }
                    row << _freq; // Center Frequency (MHz)
                }
      
                {
                    if (txEmFound) {
                        row << UlsFunctionsClass::makeNumber(bwMHz); // Bandiwdth (MHz)
                    } else {
                        row << "";
                    }
                    row << UlsFunctionsClass::makeNumber(lowFreq); // Lower Band (MHz)
                    row << UlsFunctionsClass::makeNumber(highFreq); // Upper Band (MHz)
                }

                row << UlsFunctionsClass::makeNumber(txFreq.tolerance);               // Tolerance (%)
                row << UlsFunctionsClass::makeNumber(txFreq.EIRP);                    // Tx EIRP (dBm)
                row << UlsFunctionsClass::charString(txFreq.transmitterPowerControl); // Auto Tx Pwr Control
                if (txEmFound) {
                    row << QString(e.desig);      // Emissions Designator
                    row << UlsFunctionsClass::makeNumber(e.modRate); // Digital Mod Rate
                    row << QString(e.modCode);    // Digital Mod Type
                } else {
                    row << ""
                        << ""
                        << "";
                }

                row << txFreq.transmitterMake;  // Tx Manufacturer
                row << txFreq.transmitterModel; // Tx Model
                row << txLoc.locationName;      // Tx Location Name
                row << UlsFunctionsClass::makeNumber(txLoc.latitude);
                // QString("%1-%2-%3
                // %4").arg(txLoc.latitudeDeg).arg(txLoc.latitudeMinutes).arg(txLoc.latitudeSeconds).arg(txLoc.latitudeDirection);
                // // Tx Lat Coords
                row << UlsFunctionsClass::makeNumber(txLoc.longitude);
                // QString("%1-%2-%3
                // %4").arg(txLoc.longitudeDeg).arg(txLoc.longitudeMinutes).arg(txLoc.longitudeSeconds).arg(txLoc.longitudeDirection);
                // // Tx Lon Coords
                row << UlsFunctionsClass::makeNumber(txLoc.groundElevation); // Tx Ground Elevation (m)
                row << txAnt.polarizationCode;            // Tx Polarization
                row << UlsFunctionsClass::makeNumber(txAnt.azimuth);         // Tx Azimuth Angle (deg)
                row << UlsFunctionsClass::makeNumber(txAnt.tilt);            // Tx Elevation Angle (deg)
                row << txAnt.antennaMake;                 // Tx Ant Manufacturer
                row << txAnt.antennaModel;                // Tx Ant Model
                row << txAntennaModelName.c_str();            // Tx Matched antenna model (blank if unmatched)
                row << AntennaModelClass::categoryStr(txAntennaCategory).c_str(); // Tx Antenna category
                row << UlsFunctionsClass::makeNumber(txAntennaDiameter);     // Tx Ant Diameter (m)
                row << UlsFunctionsClass::makeNumber(txAntennaMidbandGain);  // Tx Ant Midband Gain (dB)
                row << UlsFunctionsClass::makeNumber(txAnt.heightToCenterRAAT); // Tx Height to Center RAAT (m)
                row << UlsFunctionsClass::makeNumber(txAnt.beamwidth); // Tx Beamwidth
                row << UlsFunctionsClass::makeNumber(txAnt.gain);      // Tx Gain (dBi)
                row << rxLoc.locationName;          // Rx Location Name
                row << UlsFunctionsClass::makeNumber(rxLoc.latitude);
                // QString("%1-%2-%3
                // %4").arg(rxLoc.latitudeDeg).arg(rxLoc.latitudeMinutes).arg(rxLoc.latitudeSeconds).arg(rxLoc.latitudeDirection);
                // // Rx Lat Coords
                row << UlsFunctionsClass::makeNumber(rxLoc.longitude);
                // QString("%1-%2-%3
                // %4").arg(rxLoc.longitudeDeg).arg(rxLoc.longitudeMinutes).arg(rxLoc.longitudeSeconds).arg(rxLoc.longitudeDirection);
                // // Rx Lon Coords
                row << UlsFunctionsClass::makeNumber(rxLoc.groundElevation); // Rx Ground Elevation (m)
                row << "";                                // Rx Manufacturer
                row << "";                                // Rx Model
                row << rxAnt.antennaMake;                 // Rx Ant Manufacturer
                row << rxAnt.antennaModel;                // Rx Ant Model
                row << rxAntennaModelName.c_str();            // Rx Matched antenna model (blank if unmatched)
                row << AntennaModelClass::categoryStr(rxAntennaCategory).c_str(); // Rx Antenna category
                row << UlsFunctionsClass::makeNumber(rxAntennaDiameter);     // Rx Ant Diameter (m)
                row << UlsFunctionsClass::makeNumber(rxAntennaMidbandGain);  // Rx Ant Midband Gain (dB)
                row << UlsFunctionsClass::makeNumber(rxAnt.lineLoss);        // Rx Line Loss (dB)
                row << UlsFunctionsClass::makeNumber(rxAnt.heightToCenterRAAT);  // Rx Height to Center RAAT (m)
                row << UlsFunctionsClass::makeNumber(rxAnt.gain);            // Rx Gain (dBi)
                row << UlsFunctionsClass::makeNumber(rxAnt.diversityHeight); // Rx Diveristy Height (m)
                row << UlsFunctionsClass::makeNumber(rxDiversityDiameter);   // Rx Diversity Diameter (m)
                row << UlsFunctionsClass::makeNumber(rxAnt.diversityGain);   // Rx Diversity Gain (dBi)

                row << QString::number(prLocList.size());
                for(prIdx=1; prIdx<=maxNumPassiveRepeater; ++prIdx) {

                    if (    (prIdx <= prLocList.size())
                         && (prIdx <= prAntList.size()) ) {
                        UlsLocation &prLoc = prLocList[prIdx-1];
                        UlsAntenna &prAnt = prAntList[prIdx-1];
                        UlsSegment &segment = segList[prIdx];

			            AntennaModelClass *prAntModel = antennaModelMap.find(std::string(prAnt.antennaModel));

                        AntennaModelClass::TypeEnum prAntennaType;
                        AntennaModelClass::CategoryEnum prAntennaCategory;
                        double prAntennaDiameter;
                        double prAntennaMidbandGain;
                        double prAntennaReflectorWidth;
                        double prAntennaReflectorHeight;
                        std::string prAntennaModelName;

                        if (prAntModel) {
                            numAntMatch++;
                            prAntennaModelName = prAntModel->name;
                            prAntennaType = prAntModel->type;
                            prAntennaCategory = prAntModel->category;
                            prAntennaDiameter = prAntModel->diameterM;
                            prAntennaMidbandGain = prAntModel->midbandGain;
                            prAntennaReflectorWidth = prAntModel->reflectorWidthM;
                            prAntennaReflectorHeight = prAntModel->reflectorHeightM;
                        } else {
                            numAntUnmatch++;
                            prAntennaModelName = "";
                            prAntennaType = AntennaModelClass::UnknownType;
                            prAntennaCategory = AntennaModelClass::UnknownCategory;
                            prAntennaDiameter = -1.0;
                            prAntennaMidbandGain = std::numeric_limits<double>::quiet_NaN();
                            prAntennaReflectorWidth = -1.0;
                            prAntennaReflectorHeight = -1.0;
                            fixedReason.append("PR Antenna Model Unmatched");
                        }

                        row << prLoc.locationName;          // Passive Repeater Location Name
                        row << UlsFunctionsClass::makeNumber(prLoc.latitude);  // Passive Repeater Lat Coords
                        row << UlsFunctionsClass::makeNumber(prLoc.longitude); // Passive Repeater Lon Coords
                        row << UlsFunctionsClass::makeNumber(prLoc.groundElevation);       // Passive Repeater Ground Elevation
                        row << prAnt.polarizationCode;    // Passive Repeater Polarization
                        row << UlsFunctionsClass::makeNumber(prAnt.azimuth); // Passive Repeater Azimuth Angle
                        row << UlsFunctionsClass::makeNumber(prAnt.tilt);    // Passive Repeater Elevation Angle
                        row << prAnt.antennaMake;         // Passive Repeater Ant Make
                        row << prAnt.antennaModel;        // Passive Repeater Ant Model
                        row << prAntennaModelName.c_str();        // Passive Repeater antenna model (blank if unmatched)
                        row << AntennaModelClass::typeStr(prAntennaType).c_str(); // Passive Repeater Ant Type
                        row << AntennaModelClass::categoryStr(prAntennaCategory).c_str(); // Passive Repeater Ant Category
                        row << UlsFunctionsClass::makeNumber(prAnt.backtobackTxGain); // Passive Repeater Back-To-Back Tx Gain
                        row << UlsFunctionsClass::makeNumber(prAnt.backtobackRxGain); // Passive Repeater Back-To-Back Rx Gain
                        row << UlsFunctionsClass::makeNumber(prAnt.reflectorHeight); // Passive Repeater ULS Reflector Height
                        row << UlsFunctionsClass::makeNumber(prAnt.reflectorWidth);  // Passive Repeater ULS Reflector Width
                        row << UlsFunctionsClass::makeNumber(prAntennaDiameter);     // Passive Repeater Ant Model Diameter (m)
                        row << UlsFunctionsClass::makeNumber(prAntennaMidbandGain);  // Passive Repeater Ant Model Midband Gain (dB)
                        row << UlsFunctionsClass::makeNumber(prAntennaReflectorHeight); // Passive Repeater Ant Model Reflector Height
                        row << UlsFunctionsClass::makeNumber(prAntennaReflectorWidth);  // Passive Repeater Ant Model Reflector Width
                        row << UlsFunctionsClass::makeNumber(prAnt.lineLoss);        // Passive Repeater Line Loss
                        row << UlsFunctionsClass::makeNumber(prAnt.heightToCenterRAAT); // Passive Repeater Height to Center RAAT
                        row << UlsFunctionsClass::makeNumber(prAnt.beamwidth); // Passive Repeater Beamwidth
                        row << UlsFunctionsClass::makeNumber(segment.segmentLength); // Segment Length (km)
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

    std::cout << "US Num Antenna Matched: " << numAntMatch << std::endl;
    std::cout << "US Num Antenna Not Matched: " << numAntUnmatch << std::endl;
    std::cout << "US NUM Missing Rx Antenna Height: " << numMissingRxAntHeight << std::endl;
    std::cout << "US NUM Missing Tx Antenna Height: " << numMissingTxAntHeight << std::endl;

    std::cout<<"US Processed " << r.frequencies().count()
             << " frequency records and output to file; a total of " << numRecs
             << " output"<<'\n';

}
/******************************************************************************************/

/******************************************************************************************/
/**** processCA                                                                        ****/
/******************************************************************************************/
void processCA(UlsFileReader &r, int maxNumPassiveRepeater, CsvWriter &wt, CsvWriter &anomalous, FILE *fwarn, AntennaModelMapClass &antennaModelMap)
{
    int prIdx;

    qDebug() << "--- Beginning path processing";

    int cnt = 0;
    int numRecs = 0;

    int numAntMatch = 0;
    int numAntUnmatch = 0;
    int numMissingRxAntHeight = 0;
    int numMissingTxAntHeight = 0;

    for (std::string authorizationNumber : r.authorizationNumberList) {

        const QList<PassiveRepeaterCAClass> &prList = r.passiveRepeatersMap(authorizationNumber.c_str());

        int numPR = prList.size();
        std::vector<int> idxList(numPR);

        foreach (const StationDataCAClass &station, r.stationsMap(authorizationNumber.c_str())) {
            QString anomalousReason = ""; 
            QString fixedReason = ""; 

            switch (station.service) {
                case 2:
                    // Do Nothing
                    break;
                case 9:
                    // Skip this, satellite service
                    continue;
                    break;
                default:
                    // Invalid service put in anomalous file
                    anomalousReason.append("Invalid Service, ");
                    break;
            }

            double lowFreq  = station.centerFreqMHz - station.bandwidthMHz/2; // Lower Band (MHz)
            double highFreq = station.centerFreqMHz + station.bandwidthMHz/2; // Upper Band (MHz)

            if(isnan(lowFreq) || isnan(highFreq)) {
                anomalousReason.append("NaN frequency value, ");
            } else {
                bool overlapUnii5 = (highFreq > UlsFunctionsClass::unii5StartFreqMHz) && (lowFreq < UlsFunctionsClass::unii5StopFreqMHz);
                bool overlapUnii6 = (highFreq > UlsFunctionsClass::unii6StartFreqMHz) && (lowFreq < UlsFunctionsClass::unii6StopFreqMHz);
                bool overlapUnii7 = (highFreq > UlsFunctionsClass::unii7StartFreqMHz) && (lowFreq < UlsFunctionsClass::unii7StopFreqMHz);
                bool overlapUnii8 = (highFreq > UlsFunctionsClass::unii8StartFreqMHz) && (lowFreq < UlsFunctionsClass::unii8StopFreqMHz);

                if (!(overlapUnii5 || overlapUnii7 || overlapUnii6 || (includeUnii8 && overlapUnii8))) {
                    anomalousReason.append("Out of band, ");
                } else if (overlapUnii5 && overlapUnii7) {
                    anomalousReason.append("Band overlaps both Unii5 and Unii7, ");
                }
            }

            double azimuthPtg, elevationPtg;

            makeLink(station, prList, idxList, azimuthPtg, elevationPtg);

            AntennaModelClass *rxAntModel = antennaModelMap.find(std::string(station.antennaModel));

            AntennaModelClass::CategoryEnum rxAntennaCategory;
            double rxAntennaDiameter;
            double rxDiversityDiameter;
            double rxAntennaMidbandGain;
            std::string rxAntennaModelName;
            if (rxAntModel) {
                numAntMatch++;
                rxAntennaModelName = rxAntModel->name;
                rxAntennaCategory = rxAntModel->category;
                rxAntennaDiameter = rxAntModel->diameterM;
                rxDiversityDiameter = rxAntModel->diameterM;
                rxAntennaMidbandGain = rxAntModel->midbandGain;
            } else {
                numAntUnmatch++;
                rxAntennaModelName = "";
                rxAntennaCategory = AntennaModelClass::UnknownCategory;
                rxAntennaDiameter = -1.0;
                rxDiversityDiameter = -1.0;
                rxAntennaMidbandGain = std::numeric_limits<double>::quiet_NaN();
                fixedReason.append("Rx Antenna Model Unmatched");
            }

            QStringList row;
            row << "CA";                            // Region
            row << QString::fromStdString(station.callsign);                // Callsign
            row << QString("A");                // Status
            row << "";                          // Radio Service
            row << "";                          // Entity Name
            row << QString::fromStdString(authorizationNumber);         // FRN: Fcc Registration Number / CA: authorizationNumber
            row << "";                          // Grant
            row << "";                          // Expiration
            row << station.inServiceDate.c_str();                       // Effective
            row << ""                           // Address
                << ""                           // City
                << ""                           // County
                << "";                          // State
            row << "";                          // Common Carrier
            row << "";                          // Non Common Carrier
            row << "";                          // Private Comm
            row << "Y";                         // Fixed
            row << "N";                         // Mobile
            row << "N";                         // Radiolocation
            row << "N";                         // Satellite
            row << "N";                         // Developmental or STA or Demo
            row << "";                          // Interconnected
            row << "1";                         // Path Number
            row << "1";                         // Tx Location Number
            row << "1";                         // Tx Antenna Number
            row << "";                          // Rx Callsign
            row << "1";                         // Rx Location Number
            row << "1";                         // Rx Antenna Number
            row << "1";                         // Frequency Number
            row << "";                          // 1st Segment Length (km)

            row << UlsFunctionsClass::makeNumber(station.centerFreqMHz); // Center Frequency (MHz)
            row << UlsFunctionsClass::makeNumber(station.bandwidthMHz); // Bandiwdth (MHz)
  
            row << UlsFunctionsClass::makeNumber(station.centerFreqMHz - station.bandwidthMHz/2); // Lower Band (MHz)
            row << UlsFunctionsClass::makeNumber(station.centerFreqMHz + station.bandwidthMHz/2); // Upper Band (MHz)

            row << "";                          // Tolerance (%)
            row << "";                          // Tx EIRP (dBm)
            row << "";                          // Auto Tx Pwr Control
            row << station.emissionsDesignator.c_str(); // Emissions Designator
            row << "";                                  // Digital Mod Rate
            row << station.modulation.c_str();  // Digital Mod Type

            row << "";                          // Tx Manufacturer
            row << "";                          // Tx Model
            row << "";                          // Tx Location Name
            row << "";                          // Tx Latitude
            row << "";                          // Tx Longitude
            row << "";                          // Tx Ground Elevation (m)
            row << "";                          // Tx Polarization
            row << UlsFunctionsClass::makeNumber(azimuthPtg);   // Tx Azimuth Angle (deg)
            row << UlsFunctionsClass::makeNumber(elevationPtg); // Tx Elevation Angle (deg)
            row << "";                          // Tx Ant Manufacturer
            row << "";                          // Tx Ant Model
            row << "";                          // Tx Matched antenna model (blank if unmatched)
            row << "";                          // Tx Antenna category
            row << "";                          // Tx Ant Diameter (m)
            row << "";                          // Tx Ant Midband Gain (dB)
            row << "-1";                        // Tx Height to Center RAAT (m)
            row << "";                          // Tx Beamwidth
            row << "";                          // Tx Gain (dBi)
            row << station.stationLocation.c_str();                      // Rx Location Name
            row << UlsFunctionsClass::makeNumber(station.latitudeDeg);   // Rx Latitude (deg)
            row << UlsFunctionsClass::makeNumber(station.longitudeDeg);  // Rx Longitude (deg)
            row << UlsFunctionsClass::makeNumber(station.groundElevation); // Rx Ground Elevation (m)
            row << "";                                // Rx Manufacturer
            row << "";                                // Rx Model
            row << station.antennaManufacturer.c_str();  // Rx Ant Manufacturer
            row << station.antennaModel.c_str();      // Rx Ant Model
            row << rxAntennaModelName.c_str();            // Rx Matched antenna model (blank if unmatched)
            row << AntennaModelClass::categoryStr(rxAntennaCategory).c_str(); // Rx Antenna category
            row << UlsFunctionsClass::makeNumber(rxAntennaDiameter);     // Rx Ant Diameter (m)
            row << UlsFunctionsClass::makeNumber(rxAntennaMidbandGain);  // Rx Ant Midband Gain (dB)
            row << UlsFunctionsClass::makeNumber(station.lineLoss);      // Rx Line Loss (dB)
            row << UlsFunctionsClass::makeNumber(station.antennaHeightAGL);  // Rx Height to Center RAAT (m)
            row << UlsFunctionsClass::makeNumber(station.antennaGain);   // Rx Gain (dBi)
            row << "";                                                   // Rx Diveristy Height (m)
            row << "";                                                   // Rx Diversity Diameter (m)
            row << "";                                                   // Rx Diversity Gain (dBi)

            row << QString::number(numPR);                               // Num Passive Repeater
            for(int prCount=1; prCount<=maxNumPassiveRepeater; ++prCount) {

                if (prCount <= numPR) {
                    int idxVal = idxList[numPR - prCount];

                    bool repFlag = (idxVal & 0x01 ? false : true);

                    prIdx = idxVal / 2;

                    const PassiveRepeaterCAClass &pr = prList[prIdx];

                    AntennaModelClass *prAntModel = antennaModelMap.find(std::string(pr.antennaModelA));

                    PassiveRepeaterCAClass::PRTypeEnum prAntennaType;
                    AntennaModelClass::CategoryEnum prAntennaCategory;
                    double prAntennaDiameter;
                    double prAntennaMidbandGain;
                    double prAntennaReflectorWidth;
                    double prAntennaReflectorHeight;
                    std::string prAntennaModelName;

                    if (prAntModel) {
                        numAntMatch++;
                        prAntennaModelName = prAntModel->name;
                        prAntennaCategory = prAntModel->category;
                        prAntennaDiameter = prAntModel->diameterM;
                        prAntennaMidbandGain = prAntModel->midbandGain;
                        prAntennaReflectorWidth = prAntModel->reflectorWidthM;
                        prAntennaReflectorHeight = prAntModel->reflectorHeightM;
                    } else {
                        numAntUnmatch++;
                        prAntennaModelName = "";
                        prAntennaCategory = AntennaModelClass::UnknownCategory;
                        prAntennaDiameter = -1.0;
                        prAntennaMidbandGain = std::numeric_limits<double>::quiet_NaN();
                        prAntennaReflectorWidth = -1.0;
                        prAntennaReflectorHeight = -1.0;
                        fixedReason.append("PR Antenna Model Unmatched");
                    }

                    prAntennaType = pr.type;

                    std::string prAntennaTypeStr;
                    switch (prAntennaType) {
                        case PassiveRepeaterCAClass::backToBackAntennaPRType:
                            prAntennaTypeStr = "Ant";
                            break;
                        case PassiveRepeaterCAClass::billboardReflectorPRType:
                            prAntennaTypeStr = "Ref";
                            break;
                        case PassiveRepeaterCAClass::unknownPRType:
                            prAntennaTypeStr = "UNKNOWN";
                            break;
                        default:
                            break;
                    }


                    row << "";                          // Passive Repeater Location Name
                    row << UlsFunctionsClass::makeNumber(pr.latitudeDeg);  // Passive Repeater Lat Coords
                    row << UlsFunctionsClass::makeNumber(pr.longitudeDeg); // Passive Repeater Lon Coords
                    row << UlsFunctionsClass::makeNumber(pr.groundElevation);       // Passive Repeater Ground Elevation
                    row << "";                        // Passive Repeater Polarization
                    row << "";                        // Passive Repeater Azimuth Angle
                    row << "";                        // Passive Repeater Elevation Angle
                    row << "";                        // Passive Repeater Ant Make
                    row << pr.antennaModelA.c_str();          // Passive Repeater Ant Model
                    row << prAntennaModelName.c_str();        // Passive Repeater antenna model (blank if unmatched)
                    row << prAntennaTypeStr.c_str();          // Passive Repeater Ant Type
                    row << AntennaModelClass::categoryStr(prAntennaCategory).c_str(); // Passive Repeater Ant Category
                    row << UlsFunctionsClass::makeNumber(repFlag ? pr.antennaGainA : pr.antennaGainB); // Passive Repeater Back-To-Back Tx Gain
                    row << UlsFunctionsClass::makeNumber(repFlag ? pr.antennaGainB : pr.antennaGainA); // Passive Repeater Back-To-Back Rx Gain
                    row << UlsFunctionsClass::makeNumber(pr.reflectorHeight); // Passive Repeater ULS Reflector Height
                    row << UlsFunctionsClass::makeNumber(pr.reflectorWidth);  // Passive Repeater ULS Reflector Width
                    row << UlsFunctionsClass::makeNumber(prAntennaDiameter);     // Passive Repeater Ant Model Diameter (m)
                    row << UlsFunctionsClass::makeNumber(prAntennaMidbandGain);  // Passive Repeater Ant Model Midband Gain (dB)
                    row << UlsFunctionsClass::makeNumber(prAntennaReflectorHeight); // Passive Repeater Ant Model Reflector Height
                    row << UlsFunctionsClass::makeNumber(prAntennaReflectorWidth);  // Passive Repeater Ant Model Reflector Width
                    row << "";                                                   // Passive Repeater Line Loss
                    row << UlsFunctionsClass::makeNumber(pr.heightAGLA);         // Passive Repeater Height to Center RAAT
                    row << "";                                             // Passive Repeater Beamwidth
                    row << "";                                                   // Segment Length (km)
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
/******************************************************************************************/

/******************************************************************************************/
/**** makeLink                                                                         ****/
/******************************************************************************************/
void makeLink(const StationDataCAClass &station, const QList<PassiveRepeaterCAClass> &prList, std::vector<int> &idxList, double &azimuthPtg, double &elevationPtg)
{
    idxList.clear();

    std::vector<int> unassignedIdxList;
    for(int prIdx=0; prIdx<prList.size(); ++prIdx) {
        unassignedIdxList.push_back(prIdx);
    }

    Vector3 position = station.position;
    Vector3 pointingVec = station.pointingVec;
    azimuthPtg = station.azimuthPtg;
    elevationPtg = station.elevationPtg;
    while(unassignedIdxList.size()) {
        int prIdx, selUIdx;
        double maxMetric;
        for(int uIdx=0; uIdx<unassignedIdxList.size(); ++uIdx) {
            prIdx = unassignedIdxList[uIdx];
            Vector3 prPosition;
            const PassiveRepeaterCAClass &pr = prList[prIdx];
            if (pr.type == PassiveRepeaterCAClass::backToBackAntennaPRType) {
                prPosition = (pr.positionA + pr.positionB)/2;
            } else if (pr.type == PassiveRepeaterCAClass::billboardReflectorPRType) {
                prPosition = pr.reflectorPosition;
            } else {
                CORE_DUMP;
            }
            Vector3 pVec = (prPosition - position).normalized();
            double metric = pVec.dot(pointingVec);
            if ((uIdx==0) || (metric > maxMetric)) {
                maxMetric = metric;
                selUIdx = uIdx;
            }
        }
        prIdx = unassignedIdxList[selUIdx];
        const PassiveRepeaterCAClass &pr = prList[prIdx];
        if (selUIdx < unassignedIdxList.size()-1) {
            unassignedIdxList[selUIdx] = unassignedIdxList[unassignedIdxList.size()-1];
        }
        unassignedIdxList.pop_back();
        Vector3 prPosition;
        if (pr.type == PassiveRepeaterCAClass::backToBackAntennaPRType) {
            prPosition = (pr.positionA + pr.positionB)/2;
            Vector3 pVec = (position - prPosition).normalized();
            double metricA = pVec.dot(pr.pointingVecA);
            double metricB = pVec.dot(pr.pointingVecB);
            if (metricA > metricB) {
                idxList.push_back(2*prIdx);
                pointingVec = pr.pointingVecB;
                azimuthPtg = pr.azimuthPtgB;
                elevationPtg = pr.elevationPtgB;
            } else {
                idxList.push_back(2*prIdx+1);
                pointingVec = pr.pointingVecA;
                azimuthPtg = pr.azimuthPtgA;
                elevationPtg = pr.elevationPtgA;
            }
        } else if (pr.type == PassiveRepeaterCAClass::billboardReflectorPRType) {
            idxList.push_back(2*prIdx);
            prPosition = pr.reflectorPosition;
            Vector3 pVec = (position - prPosition).normalized();
            pointingVec = 2*pVec.dot(pr.reflectorPointingVec)*pr.reflectorPointingVec - pVec;

            Vector3 upVec = prPosition.normalized();
            Vector3 zVec = Vector3(0.0, 0.0, 1.0);
            Vector3 eastVec = zVec.cross(upVec).normalized();
            Vector3 northVec = upVec.cross(eastVec);

            elevationPtg = asin(pointingVec.dot(upVec))*180.0/M_PI;
            azimuthPtg = atan2(pointingVec.dot(eastVec), pointingVec.dot(northVec))*180.0/M_PI;
        } else {
            CORE_DUMP;
        }

        position = prPosition;
    }
}
/******************************************************************************************/

/******************************************************************************************/
/**** testAntennaModelMap                                                              ****/
/******************************************************************************************/
void testAntennaModelMap(AntennaModelMapClass &antennaModelMap, std::string inputFile, std::string outputFile)
{
    char *chptr;
    std::ostringstream errStr;
    FILE *fin, *fout;

    if ( !(fin = fopen(inputFile.c_str(), "rb")) ) {
        errStr << std::string("ERROR: Unable to open inputFile: \"") << inputFile << "\"" << std::endl;
        throw std::runtime_error(errStr.str());
    }

    if ( !(fout = fopen(outputFile.c_str(), "wb")) ) {
        errStr << std::string("ERROR: Unable to open outputFile: \"") << outputFile << "\"" << std::endl;
        throw std::runtime_error(errStr.str());
    }

    int linenum, fIdx;
    std::string line, strval;

    int antennaModelFieldIdx = -1;

    std::vector<int *> fieldIdxList;                       std::vector<std::string> fieldLabelList;
    fieldIdxList.push_back(&antennaModelFieldIdx);         fieldLabelList.push_back("antennaModel");

    int fieldIdx;

    enum LineTypeEnum {
         labelLineType,
          dataLineType,
        ignoreLineType,
       unknownLineType
    };

    LineTypeEnum lineType;

    linenum = 0;
    bool foundLabelLine = false;
    while (fgetline(fin, line, false)) {
        linenum++;
        std::vector<std::string> fieldList = splitCSV(line);
        std::string fixedStr = "";

        lineType = unknownLineType;
        /**************************************************************************/
        /**** Determine line type                                              ****/
        /**************************************************************************/
        if (fieldList.size() == 0) {
            lineType = ignoreLineType;
        } else {
            fIdx = fieldList[0].find_first_not_of(' ');
            if (fIdx == (int) std::string::npos) {
                if (fieldList.size() == 1) {
                    lineType = ignoreLineType;
                }
            } else {
                if (fieldList[0].at(fIdx) == '#') {
                    lineType = ignoreLineType;
                }
            }
        }

        if ((lineType == unknownLineType)&&(!foundLabelLine)) {
            lineType = labelLineType;
            foundLabelLine = 1;
        }
        if ((lineType == unknownLineType)&&(foundLabelLine)) {
            lineType = dataLineType;
        }
        /**************************************************************************/

        /**************************************************************************/
        /**** Process Line                                                     ****/
        /**************************************************************************/
        bool found;
        std::string field;
        int xIdx, yIdx;
        switch(lineType) {
            case   labelLineType:
                for(fieldIdx=0; fieldIdx<(int) fieldList.size(); fieldIdx++) {
                    field = fieldList.at(fieldIdx);

                    // std::cout << "FIELD: \"" << field << "\"" << std::endl;

                    found = false;
                    for(fIdx=0; (fIdx < (int) fieldLabelList.size())&&(!found); fIdx++) {
                        if (field == fieldLabelList.at(fIdx)) {
                            *fieldIdxList.at(fIdx) = fieldIdx;
                            found = true;
                        }
                    }
                }

                for(fIdx=0; fIdx < (int) fieldIdxList.size(); fIdx++) {
                    if (*fieldIdxList.at(fIdx) == -1) {
                        errStr << "ERROR: Invalid input file \"" << inputFile << "\" label line missing \"" << fieldLabelList.at(fIdx) << "\"" << std::endl;
                        throw std::runtime_error(errStr.str());
                    }
                }

                fprintf(fout, "%s,matchedAntennaModel\n", line.c_str());

                break;
            case    dataLineType:
                {
                    strval = fieldList.at(antennaModelFieldIdx);

                    AntennaModelClass *antModel = antennaModelMap.find(strval);

                    std::string matchedModelName;
                    if (antModel) {
                        matchedModelName = antModel->name;
                    } else {
                        matchedModelName = "";
                    }

                    fprintf(fout, "%s,%s\n", line.c_str(), matchedModelName.c_str());

                }
                break;
            case  ignoreLineType:
            case unknownLineType:
                // do nothing
                break;
            default:
                CORE_DUMP;
                break;
        }
    }

    if (fin) { fclose(fin); }
    if (fout) { fclose(fout); }
}
/******************************************************************************************/

