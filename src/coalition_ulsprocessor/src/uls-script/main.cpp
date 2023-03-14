#include <QDebug>
#include <QStringList>
#include <limits>
#include <math.h>
#include <iostream>

#include "CsvWriter.h"
#include "UlsFileReader.h"
#include "AntennaModelMap.h"
#include "TransmitterModelMap.h"
#include "FreqAssignment.h"
#include "UlsFunctions.h"

#define VERSION "1.3.0"

bool removeMobile = false;
bool includeUnii8 = false;
bool debugFlag    = false;
bool combineAntennaRegionFlag = false;

void testAntennaModelMap(AntennaModelMapClass &antennaModelMap, std::string inputFile, std::string outputFile);
void testTransmitterModelMap(TransmitterModelMapClass &transmitterModelMap, std::string inputFile, std::string outputFile);
void writeRAS(UlsFileReader &r, std::string filename);
void processUS(UlsFileReader &r, int maxNumPassiveRepeater, CsvWriter &wt, CsvWriter &anomalous, FILE *fwarn,
    AntennaModelMapClass &antennaModelMap, FreqAssignmentClass &freqAssignment, TransmitterModelMapClass &transmitterModelMap);
void processCA(UlsFileReader &r, int maxNumPassiveRepeater, CsvWriter &wt, CsvWriter &anomalous, FILE *fwarn,
    AntennaModelMapClass &antennaModelMap);
void makeLink(const StationDataCAClass &station, const QList<PassiveRepeaterCAClass> &prList, std::vector<int> &idxList, double &azimuthPtg, double &elevationPtg);

