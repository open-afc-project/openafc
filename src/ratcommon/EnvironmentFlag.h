//  

#ifndef CPOBG_SRC_CPOCOMMON_ENVIRONMENTFLAG_H_
#define CPOBG_SRC_CPOCOMMON_ENVIRONMENTFLAG_H_

//#include "cpocommon_export.h"
#include <string>
#include <memory>

namespace std {
class mutex;
}

/** Extract an environment variable one time and cache.
 * The flag is considered set if it is defined to any non-empty value.
 */
class /*CPOCOMMON_EXPORT*/ EnvironmentFlag {
 public:
    /** Define the flag name.
     *
     * @param name The name to read from the environment.
     */
    EnvironmentFlag(const std::string &name);

    /// Allow forward declarations
    ~EnvironmentFlag();

    /** Get the raw value.
     *
     * This function is thread safe.
     * @return The environment data.
     */
    const std::string & value();

    /** Get the flag state.
     *
     * This function is thread safe.
     * @return True if the flag is set.
     */
    bool operator()();

 private:
    /** Read and cache the value.
     * @post The #_value is set.
     */
    void readValue();

    std::string _name;
    std::unique_ptr<std::mutex> _mutex;
    std::unique_ptr<std::string> _value;
};

#endif /* CPOBG_SRC_CPOCOMMON_ENVIRONMENTFLAG_H_ */
