// Copyright (C) 2017 RKF Engineering Solutions, LLC

#ifndef LOGGING_SEVERITY_LEVEL_H
#define LOGGING_SEVERITY_LEVEL_H

namespace Logging{

/** Levels of severity for logging messages.
 * Associated numbers increase with increasing severity.
 */
enum severity_level{
    LOG_DEBUG =  0,//!< DEBUG
    LOG_INFO  = 10,//!< INFO
    LOG_WARN  = 20,//!< WARN
    LOG_ERROR = 30,//!< ERROR
    LOG_CRIT  = 40 //!< CRIT
};

} // End namespace Logging

#endif /* LOGGING_SEVERITY_LEVEL_H */
