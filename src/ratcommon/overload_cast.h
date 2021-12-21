// 

#ifndef OVERLOAD_CAST_H_
#define OVERLOAD_CAST_H_

/** Shortcut for casting overloaded Qt signal/slot member functions.
 * @tparam Args The desired argument(s).
 */
template<typename... Args> struct OVERLOAD {
    /** Cast a member function.
     *
     * @tparam C The class being cast.
     * @tparam R Member return type.
     * @param pmf The member function to cast.
     * @return The cast function, with specific @c Args arguments.
     */
    template<typename C, typename R>
    static auto OF( R (C::*pmf)(Args...) ) -> decltype(pmf) {
        return pmf;
    }
};

/// Specialize to treat void as empty
template<> struct OVERLOAD<void> {
    template<typename C, typename R>
    static auto OF( R (C::*pmf)() ) -> decltype(pmf) {
        return pmf;
    }
};

#endif /* OVERLOAD_CAST_H_ */
