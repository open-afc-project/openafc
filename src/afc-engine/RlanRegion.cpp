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
/**** CONSTRUCTOR: EllipseRlanRegionClass::EllipseRlanRegionClass()                    ****/
/******************************************************************************************/
EllipseRlanRegionClass::EllipseRlanRegionClass(DoubleTriplet rlanLLA, DoubleTriplet rlanUncerts_m,
    double rlanOrientationDeg, std::string rlanHeightType, TerrainClass *terrain)
{
    std::tie(centerLatitude, centerLongitude, centerHeightInput) = rlanLLA;
    std::tie(semiMinorAxis, semiMajorAxis, heightUncertainty) = rlanUncerts_m;
    orientationDeg = rlanOrientationDeg;

    double bldgHeight;
    MultibandRasterClass::HeightResult lidarHeightResult;
    CConst::HeightSourceEnum rlanHeightSource;
    terrain->getTerrainHeight(centerLongitude, centerLatitude, centerTerrainHeight, bldgHeight, lidarHeightResult, rlanHeightSource);

    // LOGGER_DEBUG(logger) << "rlanHeight: " << centerTerrainHeight << ", building height: " << bldgHeight << ", from: " << rlanHeightSource;

    if (rlanHeightType == "AMSL") {
        centerHeight = centerHeightInput;
    } else if (rlanHeightType == "AGL") {
        centerHeight = centerHeightInput + centerTerrainHeight;
    } else {
        throw std::runtime_error(ErrStream() << "ERROR: INVALID rlanHeightType = " << rlanHeightType);
    }

    double rlanOrientationRad = rlanOrientationDeg*M_PI/180.0;

    centerPosn = EcefModel::geodeticToEcef(centerLatitude, centerLongitude, centerHeight / 1000.0);

    upVec = centerPosn.normalized();
    eastVec = (Vector3(-upVec.y(), upVec.x(), 0.0)).normalized();
    northVec = upVec.cross(eastVec);

    // define orthogonal unit vectors in the directions of semiMajor and semiMinor axis of ellipse
    majorVec = cos(rlanOrientationRad) * northVec + sin(rlanOrientationRad) * eastVec;
    minorVec = sin(rlanOrientationRad) * northVec - cos(rlanOrientationRad) * eastVec;

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

    mxA = mxS*mxR*mxE*(mxR.t())*mxS;
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
std::vector<GeodeticCoord> EllipseRlanRegionClass::getBoundary() const
{
    std::vector<GeodeticCoord> ptList;
    arma::vec P(2);

    int ptIdx;
    int numRLANPoints = 32;

    for(ptIdx=0; ptIdx<numRLANPoints; ptIdx++) {
        double phi = 2*M_PI*ptIdx/numRLANPoints;
        P(0) = cos(phi);
        P(1) = sin(phi);
        double d = dot(P, mxA*P);
        double longitude = centerLongitude + P(0)/sqrt(d);
        double latitude  = centerLatitude  + P(1)/sqrt(d);
        GeodeticCoord rlanEllipsePtGeo = GeodeticCoord::fromLatLon(latitude, longitude, centerHeight/1000.0);
        ptList.push_back(rlanEllipsePtGeo);
    }

    return(ptList);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: EllipseRlanRegionClass::getMinHeightAGL()                              ****/
/******************************************************************************************/
double EllipseRlanRegionClass::getMinHeightAGL() const
{
    return(centerHeight - heightUncertainty - centerTerrainHeight);
}
/******************************************************************************************/

/******************************************************************************************/
/**** CONSTRUCTOR: PolygonRlanRegionClass::PolygonRlanRegionClass()                    ****/
/******************************************************************************************/
PolygonRlanRegionClass::PolygonRlanRegionClass(DoubleTriplet rlanLLA, DoubleTriplet rlanUncerts_m,
    const std::vector<std::pair<double, double>>& rlanPolygon, std::string rlanHeightType, TerrainClass *terrain, RLANBoundary polygonTypeVal) :
polygonType(polygonTypeVal)
{
    std::tie(centerLatitude, centerLongitude, centerHeightInput) = rlanLLA;
    std::tie(std::ignore, std::ignore, heightUncertainty) = rlanUncerts_m;

    double bldgHeight;
    MultibandRasterClass::HeightResult lidarHeightResult;
    CConst::HeightSourceEnum rlanHeightSource;
    terrain->getTerrainHeight(centerLongitude, centerLatitude, centerTerrainHeight, bldgHeight, lidarHeightResult, rlanHeightSource);

    // LOGGER_DEBUG(logger) << "rlanHeight: " << centerTerrainHeight << ", building height: " << bldgHeight << ", from: " << rlanHeightSource;

    if (rlanHeightType == "AMSL") {
        centerHeight = centerHeightInput;
    } else if (rlanHeightType == "AGL") {
        centerHeight = centerHeightInput + centerTerrainHeight;
    } else {
        throw std::runtime_error(ErrStream() << "ERROR: INVALID rlanHeightType = " << rlanHeightType);
    }

    centerPosn = EcefModel::geodeticToEcef(centerLatitude, centerLongitude, centerHeight / 1000.0);

    upVec = centerPosn.normalized();
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

            Vector3 position = centerPosn + (length/1000.0)*(northVec*cos(angle*M_PI/180.0) + eastVec*sin(angle*M_PI/180.0));
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
std::vector<GeodeticCoord> PolygonRlanRegionClass::getBoundary() const
{
    std::vector<GeodeticCoord> ptList;

    int ptIdx;
    int numRLANPoints = polygon->num_bdy_pt[0];

    for(ptIdx=0; ptIdx<numRLANPoints; ptIdx++) {
        int xval = polygon->bdy_pt_x[0][ptIdx];
        int yval = polygon->bdy_pt_y[0][ptIdx];
        double longitude = centerLongitude + xval*resolution*oneOverCosVal;
        double latitude  = centerLatitude  + yval*resolution;
        GeodeticCoord rlanEllipsePtGeo = GeodeticCoord::fromLatLon(latitude, longitude, centerHeight/1000.0);
        ptList.push_back(rlanEllipsePtGeo);
    }

    return(ptList);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PolygonRlanRegionClass::getMinHeightAGL()                              ****/
/******************************************************************************************/
double PolygonRlanRegionClass::getMinHeightAGL() const
{
    return(centerHeight - heightUncertainty - centerTerrainHeight);
}
/******************************************************************************************/
