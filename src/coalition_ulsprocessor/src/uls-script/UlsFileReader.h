#ifndef ULS_FILE_READER_H
#define ULS_FILE_READER_H

#include <stdio.h>
#include <unordered_set>
#include <algorithm>

#include "FreqAssignment.h"

#include "UlsLocation.h"
#include "UlsAntenna.h"
#include "UlsHeader.h"
#include "UlsFrequency.h"
#include "UlsPath.h"
#include "UlsCallsign.h"
#include "UlsHop.h"
#include "UlsEmission.h"
#include "UlsEntity.h"
#include "UlsMarketFrequency.h"
#include "UlsControlPoint.h"
#include "UlsSegment.h"

#include "StationDataCA.h"
#include "PassiveRepeaterCA.h"
#include "TransmitterCA.h"

#include <QList>
#include <QHash>
#include <QString>

class UlsFileReader {
public:
    UlsFileReader(const char *filePath, FILE *fwarn);

    const QList<UlsPath> &paths() { return allPaths; }
    const QList<UlsEmission> &emissions() { return allEmissions; }
    const QList<UlsAntenna> &antennas() { return allAntennas; }
    const QList<UlsFrequency> &frequencies() { return allFrequencies; }
    const QList<UlsLocation> &locations() { return allLocations; }
    const QList<UlsHeader> &headers() { return allHeaders; }
    const QList<UlsMarketFrequency> &marketFrequencies() { return allMarketFrequencies; }
    const QList<UlsEntity> &entities() { return allEntities; }
    const QList<UlsControlPoint> &controlPoints() { return allControlPoints; }
    const QList<UlsSegment> &segments() { return allSegments; }

    const QList<StationDataCAClass> &stations() { return allStations; }
    const QList<BackToBackPassiveRepeaterCAClass> &backToBackPassiveRepeaters() { return allBackToBackPassiveRepeaters; }
    const QList<ReflectorPassiveRepeaterCAClass> &reflectorPassiveRepeaters() { return allReflectorPassiveRepeaters; }
    const QList<TransmitterCAClass> &transmitters() { return allTransmitters; }

    const QList<UlsAntenna> antennasMap(const QString &s) const {
        return antennaMap[s];
    }

    const QList<UlsSegment> segmentsMap(const QString &s) const {
        return segmentMap[s];
    }

    const QList<UlsLocation> locationsMap(const QString &s) const {
        return locationMap[s];
    }

    const QList<UlsEmission> emissionsMap(const QString &s) const {
        return emissionMap[s];
    }

    const QList<UlsPath> pathsMap(const QString &s) const {
        return pathMap[s];
    }

    const QList<UlsEntity> entitiesMap(const QString &s) const {
        return entityMap[s];
    }

    const QList<UlsHeader> headersMap(const QString &s) const {
        return headerMap[s];
    }

    const QList<UlsControlPoint> controlPointsMap(const QString &s) const {
        return controlPointMap[s];
    }

    const QList<StationDataCAClass> stationsMap(const QString &s) const {
        return stationMap[s];
    }

    const QList<BackToBackPassiveRepeaterCAClass> backToBackPassiveRepeatersMap(const QString &s) const {
        return backToBackPassiveRepeaterMap[s];
    }

    const QList<ReflectorPassiveRepeaterCAClass> reflectorPassiveRepeatersMap(const QString &s) const {
        return reflectorPassiveRepeaterMap[s];
    }

    const QList<PassiveRepeaterCAClass> passiveRepeatersMap(const QString &s) const {
        return passiveRepeaterMap[s];
    }

    const QList<TransmitterCAClass> transmittersMap(const QString &s) const {
        return transmitterMap[s];
    }

    std::unordered_set<std::string> authorizationNumberList;

    int computeStatisticsUS(FreqAssignmentClass &freqAssignment, bool includeUnii8);
    int computeStatisticsCA(FILE *fwarn);

private:
    void readIndividualHeaderUS(const std::vector<std::string> &fieldList);
    void readIndividualPathUS(const std::vector<std::string> &fieldList);
    void readIndividualAntennaUS(const std::vector<std::string> &fieldList, FILE *fwarn);
    void readIndividualFrequencyUS(const std::vector<std::string> &fieldList, FILE *fwarn);
    void readIndividualLocationUS(const std::vector<std::string> &fieldList);
    void readIndividualEmissionUS(const std::vector<std::string> &fieldList);
    void readIndividualEntityUS(const std::vector<std::string> &fieldList);
    void readIndividualMarketFrequencyUS(const std::vector<std::string> &fieldList);
    void readIndividualControlPointUS(const std::vector<std::string> &fieldList);
    void readIndividualSegmentUS(const std::vector<std::string> &fieldList);

    void readStationDataCA(const std::vector<std::string> &fieldList, FILE *fwarn);
    void readBackToBackPassiveRepeaterCA(const std::vector<std::string> &fieldList, FILE *fwarn);
    void readReflectorPassiveRepeaterCA(const std::vector<std::string> &fieldList, FILE *fwarn);
    void readTransmitterCA(const std::vector<std::string> &fieldList, FILE *fwarn);

    QList<UlsPath> allPaths;
    QList<UlsEmission> allEmissions;
    QList<UlsAntenna> allAntennas;
    QList<UlsFrequency> allFrequencies;
    QList<UlsLocation> allLocations;
    QList<UlsHeader> allHeaders;
    QList<UlsMarketFrequency> allMarketFrequencies;
    QList<UlsEntity> allEntities;
    QList<UlsControlPoint> allControlPoints;
    QList<UlsSegment> allSegments;

    QList<StationDataCAClass> allStations;
    QList<BackToBackPassiveRepeaterCAClass> allBackToBackPassiveRepeaters;
    QList<ReflectorPassiveRepeaterCAClass> allReflectorPassiveRepeaters;
    QList<TransmitterCAClass> allTransmitters;

    QHash<QString, QList<UlsEmission>> emissionMap;
    QHash<QString, QList<UlsAntenna>> antennaMap;
    QHash<QString, QList<UlsSegment>> segmentMap;
    QHash<QString, QList<UlsLocation>> locationMap;
    QHash<QString, QList<UlsPath>> pathMap;
    QHash<QString, QList<UlsEntity>> entityMap;
    QHash<QString, QList<UlsControlPoint>> controlPointMap;
    QHash<QString, QList<UlsHeader>> headerMap;

    QHash<QString, QList<StationDataCAClass>> stationMap;
    QHash<QString, QList<BackToBackPassiveRepeaterCAClass>> backToBackPassiveRepeaterMap;
    QHash<QString, QList<ReflectorPassiveRepeaterCAClass>> reflectorPassiveRepeaterMap;
    QHash<QString, QList<PassiveRepeaterCAClass>> passiveRepeaterMap;
    QHash<QString, QList<TransmitterCAClass>> transmitterMap;
};

#endif
