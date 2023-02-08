/******************************************************************************************/
/**** FILE: TransmitterModelMap.cpp                                                    ****/
/******************************************************************************************/

#include <limits>
#include <algorithm>
#include "TransmitterModelMap.h"

/******************************************************************************************/
/**** CONSTRUCTOR: TransmitterModelClass::TransmitterModelClass                        ****/
/******************************************************************************************/
TransmitterModelClass::TransmitterModelClass(std::string nameVal) : name(nameVal)
{
	architecture = UnknownArchitecture;
}
/******************************************************************************************/

/******************************************************************************************/
/**** STATIC FUNCTION: TransmitterModelClass::architectureStr()                        ****/
/******************************************************************************************/
std::string TransmitterModelClass::architectureStr(ArchitectureEnum architectureVal)
{
	std::string str;

	switch(architectureVal) {
		case IDUArchitecture:
			str = "IDU";
			break;
		case ODUArchitecture:
			str = "ODU";
			break;
		case UnknownArchitecture:
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
/**** CONSTRUCTOR: TransmitterModelMapClass::TransmitterModelMapClass                  ****/
/******************************************************************************************/
TransmitterModelMapClass::TransmitterModelMapClass(std::string transmitterModelListFile)
{
	readModelList(transmitterModelListFile);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: TransmitterModelMapClass::readModelList()                              ****/
/******************************************************************************************/
void TransmitterModelMapClass::readModelList(const std::string filename)
{
	int linenum, fIdx;
	std::string line, strval;
	char *chptr;
	FILE *fp = (FILE *) NULL;
	std::string str;
	std::string reasonIgnored;
	std::ostringstream errStr;
    int numError = 0;
    int numIgnore = 0;

	int modelNameFieldIdx = -1;
	int architectureFieldIdx = -1;

	std::vector<int *> fieldIdxList;                       std::vector<std::string> fieldLabelList;
	fieldIdxList.push_back(&modelNameFieldIdx);            fieldLabelList.push_back("radioModelPrefix");
	fieldIdxList.push_back(&architectureFieldIdx);         fieldLabelList.push_back("architecture");

	std::string name;
	TransmitterModelClass::ArchitectureEnum architecture;

	int fieldIdx;

	if (filename.empty()) {
		throw std::runtime_error("ERROR: No Transmitter Model List File specified");
	}

	if ( !(fp = fopen(filename.c_str(), "rb")) ) {
		str = std::string("ERROR: Unable to open Transmitter Model List File \"") + filename + std::string("\"\n");
		throw std::runtime_error(str);
	}

	enum LineTypeEnum {
		labelLineType,
		dataLineType,
		ignoreLineType,
		unknownLineType
	};

	LineTypeEnum lineType;

	TransmitterModelClass *transmitterModel;

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
		bool errorFlag;
		bool ignoreFlag;
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
						errStr << "ERROR: Invalid Transmitter Model List file \"" << filename << "\" label line missing \"" << fieldLabelList.at(fIdx) << "\"" << std::endl;
						throw std::runtime_error(errStr.str());
					}
				}

				break;
			case    dataLineType:
                errorFlag = false;
                ignoreFlag = false;

				/**************************************************************************/
				/* modelName                                                              */
				/**************************************************************************/
				strval = fieldList.at(modelNameFieldIdx);
				if (strval.empty()) {
					errStr << "WARNING: Transmitter Model List file \"" << filename << "\" line " << linenum << " missing model name" << std::endl;

					// throw std::runtime_error(errStr.str());

					std::cout << errStr.str();
                    errStr.str(std::string());
                    errorFlag = true;
				}

				name = strval;
				/**************************************************************************/

				/**************************************************************************/
				/* architecture                                                           */
				/**************************************************************************/
                if (!errorFlag) {
				    strval = fieldList.at(architectureFieldIdx);

				    if (strval.empty()) {
					    errStr << "ERROR: Transmitter Model List file \"" << filename << "\" line " << linenum << " missing architecture" << std::endl;

					    // throw std::runtime_error(errStr.str());

					    std::cout << errStr.str();
                        errStr.str(std::string());
                        errorFlag = true;
				    } else if (strval == "IDU") {
					    architecture = TransmitterModelClass::IDUArchitecture;
				    } else if (strval == "ODU") {
					    architecture = TransmitterModelClass::ODUArchitecture;
				    } else if ( (strval == "Unknown") || (strval == "UNKNOWN") ) {
					    architecture = TransmitterModelClass::UnknownArchitecture;
                        ignoreFlag = true;
				    } else {
					    errStr << "ERROR: Transmitter Model List file \"" << filename << "\" line " << linenum << " invalid architecture: " << strval << std::endl;

					    // throw std::runtime_error(errStr.str());

					    std::cout << errStr.str();
                        errorFlag = true;
				    }
                }
				/**************************************************************************/

                if (errorFlag) {
                    numError++;
                } else if (ignoreFlag) {
                    numIgnore++;
                } else {
				    transmitterModel = new TransmitterModelClass(name);
				    transmitterModel->setArchitecture(architecture);

				    transmitterModelList.push_back(transmitterModel);
                }

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

    std::cout << "NUM LINES IGNORED ERROR in " << filename << ": " << numError << std::endl;
    std::cout << "NUM LINES IGNORED ARCHITECTURE UNKNOWN in " << filename << ": " << numIgnore << std::endl;

	return;
}
/******************************************************************************************/

inline bool isInvalidModelNameChar(char c)
{
    // Valid characters are 'A' - 'Z' and '0' - '9'
    bool isLetter = (c >= 'A') && (c <= 'Z');
    bool isNum    = (c >= '0') && (c <= '9');
    bool valid = isLetter || isNum;
    return(!valid);
}


/******************************************************************************************/
/**** FUNCTION: TransmitterModelMapClass::find()                                       ****/
/******************************************************************************************/
TransmitterModelClass *TransmitterModelMapClass::find(std::string modelName)
{
	bool found = false;
	int i;

	TransmitterModelClass *transmitterModel = (TransmitterModelClass *) NULL;

    /**********************************************************************************/
    /* Convert ModelName to uppercase                                                 */
    /**********************************************************************************/
    std::transform(modelName.begin(), modelName.end(), modelName.begin(), ::toupper);
    /**********************************************************************************/

    /**********************************************************************************/
    /* Remove non-alhpanumeric characters                                             */
    /**********************************************************************************/
    modelName.erase(std::remove_if(modelName.begin(), modelName.end(), isInvalidModelNameChar), modelName.end());
    /**********************************************************************************/

    /**********************************************************************************/
    /* Match if an transmitterModelList contains a model that is:                     */
    /*    * prefix of modelName                                                       */
    /**********************************************************************************/
    for(i=0; (i<transmitterModelList.size())&&(!found); ++i) {
        TransmitterModelClass *m = transmitterModelList[i];

        if (modelName.compare(0, m->name.size(), m->name) == 0) {
            found = true;
            transmitterModel = m;
        }
    }
    /**********************************************************************************/

	return(transmitterModel);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: TransmitterModelMapClass::find()                                       ****/
/******************************************************************************************/
int TransmitterModelMapClass::checkPrefixValues()
{
    int ia, ib;
    int numError = 0;

    for(ia=0; ia<transmitterModelList.size(); ++ia) {
        TransmitterModelClass *ma = transmitterModelList[ia];
        for(ib=0; ib<transmitterModelList.size(); ++ib) {
            if (ib != ia) {
                TransmitterModelClass *mb = transmitterModelList[ib];
                if ((ma->architecture != TransmitterModelClass::UnknownArchitecture) && (mb->name.compare(0, ma->name.size(), ma->name) == 0)) {
                    numError++;
                    std::cout << "(" << numError << ") " << ma->name << "[" << TransmitterModelClass::architectureStr(ma->architecture) << "] is a prefix of "
                                                         << mb->name << "[" << TransmitterModelClass::architectureStr(mb->architecture) << "]"
                              << (ma->architecture != mb->architecture ? " DIFFERENT ARCHITECTURE" : "")
                              << std::endl;
                }
            }
        }
    }

    return(numError);
}
/******************************************************************************************/

