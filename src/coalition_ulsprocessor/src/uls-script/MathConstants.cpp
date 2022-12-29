/*
 *      Useful constants for various purposes. All static and public so they can be seen from everywhere.
 */

#include "MathConstants.h"

const double MathConstants::GeostationaryOrbitHeight = 35786.094; // altitude above sea-level (km)
const double MathConstants::EarthMaxRadius = 6378.137; // km, from WGS '84
const double MathConstants::EarthMinRadius = 6356.760; // km
const double MathConstants::GeostationaryOrbitRadius = MathConstants::EarthMaxRadius + MathConstants::GeostationaryOrbitHeight; // from earth center (km)

const double MathConstants::WGS84EarthSemiMajorAxis = 6378.137; // km
const double MathConstants::WGS84EarthSemiMinorAxis = 6356.7523142; // km
const double MathConstants::WGS84EarthFirstEccentricitySquared  = 6.69437999014e-3; // unitless
const double MathConstants::WGS84EarthSecondEccentricitySquared = 6.73949674228e-3; // unitless

const double MathConstants::speedOfLight = 2997924580.0;
const double MathConstants::boltzmannConstant = 1.3806488e-23;
