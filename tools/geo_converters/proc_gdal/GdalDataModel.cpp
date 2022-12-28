#include <iomanip>

#include "GdalDataModel.h"

/******************************************************************************************/
/* CONSTRUCTOR GdalDataModel::GdalDataModel                                               */
/******************************************************************************************/
GdalDataModel::GdalDataModel(const std::string &dataSourcePath, const std::string &heightFieldName)
{
    // Registers necessary drivers
    OGRRegisterAll();

    // _ptrDataSource = OGRSFDriverRegistrar::Open(dataSourcePath.c_str(), FALSE);

    _ptrDataSource = (GDALDataset *) GDALOpenEx(dataSourcePath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL );

    // Check if data source was successfully opened
    if( _ptrDataSource == nullptr )
        throw std::runtime_error("Failed to open OGR data source at " + dataSourcePath);

    // Check if data source contains exactly one 1 layer
    if (_ptrDataSource->GetLayerCount() != 1) {
        throw std::runtime_error("GdalDataModel::GdalDataModel(): There may be undefined behavior if data source contains more than 1 layer");
    }

    _ptrLayer = _ptrDataSource->GetLayer(0); // Data source has a 1:1 relationship with layer

    // Extract spatial reference datum from current layer
    testSrcSpatialRef = new OGRSpatialReference();
    testSrcSpatialRef->SetWellKnownGeogCS("WGS84");

    // Create new spatial reference to transform current layer's coordinates into our preferred coordinate system
    // GDAL maintains ownership of returned pointer, so we should not manage its memory
    testDestSpatialRef = _ptrLayer->GetSpatialRef();

    // Prepare for coordinate transform
    testCoordTransform = OGRCreateCoordinateTransformation(testSrcSpatialRef, testDestSpatialRef);
    invCoordTransform = OGRCreateCoordinateTransformation(testDestSpatialRef, testSrcSpatialRef);

    if (!heightFieldName.empty()) {
        std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> ptrOFeature;
        ptrOFeature = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(_ptrLayer->GetNextFeature());
        heightFieldIdx = ptrOFeature->GetFieldIndex(heightFieldName.c_str());

        if (heightFieldIdx == -1) {
            throw std::runtime_error(std::string("GdalDataModel::GdalDataModel(): ERROR: data contains no height field \"") + heightFieldName + "\"");
        }
    } else {
        heightFieldIdx = -1;
    }
};
/******************************************************************************************/

/******************************************************************************************/
/* DESTRUCTOR GdalDataModel::GdalDataModel                                                */
/******************************************************************************************/
GdalDataModel::~GdalDataModel()
{
    // Memory de-allocation
    delete testSrcSpatialRef;
    OGRCoordinateTransformation::DestroyCT(testCoordTransform);
    OGRCoordinateTransformation::DestroyCT(invCoordTransform);
};
/******************************************************************************************/

/***********************************/
/* getMaxBuildingHeightAtPoint     */
/***********************************/
// Looks at 2.5D data (buildings with multiple heights/polygons) and returns the highest possible height for a given test point (ensures accuracy)
double GdalDataModel::getMaxBuildingHeightAtPoint(double latDeg, double lonDeg) const
{
    double maxHeight_m = std::numeric_limits<double>::quiet_NaN(); // Instantiate as NaN in case no building data exists at input lat/lon

    const auto buildingMap = getBuildingsAtPoint(latDeg, lonDeg);

    if (!buildingMap.empty()) {
        const auto maxBuildingPair_m = std::max_element
        (
            buildingMap.begin(), buildingMap.end(),
            [](const std::pair<int64_t, double>& p1, const std::pair<int64_t, double>& p2) 
            {
                return p1.second < p2.second; 
            }
        );

        maxHeight_m = maxBuildingPair_m->second;
    }

    return maxHeight_m;
}

