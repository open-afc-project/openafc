//

#ifndef SQL_INSERT_H
#define SQL_INSERT_H

#include <QSqlDatabase>
#include <QStringList>
#include <QVariantList>

/** An interface specifically for the particular needs of the INSERT query.
 */
class SqlInsert
{
    public:
        /** Create a new SELECT query on a given database and table.
         * @param db The database to query.
         * @param table The name of the table to query.
         */
        SqlInsert(const QSqlDatabase &db, const QString &table) : _db(db), _table(table)
        {
        }

        /** Get the underlying database object.
         * @param The database to query.
         */
        const QSqlDatabase &database() const
        {
            return _db;
        }

        /** Get the number of columns currently defined for the query.
         *
         * @return The number of columns.
         */
        int colCount() const
        {
            return _cols.count();
        }

        /** Set the columns used for the result of the SELECT.
         * @param cols A list of column names.
         * @return The updated query object.
         */
        SqlInsert &cols(const QStringList &cols);

        /** Get the SQL query string which would be executed by run() or filled
         * by prepared().
         *
         * @param expr The value expression to insert. This expression must have
         * the same number of values as the insert columns.
         * @return The query string to be executed.
         */
        QString query(const QString &expr) const;

        /** Get the SQL query string with positional placeholders.
         *
         * @param The query string to be executed.
         */
        QString prepared() const;

        /** Build and execute the query and return the QSqlQuery result.
         *
         * @param values The values to insert.
         * @return The successful result of a query execution.
         * @throw SqlError if the query fails to execute.
         */
        QSqlQuery run(const QVariantList &values);

        /** Build and execute the query and return the QSqlQuery result.
         *
         * @param expr The value expression to insert. This expression must have
         * the same number of values as the insert columns.
         * @return The successful result of a query execution.
         * @throw SqlError if the query fails to execute.
         */
        QSqlQuery run(const QString &expr);

    protected:
        /// Underlying database
        QSqlDatabase _db;
        /// Unquoted name of the table
        QString _table;
        /// List of quoted column names to retrieve
        QStringList _cols;
};

#endif /* SQL_INSERT_H */
