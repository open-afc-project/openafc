//
#ifndef CPOBG_SRC_CPOSQL_SQLTRANSACTION_H_
#define CPOBG_SRC_CPOSQL_SQLTRANSACTION_H_

class QSqlDatabase;

/** Define a context for DB transactions.
 * Upon construction of this class a transaction is started, and unless the
 * transaction is committed, the destructor will roll-back the state.
 */
class SqlTransaction
{
	public:
		/** Start the transaction.
		 *
		 * @param db The database to transact within.
		 * @throw SqlError if the transaction fails.
		 * @post The transaction is entered.
		 */
		SqlTransaction(QSqlDatabase &db);

		/** Rollback the transaction unless already committed.
		 * @post The transaction has been rolled-back.
		 */
		~SqlTransaction();

		/** Commit the transaction to avoid rollback.
		 * @throw SqlError if the transaction fails.
		 * @post The transaction has been committed.
		 */
		void commit();

	private:
		/// The database, which is non-null during the transaction.
		QSqlDatabase *_db;
};

#endif /* CPOBG_SRC_CPOSQL_SQLTRANSACTION_H_ */
