#include "EcefModel.h"
#include "UlsMeasurementAnalysis.h"
#include "gdal_priv.h"
#include "cpl_conv.h" // for CPLMalloc()
#include <QImage>
#include <QPainter>
#include <QColor>
#include <iomanip>
#include <exception>
#include <tuple>
#include "GdalHelpers.h"

#include "cconst.h"
#include "EcefModel.h"
#include "UlsMeasurementAnalysis.h"
#include "gdal_priv.h"
#include "cpl_conv.h" // for CPLMalloc()

extern void point_to_point(double elev[], double tht_m, double rht_m,
		double eps_dielect, double sgm_conductivity, double eno_ns_surfref,
		double frq_mhz, int radio_climate, int pol, double conf, double rel,
		double &dbloss, std::string &strmode, int &errnum);

namespace {
	// Logger for all instances of class
	LOGGER_DEFINE_GLOBAL(logger, "UlsMeasurementAnalysis")

		static bool itmInitFlag = true;
}; // end namespace

namespace UlsMeasurementAnalysis {
	void dumpHeightProfile(const char *prefix, const double *heights);

	QVector<QPointF> computeApproximateGreatCircleLine(const QPointF &from, const QPointF &to, int numpts, double *tdist){
		QVector<QPointF> latlons;
		latlons.fill(from, numpts);

		double delx = to.x() - from.x();
		double dely = to.y() - from.y();

		// Do a straight linear interpolation, then use straight line distance.
		for(int i = 1; i < numpts; ++i){
			double frac = i / double(numpts - 1);
			latlons[i] += QPointF(delx * frac,
					dely * frac);
		}

		// And distance.
		if(tdist != NULL){
			// Vector3 fvec = EcefModel::geodeticToEcef(from.x(), from.y(), 0);
			// Vector3 tvec = EcefModel::geodeticToEcef(to.x(), to.y(), 0);
			// *tdist = (fvec - tvec).len();

			double lon1Rad = from.y()*M_PI/180.0;
			double lat1Rad = from.x()*M_PI/180.0;
			double lon2Rad = to.y()*M_PI/180.0;
			double lat2Rad = to.x()*M_PI/180.0;
			double slat = sin((lat2Rad-lat1Rad)/2);
			double slon = sin((lon2Rad-lon1Rad)/2);
			*tdist = 2*CConst::averageEarthRadius*asin(sqrt(slat*slat+cos(lat1Rad)*cos(lat2Rad)*slon*slon))*1.0e-3;
		}

		return latlons;
	}