/******************************************************************************************/
/* This flag will adjust longitude/latitude values to be consistent with what             */
/* Federated/QCOM are doing.  Values will be adjusted as if printed with                  */
/* numDigit digits, then read back in.                                                    */
/******************************************************************************************/
bool alignFederatedFlag = true;
double alignFederatedScale = 1.0E8; // 10 ^ numDigit
/******************************************************************************************/

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
    if (argc != 10) {
        fprintf(stderr, "Syntax: %s [ULS file.csv] [Output FS File.csv] [Output RAS File.csv] [AntModelListFile.csv] [AntPrefixFile.csv] [AntModelMapFile.csv] [freqAssignmentFile] [transmitterModelListFile] [mode]\n", argv[0]);
        return -1;
    }

    char *tstr;

    time_t t1 = time(NULL);
    tstr = strdup(ctime(&t1));
    strtok(tstr, "\n");
    std::cout << tstr << " : Begin processing." << std::endl;
    free(tstr);

    std::string inputFile = argv[1];
    std::string outputFSFile = argv[2];
    std::string outputRASFile = argv[3];
    std::string antModelListFile = argv[4];
    std::string antPrefixFile = argv[5];
    std::string antModelMapFile = argv[6];
    std::string freqAssignmentFile = argv[7];
    std::string transmitterModelListFile = argv[8];
    std::string mode = argv[9];

    FILE *fwarn;
    std::string warningFile = "warning_uls.txt";
    if ( !(fwarn = fopen(warningFile.c_str(), "wb")) ) {
        std::cout << std::string("WARNING: Unable to open warningFile \"") + warningFile + std::string("\"\n");
    }

	AntennaModelMapClass antennaModelMap(antModelListFile, antPrefixFile, antModelMapFile);

	TransmitterModelMapClass transmitterModelMap(transmitterModelListFile);

	FreqAssignmentClass fccFreqAssignment(freqAssignmentFile);

    if (mode == "test_antenna_model_map") {
        testAntennaModelMap(antennaModelMap, inputFile, outputFSFile);
        return 0;
    } else if (mode == "test_transmitter_model_map") {
        testTransmitterModelMap(transmitterModelMap, inputFile, outputFSFile);
        return 0;
    } else if (mode == "proc_uls") {
        // Do nothing
    } else if (mode == "proc_uls_debug") {
        debugFlag = true;
    } else if (mode == "proc_uls_ca") {
        combineAntennaRegionFlag = true;
    } else if (mode == "proc_uls_include_unii8") {
        includeUnii8 = true;
    } else if (mode == "proc_uls_include_unii8_ca") {
        includeUnii8 = true;
        combineAntennaRegionFlag = true;
    } else if (mode == "proc_uls_include_unii8_debug") {
        includeUnii8 = true;
        debugFlag = true;
    } else {
        fprintf(stderr, "ERROR: Invalid mode: %s\n", mode.c_str());
        return -1;
    }

    UlsFileReader r(inputFile.c_str(), fwarn, alignFederatedFlag, alignFederatedScale);

    int maxNumPRUS = r.computeStatisticsUS(fccFreqAssignment, includeUnii8);
    int maxNumPRCA = r.computeStatisticsCA(fwarn);
    int maxNumPassiveRepeater = (maxNumPRUS > maxNumPRCA ? maxNumPRUS : maxNumPRCA);

    std::cout << "US Max Num Passive Repeater: " << maxNumPRUS << std::endl;
    std::cout << "CA Max Num Passive Repeater: " << maxNumPRCA << std::endl;
    std::cout << "Max Num Passive Repeater: "    << maxNumPassiveRepeater << std::endl;

    CsvWriter wt(outputFSFile.c_str());
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

    writeRAS(r, outputRASFile);

    processUS(r, maxNumPassiveRepeater, wt, anomalous, fwarn, antennaModelMap, fccFreqAssignment, transmitterModelMap);

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
/**** writeRAS                                                                         ****/
/******************************************************************************************/
void writeRAS(UlsFileReader &r, std::string filename)
{
    int rasid = 0;

    CsvWriter fpRAS(filename.c_str());
    {
        QStringList header = UlsFunctionsClass::getRASHeader();
        fpRAS.writeRow(header);
    }


    foreach (const RASClass &ras, r.RASList) {
        rasid++;
        QStringList row;
        row << UlsFunctionsClass::makeNumber(rasid);
        row << QString::fromStdString(ras.region);
        row << QString::fromStdString(ras.name);
        row << QString::fromStdString(ras.location);
        row << UlsFunctionsClass::makeNumber(ras.startFreqMHz);
        row << UlsFunctionsClass::makeNumber(ras.stopFreqMHz);
        row << QString::fromStdString(ras.exclusionZone);
        row << UlsFunctionsClass::makeNumber(ras.rect1lat1);
        row << UlsFunctionsClass::makeNumber(ras.rect1lat2);
        row << UlsFunctionsClass::makeNumber(ras.rect1lon1);
        row << UlsFunctionsClass::makeNumber(ras.rect1lon2);
        row << UlsFunctionsClass::makeNumber(ras.rect2lat1);
        row << UlsFunctionsClass::makeNumber(ras.rect2lat2);
        row << UlsFunctionsClass::makeNumber(ras.rect2lon1);
        row << UlsFunctionsClass::makeNumber(ras.rect2lon2);
        row << UlsFunctionsClass::makeNumber(ras.radiusKm);
        row << UlsFunctionsClass::makeNumber(ras.centerLat);
        row << UlsFunctionsClass::makeNumber(ras.centerLon);
        row << UlsFunctionsClass::makeNumber(ras.heightAGL);

        fpRAS.writeRow(row);
    }
}
/******************************************************************************************/

