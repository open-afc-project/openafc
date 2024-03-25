#include "PopulationDatabase.h"

#include <QStringList>
#include <QSqlQuery>
#include <QSqlDriver>
#include <QSqlResult>
#include <QSqlError>
#include <afcsql/SqlConnectionDefinition.h>
#include <afcsql/SqlExceptionDb.h>
#include <afcsql/SqlScopedConnection.h>
#include <afcsql/SqlTransaction.h>
#include <afcsql/SqlPreparedQuery.h>
#include <afcsql/SqlTable.h>
#include <afcsql/SqlSelect.h>
#include <afcsql/SqlTransaction.h>
#include <afclogging/Logging.h>
#include "afclogging/ErrStream.h"

namespace
{
// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "PopulationDatabase")

} // end namespace

// Loads population data from an sql database that is stored in a vector
void PopulationDatabase::loadPopulationData(const QString &dbName,
					    std::vector<PopulationRecord> &target,
					    const double &minLat,
					    const double &maxLat,
					    const double &minLon,
					    const double &maxLon)
{
	LOGGER_DEBUG(logger) << "Bounds: " << minLat << ", " << maxLat << "; " << minLon << ", "
			     << maxLon;

	// create and open db connection
	SqlConnectionDefinition config;
	config.driverName = "QSQLITE";
	config.dbName = dbName;

	LOGGER_INFO(logger) << "Opening database: " << dbName;
	SqlScopedConnection<SqlExceptionDb> db(new SqlExceptionDb(config.newConnection()));
	db->tryOpen();

	LOGGER_DEBUG(logger) << "Querying population database";
	auto populationQueryRes = SqlSelect(*db, "population")
					  .cols(QStringList() << "latitude"
							      << "longitude"
							      << "density")
					  .whereBetween("latitude",
							std::min(minLat, maxLat),
							std::max(minLat, maxLat))
					  .whereBetween("longitude",
							std::min(minLon, maxLon),
							std::max(minLon, maxLon))
					  .run();

	LOGGER_DEBUG(logger) << "Is Active: " << populationQueryRes.isActive();
	LOGGER_DEBUG(logger) << "Is Select: " << populationQueryRes.isSelect();
	if (!populationQueryRes.isActive()) {
		// Query encountered error
		QSqlError err = populationQueryRes.lastError();
		throw std::runtime_error(ErrStream() << "PopulationDatabase.cpp: Database query "
							"failed with code "
						     << err.type() << " " << err.text());
	}

	// resize vector to fit result
	if (populationQueryRes.driver()->hasFeature(QSqlDriver::QuerySize)) {
		// if the driver supports .size() then use it because is is more efficient
		LOGGER_DEBUG(logger) << target.size() << " to " << populationQueryRes.size();
		target.resize(populationQueryRes.size());
		populationQueryRes.setForwardOnly(true);
	} else {
		if (!populationQueryRes.last()) {
			throw std::runtime_error(ErrStream()
						 << "PopulationDatabase.cpp: Failed to get last "
						    "item. Check that lat/lon are within CONUS : "
						 << populationQueryRes.at());
		}
		LOGGER_DEBUG(logger) << target.size() << " to " << populationQueryRes.at();
		target.resize(populationQueryRes.at() + 1);
		populationQueryRes.first();
		populationQueryRes.previous();
	}

	while (populationQueryRes.next()) {
		int r = populationQueryRes.at();
		target.at(r).latitude = populationQueryRes.value(0).toDouble();
		target.at(r).longitude = populationQueryRes.value(1).toDouble();
		target.at(r).density = populationQueryRes.value(2).toDouble();
	}

	LOGGER_DEBUG(logger) << target.size() << " rows retreived";
}
