/******************************************************************************************/
/**** FILE: dbldbl.cpp                                                              ****/
/**** Michael Mandell                                                                  ****/
/******************************************************************************************/

#include <cstdlib>
#include <stdexcept>
#include <sstream>
#include "dbldbl.h"

DblDblClass::DblDblClass(double d0, double d1) {
	dval0 = d0;
	dval1 = d1;
}

DblDblClass::~DblDblClass() {
}

double DblDblClass::getDbl(int i) {
	if (i==0) {
		return(dval0);
	} else {
		return(dval1);
	}
}

void DblDblClass::setX(double xval) {
	dval0 = xval;
}

void DblDblClass::setY(double yval) {
	dval1 = yval;
}

double DblDblClass::x() const {
	return(dval0);
}

double DblDblClass::y() const {
	return(dval1);
}

int DblDblClass::operator==(const DblDblClass& val) const {
	if ((val.dval0 == dval0) && (val.dval1 == dval1)) {
		return(1);
	} else {
		return(0);
	}
}

int DblDblClass::operator>(const DblDblClass& val) const {
	if ( (dval0 > val.dval0) || ( (dval0 == val.dval0) && (dval1 > val.dval1) ) ) {
		return(1);
	} else {
		return(0);
	}
}

std::ostream& operator<<(std::ostream& s, const DblDblClass& val) {
	s << "(" << val.dval0 << "," << val.dval1 << ")";
	return(s);
}

int cvtStrToVal(char const* strptr, DblDblClass& val) {
	const char *chptr = strptr;
	char *nptr;
	int i;

	for(i=0; i<=1; i++) {
		switch(i) {
			case 0: val.dval0 = strtod(chptr, &nptr); break;
			case 1: val.dval1 = strtod(chptr, &nptr); break;
		}
		if (nptr == chptr) {
			std::stringstream errorStr;
			errorStr << "ERROR in cvtStrToVal() : Unable to cvt to DblDblClass \"" << strptr << "\"";
			throw std::runtime_error(errorStr.str());
			return(0);
		}
		chptr = nptr;
	}

	return(chptr-strptr);
}

