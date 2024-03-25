// *************************************
// C++ routines for this program are taken from
// a translation of the FORTRAN code written by
// U.S. Department of Commerce NTIA/ITS
// Institute for Telecommunication Sciences
// *****************
// Irregular Terrain Model (ITM) (Longley-Rice)
// *************************************

double ITMAreadBLoss(long ModVar,
		     double deltaH,
		     double tht_m,
		     double rht_m,
		     double dist_km,
		     int TSiteCriteria,
		     int RSiteCriteria,
		     double eps_dielect,
		     double sgm_conductivity,
		     double eno_ns_surfref,
		     double frq_mhz,
		     int radio_climate,
		     int pol,
		     double pctTime,
		     double pctLoc,
		     double pctConf);
