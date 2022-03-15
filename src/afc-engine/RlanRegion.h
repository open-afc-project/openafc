/******************************************************************************************/
/**** FILE: RlanRegion.h                                                               ****/
/**** Class to define uncertainty region in which RLAN may be.  There are 2 types of   ****/
/**** regions, ellipse and polygon.                                                    ****/
/******************************************************************************************/

#ifndef RLAN_REGION_H
#define RLAN_REGION_H

#include <string>
#include "cconst.h"
#include "AfcDefinitions.h"
#include "Vector3.h"
#include "GeodeticCoord.h"
#include "polygon.h"

class TerrainClass;

/******************************************************************************************/
/**** CLASS: RlanRegionClass                                                           ****/
/******************************************************************************************/
class RlanRegionClass
{
public:
    RlanRegionClass();
    virtual ~RlanRegionClass();

    virtual RLANBoundary getType() const = 0;
    virtual LatLon closestPoint(LatLon latlon, bool& contains) const = 0;
    virtual std::vector<GeodeticCoord> getBoundary(TerrainClass *terrain) const = 0;
    virtual std::vector<LatLon> getScan(CConst::ScanRegionMethodEnum method, double scanResolutionM, int pointsPerDegree) const = 0;
    virtual double getMaxDist() const = 0;
    virtual void configure(CConst::HeightTypeEnum rlanHeightType, TerrainClass *terrain) = 0;

    double getMinHeightAGL()  const;
    double getMaxHeightAGL()  const;
    double getCenterLongitude() const { return(centerLongitude); }
    double getCenterLatitude()  const { return(centerLatitude); }
    double getCenterHeightAMSL()    const { return(centerHeightAMSL); }
    double getCenterTerrainHeight()    const { return(centerTerrainHeight); }
    double getHeightUncertainty()  const { return(heightUncertainty); }
    bool   getFixedHeightAMSL()    const { return(fixedHeightAMSL); }
    
    Vector3 getCenterPosn()  const { return(centerPosn); }

protected:
    double centerLongitude;
    double centerLatitude;
    double centerHeightInput;
    double centerHeightAMSL;
    double centerTerrainHeight;
    double minTerrainHeight;
    double maxTerrainHeight;
    double heightUncertainty;
    Vector3 centerPosn;
    Vector3 upVec;
    Vector3 eastVec;
    Vector3 northVec;

    bool fixedHeightAMSL;
    bool configuredFlag;
};
/******************************************************************************************/

/******************************************************************************************/
/**** CLASS: EllipseRlanRegionClass                                                    ****/
/******************************************************************************************/
class EllipseRlanRegionClass : RlanRegionClass
{
public:
    EllipseRlanRegionClass(DoubleTriplet rlanLLA, DoubleTriplet rlanUncerts_m,
        double rlanOrientationDeg, bool fixedHeightAMSLVal);

    ~EllipseRlanRegionClass();

    RLANBoundary getType() const;
    LatLon closestPoint(LatLon latlon, bool& contains) const;
    std::vector<GeodeticCoord> getBoundary(TerrainClass *terrain) const;
    std::vector<LatLon> getScan(CConst::ScanRegionMethodEnum method, double scanResolutionM, int pointsPerDegree) const;
    double getMaxDist() const;
    void configure(CConst::HeightTypeEnum rlanHeightType, TerrainClass *terrain);

private:
    void calcHorizExtents(double latVal, double& lonA, double& lonB, bool& flag) const;
    void calcVertExtents (double lonVal, double& latA, double& latB, bool& flag) const;

    double semiMinorAxis;
    double semiMajorAxis;
    double orientationDeg;

    Vector3 majorVec;
    Vector3 minorVec;

    arma::mat mxA;
    arma::mat mxB;
};
/******************************************************************************************/

/******************************************************************************************/
/**** CLASS: PolygonRlanRegionClass                                                    ****/
/******************************************************************************************/
class PolygonRlanRegionClass : RlanRegionClass
{
public:
    PolygonRlanRegionClass(DoubleTriplet rlanLLA, DoubleTriplet rlanUncerts_m,
        const std::vector<std::pair<double, double>>& rlanPolygon, RLANBoundary polygonTypeVal, bool fixedHeightAMSLVal);

    ~PolygonRlanRegionClass();

    RLANBoundary getType() const;
    LatLon closestPoint(LatLon latlon, bool& contains) const;
    std::vector<GeodeticCoord> getBoundary(TerrainClass *terrain) const;
    std::vector<LatLon> getScan(CConst::ScanRegionMethodEnum method, double scanResolutionM, int pointsPerDegree) const;
    double getMaxDist() const;
    void configure(CConst::HeightTypeEnum rlanHeightType, TerrainClass *terrain);

private:
    double resolution, cosVal, oneOverCosVal;
    PolygonClass *polygon;
    RLANBoundary polygonType;
};
/******************************************************************************************/

#endif
