#ifndef ULS_FUNCTIONS_H
#define ULS_FUNCTIONS_H

#include <stdio.h>
#include <unordered_set>
#include <algorithm>

#include <QList>
#include <QHash>
#include <QString>

#include "UlsFileReader.h"

class UlsFunctionsClass
{
	public:
		static const double speedOfLight; // speed of light in m/s
		static const double earthRadius; // Radius of earth in m
		static const double unii5StartFreqMHz;
		static const double unii5StopFreqMHz;
		static const double unii6StartFreqMHz;
		static const double unii6StopFreqMHz;
		static const double unii7StartFreqMHz;
		static const double unii7StopFreqMHz;
		static const double unii8StartFreqMHz;
		static const double unii8StopFreqMHz;

		static QString makeNumber(const double &d);

		static QString makeNumber(const int &i);

		static QString charString(char c);
		static double emissionDesignatorToBandwidth(const QString &emDesig);
		static QString hasNecessaryFields(const UlsEmission &e,
						  UlsPath path,
						  UlsLocation rxLoc,
						  UlsLocation txLoc,
						  UlsAntenna rxAnt,
						  UlsAntenna txAnt,
						  UlsHeader txHeader,
						  QList<UlsLocation> prLocList,
						  QList<UlsAntenna> prAntList,
						  bool removeMobile);
		static bool SegmentCompare(const UlsSegment &segA, const UlsSegment &segB);

		static QStringList getCSVHeader(int numPR);

		static QStringList getRASHeader();

		static double computeSpectralOverlap(double sigStartFreq,
						     double sigStopFreq,
						     double rxStartFreq,
						     double rxStopFreq);

		static Vector3 computeHPointingVec(Vector3 position,
						   double azimuthPtg,
						   double elevationPtg);

		static double getAngleFromDMS(std::string dmsStr);
};

#endif
