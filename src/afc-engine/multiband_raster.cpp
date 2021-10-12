#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <QString>
#include <QRectF>
#include <QtCore>
#include <QDir>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <mutex>

#include "rkflogging/Logging.h"
#include "rkflogging/ErrStream.h"

#include "global_defines.h"
#include "multiband_raster.h"

namespace 
{
    // Logger for all instances of class
    LOGGER_DEFINE_GLOBAL(logger, "multiband_raster")

} // end namespace

const StrTypeClass MultibandRasterClass::strHeightResultList[] = {
    {   MultibandRasterClass::OUTSIDE_REGION, "OUTSIDE_REGION" },
    {          MultibandRasterClass::NO_DATA, "NO_DATA"        },
    {      MultibandRasterClass::NO_BUILDING, "NO_BUILDING"    },
    {         MultibandRasterClass::BUILDING, "BUILDING"       },

    {                                     -1,  (char *) 0      }

};

std::mutex gdalMtx;

/******************************************************************************************/
/**** MultibandRasterClass::MultibandRasterClass                                       ****/
/******************************************************************************************/
MultibandRasterClass::MultibandRasterClass(const std::string& rasterFile, CConst::LidarFormatEnum formatVal)
    : format(formatVal)
{
    std::ostringstream errStr;
    if (rasterFile.empty()) {
        throw std::invalid_argument("rasterFile is empty");
    }

    GDALAllRegister();

    gdalDataset = static_cast<GDALDataset*>(GDALOpen(rasterFile.c_str(), GA_ReadOnly));

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
    CPLErr readError = gdalDataset->GetGeoTransform(transform);
    if (readError != CPLErr::CE_None) {
        errStr << "ERROR: MultibandRasterClass::MultibandRasterClass(): Failed to read transform data from "
               << rasterFile << ", throwing CPLErr = " << readError;
        throw std::runtime_error(errStr.str());
    }

    int xsize = gdalDataset->GetRasterXSize();
    int ysize = gdalDataset->GetRasterYSize();

    // the tileRect is used to identify the tile
    bounds = QRectF(transform[0], transform[3], xsize*transform[1], ysize*transform[5]);
    xres = transform[1];
    yres = transform[5];
    nodataBE = gdalDataset->GetRasterBand(1)->GetNoDataValue();
    nodataBldg = gdalDataset->GetRasterBand(2)->GetNoDataValue();
    LOGGER_INFO(logger) << "Raster tile added: " << rasterFile;
}
/******************************************************************************************/

/******************************************************************************************/
/**** MultibandRasterClass::~MultibandRasterClass                                      ****/
/******************************************************************************************/
MultibandRasterClass::~MultibandRasterClass()
{
    GDALClose(gdalDataset);
}
/******************************************************************************************/

/******************************************************************************************/
/**** MultibandRasterClass::contains()                                                 ****/
/******************************************************************************************/
bool MultibandRasterClass::contains(const double& lonDeg, const double& latDeg)
{
    return(bounds.contains(lonDeg, latDeg));
}
/******************************************************************************************/

/******************************************************************************************/
/**** MultibandRasterClass::getHeight()                                                ****/
/******************************************************************************************/
void MultibandRasterClass::getHeight(const double& latDeg, const double& lonDeg, double& terrainHeight, double& bldgHeight, HeightResult& heightResult) const
{
    std::ostringstream errStr;
    if (bounds.contains(lonDeg, latDeg)) {
        float terrainHeightFloat;
        float bldgHeightFloat;

        // calculate index values. note that origin is at top left
        // here we are doing no interpolation. Interpolation may be a good addition.
        int xStart = floor((lonDeg - bounds.left()) / xres);
        int yStart = floor((latDeg - bounds.top()) / yres);

        /**********************************************************************************/
        /**** BAND 1: Bare Earth                                                       ****/
        /**********************************************************************************/
        gdalMtx.lock();
        CPLErr readError = gdalDataset->GetRasterBand(1)->RasterIO(GF_Read, xStart, yStart, 1, 1, &terrainHeightFloat, 1, 1, GDT_Float32, 0, 0);
        gdalMtx.unlock();

        // check if there was a read error
        if (readError != CPLErr::CE_None) {
            errStr << "ERROR: MultibandRasterClass::getHeight(): Failed to read BARE EARTH raster data from " << latDeg << " " << lonDeg << ", throwing CPLErr = " << readError;
            throw std::runtime_error(errStr.str());
        } else if (terrainHeightFloat == nodataBldg) {
            heightResult  = NO_DATA,
            terrainHeight = std::numeric_limits<double>::quiet_NaN();
            bldgHeight    = std::numeric_limits<double>::quiet_NaN();
        } else {
            terrainHeight = static_cast<double>(terrainHeightFloat);

            /**********************************************************************************/
            /**** BAND 2: Building                                                         ****/
            /**********************************************************************************/
            // We want to distinguish between:
            // * a point that is in the boundary area of building data, but has no building at that point [BuildingRasterModel::nodata]
            // * a point that is outside the boundary area of building data [NaN].
            gdalMtx.lock();
            readError = gdalDataset->GetRasterBand(2)->RasterIO(GF_Read, xStart, yStart, 1, 1, &bldgHeightFloat, 1, 1, GDT_Float32, 0, 0);
            gdalMtx.unlock();

            // check if there was a read error
            if (readError != CPLErr::CE_None) {
                errStr << "ERROR: MultibandRasterClass::getHeight(): Failed to read BLDG raster data from " << latDeg << " " << lonDeg << ", throwing CPLErr = " << readError;
                throw std::runtime_error(errStr.str());
            } else if (bldgHeightFloat == nodataBldg) {
                switch(format) {
                    case CConst::fromVectorLidarFormat:
                        heightResult = NO_BUILDING,
                        bldgHeight = std::numeric_limits<double>::quiet_NaN();
                        break;
                    case CConst::fromRasterLidarFormat:
                        heightResult  = NO_DATA,
                        terrainHeight = std::numeric_limits<double>::quiet_NaN();
                        bldgHeight    = std::numeric_limits<double>::quiet_NaN();
                        break;
                    default:
                        CORE_DUMP;
                        break;
                }
            } else {
                switch(format) {
                    case CConst::fromVectorLidarFormat:
                        heightResult = BUILDING;
                        bldgHeight = static_cast<double>(bldgHeightFloat);
                        break;
                    case CConst::fromRasterLidarFormat:
                        if (bldgHeightFloat > terrainHeight + 1.0) {
                            heightResult = BUILDING;
                            bldgHeight = static_cast<double>(bldgHeightFloat);
                        } else {
                            heightResult = NO_BUILDING,
                            bldgHeight = std::numeric_limits<double>::quiet_NaN();
                        }
                        break;
                    default:
                        CORE_DUMP;
                        break;
                }
            }
        }
    } else {
        // lat/lon not in building region
        heightResult = OUTSIDE_REGION;
        terrainHeight = std::numeric_limits<double>::quiet_NaN();
        bldgHeight = std::numeric_limits<double>::quiet_NaN();
    }
    return;
}
/******************************************************************************************/