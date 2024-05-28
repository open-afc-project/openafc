//

#ifndef SQL_TABLE_H
#define SQL_TABLE_H

#include <QString>
#include <QList>

/** Define an SQL expression for a simple table or a combined JOIN of tables.
 */
class SqlTable
{
    public:
        /// Initialize to an invalid definition
        SqlTable()
        {
        }

        /** Initialize to a single existing table/view name.
         *
         * @param tableExpr The fully-quoted table expression.
         */
        SqlTable(const QString &tableExpr);

        /** Join the current table with another.
         * Multiple joins may be added in-sequence.
         *
         * @param tableExpr The other fully-quoted expression to join with.
         * @param on The expression to join on.
         * @param type The type of join to perform (INNER, LEFT, RIGHT, etc.)
         * @return The updated query object.
         */
        SqlTable &join(const QString &tableExpr, const QString &on, const QString &type);

        /** Shortcut to perform a LEFT JOIN.
         *
         * @param other The other expression to join with.
         * @param on The expression to join on.
         * @return The updated query object.
         */
        SqlTable &leftJoin(const QString &other, const QString &on)
        {
            return join(other, on, "LEFT");
        }

        /** Shortcut to perform a RIGHT JOIN.
         *
         * @param other The other expression to join with.
         * @param on The expression to join on.
         * @return The updated query object.
         */
        SqlTable &rightJoin(const QString &other, const QString &on)
        {
            return join(other, on, "RIGHT");
        }

        /** Shortcut to perform a INNER JOIN.
         *
         * @param other The other expression to join with.
         * @param on The expression to join on.
         * @return The updated query object.
         */
        SqlTable &innerJoin(const QString &other, const QString &on)
        {
            return join(other, on, "INNER");
        }

        /** Get the combined expresison for this table(set).
         *
         * @return The SQL expression suitable for SELECT or UPDATE queries.
         */
        QString expression() const;

    private:
        /// Each part of a multi-table JOIN
        struct Join {
                /// Named type of join
                QString typeClause;
                /// Table name to join on
                QString whatClause;
                /// Join-on clause
                QString onClause;
        };

        /// List of joined tables.
        /// The first item in the list only uses the #whatClause value.
        QList<Join> _joins;
};

#endif /* SQL_TABLE_H */
