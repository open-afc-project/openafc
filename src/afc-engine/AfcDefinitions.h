#ifndef INCLUDE_AFCDEFINITIONS_H
#define INCLUDE_AFCDEFINITIONS_H

#include <fstream>
#include <sstream>
#include <tuple>
#include <vector>

#include <boost/program_options.hpp>

#include "MathHelpers.h"

#include "terrain.h"

namespace po = boost::program_options;

typedef typename std::pair<double, double> LatLon; // Stored as (Lat,Lon)

typedef typename std::tuple<double, double, double> DoubleTriplet;

typedef typename std::pair<double, double> AngleRadius; // (Degrees CW from true noth, Radius in meters)

const double quietNaN = std::numeric_limits<double>::quiet_NaN();

const double meanEarthR_m = 6.371e6;

//const TerrainClass* terrainObject = new TerrainClass();

enum RLANBoundary
{
	NO_BOUNDARY = 0, // empty default value
	ELLIPSE,
	LINEAR_POLY,
	RADIAL_POLY
};

enum RLANType
{
	RLAN_INDOOR,
	RLAN_OUTDOOR
};

enum BuildingType
{
	NO_BUILDING_TYPE,
	TRADITIONAL_BUILDING_TYPE,
	THERMALLY_EFFICIENCT_BUILDING_TYPE
};

enum ChannelColor
{
	RED,
	YELLOW,
	GREEN,

	BLACK  // In Denied Region
};

enum ChannelType
{
	INQUIRED_FREQUENCY,
	INQUIRED_CHANNEL
};

class ChannelStruct {
	public:
		ChannelColor availability;
		ChannelType type;
		double eirpLimit_dBm;
		int startFreqMHz;
		int stopFreqMHz;
		int index;
		int operatingClass;

		int bandwidth() const
		{
			return stopFreqMHz - startFreqMHz;
		}
};

class psdFreqRangeClass {
	public:
		std::vector<int> freqMHzList;
		std::vector<double> psd_dBm_MHzList;
		// psd_dBm_MHzList has size N
		// freqMHzList has size N+1
		// psd_dBm_MHzList[i] is the PSD from freqMHzList[i] to freqMHzList[i+1]
		// freqMHzList[0] and freqMHzList[N] are the start and stop frequencies of the corresponding inquiredFrequencyRange
};

inline std::string slurp(const std::ifstream &inStream) {
	std::stringstream strStream;
	strStream << inStream.rdbuf();
	return strStream.str();
}

#endif // INCLUDE_AFCDEFINITIONS_H
