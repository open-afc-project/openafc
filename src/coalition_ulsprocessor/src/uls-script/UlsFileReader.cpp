#include <QtCore>
#include <limits>
#include <math.h>
#include <iostream>
#include <sstream>
#include <bsd/string.h>

#include "global_fn.h"
#include "UlsFileReader.h"
#include "UlsFunctions.h"
#include "EcefModel.h"

bool fixMissingRxCallsign = false;

namespace
{
double emptyAtof(const char *str)
{
	char *endptr;
	double z = strtod(str, &endptr);

	if (endptr == str) {
		// No conversion performed, str empty or not numeric
		z = std::numeric_limits<double>::quiet_NaN();
	}

	return z;
}

// On AUG 18, 2022 the FCC modified the format of PA records increasing the number of columns from
// 22 to 24.  The variable maxcol is set to 22, and this function ignores any additional columns
// after maxcol. void SetToNextLine(FILE *fi, char c) {
//   while(c != '\n' && c != EOF) {
//     c = fgetc(fi);
//   }
//
// }
}; // namespace

/**************************************************************************/
/* UlsFileReader::UlsFileReader()                                         */
/**************************************************************************/
UlsFileReader::UlsFileReader(const char *fpath,
			     FILE *fwarn,
			     bool alignFederatedFlag,
			     double alignFederatedScale)
{
	FILE *fi = fopen(fpath, "r");
	std::ostringstream errStr;
	std::string line;

	std::string front;

	enum LineTypeEnum { labelLineType, dataLineType, ignoreLineType, unknownLineType };

	LineTypeEnum lineType;

	int linenum = 0;
	QMap<QString, int> autoCellCounts;

	while (fgetline(fi, line, false)) {
		linenum++;
		std::vector<std::string> fieldList = split(line, '|');

		lineType = unknownLineType;
		/**************************************************************************/
		/**** Determine line type                                              ****/
		/**************************************************************************/
		if (fieldList.size() == 0) {
			lineType = ignoreLineType;
		} else {
			int fIdx = fieldList[0].find_first_not_of(' ');
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

		if (lineType == unknownLineType) {
			lineType = dataLineType;
		}
		/**************************************************************************/

		/**************************************************************************/
		/**** Process Line                                                     ****/
		/**************************************************************************/
		switch (lineType) {
			case labelLineType:
				CORE_DUMP;
				break;
			case dataLineType:
				front = fieldList[0];

				/******************************************************************/
				/* United States Data (US)                                        */
				/******************************************************************/
				if (front == "US:HD") {
					readIndividualHeaderUS(fieldList);
				} else if (front == "US:PA") {
					readIndividualPathUS(fieldList);
				} else if (front == "US:AN") {
					readIndividualAntennaUS(fieldList, fwarn);
				} else if (front == "US:FR") {
					readIndividualFrequencyUS(fieldList, fwarn);
				} else if (front == "US:LO") {
					readIndividualLocationUS(fieldList,
								 alignFederatedFlag,
								 alignFederatedScale);
				} else if (front == "US:EM") {
					readIndividualEmissionUS(fieldList, fwarn);
				} else if (front == "US:EN") {
					readIndividualEntityUS(fieldList);
				} else if (front == "US:MF") {
					readIndividualMarketFrequencyUS(fieldList);
				} else if (front == "US:CP") {
					readIndividualControlPointUS(fieldList);
				} else if (front == "US:SG") {
					readIndividualSegmentUS(fieldList);
				} else if (front == "US:RA") {
					readIndividualRASUS(fieldList);
					/******************************************************************/

					/******************************************************************/
					/* Canada Data (CA) */
					/******************************************************************/
				} else if (front == "CA:SD") {
					readStationDataCA(fieldList,
							  fwarn,
							  alignFederatedFlag,
							  alignFederatedScale);
				} else if (front == "CA:PP") {
					readBackToBackPassiveRepeaterCA(fieldList, fwarn);
				} else if (front == "CA:PR") {
					readReflectorPassiveRepeaterCA(fieldList, fwarn);
				} else if (front == "CA:AP") {
					// SetToNextLine(fi, fgetc(fi));
				} else if (front == "CA:TA") {
					readTransmitterCA(fieldList, fwarn);
					/******************************************************************/

				} else {
					errStr << std::string("ERROR: Unable to process inputFile "
							      "line ")
					       << linenum << ", unrecognized: \"" << front << "\""
					       << std::endl;
					throw std::runtime_error(errStr.str());
				}

				break;
			case ignoreLineType:
			case unknownLineType:
				// do nothing
				break;
			default:
				CORE_DUMP;
				break;
		}
		/**************************************************************************/
	}

	/**************************************************************************/
	/* Create list of authorizationNumbers                                    */
	/**************************************************************************/
	foreach(const StationDataCAClass &station, stations())
	{
		authorizationNumberList.insert(station.authorizationNumber);
	}

	std::cout << "CA: Total " << authorizationNumberList.size() << " authorization numbers"
		  << std::endl;
	/**************************************************************************/

	fclose(fi);
}
/**************************************************************************/

inline bool isInvalidChar(char c)
{
	bool isComma = (c == ',');
	bool isInvalidAscii = (c & 0x80 ? true : false);
	return (isComma || isInvalidAscii);
}

/**************************************************************************/
/* UlsFileReader::readIndividualPathUS()                                  */
/**************************************************************************/
void UlsFileReader::readIndividualPathUS(const std::vector<std::string> &fieldList)
{
	UlsPath current;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 1:
				current.systemId = atoll(field.c_str());
				break;
			case 4:
				strlcpy(current.callsign, field.c_str(), 11);
				break;
			case 6:
				current.pathNumber = atoi(field.c_str());
				break;
			case 7:
				current.txLocationNumber = atoi(field.c_str());
				break;
			case 8:
				current.txAntennaNumber = atoi(field.c_str());
				break;
			case 9:
				current.rxLocationNumber = atoi(field.c_str());
				break;
			case 10:
				current.rxAntennaNumber = atoi(field.c_str());
				break;
			case 12:
				strlcpy(current.pathType, field.c_str(), 21);
				break;
			case 13:
				current.passiveReceiver = field.c_str()[0];
				break;
			case 14:
				strlcpy(current.countryCode, field.c_str(), 4);
				break;
			case 15:
				current.GSOinterference = field.c_str()[0];
				break;
			case 16:
				strlcpy(current.rxCallsign, field.c_str(), 11);
				break;
			case 17:
				current.angularSeparation = emptyAtof(field.c_str());
				break;
			case 20:
				current.statusCode = field.c_str()[0];
				break;
			case 21:
				strlcpy(current.statusDate, field.c_str(), 11);
				break;
		}
	}

	if (fixMissingRxCallsign) {
		bool replace = false;
		if (strlen(current.rxCallsign) == 0) {
			replace = true;
		} else {
			char buf[11];
			unsigned int i;

			for (i = 0; i < strlen(current.rxCallsign); i++) {
				buf[i] = toupper(current.rxCallsign[i]);
			}
			buf[i] = '\0';

			if (strstr(current.rxCallsign, "NEW") != NULL) {
				replace = true;
			} else {
				bool empty = true;
				for (i = 0; i < strlen(buf); i++) {
					if (isspace(buf[i]) == 0)
						empty = false;
				}
				if (empty == true) {
					replace = true;
				}
			}
		}
		if (replace == true) {
			strlcpy(current.rxCallsign, current.callsign, sizeof(current.rxCallsign));
		}
	}

	allPaths << current;
	pathMap[current.callsign] << current;
	return;
}
/**************************************************************************/

