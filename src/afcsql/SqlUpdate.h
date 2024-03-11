// 

#ifndef SQL_UPDATE_H
#define SQL_UPDATE_H

#include <QSqlDatabase>
#include <QStringList>

class SqlTable;
class QVariant;

/** An interface specifically for the particular needs of the UPDATE query.
 */
class SqlUpdate{
public:
    /** Create an invalid object which can be assigned-to later.
     */
    SqlUpdate(){}

    /** Create a new UPDATE query on a given database and table.
     * @param db The database to query.
     * @param tableExpr The definition of the table(s) to update.
     */
    SqlUpdate(const QSqlDatabase &db, const QString &tableExpr);

    /** Create a new UPDATE query on a given database and table.
     * @param db The database to query.
     * @param table The definition of the table(s) to update.
     */
    SqlUpdate(const QSqlDatabase &db, const SqlTable &table);

    /** Get the underlying database object.
     * @param The database to query.
     */
    const QSqlDatabase & database() const{
        return _db;
    }

    /** Add a new column and value to set.
     *
     * @param expr The full SET clause to add.
     * @return The updated query object.
     */
    SqlUpdate & set(const QString &expr){
        _setExprs.append(expr);
        return *this;
    }

    /** Add a new column and value to set.
     *
     * @param col The column name to set.
     * @param expr The SQL expression to set to.
     * @return The updated query object.
     */
    SqlUpdate & setExpr(const QString &col, const QString &expr){
        return set(QString("%1=(%2)").arg(col, expr));
    }

    /** Add a new column to set with a prepared query.
     * All SET placeholders occur before any WHERE placeholders.
     *
     * @param col The column name to set.
     * @return The updated query object.
     * @sa PreparedQuery
     */
    SqlUpdate & setPlaceholder(const QString &col);

    /** Add a new column and value to set.
     *
     * @param col The column name to set.
     * @param expr The value to set to. This value will be properly quoted.
     * @return The updated query object.
     */
    SqlUpdate & setValue(const QString &col, const QVariant &value);

    /** Add an arbitrary WHERE clause to the UPDATE.
     * Each call to this function adds a new clause which will be joined with
     * the "AND" operator.
     *
     * @param expr The WHERE expression.
     * @return The updated query object.
     */
    SqlUpdate & where(const QString &expr){
        _whereExprs << expr;
        return *this;
    }

    /** Add a WHERE clause to the UPDATE for a single column.
     * @param col The name of the column to filter.
     * @param value The value which must be in the column.
     * @return The updated query object.
     */
    SqlUpdate & whereEqual(const QString &col, const QVariant &value);

    /** Add a WHERE clause to the UPDATE for a single column.
     * @param col The name of the column to filter to include @c null values.
     * @return The updated query object.
     */
    SqlUpdate & whereNull(const QString &col);

    /** Add a WHERE clause to the UPDATE for a single column.
     * This is intended to be used in prepared queries, where particular
     * values are bound later.
     * @param col The name of the column to filter.
     * @return The updated query object.
     * @sa PreparedQuery
     */
    SqlUpdate & whereEqualPlaceholder(const QString &col);

    /** Get the SQL query string which would be executed by run().
     * @return The query string to be executed.
     * The order of the query is defined by @xref{SQL-92} with DB-specific
     * clauses at end.
     */
    QString query() const;

    /** Build and execute the query and return the QSqlQuery result.
     * @return The successful result of a query execution.
     * @throw SqlError if the query fails to execute.
     */
    QSqlQuery run() const;

private:
    /// Underlying database
    QSqlDatabase _db;
    /// Fully formed table expression
    QString _tableExpr;
    /// List of quoted column names and values to set
    QStringList _setExprs;
    /// List of WHERE clauses to be ANDed together, with unbound values
    /// The clauses should be parenthesized to avoid error
    QStringList _whereExprs;
};

#endif /* SQL_UPDATE_H */