	QVector<QPointF> computeGreatCircleLine(const QPointF &from, const QPointF &to, int numpts, double *tdist){
		//  We're going to do what the fortran program does. It's kinda stupid, though.

		//  Please excuse fortran style variable names. I have decrypted them where possible.
		double earthRad = CConst::averageEarthRadius / 1000.0;

		// double fLonRad, tLonRad;
		double fLatRad;
		double tLatRad;

		fLatRad = from.x() * M_PI / 180.0;
		//fLonRad = from.y() * M_PI / 180.0;

		tLatRad = to.x() * M_PI / 180.0;
		//tLonRad = to.y() * M_PI / 180.0;

		double deltaLat = to.x() - from.x();
		double aDeltaLat = fabs(deltaLat);
		double deltaLon = to.y() - from.y();
		double aDeltaLon = fabs(deltaLon);

		// printf("deltaLon = %.8g, deltaLat = %.8g\n", deltaLon, deltaLat);

		double wLat, eLat;

		if(deltaLon > 0){ // Moving, uh, east?
			wLat = fLatRad;
			eLat = tLatRad;
		}
		else{
			wLat = tLatRad;
			eLat = fLatRad;
		}

		// printf("wLat,eLat = %.8g, %.8g\n", wLat, eLat);

		//  Okay, now compute the azimuths at points w and e.
		double sdLat, sdLon, sadLn;
		sdLat = sin(0.5 * aDeltaLat * M_PI / 180.0);
		sdLon = sin(0.5 * aDeltaLon * M_PI / 180.0);
		sadLn = sin(aDeltaLon * M_PI / 180.0);

		double ceLat, cwLat;
		cwLat = cos(wLat);
		ceLat = cos(eLat);

		// printf("cwLat = %.8g; ceLat = %.8g\n", cwLat, ceLat);

		double P = 2.0 * (sdLat * sdLat + sdLon * sdLon * cwLat * ceLat);

		// printf("P = %.8g\n", P);
		double sgc = sqrt(P * (2.0 - P));

		//  Continue computing azimuth...
		sdLat = sin(eLat - wLat);
		// printf("sdLat = %.8g\n", sdLat);
		double cwaz = (2.0 * ceLat * sin(wLat) * sdLon * sdLon + sdLat) / sgc;
		// printf("cwaz = %.8g\n", cwaz);
		double swaz = (sadLn * ceLat) / sgc;

		double wAzimuth = atan2(swaz, cwaz) * 180 / M_PI;
		double ceaz = (2.0 * cwLat * sin(eLat) * sdLon * sdLon - sdLat) / sgc;
		// printf("ceaz = %.8g\n", ceaz);
		double seaz = (sadLn * cwLat) / sgc;
		// printf("seaz = %.9g\n", seaz);
		double eAzimuth = atan2(seaz, ceaz) * 180 / M_PI;
		eAzimuth = 360 - eAzimuth;

		double targetAz; // receiveAz;

		// printf("wAz = %.8g, eAz = %.8g\n", wAzimuth, eAzimuth);

		if(deltaLon < 0.0){
			targetAz = eAzimuth;
			//receiveAz = wAzimuth;
		}
		else{
			targetAz = wAzimuth;
			//receiveAz = eAzimuth;
		}

		//  And finish the great circle.

		double cgc = 1.0 - P;

		double greatCircleAngle = atan2(sgc, cgc);
		double greatCircleDistance = greatCircleAngle * earthRad;

		if(tdist != NULL) *tdist = greatCircleDistance;

		// printf("Computed that distance is %.8g km; azimuth = %.8g\n", greatCircleDistance, targetAz);

		//  Okay, done with that. Now we need to interpolate along the great circle.
		//  We use some of the values computed above; specifically, targetAz.
		//  We already have tLatRad and tLonRad from above.
		//  We will be interpolating along greatCircleDistance and converting distances along the great circle into lat/lons.
		QVector<QPointF> latlons;
		latlons.fill(QPointF(0, 0), numpts);
		latlons[0] = from;

		double delta = greatCircleDistance / (numpts-1);

		double coLat = M_PI / 2.0 - fLatRad;
		double cosco, sinco;
		sincos(coLat, &sinco, &cosco);

		double cosTargetAz = cos(targetAz * M_PI / 180.0);
		double fromY = from.y();

		QVector<QPointF>::iterator it = latlons.begin();
		++it;

		for(int i = 2; i < numpts; ++i, ++it){
			double tgcDist = (i - 1) * delta;
			// printf("tgcDist = %.8g * %.8g = %.8g km away\n", (i + 1) / (double)numpts, greatCircleDistance, tgcDist);
			double tgc = tgcDist / earthRad;

			//printf("END POINT: %.8g, %.8g, %.8g, %.8g\n", tLatRad, tLonRad, targetAz * M_PI / 180.0, tgc);

			//  Now through cosines, sines, ... to get the distance.
			double cosgc, singc;
			sincos(tgc, &singc, &cosgc);

			// printf("cos/sin co/gc = %.8g, %.8g, %.8g, %.8g\n", cosco, sinco, cosgc, singc);
			double cosb = cosco * cosgc + sinco * singc * cosTargetAz;
			// printf("cosb = %.8g\n", cosb);
			double arg = 1.0 - cosb * cosb;
			if(arg < 0.0) arg = 0.0;
			double B = atan2(sqrt(arg), cosb);
			double arc = (cosgc - cosco * cosb) / (sinco * sin(B));
			arg = 1.0 - arc * arc;
			if(arg < 0.0) arg = 0.0;

			//  And finally compute the end point distances.
			double rdLon = atan2(sqrt(arg), arc);
			double zrLat = ((M_PI / 2.0) - fabs(B)) * 180 / M_PI;
			// printf("zrLat = %.8g, B=%.8g\n", zrLat, B);
			if(cosb < 0.0) zrLat = -zrLat;
			double zrLon;

			if(targetAz > 180.0) zrLon = fromY - fabs(rdLon) * 180 / M_PI;
			else zrLon = fromY + fabs(rdLon) * 180 / M_PI;

			//printf("Results in %.8g, %.8g\n", zrLat, zrLon);

			it->setX(zrLat);
			it->setY(zrLon);
		}

		*it = to;

		return latlons;
	}

