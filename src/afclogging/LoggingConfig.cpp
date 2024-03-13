// 

#include "LoggingConfig.h"
#include <boost/log/core.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sinks/syslog_backend.hpp>
#include <boost/log/sinks/event_log_backend.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/attr.hpp>
#include <boost/log/expressions/predicates/matches.hpp>
#include <boost/log/expressions/formatters/stream.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/support/regex.hpp>
#include <iomanip>
#include <ostream>

//allows for backwards compatability with older version of Boost.
#if BOOST_VERSION >= 105600
#include <boost/core/null_deleter.hpp>
#define BOOST_NULL_DELETER boost::null_deleter()
#else
#include <boost/log/utility/empty_deleter.hpp>
#define BOOST_NULL_DELETER boost::log::empty_deleter()
#endif

namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

namespace{
typedef std::pair<Logging::severity_level, std::string> LevelName;
typedef std::map<Logging::severity_level, std::string> LevelMap;

/// Fixed list of names for all severities
const LevelMap levelNames({
    {Logging::LOG_DEBUG, "debug"},
    {Logging::LOG_INFO, "info"},
    {Logging::LOG_WARN, "warning"},
    {Logging::LOG_ERROR, "error"},
    {Logging::LOG_CRIT, "critical"},
});

/** Compare strings for prefix subset.
 * @param ref The long string to compare.
 * @param pre The short string to check for.
 * @return True if the @c ref string begins with @c pre string.
 */
bool startswith(const std::string &ref, const std::string &pre){
    if(pre.size() > ref.size()){
        return false;
    }
    // true if there is at least one matching character
    return (std::mismatch(pre.begin(), pre.end(), ref.begin()).second != ref.begin());
}

/** A combination of filter conditions to be checked for each message.
 */
class AttrFilterSet{
public:
    /// Boost intrinsic functional type
    using Filter = boost::log::aux::light_function< bool (boost::log::attribute_value_set const&) >;

    AttrFilterSet() {
        _rootChannel = Logging::getLoggerInstance().channel();
    }

    /** Apply the filtering to a message.
     *
     * @param attrs The message attributes.
     * @return True if all conditions are true.
     */
    bool operator()(boost::log::attribute_value_set const &attrs) const{
        { // show the root logger channel unconditionally
            const auto chanIt = attrs.find(Logging::channel_type::get_name());
            if (chanIt != attrs.end()) {
                if (chanIt->second.extract<std::string>() == _rootChannel) {
                    return true;
                }
            }
        }
        for(const auto &part : conditions){
            if(!part(attrs)){
                return false;
            }
        }
        return true;
    }

    /// The actual filter conditions to check
    std::vector<Filter> conditions;

private:
    std::string _rootChannel;
};

/// Unchanging text formatter
const boost::log::formatter textFormatter = (
    expr::stream
        // Include a timestamp for console output
        << expr::format_date_time<boost::posix_time::ptime>(Logging::utctimestamp_type::get_name(), "%Y-%m-%d %H:%M:%S.%fZ") << " "
        << "TH:" << Logging::thread_id << " "
        << "<" << Logging::severity << "> "
        << Logging::channel << ": "
        << Logging::message
);
/// Initial #curConfig has been set
bool hasConfig = false;
/// Copy of running configuration
Logging::Config curConfig;
/// Boost version of #curConfig.useStream
boost::shared_ptr<std::ostream> extRef;
}

std::ostream & Logging::operator<<(std::ostream &stream, const severity_level &val){
    stream << levelNames.at(val);
    return stream;
}

Logging::Filter::NameError::NameError(const std::string &msg)
: logic_error(msg){}


Logging::Filter::Filter()
 : leastLevel(Logging::LOG_DEBUG){}

void Logging::Filter::setLevel(const std::string &val){
    LevelMap found = levelNames;
    for(auto it = found.begin(); it != found.end(); ){
        // first-byte mismatch indicates no-match
        if(!startswith(it->second, val)){
            it = found.erase(it);
        }
        else{
            ++it;
        }
    }

    if(found.empty()){
        throw NameError("Invalid log filter \"" + val + "\"");
    }
    if(found.size() > 1){
        throw NameError("Non-unique log filter \"" + val + "\"");
    }
    leastLevel = found.begin()->first;
}

#if !defined(BOOST_LOG_WITHOUT_SYSLOG)
Logging::SyslogConfig::SyslogConfig()
  : facility(sinks::syslog::user){}
#endif

#if !defined(BOOST_LOG_WITHOUT_EVENT_LOG)
Logging::WinlogConfig::WinlogConfig(){}
#endif

Logging::Config::Config()
  : useStdOut(false), useStdErr(true){
#if !defined(BOOST_LOG_WITHOUT_SYSLOG)
    useSyslog = false;
#endif
#if !defined(BOOST_LOG_WITHOUT_EVENT_LOG)
    useWinlog = false;
#endif
}

const boost::log::formatter & Logging::getTextFormatter() {
    return textFormatter;
}

