/*
 *      This data structure stores the data relevent for a frequency record.
 *      There is a field in this for each relevent column in the ULS database, regardless of whether
 * or not it is populated.
 *
 */

#ifndef ULS_FREQUENCY_H
#define ULS_FREQUENCY_H

class UlsFrequency
{
	public:
		long long systemId; // unique identifier for this record; may not be necessary
		char callsign[11]; // this is the 'key' for the owner of this antenna.
		int locationNumber; // index among the corresponding UlsLocations
		int antennaNumber; // index among the corresponding UlsAntenna
		char classStationCode[5]; // unsure what this means
		char opAltitudeCode[3];
		double frequencyAssigned;
		double frequencyUpperBand;
		double frequencyCarrier;
		int timeBeginOperations;
		int timeEndOperations;
		double powerOutput;
		double powerERP;
		double tolerance;
		char frequencyIndicator;
		char status;
		double EIRP;
		char transmitterMake[26];
		char transmitterModel[26];
		char transmitterPowerControl;
		int numberUnits;
		int numberReceivers;
		int frequencyNumber;
		char statusCode;
		char statusDate[11];
		int pathNumber;

		enum Parameters {
			MinFrequencyParameter = 0x02000000,
			FrequencySystemID = 0x02000001,
			FrequencyCallsign = 0x02000002,
			FrequencyLocationNumber = 0x02000003,
			FrequencyAntennaNumber = 0x02000004,
			FrequencyClassStationCode = 0x02000005,
			FrequencyOpAltitudeCode = 0x02000006,
			FrequencyAssigned = 0x02000007,
			FrequencyUpperBand = 0x02000008,
			FrequencyCarrier = 0x02000009,
			FrequencyTimeBeginsOperations = 0x0200000a,
			FrequencyTimeEndsOperations = 0x0200000b,
			FrequencyPowerOutput = 0x0200000c,
			FrequencyPowerERP = 0x0200000d,
			FrequencyTolerance = 0x0200000e,
			FrequencyIndicator = 0x0200000f,
			FrequencyStatus = 0x02000010,
			FrequencyEIRP = 0x02000011,
			FrequencyTransmitterMake = 0x02000012,
			FrequencyTransmitterModel = 0x02000013,
			FrequencyPowerControl = 0x02000014,
			FrequencyNumberUnits = 0x02000015,
			FrequencyNumberReceivers = 0x02000016,
			FrequencyNumber = 0x02000017,
			FrequencyStatusCode = 0x02000018,
			FrequencyStatusDate = 0x02000019,
			MaxFrequencyParameter = 0x0200001a,
		};
};

#endif
