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

		void setName(std::string nameVal)
		{
			name = nameVal;
		}
		void setStartFreqMHz(int startFreqMHzVal)
		{
			startFreqMHz = startFreqMHzVal;
		}
		void setStopFreqMHz(int stopFreqMHzVal)
		{
			stopFreqMHz = stopFreqMHzVal;
		}

		std::string getName() const
		{
			return name;
		}
		int getStartFreqMHz() const
		{
			return startFreqMHz;
		}
		int getStopFreqMHz() const
		{
			return stopFreqMHz;
		}

	private:
		std::string name;
		int startFreqMHz;
		int stopFreqMHz;
};
/******************************************************************************************/

#endif
