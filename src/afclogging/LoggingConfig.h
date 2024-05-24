//

#ifndef LOGGING_CONFIG_H_
#define LOGGING_CONFIG_H_

#include "Logging.h"
#include <boost/regex.hpp>
#include <boost/log/detail/default_attribute_names.hpp>
#include <boost/log/attributes/current_thread_id.hpp>
#include <boost/log/attributes/time_traits.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/sinks/syslog_constants.hpp>
#include <boost/log/expressions/keyword.hpp>
#include <boost/log/expressions/formatter.hpp>

namespace Logging
{

/// Register timestamp attribute
BOOST_LOG_ATTRIBUTE_KEYWORD(
	utctimestamp, "UtcTimeStamp", boost::log::attributes::utc_time_traits::time_type)
/// Register severity attribute
BOOST_LOG_ATTRIBUTE_KEYWORD(
	severity, boost::log::aux::default_attribute_names::severity(), severity_level)
/// Register channel type attribute
BOOST_LOG_ATTRIBUTE_KEYWORD(
	channel, boost::log::aux::default_attribute_names::channel(), channel_name_type)
/// Register thread ID type attribute
BOOST_LOG_ATTRIBUTE_KEYWORD(thread_id, boost::log::aux::default_attribute_names::thread_id(),
	boost::log::attributes::current_thread_id::value_type)
/// Register message-text attribute type
using message_type = boost::log::expressions::smessage_type;
/// Register message-text attribute
const message_type message = {};

/** Encapsulate configuration of a log-level filter value.
 */
class Filter
{
	public:
		struct NameError : public std::logic_error {
				NameError(const std::string &msg);
		};

		/// Default filter is at minimum level (i.e. allow all messages)
		Filter();

		/** Extract filter level from a configuration string.
		 * @param val The value to take from.
		 * This is either a Net-SNMP-style level string prefix:
		 *  - critical
		 *  - error
		 *  - warning
		 *  - info
		 *  - debug
		 * For example, the values "E", "ERR", and "ERROR" all match error level.
		 * @throw NameError if the value is not a log level specifier.
		 */
		void setLevel(const std::string &val);

		/// The least-severe level allowed by the filter
		severity_level leastLevel;
		/// Included patterns for channel names
		std::list<boost::regex> channelInclude;
		/// Excluded patterns for channel names
		std::list<boost::regex> channelExclude;
};

/// Individual out-stream configuration
struct OStreamConfig {
		/// Default configuration
		OStreamConfig()
		{
		}

		/// Original file name associated with #stream (if applicable)
		std::string fileName;
		/// The stream to write to
		std::shared_ptr<std::ostream> stream;
		/// True if the stream is flushed after each log record
		bool autoFlush = false;
};

#if !defined(BOOST_LOG_WITHOUT_SYSLOG)
/// Syslog-specific configuration
struct SyslogConfig {
		/// Default configuration
		SyslogConfig();

		/// The particular 'facility' to log as
		boost::log::sinks::syslog::facility facility;
		/// The local identity to use for logging
		std::string identity;
};
#endif

#if !defined(BOOST_LOG_WITHOUT_EVENT_LOG)
/// Windows event log-specific configuration
struct WinlogConfig {
		/// Default configuration
		WinlogConfig();

		/// The local identity to use for logging
		std::string identity;
};
#endif

/// Logging configuration details
struct Config {
		/** Default configuration.
		 * Logging is sent to @c stderr only.
		 */
		Config();

		/// If true, output will be sent to @c stdout.
		bool useStdOut;
		/// If true, output will be sent to @c stderr.
		bool useStdErr;
#if !defined(BOOST_LOG_WITHOUT_SYSLOG)
		/// If true and #syslogConfig is set, output will be sent to @c syslog facility.
		bool useSyslog;
		/// Configuration to use iff #useSyslog is true
		std::shared_ptr<SyslogConfig> syslogConfig;
#endif
#if !defined(BOOST_LOG_WITHOUT_EVENT_LOG)
		/// If true and #winlogConfig is set, output will be sent to windows event log.
		bool useWinlog;
		/// Configuration to use iff #useWinlog is true
		std::shared_ptr<WinlogConfig> winlogConfig;
#endif
		/// If non-null, output will be appended to this stream
		std::shared_ptr<OStreamConfig> useStream;

		/// Filter for log events
		Filter filter;
};

/** Get the record formatter used for text sinks.
 *
 * @return The formatter object
 */
const boost::log::formatter &getTextFormatter();

/** Get the current running configuration.
 *
 * @return Current config content.s
 */
Config currentConfig();

/** Initialize the default appenders.
 * @param config The new logging configuration.
 */
void initialize(const Config &config = Config());

} // End namespace

#endif /* LOGGING_CONFIG_H_ */
