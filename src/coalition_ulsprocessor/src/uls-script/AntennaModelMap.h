#ifndef ANTENNA_MODEL_MAP_H
#define ANTENNA_MODEL_MAP_H

#include <iostream>
#include <string>
#include <regex>
#include "global_fn.h"

namespace AntennaModel
{
enum CategoryEnum { HPCategory, B1Category, OtherCategory, UnknownCategory };

enum TypeEnum { AntennaType, ReflectorType, UnknownType };

std::string categoryStr(AntennaModel::CategoryEnum categoryVal);
std::string typeStr(AntennaModel::TypeEnum typeVal);
}

class AntennaModelClass
{
	public:
		AntennaModelClass(std::string nameVal);

		void setType(AntennaModel::TypeEnum typeVal)
		{
			type = typeVal;
		}
		void setCategory(AntennaModel::CategoryEnum categoryVal)
		{
			category = categoryVal;
		}
		void setDiameterM(double diameterMVal)
		{
			diameterM = diameterMVal;
		}
		void setMidbandGain(double midbandGainVal)
		{
			midbandGain = midbandGainVal;
		}
		void setReflectorWidthM(double reflectorWidthMVal)
		{
			reflectorWidthM = reflectorWidthMVal;
		}
		void setReflectorHeightM(double reflectorHeightMVal)
		{
			reflectorHeightM = reflectorHeightMVal;
		}

		std::string name;
		AntennaModel::TypeEnum type;
		AntennaModel::CategoryEnum category;
		double diameterM; // Antenna diameter in meters
		double midbandGain; // Antenna midband gain (dB)
		double reflectorWidthM; // Reflector Width (m)
		double reflectorHeightM; // AReflector Height (m)
};

class AntennaPrefixClass
{
	public:
		AntennaPrefixClass(std::string prefixVal);

		void setType(AntennaModel::TypeEnum typeVal)
		{
			type = typeVal;
		}
		void setCategory(AntennaModel::CategoryEnum categoryVal)
		{
			category = categoryVal;
		}

		std::string prefix;
		AntennaModel::TypeEnum type;
		AntennaModel::CategoryEnum category;
};

class AntennaModelMapClass
{
	public:
		AntennaModelMapClass(const std::string antListFile,
				     const std::string antPrefixFile,
				     const std::string antMapFile);
		AntennaModelClass *find(std::string antPfx,
					std::string modelName,
					AntennaModel::CategoryEnum &category,
					AntennaModel::CategoryEnum modelNameBlankCategory);

	private:
		void readModelList(const std::string filename);
		void readPrefixList(const std::string filename);
		void readModelMap(const std::string filename);

		std::vector<AntennaModelClass *> antennaModelList;
		std::vector<AntennaPrefixClass *> antennaPrefixList;
		std::vector<std::regex *> regexList;
		std::vector<int> antIdxList;
};

#endif
