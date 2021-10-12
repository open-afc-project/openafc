#include "UlsDatabase.h"

#include <limits>
#include <QStringList>
#include <QSqlQuery>
#include <QSqlDriver>
#include <QSqlResult>
#include <QSqlError>
#include <rkfsql/SqlConnectionDefinition.h>
#include <rkfsql/SqlExceptionDb.h>
#include <rkfsql/SqlScopedConnection.h>
#include <rkfsql/SqlTransaction.h>
#include <rkfsql/SqlPreparedQuery.h>
#include <rkfsql/SqlTable.h>
#include <rkfsql/SqlSelect.h>
#include <rkfsql/SqlTransaction.h>
#include <rkflogging/Logging.h>
#include "rkflogging/ErrStream.h"

namespace 
{
    // Logger for all instances of class
    LOGGER_DEFINE_GLOBAL(logger, "UlsDatabase")

} // end namespace

QStringList columns = QStringList()
            << "fsid"
            << "callsign"
            << "radio_service"
            << "name"
            << "rx_callsign"
            << "rx_antenna_num"
            << "freq_assigned_start_mhz"
            << "freq_assigned_end_mhz"
            << "emissions_des"
            << "tx_lat_deg"
            << "tx_long_deg"
            << "tx_ground_elev_m"
            << "tx_polarization"
            << "tx_gain"
            << "tx_eirp"
            << "tx_height_to_center_raat_m"
            << "rx_lat_deg"
            << "rx_long_deg"
            << "rx_ground_elev_m"
            << "rx_height_to_center_raat_m"
            << "rx_gain"
            << "status"
            << "mobile"
            << "rx_ant_model";

void verifyResult(const QSqlQuery& ulsQueryRes)
{
    LOGGER_DEBUG(logger) << "Is Active: " << ulsQueryRes.isActive();
    LOGGER_DEBUG(logger) << "Is Select: " << ulsQueryRes.isSelect();
    if (!ulsQueryRes.isActive())
    {
        // Query encountered error
        QSqlError err = ulsQueryRes.lastError();
        throw std::runtime_error(ErrStream() << "UlsDatabase.cpp: Database query failed with code " << err.type() << " " << err.text());

    }
}


// construct and run sql query and return result
QSqlQuery runQueryWithBounds(const SqlScopedConnection<SqlExceptionDb>& db, 
    const double& minLat, const double& maxLat, const double& minLon, const double& maxLon);
QSqlQuery runQueryById(const SqlScopedConnection<SqlExceptionDb>& db, const int& fsid);

// fill target vector with records from query result
void fillTarget(std::vector<UlsRecord>& target, QSqlQuery& ulsQueryRes);

void UlsDatabase::loadFSById(const QString& dbName, std::vector<UlsRecord>& target, const int& fsid)
{
    LOGGER_DEBUG(logger) << "FSID: " << fsid;

    // create and open db connection
    SqlConnectionDefinition config;
    config.driverName = "QSQLITE";
    config.dbName = dbName;

    LOGGER_INFO(logger) << "Opening database: " << dbName;
    SqlScopedConnection<SqlExceptionDb> db(new SqlExceptionDb(config.newConnection()));
    db->tryOpen();
    
    LOGGER_DEBUG(logger) << "Querying uls database";
    QSqlQuery ulsQueryRes = runQueryById(db, fsid);

    verifyResult(ulsQueryRes);

    fillTarget(target, ulsQueryRes);
    
}

void UlsDatabase::loadUlsData(const QString& dbName, std::vector<UlsRecord>& target,
    const double& minLat, const double& maxLat, const double& minLon, const double& maxLon)
{
    LOGGER_DEBUG(logger) << "Bounds: " << minLat << ", " << maxLat << "; " << minLon << ", " << maxLon;

    // create and open db connection
    SqlConnectionDefinition config;
    config.driverName = "QSQLITE";
    config.dbName = dbName;

    LOGGER_INFO(logger) << "Opening database: " << dbName;
    SqlScopedConnection<SqlExceptionDb> db(new SqlExceptionDb(config.newConnection()));
    db->tryOpen();
    
    LOGGER_DEBUG(logger) << "Querying uls database";
    QSqlQuery ulsQueryRes = runQueryWithBounds(db, minLat, maxLat, minLon, maxLon);

    verifyResult(ulsQueryRes);

    fillTarget(target, ulsQueryRes);
}

