#ifndef ULS_MEASUREMENT_ANALYSIS_H
#define ULS_MEASUREMENT_ANALYSIS_H

#include <QString>
// #include <iturp452/ITURP452.h>
#include <ogr_spatialref.h>
#include <QPointF>
#include "terrain.h"


namespace UlsMeasurementAnalysis {
	int analyzeMeasurementSites(QString fullCmd, int subArgc, char **subArgv);
	int analyzeMeasurementBubble(QString fullCmd, int subArgc, char **subArgv);
	int analyzeApproximatePathDifferences(QString fullCmd, int subArgc, char **subArgv);

	double *computeElevationVector(const TerrainClass *terrain, bool includeBldg, const QPointF &from, const QPointF &to, int numpts);

	void computeElevationVector(const TerrainClass *terrain, bool includeBldg, const QPointF &from, const QPointF &to, int numpts, std::vector<double> &hi, std::vector<double> &di, double ae);

	double runPointToPoint(const TerrainClass *terrain, bool includeBldg, QPointF transLocLatLon, double transHt,       QPointF receiveLocLatLon,double receiveHt, double lineOfSightDistanceKm, double eps_dielect, double sgm_conductivity, double eno_ns_surfref, double frq_mhz, int radio_climate, int pol, double conf, double rel, int numpts, char *prefix,double **heightProfilePtr);

	bool isLOS(const TerrainClass *terrain, QPointF transLocLatLon, double transHt, QPointF receiveLocLatLon,
			double receiveHt, double lineOfSightDistanceKm, int numpts, double **heightProfilePtr);

	//    iturp452::ClutterCategory computeClutter(double lon, double lat, bool &waterFlag,
	//        OGRCoordinateTransformation *clutterTrans, const float *pafScanline);

	extern long long numInvalidSRTM;
	extern long long numSRTM;

};

#endif
