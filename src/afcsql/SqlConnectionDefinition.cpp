//

#include <QSqlDatabase>
#include <QDebug>
#include "SqlConnectionDefinition.h"
#include "ratcommon/TextHelpers.h"
#include "afclogging/Logging.h"
#include "SqlError.h"

namespace
{
/// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "SqlConnectionDefinition")

/// Number of hexadecimal digits to make the random name
const int dbNameDigits = 10;
}

SqlConnectionDefinition::SqlConnectionDefinition() : hostName("127.0.0.1"), port(0)
{
}

void SqlConnectionDefinition::configureDb(QSqlDatabase &db) const
{
	db.setHostName(hostName);
	if (port > 0) {
		db.setPort(port);
	}
	db.setDatabaseName(dbName);
	db.setUserName(userName);
	db.setPassword(password);
	db.setConnectOptions(options);
}

QSqlDatabase SqlConnectionDefinition::newConnection() const
{
	if (!QSqlDatabase::isDriverAvailable(driverName)) {
		throw SqlError(QString("SQL driver not available \"%1\"").arg(driverName));
	}

	// Search for an unused random connection name
	QString connName;
	do {
		connName = TextHelpers::randomHexDigits(dbNameDigits);
	} while (QSqlDatabase::contains(connName));

	QSqlDatabase db = QSqlDatabase::addDatabase(driverName, connName);
	configureDb(db);
	if (!db.isValid()) {
		throw SqlError("Bad SQL configuration", db.lastError());
	}
	LOGGER_DEBUG(logger) << "newConnection " << connName << " to " << driverName << "://"
			     << userName << "@" << hostName << ":" << port << "/" << dbName;
	return db;
}
