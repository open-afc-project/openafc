#ifndef ANTENNA_MODEL_MAP_H
#define ANTENNA_MODEL_MAP_H

#include <iostream>
#include <string>
#include <regex>
#include "global_fn.h"

class AntennaModelClass {
public:
	enum CategoryEnum {
		HPCategory,
		B1Category,
		OtherCategory,
		UnknownCategory
	};

	AntennaModelClass(std::string nameVal, CategoryEnum categoryVal, double diameterMVal);

	static std::string categoryStr(CategoryEnum categoryVal);

	std::string name;
	CategoryEnum category;
	double diameterM;      // Antenna diameter in meters
};

class AntennaModelMapClass {
public:
	AntennaModelMapClass(const std::string antListFile, const std::string antMapFile);
	AntennaModelClass *find(std::string modelName);

private:
	void readModelList(const std::string filename);
	void readModelMap(const std::string filename);

	std::vector<AntennaModelClass *> antennaModelList;
	std::vector<std::regex *> regexList;
	std::vector<int> antIdxList;
};

#endif
