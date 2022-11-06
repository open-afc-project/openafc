/******************************************************************************************/
/**** FILE : nfa.h                                                                     ****/
/******************************************************************************************/

#ifndef NFA_H
#define NFA_H

#include <string>

using namespace std;

/******************************************************************************************/
/**** CLASS: NFAClass                                                                  ****/
/******************************************************************************************/
class NFAClass
{
public:
	NFAClass();
	NFAClass(std::string tableFile);
	~NFAClass();

	double computeNFA(double xdb, double u, double eff);

private:
	void readTable();

	std::string tableFile;
	double ***nfaTable;
	int numxdb, numu, numeff;
	double xdbStart, uStart, effStart;
	double xdbStep, uStep, effStep;
};
/******************************************************************************************/

#endif
