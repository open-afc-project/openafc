// 
#ifndef UNIT_TEST_HELPERS_H
#define UNIT_TEST_HELPERS_H

#include "GtestShim.h"
#include <qglobal.h>
#include <functional>
#include <cstdlib>
#include <cmath>

class QPointF;
class QRectF;

namespace UnitTestHelpers{

/** Initialize boost logging with only a @c stderr stream and a filter based on
 * environment variable UNITTESTHELPERS_LOGLEVEL (defaulted to "debug").
 * @post Logging is initialized.
 */
void initLogging();

/** Wait within a Qt event loop.
 *
 * @param waitMs The wait time in milliseconds.
 */
void waitEventLoop(int waitMs);

/** Random number uniformly distributed in range [min, max).
 */
template<typename T>
inline T randVal(T min, T max) = delete;

/** Random number uniformly distributed in range [min, max].
 */
template<typename T>
inline T randFull(T min, T max) = delete;

/** Random integer in the range [min, max)
 */
template<>
inline int randVal(int min, int max){
    return min + (std::rand() % (max - min));
}

/** Random double in the range [0, 1).
 */
double randUnitInEx();
/** Random double in the range [0, 1].
 */
double randUnitInIn();

/** Random double in the range [min, max).
 */
template<>
inline double randVal(double min, double max){
    return min + ((max - min) * randUnitInEx());
}

/** Random double in the range [min, max].
 */
template<>
inline double randFull(double min, double max){
    return min + ((max - min) * randUnitInIn());
}

/** A purely random floating point value.
 *
 * @return An arbitrary double precision float.
 * This may be NaN, +/-inf, or a denormalized value.
 */
double randDouble();

/** A random number outside of a finite range.
 *
 * @param min The minimum disallowed value.
 * @param max The maximum disallowed value.
 * @return An value not in range [min, max].
 */
template<typename T>
T randExclude(T min, T max);

template<>
inline double randExclude(double min, double max){
    double result;
    do{
        result = randDouble();
    }
    while(std::isnan(result) || ((result >= min) && (result <= max)));

    return result;
}

/** Choose a random value from a list of values.
 *
 * @tparam Container A class satisfying the STL Random Access
 * Container interface.
 * @param a The list of choices.
 * @return One value from the list.
 */
template<class Container>
typename Container::value_type randChoice(const Container &a){
    typedef typename Container::size_type sizet;
    const sizet index = randVal<sizet>(0, a.size());
    return a[index];
}

/** Uniform random point within a rectangle area.
 * @param rect The area to sample over.
 * @return A value with coordinates in the range:
 *  - X coord in range [rect.left, rect.right]
 *  - Y coord in range [rect.top, rect.bottom]
 */
QPointF randPoint(const QRectF &rect);

/** Used as a comparison for QCOMPARE_TOL with QPointF.
 *
 * @param pt The 'difference vector' to compute a metric for.
 * @return The length of the vector.
 */
qreal pointDelta(const QPointF &pt);

/** Sample latitude proportional to the length of its parallel circle.
 * @param The minimum allowed result.
 * @param The maximum allowed result.
 * @return A latitude in unit of degrees, in range [minDeg, maxDeg].
 */
double randLat(double minDeg, double maxDeg);

/** Get a random, currently-unused TCP port number.
 * @return An available TCP port in the range [49152, 65535].
 */
quint16 randomTcpPort();

/** Take the sign of a number.
 * @param val The value to test.
 * @return One of:
 *  - 0 if the value is neither negative nor positive.
 *  - +1 if the value is positive.
 *  - -1 if the value is negative.
 */
template<typename T>
int signum(T val){
    const T zero(0);
    if(val > zero){
        return +1;
    }
    if(val < zero){
        return -1;
    }
    return 0;
}

} // End namespace UnitTestHelpers

#endif /* UNIT_TEST_HELPERS_H */
