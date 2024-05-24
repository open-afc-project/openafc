#include "readITUFiles.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

ITUDataClass::ITUDataClass(std::string t_radioClimatePath, std::string t_surfRefracPath)
{
	readRCFile(t_radioClimatePath);
	readSRFile(t_surfRefracPath);
}

ITUDataClass::~ITUDataClass()
{
	int latIdx;

	for (latIdx = 0; latIdx < RCNumLat; ++latIdx) {
		free(RCData[latIdx]);
	}
	free(RCData);

	for (latIdx = 0; latIdx < SRNumLat; ++latIdx) {
		free(SRData[latIdx]);
	}
	free(SRData);
}

void ITUDataClass::readRCFile(std::string RCFile)
{
	std::ifstream file(RCFile);

	int latIdx, lonIdx;

	RCData = (int **)malloc(RCNumLat * sizeof(int *));
	for (latIdx = 0; latIdx < RCNumLat; ++latIdx) {
		RCData[latIdx] = (int *)malloc(RCNumLon * sizeof(int));
	}
	std::string line;

	latIdx = 0;
	while (std::getline(file, line)) {
		std::istringstream lineBuffer(line);
		for (lonIdx = 0; lonIdx < RCNumLon; ++lonIdx)
			lineBuffer >> RCData[latIdx][lonIdx];

		latIdx++;
	}
	if (latIdx != RCNumLat) {
		throw std::length_error("ERROR: Incorrect number of rows in " + RCFile);
	}

	return;
}

void ITUDataClass::readSRFile(std::string SRFile)
{
	std::ifstream file(SRFile);

	int latIdx, lonIdx;

	SRData = (double **)malloc(SRNumLat * sizeof(double *));
	for (latIdx = 0; latIdx < SRNumLat; ++latIdx) {
		SRData[latIdx] = (double *)malloc(SRNumLon * sizeof(double));
	}
	std::string line;

	latIdx = 0;
	while (std::getline(file, line)) {
		std::istringstream lineBuffer(line);
		for (lonIdx = 0; lonIdx < SRNumLon; lonIdx++)
			lineBuffer >> SRData[latIdx][lonIdx];

		latIdx++;
	}
	if (latIdx != SRNumLat) {
		throw std::length_error("ERROR: Incorrect number of rows in " + SRFile);
	}

	return;
}

int ITUDataClass::getRadioClimateValue(double latDeg, double lonDeg)
{
	if (latDeg < -90 || latDeg > 90) {
		throw std::range_error("Latitude outside [-90.0,90.0]!");
	} else if (latDeg < -89.75) {
		latDeg = -89.75;
	}

	if (lonDeg < -180 || lonDeg > 360) {
		throw std::range_error("Longitude outside [-180.0,360.0]!");
	}

	int latIdx = std::floor((90.0 - latDeg) * 2.0);

	int lonIdx = std::floor((lonDeg + 180.0) * 2.0);

	if (lonIdx >= 720) {
		lonIdx -= 720;
	}

	int radio_climate_value = RCData[latIdx][lonIdx];

	return radio_climate_value;
}

double ITUDataClass::getSurfaceRefractivityValue(double latDeg, double lonDeg)
{
	if (latDeg < -90 || latDeg > 90) {
		throw std::range_error("Latitude outside [-90.0,90.0]!");
	}

	if (lonDeg < -180 || lonDeg > 360) {
		throw std::range_error("Longitude outside [-180.0,360.0]!");
	}

	if (lonDeg < 0.0) {
		lonDeg += 360.0;
	}

	double latIdxDbl = (90.0 - latDeg) / 1.5;

	int latIdx0 = (int)std::floor(latIdxDbl);

	if (latIdx0 == 120) {
		latIdx0 = 119;
	}

	double lonIdxDbl = (lonDeg) / 1.5;

	int lonIdx0 = (int)std::floor(lonIdxDbl);

	if (lonIdx0 == 240) {
		lonIdx0 = 239;
	}

	int latIdx1 = latIdx0 + 1;
	int lonIdx1 = lonIdx0 + 1;

	double val00 = SRData[latIdx0][lonIdx0];
	double val01 = SRData[latIdx0][lonIdx1];
	double val10 = SRData[latIdx1][lonIdx0];
	double val11 = SRData[latIdx1][lonIdx1];

	double surf_refract = (val00 * (latIdx1 - latIdxDbl) * (lonIdx1 - lonIdxDbl) +
		val01 * (latIdx1 - latIdxDbl) * (lonIdxDbl - lonIdx0) +
		val10 * (latIdxDbl - latIdx0) * (lonIdx1 - lonIdxDbl) +
		val11 * (latIdxDbl - latIdx0) * (lonIdxDbl - lonIdx0));

	return surf_refract;
}
