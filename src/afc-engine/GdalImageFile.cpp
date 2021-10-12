#include "GdalImageFile.h"
#include "ErrorTypes.h"
#include <QDebug>
#include <QRectF>
#include <QPointF>

GdalImageFile::GdalImageFile(const QString &filen) : _filename(filen){
    GDALDataset *set = (GDALDataset *)GDALOpen(filen.toStdString().c_str(), GA_ReadOnly);
    if(set == NULL){
        throw RuntimeError(QString("Unable to open tile data %1").arg(filen));
    }

    _dataset = set;
    const char *projRef = _dataset->GetProjectionRef();
    _spatialReferenceProjection = new OGRSpatialReference();
    char *projArr = (char *)calloc(strlen(projRef) + 1, sizeof(char));
    projArr = strcpy(projArr, projRef);
    
    OGRErr errCode = _spatialReferenceProjection->importFromWkt(&projArr);
    if(errCode != OGRERR_NONE){
        GDALClose(_dataset);
        throw RuntimeError(QString("FILE: %1 Failed to get spatial reference data %2").arg(_filename).arg(errCode));
    }

    _geographicReference = new OGRSpatialReference();
    errCode = _geographicReference->SetWellKnownGeogCS("WGS84");

    if(errCode != OGRERR_NONE){
        GDALClose(_dataset);
        throw RuntimeError(QString("FILE: %1 Failed to get geographic reference data %2").arg(_filename).arg(errCode));
    }

    double geoTransform[6] = {};
    CPLErr transformErr = _dataset->GetGeoTransform(geoTransform);

    if(transformErr != CPLErr::CE_None){
        GDALClose(_dataset);
        throw RuntimeError(QString("Failed to get geodetic transform"));
    }

    _rawTransform = new double[6];
    for(int i = 0; i < 6; ++i) _rawTransform[i] = geoTransform[i];
    
    _transform = OGRCreateCoordinateTransformation(_geographicReference, _spatialReferenceProjection);
    _invTransform = OGRCreateCoordinateTransformation(_spatialReferenceProjection, _geographicReference);
    GDALRasterBand *rb = _dataset->GetRasterBand(1);
    _rasterBand = rb;

    double xMax, yMax, xMin, yMin;
    xMax = _rawTransform[0] + _dataset->GetRasterXSize() * _rawTransform[1] + _dataset->GetRasterYSize() * _rawTransform[2];
    yMin = _rawTransform[3] + _dataset->GetRasterXSize() * _rawTransform[4] + _dataset->GetRasterYSize() * _rawTransform[5];
    xMin = _rawTransform[0];
    yMax = _rawTransform[3];

    _extentXMax = xMax;
    _extentYMax = yMax;
    _extentXMin = xMin;
    _extentYMin = yMin;
    _invTransform1 = 1.0 / _rawTransform[1];
    _invTransform5 = 1.0 / _rawTransform[5];

    {
        const char *pszProjection = NULL;
        double geoTrans[6];
        
        if(_dataset->GetGeoTransform(geoTrans) == CE_None){
            pszProjection = _dataset->GetProjectionRef();
        }

        OGRCoordinateTransformationH hTransform = NULL;
        if(pszProjection != NULL && strlen(pszProjection) > 0){
            OGRSpatialReferenceH hLatLong = NULL;
            OGRSpatialReferenceH hProj = OSRNewSpatialReference(pszProjection);
            hLatLong = OSRNewSpatialReference(NULL);
            OSRSetWellKnownGeogCS(hLatLong, "WGS84");
            hTransform = OCTNewCoordinateTransformation(hProj, hLatLong);
        }

        double minLat, maxLat, minLon, maxLon;
        minLat = maxLat = minLon = maxLon = 0.0;

        for(int c = 0; c < 4; ++c){
            double x, y;
            if((c / 2) > 0) x = _dataset->GetRasterXSize();
            else x = 0.0;
            if((c % 2) == 0) y = _dataset->GetRasterYSize();
            else y = 0.0;

            double geoX, geoY;
            geoX = _rawTransform[0] + _rawTransform[1] * x + _rawTransform[2] * y;
            geoY = _rawTransform[3] + _rawTransform[4] * x + _rawTransform[5] * y;
            qDebug() << x << y << "->" << geoX << geoY;
            double geoZ = 0.0;
            if(hTransform){
                OCTTransform(hTransform, 1, &geoX, &geoY, &geoZ);
                qDebug() << "    ->" << geoX << geoY;
            }

            if(c == 0 || geoY < minLat) minLat = geoY;
            if(c == 0 || geoY > maxLat) maxLat = geoY;
            if(c == 0 || geoX < minLon) minLon = geoX;
            if(c == 0 || geoX > maxLon) maxLon = geoX;
        }

        _topLeft = GeodeticCoord::fromLatLon(maxLat, minLon);
        _topRight = GeodeticCoord::fromLatLon(maxLat, maxLon);
        _bottomLeft = GeodeticCoord::fromLatLon(minLat, minLon);
        _bottomRight = GeodeticCoord::fromLatLon(minLat, maxLon);
    }

    _loadedData = false;
}

