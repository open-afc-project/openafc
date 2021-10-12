//  

#include "FileTestHelpers.h"
#include "cpocommon/Logging.h"
#include "cpocommon/TextHelpers.h"
#include <cstdlib>
#include <iostream>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#if defined(Q_OS_WIN)
#include <QCoreApplication>
#endif

namespace{
/// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "FileTestHelpers")

    // Remove all files and sub-directories of the given directory
    bool cleanDir(QDir dir){
        foreach(const QString &dirName, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)){
            if(!cleanDir(dir.absoluteFilePath(dirName))){
                std::cerr << "Failed to clean dir " << dir.absoluteFilePath(dirName).toStdString();
                return false;
            }
            if(!dir.rmdir(dirName)){
                std::cerr << "Failed to remove subdir " << dir.absoluteFilePath(dirName).toStdString();
                return false;
            }
        }
        foreach(const QString &fileName, dir.entryList(QDir::Files | QDir::Hidden | QDir::System)){
#if defined(Q_OS_WIN)
            // Ensure that the file is not read-only
            FileTestHelpers::execSub("attrib", QStringList() << "-r" << dir.absoluteFilePath(fileName));
#endif
            if(!dir.remove(fileName)){
                std::cerr << "Failed to remove file " << dir.absoluteFilePath(fileName).toStdString();
                return false;
            }
        }
        return true;
    }
}

FileTestHelpers::TestDir::TestDir(const QString &testName)
  : QDir(QDir::temp()){
#if defined(Q_OS_WIN)
    QByteArray randStr;
    randStr.resize(3);
    for(auto it = randStr.begin(); it != randStr.end(); ++it){
        *it = char(::rand() % 0x100);
    }
    const QString dirName = absoluteFilePath(
        QString("unittest-%1-%2-%3")
            .arg(testName)
            .arg(QCoreApplication::applicationPid())
            .arg(QLatin1String(randStr.toHex()))
    );
    QDir::mkdir(dirName);
#else
    QByteArray dformat = absoluteFilePath(QString("unittest-%1-XXXXXX").arg(testName)).toUtf8();
    if(::mkdtemp(dformat.data()) == NULL){
        throw RuntimeError(QString("Failed to make temporary directory %1").arg(QString(dformat)));
    }
    const QString dirName(dformat);
#endif

    const auto env = QProcessEnvironment::systemEnvironment();
    if(!env.value("FILETESTHELPERS_TESTDIR_KEEP").isEmpty()){
        _keep = true;
        LOGGER_INFO(logger) << "Saving temporary path \"" << dirName << "\"";
    }
    if(!QDir::cd(dirName)){
        throw RuntimeError(QString("Failed to enter directory %1").arg(dirName));
    }
}


FileTestHelpers::TestDir::~TestDir(){
    if(path().isEmpty()){
        return;
    }
    if (_keep) {
        return;
    }
    clean();
    if(!QDir::rmdir(absolutePath())){
        std::cerr << "Failed to remove TestDir" << absolutePath().toStdString();
    }
}

void FileTestHelpers::TestDir::setKeep(bool option) {
    _keep = option;
}

void FileTestHelpers::TestDir::takeFile(const QFileInfo &file){
    if(!QFile::copy(file.absoluteFilePath(), absoluteFilePath(file.fileName()))){
        throw std::runtime_error("Failed to copy file");
    }
}

void FileTestHelpers::TestDir::clean(){
    if(!cleanDir(*this)){
        throw std::runtime_error("Failed to clean TestDir");
    }
}

bool FileTestHelpers::makeFile(const QString &fileName){
    {
        QFileInfo info(fileName);
        QDir::root().mkpath(info.absoluteDir().path());
    }
    QFile file(fileName);
    if(!file.exists()){
        LOGGER_DEBUG(logger) << "makeFile at " << fileName;
        file.open(QFile::WriteOnly);
        file.close();
    }
    return file.exists();
}

bool FileTestHelpers::makeFile(const QString &fileName, const QByteArray &content){
    {
        QFileInfo info(fileName);
        QDir::root().mkpath(info.absoluteDir().path());
    }
    LOGGER_DEBUG(logger) << "makeFile at " << fileName;
    QFile file(fileName);
    if(!file.open(QFile::WriteOnly)){
        return false;
    }
    return (file.write(content) == content.size());
}

QByteArray FileTestHelpers::content(const QString &fileName){
    QFile file(fileName);
    if(!file.open(QFile::ReadOnly)){
        return QByteArray();
    }
    return file.readAll();
}

bool FileTestHelpers::makeReadOnly(const QString &fileName){
    if(!makeFile(fileName)){
        return false;
    }
#if defined(Q_OS_WIN)
    execSub("attrib", QStringList() << "+r" << fileName);
#else /* Assume unix */
    execSub("chmod", QStringList() << "a-w" << fileName);
#endif
    return true;
}

void FileTestHelpers::execSub(const QString &cmd, const QStringList &args, const QString &workingDir){
    LOGGER_DEBUG(logger)
        << "execSub " << TextHelpers::quoted(cmd) << " " << TextHelpers::quoted(args).join(" ");
    QProcess proc;
    proc.setWorkingDirectory(workingDir);
    proc.start(cmd, args);
    proc.waitForFinished(-1);
    const int status = proc.exitCode();
    if(status != 0){
        throw std::runtime_error(QString("Failed to execute command (%1): %2 %3").arg(status).arg(cmd).arg(args.join(" ")).toStdString());
    }
}
