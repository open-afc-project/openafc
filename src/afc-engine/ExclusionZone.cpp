/******************************************************************************************/
/**** File: ExclusionZone.cpp                                                          ****/
/******************************************************************************************/
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <fstream>

#include "MathConstants.h"
#include "global_defines.h"
#include "global_fn.h"
#include "ExclusionZone.h"

constexpr double ExclusionZoneClass::rlanAGLHeight;
constexpr double ExclusionZoneClass::rlanVerticalUncertainty;

/******************************************************************************************/
/**** FUNCTION: FSExclusionZoneClass::FSExclusionZoneClass                             ****/
/******************************************************************************************/
FSExclusionZoneClass::FSExclusionZoneClass(double psdDBm_MHzVal,
    ULSClass *ulsVal, std::vector<PolygonClass *> polyListVal)
    : psdDBm_MHz(psdDBm_MHzVal), uls(ulsVal), polyList(polyListVal)
{

}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: FSExclusionZoneClass::~FSExclusionZoneClass                            ****/
/******************************************************************************************/
FSExclusionZoneClass::~FSExclusionZoneClass()
{
    for(int polyIdx=0; polyIdx<((int) polyList.size()); ++polyIdx) {
        delete polyList[polyIdx];
    }
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ExclusionZoneClass::ExclusionZoneClass                                 ****/
/******************************************************************************************/
ExclusionZoneClass::ExclusionZoneClass()
{
    imagePtsPerDeg = 3600;
    polygonPtsPerDeg = 108000;

    areaOfIntendedOperation = (RlanRegionClass *) NULL;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ExclusionZoneClass::~ExclusionZoneClass                                ****/
/******************************************************************************************/
ExclusionZoneClass::~ExclusionZoneClass()
{
    if (areaOfIntendedOperation) {
        delete areaOfIntendedOperation;
    }

    for(int fsExclZoneIdx=0; fsExclZoneIdx<((int) fsExclusionZoneList.size()); ++fsExclZoneIdx) {
        delete fsExclusionZoneList[fsExclZoneIdx];
    }
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: FSExclusionZoneClass::createBounds()                                   ****/
/******************************************************************************************/
void FSExclusionZoneClass::createBounds(PolygonClass *& convexHull, double& enclosingCircleCenterLon, double& enclosingCircleCenterLat, double& enclosingCircleRadiusM, int polygonPtsPerDeg)

{
    /**************************************************************************************/
    /* Create convex hull of exclusion zone points                                        */
    /**************************************************************************************/
    std::vector<std::tuple<int,int>> ezPointList;
    for(int polyIdx=0; polyIdx<((int) polyList.size()); ++polyIdx) {
        PolygonClass *polygon = polyList[polyIdx];
        for(int ptIdx=0; ptIdx<polygon->num_bdy_pt[0]; ++ptIdx) {
            ezPointList.push_back(std::make_tuple(polygon->bdy_pt_x[0][ptIdx], polygon->bdy_pt_y[0][ptIdx]));           
        }
    }
    convexHull = PolygonClass::convexHull(ezPointList);
    /**************************************************************************************/

    /**************************************************************************************/
    /* Create bounding circle                                                             */
    /**************************************************************************************/
    createBoundingCircle(convexHull, enclosingCircleCenterLon, enclosingCircleCenterLat, enclosingCircleRadiusM, polygonPtsPerDeg);
            
#if 0
    printf("BOUNDING CIRCLE:\n");
    printf("Center Longitude: %.6f\n", enclosingCircleCenterLon);
    printf("Center Latitude: %.6f\n",  enclosingCircleCenterLat);
    printf("Radius: %.1f\n",  enclosingCircleRadiusM);
#endif
    /**************************************************************************************/

    return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: FSExclusionZoneClass::createBoundingCircle()                           ****/
/******************************************************************************************/
void circle_from_2_points(const std::pair<double, double>& p1, const std::pair<double, double>& p2, double& cX, double& cY, double& r) {
    double p1X, p1Y, p2X, p2Y;
    std::tie(p1X, p1Y) = p1;
    std::tie(p2X, p2Y) = p2;
    cX = (p1X + p2X)/2.0;
    cY = (p1Y + p2Y)/2.0;
    double dx = p2X - p1X;
    double dy = p2Y - p1Y;
    r = sqrt(dx*dx+dy*dy) / 2.0 + 1.0e-2;
    return;
}

void circle_from_3_points(const std::pair<double, double>& p1, const std::pair<double, double>& p2, const std::pair<double, double>& p3,
    double& cX, double& cY, double& r) {
    double p1X, p1Y, p2X, p2Y, p3X, p3Y;
    std::tie(p1X, p1Y) = p1;
    std::tie(p2X, p2Y) = p2;
    std::tie(p3X, p3Y) = p3;

    double dx, dy;
    double d = 2.0 * (p1X * (p2Y - p3Y) + p2X * (p3Y - p1Y) + p3X * (p1X - p2Y));
    if (std::abs(d) < 1e-9) {
        circle_from_2_points(p1, p2, cX, cY, r);
        dx = p3X - cX;
        dy = p3Y - cY;
        if (dx*dx+dy*dy <= r*r) {
            return;
        }
        circle_from_2_points(p1, p3, cX, cY, r);
        dx = p2X - cX;
        dy = p2Y - cY;
        if (dx*dx+dy*dy <= r*r) {
            return;
        }
        if (dx*dx+dy*dy <= r*r) {
            return;
        }

        circle_from_2_points(p2, p3, cX, cY, r);
        return;
    }

    cX = ((p1X * p1X + p1Y * p1Y) * (p2Y - p3Y) +
          (p2X * p2X + p2Y * p2Y) * (p3Y - p1Y) +
          (p3X * p3X + p3Y * p3Y) * (p1Y - p2Y)) / d;

    cY = ((p1X * p1X + p1Y * p1Y) * (p3X - p2X) +
          (p2X * p2X + p2Y * p2Y) * (p1X - p3X) +
          (p3X * p3X + p3Y * p3Y) * (p2X - p1X)) / d;

    dx = p1X - cX;
    dy = p1Y - cY;

    r = sqrt(dx*dx+dy*dy) + 1.0e-2;
    return;
}

void FSExclusionZoneClass::createBoundingCircle(PolygonClass *polygon,
            double &centerLon, double &centerLat, double &radiusM, int polygonPtsPerDeg)
{
    int N = 0;

    int minLonN, maxLonN;
    int minLatN, maxLatN;
    bool init = true;

    {
        if (polygon->num_segment != 1) {
            throw std::runtime_error(ErrStream()
                << std::string("ERROR: createBoundingCircle: polygonList contains polygon with multiple segments.") << std::endl
                );
        }
        N += polygon->num_bdy_pt[0];
        for(int ptIdx=0; ptIdx<polygon->num_bdy_pt[0]; ++ptIdx) {
            if (init) {
                minLonN = polygon->bdy_pt_x[0][ptIdx];
                maxLonN = polygon->bdy_pt_x[0][ptIdx];
                minLatN = polygon->bdy_pt_y[0][ptIdx];
                maxLatN = polygon->bdy_pt_y[0][ptIdx];
                init = false;
            } else {
                if (polygon->bdy_pt_x[0][ptIdx] < minLonN) {
                    minLonN = polygon->bdy_pt_x[0][ptIdx];
                } else if (polygon->bdy_pt_x[0][ptIdx] > maxLonN) {
                    maxLonN = polygon->bdy_pt_x[0][ptIdx];
                }
                if (polygon->bdy_pt_y[0][ptIdx] < minLatN) {
                    minLatN = polygon->bdy_pt_y[0][ptIdx];
                } else if (polygon->bdy_pt_y[0][ptIdx] > maxLatN) {
                    maxLatN = polygon->bdy_pt_y[0][ptIdx];
                }
            }
        }
    }

    if (N <= 1) {
        throw std::runtime_error(ErrStream()
                << std::string("ERROR: createBoundingCircle: polygonList invalid") << std::endl
                );
    }

    // Calculate lon0, lat0, coord transform parameters
    double lon0 = ((double) (minLonN+maxLonN)/2.0)/polygonPtsPerDeg;
    double lat0 = ((double) (minLatN+maxLatN)/2.0)/polygonPtsPerDeg;
    double cosLat0, sinLat0;
    double cosLon0, sinLon0;
    sincos(lat0*M_PI/180.0, &sinLat0, &cosLat0);
    sincos(lon0*M_PI/180.0, &sinLon0, &cosLon0);
    double R = MathConstants::WGS84EarthSemiMajorAxis*sqrt(1.0 - MathConstants::WGS84EarthFirstEccentricitySquared*sinLat0*sinLat0*(1.0-MathConstants::WGS84EarthFirstEccentricitySquared)/(1.0-MathConstants::WGS84EarthFirstEccentricitySquared*sinLat0*sinLat0))*1000.0;
    Vector3 upVec = Vector3(cosLat0*cosLon0, cosLat0*sinLon0, sinLat0);
    Vector3 eastVec = (Vector3(-upVec.y(), upVec.x(), 0.0)).normalized();
    Vector3 northVec = upVec.cross(eastVec);

    // Transform lon,lat (deg) to X,Y (meter)
    std::vector<std::pair<double,double>> pointList;
    {
        for(int ptIdx=0; ptIdx<polygon->num_bdy_pt[0]; ++ptIdx) {
            double ptLon = ((double) polygon->bdy_pt_x[0][ptIdx])/polygonPtsPerDeg;
            double ptLat = ((double) polygon->bdy_pt_y[0][ptIdx])/polygonPtsPerDeg;
            double ptCosLat, ptSinLat;
            double ptCosLon, ptSinLon;
            sincos(ptLon*M_PI/180.0, &ptSinLon, &ptCosLon);
            sincos(ptLat*M_PI/180.0, &ptSinLat, &ptCosLat);
            Vector3 ptPosn = Vector3(ptCosLat*ptCosLon, ptCosLat*ptSinLon, ptSinLat);
            double ptPosnUp    = ptPosn.dot(upVec);
            double ptPosnEast  = ptPosn.dot(eastVec);
            double ptPosnNorth = ptPosn.dot(northVec);
            Vector3 rotPtPosn = ptPosnEast*eastVec + (ptPosnUp*cosLat0 + ptPosnNorth*sinLat0)*upVec + (ptPosnNorth*cosLat0 - ptPosnUp*sinLat0)*northVec;
            double rotPtLat = asin(rotPtPosn.z())*180.0/M_PI;
            double rotPtLon = atan2(rotPtPosn.y(), rotPtPosn.x())*180.0/M_PI;
            double ptX = (rotPtLon - lon0)*R*M_PI/180.0;
            double ptY = rotPtLat*R*M_PI/180.0;
            pointList.push_back(std::make_pair(ptX, ptY));
        }
    }

    // Compute Minimum Circle in Meter Space
   int n = (int) pointList.size();
   double centerX, centerY;
   std::tie(centerX, centerY) = pointList[0];
   radiusM = 0.0;

   int i = 1;
   while (i < n) {
       // If current point is outside candidate circle
       double ptX, ptY;
       std::tie(ptX, ptY) = pointList[i];
       double dx = ptX - centerX;
       double dy = ptY - centerY;
       if (dx*dx+dy*dy > radiusM*radiusM) {
           // Recompute base circle starting with current point on boundary
           centerX = ptX;
           centerY = ptY;
           radiusM = 0.0;

           for (int j = 0; j < i; ++j) {
               std::tie(ptX, ptY) = pointList[j];
               dx = ptX - centerX;
               dy = ptY - centerY;
               if (dx*dx+dy*dy > radiusM*radiusM) {
                   circle_from_2_points(pointList[i], pointList[j], centerX, centerY, radiusM);

                   for (int k = 0; k < j; ++k) {
                       std::tie(ptX, ptY) = pointList[k];
                       dx = ptX - centerX;
                       dy = ptY - centerY;
                       if (dx*dx+dy*dy > radiusM*radiusM) {
                           circle_from_3_points(pointList[i], pointList[j], pointList[k], centerX, centerY, radiusM);
                       }
                   }
               }
           }

           // Move violating point to front to accelerate convergence
           std::pair<double, double> key = pointList[i];
           for (size_t k = i; k > 0; --k) {
               pointList[k] = pointList[k - 1];
           }
           pointList[0] = key;

           // Restart linear scan
           i = 1;
       } else {
           i++;
       }
   }

    // Transform X,Y (meter) to lon,lat (deg)
    double rotCenterLon = lon0 + (centerX / R)*(180.0/M_PI);
    double rotCenterLat = (centerY / R)*(180.0/M_PI);

    double rotCenterCosLat, rotCenterSinLat;
    double rotCenterCosLon, rotCenterSinLon;
    sincos(rotCenterLon*M_PI/180.0, &rotCenterSinLon, &rotCenterCosLon);
    sincos(rotCenterLat*M_PI/180.0, &rotCenterSinLat, &rotCenterCosLat);
    Vector3 rotCenterPosn = Vector3(rotCenterCosLat*rotCenterCosLon, rotCenterCosLat*rotCenterSinLon, rotCenterSinLat);
    double rotCenterPosnUp    = rotCenterPosn.dot(upVec);
    double rotCenterPosnEast  = rotCenterPosn.dot(eastVec);
    double rotCenterPosnNorth = rotCenterPosn.dot(northVec);
    Vector3 centerPosn = rotCenterPosnEast*eastVec + (rotCenterPosnUp*cosLat0 - rotCenterPosnNorth*sinLat0)*upVec + (rotCenterPosnNorth*cosLat0 + rotCenterPosnUp*sinLat0)*northVec;
    centerLat = asin(centerPosn.z())*180.0/M_PI;
    centerLon = atan2(centerPosn.y(), centerPosn.x())*180.0/M_PI;

    return;
}
/******************************************************************************************/