/**************************************************************************/
/* UlsFileReader::readIndividualEmissionUS()                              */
/**************************************************************************/
void UlsFileReader::readIndividualEmissionUS(const std::vector<std::string> &fieldList, FILE *fwarn)
{
	UlsEmission current;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 1:
				current.systemId = atoll(field.c_str());
				break;
			case 4:
				strlcpy(current.callsign, field.c_str(), 11);
				break;
			case 5:
				current.locationId = atoi(field.c_str());
				break;
			case 6:
				current.antennaId = atoi(field.c_str());
				break;
			case 7:
				current.frequency = emptyAtof(field.c_str());
				break;
			case 9:
				strlcpy(current.desig, field.c_str(), 11);
				break;
			case 10:
				current.modRate = emptyAtof(field.c_str());
				break;
			case 11:
				current.modCode = field;
				break;
			case 12:
				current.frequencyId = atoi(field.c_str());
				break;
		}
	}

	std::string origModCode = current.modCode;
	std::string modCode = origModCode;

	modCode.erase(std::remove_if(modCode.begin(), modCode.end(), isInvalidChar), modCode.end());

	if (modCode != origModCode) {
		if (fwarn) {
			fprintf(fwarn, "WARNING: Mod Code \"");
			for (int i = 0; i < (int)origModCode.length(); ++i) {
				char ch = origModCode[i];
				if (isInvalidChar(ch)) {
					fprintf(fwarn, "\\x%2X", (unsigned char)ch);
				} else {
					fprintf(fwarn, "%c", ch);
				}
			}
			fprintf(fwarn,
				"\" contains invalid characters, replaced with \"%s\"\n",
				modCode.c_str());
		}
		current.modCode = modCode;
	}

	allEmissions << current;
	emissionMap[current.callsign] << current;

	return;
}
/**************************************************************************/

/**************************************************************************/
/* UlsFileReader::readIndividualMarketFrequencyUS()                       */
/**************************************************************************/
void UlsFileReader::readIndividualMarketFrequencyUS(const std::vector<std::string> &fieldList)
{
	UlsMarketFrequency current;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 1:
				current.systemId = atoll(field.c_str());
				break;
			case 4:
				strlcpy(current.callsign, field.c_str(), 11);
				break;
			case 5:
				strlcpy(current.partitionSeq, field.c_str(), 7);
				break;
			case 6:
				current.lowerFreq = emptyAtof(field.c_str());
				break;
			case 7:
				current.upperFreq = emptyAtof(field.c_str());
				break;
		}
	}

	allMarketFrequencies << current;

	return;
}
/**************************************************************************/

/**************************************************************************/
/* UlsFileReader::readIndividualEntityUS()                                */
/**************************************************************************/
void UlsFileReader::readIndividualEntityUS(const std::vector<std::string> &fieldList)
{
	UlsEntity current;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 1:
				current.systemId = atoll(field.c_str());
				break;
			case 4:
				strlcpy(current.callsign, field.c_str(), 11);
				break;
			case 5:
				strlcpy(current.entityType, field.c_str(), 3);
				break;
			case 6:
				strlcpy(current.licenseeId, field.c_str(), 10);
				break;
			case 7:
				strlcpy(current.entityName, field.c_str(), 201);
				break;
			case 22:
				strlcpy(current.frn, field.c_str(), 11);
				break;
		}
	}

	allEntities << current;
	entityMap[current.callsign] << current;

	return;
}
/**************************************************************************/

/**************************************************************************/
/* UlsFileReader::readIndividualLocationUS()                              */
/**************************************************************************/
void UlsFileReader::readIndividualLocationUS(const std::vector<std::string> &fieldList,
					     bool alignFederatedFlag,
					     double alignFederatedScale)
{
	UlsLocation current;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 1:
				current.systemId = atoll(field.c_str());
				break;
			case 4:
				strlcpy(current.callsign, field.c_str(), 11);
				break;

			case 5:
				current.locationAction = field.c_str()[0];
				break;
			case 6:
				current.locationType = field.c_str()[0];
				break;
			case 7:
				current.locationClass = field.c_str()[0];
				break;
			case 8:
				current.locationNumber = atoi(field.c_str());
				break;
			case 9:
				current.siteStatus = field.c_str()[0];
				break;
			case 10:
				current.correspondingFixedLocation = atoi(field.c_str());
				break;
			case 11:
				strlcpy(current.locationAddress, field.c_str(), 81);
				break;
			case 12:
				strlcpy(current.locationCity, field.c_str(), 21);
				break;
			case 13:
				strlcpy(current.locationCounty, field.c_str(), 61);
				break;
			case 14:
				strlcpy(current.locationState, field.c_str(), 3);
				break;
			case 15:
				current.radius = emptyAtof(field.c_str());
				break;
			case 16:
				current.areaOperationCode = field.c_str()[0];
				break;
			case 17:
				current.clearanceIndication = field.c_str()[0];
				break;
			case 18:
				current.groundElevation = emptyAtof(field.c_str());
				break;
			case 19:
				current.latitude = atoi(field.c_str());
				current.latitudeDeg = atoi(field.c_str());
				break;
			case 20:
				current.latitude = current.latitude + atoi(field.c_str()) / 60.0;
				current.latitudeMinutes = atoi(field.c_str());
				break;
			case 21:
				current.latitude = current.latitude +
						   emptyAtof(field.c_str()) / 3600.0;
				current.latitudeSeconds = emptyAtof(field.c_str());
				break;
			case 22:
				if (field.c_str()[0] == 'S') {
					current.latitude = current.latitude * -1;
				}
				current.latitudeDirection = field.c_str()[0];
				break;
			case 23:
				current.longitude = emptyAtof(field.c_str());
				current.longitudeDeg = atoi(field.c_str());
				break;
			case 24:
				current.longitude = current.longitude +
						    emptyAtof(field.c_str()) / 60.0;
				current.longitudeMinutes = atoi(field.c_str());
				break;
			case 25:
				current.longitude = current.longitude +
						    emptyAtof(field.c_str()) / 3600.0;
				current.longitudeSeconds = atoi(field.c_str());
				break;
			case 26:
				if (field.c_str()[0] == 'W') {
					current.longitude = current.longitude * -1;
				}
				current.longitudeDirection = field.c_str()[0];
				break;
			case 35:
				current.nepa = field.c_str()[0];
				break;
			case 38:
				current.supportHeight = emptyAtof(field.c_str());
				break;
			case 39:
				current.overallHeight = emptyAtof(field.c_str());
				break;
			case 40:
				strlcpy(current.structureType, field.c_str(), 7);
				break;
			case 41:
				strlcpy(current.airportId, field.c_str(), 5);
				break;
			case 42:
				strlcpy(current.locationName, field.c_str(), 21);
				break;
			case 48:
				current.statusCode = field.c_str()[0];
				break;
			case 49:
				strlcpy(current.statusDate, field.c_str(), 11);
				break;
			case 50:
				current.earthStationAgreement = field.c_str()[0];
				break;
		}
	}

	if (alignFederatedFlag) {
		current.longitude = std::floor(current.longitude * alignFederatedScale + 0.5) /
				    alignFederatedScale;
		current.latitude = std::floor(current.latitude * alignFederatedScale + 0.5) /
				   alignFederatedScale;
	}

	allLocations << current;
	locationMap[current.callsign] << current;

	return;
}
/**************************************************************************/

