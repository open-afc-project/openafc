// 

#include "QtStream.h"
#include "Logging.h"
#include <QObject>
#include <QMetaObject>
#include <QUuid>
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QVersionNumber>

namespace {

/// Redirection logger for Qt
LOGGER_DEFINE_GLOBAL(logger, "Qt")

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
/** Redirect Qt logging.
 *
 * @param type Severity level of the message.
 * @param context
 * @param msg The message text.
 * @sa qt_logger
 */
void qtRedirect(QtMsgType type, const QMessageLogContext &context, const QString &msg){
    Logging::severity_level lvl;
    switch(type){
    case QtDebugMsg:
        lvl = Logging::LOG_DEBUG;
        break;
    case QtInfoMsg:
        lvl = Logging::LOG_INFO;
        break;
    case QtWarningMsg:
        lvl = Logging::LOG_WARN;
        break;
    default:
        lvl = Logging::LOG_ERROR;
        break;
    }
    LOG_SEV(logger, lvl) << msg.toStdString();
}
#endif /* QT_VERSION */

}

void QtStream::installLogHandler() {
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    qInstallMessageHandler(qtRedirect);
#endif
}

std::ostream & operator<<(std::ostream &stream, const QObject *val){
    if(val){
        const QMetaObject *const meta = val->metaObject();
        stream << meta->className() << "(" << reinterpret_cast<const void *>(val) << ")";
    }
    else{
        stream << "QObject(null)";
    }
    return stream;
}

std::ostream & operator<<(std::ostream &stream, const QString &val){
    const bool extra = stream.flags() & std::ios_base::showbase;
    if (extra) {
        stream << "QString(";
        if (val.isNull()) {
            stream << "null";
        }
        else {
            stream << '"' << val.toUtf8().constData() << '"';
        }
        stream << ")";
    }
    else {
        stream << val.toUtf8().constData();
    }
    return stream;
}

std::ostream & operator<<(std::ostream &stream, const QLatin1String &val){
    const bool extra = stream.flags() & std::ios_base::showbase;
    if (extra) {
        stream << "QLatin1String(";
        if (val.data() == nullptr) {
            stream << "null";
        }
        else {
            stream << '"' << val.data() << '"';
        }
        stream << ")";
    }
    else {
        stream << val.data();
    }
    return stream;
}

std::ostream & operator<<(std::ostream &stream, const QStringRef &val){
    const bool extra = stream.flags() & std::ios_base::showbase;
    if (extra) {
        stream << "QStringRef(\"";
    }
    stream << val.toUtf8().constData();
    if (extra) {
        stream << "\")";
    }
    return stream;
}

std::ostream & operator<<(std::ostream &stream, const QByteArray &val){
    const bool inhex = stream.flags() & std::ios_base::hex;
    QByteArray show;
    if(inhex){
        show = val.toHex();
    }
    else {
        show = val; // Qt reference copy
    }

    const bool extra = stream.flags() & std::ios_base::showbase;
    if (extra) {
        stream << "QByteArray(";
        if (val.isNull()) {
            stream << "null";
        }
        else {
            if (!inhex) {
                stream << '"';
            }
            stream << show.constData();
            if (!inhex) {
                stream << '"';
            }
        }
        stream << ")";
    }
    else {
        stream << show.constData();
    }
    return stream;
}

std::ostream & operator<<(std::ostream &stream, const QUuid &val){
    stream << val.toString();
    return stream;
}

std::ostream & operator<<(std::ostream &stream, const QJsonValue &val){
    return QtStream::out_qt(stream, val);
}
std::ostream & operator<<(std::ostream &stream, const QJsonArray &val){
    return QtStream::out_qt(stream, val);
}
std::ostream & operator<<(std::ostream &stream, const QJsonObject &val){
    return QtStream::out_qt(stream, val);
}

std::ostream & operator<<(std::ostream &stream, const QVersionNumber &val){
    return QtStream::out_qt(stream, val);
}
