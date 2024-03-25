//

#ifndef SQL_HELPERS_H
#define SQL_HELPERS_H

//#include "cposql_export.h"
#include "ratcommon/EnvironmentFlag.h"
#include <QSqlDriver>
#include <QSqlQuery>

class QVariant;

namespace SqlHelpers
{
/** Extract environment variable "CPO_SQL_DEBUG" one time and cache.
 * This function is thread safe.
 * @return True if debugging is enabled.
 */
// CPOSQL_EXPORT
extern EnvironmentFlag doDebug;

/** Guarantee that an SQL driver has a required feature.
 *
 * @param drv The driver to check.
 * @param feature The feature required.
 * @throw SqlError If the feature is not present.
 */
void ensureFeature(const QSqlDriver &drv, QSqlDriver::DriverFeature feature);

/** Override Qt encoding rules for SQL types.
 * For MySQL and PostgreSQL, Qt does not properly format datetime values.
 *
 * @param value The value to encode.
 * @return The same value, or a string-encoded represenation.
 */
QVariant encode(const QVariant &value);

/** Get a quoted representation of a given value.
 * This uses the underlying database driver QSqlDriver::formatValue().
 *
 * @param db The database used to get driver info from.
 * @param value The type of the value is used to determine the quoting and
 * the string representation is used to give the output.
 * @return The quoted value representation.
 */
QString quoted(const QSqlDriver *driver, const QVariant &value);

/** Apply a table namespace prefix to a list of column names.
 *
 * @param prefix The namespace to prepend.
 * @param cols The individual column names.
 * @return The prefixed column names.
 */
QStringList prefixCols(const QString &prefix, const QStringList &cols);

/** Attempt to prepare a specific SQL query.
 *
 * @param db The database to execute in.
 * @param query The full query string.
 * @return The prepared query object.
 * @throw SqlError If the query failed.
 * @post If the DB driver is MySQL and the error is a failed connection,
 * then the DB connection is closed.
 */
QSqlQuery prepare(const QSqlDatabase &db, const QString &query);

/** Attempt to execute a prepared SQL query.
 *
 * @param db The database to execute in.
 * @param qObj The prepared query object.
 * @throw SqlError If the query failed.
 * @post If the DB driver is MySQL and the error is a failed connection,
 * then the DB connection is closed.
 */
void execPrepared(const QSqlDatabase &db, QSqlQuery &qObj);

/** Get the list of bound positional placeholdervalues.
 *
 * @param query The query which is bound to.
 * @return The list of bound values.
 */
QStringList boundList(const QSqlQuery &query);

/** Attempt to execute a specific SQL query.
 *
 * @param db The database to execute in.
 * @param query The full query string.
 * @return The active, valid query result.
 * @throw SqlError If the query failed.
 * @post If the DB driver is MySQL and the error is a failed connection,
 * then the DB connection is closed.
 */
QSqlQuery exec(const QSqlDatabase &db, const QString &query);
}

#endif /* SQL_HELPERS_H */
