//  

#include <QSqlQuery>
#include <QVariant>
#include "SqlSelect.h"
#include "SqlTable.h"
#include "SqlError.h"
#include "SqlHelpers.h"
#include <rkflogging/Logging.h>

namespace 
{
    // Logger for all instances of class
    LOGGER_DEFINE_GLOBAL(logger, "SqlSelect")

} // end namespace

SqlSelect::SqlSelect()
  : _rowLimit(-1){}

SqlSelect::SqlSelect(const QSqlDatabase &db, const QString &table)
  : _db(db), _table(table), _rowLimit(-1){}

SqlSelect::SqlSelect(const QSqlDatabase &db, const SqlTable &table)
  : _db(db), _table(table.expression()), _rowLimit(-1){}

SqlSelect & SqlSelect::col(const QString &name){
    return cols(QStringList(name));
}

SqlSelect & SqlSelect::group(const QStringList &cols){
    return group(cols.join(","));
}

SqlSelect & SqlSelect::whereNull(const QString &col){
    return where(QString("(%1 IS NULL)").arg(col));
}

SqlSelect & SqlSelect::whereEqualPlaceholder(const QString &col){
    return where(QString("(%1 = ?)").arg(col));
}

SqlSelect & SqlSelect::whereNonZero(const QString &col){
    return where(QString("(%1 <> 0)").arg(col));
}

SqlSelect & SqlSelect::whereEqual(const QString &col, const QVariant &value){
    const QString valEnc = SqlHelpers::quoted(_db.driver(), value);
    const QString op = (value.isNull() ? "IS" : "=");
    return where(QString("(%1 %2 %3)").arg(col, op, valEnc));
}

SqlSelect & SqlSelect::whereCompare(const QString &col, const QString &op, const QVariant &value){
    const QString valEnc = SqlHelpers::quoted(_db.driver(), value);
    return where(QString("(%1 %2 %3)").arg(col, op, valEnc));
}

SqlSelect & SqlSelect::whereComparePlaceholder(const QString &col, const QString &op){
    return where(QString("(%1 %2 ?)").arg(col, op));
}

SqlSelect & SqlSelect::whereInExpr(const QString &col, const QString &expr){
    _whereExprs << QString("(%1 IN (%2))").arg(col).arg(expr);
    return *this;
}

SqlSelect & SqlSelect::whereInList(const QString &col, const QVariantList &values){
    const QSqlDriver *drv = _db.driver();
    QStringList parts;
    foreach(const QVariant &val, values){
        parts << SqlHelpers::quoted(drv, val);
    }
    return whereInExpr(col, parts.join(","));
}

SqlSelect & SqlSelect::whereBetween(const QString &col, const QVariant &minInclusive, const QVariant &maxInclusive){
    const QSqlDriver *drv = _db.driver();
    _whereExprs << QString("((%1 >= %2) AND (%1 <= %3))").arg(col)
        .arg(SqlHelpers::quoted(drv, minInclusive))
        .arg(SqlHelpers::quoted(drv, maxInclusive));
    return *this;
}

SqlSelect & SqlSelect::join(const QString &other, const QString &on, const QString &type){
    Join join;
    join.whatClause = other;
    join.onClause = on;
    join.typeClause = type;
    _joins.append(join);
    return *this;
}

SqlSelect & SqlSelect::topmost(int count){
    _rowLimit = count;
    return *this;
}

QString SqlSelect::query() const{
    QString queryStr = "SELECT ";

    if(!_prefix.isEmpty()){
        queryStr += _prefix + " ";
    }

    queryStr += _selCols.join(QChar(',')) + " FROM " + _table;

    // Optional index force
    if(!_index.isEmpty()){
        queryStr += " USE INDEX (" + _index + ")";
    }

    // Optional JOINs list
    foreach(const Join &join, _joins){
        queryStr += QString(" %1 JOIN %2 ON (%3)")
            .arg(join.typeClause, join.whatClause, join.onClause);
    }

    // Add list of WHERE clauses
    if(!_whereExprs.isEmpty()){
        queryStr += " WHERE " + _whereExprs.join(" AND ");
    }
    // Add optional grouping
    if(!_groupCols.isEmpty()){
        queryStr += " GROUP BY " + _groupCols;
    }
    // Add final filter
    if(!_havingExpr.isEmpty()){
        queryStr += " HAVING " + _havingExpr;
    }
    // Add optional sorting
    if(!_orderCols.isEmpty()){
        queryStr += " ORDER BY " + _orderCols;
    }
    // Add row restriction
    if(_rowLimit >= 0){
        queryStr += QString(" LIMIT %1").arg(_rowLimit);
    }

    return queryStr;
}

QSqlQuery SqlSelect::run() const{
    const QString queryStr = query();
    LOGGER_DEBUG(logger) << "Executing select query: " << queryStr;
    return SqlHelpers::exec(_db, queryStr);
}
