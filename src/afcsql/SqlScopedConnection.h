//

#ifndef CPOBG_SRC_CPOSQL_SQLSCOPEDCONNECTION_H_
#define CPOBG_SRC_CPOSQL_SQLSCOPEDCONNECTION_H_

//#include "cposql_export.h"
#include <QScopedPointer>
#include <QSqlDatabase>

/** Deleter for a database object.
 * Uses QSqlDatabase::removeDatabase() for DB connection cleanup.
 */
class /*CPOSQL_EXPORT*/ SqlScopedConnectionCloser
{
	public:
		/// Interface for QScopedPointerDeleter
		static void cleanup(QSqlDatabase *pointer);
};

/** A scoped pointer which closes a DB connection when it deletes the
 * DB object.
 * @tparam DbName The specific class to scope.
 * It must be either QSqlDatabase or a derived class.
 */
template<class DbType>
class SqlScopedConnection : public QScopedPointer<DbType, SqlScopedConnectionCloser>
{
		typedef QScopedPointer<DbType, SqlScopedConnectionCloser> Parent;

	public:
		/** Construct a new default instance.
		 * @post The DB instance is ready for connection assignment.
		 */
		SqlScopedConnection()
		{
			Parent::reset(new DbType());
		}

		/** Take ownership of an existing instance.
		 *
		 * @param db The database instance start with.
		 * Ownership is taken from the caller.
		 */
		SqlScopedConnection(DbType *db)
		{
			Parent::reset(db);
		}
};

#endif /* CPOBG_SRC_CPOSQL_SQLSCOPEDCONNECTION_H_ */
