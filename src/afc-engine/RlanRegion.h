/******************************************************************************************/
/**** FILE: RlanRegion.h                                                               ****/
/**** Class to define uncertainty region in which RLAN may be.  There are 2 types of   ****/
/**** regions, ellipse and polygon.                                                    ****/
/******************************************************************************************/

#ifndef RLAN_REGION_H
#define RLAN_REGION_H

#include <string>
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
    virtual std::vector<GeodeticCoord> getBoundary() const = 0;
    virtual double getMinHeightAGL()  const = 0;

    double getCenterLongitude() const { return(centerLongitude); }
    double getCenterLatitude()  const { return(centerLatitude); }
    double getCenterHeight()    const { return(centerHeight); }
    double getHeightUncertainty()  const { return(heightUncertainty); }
    
    Vector3 getCenterPosn()  const { return(centerPosn); }

protected:
    double centerLongitude;
    double centerLatitude;
    double centerHeightInput;
    double centerHeight;
    double centerTerrainHeight;
    double heightUncertainty;
    Vector3 centerPosn;
    Vector3 upVec;
    Vector3 eastVec;
    Vector3 northVec;
};
/******************************************************************************************/

/******************************************************************************************/
/**** CLASS: EllipseRlanRegionClass                                                    ****/
/******************************************************************************************/
class EllipseRlanRegionClass : RlanRegionClass
{
public:
    EllipseRlanRegionClass(DoubleTriplet rlanLLA, DoubleTriplet rlanUncerts_m,
        double rlanOrientationDeg, std::string rlanHeightType, TerrainClass *terrain);

    ~EllipseRlanRegionClass();

    RLANBoundary getType() const;
    LatLon closestPoint(LatLon latlon, bool& contains) const;
    std::vector<GeodeticCoord> getBoundary() const;
    double getMinHeightAGL() const;

private:
    double semiMinorAxis;
    double semiMajorAxis;
    double orientationDeg;

    Vector3 majorVec;
    Vector3 minorVec;

    arma::mat mxA;
};
/******************************************************************************************/

/******************************************************************************************/
/**** CLASS: PolygonRlanRegionClass                                                    ****/
/******************************************************************************************/
class PolygonRlanRegionClass : RlanRegionClass
{
public:
    PolygonRlanRegionClass(DoubleTriplet rlanLLA, DoubleTriplet rlanUncerts_m,
        const std::vector<std::pair<double, double>>& rlanPolygon, std::string rlanHeightType, TerrainClass *terrain, RLANBoundary polygonTypeVal);

    ~PolygonRlanRegionClass();

    RLANBoundary getType() const;
    LatLon closestPoint(LatLon latlon, bool& contains) const;
    std::vector<GeodeticCoord> getBoundary() const;
    double getMinHeightAGL() const;

private:
    double resolution, cosVal, oneOverCosVal;
    PolygonClass *polygon;
    RLANBoundary polygonType;
};
/******************************************************************************************/

#endif
