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

	enum TypeEnum {
		AntennaType,
		ReflectorType,
		UnknownType
	};

	AntennaModelClass(std::string nameVal);

    void setType(TypeEnum typeVal) { type = typeVal; }
    void setCategory(CategoryEnum categoryVal) { category = categoryVal; }
    void setDiameterM(double diameterMVal) { diameterM = diameterMVal; }
	void setMidbandGain(double midbandGainVal) { midbandGain = midbandGainVal; }
	void setReflectorWidthM(double reflectorWidthMVal) { reflectorWidthM = reflectorWidthMVal; }
	void setReflectorHeightM(double reflectorHeightMVal) { reflectorHeightM = reflectorHeightMVal; }

	static std::string categoryStr(CategoryEnum categoryVal);
	static std::string typeStr(TypeEnum typeVal);

	std::string name;
	TypeEnum type;
	CategoryEnum category;
	double diameterM;        // Antenna diameter in meters
	double midbandGain;      // Antenna midband gain (dB)
	double reflectorWidthM;  // Reflector Width (m)
	double reflectorHeightM; // AReflector Height (m)
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
