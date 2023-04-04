#ifndef INCLUDE_GDALDATAMODEL_H
#define INCLUDE_GDALDATAMODEL_H

#include <algorithm>
#include <iostream>
#include <iterator>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include "GdalHelpers.h"

// Uses OGR from GDAL v1.11 to read in .shp files
#include "ogrsf_frmts.h"

class GdalDataModel
{
public:
    GdalDataModel(const std::string &dataSourcePath, const std::string &heightFieldName);
    ~GdalDataModel();

    // The return object is owned by this instance of the class
    GDALDataset *getDataSource() { return _ptrDataSource; }
    // The return object is owned by this instance of the class
    OGRLayer *getLayer() { return _ptrLayer; }

    OGRSpatialReference* testSrcSpatialRef;
    OGRSpatialReference* testDestSpatialRef;
    OGRCoordinateTransformation *testCoordTransform;
    OGRCoordinateTransformation *invCoordTransform;

    double getMaxBuildingHeightAtPoint(double latDeg, double lonDeg) const;
    std::map<int64_t,double> getBuildingsAtPoint(double lat, double lon) const;

    void printDebugInfo() const;

    int heightFieldIdx;
private:
    GDALDataset *_ptrDataSource;
    OGRLayer *_ptrLayer;
};

#endif // INCLUDE_GDALDATAMODEL_H
