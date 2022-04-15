/******************************************************************************************/
/**** FILE: spline.h                                                                   ****/
/******************************************************************************************/

#ifndef SPLINE_H
#define SPLINE_H

#include <vector>
#include "dbldbl.h"

template<class T> class ListClass;

/******************************************************************************************/
/**** CLASS: SplineClass                                                               ****/
/******************************************************************************************/
class SplineClass
{
	public:
		SplineClass(ListClass<DblDblClass> *dataList);
		SplineClass(std::vector<DblDblClass> dataList);
		~SplineClass();
		double splineval(double) const;
		double splineDerivativeVal(double xpoint) const;
		double splineDerivative2Val(double xpoint) const;

	private:
		double *a, *b, *c, *d, *x;
		int n1, n2;
		int spline_getintindex(double) const;
		int makesplinecoeffs(int, int, double *, double *);
};
/******************************************************************************************/

#endif
