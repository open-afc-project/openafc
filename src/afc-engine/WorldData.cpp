#include <QtGui>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <QPoint>

#include <rkflogging/Logging.h>

#include "WorldData.h"

// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "GlobeModel")

short WorldData::valueAtLatLon(const double& latDeg, const double& lonDeg) const
{
    // loop through tiles, at most there will be 16 so not many
    for (TileData tile : this->tiles)
    {
        if (tile.bounds.contains(lonDeg, latDeg)) // lon,lat coordinates
        {
            short height;

            // calculate index values. note that origin is at top left
            // here we are doing no interpolation. Interpolation may be a good addition.
            int xStart = floor((lonDeg - tile.bounds.left()) / tile.xres);
            int yStart = floor((latDeg - tile.bounds.top()) / tile.yres);

            // read height from file
            CPLErr readError = tile.tile->GetRasterBand(1)->RasterIO(GF_Read, xStart, yStart, 1, 1, &height, 1, 1, GDT_Int16, 0, 0);

            // check if there was a read error
            if (readError != CPLErr::CE_None) {
                throw std::runtime_error(QString("BuildingRasterModel::getHeight(): Failed to read raster data from %1 %2, throwing CPLErr = %3")
                                             .arg(latDeg)
                                             .arg(lonDeg)
                                             .arg(readError)
                                             .toStdString());
            }
            else
            {
                // return height data
                return height;
            }
        }
    }
    return WorldData::NO_DATA;
}

WorldData::WorldData(const QDir& globeDir, const double& latmin, const double& lonmin, const double& latmax, const double& lonmax)
{
    LOGGER_INFO(logger) << "Loading globe files...";
    GDALRegister_EHdr();
    this->bounds = QRectF(QPoint(lonmin, latmax), QPoint(lonmax, latmin));

	// filter for .bil files
	QStringList files = globeDir.entryList(QStringList("*.bil")); 

    this->tiles = std::vector<TileData>();
	for (QString file: files)
	{
        LOGGER_DEBUG(logger) << "considering " << file;
		GDALDataset* data = static_cast<GDALDataset*>(GDALOpen(globeDir.filePath(file).toStdString().c_str(), GA_ReadOnly));
		int ysize = data->GetRasterYSize();
		int xsize = data->GetRasterXSize();
        double transform[6];
        CPLErr readError = data->GetGeoTransform(transform);
        if (readError != CPLErr::CE_None) 
        {
            throw std::runtime_error(QString("WorldData::WorldData(): Failed to read transform data from %1, throwing CPLErr = %2")
                                            .arg(file)
                                            .arg(readError)
                                            .toStdString());
        }

		TileData tile;
        tile.bounds = QRectF(transform[0], transform[3], xsize*transform[1], ysize*transform[5]);
        if (tile.bounds.intersects(this->bounds))
        {
            LOGGER_INFO(logger) << "Addding globe tile: " << file;
            tile.tile = data;
            tile.xres = transform[1];
            tile.yres = transform[5];
            this->tiles.push_back(tile);
        }
        else
        {
            // release the gdal resource handle if we don't actually need to read from it
            GDALClose(data);
        }
	}
    LOGGER_INFO(logger) << this->tiles.size() << " GLOBE files loaded.";
}

WorldData::~WorldData()
{
    LOGGER_DEBUG(logger) << "Destroying WorldData";
	for (TileData tile : this->tiles)
	{
		GDALClose(tile.tile);
	}
}