GdalImageFile::~GdalImageFile(){
    if(_loadedData){
        delete[] _rawData;
    }
    GDALClose(_dataset);
    delete[] _rawTransform;
}


void GdalImageFile::loadData(){
    _loadedData = true;
    _xsize = _dataset->GetRasterXSize();
    _ysize = _dataset->GetRasterYSize();
    _stride = _xsize;

    _rawData = new double[_xsize*_ysize];
    CPLErr readError = _rasterBand->RasterIO(GF_Read, 0, 0, _xsize, _ysize, _rawData, _xsize, _ysize, GDT_Float64, 0, 0);
	if (readError != CPLErr::CE_None) {
		throw std::runtime_error(QString("GdalDataDir::readFile(): Failed to read raster data from %1, throwing CPLErr = %2")
											.arg(_filename).arg(readError).toStdString());
	}
}

double GdalImageFile::getValue(const GeodeticCoord &pt) const {
    if(!_loadedData){
        throw RuntimeError("Requested Data on a non-loaded file.");
    }
    
    if(!containsPoint(pt)) return NO_DATA;

    double x = pt.longitudeDeg;
    double y = pt.latitudeDeg;

    if(_transform->Transform(1, &x, &y) != TRUE){
        throw RuntimeError(QString("Unable to transform coordinates %1, %2").arg(x).arg(y));
    }

    /// Now find the location inside the image.
    double srcX, srcY;

    srcX = (x - _extentXMin) * _invTransform1;
    srcY = -1 * (_extentYMax - y) * _invTransform5;
    double tf;
    int indx = floor(srcX) + floor(srcY) * _stride;
    tf = _rawData[indx];
    
    return tf;    
}

bool GdalImageFile::containsPoint(const GeodeticCoord &pt) const {
    double x = pt.longitudeDeg;
    double y = pt.latitudeDeg;

    if(_transform->Transform(1, &x, &y) != TRUE){
        throw RuntimeError(QString("Unable to transform coordinates %1, %2").arg(x).arg(y));
    }

    QRectF rect(QPointF(_extentXMax, _extentYMax), QPointF(_extentXMin, _extentYMin));
    QRectF rectN = rect.normalized();

    if(rectN.contains(x, y)) return true;
    else return false;
}

GeodeticCoord GdalImageFile::topRight() const {
    return _topRight;
}

GeodeticCoord GdalImageFile::bottomLeft() const {
    return _bottomLeft;
}

GeodeticCoord GdalImageFile::topLeft() const {
    return _topLeft;
}

GeodeticCoord GdalImageFile::bottomRight() const {
    return _bottomRight;
}
