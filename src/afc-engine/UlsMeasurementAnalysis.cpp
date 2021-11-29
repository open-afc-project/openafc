#include "EcefModel.h"
#include "UlsMeasurementAnalysis.h"
#include "gdal_priv.h"
#include "GdalDataModel.h"
#include "cpl_conv.h" // for CPLMalloc()
#include <QImage>
#include <QPainter>
#include <QColor>
#include <iomanip>
#include <exception>
#include <tuple>
#include "GdalHelpers.h"
#include "GdalDataDir.h"

#include "cconst.h"
#include "EcefModel.h"
#include "UlsMeasurementAnalysis.h"
#include "gdal_priv.h"
#include "GdalDataModel.h"
#include "BuildingRasterModel.h"
#include "cpl_conv.h" // for CPLMalloc()

extern void point_to_point(double elev[], double tht_m, double rht_m,
                           double eps_dielect, double sgm_conductivity, double eno_ns_surfref,
                           double frq_mhz, int radio_climate, int pol, double conf, double rel,
                           double &dbloss, char *strmode, int &errnum);

namespace {
    // Logger for all instances of class
    LOGGER_DEFINE_GLOBAL(logger, "UlsMeasurementAnalysis")

    const double fixedEpsDielect = 15;
    const double fixedSgmConductivity = 0.005;
    const double fixedEsNoSurfRef = 301;
    const int fixedRadioClimate = 6;
    const int fixedPolarization = 1;
    const double fixedConfidence = 0.5;
    const double fixedRelevance = 0.5;
}; // end namespace

namespace UlsMeasurementAnalysis {
    void dumpHeightProfile(const char *prefix, const double *heights);

#if 0
    // Remove this function, replace with computeTerrainHeight() function below
    double computeHeight(QPointF loc, const WorldData *globeData, const GdalDataDir *dataDir){
        numSRTM++;
        qint16 ht;
        if(!dataDir->getHeight(ht, loc.x(), loc.y())){
            // qDebug() << "USING GLOBAL DATA HEIGHT";
            numInvalidSRTM++;
            return globeData->valueAtLatLon(loc.x(), loc.y());
        } else {
            // qDebug() << "USING SRTM DATA HEIGHT";
            return ht;
        }
    }
#endif

