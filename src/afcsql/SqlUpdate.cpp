//

#include "SqlUpdate.h"
#include "SqlHelpers.h"
#include "SqlTable.h"
#include <QVariant>

SqlUpdate::SqlUpdate(const QSqlDatabase &db, const QString &tableExpr) :
	_db(db), _tableExpr(tableExpr)
{
}

SqlUpdate::SqlUpdate(const QSqlDatabase &db, const SqlTable &table) : _db(db)
{
	_tableExpr = table.expression();
}

SqlUpdate &SqlUpdate::setPlaceholder(const QString &col)
{
	return set(QString("%1=?").arg(col));
}

SqlUpdate &SqlUpdate::setValue(const QString &col, const QVariant &value)
{
	return set(QString("%1=%2").arg(col).arg(SqlHelpers::quoted(_db.driver(), value)));
}

SqlUpdate &SqlUpdate::whereEqual(const QString &col, const QVariant &value)
{
	const QString valEnc = SqlHelpers::quoted(_db.driver(), value);
	const QString op = (value.isNull() ? "IS" : "=");
	return where(QString("(%1 %2 %3)").arg(col, op, valEnc));
}

SqlUpdate &SqlUpdate::whereNull(const QString &col)
{
	return where(QString("(%1 IS NULL)").arg(col));
}

SqlUpdate &SqlUpdate::whereEqualPlaceholder(const QString &col)
{
	return where(QString("(%1 = ?)").arg(col));
}

QString SqlUpdate::query() const
{
	QString queryStr = "UPDATE ";
	queryStr += _tableExpr;

	// Add list of SET parts
	queryStr += " SET " + _setExprs.join(", ");

	// Add list of WHERE clauses
	if (!_whereExprs.isEmpty()) {
		queryStr += " WHERE " + _whereExprs.join(" AND ");
	}

	return queryStr;
}

QSqlQuery SqlUpdate::run() const
{
	const QString queryStr = query();
	return SqlHelpers::exec(_db, queryStr);
}
