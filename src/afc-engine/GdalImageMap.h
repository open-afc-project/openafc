#ifndef GDAL_IMAGE_MAP_H
#define GDAL_IMAGE_MAP_H

#include "GdalImageFile.h"

class GdalImageMap {
public:
    GdalImageMap(const QList<QString> &fileNames, const GeodeticCoord &topRight, const GeodeticCoord &bottomLeft);

    double getValue(const GeodeticCoord &gc) const;

    void printBB() const;

private:
    QList<GdalImageFile *> _files;
    QList<GeodeticCoord> _topRights;
    QList<GeodeticCoord> _bottomLefts;
};

#endif
