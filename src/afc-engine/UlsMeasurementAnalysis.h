#ifndef ULS_MEASUREMENT_ANALYSIS_H
#define ULS_MEASUREMENT_ANALYSIS_H

#include <QString>
#include <ogr_spatialref.h>
#include <QPointF>
#include "terrain.h"

namespace UlsMeasurementAnalysis {
	int analyzeMeasurementSites(QString fullCmd, int subArgc, char **subArgv);
	int analyzeMeasurementBubble(QString fullCmd, int subArgc, char **subArgv);
	int analyzeApproximatePathDifferences(QString fullCmd, int subArgc, char **subArgv);

	double *computeElevationVector(const TerrainClass *terrain, bool includeBldg, bool cdsmFlag, const QPointF &from, const QPointF &to, int numpts, double *cdsmFracPtr);

	void computeElevationVector(const TerrainClass *terrain, bool includeBldg, bool cdsmFlag, const QPointF &from, const QPointF &to, int numpts, std::vector<double> &hi, std::vector<double> &di, double ae);

	double runPointToPoint(const TerrainClass *terrain, bool includeBldg, QPointF transLocLatLon, double transHt,       QPointF receiveLocLatLon,double receiveHt, double lineOfSightDistanceKm, double eps_dielect, double sgm_conductivity, double eno_ns_surfref, double frq_mhz, int radio_climate, int pol, double conf, double rel, int numpts, char *prefix,double **heightProfilePtr);

	bool isLOS(const TerrainClass *terrain, QPointF transLocLatLon, double transHt, QPointF receiveLocLatLon,
			double receiveHt, double lineOfSightDistanceKm, int numpts, double **heightProfilePtr, double *cdsmFracPtr);

	extern long long numInvalidSRTM;
	extern long long numSRTM;

};

#endif
