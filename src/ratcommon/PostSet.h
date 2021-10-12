//  

#ifndef SRC_RATCOMMON_POSTSET_H_
#define SRC_RATCOMMON_POSTSET_H_

#include "ratcommon_export.h"
#include <memory>

/** Log an error condition.
 *
 * @param msg The exception message.
 */
RATCOMMON_EXPORT
void logPostSetError(const char *msg);

/** A class to overwrite a variable upon destruction.
 * This enforces a postcondition upon a lexical scope.
 *
 * Example of use:
 * @code
 * struct SomeC : public BaseC{
 *   int val;
 *   // At end of call, val is guaranteed to be 20
 *   // thatFunc() or otherFunc() may modify val temporarily
 *   int someThing(){
 *     PostSet<int> ps(val, 20);
 *     if(foo){
 *       return thatFunc();
 *     }
 *     return otherFunc();
 *   }
 * };
 * @endcode
 * @tparam Type The type of value to set.
 */
template <typename Type>
class PostSet{
public:
    /** Bind to a variable to overwrite and value to write with.
     * @param var The variable to overwrite.
     * Its lifetime must be at least as long as this object
     * @param val The value to set when this object is destroyed.
     */
    PostSet(Type &var, const Type &val)
      : _var(var), _val(val){}

    /** Bind to a variable and initialize with a new value.
     * @param var The variable to overwrite.
     * Its lifetime must be at least as long as this object
     * @param pre The value to set immediately.
     * @param post The value to set when this object is destroyed.
     */
    PostSet(Type &var, const Type &pre, const Type &post)
      : _var(var), _val(post){
        _var = pre;
    }

    /// Perform the write
    ~PostSet(){
        try {
            _var = _val;
        }
        catch (std::exception &err) {
            logPostSetError(err.what());
        }
        catch (...) {
            logPostSetError(nullptr);
        }
    }

private:
    /// The variable to overwrite
    Type &_var;
    /// A copy of the value to set
    Type _val;
};

/** Helper function to define a post-setter only final state.
 *
 * @tparam Type The value type to set.
 * @param var The variable to overwrite.
 * Its lifetime must be at least as long as this object
 * @param post The value to set when this object is destroyed.
 */
template<typename Type>
std::unique_ptr<const PostSet<Type>> make_PostSet(Type &var, const Type &post){
    return std::unique_ptr<const PostSet<Type>>(new PostSet<Type>(var, post));
}

/** Helper function to define a post-setter with initial state.
 *
 * @tparam Type The value type to set.
 * @param var The variable to overwrite.
 * Its lifetime must be at least as long as this object
 * @param pre The value to set immediately.
 * @param post The value to set when this object is destroyed.
 */
template<typename Type>
std::unique_ptr<const PostSet<Type>> make_PostSet(Type &var, const Type &pre, const Type &post){
    return std::unique_ptr<const PostSet<Type>>(new PostSet<Type>(var, pre, post));
}

#endif /* SRC_RATCOMMON_POSTSET_H_ */
