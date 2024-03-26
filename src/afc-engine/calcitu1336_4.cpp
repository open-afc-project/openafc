#include "calcitu1245.h"

#include <QDebug>

using namespace std;

namespace calcItu1336_4
{

double CalcITU1336_omni_avg(const double &elAngleDeg,
			    const double &maxGain,
			    const double & /* frequencyHz */)
{
	// ITU-R F.1336-4 (02/2014)
	// Section 2.2 Omnidirectional Antenna, average side-lobe pattern

	double theta3 = 107.6 * exp(-maxGain * log(10.0) / 10.0); // EQN 1b

	double k = 0.0; // sec 2.4

	double theta5 = theta3 * sqrt(1.25 - log10(k + 1.0) / 1.2); // Eqn 1d

	double absElAngleDeg = fabs(elAngleDeg);

	double gain;

	if (absElAngleDeg < theta3) {
		double r = absElAngleDeg / theta3;
		gain = maxGain - 12 * r * r;
	} else if (absElAngleDeg < theta5) {
		gain = maxGain - 15.0 + 10 * log10(k + 1.0);
	} else {
		double r = absElAngleDeg / theta3;
		gain = maxGain - 15.0 + 10 * log10(exp(-1.5 * log(r)) + k);
	}

	return (gain);
}

}
