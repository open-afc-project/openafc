//  

#include "SqlTable.h"

SqlTable::SqlTable(const QString &tableExpr){
    Join join;
    join.whatClause = tableExpr;
    _joins.append(join);
}

SqlTable & SqlTable::join(const QString &tableExpr, const QString &on, const QString &type){
    Join join;
    join.whatClause = tableExpr;
    join.onClause = on;
    join.typeClause = type;
    _joins.append(join);
    return *this;
}

QString SqlTable::expression() const{
    QList<Join>::const_iterator it = _joins.begin();
    if(it == _joins.end()){
        return QString();
    }

    QString expr;
    expr += it->whatClause;
    ++it;

    for(; it != _joins.end(); ++it){
        expr += QString(" %1 JOIN %2 ON (%3)")
            .arg(it->typeClause, it->whatClause, it->onClause);
    }

    return expr;
}
