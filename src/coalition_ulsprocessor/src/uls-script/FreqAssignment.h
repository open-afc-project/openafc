#ifndef FREQ_ASSIGNMENT_H
#define FREQ_ASSIGNMENT_H

#include <iostream>
#include <string>
#include <regex>
#include <tuple>
#include "global_fn.h"

class FreqAssignmentClass
{
	public:
		FreqAssignmentClass(const std::string freqAssignmentFile);
		double getBandwidthUS(double freqMHz);

	private:
		void readFreqAssignment(const std::string filename);

		std::vector<std::tuple<double, double>> freqBWList;
};

#endif
