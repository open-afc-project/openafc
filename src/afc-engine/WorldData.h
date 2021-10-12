#ifndef GLOBE_MODEL_H
#define GLOBE_MODEL_H

#include <vector>
#include <QRectF>
#include <QDir>
#include "gdal/gdal_priv.h"



class WorldData {
public:

    //WorldData(const char **datafiles, const QRectF *rects, int nump) {};
    WorldData(const QDir& globeDir, const double& latmin=-90, const double& lonmin=-180, const double& latmax=90, const double& lonmax=180);
    ~WorldData();


	short valueAtLatLon(const double& latDeg, const double& lonDeg) const;

    const static int NO_DATA = -500; // this is define by NOAA
    bool overOcean(const double& latDeg, const double& lonDeg) const
    {
        return valueAtLatLon(latDeg, lonDeg) == WorldData::NO_DATA;
    };

private:

    struct TileData
    {
        GDALDataset* tile;
        QRectF bounds;
        double xres;
        double yres;
    };

    // values shared across all tiles
    QRectF bounds;

    std::vector<TileData> tiles;
};

#endif
