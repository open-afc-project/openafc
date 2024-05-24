//

#ifndef SQL_SELECT_H
#define SQL_SELECT_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStringList>

class SqlTable;

/** An interface specifically for the particular needs of the SELECT query.
 */
class SqlSelect
{
	public:
		/** Create a invalid object which can be assigned-to later.
		 */
		SqlSelect();

		/** Create a new SELECT query on a given database and table.
		 * @param db The database to query.
		 * @param table The name of the table to query.
		 */
		SqlSelect(const QSqlDatabase &db, const QString &table);

		/** Create a new SELECT query on a given database and table.
		 * @param db The database to query.
		 * @param table The definition of the table to query.
		 */
		SqlSelect(const QSqlDatabase &db, const SqlTable &table);

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
			return _selCols.count();
		}

		/** Set the columns used for the result of the SELECT.
		 * @param name A single column to fetch.
		 * @return The updated query object.
		 */
		SqlSelect &col(const QString &name);

		/** Set the columns used for the result of the SELECT.
		 * @param cols A list of column names.
		 * @return The updated query object.
		 */
		SqlSelect &cols(const QStringList &colsVal)
		{
			_selCols = colsVal;
			return *this;
		}

		/** Set query prefix options (such as DISTINCT).
		 *
		 * @param options The options expression to prefix.
		 * @return The updated query object.
		 */
		SqlSelect &prefix(const QString &options)
		{
			_prefix = options;
			return *this;
		}

		/** Set the columns used to group the result of the SELECT.
		 * @param cols A comma separated list of column names.
		 * @return The updated query object.
		 */
		SqlSelect &group(const QString &colsVal)
		{
			_groupCols = colsVal;
			return *this;
		}
		/** Overload to combine multiple column names.
		 *
		 * @param cols Individual column names to group by in-order.
		 * @return The updated query object.
		 * @sa group(QString)
		 */
		SqlSelect &group(const QStringList &cols);

		/** Set the columns used to sort the result of the SELECT.
		 * @param cols A comma separated list of column names.
		 * @return The updated query object.
		 */
		SqlSelect &having(const QString &text)
		{
			_havingExpr = text;
			return *this;
		}

		/** Set the columns used to sort the result of the SELECT.
		 * @param cols A comma separated list of column names.
		 * @return The updated query object.
		 */
		SqlSelect &order(const QString &colsVal)
		{
			_orderCols = colsVal;
			return *this;
		}

		/** Add an arbitrary WHERE clause to the SELECT.
		 * Each call to this function adds a new clause which will be joined with
		 * the "AND" operator.
		 *
		 * @param expr The WHERE expression.
		 * @return The updated query object.
		 */
		SqlSelect &where(const QString &expr)
		{
			_whereExprs << expr;
			return *this;
		}

		/** Add a WHERE clause to the SELECT for a single column.
		 * @param col The name of the column to filter to include @c null values.
		 * @return The updated query object.
		 */
		SqlSelect &whereNull(const QString &col);

		/** Add a WHERE clause to the SELECT for a single column.
		 * This is intended to be used in prepared queries, where particular
		 * values are bound later.
		 * @param col The name of the column to filter.
		 * @return The updated query object.
		 * @sa PreparedQuery
		 */
		SqlSelect &whereEqualPlaceholder(const QString &col);

		/** Add a WHERE clause to the SELECT for a single column.
		 * @param col The name of the column to filter for non-zero values.
		 * @return The updated query object.
		 */
		SqlSelect &whereNonZero(const QString &col);

		/** Add a WHERE clause to the SELECT for a single column.
		 * @param col The name of the column to filter.
		 * @param value The value which must be in the column.
		 * @return The updated query object.
		 */
		SqlSelect &whereEqual(const QString &col, const QVariant &value);

		/** Add a WHERE clause to the SELECT for a single column.
		 * @param col The name of the column to filter.
		 * @param op The comparison operator for the column.
		 * The column is on the left of the operator and the value on right.
		 * @param value The value which must be in the column.
		 * @return The updated query object.
		 */
		SqlSelect &whereCompare(const QString &col, const QString &op, const QVariant &value);

		/** Add a WHERE clause to the SELECT for a single column.
		 * @param col The name of the column to filter.
		 * @param op The comparison operator for the column.
		 * The column is on the left of the operator and the placeholder on right.
		 * @return The updated query object.
		 */
		SqlSelect &whereComparePlaceholder(const QString &col, const QString &op);

		/** Add a WHERE clause to the SELECT for a single column.
		 * @param col The name of the column to filter.
		 * @param expr An SQL expression for ppossible values which must be in the column.
		 * @return The updated query object.
		 */
		SqlSelect &whereInExpr(const QString &col, const QString &expr);

		/** Add a WHERE clause to the SELECT for a single column.
		 * @param col The name of the column to filter.
		 * @param values A list of possible values which must be in the column.
		 * @return The updated query object.
		 */
		SqlSelect &whereInList(const QString &col, const QVariantList &values);

		/** Add a WHERE clause to the SELECT for a single column.
		 * @param col The name of the column to filter.
		 * @param values A list of possible values which must be in the column.
		 * @return The updated query object.
		 */
		template<class Container>
		SqlSelect &whereIn(const QString &colVal, const Container &values)
		{
			QVariantList parts;
			foreach(const typename Container::value_type &val, values)
			{
				parts << val;
			}
			return whereInList(colVal, parts);
		}

		/** Add a WHERE clause to the SELECT for a single column.
		 * @param col The name of the column to filter.
		 * @param minInclusive The minimum possible value allowed by the filter.
		 * @param maxInclusive The maximum possible value allowed by the filter.
		 * @return The updated query object.
		 */
		SqlSelect &whereBetween(
			const QString &col, const QVariant &minInclusive, const QVariant &maxInclusive);

		/** Join the current table with another.
		 * Multiple joins may be added in-sequence.
		 *
		 * @param other The other expression to join with.
		 * @param on The expression to join on.
		 * @param type The type of join to perform (INNER, LEFT, RIGHT, etc.)
		 * @return The updated query object.
		 */
		SqlSelect &join(const QString &other, const QString &on, const QString &type);

		/** Shortcut to perform a LEFT JOIN.
		 *
		 * @param other The other expression to join with.
		 * @param on The expression to join on.
		 * @return The updated query object.
		 */
		SqlSelect &leftJoin(const QString &other, const QString &on)
		{
			return join(other, on, "LEFT");
		}

		/** Shortcut to perform a INNER JOIN.
		 *
		 * @param other The other expression to join with.
		 * @param on The expression to join on.
		 * @return The updated query object.
		 */
		SqlSelect &innerJoin(const QString &other, const QString &on)
		{
			return join(other, on, "INNER");
		}

		/** Force a specific index to be used for selection.
		 * @param indexName The name of the underlying index.
		 * @return The updated query object.
		 */
		SqlSelect &index(const QString &indexName)
		{
			_index = indexName;
			return *this;
		}

		/** Add a row-limiting clause to the query.
		 *
		 * @param count The maximum number of rows to retrieve.
		 * @return The updated query object.
		 */
		SqlSelect &topmost(int count = 1);

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

		// protected:
		/// Each JOIN in this select
		struct Join {
				/// Named type of join
				QString typeClause;
				/// Join right-hand side clause
				QString whatClause;
				/// Join-on clause
				QString onClause;
		};

		/// Underlying database
		QSqlDatabase _db;
		/// Fully quoted name of the table
		QString _table;
		/// Joins defined
		QList<Join> _joins;
		/// Prefix options
		QString _prefix;
		/// List of quoted column names to retrieve
		QStringList _selCols;
		/// Comma-separated list of quoted column names for a GROUP BY clause
		QString _groupCols;
		/// Single expression used for HAVING clause
		QString _havingExpr;
		/// Comma-separated list of quoted column names for an ORDER BY by
		QString _orderCols;
		/// List of WHERE clauses to be ANDed together, with unbound values
		/// The clauses should be parenthesized to avoid error
		QStringList _whereExprs;
		/// Optional comma-separated list of indices to use
		QString _index;
		/// Optional row limit
		int _rowLimit;
};

#endif /* SQL_SELECT_H */
