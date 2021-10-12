//  

#ifndef SRC_CPOTESTCOMMON_QUANTITYNEAR_H_
#define SRC_CPOTESTCOMMON_QUANTITYNEAR_H_

#include "GtestShim.h"
#include <boost/units/cmath.hpp>
#include <cmath>

template<typename Quant>
inline Quant QuantityNearStdAbs(Quant diff) {
    return std::abs(diff);
}

template<typename Quant>
inline Quant QuantityNearBoostAbs(Quant diff) {
    return boost::units::abs(diff);
}

/** Binary predicate to compare two quantities against a maximum tolerance.
 * @tparam Quant The quantity type being differenced.
 * @tparam Metric The metric type being compared.
 */
template<typename Quant, typename Metric>
class QuantityNear{
public:
    using MetricFunc = std::function<Metric(Quant)>;
        
    /** Define the comparison.
     *
     * @param tolerance The maximum difference between two values.
     * @param metric The 'difference' metric function.
     */
    QuantityNear(const Metric &tolerance, const MetricFunc &metric)
      : _tol(tolerance), _met(metric){}

    /** Compare values.
     *
     * @param lt The left side to compare.
     * @param rt The right side to compare.
     * @return True if the metric value is less than the tolerance.
     */
    testing::AssertionResult operator()(const Quant &lt, const Quant &rt) const{
        const auto diff = lt - rt;
        const auto metval = _met(diff);
        if (metval < _tol) {
            return testing::AssertionSuccess()
                << "Metric " << metval << " below tolerance " << _tol;
        }
        else {
            return testing::AssertionFailure()
                << "Metric " << metval << " not below tolerance " << _tol;
        }
    }

private:
    Metric _tol;
    MetricFunc _met;
};

/** Convenience function to avoid explicit typing.
 * @tparam Quant The quantity type to difference, std::abs(), and compare.
 * @param tolerance The maximum difference between two values.
 */
template<typename Quant>
QuantityNear<Quant, Quant> make_QuantityNear(const Quant &tolerance){
    return QuantityNear<Quant, Quant>(tolerance, QuantityNearStdAbs<Quant>);
}

/** Convenience function to avoid explicit typing.
 * @tparam Unit The boost::units unit for the quantity.
 * @tparam Stor The value storage type for the quantity.
 * @param tolerance The maximum difference between two values.
 */
template<typename Unit, typename Stor>
QuantityNear<boost::units::quantity<Unit, Stor>, boost::units::quantity<Unit, Stor>> make_QuantityNear(const boost::units::quantity<Unit, Stor> &tolerance){
    return QuantityNear<boost::units::quantity<Unit, Stor>, boost::units::quantity<Unit, Stor>>(tolerance, QuantityNearBoostAbs<boost::units::quantity<Unit, Stor>>);
}

/** Convenience function to avoid explicit typing.
 * @tparam Quant The quantity type to difference.
 * @tparam Metric The metric-value type to compare.
 * @param tolerance The maximum difference between two values.
 */
template<typename Quant, typename Metric=Quant>
QuantityNear<Quant, Metric> make_QuantityNear(const Metric &tolerance, const std::function<Metric(Quant)> &metricFunc){
    return QuantityNear<Quant, Metric>(tolerance, metricFunc);
}

#endif /* SRC_CPOTESTCOMMON_QUANTITYNEAR_H_ */