/**************************************************************************/
/* UlsFileReader::readIndividualAntennaUS()                               */
/**************************************************************************/
void UlsFileReader::readIndividualAntennaUS(const std::vector<std::string> &fieldList, FILE *fwarn)
{
	UlsAntenna current;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 1:
				current.systemId = atoll(field.c_str());
				break;
			case 4:
				strlcpy(current.callsign, field.c_str(), 11);
				break;

			case 6:
				current.antennaNumber = atoi(field.c_str());
				break;
			case 7:
				current.locationNumber = atoi(field.c_str());
				break;
			case 8:
				strlcpy(current.recvZoneCode, field.c_str(), 6);
				break;
			case 9:
				current.antennaType = field.c_str()[0];
				break;
			case 10:
				current.heightToTip = emptyAtof(field.c_str());
				break;
			case 11:
				current.heightToCenterRAAT = emptyAtof(field.c_str());
				break;
			case 12:
				current.antennaMake = field;
				break;
			case 13:
				current.antennaModel = field;
				break;
			case 14:
				current.tilt = emptyAtof(field.c_str());
				break;
			case 15:
				strlcpy(current.polarizationCode, field.c_str(), 5);
				break;
			case 16:
				current.beamwidth = emptyAtof(field.c_str());
				break;
			case 17:
				current.gain = emptyAtof(field.c_str());
				break;
			case 18:
				current.azimuth = emptyAtof(field.c_str());
				break;
			case 19:
				current.heightAboveAverageTerrain = emptyAtof(field.c_str());
				break;
			case 20:
				current.diversityHeight = emptyAtof(field.c_str());
				break;
			case 21:
				current.diversityGain = emptyAtof(field.c_str());
				break;
			case 22:
				current.diversityBeam = emptyAtof(field.c_str());
				break;
			case 23:
				current.reflectorHeight = emptyAtof(field.c_str());
				break;
			case 24:
				current.reflectorWidth = emptyAtof(field.c_str());
				break;
			case 25:
				current.reflectorSeparation = emptyAtof(field.c_str());
				break;
			case 26:
				current.passiveRepeaterNumber = atoi(field.c_str());
				break;
			case 27:
				current.backtobackTxGain = emptyAtof(field.c_str());
				break;
			case 28:
				current.backtobackRxGain = emptyAtof(field.c_str());
				break;
			case 29:
				strlcpy(current.locationName, field.c_str(), 20);
				break;
			case 30:
				current.passiveRepeaterSequenceId = atoi(field.c_str());
				break;
			case 31:
				current.alternativeCGSA = field.c_str()[0];
				break;
			case 32:
				current.pathNumber = atoi(field.c_str());
				break;
			case 33:
				current.lineLoss = emptyAtof(field.c_str());
				break;

			case 34:
				current.statusCode = field.c_str()[0];
				break;
			case 35:
				strlcpy(current.statusDate, field.c_str(), 11);
				break;
		}
	}

	std::string origAntennaModel = current.antennaModel;
	std::string antennaModel = origAntennaModel;

	antennaModel.erase(std::remove_if(antennaModel.begin(), antennaModel.end(), isInvalidChar),
			   antennaModel.end());

	if (antennaModel != origAntennaModel) {
		if (fwarn) {
			fprintf(fwarn, "WARNING: Antenna model \"");
			for (int i = 0; i < (int)origAntennaModel.length(); ++i) {
				char ch = origAntennaModel[i];
				if (isInvalidChar(ch)) {
					fprintf(fwarn, "\\x%2X", (unsigned char)ch);
				} else {
					fprintf(fwarn, "%c", ch);
				}
			}
			fprintf(fwarn,
				"\" contains invalid characters, replaced with \"%s\"\n",
				antennaModel.c_str());
		}
		current.antennaModel = antennaModel;
	}

	allAntennas << current;
	antennaMap[current.callsign] << current;

	return;
}
/**************************************************************************/

/**************************************************************************/
/* UlsFileReader::readIndividualFrequencyUS()                             */
/**************************************************************************/
void UlsFileReader::readIndividualFrequencyUS(const std::vector<std::string> &fieldList,
					      FILE *fwarn)
{
	UlsFrequency current;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 1:
				current.systemId = atoll(field.c_str());
				break;
			case 4:
				strlcpy(current.callsign, field.c_str(), 11);
				break;

			case 6:
				current.locationNumber = atoi(field.c_str());
				break;
			case 7:
				current.antennaNumber = atoi(field.c_str());
				break;

			case 8:
				strlcpy(current.classStationCode, field.c_str(), 5);
				break;
			case 9:
				strlcpy(current.opAltitudeCode, field.c_str(), 3);
				break;
			case 10:
				current.frequencyAssigned = emptyAtof(field.c_str());
				break;
			case 11:
				current.frequencyUpperBand = emptyAtof(field.c_str());
				break;
			case 12:
				current.frequencyCarrier = emptyAtof(field.c_str());
				break;
			case 13:
				current.timeBeginOperations = atoi(field.c_str());
				break;
			case 14:
				current.timeEndOperations = atoi(field.c_str());
				break;
			case 15:
				current.powerOutput = emptyAtof(field.c_str());
				break;
			case 16:
				current.powerERP = emptyAtof(field.c_str());
				break;
			case 17:
				current.tolerance = emptyAtof(field.c_str());
				break;
			case 18:
				current.frequencyIndicator = field.c_str()[0];
				break;
			case 19:
				current.status = field.c_str()[0];
				break;
			case 20:
				current.EIRP = emptyAtof(field.c_str());
				break;
			case 21:
				strlcpy(current.transmitterMake, field.c_str(), 26);
				break;
			case 22:
				strlcpy(current.transmitterModel, field.c_str(), 26);
				break;
			case 23:
				current.transmitterPowerControl = field.c_str()[0];
				break;
			case 24:
				current.numberUnits = atoi(field.c_str());
				break;
			case 25:
				current.numberReceivers = atoi(field.c_str());
				break;
			case 26:
				current.frequencyNumber = atoi(field.c_str());
				break;

			case 27:
				current.statusCode = field.c_str()[0];
				break;
			case 28:
				strlcpy(current.statusDate, field.c_str(), 11);
				break;
		}
	}

	std::string origTransmitterModel = std::string(current.transmitterModel);
	std::string transmitterModel = origTransmitterModel;

	transmitterModel.erase(std::remove_if(transmitterModel.begin(),
					      transmitterModel.end(),
					      isInvalidChar),
			       transmitterModel.end());

	if (transmitterModel != origTransmitterModel) {
		if (fwarn) {
			fprintf(fwarn, "WARNING: Transmitter model \"");
			for (int i = 0; i < (int)origTransmitterModel.length(); ++i) {
				char ch = origTransmitterModel[i];
				if (isInvalidChar(ch)) {
					fprintf(fwarn, "\\x%2X", (unsigned char)ch);
				} else {
					fprintf(fwarn, "%c", ch);
				}
			}
			fprintf(fwarn,
				"\" contains invalid characters, replaced with \"%s\"\n",
				transmitterModel.c_str());
		}
		strlcpy(current.transmitterModel, transmitterModel.c_str(), 26);
	}

	//  allFrequencies should now contain all the antenna records in the original
	//  DB.
	allFrequencies << current;

	return;
}

