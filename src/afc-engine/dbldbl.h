#ifndef DBLDBL_H
#define DBLDBL_H

#include <iostream>

std::ostream& operator<<(std::ostream& s, const class DblDblClass& val);

int cvtStrToVal(char const*, DblDblClass&);

class DblDblClass {
	public:
		DblDblClass(double d0 = 0, double d1 = 0);
		~DblDblClass();
		int operator==(const DblDblClass& val) const;
		int operator>(const DblDblClass& val) const;
		friend std::ostream& operator<<(std::ostream& s, const DblDblClass& val);
		friend int cvtStrToVal(char const*, DblDblClass&);
		double getDbl(int i);
		void setX(double xval);
		void setY(double yval);
		double x() const;
		double y() const;
	private:
		double dval0, dval1;
};

#endif