	QVector<QPointF> computeGreatCircleLineMM(const QPointF &from, const QPointF &to, int numpts, double *tdist){

		double lon1Rad = from.y()*M_PI/180.0;
		double lat1Rad = from.x()*M_PI/180.0;
		double lon2Rad = to.y()*M_PI/180.0;
		double lat2Rad = to.x()*M_PI/180.0;
		double slat = sin((lat2Rad-lat1Rad)/2);
		double slon = sin((lon2Rad-lon1Rad)/2);
		*tdist = 2*CConst::averageEarthRadius*asin(sqrt(slat*slat+cos(lat1Rad)*cos(lat2Rad)*slon*slon))*1.0e-3;

		Vector3 posn1 = Vector3(cos(lat1Rad)*cos(lon1Rad), cos(lat1Rad)*sin(lon1Rad), sin(lat1Rad));
		Vector3 posn2 = Vector3(cos(lat2Rad)*cos(lon2Rad), cos(lat2Rad)*sin(lon2Rad), sin(lat2Rad));

		double dotprod = posn1.dot(posn2);
		if (dotprod > 1.0) {
			dotprod = 1.0;
		} else if (dotprod < - 1.0) {
			dotprod = -1.0;
		}

		double greatCircleAngle = acos(dotprod);

		Vector3 uVec = (posn1 + posn2).normalized();
		Vector3 wVec = posn1.cross(posn2).normalized();
		Vector3 vVec = wVec.cross(uVec);

		QVector<QPointF> latlons(numpts);

		for(int ptIdx=0; ptIdx<numpts; ++ptIdx) {
			double theta_i = (greatCircleAngle*(2*ptIdx - (numpts-1)))/(2*(numpts-1));
			Vector3 posn_i = uVec*cos(theta_i) + vVec*sin(theta_i);
			double lon_i = atan2(posn_i.y(), posn_i.x());
			double lat_i = atan2(posn_i.z(), posn_i.x()*cos(lon_i)+posn_i.y()*sin(lon_i));

			latlons[ptIdx] = QPointF(lat_i*180.0/M_PI, lon_i*180.0/M_PI);
		}

		return latlons;
	}

	QVector<QPointF> computePartialGreatCircleLine(const QPointF &from, const QPointF &to, int numptsTotal, int numptsPartial, double *tdist){
		QVector<QPointF> latlonGc = computeGreatCircleLine(from, to, numptsPartial + 1, tdist);

		for(int p = 0; p < latlonGc.count(); ++p){
			qDebug() << "partial = " << latlonGc[p].x() << latlonGc[p].y();
		}

		/// Now fill in between the lines using approximation.
		QVector<QPointF> ret;
		ret.reserve(numptsTotal);
		int numPerStep = numptsTotal / numptsPartial;
		int remainingPoints = numptsTotal;

		for(int i = 0; i < numptsPartial; ++i){
			const QPointF &f = latlonGc[i], &t = latlonGc[i + 1];
			int thisStepCount = numPerStep;
			if(remainingPoints < thisStepCount) thisStepCount = remainingPoints;
			else remainingPoints -= thisStepCount;
			QVector<QPointF> r = computeApproximateGreatCircleLine(f, t, thisStepCount + 1, NULL);

			for(int pr = 0; pr < r.count(); ++pr){
				qDebug() << " partial from " << f.x() << f.y() << " to " << t.x() << t.y() << " [" << pr << "] = " << r[pr].x() << r[pr].y();
			}

			const int thisCnt = r.count();
			for(int p = 1; p < thisCnt; ++p){
				ret << r[p];
			}
		}

		return ret;
	}