/**************************************************************************/
/* UlsFileReader::readIndividualHeaderUS()                                */
/**************************************************************************/
void UlsFileReader::readIndividualHeaderUS(const std::vector<std::string> &fieldList)
{
	UlsHeader current;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 1:
				current.systemId = atoll(field.c_str());
				break;
			case 4:
				strlcpy(current.callsign, field.c_str(), 11);
				break;
			case 5:
				current.licenseStatus = field.c_str()[0];
				break;
			case 6:
				strlcpy(current.radioServiceCode, field.c_str(), 3);
				break;
			case 7:
				strlcpy(current.grantDate, field.c_str(), 14);
				break;
			case 8:
				strlcpy(current.expiredDate, field.c_str(), 14);
				break;
			case 21:
				current.commonCarrier = field.c_str()[0];
				break;
			case 22:
				current.nonCommonCarrier = field.c_str()[0];
				break;
			case 23:
				current.privateCarrier = field.c_str()[0];
				break;
			case 24:
				current.fixed = field.c_str()[0];
				break;
			case 25:
				current.mobile = field.c_str()[0];
				break;
			case 26:
				current.radiolocation = field.c_str()[0];
				break;
			case 27:
				current.satellite = field.c_str()[0];
				break;
			case 28:
				current.developmental = field.c_str()[0];
				break;
			case 29:
				current.interconnected = field.c_str()[0];
				break;
			case 42:
				strlcpy(current.effectiveDate, field.c_str(), 14);
				break;
		}
	}

	//  allHeaders should now contain all the header records in the original DB.
	allHeaders << current;
	headerMap[current.callsign] << current;

	return;
}
/**************************************************************************/

/**************************************************************************/
/* UlsFileReader::readIndividualControlPointUS()                          */
/**************************************************************************/
void UlsFileReader::readIndividualControlPointUS(const std::vector<std::string> &fieldList)
{
	UlsControlPoint current;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 1:
				current.systemId = atoll(field.c_str());
				break;
			case 4:
				strlcpy(current.callsign, field.c_str(), 11);
				break;
			case 5:
				current.controlPointActionPerformed = field.c_str()[0];
				break;
			case 6:
				current.controlPointNumber = atoi(field.c_str());
				break;
			case 7:
				strlcpy(current.controlPointAddress, field.c_str(), 81);
				break;
			case 8:
				strlcpy(current.controlPointCity, field.c_str(), 21);
				break;
			case 9:
				strlcpy(current.controlPointState, field.c_str(), 3);
				break;
			case 10:
				strlcpy(current.controlPointPhone, field.c_str(), 11);
				break;
			case 11:
				strlcpy(current.controlPointCounty, field.c_str(), 61);
				break;
			case 12:
				strlcpy(current.controlPointStatus, field.c_str(), 1);
				break;
			case 13:
				strlcpy(current.controlPointStatusDate, field.c_str(), 14);
				break;
		}
	}

	allControlPoints << current;
	controlPointMap[current.callsign] << current;

	return;
}
/**************************************************************************/

/**************************************************************************/
/* UlsFileReader::readIndividualSegmentUS()                               */
/**************************************************************************/
void UlsFileReader::readIndividualSegmentUS(const std::vector<std::string> &fieldList)
{
	UlsSegment current;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 1:
				current.systemId = atoll(field.c_str());
				break;
			case 4:
				strlcpy(current.callsign, field.c_str(), 11);
				break;
			case 6:
				current.pathNumber = atoi(field.c_str());
				break;
			case 7:
				current.txLocationId = atoi(field.c_str());
				break;
			case 8:
				current.txAntennaId = atoi(field.c_str());
				break;
			case 9:
				current.rxLocationId = atoi(field.c_str());
				break;
			case 10:
				current.rxAntennaId = atoi(field.c_str());
				break;
			case 11:
				current.segmentNumber = atoi(field.c_str());
				break;
			case 12:
				current.segmentLength = emptyAtof(field.c_str());
				break;
		}
	}

	allSegments << current;
	segmentMap[current.callsign] << current;

	return;
}
/**************************************************************************/

/**************************************************************************/
/* UlsFileReader::readIndividualRASUS()                                   */
/**************************************************************************/
void UlsFileReader::readIndividualRASUS(const std::vector<std::string> &fieldList)
{
	RASClass current;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 1:
				current.name = field;
				break;
			case 2:
				current.location = field;
				break;
			case 3:
				current.startFreqMHz = emptyAtof(field.c_str());
				break;
			case 4:
				current.stopFreqMHz = emptyAtof(field.c_str());
				break;
			case 5:
				current.exclusionZone = field;
				break;
			case 6:
				current.rect1lat1 = UlsFunctionsClass::getAngleFromDMS(
					field.c_str());
				break;
			case 7:
				current.rect1lat2 = UlsFunctionsClass::getAngleFromDMS(
					field.c_str());
				break;
			case 8:
				current.rect1lon1 = UlsFunctionsClass::getAngleFromDMS(
					field.c_str());
				break;
			case 9:
				current.rect1lon2 = UlsFunctionsClass::getAngleFromDMS(
					field.c_str());
				break;
			case 10:
				current.rect2lat1 = UlsFunctionsClass::getAngleFromDMS(
					field.c_str());
				break;
			case 11:
				current.rect2lat2 = UlsFunctionsClass::getAngleFromDMS(
					field.c_str());
				break;
			case 12:
				current.rect2lon1 = UlsFunctionsClass::getAngleFromDMS(
					field.c_str());
				break;
			case 13:
				current.rect2lon2 = UlsFunctionsClass::getAngleFromDMS(
					field.c_str());
				break;

			case 14:
				current.radiusKm = emptyAtof(field.c_str());
				break;
			case 15:
				current.centerLat = UlsFunctionsClass::getAngleFromDMS(
					field.c_str());
				break;
			case 16:
				current.centerLon = UlsFunctionsClass::getAngleFromDMS(
					field.c_str());
				break;
			case 17:
				current.heightAGL = emptyAtof(field.c_str());
				break;
		}
	}

	current.region = "US";

	RASList << current;

	return;
}
/**************************************************************************/

