// UlsDatabase.h: header file for reading ULS database data on startup
// author: Sam Smucny

#ifndef AFCENGINE_ULS_DATABASE_H_
#define AFCENGINE_ULS_DATABASE_H_

#include <vector>
#include <string>
#include <QString>
#include <exception>

#include "cconst.h"

struct UlsRecord
{
    int fsid;
    std::string callsign;
    std::string radioService;
    std::string entityName;
    std::string rxCallsign;
    int rxAntennaNumber;
    std::string emissionsDesignator;
    double startFreq, stopFreq;
    double rxLatitudeDeg, rxLongitudeDeg;
    double rxGroundElevation;
    double rxHeightAboveTerrain;
    double txLatitudeDeg, txLongitudeDeg;
    double txGroundElevation;
    std::string txPolarization;
    double txHeightAboveTerrain;
    double rxGain;
    double txGain;
    double txEIRP;
    std::string status;
    bool mobile;
    std::string rxAntennaModel;
    bool hasPR;
    double prLatitudeDeg;
    double prLongitudeDeg;
    double prHeightAboveTerrain;
};

class UlsDatabase
{
    public:

    // Loads all FS within lat/lon bounds
    static void loadUlsData(const QString& dbName, std::vector<UlsRecord>& target,
            const double& minLat=-90, const double& maxLat=90, const double& minLon=-180, const double& maxLon=180);

    // Loads a single FS by looking up its Id
    static void loadFSById(const QString& dbName, std::vector<UlsRecord>& target, const int& fsid);
    static UlsRecord getFSById(const QString& dbName, const int& fsid)
    {
        // list of size 1
        auto list = std::vector<UlsRecord>();
        loadFSById(dbName, list, fsid);
        if (list.size() != 1)
            throw std::runtime_error("FS not found");
        return list.at(0);
    };
};

#endif /* AFCENGINE_ULS_DATABASE_H */