/***********************************/
/* getBuildingsAtPoint             */
/***********************************/
// Return a map of OGRGeometry FIDs and the heights associated with each of those OGRGeometries
std::map<int64_t,double> GdalDataModel::getBuildingsAtPoint(double lat, double lon) const
{ // If in a building return a polygon, if not then return NULL
    // Ensure loop begins with first feature
    _ptrLayer->ResetReading();

    // Extract spatial reference datum from current layer
    OGRSpatialReference *tmpSrcSpatialRef = new OGRSpatialReference();
    tmpSrcSpatialRef->SetWellKnownGeogCS("WGS84");
    // Create new spatial reference to transform current layer's coordinates into our preferred coordinate system
    // GDAL maintains ownership of returned pointer, so we should not manage its memory
    OGRSpatialReference* tmpDestSpatialRef = _ptrLayer->GetSpatialRef();

    // Prepare for coordinate transform
    OGRCoordinateTransformation *tmpCoordTransform = OGRCreateCoordinateTransformation(tmpSrcSpatialRef, tmpDestSpatialRef);

    int transformSuccess;
    // Perform transform into WGS-84 lat/lon coordinate system
    int allTransformSuccess = tmpCoordTransform->TransformEx(1, &lon, &lat, nullptr, &transformSuccess);

    // Store input point projected into data source's spatial reference
    GdalHelpers::GeomUniquePtr<OGRPoint> testPoint(GdalHelpers::createGeometry<OGRPoint>()); // Create OGRPoint on heap
    testPoint->setX(lon); testPoint->setY(lat); // lat and lon have been transformed into the layer's spatial reference

    // Filter for features/geometries which geographically intersect with the test point
    if (testPoint.get()->IsValid()) {
        _ptrLayer->SetSpatialFilter(testPoint.get()); // FIXME: Does this shrink layer to only include features which intersected with a previous test point?
    } else { 
        throw std::runtime_error("GdalDataModel::getBuildingsAtPoint(): OGRPoint testPoint returned invalid.");
    }

    std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> ptrOFeature;
    // Iterate over features defined in current layer
    std::map<int64_t,double> returnMap;
    while ((ptrOFeature = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(_ptrLayer->GetNextFeature())) != NULL) {
        // GDAL maintains ownership of returned pointer, so we should not manage its memory
        OGRGeometry *ptrOGeometry = ptrOFeature->GetGeometryRef();
        
        // Check whether the current geometry is a polygon
        if (ptrOGeometry != NULL && ptrOGeometry->getGeometryType() == OGRwkbGeometryType::wkbPolygon) {
            if (heightFieldIdx != -1) {
                returnMap.insert(std::make_pair(static_cast<int64_t>(ptrOFeature->GetFID()),ptrOFeature->GetFieldAsDouble(heightFieldIdx)));
            } else {
                returnMap.insert(std::make_pair(static_cast<int64_t>(ptrOFeature->GetFID()),0.0));
            }
        } else {
            std::cout << "GdalDataModel::getBuildingsAtPoint(): Can't find polygon geometries in current feature";
        }
    }

    // Memory de-allocation
    delete tmpSrcSpatialRef;
    OGRCoordinateTransformation::DestroyCT(tmpCoordTransform);

    // Un-do effects of spatial filter
    // _ptrLayer = _ptrDataSource->GetLayer(0);

    return returnMap;
}

