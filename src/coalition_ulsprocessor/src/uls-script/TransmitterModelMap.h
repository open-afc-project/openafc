#ifndef TRANSMITTER_MODEL_MAP_H
#define TRANSMITTER_MODEL_MAP_H

#include <iostream>
#include <string>
#include <regex>
#include "global_fn.h"

class TransmitterModelClass
{
	public:
		enum ArchitectureEnum { IDUArchitecture, ODUArchitecture, UnknownArchitecture };

		TransmitterModelClass(std::string nameVal);

		void setArchitecture(ArchitectureEnum architectureVal)
		{
			architecture = architectureVal;
		}

		static std::string architectureStr(ArchitectureEnum architectureVal);

		std::string name;
		ArchitectureEnum architecture;
};

class TransmitterModelMapClass
{
	public:
		TransmitterModelMapClass(const std::string transmitterModelListFile);
		TransmitterModelClass *find(std::string modelName);
		int checkPrefixValues();

	private:
		void readModelList(const std::string filename);

		std::vector<TransmitterModelClass *> transmitterModelList;
};

#endif