QSqlQuery runQueryWithBounds(const SqlScopedConnection<SqlExceptionDb>& db, 
    const double& minLat, const double& maxLat, const double& minLon, const double& maxLon)
{
    return SqlSelect(*db, "uls")
        .cols(columns)
        .where(QString(
                "(rx_lat_deg BETWEEN %1 AND %2)"
                "AND"
                "(rx_long_deg BETWEEN %3 AND %4)"
            )
            .arg(std::min(minLat, maxLat))
            .arg(std::max(minLat, maxLat))
            .arg(std::min(minLon, maxLon))
            .arg(std::max(minLon, maxLon))
        )
        .order("fsid")
        .run();
}

QSqlQuery runQueryById(const SqlScopedConnection<SqlExceptionDb>& db, const int& fsid)
{
    return SqlSelect(*db, "uls")
        .cols(columns)
        .where(QString("fsid=%1").arg(fsid))
        .topmost(1)
        .run();
}

void fillTarget(std::vector<UlsRecord>& target, QSqlQuery& q)
{
    // resize vector to fit result
    if (q.driver()->hasFeature(QSqlDriver::QuerySize))
    {
        // if the driver supports .size() then use it because is is more efficient
        LOGGER_DEBUG(logger) << target.size() << " to " << q.size();
        target.resize(q.size());
        q.setForwardOnly(true);
    }
    else
    {
        if (!q.last())
        {
            // throw std::runtime_error(ErrStream() << "UlsDatabase.cpp: Failed to get last item. Check that lat/lon are within CONUS : " << q.at());
            // No FS's within 150 Km, return with empty list
            return;
        }
        LOGGER_DEBUG(logger) << target.size() << " to last " << q.at();
        target.resize(q.at()+1);
        q.first();
        q.previous();
    }
    
    double nan = std::numeric_limits<double>::quiet_NaN();
    while (q.next())
    {
        int r = q.at();

        target.at(r).fsid = q.value(0).toInt();
        target.at(r).callsign = q.value(1).toString().toStdString();
        target.at(r).radioService = q.value(2).toString().toStdString();
        target.at(r).entityName = q.value(3).toString().toStdString();
        target.at(r).rxCallsign = q.value(4).toString().toStdString();
        target.at(r).rxAntennaNumber = q.value(5).toInt();
        target.at(r).startFreq = q.value(6).toDouble();
        target.at(r).stopFreq = q.value(7).toDouble();
        target.at(r).emissionsDesignator = q.value(8).toString().toStdString();
        target.at(r).txLatitudeDeg = q.value(9).toDouble();
        target.at(r).txLongitudeDeg = q.value(10).toDouble();
        target.at(r).txGroundElevation = q.value(11).isNull() ? nan : q.value(11).toDouble();
        target.at(r).txPolarization = q.value(12).toString().toStdString();
        target.at(r).txGain = q.value(13).isNull() ? nan : q.value(13).toDouble();
        target.at(r).txEIRP = q.value(14).toDouble();
        target.at(r).txHeightAboveTerrain = q.value(15).isNull() ? nan : q.value(15).toDouble();
        target.at(r).rxLatitudeDeg = q.value(16).toDouble();
        target.at(r).rxLongitudeDeg = q.value(17).toDouble();
        target.at(r).rxGroundElevation = q.value(18).isNull() ? nan : q.value(18).toDouble();
        target.at(r).rxHeightAboveTerrain = q.value(19).isNull() ? nan : q.value(19).toDouble();
        target.at(r).rxGain = q.value(20).isNull() ? nan : q.value(20).toDouble();
        target.at(r).status = q.value(21).toString().toStdString();
        target.at(r).mobile = q.value(22).toBool();
        target.at(r).rxAntennaModel = q.value(23).toString().toStdString();
    }
    LOGGER_DEBUG(logger) << target.size() << " rows retreived";
}
