/******************************************************************************************/
/**** FILE: lininterp.h                                                                ****/
/******************************************************************************************/

#ifndef LININTERP_H
#define LININTERP_H

#include "dbldbl.h"

template<class T> class ListClass;

/******************************************************************************************/
/**** CLASS: LinInterpClass                                                            ****/
/******************************************************************************************/
class LinInterpClass
{
	public:
		LinInterpClass(ListClass<DblDblClass> *dataList, double xshift = 0.0, double yshift = 0.0);
		~LinInterpClass();
		double lininterpval(double) const;
		double lininterpDerivativeVal(double xpoint) const;

	private:
		double *a, *b, *x;
		int n;
		int lininterp_getintindex(double) const;
		int makelininterpcoeffs(ListClass<DblDblClass> *dataList, double xshift, double yshift);
};
/******************************************************************************************/

#endif