Logging::Config Logging::currentConfig(){
    return curConfig;
}

void Logging::initialize(const Logging::Config &config){
    boost::shared_ptr<logging::core> core = logging::core::get();
    // clear initial state
    core->remove_all_sinks();
    int sinkCount = 0;

    // Fixed attributes
    core->add_global_attribute(Logging::utctimestamp_type::get_name(), attrs::utc_clock());
    core->add_global_attribute(Logging::thread_id_type::get_name(), attrs::current_thread_id());

    // Overall message filter for all sinks
    {
        AttrFilterSet filter;
        filter.conditions.push_back(Logging::severity >= config.filter.leastLevel);

        for(const auto &pat : config.filter.channelInclude){
            filter.conditions.push_back(expr::matches(Logging::channel, pat));
        }
        for(const auto &pat : config.filter.channelExclude){
            filter.conditions.push_back(!expr::matches(Logging::channel, pat));
        }

        core->set_filter(filter);
    }

    if(config.useStdOut || config.useStdErr){
        typedef sinks::text_ostream_backend backend_t;
        typedef sinks::synchronous_sink<backend_t> sink_t;

        auto backend = boost::make_shared<backend_t>();

        // Using empty_deleter to avoid destroying global stream objects
        if(config.useStdOut){
            boost::shared_ptr<std::ostream> stream(&std::cout, BOOST_NULL_DELETER);
            backend->add_stream(stream);
        }
        if(config.useStdErr){
            boost::shared_ptr<std::ostream> stream(&std::cerr, BOOST_NULL_DELETER);
            backend->add_stream(stream);
        }

        auto sink = boost::make_shared<sink_t>(backend);
        sink->set_formatter(textFormatter);

        core->add_sink(sink);
        ++sinkCount;
    }
#if !defined(BOOST_LOG_WITHOUT_SYSLOG)
    if(config.useSyslog && config.syslogConfig){
        typedef sinks::syslog_backend backend_t;
        typedef sinks::synchronous_sink<backend_t> sink_t;

        auto backend = boost::make_shared<backend_t>(
            keywords::facility = config.syslogConfig->facility,
            keywords::ident = config.syslogConfig->identity
        );

        {
            sinks::syslog::custom_severity_mapping<severity_level> mapper("Severity");
            mapper[Logging::LOG_DEBUG] = sinks::syslog::debug;
            mapper[Logging::LOG_INFO] = sinks::syslog::info;
            mapper[Logging::LOG_WARN] = sinks::syslog::warning;
            mapper[Logging::LOG_ERROR] = sinks::syslog::error;
            mapper[Logging::LOG_CRIT] = sinks::syslog::critical;
            backend->set_severity_mapper(mapper);
        }

        auto sink = boost::make_shared<sink_t>(backend);
        // syslog includes its own time stamp
        core->add_sink(sink);
        ++sinkCount;
    }
#endif
#if !defined(BOOST_LOG_WITHOUT_EVENT_LOG)
    if(config.useWinlog && config.winlogConfig){
        typedef sinks::simple_event_log_backend backend_t;
        typedef sinks::synchronous_sink<backend_t> sink_t;
        
        auto backend = boost::make_shared<backend_t>(
            keywords::log_source = config.winlogConfig->identity
        );
            {
            sinks::event_log::custom_event_type_mapping<severity_level> mapping("Severity");
            mapping[Logging::LOG_DEBUG] = sinks::event_log::success;
            mapping[Logging::LOG_INFO] = sinks::event_log::info;
            mapping[Logging::LOG_WARN] = sinks::event_log::warning;
            mapping[Logging::LOG_ERROR] = sinks::event_log::error;
            mapping[Logging::LOG_CRIT] = sinks::event_log::error;
            backend->set_event_type_mapper(mapping);
        }
        
        auto sink = boost::make_shared<sink_t>(backend);
        sink->set_formatter(textFormatter);

        core->add_sink(sink);
        ++sinkCount;
    }
#endif
    if(config.useStream){
        typedef sinks::text_ostream_backend backend_t;
        typedef sinks::synchronous_sink<backend_t> sink_t;

        // keep the reference in #extStream but do not manage memory
        extRef.reset(config.useStream->stream.get(), BOOST_NULL_DELETER);
        auto backend = boost::make_shared<backend_t>();
        backend->add_stream(extRef);
        backend->auto_flush(config.useStream->autoFlush);

        auto sink = boost::make_shared<sink_t>(backend);
        sink->set_formatter(textFormatter);

        core->add_sink(sink);
        ++sinkCount;
    }
    else{
        extRef.reset();
    }
    
    // Zero sinks triggers default logging behavior, need to disable here
    core->set_logging_enabled(sinkCount > 0);

    if (!hasConfig || (config.filter.leastLevel != curConfig.filter.leastLevel)) {
        LOGINST_INFO(getLoggerInstance()) << "Logging at level " << config.filter.leastLevel;
    }
    curConfig = config;
    hasConfig = true;
}
