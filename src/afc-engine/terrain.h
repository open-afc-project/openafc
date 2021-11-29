/******************************************************************************************/
/**** FILE : terrain.h                                                                 ****/
/******************************************************************************************/

#ifndef TERRAIN_H
#define TERRAIN_H

#include <atomic>
#include "cconst.h"
#include "ratcommon/SearchPaths.h"
#include "multiband_raster.h"
#include <QRectF>
#include <QPointF>
#include <map>
// Loggers
#include "rkflogging/ErrStream.h"
#include "rkflogging/Logging.h"
#include "rkflogging/LoggingConfig.h"
#include "DEPDataset.h"

// Use lidar files that have been pre-processed to have:
// bare-earth terrain height in band 1
// building height in band 2

class MultibandRasterClass;
struct LidarRegionStruct {
    std::string topPath;
    CConst::LidarFormatEnum format;
    std::string multibandFile;
    std::string cityName;
    double minLonDeg;
    double maxLonDeg;
    double minLatDeg;
    double maxLatDeg;
    MultibandRasterClass *multibandRaster;
};

class GdalDataDir;
class WorldData;

/******************************************************************************************/
/**** CLASS: TerrainClass                                                              ****/
/******************************************************************************************/
class TerrainClass
{
public:
    TerrainClass(QString lidarDir, std::string srtmDir, std::string depDir, QString globeDir,
        double terrainMinLat, double terrainMinLon, double terrainMaxLat, double terrainMaxLon,
        double terrainMinLatBldg, double terrainMinLonBldg, double terrainMaxLatBldg, double terrainMaxLonBldg,
        int maxLidarRegionLoadVal);
    ~TerrainClass();

    void readLidarInfo(QDir lidarDir);
    void readLidarData(double terrainMinLat, double terrainMinLon, double terrainMaxLat, double terrainMaxLon);
    int getLidarRegion(double lonDeg, double latDeg) const;
    void loadLidarRegion(int lidarRegionIdx);

    void printStats();

    void getTerrainHeight(double longitudeDeg, double latitudeDeg, double& terrainHeight, double& bldgHeight, MultibandRasterClass::HeightResult& lidarHeightResult, CConst::HeightSourceEnum& heightSource) const;

    void writeTerrainProfile(std::string filename, double startLongitudeDeg, double startLatitudeDeg, double startHeightAboveTerrain,
                                                   double  stopLongitudeDeg, double  stopLatitudeDeg, double  stopHeightAboveTerrain);
    int getNumLidarRegion();
    LidarRegionStruct& getLidarRegion(int lidarRegionIdx);

    void setSourceName(const CConst::HeightSourceEnum& sourceVal, const std::string& sourceName);
    const std::string& getSourceName(const CConst::HeightSourceEnum& sourceVal) const;

    std::vector<QRectF> getBounds() const;
private:
    /**************************************************************************************/
    /**** Data                                                                         ****/
    /**************************************************************************************/
    std::vector<LidarRegionStruct> lidarRegionList;
    std::vector<int> activeLidarRegionList;
    /**************************************************************************************/

    int maxLidarRegionLoad;

    GdalDataDir *gdalDir;
    DEPDatasetClass *depDataset;  // DEP terrain model
    WorldData *globeModel;

    std::map<CConst::HeightSourceEnum, std::string> sourceNames = {};

    static std::atomic_llong numLidar;
    static std::atomic_llong numSRTM;
    static std::atomic_llong numDEP;
    static std::atomic_llong numGlobal;

    bool useLidarZIPFiles;
    std::string lidarWorkingDir;

};
/******************************************************************************************/

#endif
