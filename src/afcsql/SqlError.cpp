//

#include "SqlError.h"

SqlError::SqlError(const QString &msg) : runtime_error(msg.toStdString())
{
}

// Convert to Latin1 representation to ensure bad characters
SqlError::SqlError(const QString &msg, const QSqlError &err) :
	runtime_error(QString("%1: (%2, \"%3\", \"%4\")")
					  .arg(msg)
					  .arg(err.number())
					  .arg(err.driverText())
					  .arg(err.databaseText())
					  .toStdString()),
	_err(err)
{
}
