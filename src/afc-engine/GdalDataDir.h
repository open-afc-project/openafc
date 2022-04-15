#ifndef GDAL_DATADIR_H
#define GDAL_DATADIR_H

#include "gdal/gdal_priv.h"
#include <QtCore>

class GdalDataDir {
	public:
		// Loads all the SRTM files in the provided directory. not reccomended because many files
		GdalDataDir(QString dataDirectory);

		// Loads SRTM raster files that intersect the given bounds
		GdalDataDir(QString dataDirectory, double minlat, double minlon, double maxlat, double maxlon);

		~GdalDataDir();

		// gets the terrain height at the lat/lon coordinates. If successful stores result in height with 'true' returned, but if no data is found then 'false' is returned
		bool getHeight(qint16& height, const double& lat, const double& lon) const;

		qint16 getInvalidHeight();

	private:
		qint16 INVALID_HEIGHT; // defined by srtm

		// 90 m SRTM data = 3 arcseconds
		// 30 m SRTM data = 1 arcsecond
		int SIZE; 			// size of raster square in pixels
		int INV_STEP_INT;	// pixels per degree
		double INV_STEP;		// pixels per degree

		QHash <int, GDALDataset*> _cachedData;

};

#endif
