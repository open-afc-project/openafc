/******************************************************************************************/
/**** FILE: RlanRegion.cpp                                                             ****/
/******************************************************************************************/

#include "RlanRegion.h"
#include "terrain.h"
#include "EcefModel.h"
#include "global_defines.h"

/******************************************************************************************/
/**** CONSTRUCTOR: RlanRegionClass::RlanRegionClass()                                  ****/
/******************************************************************************************/
RlanRegionClass::RlanRegionClass()
{
    centerLongitude = quietNaN;
    centerLatitude = quietNaN;
    centerHeightInput = quietNaN;
    centerHeightAMSL = quietNaN;
    centerTerrainHeight = quietNaN;
    minTerrainHeight = quietNaN;
    maxTerrainHeight = quietNaN;
    heightUncertainty = quietNaN;

    fixedHeightAMSL = false;
    configuredFlag = false;
}
/******************************************************************************************/

/******************************************************************************************/
/**** DESTRUCTOR: RlanRegionClass::~RlanRegionClass()                                  ****/
/******************************************************************************************/
RlanRegionClass::~RlanRegionClass()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: RlanRegionClass::getMinHeightAGL()                                     ****/
/******************************************************************************************/
double RlanRegionClass::getMinHeightAGL() const
{
    if (!configuredFlag) {
        throw std::runtime_error(ErrStream() << "ERROR: RlanRegionClass::getMinHeightAGL() RlanRegion not configured");
    }

    double minHeightAGL;
    if (fixedHeightAMSL) {
        minHeightAGL = centerHeightAMSL - heightUncertainty - maxTerrainHeight;
    } else {
        minHeightAGL = centerHeightAMSL - heightUncertainty - centerTerrainHeight;
    }

    return(minHeightAGL);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: RlanRegionClass::getMaxHeightAGL()                                     ****/
/******************************************************************************************/
double RlanRegionClass::getMaxHeightAGL() const
{
    if (!configuredFlag) {
        throw std::runtime_error(ErrStream() << "ERROR: RlanRegionClass::getMaxHeightAGL() RlanRegion not configured");
    }

    double maxHeightAGL;
    if (fixedHeightAMSL) {
        maxHeightAGL = centerHeightAMSL + heightUncertainty - minTerrainHeight;
    } else {
        maxHeightAGL = centerHeightAMSL + heightUncertainty - centerTerrainHeight;
    }

    return(maxHeightAGL);
}
/******************************************************************************************/

/******************************************************************************************/
/**** CONSTRUCTOR: EllipseRlanRegionClass::EllipseRlanRegionClass()                    ****/
/******************************************************************************************/
EllipseRlanRegionClass::EllipseRlanRegionClass(DoubleTriplet rlanLLA, DoubleTriplet rlanUncerts_m,
    double rlanOrientationDeg, bool fixedHeightAMSLVal)
{
    fixedHeightAMSL = fixedHeightAMSLVal;

    std::tie(centerLatitude, centerLongitude, centerHeightInput) = rlanLLA;
    std::tie(semiMinorAxis, semiMajorAxis, heightUncertainty) = rlanUncerts_m;
    orientationDeg = rlanOrientationDeg;

    cosVal = cos(centerLatitude*M_PI/180.0);
    oneOverCosVal = 1.0/cosVal;

    double rlanOrientationRad = rlanOrientationDeg*M_PI/180.0;

    upVec = EcefModel::geodeticToEcef(centerLatitude, centerLongitude, 0.0).normalized();
    eastVec = (Vector3(-upVec.y(), upVec.x(), 0.0)).normalized();
    northVec = upVec.cross(eastVec);

    // define orthogonal unit vectors in the directions of semiMajor and semiMinor axis of ellipse
    majorVec = cos(rlanOrientationRad) * northVec + sin(rlanOrientationRad) * eastVec;
    minorVec = sin(rlanOrientationRad) * northVec - cos(rlanOrientationRad) * eastVec;

    arma::mat mxD(2, 2);
    mxD(0, 0) = semiMinorAxis;
    mxD(1, 1) = semiMajorAxis;
    mxD(0, 1) = 0.0;
    mxD(1, 0) = 0.0;

    arma::mat mxE(2, 2);
    mxE(0, 0) = 1.0/(semiMinorAxis*semiMinorAxis);
    mxE(1, 1) = 1.0/(semiMajorAxis*semiMajorAxis);
    mxE(0, 1) = 0.0;
    mxE(1, 0) = 0.0;

    arma::mat mxR(2, 2);
    mxR(0, 0) =  cos(rlanOrientationRad);
    mxR(0, 1) =  sin(rlanOrientationRad);
    mxR(1, 0) = -sin(rlanOrientationRad);
    mxR(1, 1) =  cos(rlanOrientationRad);

    arma::mat mxS1(2, 2);
    mxS1(0, 0) = CConst::earthRadius*M_PI/180.0;
    mxS1(0, 1) = 0.0;
    mxS1(1, 0) = 0.0;
    mxS1(1, 1) = CConst::earthRadius*M_PI/180.0;

    arma::mat mxS2(2, 2);
    mxS2(0, 0) = cosVal;
    mxS2(0, 1) = 0.0;
    mxS2(1, 0) = 0.0;
    mxS2(1, 1) = 1.0;

    arma::mat mxInvS(2, 2);
    mxInvS(0, 0) = 1.0/(CConst::earthRadius*cosVal*M_PI/180.0);
    mxInvS(0, 1) = 0.0;
    mxInvS(1, 0) = 0.0;
    mxInvS(1, 1) = 1.0/(CConst::earthRadius*M_PI/180.0);

    mxC = mxS1*mxR*mxE*(mxR.t())*mxS1;
    mxA = mxS2*mxC*mxS2;
    mxB = mxInvS*mxR*mxD;
}
/******************************************************************************************/

/******************************************************************************************/
/**** DESTRUCTOR: EllipseRlanRegionClass::~EllipseRlanRegionClass()                    ****/
/******************************************************************************************/
EllipseRlanRegionClass::~EllipseRlanRegionClass()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** CONSTRUCTOR: EllipseRlanRegionClass::configure()                                 ****/
/******************************************************************************************/
void EllipseRlanRegionClass::configure(CConst::HeightTypeEnum rlanHeightType, TerrainClass *terrain)
{
    double bldgHeight;
    MultibandRasterClass::HeightResult lidarHeightResult;
    CConst::HeightSourceEnum rlanHeightSource;
    terrain->getTerrainHeight(centerLongitude, centerLatitude, centerTerrainHeight, bldgHeight, lidarHeightResult, rlanHeightSource);

    // LOGGER_DEBUG(logger) << "rlanHeight: " << centerTerrainHeight << ", building height: " << bldgHeight << ", from: " << rlanHeightSource;

    if (rlanHeightType == CConst::AMSLHeightType) {
        centerHeightAMSL = centerHeightInput;
    } else if (rlanHeightType == CConst::AGLHeightType) {
        centerHeightAMSL = centerHeightInput + centerTerrainHeight;
    } else {
        throw std::runtime_error(ErrStream() << "ERROR: INVALID rlanHeightType = " << rlanHeightType);
    }

    centerPosn = EcefModel::geodeticToEcef(centerLatitude, centerLongitude, centerHeightAMSL / 1000.0);

    minTerrainHeight = centerTerrainHeight;
    maxTerrainHeight = centerTerrainHeight;

    int scanPtIdx;
    std::vector<LatLon> scanPtList = getScan(CConst::xyAlignRegionNorthEastScanRegionMethod, 1.0, -1);
    for(scanPtIdx=0; scanPtIdx<(int) scanPtList.size(); ++scanPtIdx) {
        LatLon scanPt = scanPtList[scanPtIdx];
        double terrainHeight;
        terrain->getTerrainHeight(scanPt.second, scanPt.first, terrainHeight, bldgHeight, lidarHeightResult, rlanHeightSource);

        if (terrainHeight > maxTerrainHeight) {
            maxTerrainHeight = terrainHeight;
        } else if (terrainHeight < minTerrainHeight) {
            minTerrainHeight = terrainHeight;
        }
    }

    configuredFlag = true;
}
/******************************************************************************************/

/******************************************************************************************/
/**** CONSTRUCTOR: EllipseRlanRegionClass::calcMinAOB()                                ****/
/******************************************************************************************/
double EllipseRlanRegionClass::calcMinAOB(LatLon ulsRxLatLon, Vector3 ulsAntennaPointing, double ulsRxHeightAMSL)
{
	double minAOB;

	arma::vec ptg(3);
	ptg(0) = ulsAntennaPointing.dot(eastVec);
	ptg(1) = ulsAntennaPointing.dot(northVec);
	ptg(2) = ulsAntennaPointing.dot(upVec);

	arma::vec P(3);
	P(0) = (ulsRxLatLon.second - centerLongitude)*cosVal;
	P(1) = ulsRxLatLon.first  - centerLatitude;
	if (ulsRxHeightAMSL > centerHeightAMSL) {
		P(2) = ulsRxHeightAMSL - (centerHeightAMSL + heightUncertainty);
	} else {
		P(2) = ulsRxHeightAMSL - (centerHeightAMSL - heightUncertainty);
	}
	P(2) *= (180.0/M_PI)/CConst::earthRadius;

	bool found = false;
    if (    ((ptg(2) < 0.0) && (P(2) > 0.0)) 
         || ((ptg(2) > 0.0) && (P(2) < 0.0)) ) {
        double dist = -P(2) / ptg(2);
	    arma::vec intcpt(2);
        intcpt(0) = (P(0) + dist*ptg(0))/cosVal;
        intcpt(1) = P(1) + dist*ptg(1);

		double d = dot(intcpt, mxA*intcpt);
		if (d<=1) {
			minAOB = 0.0;
			found = true;
		}
	}

	if (!found) {
		arma::vec F(2);
		arma::vec U(2);
		F(0) = P(0);
		F(1) = P(1);
		U(0) = ptg(0);
		U(1) = ptg(1);

		double a = dot(U, mxC*U);
		double b = dot(U, (mxC+mxC.t())*F);
		double c = dot(F, mxC*F) - 1.0;

		double eps = (-b + sqrt(b*b - 4*a*c))/(2*a);

		arma::vec E(3);
		E(0) = F(0) + eps*U(0);
		E(1) = F(1) + eps*U(1);
		E(2) = 0.0;

		arma::vec L = E - P;
		L = (1.0/norm(L))*L;

		double cosAOB = dot(L, ptg);
		minAOB = acos(cosAOB)*180.0/M_PI;
	}

	return minAOB;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: EllipseRlanRegionClass::getType()                                      ****/
/******************************************************************************************/
RLANBoundary EllipseRlanRegionClass::getType() const
{
    return ELLIPSE;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: EllipseRlanRegionClass::closestPoint()                                 ****/
/******************************************************************************************/
LatLon EllipseRlanRegionClass::closestPoint(LatLon latlon, bool& contains) const
{
    double latitude  = 0.0;
    double longitude = 0.0;

    arma::vec P(2);
    P(0) = latlon.second - centerLongitude; // longitude
    P(1) = latlon.first  - centerLatitude;  // latitude

    double d = dot(P, mxA*P);

    if (d<=1) {
        contains = true;
    } else {
        contains = false;
        longitude = centerLongitude + P(0)/sqrt(d);
        latitude  = centerLatitude  + P(1)/sqrt(d);
    }

    return(std::pair<double, double>(latitude, longitude));
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: EllipseRlanRegionClass::getBoundary()                                  ****/
/******************************************************************************************/
std::vector<GeodeticCoord> EllipseRlanRegionClass::getBoundary(TerrainClass *terrain) const
{
    std::vector<GeodeticCoord> ptList;
    arma::vec P(2);

    if (!configuredFlag) {
        throw std::runtime_error(ErrStream() << "ERROR: EllipseRlanRegionClass::getBoundary() RlanRegion not configured");
    }

    int ptIdx;
    int numRLANPoints = 32;

    for(ptIdx=0; ptIdx<numRLANPoints; ptIdx++) {
        double phi = 2*M_PI*ptIdx/numRLANPoints;
        P(0) = cos(phi);
        P(1) = sin(phi);
        double d = dot(P, mxA*P);
        double longitude = centerLongitude + P(0)/sqrt(d);
        double latitude  = centerLatitude  + P(1)/sqrt(d);

        double heightAMSL;
        if (fixedHeightAMSL) {
            heightAMSL = centerHeightAMSL;
        } else {
            double terrainHeight, bldgHeight;
            MultibandRasterClass::HeightResult lidarHeightResult;
            CConst::HeightSourceEnum rlanHeightSource;
            terrain->getTerrainHeight(longitude, latitude, terrainHeight, bldgHeight, lidarHeightResult, rlanHeightSource);
            heightAMSL = terrainHeight + centerHeightAMSL - centerTerrainHeight;
        }
        GeodeticCoord rlanEllipsePtGeo = GeodeticCoord::fromLatLon(latitude, longitude, heightAMSL/1000.0);
        ptList.push_back(rlanEllipsePtGeo);
    }

    return(ptList);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: EllipseRlanRegionClass::getScan()                                      ****/
/******************************************************************************************/
std::vector<LatLon> EllipseRlanRegionClass::getScan(CConst::ScanRegionMethodEnum method, double scanResolutionM, int pointsPerDegree) const
{
    std::vector<LatLon> ptList;

    if (method == CConst::xyAlignRegionNorthEastScanRegionMethod) {
        // Scan points aligned with north/east
        int N = floor(semiMajorAxis / scanResolutionM);

        double deltaLat = (scanResolutionM / CConst::earthRadius)*(180.0/M_PI);
        double deltaLon = deltaLat / cosVal;

        int ix, iy;
        for(iy=-N; iy<=N; ++iy) {
            double latitude  = centerLatitude  + iy*deltaLat;
            for(ix=-N; ix<=N; ++ix) {
                double longitude  = centerLongitude  + ix*deltaLon;
                bool contains;
                closestPoint(std::pair<double, double>(latitude, longitude), contains);
                if (contains) {
                    ptList.push_back(std::pair<double, double>(latitude, longitude));
                }
            }
        }
    } else if (method == CConst::xyAlignRegionMajorMinorScanRegionMethod) {
        // Scan points aligned with major/minor axis

        double dx = scanResolutionM / semiMinorAxis;
        double dy = scanResolutionM / semiMajorAxis;

        arma::vec X0(2), X3(2);

        int Ny = floor(1.0 / dy);

        int ix, iy, xs, ys;
        for(iy=0; iy<=Ny; ++iy) {
            double yval = iy*dy;
            int Nx = (int) floor(sqrt(1.0 - yval*yval) / dx);
            for(ix=0; ix<=Nx; ++ix) {
                double xval = ix*dx;
                X0(0) = xval;
                X0(1) = yval;
                X3 = mxB*X0;

                for(ys=0; ys<(iy==0?1:2); ++ys) {
                    for(xs=0; xs<(ix==0?1:2); ++xs) {
                        double longitude = centerLongitude + (xs==0 ? 1 : -1) * X3(0);
                        double latitude  = centerLatitude  + (ys==0 ? 1 : -1) * X3(1);
                        ptList.push_back(std::pair<double, double>(latitude, longitude));
                    }
                }
            }
        }
    } else if (method == CConst::latLonAlignGridScanRegionMethod) {
        // Scan points aligned with lat / lon grid
        int N = floor( (semiMajorAxis / CConst::earthRadius)*(180.0/M_PI)*pointsPerDegree/cosVal ) + 1;
        int S[2*N+1][2*N+1];

        int ix, iy;
        for(ix=0; ix<=2*N; ++ix) {
            for(iy=0; iy<=2*N; ++iy) {
                S[ix][iy] = 0;
            }
        }
        S[N][N] = 1;

        int latN0 = (int) floor(centerLatitude*pointsPerDegree);
        int lonN0 = (int) floor(centerLongitude*pointsPerDegree);

        for(iy=-N+1; iy<=N; ++iy) {
            double latVal = ((double) (latN0 + iy))/pointsPerDegree;
            bool flag;
            double lonA, lonB;
            calcHorizExtents(latVal, lonA, lonB, flag);
            if (flag) {
                int iA = ((int) floor(lonA*pointsPerDegree)) - lonN0;
                int iB = ((int) floor(lonB*pointsPerDegree)) - lonN0;
                for(ix=iA; ix<=iB; ++ix) {
                    S[N+ix][N+iy] = 1;
                    S[N+ix][N+iy-1] = 1;
                }
            }
        }

        for(ix=-N+1; ix<=N; ++ix) {
            double lonVal = ((double) (lonN0 + ix))/pointsPerDegree;
            bool flag;
            double latA, latB;
            calcVertExtents(lonVal, latA, latB, flag);
            if (flag) {
                int iA = ((int) floor(latA*pointsPerDegree)) - latN0;
                int iB = ((int) floor(latB*pointsPerDegree)) - latN0;
                for(iy=iA; iy<=iB; ++iy) {
                    S[N+ix][N+iy] = 1;
                    S[N+ix-1][N+iy] = 1;
                }
            }
        }

        for(iy=2*N; iy>=0; --iy) {
            for(ix=0; ix<=2*N; ++ix) {
                if (S[ix][iy]) {
                    double lonVal = (lonN0 + ix - N + 0.5)/pointsPerDegree; 
                    double latVal = (latN0 + iy - N + 0.5)/pointsPerDegree; 
                    ptList.push_back(std::pair<double, double>(latVal, lonVal));
                }
            }
        }
    } else {
        CORE_DUMP;
    }

    return(ptList);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: EllipseRlanRegionClass::calcHorizExtents()                             ****/
/******************************************************************************************/
void EllipseRlanRegionClass::calcHorizExtents(double latVal, double& lonA, double& lonB, bool& flag) const
{
    double yval = latVal - centerLatitude;

    double B = (mxA(0,1) + mxA(1,0))*yval/mxA(0,0);
    double C = (mxA(1,1)*yval*yval - 1.0)/mxA(0,0);

    double D = B*B - 4*C;

    if (D >= 0) {
        flag = true;
        double sqrtD = sqrt(D);
        lonA = centerLongitude + (-B - sqrtD)/2;
        lonB = centerLongitude + (-B + sqrtD)/2;
    } else {
        flag = false;
    }

    return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: EllipseRlanRegionClass::calcVertExtents()                              ****/
/******************************************************************************************/
void EllipseRlanRegionClass::calcVertExtents(double lonVal, double& latA, double& latB, bool& flag) const
{
    double xval = lonVal - centerLongitude;

    double B = (mxA(0,1) + mxA(1,0))*xval/mxA(1,1);
    double C = (mxA(0,0)*xval*xval - 1.0)/mxA(1,1);

    double D = B*B - 4*C;

    if (D >= 0) {
        flag = true;
        double sqrtD = sqrt(D);
        latA = centerLatitude + (-B - sqrtD)/2;
        latB = centerLatitude + (-B + sqrtD)/2;
    } else {
        flag = false;
    }

    return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: EllipseRlanRegionClass::getMaxDist()                                   ****/
/******************************************************************************************/
double EllipseRlanRegionClass::getMaxDist() const
{
    return(semiMajorAxis);
}
/******************************************************************************************/

/******************************************************************************************/
/**** CONSTRUCTOR: PolygonRlanRegionClass::PolygonRlanRegionClass()                    ****/
/******************************************************************************************/
PolygonRlanRegionClass::PolygonRlanRegionClass(DoubleTriplet rlanLLA, DoubleTriplet rlanUncerts_m,
    const std::vector<std::pair<double, double>>& rlanPolygon, RLANBoundary polygonTypeVal, bool fixedHeightAMSLVal) :
polygonType(polygonTypeVal)
{
    fixedHeightAMSL = fixedHeightAMSLVal;

    std::tie(centerLatitude, centerLongitude, centerHeightInput) = rlanLLA;
    std::tie(std::ignore, std::ignore, heightUncertainty) = rlanUncerts_m;

    Vector3 centerPosnNoHeight = EcefModel::geodeticToEcef(centerLatitude, centerLongitude, 0.0);

    upVec = centerPosnNoHeight.normalized();
    eastVec = (Vector3(-upVec.y(), upVec.x(), 0.0)).normalized();
    northVec = upVec.cross(eastVec);

    resolution = 1.0e-6; // 0.11 meter
    centerLongitude = floor((centerLongitude / resolution) + 0.5)*resolution;
    centerLatitude  = floor((centerLatitude / resolution) + 0.5)*resolution;
    cosVal = cos(centerLatitude*M_PI/180.0);
    oneOverCosVal = 1.0/cosVal;

    std::vector<std::tuple<int, int>> *ii_list = new std::vector<std::tuple<int, int>>();
    for(int i=0; i<(int) rlanPolygon.size(); ++i) {
        double longitude, latitude;

        if (polygonType == LINEAR_POLY) {
            latitude = rlanPolygon[i].first;
            longitude = rlanPolygon[i].second;
        } else if (polygonType == RADIAL_POLY) {
            double angle = rlanPolygon[i].first;
            double length = rlanPolygon[i].second;

            Vector3 position = centerPosnNoHeight + (length/1000.0)*(northVec*cos(angle*M_PI/180.0) + eastVec*sin(angle*M_PI/180.0));
            GeodeticCoord positionGeo = EcefModel::toGeodetic(position);

            longitude = positionGeo.longitudeDeg;
            latitude  = positionGeo.latitudeDeg;
        } else {
            throw std::runtime_error(ErrStream() << "ERROR: INVALID polygonType = " << polygonType);
        }

        int xval = (int) floor(((longitude - centerLongitude)*cosVal/resolution) + 0.5);
        int yval = (int) floor(((latitude  - centerLatitude)/resolution) + 0.5);
        ii_list->push_back(std::tuple<int, int>(xval, yval));
    }

    polygon = new PolygonClass(ii_list);

    delete ii_list;
}
/******************************************************************************************/

/******************************************************************************************/
/**** DESTRUCTOR: PolygonRlanRegionClass::~PolygonRlanRegionClass()                    ****/
/******************************************************************************************/
PolygonRlanRegionClass::~PolygonRlanRegionClass()
{
    delete polygon;
}
/******************************************************************************************/

/******************************************************************************************/
/**** CONSTRUCTOR: PolygonRlanRegionClass::configure()                                 ****/
/******************************************************************************************/
void PolygonRlanRegionClass::configure(CConst::HeightTypeEnum rlanHeightType, TerrainClass *terrain)
{
    double bldgHeight;
    MultibandRasterClass::HeightResult lidarHeightResult;
    CConst::HeightSourceEnum rlanHeightSource;
    terrain->getTerrainHeight(centerLongitude, centerLatitude, centerTerrainHeight, bldgHeight, lidarHeightResult, rlanHeightSource);

    // LOGGER_DEBUG(logger) << "rlanHeight: " << centerTerrainHeight << ", building height: " << bldgHeight << ", from: " << rlanHeightSource;

    if (rlanHeightType == CConst::AMSLHeightType) {
        centerHeightAMSL = centerHeightInput;
    } else if (rlanHeightType == CConst::AGLHeightType) {
        centerHeightAMSL = centerHeightInput + centerTerrainHeight;
    } else {
        throw std::runtime_error(ErrStream() << "ERROR: INVALID rlanHeightType = " << rlanHeightType);
    }

    centerPosn = EcefModel::geodeticToEcef(centerLatitude, centerLongitude, centerHeightAMSL / 1000.0);

    minTerrainHeight = centerTerrainHeight;
    maxTerrainHeight = centerTerrainHeight;

    int scanPtIdx;
    std::vector<LatLon> scanPtList = getScan(CConst::xyAlignRegionNorthEastScanRegionMethod, 1.0, -1);
    for(scanPtIdx=0; scanPtIdx<(int) scanPtList.size(); ++scanPtIdx) {
        LatLon scanPt = scanPtList[scanPtIdx];
        double terrainHeight;
        terrain->getTerrainHeight(scanPt.second, scanPt.first, terrainHeight, bldgHeight, lidarHeightResult, rlanHeightSource);

        if (terrainHeight > maxTerrainHeight) {
            maxTerrainHeight = terrainHeight;
        } else if (terrainHeight < minTerrainHeight) {
            minTerrainHeight = terrainHeight;
        }
    }

    configuredFlag = true;
}
/******************************************************************************************/

/******************************************************************************************/
/**** CONSTRUCTOR: PolygonRlanRegionClass::calcMinAOB()                                ****/
/******************************************************************************************/
double PolygonRlanRegionClass::calcMinAOB(LatLon ulsRxLatLon, Vector3 ulsAntennaPointing, double ulsRxHeightAMSL)
{
	double minAOB;

	arma::vec ptg(3);
	ptg(0) = ulsAntennaPointing.dot(eastVec);
	ptg(1) = ulsAntennaPointing.dot(northVec);
	ptg(2) = ulsAntennaPointing.dot(upVec);

	arma::vec F(3);
	F(0) = (ulsRxLatLon.second - centerLongitude)*cosVal;
	F(1) = ulsRxLatLon.first  - centerLatitude;
	if (ulsRxHeightAMSL > centerHeightAMSL) {
		F(2) = ulsRxHeightAMSL - (centerHeightAMSL + heightUncertainty);
	} else {
		F(2) = ulsRxHeightAMSL - (centerHeightAMSL - heightUncertainty);
	}
	F(2) *= (180.0/M_PI)/CConst::earthRadius;

	bool found = false;
    if (    ((ptg(2) < 0.0) && (F(2) > 0.0)) 
         || ((ptg(2) > 0.0) && (F(2) < 0.0)) ) {
        double dist = -F(2) / ptg(2);
	    arma::vec intcpt(2);
        int xval = (int) floor((F(0) + dist*ptg(0))/resolution + 0.5);
        int yval = (int) floor((F(1) + dist*ptg(1))/resolution + 0.5);

		bool edge;
		bool contains = polygon->in_bdy_area(xval, yval, &edge);

		if (contains || edge) {
			minAOB = 0.0;
			found = true;
		}
	}

	if (!found) {
        double maxCosAOB = -1.0;
        for(int segIdx=0; segIdx<polygon->num_segment; ++segIdx) {
            int prevIdx = polygon->num_bdy_pt[segIdx]-1;
	        arma::vec prevVirtex(3);
            prevVirtex(0) = polygon->bdy_pt_x[segIdx][prevIdx]*resolution;
            prevVirtex(1) = polygon->bdy_pt_y[segIdx][prevIdx]*resolution;
            prevVirtex(2) = 0.0;
            for(int ptIdx=0; ptIdx<polygon->num_bdy_pt[segIdx]; ++ptIdx) {
	            arma::vec virtex(3);
                virtex(0) = polygon->bdy_pt_x[segIdx][ptIdx]*resolution;
                virtex(1) = polygon->bdy_pt_y[segIdx][ptIdx]*resolution;
                virtex(2) = 0.0;

                double D0 = dot(virtex - prevVirtex, virtex - prevVirtex);
                double D1 = 2*dot(virtex - prevVirtex, prevVirtex - F);
                double D2 = dot(prevVirtex - F, prevVirtex - F);

                double C0 = dot(prevVirtex - F, ptg);
                double C1 = dot(virtex - prevVirtex, ptg);

                double eps = (D0*C1 - C0*D1/2)/(D2*C0 - C1*D1/2);

                double cosAOB = C0 / sqrt(D0);
                if (cosAOB > maxCosAOB) {
                    maxCosAOB = cosAOB;
                }

                if ((eps > 0.0) && (eps < 1.0)) {
                    cosAOB = (C0 + C1*eps)/ sqrt(D0 + eps*(D1 + eps*D2));
                    if (cosAOB > maxCosAOB) {
                        maxCosAOB = cosAOB;
                    }
                }

                prevIdx = ptIdx;
                prevVirtex = virtex;
            }
        }

		minAOB = acos(maxCosAOB)*180.0/M_PI;
	}

    return minAOB;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PolygonRlanRegionClass::getType()                                      ****/
/******************************************************************************************/
RLANBoundary PolygonRlanRegionClass::getType() const
{
    return polygonType; 
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PolygonRlanRegionClass::closestPoint()                                 ****/
/******************************************************************************************/
LatLon PolygonRlanRegionClass::closestPoint(LatLon latlon, bool& contains) const
{
    double latitude  = 0.0;
    double longitude = 0.0;

    int xval, yval;
    xval = (int) floor((latlon.second - centerLongitude)*cosVal/resolution + 0.5); // longitude
    yval = (int) floor((latlon.first  - centerLatitude )/resolution + 0.5); // latitude

    bool edge;
    contains = polygon->in_bdy_area(xval, yval, &edge);
    if (edge) {
        contains = true;
    }

    if (!contains) {
        double ptX, ptY;
        std::tie(ptX, ptY) =  polygon->closestPoint(std::tuple<int, int>(xval, yval));

        longitude = centerLongitude + ptX*resolution*oneOverCosVal;
        latitude  = centerLatitude  + ptY*resolution;
    }

    return(std::pair<double, double>(latitude, longitude));
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PolygonRlanRegionClass::getBoundary()                                  ****/
/******************************************************************************************/
std::vector<GeodeticCoord> PolygonRlanRegionClass::getBoundary(TerrainClass *terrain) const
{
    std::vector<GeodeticCoord> ptList;

    if (!configuredFlag) {
        throw std::runtime_error(ErrStream() << "ERROR: PolygonRlanRegionClass::getBoundary() RlanRegion not configured");
    }

    int ptIdx;
    int numRLANPoints = polygon->num_bdy_pt[0];

    for(ptIdx=0; ptIdx<numRLANPoints; ptIdx++) {
        int xval = polygon->bdy_pt_x[0][ptIdx];
        int yval = polygon->bdy_pt_y[0][ptIdx];
        double longitude = centerLongitude + xval*resolution*oneOverCosVal;
        double latitude  = centerLatitude  + yval*resolution;

        double heightAMSL;
        if (fixedHeightAMSL) {
            heightAMSL = centerHeightAMSL;
        } else {
            double terrainHeight, bldgHeight;
            MultibandRasterClass::HeightResult lidarHeightResult;
            CConst::HeightSourceEnum rlanHeightSource;
            terrain->getTerrainHeight(longitude, latitude, terrainHeight, bldgHeight, lidarHeightResult, rlanHeightSource);
            heightAMSL = terrainHeight + centerHeightAMSL - centerTerrainHeight;
        }
        GeodeticCoord rlanEllipsePtGeo = GeodeticCoord::fromLatLon(latitude, longitude, heightAMSL/1000.0);
        ptList.push_back(rlanEllipsePtGeo);
    }

    return(ptList);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PolygonRlanRegionClass::getScan()                                      ****/
/******************************************************************************************/
std::vector<LatLon> PolygonRlanRegionClass::getScan(CConst::ScanRegionMethodEnum method, double scanResolutionM, int pointsPerDegree) const
{
    std::vector<LatLon> ptList;

    int minx, maxx, miny, maxy;
    polygon->comp_bdy_min_max(minx, maxx, miny, maxy);

    if (    (method == CConst::xyAlignRegionNorthEastScanRegionMethod)
         || (method == CConst::xyAlignRegionMajorMinorScanRegionMethod) ) {
        int minScanXIdx = (int) floor(minx*resolution*(M_PI/180.0)*CConst::earthRadius / scanResolutionM);
        int maxScanXIdx = (int) floor(maxx*resolution*(M_PI/180.0)*CConst::earthRadius / scanResolutionM)+1;
        int minScanYIdx = (int) floor(miny*resolution*(M_PI/180.0)*CConst::earthRadius / scanResolutionM);
        int maxScanYIdx = (int) floor(maxy*resolution*(M_PI/180.0)*CConst::earthRadius / scanResolutionM)+1;

        int ix, iy;
        bool isEdge;
        for(iy=minScanYIdx; iy<=maxScanYIdx; ++iy) {
            int yIdx = (int) floor(iy*scanResolutionM*(180.0/M_PI)/(CConst::earthRadius*resolution) + 0.5);
            for(ix=minScanXIdx; ix<=maxScanXIdx; ++ix) {
                int xIdx = (int) floor(ix*scanResolutionM*(180.0/M_PI)/(CConst::earthRadius*resolution) + 0.5);
                bool inBdyArea = polygon->in_bdy_area(xIdx, yIdx, &isEdge);
                if (inBdyArea || isEdge) {
                    double longitude = centerLongitude + xIdx*resolution*oneOverCosVal;
                    double latitude  = centerLatitude  + yIdx*resolution;
                    ptList.push_back(std::pair<double, double>(latitude, longitude));
                }
            }
        }
    } else if (method == CConst::latLonAlignGridScanRegionMethod) {
        // Scan points aligned with lat / lon grid
        int NX = (int) floor( (maxx - minx)*resolution*oneOverCosVal*pointsPerDegree ) + 2;
        int NY = (int) floor( (maxy - miny)*resolution*pointsPerDegree ) + 2;

        int S[NX][NY];

        // int xval = polygon->bdy_pt_x[0][ptIdx];
        // int yval = polygon->bdy_pt_y[0][ptIdx];
        // double longitude = centerLongitude + xval*resolution*oneOverCosVal;
        // double latitude  = centerLatitude  + yval*resolution;

        int ix, iy;
        for(ix=0; ix<NX; ++ix) {
            for(iy=0; iy<NY; ++iy) {
                S[ix][iy] = 0;
            }
        }

        int latN0 = (int) floor((centerLatitude + miny*resolution)*pointsPerDegree);
        int lonN0 = (int) floor((centerLongitude + minx*resolution*oneOverCosVal)*pointsPerDegree);

        for(iy=1; iy<NY; ++iy) {
            double latVal = ((double) (latN0 + iy))/pointsPerDegree;
            double yVal = (latVal - centerLatitude)/resolution;
            bool flag;
            double xA, xB;
            polygon->calcHorizExtents(yVal, xA, xB, flag);
            if (flag) {
                double lonA = centerLongitude + xA*resolution*oneOverCosVal;
                double lonB = centerLongitude + xB*resolution*oneOverCosVal;
                int iA = ((int) floor(lonA*pointsPerDegree)) - lonN0;
                int iB = ((int) floor(lonB*pointsPerDegree)) - lonN0;
                for(ix=iA; ix<=iB; ++ix) {
                    S[ix][iy] = 1;
                    S[ix][iy-1] = 1;
                }
            }
        }

        for(ix=1; ix<NX; ++ix) {
            double lonVal = ((double) (lonN0 + ix))/pointsPerDegree;
            double xVal = (lonVal - centerLongitude)/resolution;
            bool flag;
            double yA, yB;
            polygon->calcVertExtents(xVal, yA, yB, flag);
            if (flag) {
                double latA = centerLatitude + yA*resolution;
                double latB = centerLatitude + yB*resolution;
                int iA = ((int) floor(latA*pointsPerDegree)) - latN0;
                int iB = ((int) floor(latB*pointsPerDegree)) - latN0;
                for(iy=iA; iy<=iB; ++iy) {
                    S[ix][iy] = 1;
                    S[ix-1][iy] = 1;
                }
            }
        }

        for(iy=NY-1; iy>=0; --iy) {
            for(ix=0; ix<NX; ++ix) {
                if (S[ix][iy]) {
                    double lonVal = (lonN0 + ix + 0.5)/pointsPerDegree; 
                    double latVal = (latN0 + iy + 0.5)/pointsPerDegree; 
                    ptList.push_back(std::pair<double, double>(latVal, lonVal));
                }
            }
        }
    } else {
        CORE_DUMP;
    }

    return(ptList);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PolygonRlanRegionClass::getMaxDist()                                   ****/
/******************************************************************************************/
double PolygonRlanRegionClass::getMaxDist() const
{
    int ptIdx;
    double dist;
    double maxDist = 0.0;
    int numRLANPoints = polygon->num_bdy_pt[0];

    for(ptIdx=0; ptIdx<numRLANPoints; ptIdx++) {
        int xval = polygon->bdy_pt_x[0][ptIdx];
        int yval = polygon->bdy_pt_y[0][ptIdx];
        dist = sqrt(((double) xval)*xval + ((double) yval)*yval)*(resolution*M_PI/180.0)*CConst::earthRadius;
        if (dist > maxDist) {
            maxDist = dist;
        }
    }
    return(maxDist);
}
/******************************************************************************************/

