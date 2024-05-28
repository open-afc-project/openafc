//

#include <QSet>
#include <QSqlField>
#include <QSqlDriver>
#include "SqlInsert.h"
#include "SqlError.h"
#include "SqlHelpers.h"
#include "SqlPreparedQuery.h"

namespace
{
const QChar comma(',');
}

SqlInsert &SqlInsert::cols(const QStringList &colsVal)
{
    if (colsVal.toSet().count() != colsVal.count()) {
        throw SqlError("Duplicate column name");
    }

    _cols = colsVal;
    return *this;
}

QString SqlInsert::query(const QString &expr) const
{
    const QString colPart = _cols.join(comma);

    return QString("INSERT INTO %1 (%2) %3").arg(_table).arg(colPart).arg(expr);
}

QString SqlInsert::prepared() const
{
    const QString valPart = SqlPreparedQuery::qMark(_cols.count());
    return query(QString("VALUES (%1)").arg(valPart));
}

QSqlQuery SqlInsert::run(const QVariantList &values)
{
    const QSqlDriver *const drv = _db.driver();
    QStringList valStrs;
    foreach(const QVariant &val, values)
    {
        valStrs.append(SqlHelpers::quoted(drv, val));
    }

    return run(QString("VALUES (%1)").arg(valStrs.join(comma)));
}

QSqlQuery SqlInsert::run(const QString &expr)
{
    return SqlHelpers::exec(_db, query(expr));
}
