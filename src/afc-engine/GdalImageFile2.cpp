#include "GdalImageFile2.h"
#include "ErrorTypes.h"
#include <QDebug>
#include <QRectF>
#include <QPointF>

GdalImageFile2::GdalImageFile2(const QString &filen, int tileSizeXVal, int tileSizeYVal) : _filename(filen), tileSizeX(tileSizeXVal), tileSizeY(tileSizeYVal)
{
    GDALDataset *set = (GDALDataset *)GDALOpen(filen.toStdString().c_str(), GA_ReadOnly);
    if(set == NULL){
        throw RuntimeError(QString("Unable to open tile data %1").arg(filen));
    }

    _dataset = set;
    const char *projRef = _dataset->GetProjectionRef();
    _spatialReferenceProjection = new OGRSpatialReference();
    char *projArr = (char *)calloc(strlen(projRef) + 1, sizeof(char));
    strcpy(projArr, projRef);
    
    OGRErr errCode = _spatialReferenceProjection->importFromWkt(&projArr);
    if(errCode != OGRERR_NONE){
        GDALClose(_dataset);
        throw RuntimeError(QString("FILE: %1 Failed to get spatial reference data %2").arg(_filename).arg(errCode));
    }

    _geographicReference = new OGRSpatialReference();
    // _geographicReference->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
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

    if (_rasterBand->GetRasterDataType() != GDT_Byte) {
        throw("ERROR: data type in GdalImageFile2 must be GDT_Byte");
    }

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
        OGRCoordinateTransformationH hInvTransform = NULL;
        if(pszProjection != NULL && strlen(pszProjection) > 0){
            OGRSpatialReferenceH hLatLong = NULL;
            OGRSpatialReferenceH hProj = OSRNewSpatialReference(pszProjection);
            hLatLong = OSRNewSpatialReference(NULL);
            OSRSetWellKnownGeogCS(hLatLong, "WGS84");
            hTransform = OCTNewCoordinateTransformation(hProj, hLatLong);
            hInvTransform = OCTNewCoordinateTransformation(hLatLong, hProj);
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
            double origX = geoX;
            double origY = geoY;
            if(hTransform){
                OCTTransform(hTransform, 1, &geoX, &geoY, &geoZ);
                qDebug() << "    ->" << geoX << geoY;
            }
            double lonDeg = geoX;
            double latDeg = geoY;

#if 0
// For debug only
            if(hInvTransform){
                OCTTransform(hInvTransform, 1, &geoX, &geoY, &geoZ);
                qDebug() << "    ->" << geoX << geoY;
            }
            double transformBackX = geoX;
            double transformBackY = geoY;
#endif

            if(c == 0 || latDeg < minLat) minLat = latDeg;
            if(c == 0 || latDeg > maxLat) maxLat = latDeg;
            if(c == 0 || lonDeg < minLon) minLon = lonDeg;
            if(c == 0 || lonDeg > maxLon) maxLon = lonDeg;
        }

        _topLeft = GeodeticCoord::fromLatLon(maxLat, minLon);
        _topRight = GeodeticCoord::fromLatLon(maxLat, maxLon);
        _bottomLeft = GeodeticCoord::fromLatLon(minLat, minLon);
        _bottomRight = GeodeticCoord::fromLatLon(minLat, maxLon);
    }

    _xsize = _dataset->GetRasterXSize();
    _ysize = _dataset->GetRasterYSize();

    numTileX = (_xsize + tileSizeX - 1) / tileSizeX;
    numTileY = (_ysize + tileSizeY - 1) / tileSizeY;
    tileXIdx = -1;
    tileYIdx = -1;

    _loadedData = false;
}

GdalImageFile2::~GdalImageFile2()
{
    if(_loadedData){
        free(_rawData);
    }
    GDALClose(_dataset);
    delete[] _rawTransform;
}

int GdalImageFile2::getNumTileX() const { return(numTileX); }
int GdalImageFile2::getNumTileY() const { return(numTileY); }
int GdalImageFile2::getSizeX() const { return(_xsize); }
int GdalImageFile2::getSizeY() const { return(_ysize); }