	double *computeElevationVector(const TerrainClass *terrain, bool includeBldg, const QPointF &from, const QPointF &to, int numpts){
		double tdist;
		double terrainHeight, bldgHeight;
		MultibandRasterClass::HeightResult lidarHeightResult;
		CConst::HeightSourceEnum heightSource;

		// QVector<QPointF> latlons = computeApproximateGreatCircleLine(from, to, numpts, &tdist);
		QVector<QPointF> latlons = computeGreatCircleLineMM(from, to, numpts, &tdist);

		double bldgDistRes = 1.0; // 1 meter
		int maxBldgStep = std::min(100, (int) floor(tdist*1000/bldgDistRes));
		int stepIdx;
		int numBldgPtTX = 0;
		int numBldgPtRX = 0;

		// printf("Returned %d points instead of %d.\n", latlons.count(), numpts);

		double *ret = (double *)calloc(sizeof(double), numpts + 2);
		ret[0] = numpts - 1;
		ret[1] = (tdist / (numpts-1)) * 1000.0;

		if (includeBldg) {
			/*********************************************************************************************************/
			// Compute numBldgPtTX so building at TX can be removed
			/*********************************************************************************************************/
			const QPointF &ptTX = latlons.at(0);
			terrain->getTerrainHeight(ptTX.y(), ptTX.x(), terrainHeight, bldgHeight, lidarHeightResult, heightSource);
			if (lidarHeightResult == MultibandRasterClass::BUILDING) {
				bool found = false;
				for(stepIdx=1; (stepIdx<maxBldgStep)&&(!found); stepIdx++) {
					double ptIdxDbl = stepIdx*bldgDistRes / ret[1];
					int n0 = (int) floor(ptIdxDbl);
					int n1 = n0 + 1;
					double ptx = latlons.at(n0).x()*(n1 - ptIdxDbl) + latlons.at(n1).x()*(ptIdxDbl - n0);
					double pty = latlons.at(n0).y()*(n1 - ptIdxDbl) + latlons.at(n1).y()*(ptIdxDbl - n0);
					terrain->getTerrainHeight(pty, ptx, terrainHeight, bldgHeight, lidarHeightResult, heightSource);
					if (lidarHeightResult != MultibandRasterClass::BUILDING) {
						found = true;
						numBldgPtTX = n1;
					}
				}
				if (!found) {
					numBldgPtTX = (int) floor(maxBldgStep*bldgDistRes / ret[1]);
				}
			} else {
				numBldgPtTX = 0;
			}
			/*********************************************************************************************************/

			/*********************************************************************************************************/
			// Compute numBldgPtRX so building at RX can be removed
			/*********************************************************************************************************/
			const QPointF &ptRX = latlons.at(numpts-1);
			terrain->getTerrainHeight(ptRX.y(), ptRX.x(), terrainHeight, bldgHeight, lidarHeightResult, heightSource);
			if (lidarHeightResult == MultibandRasterClass::BUILDING) {
				bool found = false;
				for(stepIdx=1; (stepIdx<maxBldgStep)&&(!found); stepIdx++) {
					double ptIdxDbl = (tdist*1000 - stepIdx*bldgDistRes) / ret[1];
					int n0 = (int) floor(ptIdxDbl);
					int n1 = n0 + 1;
					double ptx = latlons.at(n0).x()*(n1 - ptIdxDbl) + latlons.at(n1).x()*(ptIdxDbl - n0);
					double pty = latlons.at(n0).y()*(n1 - ptIdxDbl) + latlons.at(n1).y()*(ptIdxDbl - n0);
					terrain->getTerrainHeight(pty, ptx, terrainHeight, bldgHeight, lidarHeightResult, heightSource);
					if (lidarHeightResult != MultibandRasterClass::BUILDING) {
						found = true;
						numBldgPtRX = numpts - n1;
					}
				}
				if (!found) {
					numBldgPtRX = (int) floor(maxBldgStep*bldgDistRes / ret[1]);
				}
			} else {
				numBldgPtRX = 0;
			}
			/*********************************************************************************************************/
		}

		double *pos = ret + 2;
		for(int i = 0; i < numpts; ++i, ++pos){
			const QPointF &pt = latlons.at(i);

			terrain->getTerrainHeight(pt.y(), pt.x(), terrainHeight, bldgHeight, lidarHeightResult, heightSource);
			if (includeBldg && (lidarHeightResult == MultibandRasterClass::BUILDING) && (i >= numBldgPtTX) && (i <= numpts-1-numBldgPtRX)) {
				*pos = terrainHeight + bldgHeight;
			} else {
				*pos = terrainHeight;
			}
		}

		//print_values(ret, 2, numpts + 2, 8);

		return ret;
	}

	double greatCircleDistance(double lat1, double lon1,
			double lat2, double lon2)
	{
		double lat1rad = deg2rad(lat1);
		double lon1rad = deg2rad(lon1); 
		double lat2rad = deg2rad(lat2);
		double lon2rad = deg2rad(lon2);
		// double deltalat = lat2rad - lat1rad;
		double deltalon = lon2rad - lon1rad;

		// law of cosines
		// inaccurate for small distances?
		double centerAngle = std::acos(std::sin(lat1rad)*std::sin(lat2rad) +
				std::cos(lat1rad)*std::cos(lat2rad) * std::cos(deltalon));
		/*double hav = 2*std::asin(
		  std::sqrt(sqr(std::sin(deltalat/2)) +
		  std::cos(lat1rad) * 
		  std::cos(lat2rad) *
		  sqr(std::sin(deltalon/2))));
		  std::cout << "hav: " << rad2deg(hav) << std::endl;*/
		return rad2deg(centerAngle);
	}

