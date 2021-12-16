/******************************************************************************************/
/**** FILE: RlanRegion.cpp                                                             ****/
/******************************************************************************************/

#include "RlanRegion.h"
#include "terrain.h"
#include "EcefModel.h"

/******************************************************************************************/
/**** CONSTRUCTOR: RlanRegionClass::RlanRegionClass()                                  ****/
/******************************************************************************************/
RlanRegionClass::RlanRegionClass()
{
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

    arma::mat mxS(2, 2);
    mxS(0, 0) = CConst::earthRadius*cos(centerLatitude*M_PI/180.0)*M_PI/180.0;
    mxS(0, 1) = 0.0;
    mxS(1, 0) = 0.0;
    mxS(1, 1) = CConst::earthRadius*M_PI/180.0;

    arma::mat mxInvS(2, 2);
    mxInvS(0, 0) = 1.0/(CConst::earthRadius*cos(centerLatitude*M_PI/180.0)*M_PI/180.0);
    mxInvS(0, 1) = 0.0;
    mxInvS(1, 0) = 0.0;
    mxInvS(1, 1) = 1.0/(CConst::earthRadius*M_PI/180.0);

    mxA = mxS*mxR*mxE*(mxR.t())*mxS;
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
void EllipseRlanRegionClass::configure(std::string rlanHeightType, TerrainClass *terrain)
{
    double bldgHeight;
    MultibandRasterClass::HeightResult lidarHeightResult;
    CConst::HeightSourceEnum rlanHeightSource;
    terrain->getTerrainHeight(centerLongitude, centerLatitude, centerTerrainHeight, bldgHeight, lidarHeightResult, rlanHeightSource);

    // LOGGER_DEBUG(logger) << "rlanHeight: " << centerTerrainHeight << ", building height: " << bldgHeight << ", from: " << rlanHeightSource;

    if (rlanHeightType == "AMSL") {
        centerHeightAMSL = centerHeightInput;
    } else if (rlanHeightType == "AGL") {
        centerHeightAMSL = centerHeightInput + centerTerrainHeight;
    } else {
        throw std::runtime_error(ErrStream() << "ERROR: INVALID rlanHeightType = " << rlanHeightType);
    }

    centerPosn = EcefModel::geodeticToEcef(centerLatitude, centerLongitude, centerHeightAMSL / 1000.0);

    minTerrainHeight = centerTerrainHeight;
    maxTerrainHeight = centerTerrainHeight;

    int scanPtIdx;
    std::vector<LatLon> scanPtList = getScan(1.0);
    for(scanPtIdx=0; scanPtIdx<scanPtList.size(); ++scanPtIdx) {
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
std::vector<LatLon> EllipseRlanRegionClass::getScan(double scanResolutionM) const
{
    std::vector<LatLon> ptList;

#if 1
    // Scan points aligned with north/east
    int N = floor(semiMajorAxis / scanResolutionM);

    double deltaLat = (scanResolutionM / CConst::earthRadius)*(180.0/M_PI);
    double deltaLon = deltaLat / cos(centerLatitude*M_PI/180.0);

    int ix, iy, xs, ys;
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
#else
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
#endif

    return(ptList);
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
}
/******************************************************************************************/

/******************************************************************************************/
/**** CONSTRUCTOR: PolygonRlanRegionClass::configure()                                 ****/
/******************************************************************************************/
void PolygonRlanRegionClass::configure(std::string rlanHeightType, TerrainClass *terrain)
{
    double bldgHeight;
    MultibandRasterClass::HeightResult lidarHeightResult;
    CConst::HeightSourceEnum rlanHeightSource;
    terrain->getTerrainHeight(centerLongitude, centerLatitude, centerTerrainHeight, bldgHeight, lidarHeightResult, rlanHeightSource);

    // LOGGER_DEBUG(logger) << "rlanHeight: " << centerTerrainHeight << ", building height: " << bldgHeight << ", from: " << rlanHeightSource;

    if (rlanHeightType == "AMSL") {
        centerHeightAMSL = centerHeightInput;
    } else if (rlanHeightType == "AGL") {
        centerHeightAMSL = centerHeightInput + centerTerrainHeight;
    } else {
        throw std::runtime_error(ErrStream() << "ERROR: INVALID rlanHeightType = " << rlanHeightType);
    }

    centerPosn = EcefModel::geodeticToEcef(centerLatitude, centerLongitude, centerHeightAMSL / 1000.0);

    minTerrainHeight = centerTerrainHeight;
    maxTerrainHeight = centerTerrainHeight;

    int scanPtIdx;
    std::vector<LatLon> scanPtList = getScan(1.0);
    for(scanPtIdx=0; scanPtIdx<scanPtList.size(); ++scanPtIdx) {
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
std::vector<LatLon> PolygonRlanRegionClass::getScan(double scanResolutionM) const
{
    std::vector<LatLon> ptList;

    int minx, maxx, miny, maxy;
    polygon->comp_bdy_min_max(minx, maxx, miny, maxy);

    int minScanXIdx = (int) floor(minx*resolution*(M_PI/180.0)*CConst::earthRadius / scanResolutionM);
    int maxScanXIdx = (int) floor(maxx*resolution*(M_PI/180.0)*CConst::earthRadius / scanResolutionM)+1;
    int minScanYIdx = (int) floor(miny*resolution*(M_PI/180.0)*CConst::earthRadius / scanResolutionM);
    int maxScanYIdx = (int) floor(maxy*resolution*(M_PI/180.0)*CConst::earthRadius / scanResolutionM)+1;

    int ix, iy, xs, ys;
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

