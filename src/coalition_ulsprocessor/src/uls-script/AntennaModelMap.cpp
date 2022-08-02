/******************************************************************************************/
/**** FILE: AntennaModelMap.cpp                                                        ****/
/******************************************************************************************/

#include <limits>
#include "AntennaModelMap.h"

/******************************************************************************************/
/**** CONSTRUCTOR: AntennaModelClass::AntennaModelClass                                ****/
/******************************************************************************************/
AntennaModelClass::AntennaModelClass(std::string nameVal, CategoryEnum categoryVal, double diameterMVal, double midbandGainVal) :
	name(nameVal), category(categoryVal), diameterM(diameterMVal), midbandGain(midbandGainVal)
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** STATIC FUNCTION: AntennaModelClass::categoryStr()                             ****/
/******************************************************************************************/
std::string AntennaModelClass::categoryStr(CategoryEnum categoryVal)
{
	std::string str;

	switch(categoryVal) {
		case B1Category:
			str = "B1";
			break;
		case HPCategory:
			str = "HP";
			break;
		case OtherCategory:
			str = "OTHER";
			break;
		case UnknownCategory:
			str = "UNKNOWN";
			break;
		default:
			CORE_DUMP;
			break;
	}

    return(str);
}
/******************************************************************************************/

/******************************************************************************************/
/**** CONSTRUCTOR: AntennaModelMapClass::AntennaModelMapClass                          ****/
/******************************************************************************************/
AntennaModelMapClass::AntennaModelMapClass(std::string antModelListFile, std::string antModelMapFile)
{
	readModelList(antModelListFile);
	readModelMap(antModelMapFile);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AntennaModelMapClass::readModelList()                                  ****/
/******************************************************************************************/
void AntennaModelMapClass::readModelList(const std::string filename)
{
	int linenum, fIdx;
	std::string line, strval;
	char *chptr;
	FILE *fp = (FILE *) NULL;
	std::string str;
	std::string reasonIgnored;
	std::ostringstream errStr;

	int modelNameFieldIdx = -1;
	int categoryFieldIdx = -1;
	int diameterFieldIdx = -1;
	int midbandGainFieldIdx = -1;

	std::vector<int *> fieldIdxList;                       std::vector<std::string> fieldLabelList;
	fieldIdxList.push_back(&modelNameFieldIdx);            fieldLabelList.push_back("Ant Model");
	fieldIdxList.push_back(&categoryFieldIdx);             fieldLabelList.push_back("Category");
	fieldIdxList.push_back(&diameterFieldIdx);             fieldLabelList.push_back("Diameter (m)");
	fieldIdxList.push_back(&midbandGainFieldIdx);          fieldLabelList.push_back("Midband Gain (dBi)");

	std::string name;
	AntennaModelClass::CategoryEnum category;
    double diameter;
    double midbandGain;

	int fieldIdx;

	if (filename.empty()) {
		throw std::runtime_error("ERROR: No Antenna Model List File specified");
	}

	if ( !(fp = fopen(filename.c_str(), "rb")) ) {
		str = std::string("ERROR: Unable to open Antenna Model List File \"") + filename + std::string("\"\n");
		throw std::runtime_error(str);
	}

	enum LineTypeEnum {
		labelLineType,
		dataLineType,
		ignoreLineType,
		unknownLineType
	};

	LineTypeEnum lineType;

	AntennaModelClass *antennaModel;

	linenum = 0;
	bool foundLabelLine = false;
	while (fgetline(fp, line, false)) {
		linenum++;
		std::vector<std::string> fieldList = splitCSV(line);

		lineType = unknownLineType;
		/**************************************************************************/
		/**** Determine line type                                              ****/
		/**************************************************************************/
		if (fieldList.size() == 0) {
			lineType = ignoreLineType;
		} else {
			fIdx = fieldList[0].find_first_not_of(' ');
			if (fIdx == (int) std::string::npos) {
				if (fieldList.size() == 1) {
					lineType = ignoreLineType;
				}
			} else {
				if (fieldList[0].at(fIdx) == '#') {
					lineType = ignoreLineType;
				}
			}
		}

		if ((lineType == unknownLineType)&&(!foundLabelLine)) {
			lineType = labelLineType;
			foundLabelLine = 1;
		}
		if ((lineType == unknownLineType)&&(foundLabelLine)) {
			lineType = dataLineType;
		}
		/**************************************************************************/

		/**************************************************************************/
		/**** Process Line                                                     ****/
		/**************************************************************************/
		bool found;
		std::string field;
		switch(lineType) {
			case   labelLineType:
				for(fieldIdx=0; fieldIdx<(int) fieldList.size(); fieldIdx++) {
					field = fieldList.at(fieldIdx);

					// std::cout << "FIELD: \"" << field << "\"" << std::endl;

					found = false;
					for(fIdx=0; (fIdx < (int) fieldLabelList.size())&&(!found); fIdx++) {
						if (field == fieldLabelList.at(fIdx)) {
							*fieldIdxList.at(fIdx) = fieldIdx;
							found = true;
						}
					}
				}

				for(fIdx=0; fIdx < (int) fieldIdxList.size(); fIdx++) {
					if (*fieldIdxList.at(fIdx) == -1) {
						errStr << "ERROR: Invalid Antenna Model List file \"" << filename << "\" label line missing \"" << fieldLabelList.at(fIdx) << "\"" << std::endl;
						throw std::runtime_error(errStr.str());
					}
				}

				break;
			case    dataLineType:
				/**************************************************************************/
				/* modelName                                                              */
				/**************************************************************************/
				strval = fieldList.at(modelNameFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Antenna Model List file \"" << filename << "\" line " << linenum << " missing model name" << std::endl;
					throw std::runtime_error(errStr.str());
				}

				name = strval;
				/**************************************************************************/

				/**************************************************************************/
				/* category                                                               */
				/**************************************************************************/
				strval = fieldList.at(categoryFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Antenna Model List file \"" << filename << "\" line " << linenum << " missing category" << std::endl;
					throw std::runtime_error(errStr.str());
				}

				if (strval == "HP") {
					category = AntennaModelClass::HPCategory;
				} else if (strval == "B1") {
					category = AntennaModelClass::B1Category;
				} else if ( (strval == "OTHER") || (strval == "Other") ) {
					category = AntennaModelClass::OtherCategory;
				} else {
					errStr << "ERROR: Antenna Model List file \"" << filename << "\" line " << linenum << " invalid category: " << strval << std::endl;
					throw std::runtime_error(errStr.str());
				}
				/**************************************************************************/

				/**************************************************************************/
				/* diameter                                                               */
				/**************************************************************************/
				strval = fieldList.at(diameterFieldIdx);
				if (strval.empty()) {
                    diameter = -1.0; // Use -1 for unknown
                } else {
                    diameter = std::strtod(strval.c_str(), &chptr);
                    if (diameter <= 0.0) {
					    errStr << "ERROR: Antenna Model List file \"" << filename << "\" line " << linenum << " invalid diameter: \"" << strval << "\"" << std::endl;
					    throw std::runtime_error(errStr.str());
                    }
                    // Use meters in input file, no conversion here
                    // diameter *= 12*2.54*0.01; // convert ft to meters
                }
				/**************************************************************************/

				/**************************************************************************/
				/* midband gain                                                           */
				/**************************************************************************/
				strval = fieldList.at(midbandGainFieldIdx);
				if (strval.empty()) {
                    midbandGain = std::numeric_limits<double>::quiet_NaN();
                } else {
                    midbandGain = std::strtod(strval.c_str(), &chptr);
                }
				/**************************************************************************/

				antennaModel = new AntennaModelClass(name, category, diameter, midbandGain);

				antennaModelList.push_back(antennaModel);

				break;
			case  ignoreLineType:
			case unknownLineType:
				// do nothing
				break;
			default:
				CORE_DUMP;
				break;
		}
	}

	if (fp) { fclose(fp); }

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AntennaModelMapClass::readModelMap()                                   ****/
/******************************************************************************************/
void AntennaModelMapClass::readModelMap(const std::string filename)
{
	int linenum, fIdx;
	std::string line, strval;
	char *chptr;
	FILE *fp = (FILE *) NULL;
	std::string str;
	std::string reasonIgnored;
	std::ostringstream errStr;

	int regexFieldIdx = -1;
	int modelNameFieldIdx = -1;

	std::vector<int *> fieldIdxList;                       std::vector<std::string> fieldLabelList;
	fieldIdxList.push_back(&regexFieldIdx);                fieldLabelList.push_back("regex");
	fieldIdxList.push_back(&modelNameFieldIdx);            fieldLabelList.push_back("Ant Model");

	int i;
	int antIdx;
	std::string name;
	std::string regexStr;
	std::regex *regExpr;

	int fieldIdx;

	if (filename.empty()) {
		throw std::runtime_error("ERROR: No Antenna Model List File specified");
	}

	if ( !(fp = fopen(filename.c_str(), "rb")) ) {
		str = std::string("ERROR: Unable to open Antenna Model List File \"") + filename + std::string("\"\n");
		throw std::runtime_error(str);
	}

	enum LineTypeEnum {
		labelLineType,
		dataLineType,
		ignoreLineType,
		unknownLineType
	};

	LineTypeEnum lineType;

	AntennaModelClass *antennaModel;

	linenum = 0;
	bool foundLabelLine = false;
	while (fgetline(fp, line, false)) {
		linenum++;
		std::vector<std::string> fieldList = splitCSV(line);

		lineType = unknownLineType;
		/**************************************************************************/
		/**** Determine line type                                              ****/
		/**************************************************************************/
		if (fieldList.size() == 0) {
			lineType = ignoreLineType;
		} else {
			fIdx = fieldList[0].find_first_not_of(' ');
			if (fIdx == (int) std::string::npos) {
				if (fieldList.size() == 1) {
					lineType = ignoreLineType;
				}
			} else {
				if (fieldList[0].at(fIdx) == '#') {
					lineType = ignoreLineType;
				}
			}
		}

		if ((lineType == unknownLineType)&&(!foundLabelLine)) {
			lineType = labelLineType;
			foundLabelLine = 1;
		}
		if ((lineType == unknownLineType)&&(foundLabelLine)) {
			lineType = dataLineType;
		}
		/**************************************************************************/

		/**************************************************************************/
		/**** Process Line                                                     ****/
		/**************************************************************************/
		bool found;
		std::string field;
		switch(lineType) {
			case   labelLineType:
				for(fieldIdx=0; fieldIdx<(int) fieldList.size(); fieldIdx++) {
					field = fieldList.at(fieldIdx);

					// std::cout << "FIELD: \"" << field << "\"" << std::endl;

					found = false;
					for(fIdx=0; (fIdx < (int) fieldLabelList.size())&&(!found); fIdx++) {
						if (field == fieldLabelList.at(fIdx)) {
							*fieldIdxList.at(fIdx) = fieldIdx;
							found = true;
						}
					}
				}

				for(fIdx=0; fIdx < (int) fieldIdxList.size(); fIdx++) {
					if (*fieldIdxList.at(fIdx) == -1) {
						errStr << "ERROR: Invalid Antenna Model Map file \"" << filename << "\" label line missing \"" << fieldLabelList.at(fIdx) << "\"" << std::endl;
						throw std::runtime_error(errStr.str());
					}
				}

				break;
			case    dataLineType:
				/**************************************************************************/
				/* regex                                                                  */
				/**************************************************************************/
				strval = fieldList.at(regexFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Antenna Model Map file \"" << filename << "\" line " << linenum << " missing regex" << std::endl;
					throw std::runtime_error(errStr.str());
				}

				regexStr = strval;
				/**************************************************************************/

				/**************************************************************************/
				/* modelName                                                              */
				/**************************************************************************/
				strval = fieldList.at(modelNameFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Antenna Model Map file \"" << filename << "\" line " << linenum << " missing model name" << std::endl;
					throw std::runtime_error(errStr.str());
				}

				name = strval;
				/**************************************************************************/

				regExpr = new std::regex(regexStr, std::regex_constants::icase);

				antIdx = -1;
				found = false;
				for(i=0; (i<antennaModelList.size())&&(!found); ++i) {
					if (antennaModelList[i]->name == name) {
						found = true;
						antIdx = i;
					}
				}
				if (!found) {
					errStr << "ERROR: Antenna Model Map file \"" << filename << "\" line " << linenum << " invalid model name: " << name << std::endl;
					throw std::runtime_error(errStr.str());
				}

				regexList.push_back(regExpr);
				antIdxList.push_back(antIdx);

				break;
			case  ignoreLineType:
			case unknownLineType:
				// do nothing
				break;
			default:
				CORE_DUMP;
				break;
		}
	}

	if (fp) { fclose(fp); }

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AntennaModelMapClass::find()                                           ****/
/******************************************************************************************/
AntennaModelClass *AntennaModelMapClass::find(const std::string modelName)
{
	bool found = false;
	int antIdx;
	int i;

	for(i=0; (i<regexList.size())&&(!found); ++i) {
		if (regex_match(modelName, *regexList[i])) {
			found = true;
			antIdx = antIdxList[i];
		}
	}

	AntennaModelClass *antennaModel;
	if (found) {
		antennaModel = antennaModelList[antIdx];
	} else {
		antennaModel = (AntennaModelClass *) NULL;
	}

	return(antennaModel);
}
/******************************************************************************************/

