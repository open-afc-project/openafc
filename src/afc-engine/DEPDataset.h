#ifndef DEP_DATASET_H
#define DEP_DATASET_H

#include "gdal/gdal_priv.h"
#include <QtCore>
// Loggers
#include "rkflogging/ErrStream.h"
#include "rkflogging/Logging.h"
#include "rkflogging/LoggingConfig.h"

class DEPDatasetClass {
	public:
		DEPDatasetClass(std::string directoryVal, int pointsPerDegreeVal, bool fastFlagVal);
		~DEPDatasetClass();

		int readRegion(double minlat, double minlon, double maxlat, double maxlon); ///> Loads only the DEP files which are in the provided box.

		void readFile(std::string filename, int lon, int lat);

		float getHeight(double lat, double lon, int *hashKey = (int *) NULL, float **hashHeight = (float **) NULL) const;

		static int latlonToHashDirect(int floorLat, int floorLon);
		float INVALID_HEIGHT;

	private:

		// fastFlag operates as follows:
		// true:  Data for each tile is read before simulation starts.  Heavily intensive on RAM, not possible to use for 
		//        full CONUS simulation.  Execution is very fast and is thread safe.
		// false: Individual height values are read from the datafile when needed.  Uses very little RAM, can use for 
		//        full CONUS simulation or even larger.  Execution is slow and is NOT thread safe.
		bool fastFlag;

		std::string directory;
		int pointsPerDegree;
		int size;
		int overlap;
		double resolution;

		QHash <int, GDALDataset *> _cachedDataGDAL;
		QHash <int, float *> _cachedDataFast;
		// QHash <int, QString> _availableData;

		// QReadWriteLock _lock;
		// QQueue <int> _cachedOrder;
		QHash <int, QReadWriteLock *> _mutexes;

		// QMutex _globalMutex;
};

#endif
