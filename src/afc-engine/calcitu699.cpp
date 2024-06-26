#include "calcitu699.h"

using namespace std;

namespace calcItu699
{

double CalcITU699(const double &angleDeg, const double &maxGain, const double &Dlambda)
{
	double cAngleDeg = angleDeg;

	while (cAngleDeg > 180.0) {
		cAngleDeg = cAngleDeg - 360.0;
	}

	cAngleDeg = fabs(cAngleDeg);

	// double Dlambda = pow(10, ((maxGain - 7.7)/20));
	double g1 = 2.0 + 15.0 * log10(Dlambda);
	double psiM = 20.0 * (1.0 / Dlambda) * pow((maxGain - g1), 0.5);
	double psiR = 15.85 * pow(Dlambda, -0.6);
	double rv;

	// qDebug() << Dlambda;

	if (Dlambda > 100.0) {
		if (cAngleDeg >= 0.0 && cAngleDeg < psiM) {
			rv = maxGain - 2.5 * pow(10, -3.0) * pow((Dlambda * cAngleDeg), 2.0);
		} else if (cAngleDeg >= psiM && cAngleDeg < std::max(psiM, psiR)) {
			rv = g1;
		} else if (cAngleDeg >= std::max(psiM, psiR) && cAngleDeg < 120.0) {
			rv = 32.0 - 25.0 * log10(cAngleDeg);
		} else {
			rv = -20.0;
		}
	} else if (Dlambda <= 100.0) {
		// qDebug() << cAngleDeg << psiM;
		if (cAngleDeg >= 0.0 && cAngleDeg < psiM) {
			rv = maxGain - 2.5 * pow(10, -3.0) * pow((Dlambda * cAngleDeg), 2.0);
		} else if (cAngleDeg >= psiM && cAngleDeg < 100.0 / Dlambda) {
			rv = g1;
		} else if (cAngleDeg >= std::max(psiM, 100.0 / Dlambda) && cAngleDeg < 48.0) {
			rv = 52.0 - 10.0 * log10(Dlambda) - 25.0 * log10(cAngleDeg);
		} else {
			rv = 10.0 - 10.0 * log10(Dlambda);
		}
	}
	return rv;
}

double CalcITU699psiM(const double &maxGain)
{
	double Dlambda = pow(10, ((maxGain - 7.7) / 20));
	double g1 = 2.0 + 15.0 * log10(Dlambda);
	double psiM = 20.0 * (1.0 / Dlambda) * pow((maxGain - g1), 0.5);

	return (psiM);
}

}
