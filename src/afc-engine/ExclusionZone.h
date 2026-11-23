#ifndef INCLUDE_EXCLUSION_ZONE_H
#define INCLUDE_EXCLUSION_ZONE_H

// Standard library
#include <stdlib.h>
#include <algorithm>
#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>
#include <ctime>
#include <exception>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <limits>
#include <utility>
#include <vector>

#include "RlanRegion.h"

class PolygonClass;
class ULSClass;

class FSExclusionZoneClass
{
	public:
        FSExclusionZoneClass(double psdDBm_MHzVal,
            ULSClass *ulsVal, std::vector<PolygonClass *> polyListVal);
		~FSExclusionZoneClass();

         ULSClass *getULS() { return(uls); }
         double getPSD() { return(psdDBm_MHz); }

         std::vector<PolygonClass *> getPolyList() { return(polyList); }

        void createBounds(PolygonClass *& convexHull, double& enclosingCircleCenterLon, double& enclosingCircleCenterLat, double& enclosingCircleRadiusM, int polygonPtsPerDeg);

	private:
        void createBoundingCircle(PolygonClass *polygon, double &centerLon, double &centerLat, double &radiusM, int polygonPtsPerDeg);

        double psdDBm_MHz;
        ULSClass *uls;
		std::vector<PolygonClass *> polyList; // Polygon list, multiple polygons for exclusion zone
};

class ExclusionZoneClass
{
	public:
		ExclusionZoneClass();
		~ExclusionZoneClass();

        void appendPSD(double rlanPSD) { rlanPSDList.push_back(rlanPSD); return; }
        int getNumRlanPSD() { return (int) rlanPSDList.size(); }
        double getRlanPSD(int psdIdx) { return rlanPSDList[psdIdx]; }

        void appendFSExclusionZone(FSExclusionZoneClass *fsExclusionZone) { fsExclusionZoneList.push_back(fsExclusionZone); return; }
        int getNumFSExclusionZone() { return (int) fsExclusionZoneList.size(); }
        FSExclusionZoneClass *getFSExclusionZone(int fsExclusionZoneIdx) { return fsExclusionZoneList[fsExclusionZoneIdx]; }

        int getPolygonPtsPerDeg() { return polygonPtsPerDeg; }
        int getImagePtsPerDeg() { return imagePtsPerDeg; }

        static constexpr double rlanAGLHeight = 10.0;
        static constexpr double rlanVerticalUncertainty = 0.0;

	private:
        std::vector<double> rlanPSDList; // PSD values (DBm/MHz) for Exclusion Zone Analysis

        std::vector<FSExclusionZoneClass *> fsExclusionZoneList;

        std::vector<std::tuple<double, double, double>> exclusionZoneDebugParamList; // list of longitude, latitude, radius values to use for debug.

		int imagePtsPerDeg; // Resolution to use for sampling exclusion zone in points per degree, 3600
						 // corresp to 1 arcsec (30 m), should not have to change
		int polygonPtsPerDeg; // Resolution to use for polygon vertices in points per degree, 108000
						 // corresp to 1.03m, should not have to change

        RlanRegionClass *areaOfIntendedOperation; // Area of intended operation

};

#endif // INCLUDE_EXCLUSION_ZONE_H
