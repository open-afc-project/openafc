#ifndef GDAL_IMAGE_FILE2_H
#define GDAL_IMAGE_FILE2_H

#include "GeodeticCoord.h"
#include <QString>
#include "gdal/gdal_priv.h"
#include "gdal/ogr_spatialref.h"
#include <armadillo>

class GdalImageFile2 {
public:
    static constexpr unsigned char NO_DATA = 0;
    
    GdalImageFile2(const QString &filename, int tileSizeXVal, int tileSizeYVal);
    ~GdalImageFile2();

    void loadTile(int tileXIdxVal, int tileYIdxVal);
    unsigned char getValue(const GeodeticCoord &pt);
    unsigned char getValue(int xIdx, int yIdx);
    bool containsPoint(const GeodeticCoord &ptr) const;
    
    GeodeticCoord topRight() const;
    GeodeticCoord bottomLeft() const;
    GeodeticCoord topLeft() const;
    GeodeticCoord bottomRight() const;

    void xyIdxtoLonLat(const int& xIdx, const int& yIdx, GeodeticCoord &pt);
    void lonlatToXY(const GeodeticCoord &pt, int& xIdx, int& yIdx);
    int getNumTileX() const;
    int getNumTileY() const;
    int getSizeX() const;
    int getSizeY() const;

private:
    GDALDataset *_dataset;
    OGRSpatialReference *_spatialReferenceProjection;
    OGRSpatialReference *_geographicReference;
    OGRCoordinateTransformation *_transform;
    OGRCoordinateTransformation *_invTransform;
    GDALRasterBand *_rasterBand;

    QString _filename;
    double *_rawTransform;
    double _extentXMax, _extentXMin, _extentYMax, _extentYMin;
    double _invTransform1, _invTransform5;

    GeodeticCoord _topRight, _bottomRight, _topLeft, _bottomLeft;
    unsigned char *_rawData;
    int _xsize, _ysize, _stride;
    bool _loadedData;

    int tileSizeX, tileSizeY;
    int numTileX, numTileY;
    int tileXIdx, tileYIdx;
};

#endif
