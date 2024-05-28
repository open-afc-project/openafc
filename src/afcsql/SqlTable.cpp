//

#include "SqlTable.h"

SqlTable::SqlTable(const QString &tableExpr)
{
    Join joinVal;
    joinVal.whatClause = tableExpr;
    _joins.append(joinVal);
}

SqlTable &SqlTable::join(const QString &tableExpr, const QString &on, const QString &type)
{
    Join joinVal;
    joinVal.whatClause = tableExpr;
    joinVal.onClause = on;
    joinVal.typeClause = type;
    _joins.append(joinVal);
    return *this;
}

QString SqlTable::expression() const
{
    QList<Join>::const_iterator it = _joins.begin();
    if (it == _joins.end()) {
        return QString();
    }

    QString expr;
    expr += it->whatClause;
    ++it;

    for (; it != _joins.end(); ++it) {
        expr += QString(" %1 JOIN %2 ON (%3)").arg(it->typeClause, it->whatClause, it->onClause);
    }

    return expr;
}
