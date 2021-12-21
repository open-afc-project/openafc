// Copyright (C) 2017 RKF Engineering Solutions, LLC
#include "SqlTransaction.h"
#include "SqlError.h"
#include "SqlHelpers.h"
#include "rkflogging/Logging.h"
#include <QSqlDatabase>

namespace {
/// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "SqlTransaction")
}

SqlTransaction::SqlTransaction(QSqlDatabase &db)
  : _db(&db){
    const bool success = _db->transaction();
    if(SqlHelpers::doDebug()){
        LOGGER_DEBUG(logger) << "Started on " << _db->connectionName() << " success=" << success;
    }
    if(!success){
        throw SqlError("Failed to start transaction", _db->lastError());
    }
}

SqlTransaction::~SqlTransaction(){
    if(!_db){
        return;
    }
    if (!_db->isOpen()) {
        return;
    }
    const bool success = _db->rollback();
    if(SqlHelpers::doDebug()){
        LOGGER_DEBUG(logger) << "Rollback on " << _db->connectionName() << " success=" << success;
    }
    // no chance to raise exception during destructor
}

void SqlTransaction::commit(){
    if(!_db){
        return;
    }
    const bool success = _db->commit();
    if(SqlHelpers::doDebug()){
        LOGGER_DEBUG(logger) << "Commit on " << _db->connectionName() << " success=" << success;
    }
    if(!success){
        throw SqlError("Failed to commit transaction", _db->lastError());
    }
    _db = nullptr;
}
