#ifndef ULS_MEASUREMENT_ANALYSIS_H
#define ULS_MEASUREMENT_ANALYSIS_H

#include <QString>
#include <iturp452/ITURP452.h>
#include <ogr_spatialref.h>
#include "WorldData.h"
#include <QPointF>
#include "terrain.h"

class GdalDataDir;
class GdalDataModel;
class BuildingRasterModel;

namespace UlsMeasurementAnalysis {
    int analyzeMeasurementSites(QString fullCmd, int subArgc, char **subArgv);
    int analyzeMeasurementBubble(QString fullCmd, int subArgc, char **subArgv);
    int analyzeApproximatePathDifferences(QString fullCmd, int subArgc, char **subArgv);
    
    double runPointToPoint(const WorldData *wd, const GdalDataDir *srtm1, QPointF transLocLatLon, double transHt, QPointF receiveLocLatLon,
                           double receiveHt, double losDistKm, double eps_dielect, double sgm_conductivity, double eno_ns_surfref,
                           double frq_mhz, int radio_climate, int pol, double conf, double rel, int numpts, char *prefix);

    double runPointToPointBldg(const WorldData *wd, const GdalDataDir *srtm1, GdalDataModel *bldg, QPointF transLocLatLon, double transHt, QPointF receiveLocLatLon,
                           double receiveHt, double losDistKm, double eps_dielect, double sgm_conductivity, double eno_ns_surfref,
                           double frq_mhz, int radio_climate, int pol, double conf, double rel, int numpts, char *prefix, double **heightProfilePtr);

    double runPointToPointBldg(const WorldData *wd, const GdalDataDir *srtm1, BuildingRasterModel *bldg, GdalDataModel *bldgPoly, QPointF transLocLatLon, double transHt, QPointF receiveLocLatLon,
                           double receiveHt, double losDistKm, double eps_dielect, double sgm_conductivity, double eno_ns_surfref,
                           double frq_mhz, int radio_climate, int pol, double conf, double rel, int numpts, char *prefix, double **heightProfilePtr
#if MM_DEBUG
                                           , std::vector<std::string> &ITMHeightType
#endif
                              );

    double runPointToPoint452(const WorldData *wd, const GdalDataDir *srtm1, QPointF transLocLatLon, double transHt, QPointF receiveLocLatLon,
                           double receiveHt, double lineOfSightDistanceKm, 
                           double frequencyGHz, iturp452::Polarization pol, double conf, int numpts, const iturp452::ITURP452 *itu,
                           OGRCoordinateTransformation *clutterTrans, const float *pafScanline, double *clutterLossPtr, std::string *txClutterStrPtr);

    // double computeHeight(QPointF loc, const WorldData *globeData, const GdalDataDir *dataDir);
    double *computeElevationVector(const TerrainClass *terrain, bool includeBldg, const QPointF &from, const QPointF &to, int numpts);
    
    void computeElevationVector(const TerrainClass *terrain, bool includeBldg, const QPointF &from, const QPointF &to, int numpts, std::vector<double> &hi, std::vector<double> &di, double ae);

    double runPointToPoint(const TerrainClass *terrain, bool includeBldg, QPointF transLocLatLon, double transHt,       QPointF receiveLocLatLon,double receiveHt, double lineOfSightDistanceKm, double eps_dielect, double sgm_conductivity, double eno_ns_surfref, double frq_mhz, int radio_climate, int pol, double conf, double rel, int numpts, char *prefix,double **heightProfilePtr);


    double computeTerrainHeight(const double& latDeg, const double& lonDeg, const BuildingRasterModel *bldg, const WorldData *globeData, const GdalDataDir *dataDir);

    iturp452::ClutterCategory computeClutter(double lon, double lat, bool &waterFlag,
        OGRCoordinateTransformation *clutterTrans, const float *pafScanline);

    extern long long numInvalidSRTM;
    extern long long numSRTM;

};

#endif
