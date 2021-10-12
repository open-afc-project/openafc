#include <math.h>
#include <stdexcept>
#include <iostream>
#include <tuple>

#include <QtConcurrentMap>
#include <QString>
#include <QChar>
#include "GdalDataDir.h"
#include "rkflogging/Logging.h"

// NXWY files are stepped from X - 0.5" to X + 1d0.5" and Y - 0.5" to Y + 1d0.5"

// Logger for all instances of class
namespace 
{
	LOGGER_DEFINE_GLOBAL(logger, "GdalDataDir");
}

// converts lat/lon coordinates into an index to be used by cache structure
inline int latLonToHashDirect(int lat, int lon) {
	return lat * 10000 + lon;
}

std::vector<std::tuple<QString, int, int>> getFileNames(const QDir& dataDir, const QRectF& bounds)
{
	auto entries = std::vector<std::tuple<QString, int, int>>();
	for (int lat = static_cast<int>(floor(bounds.left())); lat < static_cast<int>(ceil(bounds.right())); lat++)
	{
		for (int lon = static_cast<int>(floor(bounds.top())); lon < static_cast<int>(ceil(bounds.bottom())); lon++)
		{
			QString entry = QString("%1%2%3%4.hgt")
			.arg(lat >= 0 ? "N" : "S")
			.arg(abs(lat), 2, 10, QChar('0')) // .arg(value, fillwidth, base, fillchar)
			.arg(lon >= 0 ? "E" : "W")
			.arg(abs(lon), 3, 10, QChar('0'));

			if (dataDir.exists(entry))
			{
				entries.push_back(std::make_tuple(entry, lat, lon));
			}
			else
			{
				LOGGER_WARN(logger) << "Could not find SRTM tile: " << entry;
			}
		}
	}
	return entries;
}

// Loads whole globe from srtm directory by default
GdalDataDir::GdalDataDir(QString dataDirectory) : GdalDataDir(dataDirectory, -90, -180, 90, 180) {}

// Load srtm data that is within bounding box
GdalDataDir::GdalDataDir(QString dataDirectory, double minlat, double minlon, double maxlat, double maxlon){
	QDir dataDir(dataDirectory);
	this->SIZE = 0; // initalize to indicate that this needs to be updated

	GDALAllRegister();

	// only read in srtm files that match the regex, and lon/lat params
    int numRead = 0;
	this->_cachedData  = QHash<int, GDALDataset*>(); 
    QRectF rf(QPointF(minlat, minlon), QPointF(maxlat, maxlon));
	LOGGER_INFO(logger) << "Loading srtm files in lat:" << minlat << " - " << maxlat << ", lon:" << minlon << " - " << maxlon;
	// Check for .hgt file with the following filename structure:
	// (N|S)	check for 'N' or 'S'
	// (\\d+)	followed by 1 or more digits
	// (E|W)	followed by 'E' or 'W'
	// (\\d+)	followed by 1 or more digits
	// .hgt		HGT file extension
	//static QRegExp fileRx("(N|S)(\\d+)(E|W)(\\d+).hgt");
	for (std::tuple<QString, int, int> entry : getFileNames(dataDir, rf)) 
	{

		QString file;
		int lat, lon;
		std::tie(file, lat, lon) = entry;

		// srtm file name specifies lat/lon of geometric center of bottom left corner pixel
		LOGGER_DEBUG(logger) << "Tile intersects at " << lat << ", " << lon;

		/// Read the tile.
		// generate file hash and store GDAL object handle
		
		int key = latLonToHashDirect(lat, lon);
		GDALDataset* buffer = static_cast<GDALDataset*>(GDALOpen(dataDir.filePath(file).toStdString().c_str(), GA_ReadOnly));
		LOGGER_DEBUG(logger) << lat << " " << lon << " assigned key " << key;

		// set values if they are empty
		if (this->SIZE == 0)
		{
			double transform[6];
			buffer->GetGeoTransform(transform); // transform defines linear transformation from lon/lat to pixels
			this->INVALID_HEIGHT = buffer->GetRasterBand(1)->GetNoDataValue();
			this->INV_STEP_INT = floor(1/transform[1]);
			this->INV_STEP = (double)this->INV_STEP_INT;
			this->SIZE = buffer->GetRasterBand(1)->GetXSize(); // x and y size are equal with SRTM
		}

		this->_cachedData.insert(key, buffer);
		LOGGER_INFO(logger) << "[" << numRead << "] read file: " << file;

		numRead++;
	}
	
    LOGGER_DEBUG(logger) << "Finished reading srtm files"; 
}

bool GdalDataDir::getHeight(qint16& height, const double& lat, const double& lon) const{

	// Critical Section
	const int floorLat = static_cast<int>(floor(lat));
	const int floorLon = static_cast<int>(floor(lon));
    const int shiftFloorLat = floorLat * this->INV_STEP_INT;
    const int shiftFloorLon = floorLon * this->INV_STEP_INT;
    const int shiftLat = lat * this->INV_STEP;
    const int shiftLon = lon * this->INV_STEP;
    const int latIndex = this->SIZE - 1 - (shiftLat - shiftFloorLat);
    const int lonIndex = (shiftLon - shiftFloorLon);

	int key = latLonToHashDirect(floorLat, floorLon);

	QHash<int, GDALDataset *>::const_iterator dataSet = _cachedData.constFind(key);
	if (dataSet == _cachedData.constEnd()) 
	{
		// not found
		return false;
	}

	CPLErr readError = dataSet.value()->GetRasterBand(1)->RasterIO(GF_Read, lonIndex, latIndex, 1, 1, &height, 1, 1, GDT_Int16, 0, 0);
	if (readError != CPLErr::CE_None) 
	{
		throw std::runtime_error(QString("GdalDataDir::readFile(): Failed to read raster data from %1, throwing CPLErr = %2")
			.arg(key).arg(readError).toStdString());
	}
	else if (height == this->INVALID_HEIGHT)
	{
		return false;
	}
	else 
	{
		// return correct height data
		return true;
	}
}

qint16 GdalDataDir::getInvalidHeight() {
	return this->INVALID_HEIGHT;
}

GdalDataDir::~GdalDataDir()
{
	foreach(GDALDataset *poDataset, _cachedData.values())
	{
		GDALClose(poDataset);
	}
}
