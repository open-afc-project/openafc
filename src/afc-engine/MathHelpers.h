
#ifndef MATH_HELPERS_H
#define MATH_HELPERS_H

#include <cmath>
#include <limits>
#include <algorithm>

namespace MathHelpers{

/** Convert an angle from degrees to radians.
 * @param deg The angle in degrees
 * @return The angle in radians.
 */
template<typename T>
inline T deg2rad(T deg){
    return M_PI/180.0 * deg;
}
/** Convert an angle from radians to degrees.
 * @param rad The angle in radians
 * @return The angle in degrees.
 */
template<typename T>
inline T rad2deg(T rad){
    return 180.0/M_PI * rad;
}

/** Shortcut for computing squares.
 * @param val The value to square.
 * @return The square of @c val.
 */
template<typename T>
inline T sqr(T val){
    return val * val;
}

/** Shortcut for computing cubes.
 * @param val The value to cube.
 * @return The cube of @c val.
 */
template<typename T>
inline T cube(T val){
    return val * val * val;
}

/** Shortcut for computing sinc.
 * @param x The value to compute sinc(x) = sin(pi * x) / (pi * x).
 * @return The sinc of @c x.
 */
template <typename T>
inline T sinc(T x) {
    static const double eps = 1e-6;
    if (x < eps && x > -eps)
        return 1.0 - sqr(M_PI * x) / 6.0;
    else
        return sin(M_PI * x) / (M_PI * x);
}

/** One-dimensional linear interpolation.
 * @param val1 The zero-scale value.
 * @param val2 The unit-scale value.
 * @param t The scale factor.
 * @return The effective value (t * val1) + ((1-t) * val2)
 */
inline double interp1d(double val1, double val2, double t){
    return val1 + t * (val2 - val1);
}

/** Wrap a value to a particular size by tiling the object space onto
 * the image space.
 * @param size The exclusive maximum limit.
 * @param value The value to be limited.
 * @return The value limited to the range [0, @a size) by tiling.
 */
double tile(double size, double value);

/** Wrap a value to a particular size by clamping the object space to the
 * edge of the image space.
 * @param size The exclusive maximum limit.
 * @param value The value to be limited.
 * @return The value limited to the range [0, @a size] by clamping.
 */
double clamp(double size, double value);

/** Wrap a value to a particular size by mirroring the object space onto
 * the image space.
 * @param size The exclusive maximum limit.
 * @param value The value to be limited.
 * @return The value limited to the range [0, @a size) by mirroring.
 */
double mirror(double size, double value);

/** Prepare a sample point for interpolating.
 * This stores a set of two (low/high) integer-points corresponding to
 * a single sample point, and a scale factor between them.
 */
struct Align{
    /** Compute the grid-aligned points and scale.
     *
     * @param value The value to compute for.
     */
    Align(const double value){
        p1 = std::floor(value);
        p2 = std::ceil(value);
        factor = value - p1;
    }

    /// First integer-point lower than the value
    double p1;
    /// First integer-point higher than the value
    double p2;
    /** Inverse weight factor to use for #p1 point.
     * Value has range [0, 1] where 0 means p1 == value, 1 means p2 == value.
     */
    double factor;
};

//helper class to calculate statistics of a continuously sampled one dimensional process
//without storage of the invidual samples
template <typename T>
class RunningStatistic {
public:
    RunningStatistic() {
        _count = 0;
        _sum = 0.0;
        _sumOfSquares = 0.0;
        _max = 0.0;
        _min = std::numeric_limits<T>::max();
    }

    inline RunningStatistic & operator << (T sample) {
        if(_count == 0) _max = _min = sample;
                            
        _count++;
        _sum += sample;
        _sumOfSquares += sqr(sample);
        _max = std::max(_max, sample);
        _min = std::min(_min, sample);
        return *this;
    }

    inline RunningStatistic & operator << (const RunningStatistic &statistic) {
        _count += statistic._count;
        _sum += statistic._sum;
        _sumOfSquares += statistic._sumOfSquares;
        _max = std::max(_max, statistic._max);
        _min = std::min(_min, statistic._min);
        return *this;
    }

    inline int count() const {
        return _count;
    }

    inline T mean() const {
        if (_count == 0)
            return 0.0;

        return _sum / T(_count);
    }

    inline T min() const {
        if (_count == 0)
            return 0.0;
        
        return _min;
    }
		
    inline T max() const {
        return _max;
    }

    inline T variance(bool unbiased = true) const {
        if (_count < 1)
            return 0.0;

        T u = mean();
        if (unbiased)
            return _sumOfSquares / T(_count - 1) - T(_count) / T(_count - 1) * sqr(u);
        else
            return _sumOfSquares / T(_count) - sqr(u);
    }

private:
    int _count;
    T _sum;
    T _sumOfSquares;
    T _min;
    T _max;
};


} // end namespace MathHelpers

#endif /* MATH_HELPERS_H */
