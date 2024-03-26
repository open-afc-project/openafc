/******************************************************************************************/
/**** FILE : prtable.h                                                                     ****/
/******************************************************************************************/

#ifndef PRTABLE_H
#define PRTABLE_H

#include <string>

using namespace std;

/******************************************************************************************/
/**** CLASS: PRTABLEClass                                                                  ****/
/******************************************************************************************/
class PRTABLEClass
{
	public:
		PRTABLEClass();
		PRTABLEClass(std::string tableFile);
		~PRTABLEClass();

		double computePRTABLE(double Q, double oneOverKs);
		double getIdx(double val, double *valList, int numVal);

	private:
		void readTable();

		std::string tableFile;
		double **prTable;
		int numOneOverKs, numQ;
		double *oneOverKsValList;
		double *QValList;
};
/******************************************************************************************/

#endif
