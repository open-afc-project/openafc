#ifndef GDAL_IMAGE_FILE_H
#define GDAL_IMAGE_FILE_H

#include "GeodeticCoord.h"
#include <QString>
#include "gdal/gdal_priv.h"
#include "gdal/ogr_spatialref.h"
#include <armadillo>

class GdalImageFile {
public:
    static constexpr double NO_DATA = -9999.9999;
    
    GdalImageFile(const QString &filename);
    ~GdalImageFile();
    void loadData();

    double getValue(const GeodeticCoord &pt) const;
    bool containsPoint(const GeodeticCoord &ptr) const;
    
    GeodeticCoord topRight() const;
    GeodeticCoord bottomLeft() const;
    GeodeticCoord topLeft() const;
    GeodeticCoord bottomRight() const;
    
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
    double *_rawData;
    int _xsize, _ysize, _stride;
    bool _loadedData;
};

#endif
