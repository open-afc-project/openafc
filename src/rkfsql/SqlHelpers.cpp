//  

#define SQL_DEBUG

#include "SqlHelpers.h"
#include "SqlError.h"
#include "rkflogging/Logging.h"
#include <QSqlDriver>
#include <QSqlField>
#include <QDateTime>
#include <QMutex>
#include <QElapsedTimer>

LOG_STREAM_WRAP_QT(QSqlError)

#define FEATURE(Name) \
    case QSqlDriver::Name: \
        return #Name;

namespace{
    /** Name a particular feature enumeration.
     *
     * @param id The enum value.
     * @return The name of the value.
     */
    QString featureName(QSqlDriver::DriverFeature id){
        switch(id){
        FEATURE(Transactions)
        FEATURE(QuerySize)
        FEATURE(BLOB)
        FEATURE(Unicode)
        FEATURE(PreparedQueries)
        FEATURE(NamedPlaceholders)
        FEATURE(PositionalPlaceholders)
        FEATURE(LastInsertId)
        FEATURE(BatchOperations)
        FEATURE(SimpleLocking)
        FEATURE(LowPrecisionNumbers)
        FEATURE(EventNotifications)
        FEATURE(FinishQuery)
        FEATURE(MultipleResultSets)
        FEATURE(CancelQuery)
        default:
            return "other";
        }
    };
    
    /** Check for the special error case of QTBUG-223.
     * Documented at https://bugreports.qt-project.org/browse/QTBUG-223.
     * @post Close the connection if MySQL client reports "server gone away"
     */
    void checkMysql(const QSqlDatabase &db, const QSqlError &err){
        if((err.number() == 2006) && (db.driverName() == "QMYSQL")){
            // Not really a copy, since both use same connection
            QSqlDatabase dbRef(db);
            dbRef.close();
        }
    }

    /// Expand a QDateTime as full-resolution form (fixed time zone)
    const QString fullDtSpec("yyyy-MM-ddTHH:mm:ss.zzzZ");

    /// Logger for all instances of class
    LOGGER_DEFINE_GLOBAL(logger, "SqlHelpers")
}

EnvironmentFlag SqlHelpers::doDebug("CPO_SQL_DEBUG");

void SqlHelpers::ensureFeature(const QSqlDriver &drv, QSqlDriver::DriverFeature feature){
    if(!drv.hasFeature(feature)){
        throw SqlError(QString("SQL driver is missing %1").arg(featureName(feature)));
    }
}

QVariant SqlHelpers::encode(const QVariant &value){
    switch(value.type()){
    case QVariant::DateTime:{
        const QDateTime valDt = value.toDateTime();
        if (valDt.isNull()) {
            return QVariant();
        }
        if (valDt.timeSpec() != Qt::UTC) {
            throw SqlError("All DateTime values must be in UTC");
        }
        return valDt.toString(fullDtSpec);
    }

    default:
        return value;
    }
}

QString SqlHelpers::quoted(const QSqlDriver *driver, const QVariant &value){
    const QVariant val = SqlHelpers::encode(value);
    QSqlField field(QString(), val.type());
    field.setValue(val);
    return driver->formatValue(field);
}

QStringList SqlHelpers::prefixCols(const QString &prefix, const QStringList &cols){
    QStringList result;
    result.reserve(cols.count());
    foreach(const QString &col, cols){
        result.append(QString("%1.%2").arg(prefix, col));
    }
    return result;
}

QSqlQuery SqlHelpers::prepare(const QSqlDatabase &db, const QString &query){
    if(doDebug()){
        LOGGER_DEBUG(logger) << "prepare " << query;
    }

    QSqlQuery qObj(db);
    if(!qObj.prepare(query)){
        const QSqlError sqlErr = qObj.lastError();
        checkMysql(db, sqlErr);
        throw SqlError(QString("Failed prepare for \"%1\"").arg(query), sqlErr);
    }
    return qObj;
}

void SqlHelpers::execPrepared(const QSqlDatabase &db, QSqlQuery &qObj){
    std::unique_ptr<QElapsedTimer> timer;
    if(doDebug()){
        LOGGER_DEBUG(logger)
            << "execPrepared RUN " << qObj.lastQuery()
            << " WITH (" << boundList(qObj).join(", ") << ")";
        timer.reset(new QElapsedTimer);
        timer->start();
    }

    const bool success = qObj.exec();
    if(doDebug()){
        const qint64 elapsed = timer->elapsed();
        LOGGER_DEBUG(logger)
            << "execPrepared TIME " << elapsed
            << " SIZE " << qObj.size() << " ERR " << qObj.lastError();
    }
    if(!success){
        const QSqlError sqlErr = qObj.lastError();
        checkMysql(db, sqlErr);
        throw SqlError(
            QString("Failed exec for \"%1\" with values (%2)")
                .arg(qObj.lastQuery())
                .arg(boundList(qObj).join(",")),
            sqlErr
        );
    }
}

QStringList SqlHelpers::boundList(const QSqlQuery &query){
    // work-around for QTBUG-12186
    const int boundCount = query.boundValues().count();
    QStringList boundStrs;
    for(int valI = 0; valI < boundCount; ++valI){
        const QVariant &var = query.boundValue(valI);
        QString val;
        switch(var.type()){
        case QVariant::Invalid:
            val = "NULL";
            break;
        case QVariant::ByteArray:
            val = "h'" + var.toByteArray().toHex() + "'";
            break;
        case QVariant::DateTime:
            val = var.toDateTime().toString(fullDtSpec);
            break;
        case QVariant::String:
            val = "'" + var.toString() + "'";
            break;
        default:
            val = var.toString();
            break;
        }
        boundStrs << val;
    }
    return boundStrs;
}

QSqlQuery SqlHelpers::exec(const QSqlDatabase &db, const QString &query){
    std::unique_ptr<QElapsedTimer> timer;
    if(doDebug()){
        LOGGER_DEBUG(logger) << "exec RUN " << query;
        timer.reset(new QElapsedTimer);
        timer->start();
    }
    QSqlQuery qObj(db);

    // INSERT queries on ODBC may have qObj.isActive() false but have no error
    const bool success = qObj.exec(query);
    if(doDebug()){
        const qint64 elapsed = timer->elapsed();
        LOGGER_DEBUG(logger) << "exec TIME " << elapsed << " SIZE " << qObj.size() << " ERR " << qObj.lastError();
    }
    if(!success){
        const QSqlError sqlErr = qObj.lastError();
        checkMysql(db, sqlErr);
        throw SqlError(QString("Failed exec for \"%1\"").arg(query), sqlErr);
    }
    return qObj;
}
