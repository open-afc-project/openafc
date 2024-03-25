//

#ifndef SQL_CONNECTION_DEFINITION_H
#define SQL_CONNECTION_DEFINITION_H

#include <QString>

class QSqlDatabase;

/** Configuration for a single server.
 * This class provides convenience behavior for packing and unpacking
 * definitions into common "host:port" strings.
 */
struct SqlConnectionDefinition {
		/** Initialization of invalid definition.
		 */
		SqlConnectionDefinition();

		/** Load the definition into a DB connection object.
		 *
		 * @param db The database to configure with current settings.
		 */
		void configureDb(QSqlDatabase &db) const;

		/** Create a new DB connection with a random Qt DB name.
		 *
		 * @return The new thread-local connection.
		 */
		QSqlDatabase newConnection() const;

		/// Qt SQL driver name (e.g. QMYSQL, QODBC)
		QString driverName;
		/// Host name (defaults to local IP4 address
		QString hostName;
		/// TCP port number to connect to (if zero, it is invalid)
		quint16 port;
		/// Extra options for QSqlDatabase::
		QString options;

		/// Name of the DB schema (not the host name)
		QString dbName;
		/// User name for the connection
		QString userName;
		/// Password for the connection
		QString password;
};

#endif /* SQL_CONNECTION_DEFINITION_H */
