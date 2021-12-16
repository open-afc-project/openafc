// 

#include "ProgRunner.h"
#include "rkflogging/Logging.h"
#include "ratcommon/TextHelpers.h"
#include "ratcommon/overload_cast.h"
#include "rkfunittest/UnitTestHelpers.h"
#include <QStringList>
#include <QRegularExpression>
#include <QMetaType>
#include <QProcess>
#include <QEventLoop>
#include <QTimer>

// Necessary to process QProcess::finished() signal
Q_DECLARE_METATYPE(QProcess::ExitStatus);

namespace{
    const int reg = qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");

    /// Logger for all instances of class
    LOGGER_DEFINE_GLOBAL(logger, "ProgRunner")
}

ProgRunner::ProgRunner(QObject *parentVal)
  : QObject(parentVal){
    _goodExit = {0};

    _proc = new QProcess(this);
    _proc->setReadChannelMode(QProcess::MergedChannels);
    connect(_proc, &QProcess::readyReadStandardOutput, this, &ProgRunner::stdoutReadable);
    connect(_proc, &QProcess::readyReadStandardError, this, &ProgRunner::stderrReadable);

    // pass-through environment
    _proc->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
}

ProgRunner::~ProgRunner(){
    stop();
}

void ProgRunner::setProgram(const QString &prog, const QStringList &args){
    _proc->setProgram(prog);
    _proc->setArguments(args);
}

void ProgRunner::setEnvironment(const QProcessEnvironment &env){
    _proc->setProcessEnvironment(env);
}

void ProgRunner::setGoodExitCode(const QSet<int> &codes){
    _goodExit = codes;
}

bool ProgRunner::start(){
    if(_proc->state() != QProcess::NotRunning){
        LOGGER_DEBUG(logger) << "already started";
        return false;
    }

    LOGGER_DEBUG(logger) << "starting "
        << TextHelpers::quoted(_proc->program())
        << " " << TextHelpers::quoted(_proc->arguments()).join(" ");
    _proc->start();
    if(!_proc->waitForStarted()){
        LOGGER_DEBUG(logger) << "never started, exit: " << _proc->exitCode();
        return false;
    }

    return true;
}

bool ProgRunner::stop(int timeoutMs){
    if(_proc->state() == QProcess::NotRunning){
        return true;
    }

    _proc->terminate();
    return join(timeoutMs);
}

bool ProgRunner::join(int timeoutMs){
    if(_proc->state() != QProcess::NotRunning){
        // Wait in event loop
        QEventLoop waiter;
        connect(_proc, OVERLOAD<int>::OF(&QProcess::finished), &waiter, &QEventLoop::quit);
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &waiter, &QEventLoop::quit);
        timer.setInterval(timeoutMs);

        timer.start();
        waiter.exec();
        if(!timer.isActive()){
            LOGGER_ERROR(logger) << "never finished";
            _proc->kill();
            _proc->waitForFinished();
            return false;
        }
    }
    const int status = _proc->exitCode();
    LOGGER_DEBUG(logger) << "status " << status;

    return _goodExit.contains(status);
}

QByteArray ProgRunner::nextLine(){
    if(_readQueue.isEmpty()){
        return QByteArray();
    }
    return _readQueue.takeFirst();
}

bool ProgRunner::waitForLineRe(const QRegularExpression &expr, int timeoutMs){
    QEventLoop waiter;
    connect(this, &ProgRunner::linesAvailable, &waiter, &QEventLoop::quit);
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &waiter, &QEventLoop::quit);
    timer.setInterval(timeoutMs);
    timer.start();

    while(true){
        if(_readQueue.isEmpty()){
            if(_proc->atEnd() && (_proc->state() != QProcess::Running)){
                LOGGER_DEBUG(logger) << "waitForLine: Process died";
                return false;
            }

            // wait a second or when lines are ready
            if(!timer.isActive()){
                LOGGER_DEBUG(logger) << "waitForLine: timeout";
                return false;
            }
        }

        while(!_readQueue.isEmpty()){
            const QString got = QString::fromUtf8(nextLine());
            if(expr.match(got).hasMatch()){
                return true;
            }
        }

        waiter.exec();
    }
    // Satisfy warning
    return false;
}

void ProgRunner::stdoutReadable(){
    _proc->setReadChannel(QProcess::StandardOutput);
    bool added = false;
    while(_proc->canReadLine()){
        const auto line = _proc->readLine();
        _readQueue << line;
        LOGGER_DEBUG(logger) << "stdout line: " << line.trimmed().data();
        added = true;
    }

    if(added){
        emit linesAvailable();
    }
}

void ProgRunner::stderrReadable(){
    _proc->setReadChannel(QProcess::StandardError);
    bool added = false;
    while(_proc->canReadLine()){
        const auto line = _proc->readLine();
        _readQueue << line;
        LOGGER_DEBUG(logger) << "stderr line: " << line.trimmed().data();
        added = true;
    }

    if(added){
        emit linesAvailable();
    }
}
