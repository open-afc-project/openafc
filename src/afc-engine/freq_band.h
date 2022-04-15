/******************************************************************************************/
/**** FILE : freq_band.h                                                                 ****/
/******************************************************************************************/

#ifndef FREQ_BAND_H
#define FREQ_BAND_H

#include "cconst.h"
#include "Vector3.h"

/******************************************************************************************/
/**** CLASS: FreqBandClass                                                             ****/
/******************************************************************************************/
class FreqBandClass
{
	public:
		FreqBandClass(std::string nameVal, int startFreqMHzVal, int stopFreqMHzVal);
		~FreqBandClass();

		void setName(std::string nameVal) { name = nameVal; }
		void setStartFreqMHz(int startFreqMHzVal) { startFreqMHz = startFreqMHzVal; }
		void setStopFreqMHz(int stopFreqMHzVal) { stopFreqMHz = stopFreqMHzVal; }

		std::string getName() { return name; }
		int getStartFreqMHz() { return startFreqMHz; }
		int getStopFreqMHz() { return stopFreqMHz; }

	private:
		std::string name;
		int startFreqMHz;
		int stopFreqMHz;
};
/******************************************************************************************/

#endif
