// Copyright (C) 2017 RKF Engineering Solutions, LLC

#ifndef QTSTREAM_H_
#define QTSTREAM_H_

#include <ostream>
#include <QDebug>

class QObject;
class QString;
class QUuid;
class QDateTime;
class QVariant;
class QJsonValue;
class QJsonArray;
class QJsonObject;
template<typename T1>
class QList;
template<typename T1, typename T2>
class QMap;
typedef QMap<QString, QVariant> QVariantMap;
class QVersionNumber;

namespace QtStream {

/** Install a local log handler.
 * The log channel name is "Qt".
 */
void installLogHandler();

/** Write a Qt value to an @c ostream via a QDebug stream.
 *
 * @param stream The stream to write.
 * @param value The value to output.
 * @return The updated stream.
 */
template<typename Typ>
::std::ostream & out_qt(::std::ostream &stream, const Typ &value){
    QString str;
    QDebug(&str).nospace() << value;
    stream << str.toStdString();
    return stream;
}

/** Convenience for logging stl-style containers.
 *
 * @param stream The stream to write to.
 * @param val The value to write.
 * @param className The class identifier name.
 * @return The updated stream.
 */
template<class Container>
::std::ostream & out_stl_container(::std::ostream &stream, const Container &val, const ::std::string &className){
    stream << className << "[size=" << val.size() << "](";
    for (auto it = val.begin(); it != val.end(); ++it) {
        if (it != val.begin()) {
            stream << ",";
        }
        stream << *it;
    }
    stream << ")";
    return stream;
}

}

QT_BEGIN_NAMESPACE

///@{
/** Convenience for logging Qt values.
 *
 * @param stream The stream to write to.
 * @param val The value to write.
 * @return The updated stream.
 */
::std::ostream & operator<<(::std::ostream &stream, const QObject *val);
::std::ostream & operator<<(::std::ostream &stream, const QString &val);
::std::ostream & operator<<(::std::ostream &stream, const QLatin1String &val);
::std::ostream & operator<<(::std::ostream &stream, const QStringRef &val);
::std::ostream & operator<<(::std::ostream &stream, const QByteArray &val);
::std::ostream & operator<<(::std::ostream &stream, const QUuid &val);
::std::ostream & operator<<(::std::ostream &stream, const QJsonValue &val);
::std::ostream & operator<<(::std::ostream &stream, const QJsonArray &val);
::std::ostream & operator<<(::std::ostream &stream, const QJsonObject &val);
::std::ostream & operator<<(::std::ostream &stream, const QVersionNumber &val);
template<typename T>
::std::ostream & operator << (::std::ostream &stream, const QList<T> &val){
    return QtStream::out_stl_container(stream, val, "QList");
}
template<typename T>
::std::ostream & operator << (::std::ostream &stream, const QVector<T> &val){
    return QtStream::out_stl_container(stream, val, "QVector");
}
template<typename T>
::std::ostream & operator << (::std::ostream &stream, const QSet<T> &val){
    return QtStream::out_stl_container(stream, val, "QSet");
}
///@}

#define LOG_STREAM_WRAP_QT(QtType) \
    inline ::std::ostream & operator<<(::std::ostream &stream, const QtType &val){ \
        return QtStream::out_qt(stream, val); \
    }
LOG_STREAM_WRAP_QT(QDateTime)
LOG_STREAM_WRAP_QT(QVariant)
LOG_STREAM_WRAP_QT(QVariantMap)

QT_END_NAMESPACE

#endif /* QTSTREAM_H_ */
