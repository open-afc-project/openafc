#ifndef ULS_EMISSION_H
#define ULS_EMISSION_H

class UlsEmission
{
	public:
		long long systemId;
		char callsign[11];
		int locationId;
		int antennaId;
		double frequency;
		char desig[11];
		double modRate;
		std::string modCode;
		int frequencyId;
};

#endif
