// 

#include <QVariantList>
#include "SqlDelete.h"
#include "SqlHelpers.h"

SqlDelete::SqlDelete(const QSqlDatabase &db, const QString &tableName)
  : _db(db), _table(tableName), _tableExpr(tableName){}

SqlDelete::SqlDelete(const QSqlDatabase &db, const QString &tableName, const SqlTable &data)
  : _db(db),_table(tableName), _tableExpr(data.expression()){}

SqlDelete & SqlDelete::whereNull(const QString &col){
    return where(QString("(%1 IS NULL)").arg(col));
}

SqlDelete & SqlDelete::whereEqualPlaceholder(const QString &col){
    return where(QString("(%1 = ?)").arg(col));
}

SqlDelete & SqlDelete::whereEqual(const QString &col, const QVariant &value){
    const QString valEnc = SqlHelpers::quoted(_db.driver(), value);
    const QString op = (value.isNull() ? "IS" : "=");
    return where(QString("(%1 %2 %3)").arg(col, op, valEnc));
}

SqlDelete & SqlDelete::whereInList(const QString &col, const QVariantList &values){
    const QSqlDriver *const drv = _db.driver();
    QStringList listEnc;
    foreach(const QVariant &val, values){
        listEnc.append(SqlHelpers::quoted(drv, val));
    }

    return where(QString("(%1 IN (%2))").arg(col, listEnc.join(",")));
}

SqlDelete & SqlDelete::whereInExpr(const QString &col, const QString &expr){
    return where(QString("(%1 IN (%2))").arg(col, expr));
}

SqlDelete & SqlDelete::whereCompare(const QString &col, const QString &op, const QVariant &value){
    _whereExprs << QString("(%1 %2 %3)").arg(col).arg(op)
        .arg(SqlHelpers::quoted(_db.driver(), value));
    return *this;
}

QString SqlDelete::query() const{
    QString queryStr = "DELETE";
    switch(_db.driver()->dbmsType()){
    case QSqlDriver::MySqlServer:
        queryStr += " " + _table;
        break;
    default:
        break;
    }

    queryStr += " FROM " + _tableExpr;

    // Add list of WHERE clauses
    if(!_whereExprs.isEmpty()){
        queryStr += " WHERE " + _whereExprs.join(" AND ");
    }

    return queryStr;
}

QSqlQuery SqlDelete::run() const{
    return SqlHelpers::exec(_db, query());
}
