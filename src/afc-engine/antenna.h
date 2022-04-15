/******************************************************************************************/
/**** FILE: antenna.h                                                                  ****/
/******************************************************************************************/

#ifndef ANTENNA_H
#define ANTENNA_H

#include <vector>

class LinInterpClass;

class AntennaClass
{
	public:
		AntennaClass(int type, const char *strid = (char *) NULL);
		~AntennaClass();
		static std::vector<AntennaClass *> readMultipleBoresightAntennas(std::string filename);
		int readFile(const char *filepath, const char *filename);
		void setBoresightGainTable(LinInterpClass *offBoresightGainTableVal);
		double gainDB(double dx, double dy, double dz, double h_angle_rad);
		double gainDB(double phi, double theta);
		double gainDB(double theta);
		int checkGain(const char *flname, int orient, int numpts);
		char *get_strid();
		int get_type();
		int get_is_omni();
		char *get_filename();
		double h_width;
		static int color;
		double vg0;

	private:
		char *filename, *strid;
		int type, is_omni;
		double tilt_rad;
		double gain_fwd_db;   /* gain_v( tilt_rad      ) */
		double gain_back_db;  /* gain_v( PI - tilt_rad ) */
		LinInterpClass *horizGainTable;
		LinInterpClass *vertGainTable;
		LinInterpClass *offBoresightGainTable;
};
/******************************************************************************************/

#endif
