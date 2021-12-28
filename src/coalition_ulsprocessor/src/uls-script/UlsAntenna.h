/*
 *      This data structure stores the data related to an antenna.
 *      There is a field in this for each relevent column in the ULS database, regardless of whether or not it
 *      is populated.
 *
 */

#ifndef ULS_ANTENNA_H
#define ULS_ANTENNA_H

class UlsAntenna
{
public:
	long long systemId;		   // unique identifier for this record; may not be necessary
	char callsign[11];		   // this is the 'key' for the owner of this antenna.
	int antennaNumber;		   // identifier for this antenna
	int locationNumber;		   // location of the antenna. This matches up with a UlsLocation of the same callsign.
	char recvZoneCode[6];	  // marker for the received zone?
	char antennaType;		   // R or T or P
	double heightToTip;		   // Presumably in feet.
	double heightToCenterRAAT; // Likewise.
	char antennaMake[25];	  // Make/model of antenna.
	char antennaModel[25];
	double tilt;
	char polarizationCode[5]; // Many of these properties are unpopulated.
	double beamwidth;
	double gain;
	double azimuth;
	double heightAboveAverageTerrain;
	double diversityHeight;
	double diversityGain;
	double diversityBeam;
	double reflectorHeight;
	double reflectorWidth;
	double reflectorSeparation;
	int passiveRepeaterNumber;
	double backtobackTxGain;
	double backtobackRxGain;
	char locationName[20]; // Used to cross-check against locations.
	int passiveRepeaterSequenceId;
	char alternativeCGSA;
	int pathNumber; // Which microwave path this record is used in. This maps to a UlsPath of the same callsign.
	double lineLoss;
	char statusCode;
	char statusDate[11];

	enum AntennaParameters
	{
		MinAntennaParameter = 0x01000000,
		AntennaSystemId = 0x01000001,
		AntennaCallsign = 0x01000002,
		AntennaNumber = 0x01000003,
		AntennaLocationNumber = 0x01000004,
		AntennaReceiveZoneCode = 0x01000005,
		AntennaType = 0x01000006,
		AntennaHeightToTip = 0x01000007,
		AntennaHeightToCenterRAAT = 0x01000008,
		AntennaMake = 0x01000009,
		AntennaModel = 0x0100000A,
		AntennaTilt = 0x0100000B,
		AntennaPolarizationCode = 0x0100000C,
		AntennaBeamWidth = 0x0100000D,
		AntennaGain = 0x0100000F,
		AntennaAzimuth = 0x01000010,
		AntennaHeightAboveAverageTerrain = 0x01000011,
		AntennaDiversityHeight = 0x01000012,
		AntennaDiversityGain = 0x01000013,
		AntennaDiversityBeam = 0x01000014,
		AntennaReflectorHeight = 0x01000015,
		AntennaReflectorWidth = 0x01000016,
		AntennaReflectorSeparation = 0x01000017,
		AntennaPassiveRepeaterNumber = 0x01000018,
		AntennaBackToBackTxGain = 0x01000019,
		AntennaBackToBackRxGain = 0x0100001a,
		AntennaLocationName = 0x0100001b,
		AntennaPassiveRepeaterSequenceId = 0x0100001c,
		AntennaAlternativeCGSA = 0x0100001d,
		AntennaPathNumber = 0x0100001e,
		AntennaLineLoss = 0x0100001f,
		AntennaStatusCode = 0x01000020,
		AntennaStatusDate = 0x01000021,
		MaxAntennaParameter = 0x01000022,
		ReceiveAntennaSystemId = 0x11000001,
		ReceiveAntennaCallsign = 0x11000002,
		ReceiveAntennaNumber = 0x11000003,
		ReceiveAntennaLocationNumber = 0x11000004,
		ReceiveAntennaReceiveZoneCode = 0x11000005,
		ReceiveAntennaType = 0x11000006,
		ReceiveAntennaHeightToTip = 0x11000007,
		ReceiveAntennaHeightToCenterRAAT = 0x11000008,
		ReceiveAntennaMake = 0x11000009,
		ReceiveAntennaModel = 0x1100000A,
		ReceiveAntennaTilt = 0x1100000B,
		ReceiveAntennaPolarizationCode = 0x1100000C,
		ReceiveAntennaBeamWidth = 0x1100000D,
		ReceiveAntennaGain = 0x1100000F,
		ReceiveAntennaAzimuth = 0x11000010,
		ReceiveAntennaHeightAboveAverageTerrain = 0x11000011,
		ReceiveAntennaDiversityHeight = 0x11000012,
		ReceiveAntennaDiversityGain = 0x11000013,
		ReceiveAntennaDiversityBeam = 0x11000014,
		ReceiveAntennaReflectorHeight = 0x11000015,
		ReceiveAntennaReflectorWidth = 0x11000016,
		ReceiveAntennaReflectorSeparation = 0x11000017,
		ReceiveAntennaPassiveRepeaterNumber = 0x11000018,
		ReceiveAntennaBackToBackTxGain = 0x11000019,
		ReceiveAntennaBackToBackRxGain = 0x1100001a,
		ReceiveAntennaLocationName = 0x1100001b,
		ReceiveAntennaPassiveRepeaterSequenceId = 0x1100001c,
		ReceiveAntennaAlternativeCGSA = 0x1100001d,
		ReceiveAntennaPathNumber = 0x1100001e,
		ReceiveAntennaLineLoss = 0x1100001f,
		ReceiveAntennaStatusCode = 0x11000020,
		ReceiveAntennaStatusDate = 0x11000021,
	};
};

#endif
