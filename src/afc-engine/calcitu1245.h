#ifndef CALCITU1245_H
#define CALCITU1245_H

#include <math.h>
#include <complex>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

using namespace std;

namespace calcItu1245{

 double mymin(const double &a, const double &b);

double mymax(const double &a, const double &b);

double CalcITU1245(const double &angleDeg, const double &maxGain);

double CalcITU1245psiM(const double &maxGain);

double CalcFCCPattern(const double &angleDeg, const double &maxGain);

double CalcETSIClass4(const double &angleDeg, const double &maxGain);
}

#endif // CALCITU1245_H
