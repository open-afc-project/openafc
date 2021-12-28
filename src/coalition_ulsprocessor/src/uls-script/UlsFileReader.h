#ifndef ULS_FILE_READER_H
#define ULS_FILE_READER_H

#include <stdio.h>

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

#include <QList>
#include <QHash>
#include <QString>

class UlsFileReader {
public:
  UlsFileReader(const char *filePath);

  const QList<UlsPath> &paths() { return allPaths; }
  const QList<UlsEmission> &emissions() { return allEmissions; }
  const QList<UlsAntenna> &antennas() { return allAntennas; }
  const QList<UlsFrequency> &frequencies() { return allFrequencies; }
  const QList<UlsLocation> &locations() { return allLocations; }
  const QList<UlsHeader> &headers() { return allHeaders; }
  const QList<UlsMarketFrequency> &marketFrequencies() {
    return allMarketFrequencies;
  }
  const QList<UlsEntity> &entities() { return allEntities; }
  const QList<UlsControlPoint> &controlPoints() { return allControlPoints; }
  const QList<UlsSegment> &segments() { return allSegments; }

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

private:
  void readIndividualPath(FILE *from);
  void readIndividualAntenna(FILE *from);
  void readIndividualFrequency(FILE *from);
  void readIndividualLocation(FILE *from);
  void readIndividualEmission(FILE *from);
  void readIndividualEntity(FILE *from);
  void readIndividualHeader(FILE *from);
  void readIndividualMarketFrequency(FILE *from);
  void readIndividualControlPoint(FILE *from);
  void readIndividualSegment(FILE *from);

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

  QHash<QString, QList<UlsEmission>> emissionMap;
  QHash<QString, QList<UlsAntenna>> antennaMap;
  QHash<QString, QList<UlsSegment>> segmentMap;
  QHash<QString, QList<UlsLocation>> locationMap;
  QHash<QString, QList<UlsPath>> pathMap;
  QHash<QString, QList<UlsEntity>> entityMap;
  QHash<QString, QList<UlsControlPoint>> controlPointMap;
  QHash<QString, QList<UlsHeader>> headerMap;
};

#endif
