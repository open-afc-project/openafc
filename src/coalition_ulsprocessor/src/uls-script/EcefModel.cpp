#include <armadillo>
#include <cmath>
#include "MathConstants.h"
#include "EcefModel.h"

//  Note: Altitude here is a true altitude, i.e. a height. Given an altitude (in km), this returns a
//  value in an ECEF coordinate
//      frame in km.
Vector3 EcefModel::geodeticToEcef(double lat, double lon, double alt)
{
	const double a =
		MathConstants::WGS84EarthSemiMajorAxis; // 6378.137; // Radius of the earth in km.
	const double esq =
		MathConstants::WGS84EarthFirstEccentricitySquared; // 6.694379901e-3; // First
								   // eccentricity squared.

	//  Convert lat/lon to radians.
	const double latr = lat * M_PI / 180.0;
	const double lonr = lon * M_PI / 180.0;

	double cosLon, sinLon;
	::sincos(lonr, &sinLon, &cosLon);
	double cosLat, sinLat;
	::sincos(latr, &sinLat, &cosLat);

	//  Compute 'chi', which adjusts for vertical eccentricity.
	const double chi = sqrt(1.0 - esq * sinLat * sinLat);

	return Vector3((a / chi + alt) * cosLat * cosLon,
		       (a / chi + alt) * cosLat * sinLon,
		       (a * (1 - esq) / chi + alt) * sinLat);
}

//  Converts from ecef to geodetic coordinates. This algorithm is from Wikipedia, and
//      all constants are from WGS '84.
GeodeticCoord EcefModel::ecefToGeodetic(const Vector3 &ecef)
{
	const double a = MathConstants::WGS84EarthSemiMajorAxis; // 6378.137;
	const double b = MathConstants::WGS84EarthSemiMinorAxis; // 6356.7523142;
	// double e = sqrt(MathConstants::WGS84EarthFirstEccentricitySquared);
	const double eprime = sqrt(MathConstants::WGS84EarthSecondEccentricitySquared);
	const double esq = MathConstants::WGS84EarthFirstEccentricitySquared;
	// double eprimesq = MathConstants::WGS84EarthSecondEccentricitySquared;

	const double X = ecef.x();
	const double Y = ecef.y();
	const double Z = ecef.z();

	double r = sqrt(X * X + Y * Y);
	double Esq = a * a - b * b;
	double F = 54 * b * b * Z * Z;
	double G = r * r + (1 - esq) * Z * Z - esq * Esq;
	double C = esq * esq * F * r * r / (G * G * G);
	double S = pow(1 + C + sqrt(C * C + 2 * C), 1.0 / 3.0);
	double P = F / (3 * (S + 1 / S + 1) * (S + 1 / S + 1) * G * G);
	double Q = sqrt(1 + 2 * esq * esq * P);
	double r0 = -(P * esq * r) / (1 + Q) +
		    sqrt(a * a / 2 * (1 + 1 / Q) - (P * (1 - esq) * Z * Z) / (Q * (1 + Q)) -
			 P * r * r / 2.0);
	double U = sqrt((r - esq * r0) * (r - esq * r0) + Z * Z);
	double V = sqrt((r - esq * r0) * (r - esq * r0) + (1 - esq) * Z * Z);
	double Z0 = (b * b * Z) / (a * V);

	double h = U * (1 - (b * b) / (a * V));
	double lat = atan((Z + eprime * eprime * Z0) / r) * 180 / M_PI;
	double lon = atan2(Y, X) * 180 / M_PI;
	return GeodeticCoord(lon, lat, h);
}

Vector3 EcefModel::fromGeodetic(const GeodeticCoord &in)
{
	return geodeticToEcef(in.latitudeDeg, in.longitudeDeg, in.heightKm);
}

GeodeticCoord EcefModel::toGeodetic(const Vector3 &in)
{
	return ecefToGeodetic(in);
}

Vector3 EcefModel::localVertical(const GeodeticCoord &in)
{
	double cosLon, sinLon;
	::sincos(M_PI / 180.0 * in.longitudeDeg, &sinLon, &cosLon);
	double cosLat, sinLat;
	::sincos(M_PI / 180.0 * in.latitudeDeg, &sinLat, &cosLat);

	return Vector3(cosLat * cosLon, cosLat * sinLon, sinLat);
}
