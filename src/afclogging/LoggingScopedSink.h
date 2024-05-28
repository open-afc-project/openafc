
#ifndef SRC_AFCLOGGING_LOGGINGSCOPEDSINK_H_
#define SRC_AFCLOGGING_LOGGINGSCOPEDSINK_H_

#include <boost/log/sinks.hpp>
#include <boost/shared_ptr.hpp>

namespace Logging
{

/** An RAII class to manage the lifetime of a boost::log sink.
 */
class ScopedSink
{
    public:
        /** Attach the sink.
         *
         * @param sink The sink to manage:
         */
        ScopedSink(const boost::shared_ptr<boost::log::sinks::sink> &sink);

        /** Detach the sink.
         */
        ~ScopedSink();

        /// The managed sink
        boost::shared_ptr<boost::log::sinks::sink> sink;
};

} // End namespace

#endif /* SRC_AFCLOGGING_LOGGINGSCOPEDSINK_H_ */
