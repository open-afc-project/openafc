// Copyright (C) 2017 RKF Engineering Solutions, LLC

#ifndef CPOTESTCOMMON_PGSQLDAEMONRUNNER_H_
#define CPOTESTCOMMON_PGSQLDAEMONRUNNER_H_

#include "ProgRunner.h"
#include <QString>
#include <QDir>
#include <QSqlDatabase>

class QProcess;
class QEventLoop;
class QTimer;
class QRegularExpression;

/** Wrapper to run a local PostgreSQL daemon.
 *
 * Relies on environment variables to override binary run paths:
 *  - POSTGREGSQL_BIN The path to "postgres" server executable
 *  - POSTGREGSQL_INITDB_BIN The path to the "initdb" executable
 */
class PgsqlDaemonRunner : public ProgRunner{
    Q_OBJECT
public:
    /** Initialize the state, but do not run any process.
     *
     * @param root An empty root directory for exclusive use.
     */
    PgsqlDaemonRunner(const QDir &root, QObject *parent = NULL);

    /// Ensure worker has exited
    ~PgsqlDaemonRunner();

    QString listenHost() const{
        return _listenHost;
    }

    quint16 listenPort() const{
        return _listenPort;
    }

    virtual bool start();
    virtual bool stop(int timeoutMs=30000);

    const QSqlDatabase & adminDb() const{
        return _dbconn;
    }

    /** Execute query as the administrative user.
     *
     * @param query The query to execute.
     * @return The resulting object.
     * @throw std::runtime_error if the execution fails.
     */
    QSqlQuery exec(const QString &query);

    bool createUser(const QString &roleName);

    bool createDatabase(const QString &dbName);

    bool clearDatabases();

private:
    /// Temporary directory for test
    QDir _root;
    //QDir _dataDir;
    /// Config file path in #_dir
    QString _confPath;
    QString _listenHost;
    quint16 _listenPort;

    /// Administrative DB connection
    QSqlDatabase _dbconn;
};


#endif /* CPOTESTCOMMON_PGSQLDAEMONRUNNER_H_ */
