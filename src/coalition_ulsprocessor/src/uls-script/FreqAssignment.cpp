/******************************************************************************************/
/**** FILE: FreqAssignment.cpp                                                         ****/
/******************************************************************************************/

#include <limits>
#include <algorithm>
#include "FreqAssignment.h"

/******************************************************************************************/
/**** CONSTRUCTOR: FreqAssignmentClass::FreqAssignmentClass                            ****/
/******************************************************************************************/
FreqAssignmentClass::FreqAssignmentClass(std::string freqAssignmentFile)
{
	readFreqAssignment(freqAssignmentFile);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: FreqAssignmentClass::readFreqAssignment()                              ****/
/******************************************************************************************/
void FreqAssignmentClass::readFreqAssignment(const std::string filename)
{
	int linenum, fIdx;
	std::string line, strval;
	char *chptr;
	FILE *fp = (FILE *)NULL;
	std::string str;
	std::string reasonIgnored;
	std::ostringstream errStr;

	int frequencyFieldIdx = -1;
	int bandwidthFieldIdx = -1;

	std::vector<int *> fieldIdxList;
	std::vector<std::string> fieldLabelList;
	fieldIdxList.push_back(&frequencyFieldIdx);
	fieldLabelList.push_back("channelFrequency");
	fieldIdxList.push_back(&bandwidthFieldIdx);
	fieldLabelList.push_back("channelBandwidth");

	double frequency;
	double bandwidth;

	int fieldIdx;

	if (filename.empty()) {
		throw std::runtime_error("ERROR: No Frequency Assignment File specified");
	}

	if (!(fp = fopen(filename.c_str(), "rb"))) {
		str = std::string("ERROR: Unable to open Frequency Assignment File \"") + filename +
			std::string("\"\n");
		throw std::runtime_error(str);
	}

	enum LineTypeEnum { labelLineType, dataLineType, ignoreLineType, unknownLineType };

	LineTypeEnum lineType;

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
			if (fIdx == (int)std::string::npos) {
				if (fieldList.size() == 1) {
					lineType = ignoreLineType;
				}
			} else {
				if (fieldList[0].at(fIdx) == '#') {
					lineType = ignoreLineType;
				}
			}
		}

		if ((lineType == unknownLineType) && (!foundLabelLine)) {
			lineType = labelLineType;
			foundLabelLine = 1;
		}
		if ((lineType == unknownLineType) && (foundLabelLine)) {
			lineType = dataLineType;
		}
		/**************************************************************************/

		/**************************************************************************/
		/**** Process Line                                                     ****/
		/**************************************************************************/
		bool found;
		std::string field;
		switch (lineType) {
			case labelLineType:
				for (fieldIdx = 0; fieldIdx < (int)fieldList.size(); fieldIdx++) {
					field = fieldList.at(fieldIdx);

					// std::cout << "FIELD: \"" << field << "\"" << std::endl;

					found = false;
					for (fIdx = 0; (fIdx < (int)fieldLabelList.size()) && (!found); fIdx++) {
						if (field == fieldLabelList.at(fIdx)) {
							*fieldIdxList.at(fIdx) = fieldIdx;
							found = true;
						}
					}
				}

				for (fIdx = 0; fIdx < (int)fieldIdxList.size(); fIdx++) {
					if (*fieldIdxList.at(fIdx) == -1) {
						errStr << "ERROR: Invalid Frequency Assignment "
								  "file \""
							   << filename << "\" label line missing \"" << fieldLabelList.at(fIdx)
							   << "\"" << std::endl;
						throw std::runtime_error(errStr.str());
					}
				}

				break;
			case dataLineType:
				/**************************************************************************/
				/* frequency */
				/**************************************************************************/
				strval = fieldList.at(frequencyFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Frequency Assignment file \"" << filename << "\" line "
						   << linenum << " missing frequency" << std::endl;
					throw std::runtime_error(errStr.str());
				} else {
					frequency = std::strtod(strval.c_str(), &chptr);
					if (frequency <= 0.0) {
						errStr << "ERROR: Frequency Assignment file \"" << filename << "\" line "
							   << linenum << " invalid frequency: \"" << strval << "\""
							   << std::endl;
						throw std::runtime_error(errStr.str());
					}
				}
				/**************************************************************************/

				/**************************************************************************/
				/* bandwidth */
				/**************************************************************************/
				strval = fieldList.at(bandwidthFieldIdx);
				if (strval.empty()) {
					errStr << "ERROR: Frequency Assignment file \"" << filename << "\" line "
						   << linenum << " missing bandwidth" << std::endl;
					throw std::runtime_error(errStr.str());
				} else {
					bandwidth = std::strtod(strval.c_str(), &chptr);
					if (bandwidth <= 0.0) {
						errStr << "ERROR: Frequency Assignment file \"" << filename << "\" line "
							   << linenum << " invalid bandwidth: \"" << strval << "\""
							   << std::endl;
						throw std::runtime_error(errStr.str());
					}
				}
				/**************************************************************************/

				freqBWList.push_back(std::make_tuple(frequency, bandwidth));

				break;
			case ignoreLineType:
			case unknownLineType:
				// do nothing
				break;
			default:
				CORE_DUMP;
				break;
		}
	}

	if (fp) {
		fclose(fp);
	}

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: FreqAssignmentClass::getBandwidthUS                                    ****/
/******************************************************************************************/
double FreqAssignmentClass::getBandwidthUS(double freqMHz)
{
	bool found = false;
	double bandwidth;
	int i;

	// R2-AIP-19 (b), (c)
	for (i = 0; (i < freqBWList.size()) && (!found); ++i) {
		double freq = std::get<0>(freqBWList[i]);
		double bw = std::get<1>(freqBWList[i]);
		if (fabs(freqMHz - freq) <= 0.5) {
			found = true;
			bandwidth = bw;
		}
	}

	if (!found) {
		// R2-AIP-19 (d)
		if (freqMHz < 5925.0) {
			bandwidth = -1.0;
		} else if (freqMHz < 5955.0) {
			bandwidth = 2 * (freqMHz - 5925.0);
		} else if (freqMHz < 6395.0) {
			bandwidth = 60.0;
		} else if (freqMHz < 6425.0) {
			bandwidth = 2 * (6425.0 - freqMHz);
		} else if (freqMHz < 6525.0) {
			bandwidth = -1.0; // UNII-6 not allowed for US
		} else if (freqMHz < 6540.0) {
			bandwidth = 2 * (freqMHz - 6525.0);
		} else if (freqMHz < 6860.0) {
			bandwidth = 30.0;
		} else if (freqMHz < 6875.0) {
			bandwidth = 2 * (6875.0 - freqMHz);
		} else {
			bandwidth = -1.0;
		}
	}

	return (bandwidth);
}
/******************************************************************************************/
