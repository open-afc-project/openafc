#ifndef CALCITU699_H
#define CALCITU699_H

#include <math.h>
#include <complex>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

using namespace std;

namespace calcItu699 {

double CalcITU699(const double &angleDeg, const double &maxGain, const double &Dlambda);

double CalcITU699psiM(const double &maxGain);
}

#endif // CALCITU699_H
