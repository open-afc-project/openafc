/*
 *      This data structure stores the data related to a path.
 *      There is a field in this for each relevent column in the ULS database, regardless of whether or not it
 *      is populated.
 *
 */

#ifndef ULS_PATH_H
#define ULS_PATH_H

class UlsPath {
public:
	long long systemId;                 // unique identifier for this record; may not be necessary
	char callsign[11];                  // this is the 'key' for the owner of this antenna.
    int pathNumber;                     // Path ID.
	int txLocationNumber;               // This matches a UlsLocation of the same callsign as above.
	int txAntennaNumber;                // Likewise for a UlsAntenna.
	int rxLocationNumber;               // This matches a UlsLocation with either the same callsign of the one below.
	int rxAntennaNumber;                // Likewise.
	char pathType[21];
	char passiveReceiver;
	char countryCode[4];
	char GSOinterference;
	char rxCallsign[11];                // If empty in DB, program will make the same as above.
	double angularSeparation;
	char statusCode;
	char statusDate[11];

    enum Parameter {
        MinPathParameter = 0x04000000,
		PathCallsign = 0x04000001,
		PathNumber = 0x04000002,
        PathTxLocationNumber = 0x04000003,
		PathTxAntennaNumber = 0x04000004,
		PathRxLocationNumber = 0x04000005,
		PathRxAntennaNumber = 0x04000006,
		PathType = 0x04000007,
        PathPassiveReceiver = 0x04000008,
		PathCountryCode = 0x04000009,
		PathGSOInterference = 0x0400000a,
		PathRxCallsign = 0x0400000b,
		PathAngularSeparation = 0x0400000c,
		PathStatusCode = 0x0400000d,
		PathStatusDate = 0x0400000e,
		MaxPathParameter = 0x0400000f,
    };
};

#endif
