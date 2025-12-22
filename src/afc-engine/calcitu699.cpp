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
	double psiM = (maxGain >= g1 ? (20.0 / Dlambda)*sqrt(maxGain - g1) : 0.0);
	double psiR = 15.85 * pow(Dlambda, -0.6);
	double rv;

	// qDebug() << Dlambda;

	if (Dlambda > 100.0) {
		if (cAngleDeg < psiM) {
			double DlambdaPhi = Dlambda * cAngleDeg;
			rv = maxGain - 2.5e-3 * DlambdaPhi*DlambdaPhi;
		} else if (cAngleDeg < psiR) {
			rv = g1;
		} else if (cAngleDeg < 48.0) {
			rv = 32.0 - 25.0 * log10(cAngleDeg);
		} else {
			rv = -10.0;
		}
	} else if (Dlambda <= 100.0) {
		if (cAngleDeg < psiM) {
			double DlambdaPhi = Dlambda * cAngleDeg;
			rv = maxGain - 2.5e-3 * DlambdaPhi*DlambdaPhi;
		} else if (cAngleDeg < 100.0 / Dlambda) {
			rv = g1;
		} else if (cAngleDeg < 48.0) {
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
