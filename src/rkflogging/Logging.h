// Copyright (C) 2017 RKF Engineering Solutions, LLC

#ifndef LOGGING_H_
#define LOGGING_H_

#include "LoggingSeverityLevel.h"
#include "QtStream.h"
#include <ostream>
#include <stdexcept>
#include <boost/regex.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_feature.hpp>

/** Utility functions for interacting with boost::log library.
 *
 * Typical thread-safe use of this library is in the example:
 * @code
namespace {
/// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "logname")
}
...
void func(){
    LOGGER_INFO(logger) << "some message " << with_data;
}
 * @endcode
 */
namespace Logging{
/// the type of the channel name
using channel_name_type = std::string;

/// Convert severity to text
std::ostream & operator<<(std::ostream &stream, const severity_level &val);

/// Convenience name for single-thread logger class
typedef boost::log::sources::severity_channel_logger<
    severity_level, // the type of the severity level
    channel_name_type // the type of the channel name
> logger_st;

/// Convenience name for multi-thread-safe logger class
typedef boost::log::sources::severity_channel_logger_mt<
    severity_level, // the type of the severity level
    channel_name_type // the type of the channel name
> logger_mt;

/** Access the root logger, which always passes the record filter.
 *
 * @return The root logger instance with channel name "Logging".
 */
logger_mt & getLoggerInstance();

/** Flush all current logging sinks.
 * @note Logging is inhibited during the flush.
 */
void flush();

/** An RAII class to call Logging::flush() in its destructor.
 */
class Flusher{
public:
    /// Do nothing
    Flusher(){}
    /// Flush the logs
    ~Flusher(){
        Logging::flush();
    }
};

} // End namespace

/** Define a global thread-safe logger instance.
 *
 * @param tag_name The object name used to identify the logger.
 * @param chan_name The channel name to include with log messages.
 * @sa LOG_SEV, LOGGER_DEBUG, LOGGER_INFO, LOGGER_WARN, LOGGER_ERROR
 */
#define LOGGER_DEFINE_GLOBAL(tag_name, chan_name) \
    BOOST_LOG_INLINE_GLOBAL_LOGGER_INIT(tag_name, Logging::logger_mt) \
    { \
        namespace keywords = boost::log::keywords; \
        return Logging::logger_mt(keywords::channel = chan_name); \
    }

/** Log at Logging::LOG_DEBUG level.
 * @param inst The logger instance to write to.
 */
#define LOGINST_DEBUG(inst) \
    BOOST_LOG_SEV(inst, Logging::LOG_DEBUG)

/** Log at Logging::LOG_INFO level.
 * @param inst The logger instance to write to.
 */
#define LOGINST_INFO(inst) \
    BOOST_LOG_SEV(inst, Logging::LOG_INFO)

/** Log at Logging::LOG_WARN level.
 * @param inst The logger instance to write to.
 */
#define LOGINST_WARN(inst) \
    BOOST_LOG_SEV(inst, Logging::LOG_WARN)

/** Log at Logging::ERROR level.
 * @param inst The logger instance to write to.
 */
#define LOGINST_ERROR(inst) \
    BOOST_LOG_SEV(inst, Logging::LOG_ERROR)

/** Log at Logging::LOG_CRIT level.
 * @param inst The logger instance to write to.
 */
#define LOGINST_CRIT(inst) \
    BOOST_LOG_SEV(inst, Logging::LOG_CRIT)

/** Log at arbitrary severity level.
 * @param tag_name The object name used to identify the logger.
 * @param severity The specific severity level.
 */
#define LOG_SEV(tag_name, severity) \
    BOOST_LOG_SEV(tag_name::get(), severity)

/** Log at Logging::DEBUG level.
 * @param tag_name The object name used to identify the logger.
 */
#define LOGGER_DEBUG(tag_name) \
    LOG_SEV(tag_name, Logging::LOG_DEBUG)

/** Log at Logging::INFO level.
 * @param tag_name The object name used to identify the logger.
 */
#define LOGGER_INFO(tag_name) \
    LOG_SEV(tag_name, Logging::LOG_INFO)

/** Log at Logging::WARN level.
 * @param tag_name The object name used to identify the logger.
 */
#define LOGGER_WARN(tag_name) \
    LOG_SEV(tag_name, Logging::LOG_WARN)

/** Log at Logging::ERROR level.
 * @param tag_name The object name used to identify the logger.
 */
#define LOGGER_ERROR(tag_name) \
    LOG_SEV(tag_name, Logging::LOG_ERROR)

/** Log at Logging::CRIT level.
 * @param tag_name The object name used to identify the logger.
 */
#define LOGGER_CRIT(tag_name) \
    LOG_SEV(tag_name, Logging::LOG_CRIT)

#endif /* LOGGING_H_ */
