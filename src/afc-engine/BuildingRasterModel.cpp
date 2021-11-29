#include "BuildingRasterModel.h"
#include "rkflogging/Logging.h"
#include <QtCore>
#include <limits>
#include <cmath>
#include <stdexcept>

// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "BuildingRasterModel")

BuildingRasterModel::BuildingRasterModel(const QString& modelDir, double minlat, double minlon, double maxlat, double maxlon)
{
    if (modelDir.isEmpty())
    {
        throw std::invalid_argument("modelDir is empty");
    }
    // get directory and all .tiff files in it
    QDir dataDir(modelDir);
	QStringList entries = dataDir.entryList(QStringList("*.tiff"));

	GDALAllRegister();
    this->bounds = QRectF(QPointF(minlon, maxlat), QPointF(maxlon, minlat)); // top left and bottom right
    LOGGER_INFO(logger) << "Loading building raster file";
    this->models = std::vector<RasterModel>();

    // loop through tiff files and add them to cache if they are in bounding box
    foreach(QString entry, entries)
    {
        GDALDataset* data = static_cast<GDALDataset*>(GDALOpen(dataDir.filePath(entry).toStdString().c_str(), GA_ReadOnly));

        // https://gdal.org/api/gdaldataset_cpp.html#_CPPv4N11GDALDataset15GetGeoTransformEPd
        // linear transform from pizel/line (P/L) raster space to projection coordinates (Xp, Yp):
        // in a north up image there is no rotation so transform[2] and transform[4] = 0
        // Xp = padfTransform[0] + P*padfTransform[1] + L*padfTransform[2];
        // Yp = padfTransform[3] + P*padfTransform[4] + L*padfTransform[5];
        // [0] = startX
        // [1] = pixel width (degrees longitude per pixel)
        // [2] = always 0 for not up image (not used)
        // [3] = startY
        // [4] = always 0 for not up image (not used)
        // [5] = pixel height (degrees latitude per pixel)
        double transform[6];
        CPLErr readError = data->GetGeoTransform(transform);
        if (readError != CPLErr::CE_None) {
                throw std::runtime_error(QString("BuildingRasterModel::BuildingRasterModel(): Failed to read transform data from %1, throwing CPLErr = %2")
                                             .arg(entry)
                                             .arg(readError)
                                             .toStdString());
        }

        RasterModel m;

        int xsize = data->GetRasterXSize();
        int ysize = data->GetRasterYSize();

        // the tileRect is used to identify the tile
        m.bounds = QRectF(transform[0], transform[3], xsize*transform[1], ysize*transform[5]);
        if (m.bounds.intersects(this->bounds))
        {
            m.model = data;
            m.xres = transform[1];
            m.yres = transform[5];
            m.nodata = data->GetRasterBand(1)->GetNoDataValue();
            this->models.push_back(m);
            LOGGER_DEBUG(logger) << "Building tile added:" << entry;
        } 
        else
        {
            // release the gdal resource handle if we don't actually need to read from it
            GDALClose(data);
        }
    }
}

BuildingRasterModel::~BuildingRasterModel()
{
    foreach(RasterModel m, models)
	{
		GDALClose(m.model);
	}
}

std::pair<BuildingRasterModel::HeightResult, double> BuildingRasterModel::getHeight(const double& latDeg, const double& lonDeg) const
{
    for (RasterModel m : this->models)
    {
        if (m.bounds.contains(lonDeg, latDeg))
        {
            float height;

            // calculate index values. note that origin is at top left
            // here we are doing no interpolation. Interpolation may be a good addition.
            int xStart = floor((lonDeg - m.bounds.left()) / m.xres);
            int yStart = floor((latDeg - m.bounds.top()) / m.yres);

            // read height from file
            CPLErr readError = m.model->GetRasterBand(1)->RasterIO(GF_Read, xStart, yStart, 1, 1, &height, 1, 1, GDT_Float32, 0, 0);

            // check if there was a read error
            if (readError != CPLErr::CE_None) {
                throw std::runtime_error(QString("BuildingRasterModel::getHeight(): Failed to read raster data from %1 %2, throwing CPLErr = %3")
                                             .arg(latDeg)
                                             .arg(lonDeg)
                                             .arg(readError)
                                             .toStdString());
            }
            // We want to distinguish between a point that is in the boundary area of building data, but has no building at that point [BuildingRasterModel::nodata], and a point that is 
            // outside the boundary area of building data [NaN].
            else if (height == m.nodata)
            {
                // return std::numeric_limits<double>::quiet_NaN();
                return std::pair<BuildingRasterModel::HeightResult, double>(HeightResult::NO_BUILDING, std::numeric_limits<double>::quiet_NaN());
            }
            else
            {
                // return height data
                return std::pair<BuildingRasterModel::HeightResult, double>(HeightResult::BUILDING, static_cast<double>(height));
            }
        }
    }
    // lat/lon not in building region
    return std::pair<BuildingRasterModel::HeightResult, double>(HeightResult::OUTSIDE_REGION, std::numeric_limits<double>::quiet_NaN());
}

std::vector<QRectF> BuildingRasterModel::getBounds() const
{
    std::vector<QRectF> boundsList = std::vector<QRectF>();
    for (RasterModel m : this->models)
    {
        boundsList.push_back(m.bounds);
    }
    return boundsList;
}