/**************************************************************************/
/* UlsFileReader::readStationDataCA()                                     */
/**************************************************************************/
void UlsFileReader::readStationDataCA(const std::vector<std::string> &fieldList,
				      FILE *fwarn,
				      bool alignFederatedFlag,
				      double alignFederatedScale)
{
	StationDataCAClass current;

	std::string linceseeName;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 1:
				current.service = atoi(field.c_str());
				break;
			case 2:
				current.subService = atoi(field.c_str());
				break;
			case 3:
				current.authorizationNumber = field;
				break;
			case 4:
				linceseeName = field;
				break;
			case 6:
				current.callsign = field;
				break;
			case 7:
				current.stationLocation = field;
				break;
			case 9:
				current.latitudeDeg = emptyAtof(field.c_str());
				break;
			case 10:
				current.longitudeDeg = emptyAtof(field.c_str());
				break;
			case 11:
				current.groundElevation = emptyAtof(field.c_str());
				break;
			case 13:
				current.antennaHeightAGL = emptyAtof(field.c_str());
				break;
			case 17:
				current.emissionsDesignator = field;
				break;
			case 18:
				current.bandwidthMHz = emptyAtof(field.c_str()) / 1000.0;
				break;
			case 19:
				current.centerFreqMHz = emptyAtof(field.c_str());
				break;
			case 20:
				current.antennaGain = emptyAtof(field.c_str());
				break;
			case 21:
				current.lineLoss = emptyAtof(field.c_str());
				break;
			case 23:
				current.antennaManufacturer = field;
				break;
			case 24:
				current.antennaModel = field;
				break;
			case 25:
				current.inServiceDate = field;
				break;
			case 26:
				current.modulation = field;
				break;
			case 14:
				current.azimuthPtg = emptyAtof(field.c_str());
				break;
			case 15:
				current.elevationPtg = emptyAtof(field.c_str());
				break;
		}
	}

	if (alignFederatedFlag) {
		current.longitudeDeg = std::floor(current.longitudeDeg * alignFederatedScale +
						  0.5) /
				       alignFederatedScale;
		current.latitudeDeg = std::floor(current.latitudeDeg * alignFederatedScale + 0.5) /
				      alignFederatedScale;
	}

	if (isnan(current.antennaHeightAGL)) {
		current.antennaHeightAGL = 56.0;
	} else if (current.antennaHeightAGL < 1.5) {
		current.antennaHeightAGL = 1.5;
	}

	double heightAMSL_km = (current.groundElevation + current.antennaHeightAGL) / 1000.0;
	current.position = EcefModel::geodeticToEcef(current.latitudeDeg,
						     current.longitudeDeg,
						     heightAMSL_km);

	current.pointingVec = UlsFunctionsClass::computeHPointingVec(current.position,
								     current.azimuthPtg,
								     current.elevationPtg);

	std::string origAntennaModel = std::string(current.antennaModel);
	std::string antennaModel = origAntennaModel;

	antennaModel.erase(std::remove_if(antennaModel.begin(), antennaModel.end(), isInvalidChar),
			   antennaModel.end());

	if (antennaModel != origAntennaModel) {
		if (fwarn) {
			fprintf(fwarn, "WARNING: Antenna model \"");
			for (int i = 0; i < (int)origAntennaModel.length(); ++i) {
				char ch = origAntennaModel[i];
				if (isInvalidChar(ch)) {
					fprintf(fwarn, "\\x%2X", (unsigned char)ch);
				} else {
					fprintf(fwarn, "%c", ch);
				}
			}
			fprintf(fwarn,
				"\" contains invalid characters, replaced with \"%s\"\n",
				antennaModel.c_str());
		}
		current.antennaModel = antennaModel;
	}

	// R1-AIP-19-CAN
	if ((isnan(current.bandwidthMHz)) || (current.bandwidthMHz == 0.0)) {
		current.bandwidthMHz = UlsFunctionsClass::emissionDesignatorToBandwidth(
			QString::fromStdString(current.emissionsDesignator));
	}

	if (isnan(current.bandwidthMHz)) {
		if (current.centerFreqMHz < 5925.0) {
			// Do nothing
		} else if (current.centerFreqMHz < 5955.0) {
			current.bandwidthMHz = 2 * (current.centerFreqMHz - 5925.0);
		} else if (current.centerFreqMHz < 6395.0) {
			current.bandwidthMHz = 60;
		} else if (current.centerFreqMHz < 6425.0) {
			current.bandwidthMHz = 2 * (6425.0 - current.centerFreqMHz);
		} else if (current.centerFreqMHz < 6440.0) {
			current.bandwidthMHz = 2 * (current.centerFreqMHz - 6425.0);
		} else if (current.centerFreqMHz < 6860.0) {
			current.bandwidthMHz = 30;
		} else if (current.centerFreqMHz < 6875.0) {
			current.bandwidthMHz = 2 * (6875.0 - current.centerFreqMHz);
		}
	}

	if (current.service == 9) {
		RASClass ras;
		ras.region = "CA";
		ras.name = linceseeName;
		ras.location = current.stationLocation;
		ras.startFreqMHz = current.centerFreqMHz - current.bandwidthMHz / 2;
		ras.stopFreqMHz = current.centerFreqMHz + current.bandwidthMHz / 2;
		ras.exclusionZone = "Horizon Distance";
		ras.rect1lat1 = std::numeric_limits<double>::quiet_NaN();
		ras.rect1lat2 = std::numeric_limits<double>::quiet_NaN();
		ras.rect1lon1 = std::numeric_limits<double>::quiet_NaN();
		ras.rect1lon2 = std::numeric_limits<double>::quiet_NaN();
		ras.rect2lat1 = std::numeric_limits<double>::quiet_NaN();
		ras.rect2lat2 = std::numeric_limits<double>::quiet_NaN();
		ras.rect2lon1 = std::numeric_limits<double>::quiet_NaN();
		ras.rect2lon2 = std::numeric_limits<double>::quiet_NaN();
		ras.radiusKm = std::numeric_limits<double>::quiet_NaN();
		ras.centerLat = current.latitudeDeg;
		ras.centerLon = current.longitudeDeg;
		ras.heightAGL = current.antennaHeightAGL;
		RASList << ras;
	} else {
		allStations << current;
		stationMap[current.authorizationNumber.c_str()] << current;
	}

	return;
}
/**************************************************************************/

