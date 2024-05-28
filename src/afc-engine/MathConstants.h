#ifndef MATH_CONSTANTS_H
#define MATH_CONSTANTS_H

//  See MathConstants.cpp for implementation details, values and units.

class MathConstants
{
    public:
        static const double GeostationaryOrbitHeight;
        static const double GeostationaryOrbitRadius;
        static const double EarthMaxRadius, EarthMinRadius;
        static const double EarthEccentricitySquared;

        //  WGS'84 constants.
        static const double WGS84EarthSemiMajorAxis;
        static const double WGS84EarthSemiMinorAxis;
        static const double WGS84EarthFirstEccentricitySquared;
        static const double WGS84EarthSecondEccentricitySquared;

        // Physics constants
        static const double speedOfLight;
        static const double boltzmannConstant;
};

#endif
