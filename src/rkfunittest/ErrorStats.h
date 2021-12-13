// Copyright (C) 2017 RKF Engineering Solutions, LLC

#ifndef ERROR_STATS_H
#define ERROR_STATS_H

#include <QString>
#include <cmath>

template<typename T>
class ErrorStats{
public:
    static T sqr(T val){
        return val * val;
    }

    ErrorStats(const QString &name)
        : _name(name), _moment0(0), _moment1(0), _moment2(0),
          _worst(0){}

    /** Add an error based on comparing two values.
     * The error is the difference of measured from expected.
     *
     * @param actual The measured value.
     * @param expect The expected value.
     */
    void addError(const T &actual, const T &expect){
        addError(actual - expect);
    }

    /** Add a relative error value.
     * The error is the difference of measured from expected scaled by the
     * expected value.
     *
     * @param actual The measured value.
     * @param expect The expected value.
     */
    void addRelError(const T &actual, const T &expect){
        addError((actual - expect) / std::abs(expect));

    }

    /** Add an error value directly.
     *
     * @param err The error value with sign and magnitude.
     * @post The statistics are all updated.
     */
    void addError(const T &err){
        ++_moment0;
        _moment1 += err;
        _moment2 += sqr(err);
        // Preserve sign
        if(std::abs(err) > std::abs(_worst)){
            _worst = err;
        }
    }
    
    const QString & name() const{
        return _name;
    }

    int count() const{
        return _moment0;
    }

    /** Compute the numeric mean of the errors.
    */
    T mean() const{
        return _moment1 / T(_moment0);
    }

    /** Compute the central variance (ignoring mean) of the errors.
     * @return The variance of the error values themselves.
     */
    T centralVar() const{
        return _moment2 / T(_moment0 - 1);
    }

    /** Compute the non-central variance of the errors.
     * @return The variance of the error values less the mean value.
     */
    T noncentralVar() const{
        return centralVar() - sqr(mean());
    }

    /** Get the worst seen error.
     *
     * @return The largest-magnitude error value.
     * This value matches the error value exactly, including sign.
     */
    T worst() const{
        return _worst;
    }

private:
    /// The name used to print stats()
    QString _name;
    /// The zeroth statistical moment (sample count)
    int _moment0;
    /// The first statistical moment (sum of values)
    T _moment1;
    /// The second statistical moment (sum of squares)
    T _moment2;
    /// The largest-magnitude value
    T _worst;
};

template<typename T>
std::ostream & operator<<(std::ostream &stream, const ErrorStats<T> &stats){
    stream
        << "Errors for " << stats.name().toStdString()
        << ", count: " << stats.count()
        << ", mean: " << stats.mean()
        << ", std-dev: " << std::sqrt(stats.centralVar())
        << ", worst " << stats.worst();
    return stream;
}

#endif /* ERROR_STATS_H */
