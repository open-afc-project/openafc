// 

#ifndef LOGCOLLECTOR_H_
#define LOGCOLLECTOR_H_

#include "rkflogging/LoggingSeverityLevel.h"
#include <boost/log/sinks/basic_sink_backend.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <deque>
#include <QString>
#include <QRegExp>

class LogCollector : public boost::log::sinks::basic_sink_backend<
    boost::log::sinks::combine_requirements<
        boost::log::sinks::synchronized_feeding
    >::type> {
public:
    /// Type for instances of this class
    typedef boost::shared_ptr<LogCollector> PtrT;

    static PtrT overrideSinks();

    /// A single logged entry
    struct Entry{
        Entry(const boost::log::record_view &rec);

        /// Exact log severity level
        Logging::severity_level level;
        /// Channel for the entry
        QString channel;
        /// Exact message
        QString message;
    };

    struct Match{
        Match(){}

        Match & level_is(Logging::severity_level filter){
            level_eq.reset(new Logging::severity_level(filter));
            return *this;
        }

        Match & channel_is(const QString &filter){
            channel_eq.reset(new QString(filter));
            return *this;
        }

        Match & message_like(const QString &filter){
            message_re.reset(new QRegExp(filter));
            return *this;
        }

        /// Require exact severity level
        std::unique_ptr<Logging::severity_level> level_eq;
        /// Require exact channel name
        std::unique_ptr<QString> channel_eq;
        /// Require message pattern
        std::unique_ptr<QRegExp> message_re;
    };

    // The constructor initializes the internal data
    explicit LogCollector(){}

    /// Reset the collected logs
    void reset(){
        entries.clear();
    }

    // The function consumes the log records that come from the frontend
    void consume(const boost::log::record_view &rec){
        entries.push_back(Entry(rec));
    }

    std::deque<Entry> entries;
};

/** Compare a match filter against an actual entry.
 *
 * @param entry The entry to match.
 * @param match The match filter
 * @return True if the entry passes the filter.
 */
bool operator==(const LogCollector::Entry &entry, const LogCollector::Match &match);
/// Commutative overload
bool operator==(const LogCollector::Match &match, const LogCollector::Entry &entry){
    return (entry == match);
}

std::ostream & operator<<(std::ostream &out, const LogCollector::Entry &entry);
std::ostream & operator<<(std::ostream &out, const LogCollector::Match &match);

#endif /* LOGCOLLECTOR_H_ */
