#ifndef INCLUDE_IPDR_UTIL_READITUFILES_HPP_
#define INCLUDE_IPDR_UTIL_READITUFILES_HPP_

#include <array>
#include <string>
#include <vector>

class ITUDataClass
{
	public:
		ITUDataClass(std::string t_radioClimatePath, std::string t_surfRefracPath);
		~ITUDataClass();

		int getRadioClimateValue(double latDeg, double lonDeg);
		double getSurfaceRefractivityValue(double latDeg, double lonDeg);

	private:
		const int RCNumLat = 360;
		const int RCNumLon = 720;
		void readRCFile(std::string RCFile);
		int **RCData;

		const int SRNumLat = 121;
		const int SRNumLon = 241;
		void readSRFile(std::string SRFile);
		double **SRData;
};

#endif // INCLUDE_IPDR_UTIL_READITUFILES_HPP_
