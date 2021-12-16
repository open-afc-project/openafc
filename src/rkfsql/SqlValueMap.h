// 

#ifndef SQL_VALUE_MAP_H
#define SQL_VALUE_MAP_H

#include <QSqlRecord>
#include <QVariant>

/** A class to represent a single row of an SQL query result by a name--value
 * map.
 */
class SqlValueMap{
public:
    /** Pair of mismatched values.
     * The @c first value is from this object, the @c second value
     * is from the other object.
     */
    typedef QPair<QString, QString> MismatchPair;
    /** Type for result of mismatch() function.
     * Map from key string to pair of (this, other) values.
     */
    typedef QMap<QString, MismatchPair> MismatchMap;

    /// Create an empty value map
    SqlValueMap(){}

    /** Extract a value map from an SQL record.
     *
     * @param rec The record to extract from.
     */
    explicit SqlValueMap(const QSqlRecord &rec);

    /** Get a single value from the set.
     *
     * @param name The unique name for the value.
     * @return The value in the map by the name.
     * @throw RuntimeError If the name does not exist.
     */
    QVariant value(const QString &name) const;

    /** Get multiple values from the set.
     *
     * @param names The ordered list of unique names to get.
     * If the names are not unique within the list it is an error.
     * @return The ordered list of values associated with each name.
     * @throw RuntimeError If any name does not exist, or any name is
     * duplicated.
     */
    QVariantList values(const QStringList &names) const;

    /** Determine if two value sets are identical.
     *
     * @param other The values to compare against.
     * @return An empty map if both sets have identical names and each name has
     * identical values. A non-empty map indicates which names (and values)
     * are mismatched.
     */
    MismatchMap mismatch(const SqlValueMap &other) const;

    friend QDebug operator << (QDebug stream, const SqlValueMap &obj);

private:
    /// Value storage
    QVariantMap _vals;
};

/** Debug printing.
 *
 * @param stream The stream to write.
 * @param obj The values to write.
 * @return The updated stream.
 */
QDebug operator << (QDebug stream, const SqlValueMap &obj);

#endif /* SQL_VALUE_MAP_H */