void GdalDataModel::printDebugInfo() const
{
    double minLon, maxLon;
    double minLat, maxLat;
    bool minMaxFlag = true;
    int numPolygon = 0;
    std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> ptrOFeature;
    // Iterate over features defined in current layer
    std::map<int64_t,double> returnMap;
    _ptrLayer->ResetReading();

    OGREnvelope oExt;
    if (_ptrLayer->GetExtent(&oExt, TRUE) == OGRERR_NONE) {
        printf("Extent: (%f, %f) - (%f, %f)\n",
                      oExt.MinX, oExt.MinY, oExt.MaxX, oExt.MaxY);
    }

    _ptrLayer->ResetReading();
    while ((ptrOFeature = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(_ptrLayer->GetNextFeature())) != NULL) {
        // GDAL maintains ownership of returned pointer, so we should not manage its memory
        OGRGeometry *ptrOGeometry = ptrOFeature->GetGeometryRef();

        std::cout << std::setprecision(25);
        // Check whether the current geometry is a polygon
        if (ptrOGeometry != NULL && ptrOGeometry->getGeometryType() == OGRwkbGeometryType::wkbPolygon) {
            double height = (heightFieldIdx != -1 ? ptrOFeature->GetFieldAsDouble(heightFieldIdx) : 0.0);
            returnMap.insert(std::make_pair(static_cast<int64_t>(ptrOFeature->GetFID()),height));

            OGRPolygon *poly = (OGRPolygon *) ptrOGeometry;
#if 1
            std::cout << "POLYGON: " << numPolygon << std::endl;
            std::cout << "FEATURE ID: " << ptrOFeature->GetFID() << std::endl;
            char *wkt_tmp = (char *) NULL;
            poly->exportToWkt(&wkt_tmp);
            std::cout << wkt_tmp << std::endl;
            std::cout << "POLYGON_HEIGHT = " << height << std::endl << std::endl;

            std::cout << "NUM_FIELD: " << ptrOFeature->GetFieldCount() << std::endl;
            for(int fieldIdx=0; fieldIdx<ptrOFeature->GetFieldCount(); ++fieldIdx) {
                std::cout << "    FIELD_" << fieldIdx << ": " << ptrOFeature->GetFieldDefnRef(fieldIdx)->GetNameRef() << " = " << ptrOFeature->GetFieldAsDouble(fieldIdx) << std::endl;
            }
#endif

            OGRLinearRing *pRing = poly->getExteriorRing();
            // std::cout << "NUM_POINTS = " << pRing->getNumPoints() << std::endl << std::endl;
            for(int ptIdx=0; ptIdx<pRing->getNumPoints(); ptIdx++) {
                double lon = pRing->getX(ptIdx);
                double lat = pRing->getY(ptIdx);

                int transformSuccess;
                // Perform transform into WGS-84 lat/lon coordinate system
                int allTransformSuccess = invCoordTransform->TransformEx(1, &lon, &lat, nullptr, &transformSuccess);

#if 1
                std::cout << "    POINT " << ptIdx << " : " << pRing->getX(ptIdx) << " " << pRing->getY(ptIdx) << " " << lon << " " << lat << std::endl;
#endif

                if (minMaxFlag) {
                    minLon = lon;
                    maxLon = lon;
                    minLat = lat;
                    maxLat = lat;
                    minMaxFlag = false;
                }
                if (lon < minLon) {
                    minLon = lon;
                } else if (lon > maxLon) {
                    maxLon = lon;
                }
                if (lat < minLat) {
                    minLat = lat;
                } else if (lat > maxLat) {
                    maxLat = lat;
                }
            }

            OGREnvelope env;
            poly->getEnvelope(&env);
            std::cout << "    MIN_X " << env.MinX << std::endl;
            std::cout << "    MAX_X " << env.MaxX << std::endl;
            std::cout << "    MIN_Y " << env.MinY << std::endl;
            std::cout << "    MAX_Y " << env.MaxY << std::endl;
#if 1
            free(wkt_tmp);
            std::cout << std::endl;
#endif
        } else {
            std::cout << "GdalDataModel::getBuildingsAtPoint(): Can't find polygon geometries in current feature";
        }
        numPolygon++;
    }

    std::cout << "NUM_POLYGON = " << numPolygon << std::endl;
    std::cout << "MIN_LON = " << minLon << std::endl;
    std::cout << "MAX_LON = " << maxLon << std::endl;
    std::cout << "MIN_LAT = " << minLat << std::endl;
    std::cout << "MAX_LAT = " << maxLat << std::endl;
    std::cout << std::endl;

    double rlanLon = minLon*0.75 + maxLon*0.25;
    double rlanLat = minLat*0.75 + maxLat*0.25;
    double fsLon   = minLon*0.25 + maxLon*0.75;
    double fsLat   = minLat*0.25 + maxLat*0.75;

// fsLon = -73.98427;
// fsLat = 40.74801;

    double rlanX = rlanLon;
    double rlanY = rlanLat;
    double fsX   = fsLon;
    double fsY   = fsLat;

    int transformSuccess;
    testCoordTransform->TransformEx(1, &rlanX, &rlanY, nullptr, &transformSuccess);
    testCoordTransform->TransformEx(1, &fsX,   &fsY,   nullptr, &transformSuccess);

    std::cout << "RLAN " << " : " << rlanX << " " << rlanY << " " << rlanLon << " " << rlanLat << std::endl;
    std::cout << "FS   " << " : " <<   fsX << " " <<   fsY << " " <<   fsLon << " " <<   fsLat << std::endl;

    OGRPoint rlanPoint;
    OGRPoint   fsPoint;

    rlanPoint.setX(rlanX);
    rlanPoint.setY(rlanY);
    fsPoint.setX(fsX);
    fsPoint.setY(fsY);

    GdalHelpers::GeomUniquePtr<OGRLineString> signalPath(GdalHelpers::createGeometry<OGRLineString>());
    signalPath->addPoint(rlanX, rlanY);
    signalPath->addPoint(fsX, fsY);

    std::vector<GIntBig> idList;

    _ptrLayer->SetSpatialFilter(signalPath.get()); // Filter entire building database for only polygons which intersect with signal path into constrained layer

    numPolygon = 0;
    _ptrLayer->ResetReading();
    while ((ptrOFeature = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(_ptrLayer->GetNextFeature())) != NULL) {
        // GDAL maintains ownership of returned pointer, so we should not manage its memory
        OGRGeometry *ptrOGeometry = ptrOFeature->GetGeometryRef();

        std::cout << std::setprecision(25);
        // Check whether the current geometry is a polygon
        if (ptrOGeometry != NULL && ptrOGeometry->getGeometryType() == OGRwkbGeometryType::wkbPolygon) {
            OGRPolygon *poly = (OGRPolygon *) ptrOGeometry;
            std::cout << "POLYGON: " << numPolygon << std::endl;

            std::cout << "FEATURE ID: " << ptrOFeature->GetFID() << std::endl;

            OGRLinearRing *pRing = poly->getExteriorRing();
            std::cout << "NUM_POINTS = " << pRing->getNumPoints() << std::endl << std::endl;
            for(int ptIdx=0; ptIdx<pRing->getNumPoints(); ptIdx++) {
                double lon = pRing->getX(ptIdx);
                double lat = pRing->getY(ptIdx);

                // Perform transform into WGS-84 lat/lon coordinate system
                int allTransformSuccess = invCoordTransform->TransformEx(1, &lon, &lat, nullptr, &transformSuccess);

                std::cout << "    POINT " << ptIdx << " : " << pRing->getX(ptIdx) << " " << pRing->getY(ptIdx) << " " << lon << " " << lat << std::endl;
            }

            double flag = true;
            if (ptrOGeometry->Contains(&rlanPoint)) {
                std::cout << "CONTAINS RLAN" << std::endl;
                flag = false;
            }
            if (ptrOGeometry->Contains(&fsPoint)) {
                std::cout << "CONTAINS FS" << std::endl;
                flag = false;
            }

            if (flag) {
                idList.push_back(ptrOFeature->GetFID());
            }

            std::cout << std::endl;
        } else {
            std::cout << "GdalDataModel::getBuildingsAtPoint(): Can't find polygon geometries in current feature";
        }

        numPolygon++;
    }

    std::cout << "NUM_POLYGON_IN_PATH = " << numPolygon << std::endl;

    std::cout << "NUM_POLYGON_NOT_CONTAIN_ENDPTS = " << idList.size() << std::endl;

}

