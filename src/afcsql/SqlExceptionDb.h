// 

#ifndef SQL_EXCEPTION_DB_H
#define SQL_EXCEPTION_DB_H

#include <QSqlDatabase>
#include <QSqlDriver>

/** Extend the basic database class to provide overloads which raise exceptions
 * upon common errors.
 */
class SqlExceptionDb : public QSqlDatabase{
public:
    /// Convenience definition
    typedef QList<QSqlDriver::DriverFeature> FeatureList;

    /// Default constructor with invalid state
    SqlExceptionDb(){}

    /** Construct from an existing database connection.
     *
     * @param db The existing DB.
     * @throw SqlError With any issues in the source database.
     */
    SqlExceptionDb(const QSqlDatabase &db){
        operator=(db);
    }

    /** Copy constructor.
     * Overload to also copy required features.
     *
     * @param db The existing DB.
     */
    SqlExceptionDb(const SqlExceptionDb &db)
      : QSqlDatabase(db){
        _feats = db._feats;
    }

    /** Wrap an existing database connection.
     *
     * @param db The existing DB.
     * @throw SqlError With any issues in the source database.
     */
    SqlExceptionDb & operator = (const QSqlDatabase &db);

    /** Set the features which are required of this database.
     *
     * This should be done before ensureDriverValid().
     * @param features
     */
    void setRequiredFeatures(const FeatureList &features);

    /** Ensure that this DB connection has all required features.
     *
     * Features are required by setRequiredFeatures().
     * @pre The DB connection is open.
     * @throw SqlError If any required features are not present.
     */
    void ensureDriverValid() const;

    /** Ensure that the connection is open.
     * @throw SqlError If the connection is not open.
     */
    void ensureOpen() const;

    /** Attempt to force the connection to be open.
     * If the connection is already valid then this is no-op.
     *
     * @note This should be called if an existing connection has been kept
     * open but idle for a long time (hours) to ensure that the server
     * has not reset the connection during the idle time.
     *
     * @throw SqlError If there is a problem connecting.
     * @post The connection will be open if no exception is raised.
     */
    void tryOpen();

    /** Overload to throw exception.
     * @throw SqlError if the operation fails.
     */
    void transaction();

    /** Overload to throw exception.
     * @throw SqlError if the operation fails.
     */
    void commit();

    /** Overload to throw exception.
     * @throw SqlError if the operation fails.
     */
    void rollback();

private:
    /// Required features
    FeatureList _feats;
};

#endif /* SQL_EXCEPTION_DB_H */
