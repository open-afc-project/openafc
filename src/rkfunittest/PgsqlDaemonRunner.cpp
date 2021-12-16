// 

#include "PgsqlDaemonRunner.h"
#include "rkflogging/Logging.h"
#include "rkfunittest/UnitTestHelpers.h"
#include <QStringList>
#include <QTextStream>
#include <QRegularExpression>
#include <QMetaType>
#include <QProcess>
#include <QEventLoop>
#include <QTimer>
#include <QSqlDriver>
#include <QSqlQuery>
#include <QSqlError>

namespace{
    /// Logger for all instances of class
    LOGGER_DEFINE_GLOBAL(logger, "PgsqlDaemonRunner")
}

PgsqlDaemonRunner::PgsqlDaemonRunner(const QDir &root, QObject *parentVal)
  : ProgRunner(parentVal), _root(root){

    if(!_root.mkdir("data")){
        throw std::runtime_error("failed to create data directory");
    }
    const auto dataPath = _root.absoluteFilePath("data");
    QFile(dataPath).setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    _listenHost = "127.0.0.1";
    _listenPort = UnitTestHelpers::randomTcpPort();

    _confPath = _root.absoluteFilePath("config");
    const auto hbaPath = _root.absoluteFilePath("hba.conf");
    const auto identPath = _root.absoluteFilePath("ident.conf");
    const auto socketsPath = _root.absoluteFilePath("sockets");

    {
        QFile file(_confPath);
        if(!file.open(QIODevice::WriteOnly)){
            throw std::runtime_error("failed to create config file");
        }
        QTextStream cstr(&file);
        cstr << "data_directory = '" << dataPath << "'" << endl;
        cstr << "unix_socket_directories = '" << socketsPath << "'" << endl;
        cstr << "hba_file = '" << hbaPath << "'" << endl;
        cstr << "ident_file = '" << identPath << "'" << endl;
        cstr << "external_pid_file = '" << _root.absoluteFilePath("pidfile") << "'" << endl;
        cstr << "listen_addresses = '" << _listenHost << "'" << endl;
        cstr << "port = " << _listenPort << endl;
    }
    {
        QFile file(hbaPath);
        if(!file.open(QIODevice::WriteOnly)){
            throw std::runtime_error("failed to create HBA file");
        }
        QTextStream cstr(&file);
        cstr << "# TYPE  DATABASE        USER            ADDRESS                 METHOD" << endl;
        cstr << "local   all             all                                     trust" << endl;
        cstr << "host    all             all             0.0.0.0/0               trust" << endl;
    }
    {
        QFile file(identPath);
        if(!file.open(QIODevice::WriteOnly)){
            throw std::runtime_error("failed to create ident file");
        }
    }
    _root.mkdir(socketsPath);

    const QString postgresqlBin =
        QProcessEnvironment::systemEnvironment().value("POSTGREGSQL_BIN", "postgres");
    const QString initdbBin =
        QProcessEnvironment::systemEnvironment().value("POSTGREGSQL_INITDB_BIN", "initdb");

    // Prepare DB data
    {
        ProgRunner initProg;
        initProg.setProgram(initdbBin, {QString("--pgdata=%1").arg(dataPath)});
        if(!initProg.start()){
            throw std::runtime_error("failed to run initdb");
        }
        if(!initProg.join()){
            throw std::runtime_error("failed to finish initdb");
        }
    }

    const QStringList args = {QString("--config-file=%1").arg(_confPath)};
    setProgram(postgresqlBin, args);
}

PgsqlDaemonRunner::~PgsqlDaemonRunner(){
    stop();
}

bool PgsqlDaemonRunner::start(){
    if(!ProgRunner::start()){
        return false;
    }

    if(!waitForLineRe(QRegularExpression(".*database system is ready to accept connections"))){
        return false;
    }

    _dbconn = QSqlDatabase::addDatabase("QPSQL", "PgsqlDaemonRunner");
    _dbconn.setHostName(_listenHost);
    _dbconn.setPort(_listenPort);
    _dbconn.setDatabaseName("postgres");
    if(!_dbconn.open()){
        LOGGER_ERROR(logger) << "failed DB: " << _dbconn.lastError().text().toStdString();
        return false;
    }

    return true;
}

bool PgsqlDaemonRunner::stop(int timeoutMs){
    _dbconn.close();

    return ProgRunner::stop(timeoutMs);
}

QSqlQuery PgsqlDaemonRunner::exec(const QString &query){
    if(!_dbconn.isOpen()){
        throw std::runtime_error("DB not open");
    }

    LOGGER_DEBUG(logger) << "Running query: \"" << query.toStdString() << "\"";
    QSqlQuery res(_dbconn);
    if(!res.exec(query)){
        throw std::runtime_error(QString("query failed: %1").arg(res.lastError().text()).toStdString());
    }
    return res;
}

bool PgsqlDaemonRunner::createDatabase(const QString &dbName){
    exec(QString("CREATE DATABASE %1")
        .arg(_dbconn.driver()->escapeIdentifier(dbName, QSqlDriver::TableName))
    );
    return true;
}

bool PgsqlDaemonRunner::clearDatabases(){
    auto res = exec("SELECT datname FROM pg_database WHERE datistemplate=false AND datname!='postgres'");
    while(res.next()){
        const auto dbName = res.value(0).toString();
        exec(QString("DROP DATABASE %1")
            .arg(_dbconn.driver()->escapeIdentifier(dbName, QSqlDriver::TableName))
        );
    }
    return true;
}
