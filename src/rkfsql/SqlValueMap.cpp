// 

#include <cmath>
#include <QSqlField>
#include <QStringList>
#include <QDebug>
#include "SqlValueMap.h"
#include "rkflogging/ErrStream.h"

namespace{
/** Determine if a value is floating point.
 *
 * @param var The variant value.
 * @return True if the value is floating point.
 */
bool isFloat(const QVariant &var){
    // userType() will either be a QVariant ID or a QMetaType ID
    return ((var.userType() == QVariant::Double) || (var.userType() == QMetaType::Float));
}

/** Compare two floating point values.
 *
 * @param valA The first value.
 * @param valB The second value.
 * @return True if the values are within a fraction of their magnitudes.
 */
bool compareFloat(double valA, double valB){
    const double diff = std::abs(valB - valA);
    const double mag = std::abs(valA) + std::abs(valB);
    if(mag == 0){
        // both are zero
        return true;
    }
    return (diff / mag) < 1e-5;
}
}

SqlValueMap::SqlValueMap(const QSqlRecord &rec){
    const int valCount = rec.count();

    for(int colI = 0; colI < valCount; ++colI){
        const QSqlField field = rec.field(colI);
        _vals.insert(field.name(), field.value());
    }
}

QVariant SqlValueMap::value(const QString &name) const{
    const QVariantMap::const_iterator it = _vals.find(name);
    if(it == _vals.end()){
        throw std::runtime_error(ErrStream() << "Bad value map name \"" << name << "\"");
    }
    return it.value();
}

QVariantList SqlValueMap::values(const QStringList &names) const{
    QVariantList vals;

    foreach(const QString &name, names){
        vals.append(value(name));
    }

    return vals;
}

SqlValueMap::MismatchMap SqlValueMap::mismatch(const SqlValueMap &other) const{
    MismatchMap result;

    // QMap iterates in key-order so keys will have same order
    QVariantMap::const_iterator itA = _vals.begin();
    QVariantMap::const_iterator itB = other._vals.begin();

    while((itA != _vals.end()) && (itB != other._vals.end())){
        if(itA.key() != itB.key()){
            // itB is ahead, other missing at key itA
            if(itA.key() < itB.key()){
                result.insert(itA.key(), MismatchPair(itA->toString(), QString()));
                ++itA;
            }
            else{
                result.insert(itB.key(), MismatchPair(QString(), itB->toString()));
                ++itB;
            }
        }

        bool same;
        if(isFloat(itA.value()) || isFloat(itB.value())){
            // compare as floats with tolerance
            same = compareFloat(itA->toDouble(), itB->toDouble());
        }
        else if((itA->type() == QVariant::String) || (itB->type() == QVariant::String)){
            // compare as strings ignoring case
            same = (itA->toString().compare(itB->toString(), Qt::CaseInsensitive) == 0);
        }
        else{
            same = (itA.value() == itB.value());
        }
        if(!same){
            result.insert(itA.key(), MismatchPair(itA->toString(), itB->toString()));
        }

        ++itA;
        ++itB;
    }

    // Keys only in this map
    for(; itA != _vals.end(); ++itA){
        result.insert(itA.key(), MismatchPair(itA->toString(), QString()));
    }
    // Keys only in other map
    for(; itB != other._vals.end(); ++itB){
        result.insert(itB.key(), MismatchPair(QString(), itB->toString()));
    }

    return result;
}

QDebug operator << (QDebug stream, const SqlValueMap &obj){
    stream << obj._vals;
    return stream;
}