    double computeTerrainHeight(const double& latDeg, const double& lonDeg, const BuildingRasterModel *bldg, const WorldData *globeData, const GdalDataDir *dataDir)
    {
        if (bldg) {
            for (QRectF r : bldg->getBounds()) {
                if (r.contains(lonDeg, latDeg)) {
                    double height = globeData->valueAtLatLon(latDeg, lonDeg);
                    if (height == WorldData::NO_DATA) {
                        height = 0.0;
                    }
                    return(height);
                }
            }
        }
        // lat/lon not in building region

        numSRTM++;
        qint16 ht;
        if(!dataDir->getHeight(ht, latDeg, lonDeg)){
            // qDebug() << "USING GLOBAL DATA HEIGHT";
            numInvalidSRTM++;
            double height = globeData->valueAtLatLon(latDeg, lonDeg);
            if (height == WorldData::NO_DATA) {
                height = 0.0;
            }
            return(height);
        } else {
            // qDebug() << "USING SRTM DATA HEIGHT";
            return ht;
        }
    }

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
            Vector3 fvec = EcefModel::geodeticToEcef(from.x(), from.y(), 0);
            Vector3 tvec = EcefModel::geodeticToEcef(to.x(), to.y(), 0);
            *tdist = (fvec - tvec).len();
        }

        return latlons;
    }
    
    QVector<QPointF> computeGreatCircleLine(const QPointF &from, const QPointF &to, int numpts, double *tdist){
        //  We're going to do what the fortran program does. It's kinda stupid, though.

        //  Please excuse fortran style variable names. I have decrypted them where possible.
        double earthRad = 6370.0;

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

        double delta = greatCircleDistance / numpts;

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

        QVector<QPointF> latlons = computeApproximateGreatCircleLine(from, to, numpts, &tdist);

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

        int srtmKey = INT_MAX;
        qint16 *srtmData;
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
    

    double *computeElevationVectorWithSRTM1(const WorldData *wd, const GdalDataDir *srtm1, const QPointF &from, const QPointF &to, int numpts){
        double tdist;
        QVector<QPointF> latlons = computeApproximateGreatCircleLine(from, to, numpts, &tdist);

        // printf("Returned %d points instead of %d.\n", latlons.count(), numpts);           

        double *ret = (double *)calloc(sizeof(double), numpts + 2);
        ret[0] = numpts - 1;
        ret[1] = tdist / numpts * 1000.0;

        if(srtm1 == NULL){
            for(int i = 0; i < numpts; i++){
                const QPointF &pt = latlons.at(i);

                // qint16 height = srtm1->getHeight(pt.x(), pt.y());
                double height = wd->valueAtLatLon(pt.x(), pt.y());
                if (height == WorldData::NO_DATA) {
                    height = 0.0;
                }
                ret[2 + i] = height;
            }
        }
        else{
            double *pos = ret + 2;
            for(int i = 0; i < numpts; ++i, ++pos){
                const QPointF &pt = latlons.at(i);
                qint16 height;

                

                // some dead spots exist in srtm1.  revert to wd when this is true
                if (!srtm1->getHeight(height, pt.x(), pt.y())) {
                    numInvalidSRTM++;
                    double heightVal = wd->valueAtLatLon(pt.x(), pt.y());
                    if (heightVal == WorldData::NO_DATA) {
                        heightVal = 0.0;
                    }
                    *pos = heightVal;
                }
                else {
                    *pos = height;
                }
                numSRTM++;
            }
        }

        //print_values(ret, 2, numpts + 2, 8);

        return ret;
    }
    /******************************************************************************************/
    /**** computePathIdxBldg(), used to remove bldgs containing RLAN/FS_RX with raster     ****/
    /**** data is used.  Only called from computeElevationVectorWithBldg()                 ****/
    /******************************************************************************************/
    int computePathIdxBldg(double fromX, double fromY, double toX, double toY, int numpts, GdalDataModel *bldgPoly, int sense, int endpt) {

        OGRPoint fromPoint;
        OGRPoint   toPoint;

        fromPoint.setX(fromX);
        fromPoint.setY(fromY);
        toPoint.setX(toX);
        toPoint.setY(toY);

        int minIdx, maxIdx;
        double minVal, maxVal;
        bool initFlag = false;

        OGRPoint *ogrPt = (endpt==0 ? &fromPoint : &toPoint);
        bldgPoly->getLayer()->SetSpatialFilter(ogrPt); // Filter entire building database for that intersect the RLAN
        bldgPoly->getLayer()->ResetReading();
        std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> ptrOFeature;
        while ((ptrOFeature = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(bldgPoly->getLayer()->GetNextFeature())) != NULL) {
            // GDAL maintains ownership of returned pointer, so we should not manage its memory
            OGRGeometry *ptrOGeometry = ptrOFeature->GetGeometryRef();
            // Check whether the current geometry is a polygon
            if (ptrOGeometry != NULL && ptrOGeometry->getGeometryType() == OGRwkbGeometryType::wkbPolygon) {
                if (ptrOGeometry->Contains(ogrPt)) {
                    OGRPolygon *poly = (OGRPolygon *) ptrOGeometry;
                    OGRLinearRing *pRing = poly->getExteriorRing();
                    if (pRing->getNumPoints() >= 4) {
                        double prevPtX = pRing->getX(0);
                        double prevPtY = pRing->getY(0);
                        for(int ptIdx=1; ptIdx<pRing->getNumPoints(); ptIdx++) {
                            // 1: from
                            // 2: to
                            // 3: prevPt
                            // 4: pt
                            double ptX = pRing->getX(ptIdx);
                            double ptY = pRing->getY(ptIdx);
                            bool checkSegment = true;
                            if (initFlag) {
                                if (sense == 0) {
                                    if (    (ptX >= minVal) && (ptX<=maxVal)
                                         && (prevPtX >= minVal) && (prevPtX<=maxVal) ) {
                                        checkSegment = false;
                                    }
                                } else {
                                    if (    (ptY >= minVal) && (ptY<=maxVal)
                                         && (prevPtY >= minVal) && (prevPtY<=maxVal) ) {
                                        checkSegment = false;
                                    }
                                }
                            }

                            if (checkSegment) {
                            double s1 = (prevPtY   - fromY)*(toX -   fromX) - (prevPtX -   fromX)*(toY -   fromY);
                            double s2 = (    ptY   - fromY)*(toX -   fromX) - (    ptX -   fromX)*(toY -   fromY);
                            double s3 = (  fromY - prevPtY)*(ptX - prevPtX) - (  fromX - prevPtX)*(ptY - prevPtY);
                            double s4 = (    toY - prevPtY)*(ptX - prevPtX) - (    toX - prevPtX)*(ptY - prevPtY);

                            if (    (((s1<=0.0)&&(s2>=0.0))||((s1>=0.0)&&(s2<=0.0)))
                                 && (((s3<=0.0)&&(s4>=0.0))||((s3>=0.0)&&(s4<=0.0))) ) {
                                int currIdx;
                                double currVal;
                                if (sense == 0) {
                                    double xc = ((prevPtY+ptY-fromY-toY)*(toX-fromX)*(ptX-prevPtX) + (fromX+toX)*(toY-fromY)*(ptX-prevPtX) - (prevPtX+ptX)*(ptY-prevPtY)*(toX-fromX))
                                              / (2*((toY-fromY)*(ptX-prevPtX) - (ptY-prevPtY)*(toX-fromX)));
                                    currIdx = (int) floor( ((xc - fromX)/(toX-fromX))*(numpts-1) + 0.5 );
                                    currVal = xc;
                                } else {
                                    double yc = ((prevPtX+ptX-fromX-toX)*(toY-fromY)*(ptY-prevPtY) + (fromY+toY)*(toX-fromX)*(ptY-prevPtY) - (prevPtY+ptY)*(ptX-prevPtX)*(toY-fromY))
                                              / (2*((toX-fromX)*(ptY-prevPtY) - (ptX-prevPtX)*(toY-fromY)));
                                    currIdx = (int) floor( ((yc - fromY)/(toY-fromY))*(numpts-1) + 0.5 );
                                    currVal = yc;
                                }
                                if (!initFlag) {
                                    minIdx = currIdx;
                                    maxIdx = currIdx;
                                    minVal = currVal;
                                    maxVal = currVal;
                                    initFlag = true;
                                } else if (currIdx < minIdx) {
                                    minIdx = currIdx;
                                    minVal = currVal;
                                } else if (currIdx > maxIdx) {
                                    maxIdx = currIdx;
                                    maxVal = currVal;
                                }
                            }
                            }
                            prevPtX = ptX;
                            prevPtY = ptY;
                        }
                    }
                }
            }
        }

        int extIdx;
        if (!initFlag) {
            extIdx = ((endpt == 0) ? -1 : numpts);
        } else {
            extIdx = ((endpt == 0) ? maxIdx : minIdx);
        }

        return(extIdx);
    }
    /******************************************************************************************/

    /******************************************************************************************/
    /**** Using Building Raster Data                                                       ****/
    /******************************************************************************************/
    double *computeElevationVectorWithBldg(const WorldData *wd, const GdalDataDir *srtm1, BuildingRasterModel *bldg, GdalDataModel *bldgPoly, const QPointF &from, const QPointF &to, int numpts, double lineOfSightDistanceKm
#if MM_DEBUG
                                           , std::vector<std::string> &ITMHeightType
#endif
                                          ) {

        int heightFieldIdx = bldgPoly->heightFieldIdx;

        // The from/to QPointF's have LAT in x-coord and LON in y-coord.  UTM/WGS84 transform has LON in x-coord and LAT in y-coord.
        double fromX = from.y();
        double fromY = from.x();
        double toX   = to.y();
        double toY   = to.x();

        double *ret = (double *)calloc(sizeof(double), numpts + 2);
        ret[0] = numpts - 1;
        ret[1] = 1000 * lineOfSightDistanceKm / numpts;

        int sense = (fabs(toX - fromX) >= fabs(toY-fromY) ? 0 : 1);
        int fromMaxIdx = computePathIdxBldg(fromX, fromY, toX, toY, numpts, bldgPoly, sense, 0);
        int   toMinIdx = computePathIdxBldg(fromX, fromY, toX, toY, numpts, bldgPoly, sense, 1);



#if MM_DEBUG
        std::string heightType;
        ITMHeightType.clear();
#endif
        double *pos = ret + 2;
        std::pair<BuildingRasterModel::HeightResult, double> height_res;

        for(int pointIdx = 0; pointIdx < numpts; ++pointIdx, ++pos){

            double intLon = (fromX*(numpts-1-pointIdx)+toX*pointIdx)/(numpts-1);
            double intLat = (fromY*(numpts-1-pointIdx)+toY*pointIdx)/(numpts-1);

            bool removeBldgFlag = ((pointIdx <= fromMaxIdx) || (pointIdx >= toMinIdx));

            height_res = bldg->getHeight(intLat, intLon);
#if MM_DEBUG
            if (pointIdx <= fromMaxIdx) {
                heightType = "RLANBLDG";
            } else if (pointIdx >= toMinIdx) {
                heightType = "FSRXBLDG";
            } else if (height_res.first == BuildingRasterModel::OUTSIDE_REGION) {
                heightType = "OBR"; // Outside Building Region
            } else if (height_res.first == BuildingRasterModel::NO_BUILDING) {
                heightType = "NOBLDG";
            } else {
                heightType = "BLDG";
            }
#endif

            if ((!removeBldgFlag) && (height_res.first == BuildingRasterModel::OUTSIDE_REGION)) {
                qint16 srtmHeight;
                numSRTM++;

                // some dead spots exist in srtm1.  revert to wd when this is true
                if (!srtm1->getHeight(srtmHeight, intLat, intLon)) {
                    numInvalidSRTM++;
                    height_res.second = wd->valueAtLatLon(intLat, intLon);
                    if (height_res.second == WorldData::NO_DATA) {
                        height_res.second = 0.0;
#if MM_DEBUG
                        heightType += "_NODATA";
                    } else {
                        heightType += "_WD";
#endif
                    }
                } else {
                    height_res.second = (double) srtmHeight;
#if MM_DEBUG
                    heightType += "_SRTM";
#endif
                }
            } else if (removeBldgFlag || (height_res.first == BuildingRasterModel::NO_BUILDING)) {
                    height_res.second = wd->valueAtLatLon(intLat, intLon);
                if (height_res.second == WorldData::NO_DATA) {
                    height_res.second = 0.0;
#if MM_DEBUG
                    heightType += "_NODATA";
                } else {
                    heightType += "_WD";
#endif
                }
            }

            *pos = height_res.second;
#if MM_DEBUG
            ITMHeightType.push_back(heightType);
#endif
        }

        //print_values(ret, 2, numpts + 2, 8);

        return ret;
    }
    /******************************************************************************************/

    /******************************************************************************************/
    /**** Using Building Polygon Data                                                      ****/
    /******************************************************************************************/
    double *computeElevationVectorWithBldg(const WorldData *wd, const GdalDataDir *srtm1, GdalDataModel *bldg, const QPointF &from, const QPointF &to, int numpts, double lineOfSightDistanceKm){

        int heightFieldIdx = bldg->heightFieldIdx;

        // The from/to QPointF's have LAT in x-coord and LON in y-coord.  UTM/WGS84 transform has LON in x-coord and LAT in y-coord.
        double fromX = from.y();
        double fromY = from.x();
        double toX   = to.y();
        double toY   = to.x();

        int transformSuccess;
        bldg->testCoordTransform->TransformEx(1, &fromX, &fromY, nullptr, &transformSuccess);
        bldg->testCoordTransform->TransformEx(1, &toX,   &toY,   nullptr, &transformSuccess);

        OGRPoint fromPoint;
        OGRPoint   toPoint;

        fromPoint.setX(fromX);
        fromPoint.setY(fromY);
        toPoint.setX(toX);
        toPoint.setY(toY);

        int sense = (fabs(toX - fromX) >= fabs(toY-fromY) ? 0 : 1);

        std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> ptrOFeature;

        double *ret = (double *)calloc(sizeof(double), numpts + 2);
        ret[0] = numpts - 1;
        ret[1] = 1000 * lineOfSightDistanceKm / numpts;

        int srtmKey = INT_MAX;
        qint16 *srtmData;

        for(int i=0; i<numpts; ++i) {
            ret[2+i] = std::numeric_limits<double>::quiet_NaN();
        }
#if 0
        std::vector<GIntBig> exclIDList;
        for(int i=0; i<2; i++) {
            OGRPoint *ogrPt = (i==0 ? &fromPoint : &toPoint);

            bldg->getLayer()->SetSpatialFilter(ogrPt);
            bldg->getLayer()->ResetReading();
            while ((ptrOFeature = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(bldg->getLayer()->GetNextFeature())) != NULL) {
                // GDAL maintains ownership of returned pointer, so we should not manage its memory
                OGRGeometry *ptrOGeometry = ptrOFeature->GetGeometryRef();
                if (ptrOGeometry->Contains(ogrPt)) {
                    exclIDList.push_back(ptrOFeature->GetFID());
                }
            }
        }
#endif

#if 1
        GdalHelpers::GeomUniquePtr<OGRLineString> signalPath(GdalHelpers::createGeometry<OGRLineString>());
        signalPath->addPoint(fromX, fromY);
        signalPath->addPoint(toX, toY);

        bldg->getLayer()->SetSpatialFilter(signalPath.get()); // Filter entire building database for only polygons which intersect with signal path into constrained layer
        std::vector<GIntBig> idList;

        int numBldg = 0;
        bldg->getLayer()->ResetReading();
        while ((ptrOFeature = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(bldg->getLayer()->GetNextFeature())) != NULL) {
            // GDAL maintains ownership of returned pointer, so we should not manage its memory
            OGRGeometry *ptrOGeometry = ptrOFeature->GetGeometryRef();

            // Check whether the current geometry is a polygon
            if (ptrOGeometry != NULL && ptrOGeometry->getGeometryType() == OGRwkbGeometryType::wkbPolygon) {
                double flag = true;
                if (ptrOGeometry->Contains(&fromPoint)) {
                    // std::cout << "CONTAINS RLAN" << std::endl;
                    flag = false;
                }
                if (ptrOGeometry->Contains(&toPoint)) {
                    // std::cout << "CONTAINS FS" << std::endl;
                    flag = false;
                }

                if (flag) {
                    OGRPolygon *poly = (OGRPolygon *) ptrOGeometry;
                    OGRLinearRing *pRing = poly->getExteriorRing();
                    if (pRing->getNumPoints() >= 4) {
                        int minIdx, maxIdx;
                        double minVal, maxVal;
                        double prevPtX = pRing->getX(0);
                        double prevPtY = pRing->getY(0);
                        for(int ptIdx=1; ptIdx<pRing->getNumPoints(); ptIdx++) {
                            // 1: from
                            // 2: to
                            // 3: prevPt
                            // 4: pt
                            double ptX = pRing->getX(ptIdx);
                            double ptY = pRing->getY(ptIdx);
                            bool checkSegment = true;
                            if (!flag) {
                                if (sense == 0) {
                                    if (    (ptX >= minVal) && (ptX<=maxVal)
                                         && (prevPtX >= minVal) && (prevPtX<=maxVal) ) {
                                        checkSegment = false;
                                    }
                                } else {
                                    if (    (ptY >= minVal) && (ptY<=maxVal)
                                         && (prevPtY >= minVal) && (prevPtY<=maxVal) ) {
                                        checkSegment = false;
                                    }
                                }
                            }

                            if (checkSegment) {
                            double s1 = (prevPtY   - fromY)*(toX -   fromX) - (prevPtX -   fromX)*(toY -   fromY);
                            double s2 = (    ptY   - fromY)*(toX -   fromX) - (    ptX -   fromX)*(toY -   fromY);
                            double s3 = (  fromY - prevPtY)*(ptX - prevPtX) - (  fromX - prevPtX)*(ptY - prevPtY);
                            double s4 = (    toY - prevPtY)*(ptX - prevPtX) - (    toX - prevPtX)*(ptY - prevPtY);

                            if (    (((s1<=0.0)&&(s2>=0.0))||((s1>=0.0)&&(s2<=0.0)))
                                 && (((s3<=0.0)&&(s4>=0.0))||((s3>=0.0)&&(s4<=0.0))) ) {
                                int currIdx;
                                double currVal;
                                if (sense == 0) {
                                    double xc = ((prevPtY+ptY-fromY-toY)*(toX-fromX)*(ptX-prevPtX) + (fromX+toX)*(toY-fromY)*(ptX-prevPtX) - (prevPtX+ptX)*(ptY-prevPtY)*(toX-fromX))
                                              / (2*((toY-fromY)*(ptX-prevPtX) - (ptY-prevPtY)*(toX-fromX)));
                                    currIdx = (int) floor( ((xc - fromX)/(toX-fromX))*(numpts-1) + 0.5 );
                                    currVal = xc;
                                } else {
                                    double yc = ((prevPtX+ptX-fromX-toX)*(toY-fromY)*(ptY-prevPtY) + (fromY+toY)*(toX-fromX)*(ptY-prevPtY) - (prevPtY+ptY)*(ptX-prevPtX)*(toY-fromY))
                                              / (2*((toX-fromX)*(ptY-prevPtY) - (ptX-prevPtX)*(toY-fromY)));
                                    currIdx = (int) floor( ((yc - fromY)/(toY-fromY))*(numpts-1) + 0.5 );
                                    currVal = yc;
                                }
                                if (flag) {
                                    minIdx = currIdx;
                                    maxIdx = currIdx;
                                    minVal = currVal;
                                    maxVal = currVal;
                                    flag = false;
                                } else if (currIdx < minIdx) {
                                    minIdx = currIdx;
                                    minVal = currVal;
                                } else if (currIdx > maxIdx) {
                                    maxIdx = currIdx;
                                    maxVal = currVal;
                                }
                            }
                            }
                            prevPtX = ptX;
                            prevPtY = ptY;
                        }
                        if (!flag) {
                            numBldg++;
                            double height = ptrOFeature->GetFieldAsDouble(heightFieldIdx);
                            for(int i=minIdx; i<=maxIdx; ++i) {
                                if (std::isnan(ret[2+i]) || (height > ret[2+i])) {
                                    ret[2+i] = height;
                                }
                            }
                        }
                    }
                }

                // std::cout << std::endl;
            } else {
                LOGGER_DEBUG(logger) << "GdalDataModel::getBuildingsAtPoint(): Can't find polygon geometries in current feature";
            }
        }

        for(int i=0; i<numpts; ++i) {
            if (std::isnan(ret[2+i])) {
                double intLon = ((fromX*(numpts-1-i)+toX*i)/(numpts-1));
                double intLat = ((fromY*(numpts-1-i)+toY*i)/(numpts-1));

                qint16 srtmHeight;
                numSRTM++;

                // some dead spots exist in srtm1.  revert to wd when this is true
                if (!srtm1->getHeight(srtmHeight, intLat, intLon)) {
                    numInvalidSRTM++;
                    double height = wd->valueAtLatLon(intLat, intLon);
                    if (height == WorldData::NO_DATA) {
                        height = 0.0;
                    }
                    ret[2+i] = height;
                } else {
                    ret[2+i] = (double) srtmHeight;
                }
            }
        }

        LOGGER_DEBUG(logger) << "PATH_PROFILE: NUM_BLDG = " << numBldg << "  NUMPTS = " << numpts;
#endif

#if 0
        double *pos = ret + 2;
        OGRPoint intPoint;
        double height_m;

        GdalHelpers::GeomUniquePtr<OGRLineString> lineSeg(GdalHelpers::createGeometry<OGRLineString>());
        lineSeg->addPoint(fromX, fromY);
        lineSeg->addPoint(toX, toY);

        for(int pointIdx = 0; pointIdx < numpts; ++pointIdx, ++pos){
            intPoint.setX((fromX*(numpts-1-pointIdx)+toX*pointIdx)/(numpts-1));
            intPoint.setY((fromY*(numpts-1-pointIdx)+toY*pointIdx)/(numpts-1));

            bool foundBldg = false;
#if 0
            for(int polyIdx=0; polyIdx<idList.size(); polyIdx++) {
                ptrOFeature = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(bldg->getLayer()->GetFeature(idList[polyIdx]));
                OGRGeometry *ptrOGeometry = ptrOFeature->GetGeometryRef();
                if (ptrOGeometry->Contains(&intPoint)) {
                    double height = ptrOFeature->GetFieldAsDouble(heightFieldIdx);
                    if ((!foundBldg) || (height > height_m)) {
                        height_m = height;
                    }
                    foundBldg = true;
                }
            }
#else
            bldg->getLayer()->SetSpatialFilter(&intPoint);
            bldg->getLayer()->ResetReading();
            while ((ptrOFeature = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(bldg->getLayer()->GetNextFeature())) != NULL) {
                if(std::find(exclIDList.begin(), exclIDList.end(), ptrOFeature->GetFID()) == exclIDList.end()) {
                    // GDAL maintains ownership of returned pointer, so we should not manage its memory
                    OGRGeometry *ptrOGeometry = ptrOFeature->GetGeometryRef();

                    if (ptrOGeometry->Contains(&intPoint)) {
                        double height = ptrOFeature->GetFieldAsDouble(heightFieldIdx);
                        if ((!foundBldg) || (height > height_m)) {
                            height_m = height;
                        }
                        foundBldg = true;
                    }
                }
            }
#endif

            // some dead spots exist in bldg.  revert to strm when this is true
            if (!foundBldg) {

                double intLon = intPoint.getX();
                double intLat = intPoint.getY();

                int transformSuccess;
                // Perform transform into WGS-84 lat/lon coordinate system
                int allTransformSuccess = bldg->invCoordTransform->TransformEx(1, &intLon, &intLat, nullptr, &transformSuccess);

                qint16 srtmHeight = srtm1->getHeight(intLat, intLon, &srtmKey, &srtmData);
                numSRTM++;

                // some dead spots exist in srtm1.  revert to wd when this is true
                if (srtmHeight == GdalDataDir::INVALID_HEIGHT) {
                    numInvalidSRTM++;
                    height_m = wd->valueAtLatLon(intLat, intLon);
                    if (height_m == WorldData::NO_DATA) {
                        height_m = 0.0;
                    }
                } else {
                    height_m = (double) srtmHeight;
                }
            }

            *pos = height_m;
        }
#endif

        //print_values(ret, 2, numpts + 2, 8);

        return ret;
    }
    /******************************************************************************************/








    double greatCircleDistance(double lat1, double lon1,
                                double lat2, double lon2)
    {
        double lat1rad = deg2rad(lat1);
        double lon1rad = deg2rad(lon1); 
        double lat2rad = deg2rad(lat2);
        double lon2rad = deg2rad(lon2);
        //    double deltalat = lat2rad - lat1rad;
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

    void computeElevationVectorWithSRTM1(const WorldData *wd, const GdalDataDir *srtm1, const QPointF &from, const QPointF &to, int numpts,
        std::vector<double> &hi, std::vector<double> &di, double ae) {
        // For compatability with P.452
        double tdist;
        QVector<QPointF> latlons = computeApproximateGreatCircleLine(from, to, numpts, &tdist);

        // printf("Returned %d points instead of %d.\n", latlons.count(), numpts);           

        hi.resize(numpts);

        if(srtm1 == NULL){
            for(int i = 0; i < numpts; i++){
                const QPointF &pt = latlons.at(i);

                // qint16 height = srtm1->getHeight(pt.x(), pt.y());
                double height = wd->valueAtLatLon(pt.x(), pt.y());
                if (height == WorldData::NO_DATA) {
                    height = 0.0;
                }
                hi[i] = height;
                numInvalidSRTM++;
                numSRTM++;
            }
        }
        else{
            int srtmKey = INT_MAX;
            qint16 *srtmData;
            for(int i = 0; i < numpts; i++){
                const QPointF &pt = latlons.at(i);
                qint16 height;

                // some dead spots exist in srtm1.  revert to wd when this is true
                if (!srtm1->getHeight(height, pt.x(), pt.y())) {
                    double heightVal = wd->valueAtLatLon(pt.x(), pt.y());
                    if (heightVal == WorldData::NO_DATA) {
                        heightVal = 0.0;
                    }
                    hi[i] = heightVal;
                    numInvalidSRTM++;
                } else {
                    hi[i] = height;
                }

                numSRTM++;
            }
        }

        double angularDistance = greatCircleDistance(from.x(), from.y(), to.x(), to.y());
        double pathDistance = deg2rad(angularDistance) * ae;

        for(int i=0; i<numpts; i++) {
            di[i] = (pathDistance*i)/(numpts-1);
        }

        return;
    }

    double runPointToPoint(const WorldData *wd, const GdalDataDir *srtm1, QPointF transLocLatLon, double transHt, QPointF receiveLocLatLon,
                           double receiveHt, double lineOfSightDistanceKm, double eps_dielect, double sgm_conductivity, double eno_ns_surfref,
                           double frq_mhz, int radio_climate, int pol, double conf, double rel, int numpts, char *prefix){

        double *heights = computeElevationVectorWithSRTM1(wd, srtm1, transLocLatLon, receiveLocLatLon, numpts);

        double rv;
        char buf[256];
        int errnum;

        // Correct the distance for line of sight.
        heights[1] = 1000 * lineOfSightDistanceKm / numpts;
        
        point_to_point(heights, transHt, receiveHt, eps_dielect, sgm_conductivity, eno_ns_surfref, frq_mhz, radio_climate, pol, conf, rel, rv, buf, errnum);
        //qDebug() << " point_to_point" << rv << buf << errnum;

        if(prefix != NULL){
            heights[2] += transHt;
            dumpHeightProfile(prefix, heights);
            int i = (int)heights[0];
            heights[2 + i - 1] += receiveHt;
        }
        
        free(heights);

        return rv;
    }

    /******************************************************************************************/
    /**** Using Building Raster Data                                                       ****/
    /******************************************************************************************/
    double runPointToPointBldg(const WorldData *wd, const GdalDataDir *srtm1, BuildingRasterModel *bldg, GdalDataModel *bldgPoly, QPointF transLocLatLon, double transHt, QPointF receiveLocLatLon,
                           double receiveHt, double lineOfSightDistanceKm, double eps_dielect, double sgm_conductivity, double eno_ns_surfref,
                           double frq_mhz, int radio_climate, int pol, double conf, double rel, int numpts, char *prefix, double **heightProfilePtr
#if MM_DEBUG
                                           , std::vector<std::string> &ITMHeightType
#endif
                              ) {

        if (!(*heightProfilePtr)) {
            *heightProfilePtr = computeElevationVectorWithBldg(wd, srtm1, bldg, bldgPoly, transLocLatLon, receiveLocLatLon, numpts, lineOfSightDistanceKm
#if MM_DEBUG
                                                               , ITMHeightType
#endif
                                                              );
        }
        (*heightProfilePtr)[1] = 1000 * lineOfSightDistanceKm / numpts;

        double rv;
        char buf[256];
        int errnum;

        point_to_point(*heightProfilePtr, transHt, receiveHt, eps_dielect, sgm_conductivity, eno_ns_surfref, frq_mhz, radio_climate, pol, conf, rel, rv, buf, errnum);
        //qDebug() << " point_to_point" << rv << buf << errnum;

        if(prefix != NULL){
            (*heightProfilePtr)[2] += transHt;
            dumpHeightProfile(prefix, *heightProfilePtr);
            int i = (int)(*heightProfilePtr)[0];
            (*heightProfilePtr)[2 + i - 1] += receiveHt;
        }

        return rv;
    }
    /******************************************************************************************/

    /******************************************************************************************/
    /**** Using Building Polygon Data                                                      ****/
    /******************************************************************************************/
    double runPointToPointBldg(const WorldData *wd, const GdalDataDir *srtm1, GdalDataModel *bldg, QPointF transLocLatLon, double transHt, QPointF receiveLocLatLon,
                           double receiveHt, double lineOfSightDistanceKm, double eps_dielect, double sgm_conductivity, double eno_ns_surfref,
                           double frq_mhz, int radio_climate, int pol, double conf, double rel, int numpts, char *prefix, double **heightProfilePtr){

        if (!(*heightProfilePtr)) {
            *heightProfilePtr = computeElevationVectorWithBldg(wd, srtm1, bldg, transLocLatLon, receiveLocLatLon, numpts, lineOfSightDistanceKm);
        }
        (*heightProfilePtr)[1] = 1000 * lineOfSightDistanceKm / numpts;

        double rv;
        char buf[256];
        int errnum;

        point_to_point(*heightProfilePtr, transHt, receiveHt, eps_dielect, sgm_conductivity, eno_ns_surfref, frq_mhz, radio_climate, pol, conf, rel, rv, buf, errnum);
        //qDebug() << " point_to_point" << rv << buf << errnum;

        if(prefix != NULL){
            (*heightProfilePtr)[2] += transHt;
            dumpHeightProfile(prefix, *heightProfilePtr);
            int i = (int)(*heightProfilePtr)[0];
            (*heightProfilePtr)[2 + i - 1] += receiveHt;
        }

        return rv;
    }

    void computeElevationVector(const TerrainClass *terrain, bool includeBldg, const QPointF &from, const QPointF &to, int numpts,
        std::vector<double> &hi, std::vector<double> &di, double ae) {
        // For compatability with P.452
        double tdist;
        QVector<QPointF> latlons = computeApproximateGreatCircleLine(from, to, numpts, &tdist);

        // printf("Returned %d points instead of %d.\n", latlons.count(), numpts);           

        hi.resize(numpts);

        int srtmKey = INT_MAX;
        qint16 *srtmData;
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
        (*heightProfilePtr)[1] = 1000 * lineOfSightDistanceKm / numpts;

        double rv;
        char buf[256];
        int errnum;

        point_to_point(*heightProfilePtr, transHt, receiveHt, eps_dielect, sgm_conductivity, eno_ns_surfref, frq_mhz, radio_climate, pol, conf, rel, rv, buf, errnum);
        //qDebug() << " point_to_point" << rv << buf << errnum;

        if(prefix != NULL){
            (*heightProfilePtr)[2] += transHt;
            dumpHeightProfile(prefix, *heightProfilePtr);
            int i = (int)(*heightProfilePtr)[0];
            (*heightProfilePtr)[2 + i - 1] += receiveHt;
        }

        return rv;
    }

    bool isLOS(const TerrainClass *terrain, QPointF transLocLatLon, double transHt, QPointF receiveLocLatLon,
                           double receiveHt, double lineOfSightDistanceKm, int numpts, double **heightProfilePtr) {
        if (!(*heightProfilePtr)) {
            *heightProfilePtr = computeElevationVector(terrain, true, transLocLatLon, receiveLocLatLon, numpts);
        }
        (*heightProfilePtr)[1] = 1000 * lineOfSightDistanceKm / numpts;

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

