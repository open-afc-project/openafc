// 

#ifndef SRC_RKFLOGGING_ERRSTREAM_H_
#define SRC_RKFLOGGING_ERRSTREAM_H_

#include "rkflogging_export.h"
#include "QtStream.h"
#include <sstream>

class QString;

/** A helper class to define exception text in-line with the object
 * constructor.
 *
 * Example use is:
 * @code
 * throw std::logic_error(ErrStream() << "some message: " << errCode);
 * @endcode
 */
class RKFLOGGING_EXPORT ErrStream {
public:
    /** Initialize to an empty string.
     */
    ErrStream();

    /** Implicitly convert to std::string for exceptions.
     * @return The string representation.
     */
    operator std::string () const{
        return _str.str();
    }

    /** Implicitly convert to QString for processing.
     * @return The text representation.
     */
    operator QString () const;

    /** Append to stream and keep this class type for final conversion.
     *
     * @tparam The type to append.
     * @param val The value to append.
     * @return This updated stream.
     */
    template<typename T>
    ErrStream & operator<<(const T &val){
        _str << val;
        return *this;
    }

    ///@{
    /** Handle stream manipulation functions.
     *
     * @tparam The stream type to manipulate.
     * @param func The manipulator to apply.
     * @return This updated stream.
     */
    template<typename T>
    ErrStream & operator<<(T & (*func) (T &)){
        _str << func;
        return *this;
    }
    ErrStream & operator<<(std::ostream & (*func) (std::ostream &)){
        _str << func;
        return *this;
    }
    ///@}

private:
    /// string storage
    std::ostringstream _str;
};

#endif /* SRC_RKFLOGGING_ERRSTREAM_H_ */
