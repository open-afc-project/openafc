/******************************************************************************************/
/**** PROGRAM: spline.cpp                                                              ****/
/******************************************************************************************/
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>

#include "global_defines.h"
#include "list.h"
#include "spline.h"

/******************************************************************************************/
/**** CONSTRUCTOR: SplineClass::SplineClass                                            ****/
/******************************************************************************************/
SplineClass::SplineClass(ListClass<DblDblClass> *dataList)
{
	int ptIdx;
	int n = dataList->getSize();

	double *origDataX = DVECTOR(n);
	double *origDataY = DVECTOR(n);

	for (ptIdx = 0; ptIdx <= dataList->getSize() - 1; ptIdx++) {
		origDataX[ptIdx] = (*dataList)[ptIdx].x();
		origDataY[ptIdx] = (*dataList)[ptIdx].y();
	}

	a = DVECTOR(n + 2);
	b = DVECTOR(n + 2);
	c = DVECTOR(n + 2);
	d = DVECTOR(n + 2);
	x = DVECTOR(n + 2);

	makesplinecoeffs(1, n, origDataX - 1, origDataY - 1);

	free(origDataX);
	free(origDataY);
}
/******************************************************************************************/

/******************************************************************************************/
/**** CONSTRUCTOR: SplineClass::SplineClass                                            ****/
/******************************************************************************************/
SplineClass::SplineClass(std::vector<DblDblClass> dataList)
{
	int ptIdx;
	int n = (int)dataList.size();

	double *origDataX = DVECTOR(n);
	double *origDataY = DVECTOR(n);

	for (ptIdx = 0; ptIdx < n; ptIdx++) {
		origDataX[ptIdx] = dataList.at(ptIdx).x();
		origDataY[ptIdx] = dataList.at(ptIdx).y();
	}

	a = DVECTOR(n + 2);
	b = DVECTOR(n + 2);
	c = DVECTOR(n + 2);
	d = DVECTOR(n + 2);
	x = DVECTOR(n + 2);

	makesplinecoeffs(1, n, origDataX - 1, origDataY - 1);

	free(origDataX);
	free(origDataY);
}
/******************************************************************************************/

