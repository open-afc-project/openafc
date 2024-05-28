//

#ifndef SQL_DELETE_H
#define SQL_DELETE_H

#include <QSqlDatabase>
#include <QStringList>
#include "SqlTable.h"

/** An interface specifically for the particular needs of the UPDATE query.
 */
class SqlDelete
{
    public:
        /** Create an invalid object which can be assigned-to later.
         */
        SqlDelete()
        {
        }

        /** Create a new DELETE query on a given database and table.
         * @param db The database to query.
         * @param tableName The definition of the table to delete from.
         */
        SqlDelete(const QSqlDatabase &db, const QString &tableName);

        /** Create a complex DELETE based on a joined table.
         * @param db The database to query.
         * @param tableName The name of the table to delete from.
         * @param data The data table to query with FROM clauses.
         */
        SqlDelete(const QSqlDatabase &db, const QString &tableName, const SqlTable &data);

        /** Get the underlying database object.
         * @param The database to query.
         */
        const QSqlDatabase &database() const
        {
            return _db;
        }

        /** Add an arbitrary WHERE clause to the DELETE.
         * Each call to this function adds a new clause which will be joined with
         * the "AND" operator.
         *
         * @param expr The WHERE expression.
         * @return The updated query object.
         */
        SqlDelete &where(const QString &expr)
        {
            _whereExprs << expr;
            return *this;
        }

        /** Add a WHERE clause to the DELETE for a single column.
         * @param col The name of the column to filter, including only @c null values.
         * @return The updated query object.
         */
        SqlDelete &whereNull(const QString &col);

        /** Add a WHERE clause to the DELETE for a single column.
         * This is intended to be used in prepared queries, where particular
         * values are bound later.
         * @param col The name of the column to filter.
         * @return The updated query object.
         * @sa PreparedQuery
         */
        SqlDelete &whereEqualPlaceholder(const QString &col);

        /** Add a WHERE clause to the DELETE for a single column.
         * @param col The name of the column to filter.
         * @param value The value which must be in the column.
         * @return The updated query object.
         */
        SqlDelete &whereEqual(const QString &col, const QVariant &value);

        /** Add a WHERE clause to the DELETE for a single column.
         * @param col The name of the column to filter.
         * @param values The list of values to match the column.
         * @return The updated query object.
         */
        SqlDelete &whereInList(const QString &col, const QVariantList &values);

        /** Add a WHERE clause to the DELETE for a single column.
         * @param col The name of the column to filter.
         * @param expr The SQL expression to match equality to.
         * @return The updated query object.
         */
        SqlDelete &whereInExpr(const QString &col, const QString &expr);

        /** Add a WHERE clause to the DELETE for a single column.
         * @param col The name of the column to filter.
         * @param op The comparison operator for the column.
         * The column is on the left of the operator and the value on right.
         * @param value The value which must be in the column.
         * @return The updated query object.
         */
        SqlDelete &whereCompare(const QString &col, const QString &op, const QVariant &value);

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
        /// The table to delete rows from
        QString _table;
        /// Fully formed table expression
        QString _tableExpr;
        /// List of WHERE clauses to be ANDed together, with unbound values
        /// The clauses should be parenthesized to avoid error
        QStringList _whereExprs;
};

#endif /* SQL_DELETE_H */
