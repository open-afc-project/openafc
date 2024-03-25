/*
 *      This class represents a single hop in a microwave link. This basically assembles the
 * information read in from the database and builds a single, larger database with pointers to the
 * appropriate locations, antennas and frequencies.
 *
 */

#ifndef ULS_HOP_H
#define ULS_HOP_H

#include "UlsAntenna.h"
#include "UlsLocation.h"
#include "UlsPath.h"
#include "UlsFrequency.h"

enum HopType { HopArea, HopFixedLink, HopOther };

class UlsHop
{
	public:
		enum HopType type;
		char callsign[11];
		char unconfirmedReceiver;

		struct UlsAntenna *txAntenna;
		struct UlsAntenna *rxAntenna;
		struct QList<UlsFrequency *> freq;
		struct UlsPath *path;
		struct UlsLocation *rxLocation;
		struct UlsLocation *txLocation;
		struct UlsEmission *emission;
		struct UlsHeader *header;

		QList<UlsEntity *> txEntities;
		QList<UlsEntity *> rxEntities;
};

#endif
