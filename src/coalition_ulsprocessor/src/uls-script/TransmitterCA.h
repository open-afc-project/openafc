/*
 *      This data structure stores the data related to Transmitters for Canada (CA).
 *      There is a field in this for each relevent column in the ISED database, regardless of
 * whether or not it is populated.
 *
 */

#ifndef TRANSMITTER_CA_H
#define TRANSMITTER_CA_H

class TransmitterCAClass
{
	public:
		int service;
		int subService;
		std::string authorizationNumber;
		std::string callsign;
		double latitudeDeg;
		double longitudeDeg;
		double groundElevation;
		double antennaHeightAGL;
		std::string emissionsDesignator;
		double bandwidthMHz;
		double centerFreqMHz;
		double antennaGain;
		std::string antennaModel;
		std::string modulation;
		double modRate;
};

#endif
