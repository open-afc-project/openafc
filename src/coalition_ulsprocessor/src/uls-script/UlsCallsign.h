/*
 *      This database permits fast lookup of links, locations, etc via callsign. Each callsign has its own table of locations, antennas, etc.
 *
 */

#ifndef ULS_CALLSIGN_H
#define ULS_CALLSIGN_H

#include "UlsLocation.h"
#include "UlsAntenna.h"
#include "UlsFrequency.h"
#include "UlsPath.h"
#include "UlsEmission.h"
#include "UlsEntity.h"
#include "UlsMarketFrequency.h"
#include <bsd/string.h>
#include <QList>
#include <QString>

class UlsCallsign {
public:
    QString callsign;
    char callsignascii[11];
    QList<UlsHeader *> *headers;
	QList<UlsAntenna *> *antennas;
	QList<UlsLocation *> *locations;
	QList<UlsFrequency *> *frequencies;
    QList<UlsEmission *> *emissions;
    QList<UlsEntity *> *entities;
	QList<UlsMarketFrequency *> *marketFreqs;

	UlsCallsign() {
        antennas = NULL;
		locations = NULL;
		frequencies = NULL;
        headers = NULL;
		strlcpy(callsignascii, "", sizeof(callsignascii));
    }
};

#endif
