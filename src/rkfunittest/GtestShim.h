// Copyright (C) 2017 RKF Engineering Solutions, LLC
#ifndef SRC_RKFUNITTEST_GTESTSHIM_H_
#define SRC_RKFUNITTEST_GTESTSHIM_H_

/** @file
 * This header must be included instead of <gtest/gtest.h> because it defines
 * template function PrintTo() overloads which must be visible to all compilation units
 * within a test suite.
 */
#if defined(GTEST_FAIL)
#error Must include "rkfunittest/GtestShim.h" instead of <gtest/gtest.h> directly.
#endif

//#include "rkfunittest_export.h"
#include "rkflogging/QtStream.h"
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QDebug>
#include <gtest/gtest.h>
#include <boost/optional/optional_io.hpp>
#include <memory>
#include <vector>
#include <set>
#include <ostream>

class QString;
class QLatin1String;
class QByteArray;
class QDateTime;
class QUrl;

#define LOCAL_GTEST_PRINTTO_FUNC(Cls) \
    class Cls; \
    inline void PrintTo(const Cls &value, ::std::ostream *stream){ \
        QString str; \
        QDebug(&str).nospace() << value; \
        *stream << str.toStdString(); \
    }

QT_BEGIN_NAMESPACE

LOCAL_GTEST_PRINTTO_FUNC(QChar)
LOCAL_GTEST_PRINTTO_FUNC(QJsonValue)
LOCAL_GTEST_PRINTTO_FUNC(QJsonObject)
LOCAL_GTEST_PRINTTO_FUNC(QJsonArray)
LOCAL_GTEST_PRINTTO_FUNC(QVariant)

//RKFUNITTEST_EXPORT
void PrintTo(const QString &value, ::std::ostream *stream);
//RKFUNITTEST_EXPORT
void PrintTo(const QLatin1String &value, ::std::ostream *stream);
//RKFUNITTEST_EXPORT
void PrintTo(const QByteArray &value, ::std::ostream *stream);
//RKFUNITTEST_EXPORT
void PrintTo(const QDateTime &value, ::std::ostream *stream);
//RKFUNITTEST_EXPORT
void PrintTo(const QDate &value, ::std::ostream *stream);
//RKFUNITTEST_EXPORT
void PrintTo(const QTime &value, ::std::ostream *stream);
//RKFUNITTEST_EXPORT
void PrintTo(const QUrl &value, ::std::ostream *stream);

QT_END_NAMESPACE

template<typename T>
::std::ostream & operator << (::std::ostream &stream, const ::std::vector<T> &val){
    return QtStream::out_stl_container(stream, val, "std::vector");
}
template<typename T>
::std::ostream & operator << (::std::ostream &stream, const ::std::list<T> &val){
    return QtStream::out_stl_container(stream, val, "std::list");
}
template<typename T>
::std::ostream & operator << (::std::ostream &stream, const ::std::set<T> &val){
    return QtStream::out_stl_container(stream, val, "std::set");
}
template<typename T>
::std::ostream & operator << (::std::ostream &stream, const ::std::shared_ptr<T> &val){
    return (stream << "std::shared_ptr(" << val.get() << ")");
}

#endif //SRC_RKFUNITTEST_GTESTSHIM_H_
