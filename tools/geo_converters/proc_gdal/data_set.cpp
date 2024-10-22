/******************************************************************************************/
/**** FILE : data_set.cpp                                                              ****/
/******************************************************************************************/

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <iomanip>
#include <cstring>
#include <string>
#include <math.h>
#include <stdexcept>
#include <ogr_api.h>
#include <ogr_spatialref.h>
#include <dirent.h>
#include <limits>

#include "global_defines.h"
#include "GdalDataModel.h"
#include "image.h"
#include "data_set.h"
#include "polygon.h"

/******************************************************************************************/
/**** FUNCTION: DataSetClass::DataSetClass()                                           ****/
/******************************************************************************************/
DataSetClass::DataSetClass()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: DataSetClass::~DataSetClass()                                          ****/
/******************************************************************************************/
DataSetClass::~DataSetClass()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: DataSetClass::run                                                      ****/
/******************************************************************************************/
void DataSetClass::run()
{
    if (parameterTemplate.function == "combine3D2D") {
        combine3D2D();
    } else if (parameterTemplate.function == "fixRaster") {
        fixRaster();
    } else if (parameterTemplate.function == "fixVector") {
        fixVector();
    } else if (parameterTemplate.function == "vectorCvg") {
        vectorCvg();
    } else if (parameterTemplate.function == "mbRasterCvg") {
        mbRasterCvg();
    } else if (parameterTemplate.function == "procBoundary") {
        procBoundary();
    } else {
        std::ostringstream errStr;
        errStr << "ERROR: function set to unrecognized value: " << parameterTemplate.function;
        throw std::runtime_error(errStr.str());
    }
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: DataSetClass::combine3D2D                                              ****/
/******************************************************************************************/
void DataSetClass::combine3D2D()
{
    GDALAllRegister();

    GdalDataModel *gdalDataModel3D = new GdalDataModel(parameterTemplate.srcFile3D, parameterTemplate.heightFieldName3D);
    // gdalDataModel3D->printDebugInfo();

    GdalDataModel *gdalDataModel2D = new GdalDataModel(parameterTemplate.srcFile2D, parameterTemplate.heightFieldName2D);
    // gdalDataModel2D->printDebugInfo();


    /**************************************************************************************/
    /**** Create Driver                                                                ****/
    /**************************************************************************************/
    const char *pszDriverName = "ESRI Shapefile";
    GDALDriver *poDriver;

    poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName );
    if( poDriver == NULL )
    {
        printf( "%s driver not available.\n", pszDriverName );
        exit( 1 );
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Create Output Dataset (specify output file)                                  ****/
    /**************************************************************************************/
    GDALDataset *outputDS;

    outputDS = poDriver->Create( parameterTemplate.outputFile.c_str(), 0, 0, 0, GDT_Unknown, NULL );
    if( outputDS == NULL )
    {
        printf( "Creation of output file failed.\n" );
        exit( 1 );
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Create Layer                                                                 ****/
    /**************************************************************************************/
    OGRLayer *poLayer;

    poLayer = outputDS->CreateLayer( parameterTemplate.outputLayer.c_str(), NULL, wkbPolygon, NULL );
    if( poLayer == NULL )
    {
        printf( "Layer creation failed.\n" );
        exit( 1 );
    }
    /**************************************************************************************/

#if 0
    /**************************************************************************************/
    /**** Create FIELD: OGR_FID                                                        ****/
    /**************************************************************************************/
    OGRFieldDefn oField1( "OGR_FID", OFTInteger );

    oField1.SetWidth(9);

    if( poLayer->CreateField( &oField1 ) != OGRERR_NONE )
    {
        printf( "Creating OGR_FID field failed.\n" );
        exit( 1 );
    }
    /**************************************************************************************/
#endif

    /**************************************************************************************/
    /**** Create FIELD: Height                                                         ****/
    /**************************************************************************************/
    OGRFieldDefn heightField( parameterTemplate.outputHeightFieldName.c_str(), OFTReal );

    // 24.15 is ridiculous, change to 12.2
    heightField.SetWidth(12);
    heightField.SetPrecision(2);

    if( poLayer->CreateField( &heightField ) != OGRERR_NONE )
    {
        std::cout << "Creating " << parameterTemplate.outputHeightFieldName << " field failed.\n";
        exit( 1 );
    }
    int heightFIeldIdx = poLayer->FindFieldIndex(parameterTemplate.outputHeightFieldName.c_str(), TRUE);
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Add all polygons from 3D layer                                               ****/
    /**************************************************************************************/
    std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> ptrOFeature;
    OGRLayer *layer3D = gdalDataModel3D->getLayer();
    layer3D->ResetReading();
    while ((ptrOFeature = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(layer3D->GetNextFeature())) != NULL) {
        // GDAL maintains ownership of returned pointer, so we should not manage its memory
        OGRGeometry *ptrOGeometry = ptrOFeature->GetGeometryRef();

        if (ptrOGeometry != NULL && ptrOGeometry->getGeometryType() == OGRwkbGeometryType::wkbPolygon) {
            double height = ptrOFeature->GetFieldAsDouble(gdalDataModel3D->heightFieldIdx);

            OGRFeature *poFeature;

            poFeature = OGRFeature::CreateFeature( poLayer->GetLayerDefn() );
            poFeature->SetField(heightFIeldIdx, height);

            OGRPolygon *poly = (OGRPolygon *) ptrOGeometry;

            poFeature->SetGeometry( poly );

            if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE )
            {
                printf( "Failed to create feature in shapefile.\n" );
                exit( 1 );
            }

            OGRFeature::DestroyFeature( poFeature );
        }
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Add all polygons from 2D layer that dont overlay anything in 3D layer        ****/
    /**************************************************************************************/
    int num2DPolygonUsed = 0;
    int num2DPolygonDiscarded = 0;

    std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> ptrOFeature2D;
    OGRLayer *layer2D = gdalDataModel2D->getLayer();
    layer2D->ResetReading();
    while ((ptrOFeature2D = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(layer2D->GetNextFeature())) != NULL) {
        // GDAL maintains ownership of returned pointer, so we should not manage its memory
        OGRGeometry *ptrOGeometry = ptrOFeature2D->GetGeometryRef();

        if (ptrOGeometry != NULL && ptrOGeometry->getGeometryType() == OGRwkbGeometryType::wkbPolygon) {

            OGRPolygon *poly = (OGRPolygon *) ptrOGeometry;

            layer3D->SetSpatialFilter(poly);

            layer3D->ResetReading();

            if ((ptrOFeature = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(layer3D->GetNextFeature())) == NULL) {

                double height = ptrOFeature2D->GetFieldAsDouble(gdalDataModel2D->heightFieldIdx);

                OGRFeature *poFeature;

                poFeature = OGRFeature::CreateFeature( poLayer->GetLayerDefn() );
                poFeature->SetField(heightFIeldIdx, height);

                poFeature->SetGeometry( poly );

                if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE )
                {
                    printf( "Failed to create feature in shapefile.\n" );
                    exit( 1 );
                }

                OGRFeature::DestroyFeature( poFeature );

                num2DPolygonUsed++;
            } else {
                num2DPolygonDiscarded++;
            }
        }
    }
    /**************************************************************************************/

    GDALClose( outputDS );

    std::cout << "NUM 2D POLYGONS USED: " << num2DPolygonUsed << std::endl;
    std::cout << "NUM 2D POLYGONS DISCARDED: " << num2DPolygonDiscarded << std::endl;

    return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: DataSetClass::fixRaster                                                ****/
/******************************************************************************************/
void DataSetClass::fixRaster()
{
    std::ostringstream errStr;
    GDALAllRegister();

    const char *pszFormat = "GTiff";
    GDALDriver *poDriver;
    char **papszMetadata;
    poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
    if( !poDriver ) {
        exit( 1 );
    }

    bool genImageFlag = (!parameterTemplate.imageFile.empty());
    std::string tmpImageFile = (parameterTemplate.tmpImageFile.empty() ? "/tmp/image.ppm" : parameterTemplate.tmpImageFile);

    papszMetadata = poDriver->GetMetadata();
    if( CSLFetchBoolean( papszMetadata, GDAL_DCAP_CREATE, FALSE ) )
        printf( "Driver %s supports Create() method.\n", pszFormat );
    if( CSLFetchBoolean( papszMetadata, GDAL_DCAP_CREATECOPY, FALSE ) )
        printf( "Driver %s supports CreateCopy() method.\n", pszFormat );

    GDALDataset *srcDS = (GDALDataset *) GDALOpen( parameterTemplate.srcFileRaster.c_str(), GA_ReadOnly );

    char **options = (char **) NULL;

    options = CSLSetNameValue(options, "TILED", "YES");
    options = CSLSetNameValue(options, "BLOCKXSIZE", "256");

    GDALDataset *dstDS;
    dstDS = poDriver->CreateCopy( parameterTemplate.outputFile.c_str(), srcDS, FALSE, options, GDALTermProgress, NULL );
    GDALClose( (GDALDatasetH) srcDS );

    CSLDestroy(options);

    int nXSize = srcDS->GetRasterXSize();
    int nYSize = srcDS->GetRasterYSize();
    int numRasterBand = dstDS->GetRasterCount();

    double adfGeoTransform[6];
    printf( "Driver: %s/%s\n",
        dstDS->GetDriver()->GetDescription(),
        dstDS->GetDriver()->GetMetadataItem( GDAL_DMD_LONGNAME ) );
    printf( "Size is %dx%dx%d\n", nXSize, nYSize, numRasterBand );
    if( dstDS->GetProjectionRef()  != NULL ) {
        printf( "Projection is `%s'\n", dstDS->GetProjectionRef() );
    }
    const char  *pszProjection = nullptr;
    if( dstDS->GetGeoTransform( adfGeoTransform ) == CE_None ) {
        printf( "Origin = (%.6f,%.6f)\n", adfGeoTransform[0], adfGeoTransform[3] );
        printf( "Pixel Size = (%.6f,%.6f)\n", adfGeoTransform[1], adfGeoTransform[5] );
        pszProjection = GDALGetProjectionRef(dstDS);
    } else {
        throw std::runtime_error("ERROR: getting GEO Transform");
    }

    double pixelSize = adfGeoTransform[1];
    if ( fabs(pixelSize + adfGeoTransform[5]) > 1.0e-8 ) {
        throw std::runtime_error("ERROR: X / Y pixel sizes not properly set");
    }

    OGRCoordinateTransformationH hTransform = nullptr;

    if( pszProjection != nullptr && strlen(pszProjection) > 0 )
    {
        OGRSpatialReferenceH hLatLong = nullptr;

        OGRSpatialReferenceH hProj = OSRNewSpatialReference( pszProjection );

        if (hProj != nullptr) {
            OGRErr eErr = OGRERR_NONE;
            // Check that it looks like Earth before trying to reproject to wgs84...
            hLatLong = OSRCloneGeogCS( hProj );
            if( hLatLong ) {
                // Drop GEOGCS|UNIT child to be sure to output as degrees
                OGRSpatialReference* poLatLong = reinterpret_cast<
                    OGRSpatialReference*>(hLatLong);
                OGR_SRSNode *poGEOGCS = poLatLong->GetRoot();
                if( poGEOGCS )
                {
                    const int iUnitChild =
                        poGEOGCS->FindChild("UNIT");
                    if( iUnitChild != -1 )
                        poGEOGCS->DestroyChild(iUnitChild);
                }
            }
        }

        if( hLatLong != nullptr )
        {
            CPLPushErrorHandler( CPLQuietErrorHandler );
            hTransform = OCTNewCoordinateTransformation( hProj, hLatLong );
            CPLPopErrorHandler(); 

            OSRDestroySpatialReference( hLatLong );
        }

        if( hProj != nullptr ) {
            OSRDestroySpatialReference( hProj );
        }
    }

    if (!hTransform) {
        throw std::runtime_error("ERROR: unable to create coordinate transform");
    }

    double ULX = adfGeoTransform[0] + adfGeoTransform[1] * 0      + adfGeoTransform[2] * 0;
    double ULY = adfGeoTransform[3] + adfGeoTransform[4] * 0      + adfGeoTransform[5] * 0;
    double ULZ = 0.0;

    double LLX = adfGeoTransform[0] + adfGeoTransform[1] * 0      + adfGeoTransform[2] * nYSize;
    double LLY = adfGeoTransform[3] + adfGeoTransform[4] * 0      + adfGeoTransform[5] * nYSize;
    double LLZ = 0.0;

    double URX = adfGeoTransform[0] + adfGeoTransform[1] * nXSize + adfGeoTransform[2] * 0;
    double URY = adfGeoTransform[3] + adfGeoTransform[4] * nXSize + adfGeoTransform[5] * 0;
    double URZ = 0.0;

    double LRX = adfGeoTransform[0] + adfGeoTransform[1] * nXSize + adfGeoTransform[2] * nYSize;
    double LRY = adfGeoTransform[3] + adfGeoTransform[4] * nXSize + adfGeoTransform[5] * nYSize;
    double LRZ = 0.0;

    OCTTransform(hTransform,1,&ULX,&ULY,&ULZ);
    OCTTransform(hTransform,1,&LLX,&LLY,&LLZ);
    OCTTransform(hTransform,1,&URX,&URY,&URZ);
    OCTTransform(hTransform,1,&LRX,&LRY,&LRZ);

    double resLon = (std::min(URX, LRX) - std::max(ULX, LLX))/nXSize;
    double resLat = (std::min(ULY, URY) - std::max(LLY, LRY))/nYSize;
    double resLonLat = std::min(resLon, resLat);

    if (parameterTemplate.verbose) {
        std::cout << "UL LONLAT: " << ULX << " " << ULY << std::endl;
        std::cout << "LL LONLAT: " << LLX << " " << LLY << std::endl;
        std::cout << "UR LONLAT: " << URX << " " << URY << std::endl;
        std::cout << "LR LONLAT: " << LRX << " " << LRY << std::endl;

        std::cout << "RES_LON = " << resLon << std::endl;
        std::cout << "RES_LAT = " << resLat << std::endl;
        std::cout << "RES_LONLAT = " << resLonLat << std::endl;
    }

    std::cout << "NUMBER RASTER BANDS: " << numRasterBand << std::endl;

    if (numRasterBand != 1) {
        throw std::runtime_error("ERROR numRasterBand must be 1");
    }

    int nBlockXSize, nBlockYSize;
    GDALRasterBand *rasterBand = dstDS->GetRasterBand(1);
    char **rasterMetadata = rasterBand->GetMetadata();

    if (rasterMetadata) {
        std::cout << "RASTER METADATA: " << std::endl;
        char **chptr = rasterMetadata;
        while (*chptr) {
            std::cout << "    " << *chptr << std::endl;
            chptr++;
        }
    } else {
        std::cout << "NO RASTER METADATA: " << std::endl;
    }

    rasterBand->GetBlockSize( &nBlockXSize, &nBlockYSize );
    printf( "Block=%dx%d Type=%s, ColorInterp=%s\n",
        nBlockXSize, nBlockYSize,
        GDALGetDataTypeName(rasterBand->GetRasterDataType()),
        GDALGetColorInterpretationName(
            rasterBand->GetColorInterpretation()) );

    int bGotMin, bGotMax;
    double adfMinMax[2];
    adfMinMax[0] = rasterBand->GetMinimum( &bGotMin );
    adfMinMax[1] = rasterBand->GetMaximum( &bGotMax );
    if( ! (bGotMin && bGotMax) ) {
        std::cout << "calling GDALComputeRasterMinMax()" << std::endl;
        GDALComputeRasterMinMax((GDALRasterBandH)rasterBand, TRUE, adfMinMax);
    }

    printf( "Min=%.3f\nMax=%.3f\n", adfMinMax[0], adfMinMax[1] );
    if( rasterBand->GetOverviewCount() > 0 ) {
        printf( "Band has %d overviews.\n", rasterBand->GetOverviewCount() );
    }
    if( rasterBand->GetColorTable() != NULL ) {
        printf( "Band has a color table with %d entries.\n",
                rasterBand->GetColorTable()->GetColorEntryCount() );
    }

    int hasNoData;
    double origNodataValue = rasterBand->GetNoDataValue(&hasNoData);
    float origNodataValueFloat = (float) origNodataValue;
    if (hasNoData) {
        std::cout << "ORIG NODATA: " << origNodataValue << std::endl;
        std::cout << "ORIG NODATA (float): " << origNodataValueFloat << std::endl;
    } else {
        std::cout << "ORIG NODATA undefined" << std::endl;
    }

    rasterBand->SetNoDataValue(parameterTemplate.nodataVal);

    int xIdx, yIdx;
    if (parameterTemplate.verbose) {
        std::cout << "nXSize: " << nXSize << std::endl;
        std::cout << "nYSize: " << nYSize << std::endl;
        std::cout << "GDALGetDataTypeSizeBytes(GDT_Float32) = " << GDALGetDataTypeSizeBytes(GDT_Float32) << std::endl;
        std::cout << "sizeof(GDT_Float32) = " << sizeof(GDT_Float32) << std::endl;
        std::cout << "sizeof(GDT_Float64) = " << sizeof(GDT_Float64) << std::endl;
        std::cout << "sizeof(float) = " << sizeof(float) << std::endl;
    }
    float *pafScanline = (float *) CPLMalloc(nXSize*GDALGetDataTypeSizeBytes(GDT_Float32));

    if (fabs(parameterTemplate.nodataVal) > std::numeric_limits<float>::max()) {
        errStr << "ERROR: nodataVal set to illegal value: " << parameterTemplate.nodataVal << ", max value for float is " << std::numeric_limits<float>::max();
        throw std::runtime_error(errStr.str());
    }

    /**************************************************************************************/
    /* Define color scheme                                                                */
    /**************************************************************************************/
    std::vector<std::string> colorList;
    colorList.push_back("  0   0   0"); // 0: NO DATA
    colorList.push_back("  0 255   0"); // 1: VALID DATA
    colorList.push_back("  0 255 255"); // 2: Mix of data/nodata samples
    /**************************************************************************************/

    /**************************************************************************************/
    /* Create PPM File                                                                    */
    /**************************************************************************************/
    FILE *fppm = (FILE *) NULL;
    int N = (int) ceil( (parameterTemplate.imageLonLatRes/resLonLat) - 1.0e-8 );
    if (N < 1) { N = 1; }
    int imageXSize = (nXSize-1)/N + 1;
    int imageYSize = (nYSize-1)/N + 1;
    int *imageScanline = (int *) NULL;
    int imgXIdx, imgYIdx;
    if (genImageFlag) {
        if ( !(fppm = fopen(tmpImageFile.c_str(), "wb")) ) {
            throw std::runtime_error("ERROR");
        }
        fprintf(fppm, "P3\n");
        fprintf(fppm, "%d %d %d\n", imageXSize, imageYSize, 255);
        imageScanline = (int *) malloc(imageXSize*sizeof(int));
        for(imgXIdx=0; imgXIdx<imageXSize; ++imgXIdx) {
            imageScanline[imgXIdx] = -1;
        }
    }
    /**************************************************************************************/

    int numNoData = 0;
    int numClampMin = 0;
    int numClampMax = 0;
    int numMinMag   = 0;
    int numValid    = 0;
    for(yIdx=0; yIdx<nYSize; yIdx++) {
        rasterBand->RasterIO( GF_Read, 0, yIdx, nXSize, 1, pafScanline, nXSize, 1, GDT_Float32, 0, 0 );
        int colorIdx;
        for(xIdx=0; xIdx<nXSize; xIdx++) {
            if (hasNoData && (pafScanline[xIdx] == origNodataValueFloat)) {
                numNoData++;
                pafScanline[xIdx] = (float) parameterTemplate.nodataVal;
                colorIdx = 0;
            } else if (pafScanline[xIdx] < parameterTemplate.clampMin) {
                numClampMin++;
                pafScanline[xIdx] = (float) parameterTemplate.nodataVal;
                colorIdx = 0;
            } else if (pafScanline[xIdx] > parameterTemplate.clampMax) {
                numClampMax++;
                pafScanline[xIdx] = (float) parameterTemplate.nodataVal;
                colorIdx = 0;
            } else if (fabs(pafScanline[xIdx]) < parameterTemplate.minMag) {
                numMinMag++;
                pafScanline[xIdx] = (float) parameterTemplate.nodataVal;
                colorIdx = 0;
            } else {
                numValid++;
                colorIdx = 1;
            }
            if (genImageFlag) {
                imgXIdx = xIdx / N;
                if (imageScanline[imgXIdx] == -1) {
                    imageScanline[imgXIdx] = colorIdx;
                } else if (colorIdx != imageScanline[imgXIdx]) {
                    imageScanline[imgXIdx] = 2;
                }
            }
        }
        if (genImageFlag) {
            if ((yIdx % N == N-1) || (yIdx == nYSize-1)) {
                for(imgXIdx=0; imgXIdx<imageXSize; ++imgXIdx) {
                    colorIdx = imageScanline[imgXIdx];

                    if (imgXIdx) { fprintf(fppm, " "); }
                    fprintf(fppm, "%s", colorList[colorIdx].c_str());

                    imageScanline[imgXIdx] = -1;
                }
                fprintf(fppm, "\n");
            }
        }
        rasterBand->RasterIO( GF_Write, 0, yIdx, nXSize, 1, pafScanline, nXSize, 1, GDT_Float32, 0, 0 );
    }

    if (genImageFlag) {
        fclose(fppm);
    }

    std::cout << "Num NODATA values " << numNoData << " (" << 100.0*numNoData / (1.0*nXSize*nYSize) << "%)" << std::endl;
    std::cout << "Num values below min clamp " << parameterTemplate.clampMin << ": " << numClampMin << " (" << 100.0*numClampMin / (1.0*nXSize*nYSize) << "%)" << std::endl;
    std::cout << "Num values above max clamp " << parameterTemplate.clampMax << ": " << numClampMax << " (" << 100.0*numClampMax / (1.0*nXSize*nYSize) << "%)" << std::endl;
    std::cout << "Num values with fabs() below minMag " << parameterTemplate.minMag << ": " << numMinMag << " (" << 100.0*numMinMag / (1.0*nXSize*nYSize) << "%)" << std::endl;
    std::cout << "Num VALID  values " << numValid << " (" << 100.0*numValid / (1.0*nXSize*nYSize) << "%)" << std::endl;

    int numModified = numClampMin+numClampMax+numMinMag;
    std::cout << "Num values modified: " << numModified << " (" << 100.0*numModified / (1.0*nXSize*nYSize) << "%)" << std::endl;

    int numData = numValid;
    std::cout << "Num DATA values: " << numData << " (" << 100.0*numData / (1.0*nXSize*nYSize) << "%)" << std::endl;

    CPLFree(pafScanline);

    if (numValid) {
        double pdfMin, pdfMax, pdfMean, pdfStdDev;
        rasterBand->ComputeStatistics  (0, &pdfMin, &pdfMax, &pdfMean, &pdfStdDev, NULL, (void *)   NULL);
        rasterBand->SetStatistics  (pdfMin, pdfMax, pdfMean, pdfStdDev);
    } else {
        rasterBand->SetStatistics  (0.0, 0.0, 0.0, 0.0);
    }

    /* Once we're done, close properly the dataset */
    if( dstDS != NULL ) {
        GDALClose( (GDALDatasetH) dstDS );
    }

    if (genImageFlag) {
        std::string command = "convert " + tmpImageFile + " " + parameterTemplate.imageFile;
        std::cout << "COMMAND: " << command << std::endl;
        system(command.c_str());
    }
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: DataSetClass::fixVector                                                ****/
/******************************************************************************************/
void DataSetClass::fixVector()
{
    GDALAllRegister();

    bool genImageFlag = (!parameterTemplate.imageFile.empty());
    std::string tmpImageFile = (parameterTemplate.tmpImageFile.empty() ? "/tmp/image.ppm" : parameterTemplate.tmpImageFile);

    GdalDataModel *gdalDataModel = new GdalDataModel(parameterTemplate.srcFileVector, parameterTemplate.srcHeightFieldName);

    OGRLayer *layer = gdalDataModel->getLayer();

    double minLon, maxLon;
    double minLat, maxLat;

    OGREnvelope oExt;
    if (layer->GetExtent(&oExt, TRUE) == OGRERR_NONE) {
        minLon = oExt.MinX;
        maxLon = oExt.MaxX;
        minLat = oExt.MinY;
        maxLat = oExt.MaxY;
    }

    if (parameterTemplate.verbose) {
        std::cout << "MIN_LON = " << minLon << std::endl;
        std::cout << "MAX_LON = " << maxLon << std::endl;
        std::cout << "MIN_LAT = " << minLat << std::endl;
        std::cout << "MAX_LAT = " << maxLat << std::endl;
    }

    double res = parameterTemplate.imageLonLatRes;

    int lonN0 = (int) floor(minLon / res);
    int lonN1 = (int) ceil(maxLon / res);
    int latN0 = (int) floor(minLat / res);
    int latN1 = (int) ceil(maxLat / res);

    /**************************************************************************************/
    /* Create PPM File                                                                    */
    /**************************************************************************************/
    FILE *fppm = (FILE *) NULL;
    int imageXSize = lonN1 - lonN0;
    int imageYSize = latN1 - latN0;
    int imgXIdx, imgYIdx;

    if (genImageFlag) {
        if ( !(fppm = fopen(tmpImageFile.c_str(), "wb")) ) {
            throw std::runtime_error("ERROR");
        }
        fprintf(fppm, "P3\n");
        fprintf(fppm, "%d %d %d\n", imageXSize, imageYSize, 255);
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /* Define color scheme                                                                */
    /**************************************************************************************/
    std::vector<std::string> colorList;
    colorList.push_back("255 255 255"); // 0: NO BLDG
    colorList.push_back("255   0   0"); // 1: BLDG
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Allocate and initialize image array                                          ****/
    /**************************************************************************************/
    int **image = (int **) malloc(imageXSize*sizeof(int *));
    for(imgXIdx=0; imgXIdx<imageXSize; ++imgXIdx) {
        image[imgXIdx] = (int *) malloc(imageYSize*sizeof(int));
        for(imgYIdx=0; imgYIdx<imageYSize; ++imgYIdx) {
            image[imgXIdx][imgYIdx] = 0;
        }
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Create Driver                                                                ****/
    /**************************************************************************************/
    const char *pszDriverName = "ESRI Shapefile";
    GDALDriver *poDriver;

    poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName );
    if( poDriver == NULL )
    {
        printf( "%s driver not available.\n", pszDriverName );
        exit( 1 );
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Create Output Dataset (specify output file)                                  ****/
    /**************************************************************************************/
    GDALDataset *outputDS;

    outputDS = poDriver->Create( parameterTemplate.outputFile.c_str(), 0, 0, 0, GDT_Unknown, NULL );
    if( outputDS == NULL )
    {
        printf( "Creation of output file failed.\n" );
        exit( 1 );
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Create Layer                                                                 ****/
    /**************************************************************************************/
    OGRLayer *poLayer;

    poLayer = outputDS->CreateLayer( parameterTemplate.outputLayer.c_str(), layer->GetSpatialRef(), wkbPolygon, NULL );
    if( poLayer == NULL )
    {
        printf( "Layer creation failed.\n" );
        exit( 1 );
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Create FIELD: Height                                                         ****/
    /**************************************************************************************/
    OGRFieldDefn heightField( parameterTemplate.outputHeightFieldName.c_str(), OFTReal );

    // 24.15 is ridiculous, change to 12.2
    heightField.SetWidth(12);
    heightField.SetPrecision(2);

    if( poLayer->CreateField( &heightField ) != OGRERR_NONE )
    {
        std::cout << "Creating " << parameterTemplate.outputHeightFieldName << " field failed.\n";
        exit( 1 );
    }
    int heightFIeldIdx = poLayer->FindFieldIndex(parameterTemplate.outputHeightFieldName.c_str(), TRUE);
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Iterate through all features in layer, replace MultiPolygon with polygons    ****/
    /**************************************************************************************/
    int numPolygon = 0;
    int numNullGeometry = 0;
    int numUnrecognizedGeometry = 0;
    OGREnvelope env;
    std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> ptrOFeature;
    layer->ResetReading();
    while ((ptrOFeature = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(layer->GetNextFeature())) != NULL) {
        // GDAL maintains ownership of returned pointer, so we should not manage its memory
        OGRGeometry *ptrOGeometry = ptrOFeature->GetGeometryRef();
        bool useGeometry = false;

        if (ptrOGeometry != NULL) {
            OGRwkbGeometryType geometryType = ptrOGeometry->getGeometryType();
            if (geometryType == OGRwkbGeometryType::wkbPolygon) {
                double height = ptrOFeature->GetFieldAsDouble(gdalDataModel->heightFieldIdx);

                OGRFeature *poFeature = OGRFeature::CreateFeature( poLayer->GetLayerDefn() );
                poFeature->SetField(heightFIeldIdx, height);

                OGRPolygon *poly = (OGRPolygon *) ptrOGeometry;

                poFeature->SetGeometry( poly );

                if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE ) {
                    printf( "Failed to create feature in shapefile.\n" );
                    exit( 1 );
                }

                OGRFeature::DestroyFeature( poFeature );
                useGeometry = true;
                numPolygon++;
            } else if (geometryType == OGRwkbGeometryType::wkbMultiPolygon ) {
                double height = ptrOFeature->GetFieldAsDouble(gdalDataModel->heightFieldIdx);
                OGRMultiPolygon *multipoly = (OGRMultiPolygon *) ptrOGeometry;
                OGRPolygon *poly;
                OGRPolygon **iter;
                for (iter = multipoly->begin(); iter != multipoly->end(); ++iter) { 
                    poly = *iter;
                    OGRFeature *poFeature = OGRFeature::CreateFeature( poLayer->GetLayerDefn() );
                    poFeature->SetField(heightFIeldIdx, height);
                    poFeature->SetGeometry( poly );
                    if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE ) {
                        printf( "Failed to create feature in shapefile.\n" );
                        exit( 1 );
                    }
                    numPolygon++;
                    OGRFeature::DestroyFeature( poFeature );
                }
                useGeometry = true;
            } else {
                std::cout << "WARNING: Unrecognized Geometry Type: " << OGRGeometryTypeToName(geometryType) << std::endl;
                numUnrecognizedGeometry++;
                useGeometry = false;
            }

            if ( (genImageFlag) && (useGeometry) ) {
                ptrOGeometry->getEnvelope(&env);
                int x0 = ((int) floor(env.MinX / res)) - lonN0;
                int x1 = ((int) floor(env.MaxX / res)) - lonN0;
                int y0 = ((int) floor(env.MinY / res)) - latN0;
                int y1 = ((int) floor(env.MaxY / res)) - latN0;

                for(imgXIdx=x0; imgXIdx<=x1; imgXIdx++) {
                    for(imgYIdx=y0; imgYIdx<=y1; imgYIdx++) {
                        image[imgXIdx][imgYIdx] = 1;
                    }
                }
            }
        } else {
            numNullGeometry++;
        }
    }
    /**************************************************************************************/

    std::cout << "NUM_UNRECOGNIZED_GEOMETRY: " << numUnrecognizedGeometry << std::endl;
    std::cout << "NUM_NULL_GEOMETRY: " << numNullGeometry << std::endl;
    std::cout << "NUM_POLYGON: " << numPolygon << std::endl;

    if (genImageFlag) {
        for(imgYIdx=imageYSize-1; imgYIdx>=0; --imgYIdx) {
            for(imgXIdx=0; imgXIdx<imageXSize; ++imgXIdx) {
                if (imgXIdx) { fprintf(fppm, " "); }
                fprintf(fppm, "%s", colorList[image[imgXIdx][imgYIdx]].c_str());
            }
            fprintf(fppm, "\n");
        }
        fclose(fppm);

        std::string command = "convert " + tmpImageFile + " " + parameterTemplate.imageFile;
        std::cout << "COMMAND: " << command << std::endl;
        system(command.c_str());
    }

    GDALClose( outputDS );

    return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: DataSetClass::vectorCvg                                                ****/
/******************************************************************************************/
void DataSetClass::vectorCvg()
{
    GDALAllRegister();

    GdalDataModel *gdalDataModel = new GdalDataModel(parameterTemplate.srcFileVector, "");
    // gdalDataModel->printDebugInfo();

    double minLon, maxLon;
    double minLat, maxLat;

    OGRLayer *layer = gdalDataModel->getLayer();
    OGREnvelope oExt;
    if (layer->GetExtent(&oExt, TRUE) == OGRERR_NONE) {
        minLon = oExt.MinX;
        maxLon = oExt.MaxX;
        minLat = oExt.MinY;
        maxLat = oExt.MaxY;
    }

    std::cout << "MIN_LON = " << minLon << std::endl;
    std::cout << "MAX_LON = " << maxLon << std::endl;
    std::cout << "MIN_LAT = " << minLat << std::endl;
    std::cout << "MAX_LAT = " << maxLat << std::endl;

    double res = parameterTemplate.imageLonLatRes;

    int lonN0 = (int) floor(minLon / res);
    int lonN1 = (int) ceil(maxLon / res);
    int latN0 = (int) floor(minLat / res);
    int latN1 = (int) ceil(maxLat / res);

    std::string tmpImageFile = (parameterTemplate.tmpImageFile.empty() ? "/tmp/image.ppm" : parameterTemplate.tmpImageFile);

    /**************************************************************************************/
    /* Create PPM File                                                                    */
    /**************************************************************************************/
    FILE *fppm = (FILE *) NULL;
    int imageXSize = lonN1 - lonN0;
    int imageYSize = latN1 - latN0;
    int imgXIdx, imgYIdx;

    if ( !(fppm = fopen(tmpImageFile.c_str(), "wb")) ) {
        throw std::runtime_error("ERROR");
    }
    fprintf(fppm, "P3\n");
    fprintf(fppm, "%d %d %d\n", imageXSize, imageYSize, 255);
    /**************************************************************************************/

    /**************************************************************************************/
    /* Define color scheme                                                                */
    /**************************************************************************************/
    std::vector<std::string> colorList;
    colorList.push_back("255 255 255"); // 0: NO BLDG
    colorList.push_back("255   0   0"); // 1: BLDG
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Allocate and initialize image array                                          ****/
    /**************************************************************************************/
    int **image = (int **) malloc(imageXSize*sizeof(int *));
    for(imgXIdx=0; imgXIdx<imageXSize; ++imgXIdx) {
        image[imgXIdx] = (int *) malloc(imageYSize*sizeof(int));
        for(imgYIdx=0; imgYIdx<imageYSize; ++imgYIdx) {
            image[imgXIdx][imgYIdx] = 0;
        }
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Create Driver                                                                ****/
    /**************************************************************************************/
    const char *pszDriverName = "ESRI Shapefile";
    GDALDriver *poDriver;

    poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName );
    if( poDriver == NULL )
    {
        printf( "%s driver not available.\n", pszDriverName );
        exit( 1 );
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Add all polygons from layer                                                  ****/
    /**************************************************************************************/
    OGREnvelope env;
    std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> ptrOFeature;
    layer->ResetReading();
    while ((ptrOFeature = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(layer->GetNextFeature())) != NULL) {
        // GDAL maintains ownership of returned pointer, so we should not manage its memory
        OGRGeometry *ptrOGeometry = ptrOFeature->GetGeometryRef();

        if (ptrOGeometry != NULL) {
            OGRwkbGeometryType geometryType = ptrOGeometry->getGeometryType();
            if (geometryType == OGRwkbGeometryType::wkbPolygon) {

                ptrOGeometry->getEnvelope(&env);
#if 0
                std::cout << "    MIN_X " << env.MinX
                          << "    MAX_X " << env.MaxX
                          << "    MIN_Y " << env.MinY
                          << "    MAX_Y " << env.MaxY << std::endl;
#endif

                int x0 = ((int) floor(env.MinX / res)) - lonN0;
                int x1 = ((int) floor(env.MaxX / res)) - lonN0;
                int y0 = ((int) floor(env.MinY / res)) - latN0;
                int y1 = ((int) floor(env.MaxY / res)) - latN0;

                for(imgXIdx=x0; imgXIdx<=x1; imgXIdx++) {
                    for(imgYIdx=y0; imgYIdx<=y1; imgYIdx++) {
                        image[imgXIdx][imgYIdx] = 1;
                    }
                }
            } else {
                std::cout << "Contains features of type: " << OGRGeometryTypeToName(geometryType) << std::endl;
            }
        }
    }
    /**************************************************************************************/

    for(imgYIdx=imageYSize-1; imgYIdx>=0; --imgYIdx) {
        for(imgXIdx=0; imgXIdx<imageXSize; ++imgXIdx) {
            if (imgXIdx) { fprintf(fppm, " "); }
            fprintf(fppm, "%s", colorList[image[imgXIdx][imgYIdx]].c_str());
        }
        fprintf(fppm, "\n");
    }
    fclose(fppm);

    std::string command = "convert " + tmpImageFile + " " + parameterTemplate.imageFile;
    std::cout << "COMMAND: " << command << std::endl;
    system(command.c_str());

    return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: DataSetClass::mbRasterCvg                                              ****/
/******************************************************************************************/
void DataSetClass::mbRasterCvg()
{
    std::ostringstream errStr;
    GDALAllRegister();

    const char *pszFormat = "GTiff";
    GDALDriver *poDriver;
    char **papszMetadata;
    poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
    if( !poDriver ) {
        exit( 1 );
    }

    bool genImageFlag = (!parameterTemplate.imageFile.empty());

    papszMetadata = poDriver->GetMetadata();
    if( CSLFetchBoolean( papszMetadata, GDAL_DCAP_CREATE, FALSE ) )
        printf( "Driver %s supports Create() method.\n", pszFormat );
    if( CSLFetchBoolean( papszMetadata, GDAL_DCAP_CREATECOPY, FALSE ) )
        printf( "Driver %s supports CreateCopy() method.\n", pszFormat );

    GDALDataset *srcDS = (GDALDataset *) GDALOpen( parameterTemplate.srcFileRaster.c_str(), GA_ReadOnly );

    int nXSize = srcDS->GetRasterXSize();
    int nYSize = srcDS->GetRasterYSize();
    int numRasterBand = srcDS->GetRasterCount();

    printf( "Driver: %s/%s\n",
        srcDS->GetDriver()->GetDescription(),
        srcDS->GetDriver()->GetMetadataItem( GDAL_DMD_LONGNAME ) );
    printf( "Size is %dx%dx%d\n", nXSize, nYSize, numRasterBand );

    if( srcDS->GetProjectionRef()  != NULL ) {
        printf( "Projection is `%s'\n", srcDS->GetProjectionRef() );
    }

    double adfGeoTransform[6];
    if( srcDS->GetGeoTransform( adfGeoTransform ) == CE_None ) {
        printf( "Origin = (%.6f,%.6f)\n", adfGeoTransform[0], adfGeoTransform[3] );
        printf( "Pixel Size = (%.6f,%.6f)\n", adfGeoTransform[1], adfGeoTransform[5] );
    } else {
        throw std::runtime_error("ERROR in mbRasterCvg(), unable to determine origin/pixel size");
    }

    double pixelSize = adfGeoTransform[1];
    if ( fabs(pixelSize + adfGeoTransform[5]) > 1.0e-8 ) {
        throw std::runtime_error("ERROR: X / Y pixel sizes not properly set");
    }

    std::cout << "NUMBER RASTER BANDS: " << numRasterBand << std::endl;

    if (numRasterBand != 2) {
        throw std::runtime_error("ERROR in mbRasterCvg(), numRasterBand must be 2");
    }

    /**************************************************************************************/
    /* Define color scheme                                                                */
    /**************************************************************************************/
    std::vector<std::string> colorList;
    colorList.push_back("  0   0   0"); // 0: BE NO DATA
    colorList.push_back("  0 255   0"); // 1: BE VALID DATA
    colorList.push_back("  0 255 255"); // 2: BE Mix of data/nodata samples
    colorList.push_back("255 255 255"); // 3: NO BLDG
    colorList.push_back("255   0   0"); // 4: BLDG
    /**************************************************************************************/

    int N = (int) ceil( (parameterTemplate.imageLonLatRes/pixelSize) - 1.0e-8 );
    int imageXSize = (nXSize-1)/N + 1;
    int imageYSize = (nYSize-1)/N + 1;
    int imgXIdx, imgYIdx;
    int *imageScanline = (int *) malloc(imageXSize*sizeof(int));
    float *pafScanline = (float *) CPLMalloc(nXSize*GDALGetDataTypeSizeBytes(GDT_Float32));

    int rasterBandIdx;
    for(rasterBandIdx=1; rasterBandIdx<=numRasterBand; rasterBandIdx++) {
        GDALRasterBand *rasterBand = srcDS->GetRasterBand(rasterBandIdx);

        int hasNoData;
        float nodataValue = (float) rasterBand->GetNoDataValue(&hasNoData);
        if (hasNoData) {
            std::cout << "NODATA: " << nodataValue << std::endl;
        } else {
            std::cout << "NODATA undefined" << std::endl;
        }

        int xIdx, yIdx;
        if (parameterTemplate.verbose) {
            std::cout << "GDALGetDataTypeSizeBytes(GDT_Float32) = " << GDALGetDataTypeSizeBytes(GDT_Float32) << std::endl;
            std::cout << "sizeof(GDT_Float32) = " << sizeof(GDT_Float32) << std::endl;
            std::cout << "sizeof(GDT_Float64) = " << sizeof(GDT_Float64) << std::endl;
            std::cout << "sizeof(float) = " << sizeof(float) << std::endl;
        }

        /**************************************************************************************/
        /* Create PPM File                                                                    */
        /**************************************************************************************/
        std::string ppmFile = "/tmp/image_" + std::to_string(rasterBandIdx) + ".ppm";
        FILE *fppm = (FILE *) NULL;
        if ( !(fppm = fopen(ppmFile.c_str(), "wb")) ) {
            throw std::runtime_error("ERROR");
        }
        fprintf(fppm, "P3\n");
        fprintf(fppm, "%d %d %d\n", imageXSize, imageYSize, 255);
        for(imgXIdx=0; imgXIdx<imageXSize; ++imgXIdx) {
            imageScanline[imgXIdx] = ((rasterBandIdx == 1) ? -1 : 3);
        }
        /**************************************************************************************/

        int numNoData = 0;
        int numValid    = 0;
        for(yIdx=0; yIdx<nYSize; yIdx++) {
            rasterBand->RasterIO( GF_Read, 0, yIdx, nXSize, 1, pafScanline, nXSize, 1, GDT_Float32, 0, 0 );
            int colorIdx;
            for(xIdx=0; xIdx<nXSize; xIdx++) {
                imgXIdx = xIdx / N;

                if (rasterBandIdx == 1) {
                    if (hasNoData && (pafScanline[xIdx] == nodataValue)) {
                        numNoData++;
                        pafScanline[xIdx] = (float) parameterTemplate.nodataVal;
                        colorIdx = 0;
                    } else {
                        numValid++;
                        colorIdx = 1;
                    }

                    if (imageScanline[imgXIdx] == -1) {
                        imageScanline[imgXIdx] = colorIdx;
                    } else if (colorIdx != imageScanline[imgXIdx]) {
                        imageScanline[imgXIdx] = 2;
                    }
                } else if (rasterBandIdx == 2) {
                    if (hasNoData && (pafScanline[xIdx] == nodataValue)) {
                        numNoData++;
                    } else {
                        numValid++;
                        imageScanline[imgXIdx] = 4;
                    }
                }
            }
            if ((yIdx % N == N-1) || (yIdx == nYSize-1)) {
                for(imgXIdx=0; imgXIdx<imageXSize; ++imgXIdx) {
                    colorIdx = imageScanline[imgXIdx];
    
                    if (imgXIdx) { fprintf(fppm, " "); }
                    fprintf(fppm, "%s", colorList[colorIdx].c_str());

                    imageScanline[imgXIdx] = ((rasterBandIdx == 1) ? -1 : 3);
                }
                fprintf(fppm, "\n");
            }
        }

        fclose(fppm);

        std::cout << "Num NODATA values " << numNoData << " (" << 100.0*numNoData / (1.0*nXSize*nYSize) << "%)" << std::endl;
        std::cout << "Num VALID  values " << numValid << " (" << 100.0*numValid / (1.0*nXSize*nYSize) << "%)" << std::endl;

    }

    CPLFree(pafScanline);

    /* Once we're done, close properly the dataset */
    if( srcDS != NULL ) {
        GDALClose( (GDALDatasetH) srcDS );
    }

    std::string command;

    command = "convert /tmp/image_1.ppm " + parameterTemplate.imageFile;
    std::cout << "COMMAND: " << command << std::endl;
    system(command.c_str());

    command = "convert /tmp/image_2.ppm " + parameterTemplate.imageFile2;
    std::cout << "COMMAND: " << command << std::endl;
    system(command.c_str());
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: DataSetClass::procBoundary                                             ****/
/******************************************************************************************/
void DataSetClass::procBoundary()
{
    std::ostringstream errStr;

    GDALAllRegister();

    GdalDataModel *gdalDataModel = new GdalDataModel(parameterTemplate.srcFileVector, "");
    // gdalDataModel->printDebugInfo();

    OGRLayer *layer = gdalDataModel->getLayer();

    double minLon, maxLon;
    double minLat, maxLat;

#if 1
    gdalDataModel->getExtents(minLon, maxLon, minLat, maxLat, parameterTemplate.minLonWrap);
#else
    // If polygon wraps around discontinuity in longitude (-180 to 180) this does not work well
    OGREnvelope oExt;
    if (layer->GetExtent(&oExt, TRUE) == OGRERR_NONE) {
        minLon = oExt.MinX;
        maxLon = oExt.MaxX;
        minLat = oExt.MinY;
        maxLat = oExt.MaxY;
    }
#endif

    std::cout << "MIN_LON = " << minLon << std::endl;
    std::cout << "MAX_LON = " << maxLon << std::endl;
    std::cout << "MIN_LAT = " << minLat << std::endl;
    std::cout << "MAX_LAT = " << maxLat << std::endl;

    double samplesPerDeg = parameterTemplate.samplesPerDeg;

    int lonN0 = (int) floor(minLon * samplesPerDeg) - (parameterTemplate.polygonExpansion + 4);
    int lonN1 = (int) floor(maxLon * samplesPerDeg) + (parameterTemplate.polygonExpansion + 4);
    int latN0 = (int) floor(minLat * samplesPerDeg) - (parameterTemplate.polygonExpansion + 4);
    int latN1 = (int) floor(maxLat * samplesPerDeg) + (parameterTemplate.polygonExpansion + 4);

    int numLon = lonN1 - lonN0 + 1;
    int numLat = latN1 - latN0 + 1;
    int lonIdx, latIdx;

    double longitudeDegStart = ((double) lonN0     ) / samplesPerDeg;
    double longitudeDegStop  = ((double) lonN1  + 1) / samplesPerDeg;
    double latitudeDegStart  = ((double) latN0     ) / samplesPerDeg;
    double latitudeDegStop   = ((double) latN1  + 1) / samplesPerDeg;

#if 0
    FILE *fkml = (FILE *) NULL;
    /**************************************************************************************/
    /* Open KML File and write header                                                     */
    /**************************************************************************************/
    if ( !(fkml = fopen("/tmp/doc.kml", "wb")) ) {
        errStr << std::string("ERROR: Unable to open kmlFile \"") + "/tmp/doc.kml" + std::string("\"\n");
        throw std::runtime_error(errStr.str());
    }
    fprintf(fkml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fkml, "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n");
    fprintf(fkml, "\n");
    fprintf(fkml, "    <Document>\n");
    fprintf(fkml, "        <name>Shapefile Coverage</name>\n");
    fprintf(fkml, "        <open>1</open>\n");
    fprintf(fkml, "        <description>%s : Display Shapefile Coverage</description>\n", parameterTemplate.name.c_str());
    fprintf(fkml, "\n");
    /**************************************************************************************/
#endif

    std::string tmpImageFile = (parameterTemplate.tmpImageFile.empty() ? "/tmp/image.ppm" : parameterTemplate.tmpImageFile);

    /**************************************************************************************/
    /* Define color scheme                                                                */
    /**************************************************************************************/
    std::vector<std::string> colorList;
    colorList.push_back("255   0   0"); // 0: Not Used
    colorList.push_back("  0   0   0"); // 1: Polygon Interior
    colorList.push_back("255 255 255"); // 2: Polygon Exterior
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Allocate and initialize image array                                          ****/
    /**************************************************************************************/
    ImageClass *image = new ImageClass(lonN0, lonN1, latN0, latN1, samplesPerDeg);
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Create Driver                                                                ****/
    /**************************************************************************************/
    const char *pszDriverName = "ESRI Shapefile";
    GDALDriver *poDriver;

    poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName );
    if( poDriver == NULL )
    {
        printf( "%s driver not available.\n", pszDriverName );
        exit( 1 );
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Add all polygons from layer, one segment at a time                           ****/
    /**************************************************************************************/
    int totalNumberPoints = 0;
    OGREnvelope env;
    std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter> ptrOFeature;
    layer->ResetReading();
    while ((ptrOFeature = std::unique_ptr<OGRFeature, GdalHelpers::FeatureDeleter>(layer->GetNextFeature())) != NULL) {
        // GDAL maintains ownership of returned pointer, so we should not manage its memory
        OGRGeometry *ptrOGeometry = ptrOFeature->GetGeometryRef();

        if (ptrOGeometry != NULL) {
            bool useGeometryFlag;
            bool isMultiPolygonFlag;
            bool cont;
            OGRMultiPolygon *multipoly;
            OGRPolygon *poly;
            OGRPolygon **iter;
            OGRwkbGeometryType geometryType = ptrOGeometry->getGeometryType();
            if (geometryType == OGRwkbGeometryType::wkbPolygon) {
                poly = (OGRPolygon *) ptrOGeometry;
                useGeometryFlag = true;
                isMultiPolygonFlag = false;
            } else if (geometryType == OGRwkbGeometryType::wkbMultiPolygon ) {
                multipoly = (OGRMultiPolygon *) ptrOGeometry;
                iter = multipoly->begin();
                if (iter != multipoly->end()) {
                    useGeometryFlag = true;
                } else {
                    useGeometryFlag = false;
                }
                isMultiPolygonFlag = true;
            } else {
                useGeometryFlag = false;
                isMultiPolygonFlag = false;
                std::cout << "Ignore features of type: " << OGRGeometryTypeToName(geometryType) << std::endl;
            }

            if (useGeometryFlag) {
                do {
                    if (isMultiPolygonFlag) {
                        poly = *iter;
                    }

                    OGRLinearRing *pRing = poly->getExteriorRing();
                    int numPoints = pRing->getNumPoints();
                    // std::cout << "NUM_POINTS = " << pRing->getNumPoints() << std::endl;
                    if (numPoints > 2) {
                        double prevLon = pRing->getX(numPoints-1);
                        double prevLat = pRing->getY(numPoints-1);
                        while(prevLon < parameterTemplate.minLonWrap) {
                            prevLon += 360.0;
                        }
                        while(prevLon >= parameterTemplate.minLonWrap+360.0) {
                            prevLon -= 360.0;
                        }

                        int nx0 = (int) floor(prevLon * samplesPerDeg) - lonN0;
                        int ny0 = (int) floor(prevLat * samplesPerDeg) - latN0;
                        for(int ptIdx=0; ptIdx<pRing->getNumPoints(); ptIdx++) {
                            double lon = pRing->getX(ptIdx);
                            double lat = pRing->getY(ptIdx);
                            while(lon < parameterTemplate.minLonWrap) {
                                lon += 360.0;
                            }
                            while(lon >= parameterTemplate.minLonWrap+360.0) {
                                lon -= 360.0;
                            }

                            int nx1 = (int) floor(lon * samplesPerDeg) - lonN0;
                            int ny1 = (int) floor(lat * samplesPerDeg) - latN0;

                            // std::cout << "POINT " << ptIdx << ": " << lon << " " << lat << std::endl;

                            image->processSegment(prevLon, prevLat, nx0, ny0, lon, lat, nx1, ny1);

                            prevLon = lon;
                            prevLat = lat;
                            nx0 = nx1;
                            ny0 = ny1;
                            totalNumberPoints++;
                        }
                    } else {
                        std::cout << "WARNING: Polygon has " << numPoints << " virtices" << std::endl;
                    }

                    if (isMultiPolygonFlag) {
                        iter++;
                        if (iter != multipoly->end()) {
                            cont = true;
                        } else {
                            cont = false;
                        }
                    } else {
                        cont = false;
                    }
                } while(cont);

            }
        }
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /**** Everything is in image, done with gdalDataModel                              ****/
    /**************************************************************************************/
    delete gdalDataModel;
    /**************************************************************************************/

    std::cout << "TOTAL_NUM_POINTS = " << totalNumberPoints << std::endl;

    image->expand(1, parameterTemplate.polygonExpansion + 2);
    image->fill();

    image->changeVal(0, 1);
    image->changeVal(2, 0);

    std::vector<PolygonClass *> polyList = image->createPolygonList();

    // PolygonClass::writeMultiGeometry(polyList, std::string("/tmp/") + parameterTemplate.name + "_dbg.kml", 1.0/samplesPerDeg, parameterTemplate.name + "_dbg");

    int totalNumPts = 0;
    for(int polyIdx=0; polyIdx<polyList.size(); ++polyIdx) {
        PolygonClass *poly = polyList[polyIdx];
        double area = poly->comp_bdy_area();
        int numDel = 0;
        if (area > 100.0) {
            numDel = poly->simplify(0, parameterTemplate.polygonSimplify);
        }

        std::cout << "[" << polyIdx << "] POLYGON NUM_VIRTICES: " << poly->num_bdy_pt[0] << " Deleted " << numDel << " Points from polygon" << std::endl;
        totalNumPts += poly->num_bdy_pt[0];
    }
    std::cout << "TOTAL_NUM_VIRTICES: " << totalNumPts << std::endl;

    PolygonClass::writeMultiGeometry(polyList, parameterTemplate.kmlFile, 1.0/samplesPerDeg, parameterTemplate.name);

#if 0
    int interpolationFactor = 1;
    int maxPtsPerRegion = 1000;
    int numRegionLon = (numLon + maxPtsPerRegion - 1)/maxPtsPerRegion;
    int numRegionLat = (numLat + maxPtsPerRegion - 1)/maxPtsPerRegion;

    int lonRegionIdx;
    int latRegionIdx;
    int startLonIdx, stopLonIdx;
    int startLatIdx, stopLatIdx;
    int lonN = numLon/numRegionLon;
    int lonq = numLon%numRegionLon;
    int latN = numLat/numRegionLat;
    int latq = numLat%numRegionLat;

    for(lonRegionIdx=0; lonRegionIdx<numRegionLon; lonRegionIdx++) {

        if (lonRegionIdx < lonq) {
            startLonIdx = (lonN+1)*lonRegionIdx;
            stopLonIdx  = (lonN+1)*lonRegionIdx+lonN;
        } else {
            startLonIdx = lonN*lonRegionIdx + lonq;
            stopLonIdx  = lonN*lonRegionIdx + lonq + lonN - 1;
        }

        for(latRegionIdx=0; latRegionIdx<numRegionLat; latRegionIdx++) {

            if (latRegionIdx < latq) {
                startLatIdx = (latN+1)*latRegionIdx;
                stopLatIdx  = (latN+1)*latRegionIdx+latN;
            } else {
                startLatIdx = latN*latRegionIdx + latq;
                stopLatIdx  = latN*latRegionIdx + latq + latN - 1;
            }

            /**************************************************************************************/
            /* Create PPM File                                                                    */
            /**************************************************************************************/
            FILE *fppm;
            if ( !(fppm = fopen("/tmp/image.ppm", "wb")) ) {
                throw std::runtime_error("ERROR");
            }
            fprintf(fppm, "P3\n");
            fprintf(fppm, "%d %d %d\n", (stopLonIdx-startLonIdx+1)*interpolationFactor, (stopLatIdx-startLatIdx+1)*interpolationFactor, 255);

            for(latIdx=stopLatIdx; latIdx>=startLatIdx; --latIdx) {
                for(int interpLat=interpolationFactor-1; interpLat>=0; --interpLat) {
                    for(lonIdx=startLonIdx; lonIdx<=stopLonIdx; ++lonIdx) {
                        for(int interpLon=0; interpLon<interpolationFactor; interpLon++) {
                            if (lonIdx || interpLon) { fprintf(fppm, " "); }
                            fprintf(fppm, "%s", colorList[image->getVal(lonIdx,latIdx)].c_str());
                        }
                    }
                    fprintf(fppm, "\n");
                }
            }
            fclose(fppm);
            /**************************************************************************************/

            std::string pngFile = "/tmp/image_" + std::to_string(lonRegionIdx)
                                          + "_" + std::to_string(latRegionIdx) + ".png";

            std::string command = "convert /tmp/image.ppm -transparent white " + pngFile;
            std::cout << "COMMAND: " << command << std::endl;
            system(command.c_str());

            /**************************************************************************************/
            /* Write to KML File                                                                  */
            /**************************************************************************************/
            fprintf(fkml, "<GroundOverlay>\n");
            fprintf(fkml, "    <name>Region: %d_%d</name>\n", lonRegionIdx, latRegionIdx);
            fprintf(fkml, "    <visibility>%d</visibility>\n", 1);
            fprintf(fkml, "    <color>C0ffffff</color>\n");
            fprintf(fkml, "    <Icon>\n");
            fprintf(fkml, "        <href>image_%d_%d.png</href>\n", lonRegionIdx, latRegionIdx);
            fprintf(fkml, "    </Icon>\n");
            fprintf(fkml, "    <LatLonBox>\n");
            fprintf(fkml, "        <north>%.8f</north>\n", latitudeDegStart  + ((double) (stopLatIdx + 1))/samplesPerDeg);
            fprintf(fkml, "        <south>%.8f</south>\n", latitudeDegStart  + ((double) (startLatIdx   ))/samplesPerDeg);
            fprintf(fkml, "        <east>%.8f</east>\n",   longitudeDegStart + ((double) (stopLonIdx + 1))/samplesPerDeg);
            fprintf(fkml, "        <west>%.8f</west>\n",   longitudeDegStart + ((double) (startLonIdx   ))/samplesPerDeg);
            fprintf(fkml, "    </LatLonBox>\n");
            fprintf(fkml, "</GroundOverlay>\n");
            /**************************************************************************************/

        }
    }

    /**************************************************************************************/
    /* Write end of KML and close                                                         */
    /**************************************************************************************/
    if (fkml) {
        fprintf(fkml, "    </Document>\n");
        fprintf(fkml, "</kml>\n");
        fclose(fkml);

        std::string kmzFile = parameterTemplate.name + ".kmz";

        std::string command;

        std::cout << "CLEARING KMZ FILE: " << std::endl;
        command = "rm -fr " + kmzFile;
        system(command.c_str());

        command = "zip -j " + kmzFile + " /tmp/doc.kml";
        int caseIdx;
        for(lonRegionIdx=0; lonRegionIdx<numRegionLon; lonRegionIdx++) {
            for(latRegionIdx=0; latRegionIdx<numRegionLat; latRegionIdx++) {
                command += " /tmp/image_" + std::to_string(lonRegionIdx)
                                    + "_" + std::to_string(latRegionIdx) + ".png";
            }
        }
        std::cout << "COMMAND: " << command.c_str() << std::endl;
        system(command.c_str());
    }
    /**************************************************************************************/
#endif
    return;
}
/******************************************************************************************/