void GdalImageFile2::xyIdxtoLonLat(const int& xIdx, const int& yIdx, GeodeticCoord &pt)
{
    double xVal =  (xIdx+0.5)*_rawTransform[1] + _extentXMin;
    double yVal =  (yIdx+0.5)*_rawTransform[5] + _extentYMax;

    if(_invTransform->Transform(1, &xVal, &yVal) != TRUE){
        throw RuntimeError(QString("Unable to transform coordinates %1, %2").arg(xVal).arg(yVal));
    }

    pt.longitudeDeg = xVal;
    pt.latitudeDeg = yVal;

    return;
}

void GdalImageFile2::lonlatToXY(const GeodeticCoord &pt, int& xIdx, int& yIdx)
{
    double x = pt.longitudeDeg;
    double y = pt.latitudeDeg;

    if(_transform->Transform(1, &x, &y) != TRUE){
        throw RuntimeError(QString("Unable to transform coordinates %1, %2").arg(x).arg(y));
    }

    /// Now find the location inside the image.
    xIdx = (int) floor( (x - _extentXMin) * _invTransform1 );
    yIdx = (int) floor( -1 * (_extentYMax - y) * _invTransform5 );

    return;
}

void GdalImageFile2::loadTile(int tileXIdxVal, int tileYIdxVal) {
    if ((tileXIdx != tileXIdxVal) || (tileYIdx != tileYIdxVal)) {
        tileXIdx = tileXIdxVal;
        tileYIdx = tileYIdxVal;

        if(!_loadedData){
            _rawData = (unsigned char *) malloc(tileSizeX*tileSizeY);
            _loadedData = true;
        }

        int tsx, tsy;
        if (tileXIdx == numTileX-1) {
           tsx = ((_xsize-1) % tileSizeX) + 1;
        } else {
           tsx = tileSizeX;
        }

        if (tileYIdx == numTileY-1) {
           tsy = ((_ysize-1) % tileSizeY) + 1;
        } else {
           tsy = tileSizeY;
        }

        printf("Reading tile: [%d, %d]\n", tileXIdx, tileYIdx);
        fflush(stdout);
        CPLErr readError = _rasterBand->RasterIO(GF_Read, tileXIdx*tileSizeX, tileYIdx*tileSizeY, tsx, tsy, _rawData, tsx, tsy, GDT_Byte, 0, 0);
        if (readError != CPLErr::CE_None) {
            throw std::runtime_error(QString("GdalDataDir::loadFile(): Failed to read raster data from %1, throwing CPLErr = %2")
                                                .arg(_filename).arg(readError).toStdString());
        }
        _stride = tsx;
    }
}

unsigned char GdalImageFile2::getValue(const GeodeticCoord &pt)
{
    // if(!containsPoint(pt)) return NO_DATA;

    int srcX, srcY;
    lonlatToXY(pt, srcX, srcY);

    if (    (srcX < 0) || (srcX > _xsize-1)
         || (srcY < 0) || (srcY > _ysize-1) ) {
        return NO_DATA;
    }

    unsigned char tf = getValue(srcX, srcY);

    return tf;
}

unsigned char GdalImageFile2::getValue(int xIdx, int yIdx)
{
    int ptTileIdxX = xIdx / tileSizeX;
    int ptTileIdxY = yIdx / tileSizeY;

    loadTile(ptTileIdxX, ptTileIdxY);

    unsigned char tf;
    int indx = (xIdx - tileXIdx*tileSizeX) + (yIdx - tileYIdx*tileSizeY) * _stride;
    tf = _rawData[indx];
    
    return tf;
}

bool GdalImageFile2::containsPoint(const GeodeticCoord &pt) const {
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

GeodeticCoord GdalImageFile2::topRight() const {
    return _topRight;
}

GeodeticCoord GdalImageFile2::bottomLeft() const {
    return _bottomLeft;
}

GeodeticCoord GdalImageFile2::topLeft() const {
    return _topLeft;
}

GeodeticCoord GdalImageFile2::bottomRight() const {
    return _bottomRight;
}