/**************************************************************************/
/* UlsFileReader::readBackToBackPassiveRepeaterCA()                       */
/**************************************************************************/
void UlsFileReader::readBackToBackPassiveRepeaterCA(const std::vector<std::string> &fieldList,
						    FILE *fwarn)
{
	BackToBackPassiveRepeaterCAClass current;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 2:
				current.authorizationNumber = field;
				break;
			case 6:
				current.latitudeDeg = emptyAtof(field.c_str());
				break;
			case 7:
				current.longitudeDeg = emptyAtof(field.c_str());
				break;
			case 8:
				current.groundElevation = emptyAtof(field.c_str());
				break;
			case 9:
				current.heightAGL = emptyAtof(field.c_str());
				break;
			case 10:
				current.antennaGain = emptyAtof(field.c_str());
				break;
			case 11:
				current.antennaModel = field;
				break;
			case 3:
				current.azimuthPtg = emptyAtof(field.c_str());
				break;
			case 4:
				current.elevationPtg = emptyAtof(field.c_str());
				break;
		}
	}

	std::string origAntennaModel = std::string(current.antennaModel);
	std::string antennaModel = origAntennaModel;

	antennaModel.erase(std::remove_if(antennaModel.begin(), antennaModel.end(), isInvalidChar),
			   antennaModel.end());

	if (antennaModel != origAntennaModel) {
		if (fwarn) {
			fprintf(fwarn, "WARNING: Antenna model \"");
			for (int i = 0; i < (int)origAntennaModel.length(); ++i) {
				char ch = origAntennaModel[i];
				if (isInvalidChar(ch)) {
					fprintf(fwarn, "\\x%2X", (unsigned char)ch);
				} else {
					fprintf(fwarn, "%c", ch);
				}
			}
			fprintf(fwarn,
				"\" contains invalid characters, replaced with \"%s\"\n",
				antennaModel.c_str());
		}
		current.antennaModel = antennaModel;
	}

	allBackToBackPassiveRepeaters << current;
	backToBackPassiveRepeaterMap[current.authorizationNumber.c_str()] << current;
	return;
}
/**************************************************************************/

/**************************************************************************/
/* UlsFileReader::readReflectorPassiveRepeaterCA()                        */
/**************************************************************************/
void UlsFileReader::readReflectorPassiveRepeaterCA(const std::vector<std::string> &fieldList,
						   FILE * /* fwarn */)
{
	ReflectorPassiveRepeaterCAClass current;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 2:
				current.authorizationNumber = field;
				break;
			case 4:
				current.latitudeDeg = emptyAtof(field.c_str());
				break;
			case 5:
				current.longitudeDeg = emptyAtof(field.c_str());
				break;
			case 6:
				current.groundElevation = emptyAtof(field.c_str());
				break;
			case 7:
				current.heightAGL = emptyAtof(field.c_str());
				break;
			case 8:
				current.azimuthPtg = emptyAtof(field.c_str());
				break;
			case 9:
				current.elevationPtg = emptyAtof(field.c_str());
				break;
			case 10:
				current.reflectorHeight = emptyAtof(field.c_str());
				break;
			case 11:
				current.reflectorWidth = emptyAtof(field.c_str());
				break;
		}
	}

	if (isnan(current.reflectorHeight) || isnan(current.reflectorWidth)) {
		current.reflectorHeight = 7.32;
		current.reflectorWidth = 9.14;
	}

	allReflectorPassiveRepeaters << current;
	reflectorPassiveRepeaterMap[current.authorizationNumber.c_str()] << current;

	return;
}
/**************************************************************************/

/**************************************************************************/
/* UlsFileReader::readTransmitterCA()                                     */
/**************************************************************************/
void UlsFileReader::readTransmitterCA(const std::vector<std::string> &fieldList, FILE *fwarn)
{
	TransmitterCAClass current;

	for (int fieldIdx = 0; fieldIdx < (int)fieldList.size(); ++fieldIdx) {
		std::string field = fieldList[fieldIdx];

		switch (fieldIdx) {
			case 49:
				current.service = atoi(field.c_str());
				break;
			case 50:
				current.subService = atoi(field.c_str());
				break;
			case 48:
				current.authorizationNumber = field;
				break;
			case 34:
				current.callsign = field;
				break;
			case 41:
				current.latitudeDeg = emptyAtof(field.c_str());
				break;
			case 42:
				current.longitudeDeg = emptyAtof(field.c_str());
				break;
			case 43:
				current.groundElevation = emptyAtof(field.c_str());
				break;
			case 29:
				current.antennaHeightAGL = emptyAtof(field.c_str());
				break;
#if 0
// Not implementing at this time.  Ideally ISED would make these fields available in the StationData file.
            case 12:
                current.emissionsDesignator = field;
                break;
            case 11:
                current.bandwidthMHz = emptyAtof(field.c_str())/1000.0;
                break;
            case 2:
                current.centerFreqMHz = emptyAtof(field.c_str());
                break;
            case 24:
                current.antennaGain = emptyAtof(field.c_str());
                break;
            case 23:
                current.antennaModel = field;
                break;
            case 13:
                current.modulation = field;
                break;
            case 19:
                current.modRate = emptyAtof(field.c_str());
                break;
#endif
		}
	}

	std::string origAntennaModel = std::string(current.antennaModel);
	std::string antennaModel = origAntennaModel;

	antennaModel.erase(std::remove_if(antennaModel.begin(), antennaModel.end(), isInvalidChar),
			   antennaModel.end());

	if (antennaModel != origAntennaModel) {
		if (fwarn) {
			fprintf(fwarn, "WARNING: Antenna model \"");
			for (int i = 0; i < (int)origAntennaModel.length(); ++i) {
				char ch = origAntennaModel[i];
				if (isInvalidChar(ch)) {
					fprintf(fwarn, "\\x%2X", (unsigned char)ch);
				} else {
					fprintf(fwarn, "%c", ch);
				}
			}
			fprintf(fwarn,
				"\" contains invalid characters, replaced with \"%s\"\n",
				antennaModel.c_str());
		}
		current.antennaModel = antennaModel;
	}

	allTransmitters << current;
	transmitterMap[current.authorizationNumber.c_str()] << current;
	return;
}
/**************************************************************************/