	void computeElevationVector(const TerrainClass *terrain, bool includeBldg, const QPointF &from, const QPointF &to, int numpts,
			std::vector<double> &hi, std::vector<double> &di, double ae) {
		// For compatability with P.452
		double tdist;
		QVector<QPointF> latlons = computeApproximateGreatCircleLine(from, to, numpts, &tdist);

		// printf("Returned %d points instead of %d.\n", latlons.count(), numpts);

		hi.resize(numpts);

		for(int i = 0; i < numpts; i++){
			const QPointF &pt = latlons.at(i);

			double terrainHeight, bldgHeight;
			MultibandRasterClass::HeightResult lidarHeightResult;
			CConst::HeightSourceEnum heightSource;
			terrain->getTerrainHeight(pt.y(), pt.x(), terrainHeight, bldgHeight, lidarHeightResult, heightSource);

			if (includeBldg && (lidarHeightResult == MultibandRasterClass::BUILDING)) {
				// CORE DUMP, not yet implemented, need to remove building at TX and RX locations
				printf("%d", *((int *) NULL));
				hi[i] = terrainHeight + bldgHeight;
			} else {
				hi[i] = terrainHeight;
			}
		}

		double angularDistance = greatCircleDistance(from.x(), from.y(), to.x(), to.y());
		double pathDistance = deg2rad(angularDistance) * ae;

		for(int i=0; i<numpts; i++) {
			di[i] = (pathDistance*i)/(numpts-1);
		}

		return;
	}

	double runPointToPoint(const TerrainClass *terrain, bool includeBldg, QPointF transLocLatLon, double transHt, QPointF receiveLocLatLon,
			double receiveHt, double lineOfSightDistanceKm, double eps_dielect, double sgm_conductivity, double eno_ns_surfref,
			double frq_mhz, int radio_climate, int pol, double conf, double rel, int numpts, char *prefix,double **heightProfilePtr){
		if (!(*heightProfilePtr)) {
			*heightProfilePtr = computeElevationVector(terrain, includeBldg, transLocLatLon, receiveLocLatLon, numpts);
		}

		double rv;
		std::string strmode;
		// char strmode[50];
		int errnum;

		if (itmInitFlag) {
			LOGGER_INFO(logger) << "ITM Parameter: eps_dielect = " << eps_dielect;
			LOGGER_INFO(logger) << "ITM Parameter: sgm_conductivity = " << sgm_conductivity;
			LOGGER_INFO(logger) << "ITM Parameter: pol = " << pol;
			itmInitFlag = false;
		}
		point_to_point(*heightProfilePtr, transHt, receiveHt, eps_dielect, sgm_conductivity, eno_ns_surfref, frq_mhz, radio_climate, pol, conf, rel, rv, strmode, errnum);
		//qDebug() << " point_to_point" << rv << strmode << errnum;

		if(prefix != NULL){
			dumpHeightProfile(prefix, *heightProfilePtr);
		}

		return rv;
	}

	bool isLOS(const TerrainClass *terrain, QPointF transLocLatLon, double transHt, QPointF receiveLocLatLon,
			double receiveHt, double lineOfSightDistanceKm, int numpts, double **heightProfilePtr) {
		if (!(*heightProfilePtr)) {
			*heightProfilePtr = computeElevationVector(terrain, true, transLocLatLon, receiveLocLatLon, numpts);
		}

		double txHeightAMSL = (*heightProfilePtr)[2] + transHt;
		double rxHeightAMSL = (*heightProfilePtr)[2+numpts-1] + receiveHt;

		int ptIdx;
		bool losFlag = true;
		for(ptIdx = 0; (ptIdx < numpts)&&(losFlag); ++ptIdx){
			double ptHeight = (*heightProfilePtr)[2 + ptIdx];
			double signalHeight = ( txHeightAMSL*(numpts-1-ptIdx) + rxHeightAMSL*ptIdx ) / (numpts-1);

			double clearance = signalHeight - ptHeight;

			if (clearance < 0.0) {
				losFlag = false;
			}
		}

		return losFlag;
	}

	/******************************************************************************************/

	void dumpHeightProfile(const char *prefix, const double *heights){
		int numHeights = int(heights[0]);

		for(int x = 0; x < numHeights; ++x){
			double ht = heights[2 + x];
			qDebug() << "HEIGHTPROFILE" << prefix << ht;
		}
	}

	long long numInvalidSRTM;
	long long numSRTM;
}

