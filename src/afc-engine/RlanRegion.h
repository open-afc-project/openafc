/******************************************************************************************/
/**** FILE: RlanRegion.h                                                               ****/
/**** Class to define uncertainty region in which RLAN may be.  There are 2 types of   ****/
/**** regions, ellipse and polygon.                                                    ****/
/******************************************************************************************/

#ifndef RLAN_REGION_H
#define RLAN_REGION_H

#include <string>
#include <armadillo>
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
		virtual std::vector<LatLon> getScan(CConst::ScanRegionMethodEnum method, double scanResolutionM, int pointsPerDegree) = 0;
		virtual double getMaxDist() const = 0;
		virtual void configure(CConst::HeightTypeEnum rlanHeightType, TerrainClass *terrain) = 0;
		virtual double calcMinAOB(LatLon ulsRxLatLon, Vector3 ulsAntennaPointing, double ulsRxHeightAMSL,
			double& minAOBLon, double& minAOBLat, double& minAOBHeghtAMSL) = 0;

		std::vector<GeodeticCoord> getBoundaryPolygon(TerrainClass *terrain) const;
		double getMinHeightAGL()  const;
		double getMaxHeightAGL()  const;
		double getMinHeightAMSL()  const;
		double getMaxHeightAMSL()  const;
		double getCenterLongitude() const { return(centerLongitude); }
		double getCenterLatitude()  const { return(centerLatitude); }
		double getCenterHeightAMSL()    const { return(centerHeightAMSL); }
		double getCenterTerrainHeight()    const { return(centerTerrainHeight); }
		double getHeightUncertainty()  const { return(heightUncertainty); }
		bool   getFixedHeightAMSL()    const { return(fixedHeightAMSL); }
		Vector3 getCenterPosn()  const { return(centerPosn); }

		Vector3 computePointing(double azimuth, double elevation) const;

		static double calcMinAOB(PolygonClass *poly, double polyResolution, arma::vec& F, arma::vec& ptg, arma::vec& minLoc);
		static double minRlanHeightAboveTerrain;

	protected:
		std::vector<std::tuple<int, int>> *calcScanPointVirtices(int **S, int NX, int NY) const;

		double centerLongitude;
		double centerLatitude;
		double cosVal, oneOverCosVal;
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

		PolygonClass *boundaryPolygon;
		double polygonResolution;
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
		std::vector<LatLon> getScan(CConst::ScanRegionMethodEnum method, double scanResolutionM, int pointsPerDegree);
		double getMaxDist() const;
		void configure(CConst::HeightTypeEnum rlanHeightType, TerrainClass *terrain);
		double calcMinAOB(LatLon ulsRxLatLon, Vector3 ulsAntennaPointing, double ulsRxHeightAMSL,
			double& minAOBLon, double& minAOBLat, double& minAOBHeghtAMSL);

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
		arma::mat mxC;
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
		std::vector<LatLon> getScan(CConst::ScanRegionMethodEnum method, double scanResolutionM, int pointsPerDegree);
		double getMaxDist() const;
		void configure(CConst::HeightTypeEnum rlanHeightType, TerrainClass *terrain);
		double calcMinAOB(LatLon ulsRxLatLon, Vector3 ulsAntennaPointing, double ulsRxHeightAMSL,
			double& minAOBLon, double& minAOBLat, double& minAOBHeghtAMSL);

	private:
		PolygonClass *polygon;
		RLANBoundary polygonType;
};
/******************************************************************************************/

#endif
