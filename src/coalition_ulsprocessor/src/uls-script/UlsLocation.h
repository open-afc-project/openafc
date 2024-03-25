/*
 *      This data structure stores the data related to an location.
 *      There is a field in this for each relevent column in the ULS database, regardless of whether
 * or not it is populated.
 *
 */

#ifndef ULS_LOCATION_H
#define ULS_LOCATION_H

class UlsLocation
{
	public:
		long long systemId; // unique identifier for this record; may not be necessary
		char callsign[11]; // this is the 'key' for the owner of this antenna.
		char locationAction;
		char locationType;
		char locationClass;
		int locationNumber;
		char siteStatus;
		int correspondingFixedLocation;
		char locationAddress[81];
		char locationCity[21];
		char locationCounty[61];
		char locationState[3];
		double radius;
		char areaOperationCode;
		char clearanceIndication;
		double groundElevation;
		double latitude; // Negative = south
		double longitude; // Negative = west
		char nepa;
		double supportHeight;
		double overallHeight;
		char structureType[7];
		char airportId[5];
		char locationName[21];
		char statusCode;
		char statusDate[11];
		char earthStationAgreement;

		int latitudeDeg, latitudeMinutes;
		double latitudeSeconds;
		char latitudeDirection;
		int longitudeDeg, longitudeMinutes;
		double longitudeSeconds;
		char longitudeDirection;

		enum Parameters {
			MinLocationParameter = 0x03000000,
			LocationSystemId = 0x03000001,
			LocationCallsign = 0x03000002,
			LocationAction = 0x03000003,
			LocationType = 0x03000004,
			LocationClass = 0x03000005,
			LocationSiteStatus = 0x03000006,
			LocationCorrespondingFixed = 0x03000007,
			LocationAddress = 0x03000008,
			LocationCity = 0x03000009,
			LocationCounty = 0x0300000a,
			LocationState = 0x0300000b,
			LocationRadius = 0x0300000c,
			LocationAreaOperationCode = 0x0300000d,
			LocationClearanceIndication = 0x0300000e,
			LocationGroundElevation = 0x0300000f,
			LocationLatitude = 0x03000010,
			LocationLongitude = 0x03000011,
			LocationNepa = 0x03000012,
			LocationSupportHeight = 0x03000013,
			LocationOverallHeight = 0x03000014,
			LocationStructureType = 0x03000015,
			LocationAirportId = 0x03000016,
			LocationName = 0x03000017,
			LocationStatusCode = 0x03000018,
			LocationStatusDate = 0x03000019,
			LocationEarthStationAgreement = 0x0300001a,
			MaxLocationParameter = 0x0300001b,
			ReceiveLocationSystemId = 0x13000001,
			ReceiveLocationCallsign = 0x13000002,
			ReceiveLocationAction = 0x13000003,
			ReceiveLocationType = 0x13000004,
			ReceiveLocationClass = 0x13000005,
			ReceiveLocationSiteStatus = 0x13000006,
			ReceiveLocationCorrespondingFixed = 0x13000007,
			ReceiveLocationAddress = 0x13000008,
			ReceiveLocationCity = 0x13000009,
			ReceiveLocationCounty = 0x1300000a,
			ReceiveLocationState = 0x1300000b,
			ReceiveLocationRadius = 0x1300000c,
			ReceiveLocationAreaOperationCode = 0x1300000d,
			ReceiveLocationClearanceIndication = 0x1300000e,
			ReceiveLocationGroundElevation = 0x1300000f,
			ReceiveLocationLatitude = 0x13000010,
			ReceiveLocationLongitude = 0x13000011,
			ReceiveLocationNepa = 0x13000012,
			ReceiveLocationSupportHeight = 0x13000013,
			ReceiveLocationOverallHeight = 0x13000014,
			ReceiveLocationStructureType = 0x13000015,
			ReceiveLocationAirportId = 0x13000016,
			ReceiveLocationName = 0x13000017,
			ReceiveLocationStatusCode = 0x13000018,
			ReceiveLocationStatusDate = 0x13000019,
			ReceiveLocationEarthStationAgreement = 0x1300001a,
		};
};

#endif
