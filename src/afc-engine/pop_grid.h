/******************************************************************************************/
/**** FILE : pop_grid.h                                                                ****/
/******************************************************************************************/

#ifndef POP_GRID_H
#define POP_GRID_H

#include <vector>
#include "list.h"
#include "cconst.h"

class ULSClass;
class PolygonClass;

/******************************************************************************************/
/**** CLASS: PopGridClass                                                              ****/
/**** Population Grid Class.  Grid of LON/LAT coordinates, where LON vals are equally  ****/
/**** spaced in increments of deltaLon, and LAT values equally spaced in increments of ****/
/**** deltaLat.  For each LON/LAT coord in the grid, the matrix stores the population  ****/
/**** over the region defined by LON +/- deltaLon/2, LAT +/- deltaLat/2.  The area of  ****/
/**** this region is taken to be:                                                      ****/
/****     R*cos(LAT)*deltaLon*deltaLat                                                 ****/
/**** Note that regions at different latitudes have different areas.                   ****/
/**** The population over the region is the polulation density times the area.         ****/
/**** An effort is made to be explicit with angle units by using Rad or Deg in         ****/
/**** variable names.                                                                  ****/
/**** Non-rectangular regions can be converted to rectangular regions by padding with  ****/
/**** zeros.                                                                           ****/
/******************************************************************************************/
class PopGridClass
{
	public:
		PopGridClass(double densityThrUrbanVal, double densityThrSuburbanVal, double densityThrRuralVal);
		PopGridClass(std::string worldPopulationFile, const std::vector<PolygonClass *> &regionPolygonList, double regionPolygonResolution, double densityThrUrbanVal, double densityThrSuburbanVal, double densityThrRuralVal);
		PopGridClass(const PopGridClass &obj);
		~PopGridClass();
		void readData(std::string populationDensityFile, const std::vector<std::string> &regionNameList, const std::vector<int> &regionIDList,
				int numLonVal, double deltaLonDeg, double minLonDeg,
				int numLatVal, double deltaLatDeg, double minLatDeg);
		void setDimensions(int numLonVal, double deltaLonRad, double minLonRad,
				int numLatVal, double deltaLatRad, double minLatRad);
		void genRandDeg(double &longitudeDeg, double &latitudeDeg, char &propEnvVal, int &regionIdxVal, int *lonIdxPtr = (int *) NULL, int *latIdxPtr = (int *) NULL);
		void scale(std::vector<double> urbanPopVal, std::vector<double> suburbanPopVal, std::vector<double> ruralPopVal, std::vector<double> barrenPopVal);
		int adjustRegion(double centerLongitudeDeg, double centerLatitudeDeg, double radius);
		double adjustRegion(ListClass<ULSClass *> *ulsList, double maxRadius);
		void makeCDF();
		void check(std::string s);
		void writeDensity(std::string filename, bool dumpPopGrid = false);
		void findDeg(double longitudeDeg, double latitudeDeg, int &lonIdx, int &latIdx, char &propEnvVal, int& regionIdx) const;
		double computeArea(int lonIdx, int latIdx) const;
		void setPop(int lonIdx, int latIdx, double popVal);
		void setPropEnv(int lonIdx, int latIdx, char propEnvVal);
		char getPropEnv(int lonIdx, int latIdx) const;
		double getPropEnvPop(CConst::PropEnvEnum propEnv, int regionIdx) const;
		double getPop(int lonIdx, int latIdx) const;
		double getPopFromCDF(int lonIdx, int latIdx) const;
		double getProbFromCDF(int lonIdx, int latIdx) const;
		void getLonLatDeg(int lonIdx, int latIdx, double &longitudeDeg, double &latitudeDeg);
		int getNumLon();
		int getNumLat();
		double getDensityThrUrban();
		double getDensityThrSuburban();
		double getDensityThrRural();
		std::string getRegionName(int regionIdx) { return(regionNameList[regionIdx]); }
		double getMinLonDeg() { return(minLonDeg); }
		double getMinLatDeg() { return(minLatDeg); }
		double getMaxLonDeg() { return(minLonDeg + numLon*deltaLonDeg); }
		double getMaxLatDeg() { return(minLatDeg + numLat*deltaLatDeg); }

	private:
		int numRegion;
		std::vector<std::string> regionNameList;
		// std::vector<int> regionIDList;
		double densityThrUrban, densityThrSuburban, densityThrRural;

		double minLonDeg;
		double minLatDeg;
		double deltaLonDeg;
		double deltaLatDeg;

		int numLon, numLat;
		double **pop;
		char **propEnv;
		int **region;
		std::vector<double> urbanPop;
		std::vector<double> suburbanPop;
		std::vector<double> ruralPop;
		std::vector<double> barrenPop;
		bool isCumulative;
};
/******************************************************************************************/

#endif
