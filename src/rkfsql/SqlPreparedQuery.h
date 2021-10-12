//  

#ifndef PREPARED_QUERY_H
#define PREPARED_QUERY_H

#include <QString>
#include <QSqlQuery>
#include <QVariant>

class SqlSelect;
class SqlInsert;
class SqlUpdate;
class SqlDelete;

/** Wrapper class around QSqlQuery to provide a binding interface similar
 * to QString::arg() and to throw SqlError upon failure.
 *
 * This is intended to be used as:
 * @code
 * result = PreparedQuery("SELECT * FROM table WHERE col = ?").bind("value").run();
 * @endcode
 */
class SqlPreparedQuery : public QSqlQuery{
public:
    /** Generate a binding list of question marks.
     * @param number The number of parameters-to-be-bound.
     * @return A string containing the desired number of question marks.
     */
    static QString qMark(int number);

    /// Default constructor does nothing
    SqlPreparedQuery(){}

    /** Construct from SELECT expression.
     *
     * @param query The query object to take the prepared expression from.
     */
    SqlPreparedQuery(const SqlSelect &query);

    /** Construct from INSERT expression.
     *
     * @param query The query object to take the prepared expression from.
     */
    SqlPreparedQuery(const SqlInsert &query);

    /** Construct from UPDATE expression.
     *
     * @param query The query object to take the expression from.
     */
    SqlPreparedQuery(const SqlUpdate &query);

    /** Construct from DELETE expression.
     *
     * @param query The query object to take the expression from.
     */
    SqlPreparedQuery(const SqlDelete &query);

    /** Prepare (but do not execute) a given query.
     * @param db The database in which the query is to be run.
     * @param query The query string, with possible unbound values.
     * @throw SqlError if the query fails to prepare.
     */
    SqlPreparedQuery(const QSqlDatabase &db, const QString &query);

    /** Bind a single parameter to a prepared query.
     * @param param The parameter value to bind.
     * @return The modified query.
     */
    SqlPreparedQuery & bind(const QVariant &param);

    /** Bind multiple parameters to a prepared query.
     * @param params The parameters value to bind.
     * @return The modified query.
     */
    SqlPreparedQuery & bindList(const QVariantList &params);

    /** Execute the query and return the QSqlQuery result.
     * @return The successful result of a query execution.
     * @throw SqlError if the query fails to execute.
     */
    QSqlQuery & run();
    
private:
    /// Database connection. This is not accessible by QSqlQuery.
    QSqlDatabase _db;
};

#endif /* PREPARED_QUERY_H */
