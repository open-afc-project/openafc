// 

#include <QDebug>
#include "SqlExceptionDb.h"
#include "SqlError.h"
#include "SqlHelpers.h"

using namespace SqlHelpers;

void SqlExceptionDb::setRequiredFeatures(const FeatureList &features){
    _feats = features;
}

void SqlExceptionDb::ensureDriverValid() const{
    if(!isValid()){
        throw SqlError("ensureDriverValid invalid connection", lastError());
    }
    const QSqlDriver *const drv = driver();
    foreach(QSqlDriver::DriverFeature feature, _feats){
        ensureFeature(*drv, feature);
    }
}

SqlExceptionDb & SqlExceptionDb::operator = (const QSqlDatabase &db){
    if(!db.isValid()){
        throw SqlError("Bad SQL driver", db.lastError());
    }
    QSqlDatabase::operator =(db);
    return *this;
}

void SqlExceptionDb::ensureOpen() const{
    if(!isOpen()){
        throw SqlError("Database connection not open");
    }
}

void SqlExceptionDb::tryOpen(){
    // Verify with active query first
    if(isOpen()){
        QSqlQuery test = exec("SELECT 1");
        if(!test.isActive()){
            const QString name = QString("%1://%3@%2/%4").arg(driverName(), hostName(), userName(), databaseName());
            qWarning().nospace()
                << "SqlExceptionDb closing supposedly open connection to "
                << name;
            close();
        }
    }
    if(!isOpen()){
        if(!open()){
            throw SqlError("Database connection failed", lastError());
        }
    }
    ensureDriverValid();
}

void SqlExceptionDb::transaction(){
    if(!QSqlDatabase::transaction()){
        throw SqlError("Failed to start transaction", lastError());
    }
}

void SqlExceptionDb::commit(){
    if(!QSqlDatabase::commit()){
        throw SqlError("Failed to commit transaction", lastError());
    }
}

void SqlExceptionDb::rollback(){
    if(!QSqlDatabase::rollback()){
        throw SqlError("Failed to roll-back transaction", lastError());
    }
}
