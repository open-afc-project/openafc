/******************************************************************************************/
/**** FILE: lininterp.cpp                                                              ****/
/******************************************************************************************/
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <stdexcept>

#include "global_defines.h"
#include "list.h"
#include "lininterp.h"

/******************************************************************************************/
/**** CONSTRUCTOR: LinInterpClass::LinInterpClass                                      ****/
/******************************************************************************************/
LinInterpClass::LinInterpClass(ListClass<DblDblClass> *dataList, double xshift, double yshift)
{
	n = dataList->getSize();
	// n==1 → DVECTOR(n-1) == DVECTOR(0) returns NULL; a[0]/b[0] would
	// then dereference NULL.  Linear interpolation requires at least 2
	// data points.
	if (n < 2) {
		throw std::invalid_argument("LinInterpClass: data list must contain at least 2 "
					    "points");
	}

	a = DVECTOR(n - 1);
	b = DVECTOR(n - 1);
	x = DVECTOR(n);

	makelininterpcoeffs(dataList, xshift, yshift);
}
/******************************************************************************************/

/******************************************************************************************/
/**** CONSTRUCTOR: LinInterpClass::LinInterpClass                                      ****/
/******************************************************************************************/
LinInterpClass::LinInterpClass(std::vector<std::tuple<double, double>> dataList,
			       double xshift,
			       double yshift)
{
	n = dataList.size();
	// See ListClass constructor: n<2 yields a NULL 'a' and 'b' from
	// DVECTOR(n-1), which makelininterpcoeffs would then dereference.
	if (n < 2) {
		throw std::invalid_argument("LinInterpClass: data list must contain at least 2 "
					    "points");
	}

	a = DVECTOR(n - 1);
	b = DVECTOR(n - 1);
	x = DVECTOR(n);

	makelininterpcoeffs(dataList, xshift, yshift);
}
/******************************************************************************************/

/******************************************************************************************/
/**** CONSTRUCTOR: LinInterpClass::~LinInterpClass                                     ****/
/******************************************************************************************/
LinInterpClass::~LinInterpClass()
{
	free(a);
	free(b);
	free(x);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: LinInterpClass::makelininterpcoeffs                                    ****/
/**** Return value: 1 if successfull, 0 if not successful                              ****/
/******************************************************************************************/
int LinInterpClass::makelininterpcoeffs(ListClass<DblDblClass> *dataList,
					double xshift,
					double yshift)
{
	int i;
	double x0, x1, y0, y1;

	x0 = (*dataList)[0].x() + xshift;
	y0 = (*dataList)[0].y() + yshift;
	x[0] = x0;
	for (i = 1; i <= dataList->getSize() - 1; i++) {
		x1 = (*dataList)[i].x() + xshift;
		y1 = (*dataList)[i].y() + yshift;
		x[i] = x1;

		a[i - 1] = y0;
		b[i - 1] = (y1 - y0) / (x1 - x0);

		x0 = x1;
		y0 = y1;
	}

	return (1);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: LinInterpClass::makelininterpcoeffs                                    ****/
/**** Return value: 1 if successfull, 0 if not successful                              ****/
/******************************************************************************************/
int LinInterpClass::makelininterpcoeffs(std::vector<std::tuple<double, double>> dataList,
					double xshift,
					double yshift)
{
	int i;
	double x0, x1, y0, y1;

	x0 = std::get<0>(dataList[0]) + xshift;
	y0 = std::get<1>(dataList[0]) + yshift;
	x[0] = x0;
	for (i = 1; i <= (int)dataList.size() - 1; i++) {
		x1 = std::get<0>(dataList[i]) + xshift;
		y1 = std::get<1>(dataList[i]) + yshift;
		x[i] = x1;

		a[i - 1] = y0;
		b[i - 1] = (y1 - y0) / (x1 - x0);

		x0 = x1;
		y0 = y1;
	}

	return (1);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: LinInterpClass::lininterpval                                           ****/
/**** this evaluates the linear interpolation curve as defined by the last call to     ****/
/**** makeslininterpoeffs.                                                             ****/
/****                                                                                  ****/
/****    xpoint = desired function argument                                            ****/
/****                                                                                  ****/
/******************************************************************************************/
double LinInterpClass::lininterpval(double xpoint) const
{
	int s = -1;
	double h, z;

	if (!std::isfinite(xpoint)) {
		throw std::range_error("LinInterpClass::lininterpval: non-finite xpoint");
	}
	if ((xpoint >= x[0]) && (xpoint <= x[n - 1])) {
		s = lininterp_getintindex(xpoint);
	} else if (xpoint > x[n - 1]) {
		s = n - 2;
	} else if (xpoint < x[0]) {
		s = 0;
	} else {
		fprintf(stdout, "ERROR in routine lininterpval()\n");
		fprintf(stdout, "input x out of range in lininterp evaluation routine.\n");
		fprintf(stdout, "input x = %5.3f\n", xpoint);
		fprintf(stdout, "first, last in array = %5.3f, %5.3f\n", x[0], x[n - 1]);
		fprintf(stdout, "\n");
		CORE_DUMP;
	}

	h = xpoint - x[s];
	z = b[s] * h + a[s];

	return (z);
}
/******************************************************************************************/
/**** FUNCTION: LinInterpClass::lininterpDerivativeVal                                 ****/
/**** Evaluated the first derivative of the lininterp.                                 ****/
/****                                                                                  ****/
/****    xpoint = desired function argument                                            ****/
/****                                                                                  ****/
/******************************************************************************************/
double LinInterpClass::lininterpDerivativeVal(double xpoint) const
{
	int s = -1;
	double z;

	if ((xpoint >= x[0]) && (xpoint <= x[n - 1])) {
		s = lininterp_getintindex(xpoint);
	} else if (xpoint > x[n - 1]) {
		s = n - 2;
	} else if (xpoint < x[0]) {
		s = 0;
	} else {
		fprintf(stdout, "ERROR in routine lininterpval()\n");
		fprintf(stdout, "input x out of range in lininterp evaluation routine.\n");
		fprintf(stdout, "input x = %5.3f\n", xpoint);
		fprintf(stdout, "first, last in array = %5.3f, %5.3f\n", x[0], x[n - 1]);
		fprintf(stdout, "\n");
		CORE_DUMP;
	}

	z = b[s];

	return (z);
}
/******************************************************************************************/
/**** FUNCTION: LinInterpClass::lininterp_getintindex                                        ****/
/******************************************************************************************/
int LinInterpClass::lininterp_getintindex(double xtest) const
{
	int lowind, upind, testind, index;

	if ((xtest < x[0]) || (xtest > x[n - 1])) {
		fprintf(stdout, "ERROR in routine getintindex()\n");
		fprintf(stdout, "input x out of range.\n");
		fprintf(stdout, "x =%5.3f\n", xtest);
		fprintf(stdout, "range = %5.3f, %5.3f\n", x[0], x[n - 1]);
		fprintf(stdout, "\n");
		CORE_DUMP;
	}

	lowind = 0;
	upind = n - 1;

	do {
		testind = (lowind + upind) / 2;
		if (xtest > x[testind]) {
			lowind = testind;
		} else {
			upind = testind;
		}
	} while (upind - lowind > 1);

	if (xtest > x[upind]) {
		index = upind;
	} else {
		index = lowind;
	}

	return (index);
}
/******************************************************************************************/