/******************************************************************************************/
/**** processUS                                                                        ****/
/******************************************************************************************/
void processUS(UlsFileReader &r, int maxNumPassiveRepeater, CsvWriter &wt, CsvWriter &anomalous, FILE *fwarn,
    AntennaModelMapClass &antennaModelMap, FreqAssignmentClass &freqAssignment, TransmitterModelMapClass &transmitterModelMap)
{
    int prIdx;
    std::string antPfx = (combineAntennaRegionFlag ? "" : "US:");

    qDebug() << "--- Beginning path processing";

    const std::vector<double> bwMHzListUnii5 = {
        0.4, 0.8, 1.25, 2.5, 3.75, 5.0, 10.0, 30.0, 60.0
    };

    const std::vector<double> bwMHzListUnii7 = {
        0.4, 0.8, 1.25, 2.5, 3.75, 5.0, 10.0, 30.0
    };

    int cnt = 0;
    int numRecs = 0;

    int numAntMatch = 0;
    int numAntUnmatch = 0;
    int numMissingRxAntHeight = 0;
    int numMissingTxAntHeight = 0;
    int numFreqAssignedMissing = 0;
    int numUnableGetBandwidth = 0;
    int numFreqUpperBandMissing = 0;
    int numFreqUpperBandPresent = 0;
    int numFreqInconsistent = 0;
    int numAssignedStart  = 0; // Number of times frequencyAssigned, frequencyUpperBand consistent with frequencyAssigned being startFreq
    int numAssignedCenter = 0; // Number of times frequencyAssigned, frequencyUpperBand consistent with frequencyAssigned being centerFreq
    int numAssignedOther  = 0; // Number of times frequencyAssigned, frequencyUpperBand not consistent with frequencyAssigned being startFreq or centerFreq

    if (debugFlag) {
        std::cout << "Item,Callsign,frequencyAssigned,frequencyUpperBand" <<  std::endl;
    }

    foreach (const UlsFrequency &freq, r.frequencies()) {
        // qDebug() << "processing frequency " << cnt << "/" << r.frequencies().count()
                //  << " callsign " << freq.callsign;
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
            bool txEmFound = false;
            QList<UlsEmission> allTxEm;
            foreach (const UlsEmission &e, r.emissionsMap(path.callsign)) {
                if (strcmp(e.callsign, path.callsign) == 0) {
                    if (e.locationId == txLoc.locationNumber &&
                        e.antennaId == txAnt.antennaNumber &&
                        e.frequencyId == txFreq.frequencyNumber) {
                        allTxEm << e;
                        txEmFound = true;
                    }
                }
            }
            if (!txEmFound) {
                UlsEmission txEm;
                allTxEm << txEm; // Make sure at least one emission.
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
                double startFreq  = std::numeric_limits<double>::quiet_NaN();
                double stopFreq = std::numeric_limits<double>::quiet_NaN();
                double startFreqBand  = std::numeric_limits<double>::quiet_NaN();
                double stopFreqBand = std::numeric_limits<double>::quiet_NaN();
                double bwMHz = std::numeric_limits<double>::quiet_NaN();
                bool freqInconsistentFlag = false;
                bool freqUpperBandMissingFlag = false;

                if (isnan(txFreq.frequencyAssigned)) {
                    numFreqAssignedMissing++;
                    anomalousReason.append("FrequencyAssigned value missing");
                } else {
                    if (txEmFound) {
                        bwMHz = UlsFunctionsClass::emissionDesignatorToBandwidth(e.desig);
                    }
                    if ( isnan(bwMHz) || (bwMHz > 60.0) || (bwMHz == 0) ) {
                        bwMHz = freqAssignment.getBandwidth(txFreq.frequencyAssigned);
                    } else {
                        bool unii5Flag = (txFreq.frequencyAssigned >= UlsFunctionsClass::unii5StartFreqMHz)
                                      && (txFreq.frequencyAssigned <= UlsFunctionsClass::unii5StopFreqMHz);
                        bool unii7Flag = (txFreq.frequencyAssigned >= UlsFunctionsClass::unii5StartFreqMHz)
                                      && (txFreq.frequencyAssigned <= UlsFunctionsClass::unii5StopFreqMHz);
                        const std::vector<double> *fccBWList = (std::vector<double> *) NULL;
                        if (unii5Flag) {
                            fccBWList = &bwMHzListUnii5;
                        } else if (unii7Flag) {
                            fccBWList = &bwMHzListUnii7;
                        }
                        if (fccBWList) {
                            bool found = false;
                            double fccBW;
                            for(int i=0; (i<fccBWList->size()) &&(!found); ++i) {
                                if (fccBWList->at(i) >= bwMHz) {
                                    found = true;
                                    fccBW = fccBWList->at(i);
                                }
                            }
                            if (found) {
                                bwMHz = std::min(fccBW, bwMHz*1.1);
                            }
                        }
                    }


                    if (bwMHz == -1.0) {
                        numUnableGetBandwidth++;
                        anomalousReason.append("Unable to get bandwidth");
                    } else if (isnan(txFreq.frequencyUpperBand)) {
                        freqUpperBandMissingFlag = true;
                        startFreq  = txFreq.frequencyAssigned - bwMHz / 2.0; // Lower Band (MHz)
                        stopFreq = txFreq.frequencyAssigned + bwMHz / 2.0; // Upper Band (MHz)
                        startFreqBand = startFreq;
                        stopFreqBand = stopFreq;
                    } else {
                        // frequencyAssigned is taken to be center frequency and frequencyUpperBand is ignored as per TS 1014
                        startFreq  = txFreq.frequencyAssigned - bwMHz / 2.0; // Lower Band (MHz)
                        stopFreq = txFreq.frequencyAssigned + bwMHz / 2.0; // Upper Band (MHz)

                        // For the purpuse of determining which UNII band the link is in, frequencyAssigned is the start freq
                        startFreqBand = txFreq.frequencyAssigned;  // Lower Band (MHz)
                        stopFreqBand = startFreqBand + bwMHz; // Upper Band (MHz)

                        if (fabs(txFreq.frequencyUpperBand - txFreq.frequencyAssigned - bwMHz) > 1.0e-6) {
                            freqInconsistentFlag = true;
                            std::stringstream stream;
                            stream << "frequencyUpperBand = " << txFreq.frequencyUpperBand <<
                                      " inconsistent with frequencyAssigned and bandwidth ";
                            fixedReason.append(QString::fromStdString(stream.str()));
                            stream.str(std::string());
                        }

                        if (fabs(txFreq.frequencyUpperBand - txFreq.frequencyAssigned - bwMHz) < 1.0e-6) {
                            numAssignedStart++;
                        } else if (fabs(txFreq.frequencyUpperBand - txFreq.frequencyAssigned - bwMHz/2) < 1.0e-6) {
                            numAssignedCenter++;
                        } else {
                            numAssignedOther++;
                        }
                    }
                }

                AntennaModel::CategoryEnum category;
                AntennaModelClass *rxAntModel = antennaModelMap.find(antPfx, rxAnt.antennaModel, category);

                AntennaModel::CategoryEnum rxAntennaCategory;
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
                    rxAntennaCategory = category;
                    rxAntennaDiameter = -1.0;
                    rxDiversityDiameter = -1.0;
                    rxAntennaMidbandGain = std::numeric_limits<double>::quiet_NaN();
                    fixedReason.append("Rx Antenna Model Unmatched");
                }

                AntennaModelClass *txAntModel = antennaModelMap.find(antPfx, txAnt.antennaModel, category);

                AntennaModel::CategoryEnum txAntennaCategory;
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
                    txAntennaCategory = category;
                    txAntennaDiameter = -1.0;
                    txAntennaMidbandGain = std::numeric_limits<double>::quiet_NaN();
                    fixedReason.append("Tx Antenna Model Unmatched");
                }

                if(isnan(startFreq) || isnan(stopFreq)) {
                    anomalousReason.append("NaN frequency value, ");
                } else {
                    bool overlapUnii5 = (stopFreqBand > UlsFunctionsClass::unii5StartFreqMHz) && (startFreqBand < UlsFunctionsClass::unii5StopFreqMHz);
                    bool overlapUnii7 = (stopFreqBand > UlsFunctionsClass::unii7StartFreqMHz) && (startFreqBand < UlsFunctionsClass::unii7StopFreqMHz);
                    bool overlapUnii8 = (stopFreqBand > UlsFunctionsClass::unii8StartFreqMHz) && (startFreqBand < UlsFunctionsClass::unii8StopFreqMHz);

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

                if(anomalousReason.length() == 0) {
                    if (freqInconsistentFlag) {
                        numFreqInconsistent++;
                    }
                    if (freqUpperBandMissingFlag) {
                        numFreqUpperBandMissing++;
                    } else {
                        numFreqUpperBandPresent++;
                        if (debugFlag) {
                            std::cout << "Inband link with Frequency Upper Band specified," <<  path.callsign
                                      << "," << txFreq.frequencyAssigned
                                      << "," << txFreq.frequencyUpperBand
                                      << std::endl;
                        }
                    }
                }

                TransmitterModelClass *matchedTransmitterModel = transmitterModelMap.find(std::string(txFreq.transmitterModel));

                std::string matchedTransmitterStr;
                std::string transmitterArchitectureStr;
                if (matchedTransmitterModel) {
                    matchedTransmitterStr = matchedTransmitterModel->name;
                    transmitterArchitectureStr = TransmitterModelClass::architectureStr(matchedTransmitterModel->architecture);
                } else {
                    matchedTransmitterStr = std::string("");
                    transmitterArchitectureStr = "UNKNOWN";
                }

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

                row << UlsFunctionsClass::makeNumber((startFreq + stopFreq)/2); // Center Frequency (MHz)
                row << UlsFunctionsClass::makeNumber(bwMHz); // Bandiwdth (MHz)
                row << UlsFunctionsClass::makeNumber(startFreq); // Lower Band (MHz)
                row << UlsFunctionsClass::makeNumber(stopFreq); // Upper Band (MHz)

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
                row << txFreq.transmitterModel; // Tx Model ULS
                row << QString::fromStdString(matchedTransmitterStr);   // Tx Model Matched
                row << QString::fromStdString(transmitterArchitectureStr);   // Tx Architecture
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
                row << QString::fromStdString(txAnt.antennaMake);            // Tx Ant Manufacturer
                row << QString::fromStdString(txAnt.antennaModel);           // Tx Ant Model
                row << txAntennaModelName.c_str();            // Tx Matched antenna model (blank if unmatched)
                row << AntennaModel::categoryStr(txAntennaCategory).c_str(); // Tx Antenna category
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
                row << QString::fromStdString(rxAnt.antennaMake);            // Rx Ant Manufacturer
                row << QString::fromStdString(rxAnt.antennaModel);           // Rx Ant Model
                row << rxAntennaModelName.c_str();            // Rx Matched antenna model (blank if unmatched)
                row << AntennaModel::categoryStr(rxAntennaCategory).c_str(); // Rx Antenna category
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

			            AntennaModelClass *prAntModel = antennaModelMap.find(antPfx, prAnt.antennaModel, category);

                        AntennaModel::TypeEnum prAntennaType;
                        AntennaModel::CategoryEnum prAntennaCategory;
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
                            prAntennaType = AntennaModel::UnknownType;
                            prAntennaCategory = category;
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
                        row << QString::fromStdString(prAnt.antennaMake);    // Passive Repeater Ant Make
                        row << QString::fromStdString(prAnt.antennaModel);   // Passive Repeater Ant Model
                        row << prAntennaModelName.c_str();        // Passive Repeater antenna model (blank if unmatched)
                        row << AntennaModel::typeStr(prAntennaType).c_str(); // Passive Repeater Ant Type
                        row << AntennaModel::categoryStr(prAntennaCategory).c_str(); // Passive Repeater Ant Category
                        row << UlsFunctionsClass::makeNumber(prAnt.backtobackTxGain); // Passive Repeater Back-To-Back Tx Gain
                        row << UlsFunctionsClass::makeNumber(prAnt.backtobackRxGain); // Passive Repeater Back-To-Back Rx Gain
                        row << UlsFunctionsClass::makeNumber(prAnt.reflectorHeight); // Passive Repeater ULS Reflector Height
                        row << UlsFunctionsClass::makeNumber(prAnt.reflectorWidth);  // Passive Repeater ULS Reflector Width
                        row << UlsFunctionsClass::makeNumber(prAntennaDiameter);     // Passive Repeater Ant Model Diameter (m)
                        row << UlsFunctionsClass::makeNumber(prAntennaMidbandGain);  // Passive Repeater Ant Model Midband Gain (dB)
                        row << UlsFunctionsClass::makeNumber(prAntennaReflectorHeight); // Passive Repeater Ant Model Reflector Height
                        row << UlsFunctionsClass::makeNumber(prAntennaReflectorWidth);  // Passive Repeater Ant Model Reflector Width
                        row << UlsFunctionsClass::makeNumber(prAnt.lineLoss);        // Passive Repeater Line Loss
                        row << UlsFunctionsClass::makeNumber(prAnt.heightToCenterRAAT); // Passive Repeater Height to Center RAAT Tx
                        row << UlsFunctionsClass::makeNumber(prAnt.heightToCenterRAAT); // Passive Repeater Height to Center RAAT Rx
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

    std::cout << "US Num Frequency Assigned Missing: "   << numFreqAssignedMissing  << std::endl;
    std::cout << "US Num Unable To Get Bandwidth: "      << numUnableGetBandwidth   << std::endl;
    std::cout << "US Num Frequency Upper Band Missing: " << numFreqUpperBandMissing << std::endl;
    std::cout << "US Num Frequency Upper Band Present: " << numFreqUpperBandPresent << std::endl;
    std::cout << "US Num Frequency Inconsistent: "       << numFreqInconsistent     << std::endl;
    std::cout << "US Num Frequency Assigned = Start: "   << numAssignedStart        << std::endl;
    std::cout << "US Num Frequency Assigned = Center: "  << numAssignedCenter       << std::endl;
    std::cout << "US Num Frequency Assigned = Other: "   << numAssignedOther        << std::endl;

    std::cout<<"US Processed " << r.frequencies().count()
             << " frequency records and output to file; a total of " << numRecs
             << " output"<<'\n';

}
/******************************************************************************************/

/******************************************************************************************/
/**** processCA                                                                        ****/
/******************************************************************************************/
void processCA(UlsFileReader &r, int maxNumPassiveRepeater, CsvWriter &wt, CsvWriter &anomalous, FILE * /* fwarn */,
    AntennaModelMapClass &antennaModelMap)
{
    int prIdx;
    std::string antPfx = (combineAntennaRegionFlag ? "" : "CA:");

    qDebug() << "--- Beginning path processing";

    int numRecs = 0;

    int numAntMatch = 0;
    int numAntUnmatch = 0;

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

            double startFreq  = station.centerFreqMHz - station.bandwidthMHz/2; // Lower Band (MHz)
            double stopFreq = station.centerFreqMHz + station.bandwidthMHz/2; // Upper Band (MHz)

            if(isnan(startFreq) || isnan(stopFreq)) {
                anomalousReason.append("NaN frequency value, ");
            } else {
                bool overlapUnii5 = (stopFreq > UlsFunctionsClass::unii5StartFreqMHz) && (startFreq < UlsFunctionsClass::unii5StopFreqMHz);
                bool overlapUnii6 = (stopFreq > UlsFunctionsClass::unii6StartFreqMHz) && (startFreq < UlsFunctionsClass::unii6StopFreqMHz);
                bool overlapUnii7 = (stopFreq > UlsFunctionsClass::unii7StartFreqMHz) && (startFreq < UlsFunctionsClass::unii7StopFreqMHz);
                bool overlapUnii8 = (stopFreq > UlsFunctionsClass::unii8StartFreqMHz) && (startFreq < UlsFunctionsClass::unii8StopFreqMHz);

                if (!(overlapUnii5 || overlapUnii7 || overlapUnii6 || (includeUnii8 && overlapUnii8))) {
                    anomalousReason.append("Out of band, ");
                } else if (overlapUnii5 && overlapUnii7) {
                    anomalousReason.append("Band overlaps both Unii5 and Unii7, ");
                }
            }

            double azimuthPtg, elevationPtg;

            makeLink(station, prList, idxList, azimuthPtg, elevationPtg);

            AntennaModel::CategoryEnum category;
            AntennaModelClass *rxAntModel = antennaModelMap.find(antPfx, station.antennaModel, category);

            AntennaModel::CategoryEnum rxAntennaCategory;
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
                rxAntennaCategory = category;
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
            row << "";                          // Tx Model ULS
            row << "";                          // Tx Model Matched
            row << "UNKNOWN";                   // Tx Architecture
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
            row << AntennaModel::categoryStr(rxAntennaCategory).c_str(); // Rx Antenna category
            row << UlsFunctionsClass::makeNumber(rxAntennaDiameter);     // Rx Ant Diameter (m)
            row << UlsFunctionsClass::makeNumber(rxAntennaMidbandGain);  // Rx Ant Midband Gain (dB)
            row << UlsFunctionsClass::makeNumber(station.lineLoss);      // Rx Line Loss (dB)
            row << UlsFunctionsClass::makeNumber(station.antennaHeightAGL);  // Rx Height to Center RAAT (m)
            row << UlsFunctionsClass::makeNumber(station.antennaGain);   // Rx Gain (dBi)
            row << "";                                                   // Rx Diveristy Height (m)
            row << UlsFunctionsClass::makeNumber(rxDiversityDiameter);   // Rx Diversity Diameter (m)
            row << "";                                                   // Rx Diversity Gain (dBi)

            row << QString::number(numPR);                               // Num Passive Repeater
            for(int prCount=1; prCount<=maxNumPassiveRepeater; ++prCount) {

                if (prCount <= numPR) {
                    int idxVal = idxList[numPR - prCount];

                    bool repFlag = (idxVal & 0x01 ? false : true);

                    prIdx = idxVal / 2;

                    const PassiveRepeaterCAClass &pr = prList[prIdx];

                    AntennaModelClass *prAntModel = antennaModelMap.find(antPfx, pr.antennaModelA, category);

                    PassiveRepeaterCAClass::PRTypeEnum prAntennaType;
                    AntennaModel::CategoryEnum prAntennaCategory;
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
                        prAntennaCategory = category;
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
                    row << AntennaModel::categoryStr(prAntennaCategory).c_str(); // Passive Repeater Ant Category
                    row << UlsFunctionsClass::makeNumber(repFlag ? pr.antennaGainA : pr.antennaGainB); // Passive Repeater Back-To-Back Tx Gain
                    row << UlsFunctionsClass::makeNumber(repFlag ? pr.antennaGainB : pr.antennaGainA); // Passive Repeater Back-To-Back Rx Gain
                    row << UlsFunctionsClass::makeNumber(pr.reflectorHeight); // Passive Repeater ULS Reflector Height
                    row << UlsFunctionsClass::makeNumber(pr.reflectorWidth);  // Passive Repeater ULS Reflector Width
                    row << UlsFunctionsClass::makeNumber(prAntennaDiameter);     // Passive Repeater Ant Model Diameter (m)
                    row << UlsFunctionsClass::makeNumber(prAntennaMidbandGain);  // Passive Repeater Ant Model Midband Gain (dB)
                    row << UlsFunctionsClass::makeNumber(prAntennaReflectorHeight); // Passive Repeater Ant Model Reflector Height
                    row << UlsFunctionsClass::makeNumber(prAntennaReflectorWidth);  // Passive Repeater Ant Model Reflector Width
                    row << "";                                                   // Passive Repeater Line Loss
                    row << UlsFunctionsClass::makeNumber(repFlag ? pr.heightAGLA : pr.heightAGLB); // Passive Repeater Height to Center RAAT Tx
                    row << UlsFunctionsClass::makeNumber(repFlag ? pr.heightAGLB : pr.heightAGLA); // Passive Repeater Height to Center RAAT Rx
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
        for(int uIdx=0; uIdx<(int) unassignedIdxList.size(); ++uIdx) {
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
        if (selUIdx < (int) unassignedIdxList.size()-1) {
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
    std::ostringstream errStr;
    FILE *fin, *fout;

    std::string antPfx = (combineAntennaRegionFlag ? "" : "US:");

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

                    AntennaModel::CategoryEnum category;
                    AntennaModelClass *antModel = antennaModelMap.find(antPfx, strval, category);

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

/******************************************************************************************/
/**** testTransmitterModelMap                                                          ****/
/******************************************************************************************/
void testTransmitterModelMap(TransmitterModelMapClass &transmitterModelMap, std::string inputFile, std::string outputFile)
{
    std::ostringstream errStr;
    FILE *fin, *fout;

    transmitterModelMap.checkPrefixValues();

    int numTX = 0;
    int numMatch = 0;

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

    int transmitterModelFieldIdx = -1;

    std::vector<int *> fieldIdxList;                       std::vector<std::string> fieldLabelList;
    fieldIdxList.push_back(&transmitterModelFieldIdx);     fieldLabelList.push_back("transmitterModel");

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

                fprintf(fout, "%s,matchedTransmitterModel\n", line.c_str());

                break;
            case    dataLineType:
                {
                    numTX++;
                    strval = fieldList.at(transmitterModelFieldIdx);

                    TransmitterModelClass *transmitterModel = transmitterModelMap.find(strval);

                    std::string matchedModelName;
                    if (transmitterModel) {
                        matchedModelName = transmitterModel->name;
                        numMatch++;
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

    std::cout << "NUM TX: " << numTX << std::endl;
    std::cout << "NUM MATCH: " << numMatch << std::endl;
}
/******************************************************************************************/