/******************************************************************************************/
/**** computeStatisticsUS                                                              ****/
/******************************************************************************************/
int UlsFileReader::computeStatisticsUS(FreqAssignmentClass &freqAssignment,
				       bool includeUnii5,
				       bool includeUnii6,
				       bool includeUnii7,
				       bool includeUnii8)
{
	int n = 0;
	int maxNumSegment;
	std::string maxNumSegmentCallsign = "";

	const std::vector<double> bwMHzListUnii5 =
		{0.4, 0.8, 1.25, 2.5, 3.75, 5.0, 10.0, 30.0, 60.0};

	const std::vector<double> bwMHzListUnii7 = {0.4, 0.8, 1.25, 2.5, 3.75, 5.0, 10.0, 30.0};

	foreach(const UlsFrequency &freq, frequencies())
	{
		UlsPath path;
		bool pathFound = false;

		foreach(const UlsPath &p, pathsMap(freq.callsign))
		{
			if (strcmp(p.callsign, freq.callsign) == 0) {
				if (freq.locationNumber == p.txLocationNumber &&
				    freq.antennaNumber == p.txAntennaNumber) {
					path = p;
					pathFound = true;
					break;
				}
			}
		}

		if (pathFound == false) {
			continue;
		}

		/// Find the emissions information.
		bool txEmFound = false;
		QList<UlsEmission> allTxEm;
		foreach(const UlsEmission &e, emissionsMap(freq.callsign))
		{
			if (strcmp(e.callsign, freq.callsign) == 0 &&
			    e.locationId == freq.locationNumber &&
			    e.antennaId == freq.antennaNumber &&
			    e.frequencyId == freq.frequencyNumber) {
				allTxEm << e;
				txEmFound = true;
			}
		}
		if (!txEmFound) {
			UlsEmission txEm;
			allTxEm << txEm; // Make sure at least one emission.
		}

		/// Find the header.
		UlsHeader txHeader;
		bool txHeaderFound = false;
		foreach(const UlsHeader &h, headersMap(path.callsign))
		{
			if (strcmp(h.callsign, path.callsign) == 0) {
				txHeader = h;
				txHeaderFound = true;
				break;
			}
		}

		if (!txHeaderFound) {
			continue;
		} else if (txHeader.licenseStatus != 'A' && txHeader.licenseStatus != 'L') {
			continue;
		}

		// std::cout << freq.callsign << ": " << allTxEm.size() << " emissions" <<
		// std::endl;
		foreach(const UlsEmission &e, allTxEm)
		{
			bool invalidFlag = false;
			double startFreq = std::numeric_limits<double>::quiet_NaN();
			double stopFreq = std::numeric_limits<double>::quiet_NaN();
			double startFreqBand = std::numeric_limits<double>::quiet_NaN();
			double stopFreqBand = std::numeric_limits<double>::quiet_NaN();
			double bwMHz = std::numeric_limits<double>::quiet_NaN();

			if (isnan(freq.frequencyAssigned)) {
				invalidFlag = true;
			} else {
				if (txEmFound) {
					bwMHz = UlsFunctionsClass::emissionDesignatorToBandwidth(
						e.desig);
				}
				if (isnan(bwMHz) || (bwMHz > 60.0) || (bwMHz == 0)) {
					bwMHz = freqAssignment.getBandwidthUS(
						freq.frequencyAssigned);
				} else {
					bool unii5Flag = (freq.frequencyAssigned >=
							  UlsFunctionsClass::unii5StartFreqMHz) &&
							 (freq.frequencyAssigned <=
							  UlsFunctionsClass::unii5StopFreqMHz);
					bool unii7Flag = (freq.frequencyAssigned >=
							  UlsFunctionsClass::unii7StartFreqMHz) &&
							 (freq.frequencyAssigned <=
							  UlsFunctionsClass::unii7StopFreqMHz);
					const std::vector<double> *fccBWList =
						(std::vector<double> *)NULL;
					if (unii5Flag) {
						fccBWList = &bwMHzListUnii5;
					} else if (unii7Flag) {
						fccBWList = &bwMHzListUnii7;
					}
					if (fccBWList) {
						bool found = false;
						double fccBW;
						for (int i = 0;
						     (i < (int)fccBWList->size()) && (!found);
						     ++i) {
							if (fccBWList->at(i) >= bwMHz) {
								found = true;
								fccBW = fccBWList->at(i);
							}
						}
						if (found) {
							bwMHz = std::min(fccBW, bwMHz * 1.1);
						}
					}
				}

				if ((bwMHz == -1)) {
					invalidFlag = true;
				} else if (isnan(freq.frequencyUpperBand)) {
					startFreq = freq.frequencyAssigned -
						    bwMHz / 2.0; // Lower Band (MHz)
					stopFreq = freq.frequencyAssigned +
						   bwMHz / 2.0; // Upper Band (MHz)
					startFreqBand = startFreq;
					stopFreqBand = stopFreq;
				} else {
					startFreq = freq.frequencyAssigned -
						    bwMHz / 2.0; // Lower Band (MHz)
					stopFreq = freq.frequencyAssigned +
						   bwMHz / 2.0; // Upper Band (MHz)
					startFreqBand = freq.frequencyAssigned; // Lower Band (MHz)
					stopFreqBand = startFreqBand + bwMHz; // Upper Band (MHz)
				}
			}

			if (!invalidFlag) {
				// skip if no overlap UNII5 and 7
				bool overlapUnii5 = (stopFreqBand >
						     UlsFunctionsClass::unii5StartFreqMHz) &&
						    (startFreqBand <
						     UlsFunctionsClass::unii5StopFreqMHz);
				bool overlapUnii6 = (stopFreqBand >
						     UlsFunctionsClass::unii6StartFreqMHz) &&
						    (startFreqBand <
						     UlsFunctionsClass::unii6StopFreqMHz);
				bool overlapUnii7 = (stopFreqBand >
						     UlsFunctionsClass::unii7StartFreqMHz) &&
						    (startFreqBand <
						     UlsFunctionsClass::unii7StopFreqMHz);
				bool overlapUnii8 = (stopFreqBand >
						     UlsFunctionsClass::unii8StartFreqMHz) &&
						    (startFreqBand <
						     UlsFunctionsClass::unii8StopFreqMHz);

				if (!((includeUnii5 && overlapUnii5) ||
				      (includeUnii6 && overlapUnii6) ||
				      (includeUnii7 && overlapUnii7) ||
				      (includeUnii8 && overlapUnii8))) {
					invalidFlag = true;
				}
			}
			if (!invalidFlag) {
				foreach(const UlsSegment &segment, segmentsMap(freq.callsign))
				{
					int segmentNumber = segment.segmentNumber;
					if ((n == 0) || (segmentNumber > maxNumSegment)) {
						maxNumSegment = segmentNumber;
						maxNumSegmentCallsign = std::string(
							segment.callsign);
						n++;
					}
				}
			}
		}
	}

	int maxNumPassiveRepeater = (n ? maxNumSegment - 1 : 0);

	qDebug() << "DATA statistics:";
	qDebug() << "paths" << paths().count();
	qDebug() << "emissions" << emissions().count();
	qDebug() << "antennas" << antennas().count();
	qDebug() << "frequencies" << frequencies().count();
	qDebug() << "locations" << locations().count();
	qDebug() << "headers" << headers().count();
	qDebug() << "market freqs" << marketFrequencies().count();
	qDebug() << "entities" << entities().count();
	qDebug() << "control points" << controlPoints().count();
	qDebug() << "segments" << segments().count();
	qDebug() << "maxNumPassiveRepeater" << maxNumPassiveRepeater
		 << " callsign: " << QString::fromStdString(maxNumSegmentCallsign);

	return (maxNumPassiveRepeater);
}
/******************************************************************************************/

