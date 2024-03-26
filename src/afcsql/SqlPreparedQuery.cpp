//

#include <QDebug>
#include <QStringList>
#include <QVariantList>
#include <QSqlRecord>
#include "SqlSelect.h"
#include "SqlInsert.h"
#include "SqlUpdate.h"
#include "SqlDelete.h"
#include "SqlHelpers.h"
#include "SqlPreparedQuery.h"

QString SqlPreparedQuery::qMark(int number)
{
	QStringList parts;
	for (int ix = 0; ix < number; ++ix) {
		parts += "?";
	}
	return parts.join(",");
}

SqlPreparedQuery::SqlPreparedQuery(const SqlSelect &query)
{
	_db = query.database();
	QSqlQuery::operator=(SqlHelpers::prepare(_db, query.query()));
}

SqlPreparedQuery::SqlPreparedQuery(const SqlInsert &query)
{
	_db = query.database();
	QSqlQuery::operator=(SqlHelpers::prepare(_db, query.prepared()));
}

SqlPreparedQuery::SqlPreparedQuery(const SqlUpdate &query)
{
	_db = query.database();
	QSqlQuery::operator=(SqlHelpers::prepare(_db, query.query()));
}

SqlPreparedQuery::SqlPreparedQuery(const SqlDelete &query)
{
	_db = query.database();
	QSqlQuery::operator=(SqlHelpers::prepare(_db, query.query()));
}

SqlPreparedQuery::SqlPreparedQuery(const QSqlDatabase &db, const QString &query) : _db(db)
{
	QSqlQuery::operator=(SqlHelpers::prepare(_db, query));
}

SqlPreparedQuery &SqlPreparedQuery::bind(const QVariant &param)
{
	addBindValue(param);
	return *this;
}

SqlPreparedQuery &SqlPreparedQuery::bindList(const QVariantList &params)
{
	for (int ix = 0; ix < params.count(); ++ix) {
		bindValue(ix, params.at(ix));
	}
	return *this;
}

QSqlQuery &SqlPreparedQuery::run()
{
	SqlHelpers::execPrepared(_db, *this);
	return *this;
}
