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
#include <QDir>
#include <map>
#include <memory>
// Loggers
#include "afclogging/ErrStream.h"
#include "afclogging/Logging.h"
#include "afclogging/LoggingConfig.h"
#include "CachedGdal.h"

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

/******************************************************************************************/
/**** CLASS: TerrainClass                                                              ****/
/******************************************************************************************/
class TerrainClass
{
    public:
        TerrainClass(std::string lidarDir, std::string cdsmDir, std::string srtmDir, std::string depDir, std::string globeDir, double terrainMinLat,
            double terrainMinLon, double terrainMaxLat, double terrainMaxLon, double terrainMinLatBldg, double terrainMinLonBldg,
            double terrainMaxLatBldg, double terrainMaxLonBldg, int maxLidarRegionLoadVal);
        ~TerrainClass();

        void readLidarInfo(std::string lidarDir);
        void readLidarData(double terrainMinLat, double terrainMinLon, double terrainMaxLat, double terrainMaxLon);
        int getLidarRegion(double lonDeg, double latDeg) const;
        void loadLidarRegion(int lidarRegionIdx);

        void printStats();

        void getTerrainHeight(double longitudeDeg, double latitudeDeg, double &terrainHeight, double &bldgHeight,
            MultibandRasterClass::HeightResult &lidarHeightResult, CConst::HeightSourceEnum &heightSource, bool cdsmFlag = false) const;

        void writeTerrainProfile(std::string filename, double startLongitudeDeg, double startLatitudeDeg, double startHeightAboveTerrain,
            double stopLongitudeDeg, double stopLatitudeDeg, double stopHeightAboveTerrain);
        int getNumLidarRegion();
        LidarRegionStruct &getLidarRegion(int lidarRegionIdx);

        void setSourceName(const CConst::HeightSourceEnum &sourceVal, const std::string &sourceName);
        const std::string &getSourceName(const CConst::HeightSourceEnum &sourceVal) const;

        std::vector<QRectF> getBounds() const;

        bool getGdalDirectMode() const;
        bool setGdalDirectMode(bool newGdalDirectMode);

        static std::atomic_llong numITM;

    private:
        /**************************************************************************************/
        /**** Data ****/
        /**************************************************************************************/
        std::vector<LidarRegionStruct> lidarRegionList;
        std::vector<int> activeLidarRegionList;
        /**************************************************************************************/

        double minLidarLongitude, maxLidarLongitude;
        double minLidarLatitude, maxLidarLatitude;
        int maxLidarRegionLoad;

        std::shared_ptr<CachedGdal<float>> cgCdsm;
        std::shared_ptr<CachedGdal<int16_t>> cgSrtm;
        std::shared_ptr<CachedGdal<float>> cgDep;
        std::shared_ptr<CachedGdal<int16_t>> cgGlobe;
        bool gdalDirectMode;

        std::map<CConst::HeightSourceEnum, std::string> sourceNames = {};

        static std::atomic_llong numLidar;
        static std::atomic_llong numCDSM;
        static std::atomic_llong numSRTM;
        static std::atomic_llong numDEP;
        static std::atomic_llong numGlobal;

        std::string lidarWorkingDir;
};
/******************************************************************************************/

#endif