/******************************************************************************************/
/**** computeStatisticsCA                                                              ****/
/******************************************************************************************/
int UlsFileReader::computeStatisticsCA(FILE *fwarn)
{
	int i;
	int maxNumPassiveRepeater = 0;
	int numMatchedBackToBack = 0;
	double epsLonLat = 1.0e-5;
	double epsGroundElevation = 0.05;

	/**************************************************************************************/
	/* CA database contains 2 entries for each back to back passive repeater, 1 entry for */
	/* each antenna.  Here entries are matched.  Entries can be matched if they have the  */
	/* same:                                                                              */
	/*     authorizationNumber                                                            */
	/*     longitude                                                                      */
	/*     latitude                                                                       */
	/* If entries don't match, there is an error in the database, report in warning file. */
	/**************************************************************************************/
	for (std::string authorizationNumber : authorizationNumberList) {
		const QList<BackToBackPassiveRepeaterCAClass> &bbList =
			backToBackPassiveRepeatersMap(authorizationNumber.c_str());
		std::vector<int> idxList;
		idxList.clear();
		for (i = 0; i < bbList.size(); ++i) {
			idxList.push_back(i);
		}

		while (idxList.size()) {
			int iiA, iiMatch;
			iiA = idxList.size() - 1;
			const BackToBackPassiveRepeaterCAClass &bbA = bbList[idxList[iiA]];
			bool found = false;
			for (int iiB = 0; (iiB < iiA) && (!found); ++iiB) {
				const BackToBackPassiveRepeaterCAClass &bbB = bbList[idxList[iiB]];
				if ((fabs(bbA.longitudeDeg - bbB.longitudeDeg) < epsLonLat) &&
				    (fabs(bbA.latitudeDeg - bbB.latitudeDeg) < epsLonLat) &&
				    (fabs(bbA.groundElevation - bbB.groundElevation) <
				     epsGroundElevation)) {
					found = true;
					iiMatch = iiB;
				}
			}
			if (found) {
				const BackToBackPassiveRepeaterCAClass &bbB =
					bbList[idxList[iiMatch]];
				PassiveRepeaterCAClass pr;
				pr.type = PassiveRepeaterCAClass::backToBackAntennaPRType;
				pr.authorizationNumber = authorizationNumber;
				pr.latitudeDeg = bbA.latitudeDeg;
				pr.longitudeDeg = bbA.longitudeDeg;
				pr.groundElevation = bbA.groundElevation;

				pr.heightAGLA = bbA.heightAGL;
				pr.heightAGLB = bbB.heightAGL;
				pr.antennaGainA = bbA.antennaGain;
				pr.antennaGainB = bbB.antennaGain;
				pr.antennaModelA = bbA.antennaModel;
				pr.antennaModelB = bbB.antennaModel;
				pr.azimuthPtgA = bbA.azimuthPtg;
				pr.azimuthPtgB = bbB.azimuthPtg;
				pr.elevationPtgA = bbA.elevationPtg;
				pr.elevationPtgB = bbA.elevationPtg;

				pr.positionA = EcefModel::geodeticToEcef(
					pr.latitudeDeg,
					pr.longitudeDeg,
					(pr.groundElevation + pr.heightAGLA) / 1000.0);
				pr.positionB = EcefModel::geodeticToEcef(
					pr.latitudeDeg,
					pr.longitudeDeg,
					(pr.groundElevation + pr.heightAGLB) / 1000.0);

				pr.reflectorHeight = std::numeric_limits<double>::quiet_NaN();
				pr.reflectorWidth = std::numeric_limits<double>::quiet_NaN();

				passiveRepeaterMap[authorizationNumber.c_str()] << pr;
				if (iiMatch < iiA - 1) {
					idxList[iiMatch] = idxList[iiA - 1];
				}
				idxList.pop_back();
				idxList.pop_back();
				numMatchedBackToBack++;
			} else {
				fprintf(fwarn,
					"UNMATCHED BACK-TO-BACK REPEATER: authorizationNumber: %s, "
					"LON = %.6f, LAT = %.6f\n",
					authorizationNumber.c_str(),
					bbA.longitudeDeg,
					bbA.latitudeDeg);
				idxList.pop_back();
			}
		}

		foreach(const ReflectorPassiveRepeaterCAClass &br,
			reflectorPassiveRepeatersMap(authorizationNumber.c_str()))
		{
			PassiveRepeaterCAClass pr;
			pr.type = PassiveRepeaterCAClass::billboardReflectorPRType;
			pr.authorizationNumber = authorizationNumber;
			pr.latitudeDeg = br.latitudeDeg;
			pr.longitudeDeg = br.longitudeDeg;
			pr.groundElevation = br.groundElevation;

			pr.reflectorHeight = br.reflectorHeight;
			pr.reflectorWidth = br.reflectorWidth;

			pr.heightAGLA = br.heightAGL;
			pr.heightAGLB = br.heightAGL;
			pr.antennaGainA = std::numeric_limits<double>::quiet_NaN();
			pr.antennaGainB = std::numeric_limits<double>::quiet_NaN();
			pr.antennaModelA = "";
			pr.antennaModelB = "";
			pr.azimuthPtgA = std::numeric_limits<double>::quiet_NaN();
			pr.azimuthPtgB = std::numeric_limits<double>::quiet_NaN();
			pr.elevationPtgA = std::numeric_limits<double>::quiet_NaN();
			pr.elevationPtgB = std::numeric_limits<double>::quiet_NaN();

			pr.reflectorPosition = EcefModel::geodeticToEcef(
				pr.latitudeDeg,
				pr.longitudeDeg,
				(pr.groundElevation + pr.heightAGLA) / 1000.0);

			passiveRepeaterMap[authorizationNumber.c_str()] << pr;
		}

		int numPR = passiveRepeaterMap[authorizationNumber.c_str()].size();

		if (numPR > maxNumPassiveRepeater) {
			maxNumPassiveRepeater = numPR;
		}
	}
	/**************************************************************************************/

	std::cout << "CA: Number of matched back-to-back passive repeaters: "
		  << numMatchedBackToBack << std::endl;

	return (maxNumPassiveRepeater);
}
/******************************************************************************************/
