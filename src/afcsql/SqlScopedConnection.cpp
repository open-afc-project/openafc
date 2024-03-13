// 

#include "SqlScopedConnection.h"

void SqlScopedConnectionCloser::cleanup(QSqlDatabase *pointer){
    if(!pointer){
        return;
    }
    const QString cName = pointer->connectionName();
    delete pointer;
    QSqlDatabase::removeDatabase(cName);
}