/******************************************************************************************/
/**** CONSTRUCTOR: SplineClass::~SplineClass                                            ****/
/******************************************************************************************/
SplineClass::~SplineClass()
{
	free(a);
	free(b);
	free(c);
	free(d);
	free(x);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: SplineClass::makesplinecoeffs                                          ****/
/**** Return value: 1 if successfull, 0 if not successful                              ****/
/******************************************************************************************/
int SplineClass::makesplinecoeffs(int p_n1, int p_n2, double *xs, double *ys)
{
	int i, m1, m2, cont;
	double *y;
	double e, f, f2, g, h, p, s;
	std::ostringstream errStr;

	/* the dimensioning of the arrays is from n1-1 to n2+1   */
	double *r, *r1, *r2, *t, *t1, *u, *v, *dy;

	n1 = p_n1;
	n2 = p_n2;

	if (n1 <= 0) {
		errStr << std::string("ERROR in routine makesplinecoeffs()") << std::endl
		       << "first data point index must be > 0" << std::endl
		       << "n1 = " << n1 << std::endl;
		throw std::runtime_error(errStr.str());
	}

	if ((n1 >= n2)) {
		errStr << std::string("ERROR in routine makesplinecoeffs()") << std::endl
		       << "n1  >=  n2" << std::endl
		       << "n1 = " << n1 << ", n2 = " << n2 << std::endl;
		throw std::runtime_error(errStr.str());
	}

	for (i = n1; i <= n2; i++) {
		x[i] = xs[i];
		if ((i > n1) && (x[i] <= x[i - 1])) {
			errStr << std::string("ERROR in routine makesplinecoeffs()") << std::endl
			       << "x var not strictly increasing between indicies = " << i - 1
			       << "," << i << " (" << x[i - 1] << ", " << x[i] << std::endl;
			throw std::runtime_error(errStr.str());
		}
	}

	y = DVECTOR(n2 + 1);
	r = DVECTOR(n2 + 2);
	r1 = DVECTOR(n2 + 2);
	r2 = DVECTOR(n2 + 2);
	t = DVECTOR(n2 + 2);
	t1 = DVECTOR(n2 + 2);
	u = DVECTOR(n2 + 2);
	v = DVECTOR(n2 + 2);
	dy = DVECTOR(n2 + 2);

	for (i = n1; i <= n2; i++) {
		y[i] = ys[i];
	}

	s = 0.0;

	for (i = n1 - 1; i <= n2 + 1; i++) {
		dy[i] = 1.0;
	}

	m1 = n1 - 1;
	m2 = n2 + 1;
	r[m1] = 0.0;
	r[n1] = 0.0;
	r1[n2] = 0.0;
	r2[n2] = 0.0;
	r2[m2] = 0.0;
	u[m1] = 0.0;
	u[n1] = 0.0;
	u[n2] = 0.0;
	u[m2] = 0.0;
	p = 0.0;
	m1 = n1 + 1;
	m2 = n2 - 1;
	h = x[m1] - x[n1];
	f = (y[m1] - y[n1]) / h;

	g = h;
	for (i = m1; i <= m2; i++) {
		g = h;
		h = x[i + 1] - x[i];
		e = f;
		f = (y[i + 1] - y[i]) / h;
		a[i] = f - e;
		t[i] = 2.0 * (g + h) / 3.0;
		t1[i] = h / 3.0;
		r2[i] = dy[i - 1] / g;
		r[i] = dy[i + 1] / h;
		r1[i] = -dy[i] / g - dy[i] / h;
	}

	for (i = m1; i <= m2; i++) {
		b[i] = r[i] * r[i] + r1[i] * r1[i] + r2[i] * r2[i];
		c[i] = r[i] * r1[i + 1] + r1[i] * r2[i + 1];
		d[i] = r[i] * r2[i + 2];
	}

	f2 = -s;

	cont = 1;
	do {
		for (i = m1; i <= m2; i++) {
			r1[i - 1] = f * r[i - 1];
			r2[i - 2] = g * r[i - 2];
			r[i] = 1.0 / (p * b[i] + t[i] - f * r1[i - 1] - g * r2[i - 2]);
			u[i] = a[i] - r1[i - 1] * u[i - 1] - r2[i - 2] * u[i - 2];
			f = p * c[i] + t1[i] - h * r1[i - 1];
			g = h;
			h = d[i] * p;
		}

		for (i = m2; i >= m1; i--) {
			u[i] = r[i] * u[i] - r1[i] * u[i + 1] - r2[i] * u[i + 2];
		}

		e = 0.0;
		h = 0.0;
		for (i = n1; i <= m2; i++) {
			g = h;
			h = (u[i + 1] - u[i]) / (x[i + 1] - x[i]);
			v[i] = (h - g) * dy[i] * dy[i];
			e = e + v[i] * (h - g);
		}

		g = -h * dy[n2] * dy[n2];
		v[n2] = g;
		e = e - g * h;
		g = f2;
		f2 = e * p * p;
		if ((f2 >= s) || (f2 <= g)) {
			cont = 0;
		} else {
			f = 0.0;
			h = (v[m1] - v[n1]) / (x[m1] - x[n1]);
			for (i = m1; i <= m2; i++) {
				g = h;
				h = (v[i + 1] - v[i]) / (x[i + 1] - x[i]);
				g = h - g - r1[i - 1] * r[i - 1] - r2[i - 2] * r[i - 2];
				f = f + g * r[i] * g;
				r[i] = g;
			}

			h = e - p * f;

			if (h > 0.0) {
				p = p + (s - f2) / ((sqrt(s / e) + p) * h);
			} else {
				cont = 0;
			}
		}
	} while (cont);

	for (i = n1; i <= n2; i++) {
		a[i] = y[i] - p * v[i];
		c[i] = u[i];
	}

	for (i = n1; i <= m2; i++) {
		h = x[i + 1] - x[i];
		d[i] = (c[i + 1] - c[i]) / (3.0 * h);
		b[i] = (a[i + 1] - a[i]) / h - (h * d[i] + c[i]) * h;
	}

	free(y);
	free(r);
	free(r1);
	free(r2);
	free(t);
	free(t1);
	free(u);
	free(v);
	free(dy);

	return (1);
}
/******************************************************************************************/
/**** FUNCTION: SplineClass::splineval                                                 ****/
/**** this evaluates the cubic spline as defined by the last call to makesplinecoeffs. ****/
/****                                                                                  ****/
/****    xpoint = desired function argument                                            ****/
/****                                                                                  ****/
/******************************************************************************************/
double SplineClass::splineval(double xpoint) const
{
	int s = -1;
	double h, z;

	if ((xpoint >= x[n1]) && (xpoint <= x[n2])) {
		s = spline_getintindex(xpoint);
	} else if (xpoint > x[n2]) {
		s = n2 - 1;
	} else if (xpoint < x[n1]) {
		s = n1;
	} else {
		fprintf(stdout, "ERROR in routine splineval()\n");
		fprintf(stdout, "input x out of range in spline evaluation routine.\n");
		fprintf(stdout, "input x = %5.3f\n", xpoint);
		fprintf(stdout, "first, last in array = %5.3f, %5.3f\n", x[n1], x[n2]);
		fprintf(stdout, "\n");
		CORE_DUMP;
	}

	h = xpoint - x[s];
	z = ((d[s] * h + c[s]) * h + b[s]) * h + a[s];

	return (z);
}
/******************************************************************************************/
/**** FUNCTION: SplineClass::splineDerivativeVal                                       ****/
/**** Evaluated the first derivative of the spline.                                    ****/
/****                                                                                  ****/
/****    xpoint = desired function argument                                            ****/
/****                                                                                  ****/
/******************************************************************************************/
double SplineClass::splineDerivativeVal(double xpoint) const
{
	int s = -1;
	double h, z;

	if ((xpoint >= x[n1]) && (xpoint <= x[n2])) {
		s = spline_getintindex(xpoint);
	} else if (xpoint > x[n2]) {
		s = n2 - 1;
	} else if (xpoint < x[n1]) {
		s = n1;
	} else {
		fprintf(stdout, "ERROR in routine splineval()\n");
		fprintf(stdout, "input x out of range in spline evaluation routine.\n");
		fprintf(stdout, "input x = %5.3f\n", xpoint);
		fprintf(stdout, "first, last in array = %5.3f, %5.3f\n", x[n1], x[n2]);
		fprintf(stdout, "\n");
		CORE_DUMP;
	}

	h = xpoint - x[s];
	z = ((3 * d[s] * h + 2 * c[s]) * h + b[s]);

	return (z);
}
/******************************************************************************************/
/**** FUNCTION: SplineClass::splineDerivative2Val                                      ****/
/**** Evaluate the second derivative of the spline.                                    ****/
/****                                                                                  ****/
/****    xpoint = desired function argument                                            ****/
/****                                                                                  ****/
/******************************************************************************************/
double SplineClass::splineDerivative2Val(double xpoint) const
{
	int s = -1;
	double h, z;

	if ((xpoint >= x[n1]) && (xpoint <= x[n2])) {
		s = spline_getintindex(xpoint);
	} else if (xpoint > x[n2]) {
		s = n2 - 1;
	} else if (xpoint < x[n1]) {
		s = n1;
	} else {
		fprintf(stdout, "ERROR in routine splineval()\n");
		fprintf(stdout, "input x out of range in spline evaluation routine.\n");
		fprintf(stdout, "input x = %5.3f\n", xpoint);
		fprintf(stdout, "first, last in array = %5.3f, %5.3f\n", x[n1], x[n2]);
		fprintf(stdout, "\n");
		CORE_DUMP;
	}

	h = xpoint - x[s];
	z = 6 * d[s] * h + 2 * c[s];

	return (z);
}
/******************************************************************************************/
/**** FUNCTION: SplineClass::spline_getintindex                                        ****/
/******************************************************************************************/
int SplineClass::spline_getintindex(double xtest) const
{
	int lowind, upind, testind, index;

	if ((xtest < x[n1]) || (xtest > x[n2])) {
		fprintf(stdout, "ERROR in routine getintindex()\n");
		fprintf(stdout, "input x out of range.\n");
		fprintf(stdout, "x =%5.3f\n", xtest);
		fprintf(stdout, "range = %5.3f, %5.3f\n", x[n1], x[n2]);
		fprintf(stdout, "\n");
		CORE_DUMP;
	}

	lowind = n1;
	upind = n2;

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
