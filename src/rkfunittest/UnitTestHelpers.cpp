//  

#include "UnitTestHelpers.h"
#include "rkflogging/LoggingConfig.h"
#include <cmath>
#include <QEventLoop>
#include <QTimer>
#include <QRectF>
#include <QTcpSocket>
#include <QHostAddress>
#include <QProcessEnvironment>

void UnitTestHelpers::initLogging() {
    const auto env = QProcessEnvironment::systemEnvironment();

    Logging::Config config;
    config.useStdOut = false;
    config.useStdErr = true;
    config.filter.setLevel(env.value("UNITTESTHELPERS_LOGLEVEL", "debug").toStdString());
    Logging::initialize(config);
}

void UnitTestHelpers::waitEventLoop(int waitMs){
    QEventLoop eloop;
    QTimer::singleShot(std::max(1, waitMs), &eloop, &QEventLoop::quit);
    eloop.exec();
}

double UnitTestHelpers::randUnitInEx(){
    return (std::rand() / double(INT_MAX));
}

double UnitTestHelpers::randUnitInIn(){
    return (std::rand() / double(INT_MAX - 1));
}

double UnitTestHelpers::randDouble(){
    double result;

    quint8 *data = reinterpret_cast<quint8 *>(&result);
    for(::size_t ix = 0; ix < sizeof(double); ++ix){
        // Modulo is implicit by bit size reduction
        data[ix] = ::rand();
    }

    return result;
}

QPointF UnitTestHelpers::randPoint(const QRectF &rect){
    return QPointF(
        randFull<qreal>(rect.left(), rect.right()),
        randFull<qreal>(rect.top(), rect.bottom())
    );
}

qreal UnitTestHelpers::pointDelta(const QPointF &pt){
    return pt.manhattanLength();
}

double UnitTestHelpers::randLat(double minDeg, double maxDeg){
    const double minProj = std::sin(M_PI / 180.0 * minDeg);
    const double maxProj = std::sin(M_PI / 180.0 * maxDeg);
    return 180.0 / M_PI * std::asin(randFull<double>(minProj, maxProj));
}

quint16 UnitTestHelpers::randomTcpPort(){
    forever{
        const quint16 port = UnitTestHelpers::randVal<qint32>(49152, 65536);
        QTcpSocket s;
        s.connectToHost(QHostAddress::LocalHost, port);
        if(!s.waitForConnected(1000)){
            return port;
        }
    }

    // avoid warning
    return 0;
}
