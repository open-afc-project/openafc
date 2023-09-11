/******************************************************************************************/
/**** FILE : prtable.cpp                                                               ****/
/******************************************************************************************/

#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <limits>

#include "prtable.h"
#include "global_fn.h"

/******************************************************************************************/
/**** CONSTRUCTOR: PRTABLEClass::PRTABLEClass()                                        ****/
/******************************************************************************************/
PRTABLEClass::PRTABLEClass()
{
	tableFile = "";
	prTable = (double **) NULL;
	numQ = -1;
	numOneOverKs = -1;
	oneOverKsValList = (double *) NULL;
	QValList = (double *) NULL;
};

PRTABLEClass::PRTABLEClass(std::string tableFileVal) : tableFile(tableFileVal)
{
	readTable();
};
/******************************************************************************************/

/******************************************************************************************/
/**** DESTRUCTOR: PRTABLEClass::~PRTABLEClass()                                        ****/
/******************************************************************************************/
PRTABLEClass::~PRTABLEClass()
{
};
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PRTABLEClass::readTable()                                              ****/
/******************************************************************************************/
void PRTABLEClass::readTable()
{
	string line;
	vector<string> headerList;
	vector<vector<double>> datastore;
	// data structure
	std::ostringstream errStr;

	ifstream file(tableFile);
	if (!file.is_open()) {
		errStr << std::string("ERROR: Unable to open Passive Repeater Table File \"") + tableFile + std::string("\"\n");
		throw std::runtime_error(errStr.str());
	}
	int linenum = 0;

	enum LineTypeEnum {
		labelLineType,
		dataLineType,
		ignoreLineType,
		unknownLineType
	};

	LineTypeEnum lineType;

	bool foundLabelLine = false;
	int kIdx = -1;
	int qIdx, fieldIdx;

	while (getline(file, line))
	{
		linenum++;
		std::vector<std::string> fieldList = splitCSV(line);

		lineType = unknownLineType;
		/**************************************************************************/
		/**** Determine line type                                              ****/
		/**************************************************************************/
		if (fieldList.size() == 0) {
			lineType = ignoreLineType;
		} else { 
			int fIdx = fieldList[0].find_first_not_of(' ');
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
		std::string field;
		switch(lineType) {
			case   labelLineType:
				{
					std::vector<std::string> sizeStrList = split(fieldList[0], ':');
					if (sizeStrList.size() != 2)
					{
						errStr << std::string("ERROR: Passive Repeater Table File ") << tableFile << ":" << linenum
								<< " Ivalid table size " << fieldList[0] << std::endl;
						throw std::runtime_error(errStr.str());
					}
					numQ = std::stoi(sizeStrList[0]);
					numOneOverKs = std::stoi(sizeStrList[1]);
				}
				QValList = (double *) malloc(numQ*sizeof(double));
				oneOverKsValList = (double *) malloc(numOneOverKs*sizeof(double));
				prTable = (double **) malloc(numQ*sizeof(double *));
				for(qIdx=0; qIdx<numQ; ++qIdx) {
					prTable[qIdx] = (double *) malloc(numOneOverKs*sizeof(double));
				}
				if ((int) fieldList.size() != numQ+1) {
					errStr << std::string("ERROR: Passive Repeater Table File ") << tableFile << ":" << linenum << " INVALID DATA\n";
					throw std::runtime_error(errStr.str());
				}

				for(fieldIdx=1; fieldIdx<(int) fieldList.size(); fieldIdx++) {
					field = fieldList.at(fieldIdx);

					QValList[fieldIdx-1] = std::stod(field);

					// std::cout << "FIELD: \"" << field << "\"" << std::endl;
				}
				kIdx = 0;

				break;
			case   dataLineType:
				if (kIdx > numOneOverKs-1) {
					errStr << std::string("ERROR: Passive Repeater Table File ") << tableFile << ":" << linenum << " INVALID DATA\n";
					throw std::runtime_error(errStr.str());
				}

				oneOverKsValList[kIdx] = std::stod(fieldList[0]);

				if (((int) fieldList.size()) != numQ+1) {
					errStr << std::string("ERROR: Passive Repeater Table File ") << tableFile << ":" << linenum << " INVALID DATA\n";
					throw std::runtime_error(errStr.str());
				}

				for(fieldIdx=1; fieldIdx<(int) fieldList.size(); fieldIdx++) {
					field = fieldList.at(fieldIdx);

					prTable[fieldIdx-1][kIdx] = std::stod(field);

					// std::cout << "FIELD: \"" << field << "\"" << std::endl;
				}
				kIdx++;
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
	if (kIdx != numOneOverKs) {
		errStr << std::string("ERROR: Passive Repeater Table File ") << tableFile << ":"
			   << " Read " << kIdx << " lines of data, expecting " << numOneOverKs << std::endl;
		throw std::runtime_error(errStr.str());
	}

	return;
};
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PRTABLEClass::computePRTABLE()                                         ****/
/******************************************************************************************/
double PRTABLEClass::computePRTABLE(double Q, double oneOverKs)
{

	double qIdxDbl = getIdx(Q, QValList, numQ);
	double kIdxDbl = getIdx(oneOverKs, oneOverKsValList, numOneOverKs);

	if (qIdxDbl < 0.0) {
		qIdxDbl = 0.0;
	} else if (qIdxDbl > numQ - 1) {
		qIdxDbl = (double) numQ - 1;
	}

	if (kIdxDbl < 0.0) {
		kIdxDbl = 0.0;
	} else if (kIdxDbl > numOneOverKs - 1) {
		kIdxDbl = (double) numOneOverKs - 1;
	}

	int qIdx0 = (int) floor(qIdxDbl);
	if (qIdx0 == numQ - 1) { qIdx0 = numQ-2; }

	int kIdx0 = (int) floor(kIdxDbl);
	if (kIdx0 == numOneOverKs - 1) { kIdx0 = numOneOverKs-2; }

	double F00 = prTable[qIdx0  ][kIdx0  ];
	double F01 = prTable[qIdx0  ][kIdx0+1];
	double F10 = prTable[qIdx0+1][kIdx0  ];
	double F11 = prTable[qIdx0+1][kIdx0+1];

	double tableVal = F00 * (qIdx0+1 - qIdxDbl) * (kIdx0+1 - kIdxDbl)
			   		+ F01 * (qIdx0+1 - qIdxDbl) * (kIdxDbl - kIdx0  )
			   		+ F10 * (qIdxDbl - qIdx0  ) * (kIdx0+1 - kIdxDbl)
			   		+ F11 * (qIdxDbl - qIdx0  ) * (kIdxDbl - kIdx0  );

	return(tableVal);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PRTABLEClass::getIdx()                                                 ****/
/******************************************************************************************/
double PRTABLEClass::getIdx(double val, double *valList, int numVal)
{
	int i0 = 0;
	int i1 = numVal-1;

	double v0 = valList[i0];
	double v1 = valList[i1];

	if (val <= v0) {
		return(-1.0);
	}
	if (val >= v1) {
		return((double) numVal - 1.0);
	}

	while(i1 > i0+1) {
		int im = (i0+i1)/2;
		double vm = valList[im];
		if (val >= vm) {
			i0 = im;
			v0 = vm;
		} else {
			i1 = im;
			v1 = vm;
		}
	}

	double idxDbl = i0 + (val - v0)/(v1 - v0);

	return(idxDbl);
}
/******************************************************************************************/
