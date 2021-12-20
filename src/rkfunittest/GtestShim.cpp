// Copyright (C) 2017 RKF Engineering Solutions, LLC
#include "GtestShim.h"
#include <QDateTime>
#include <QUrl>

void PrintTo(const QString &value, std::ostream *stream){
    *stream << std::showbase << value;
}

void PrintTo(const QLatin1String &value, std::ostream *stream){
    *stream << std::showbase << value;
}

void PrintTo(const QByteArray &value, std::ostream *stream){
    *stream << std::showbase << std::hex << value;
}

void PrintTo(const QDateTime &value, std::ostream *stream){
    *stream << "QDateTime(" << value.toString(Qt::ISODate).toStdString() << ")";
}

void PrintTo(const QDate &value, std::ostream *stream){
    *stream << "QDate(" << value.toString(Qt::ISODate).toStdString() << ")";
}

void PrintTo(const QTime &value, std::ostream *stream){
    *stream << "QTime(" << value.toString(Qt::ISODate).toStdString() << ")";
}

void PrintTo(const QUrl &value, ::std::ostream *stream) {
    *stream << "QUrl(" << value.toString().toStdString() << ")";
}
