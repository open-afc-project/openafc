#include "LoggingScopedSink.h"
#include <boost/log/core.hpp>

Logging::ScopedSink::ScopedSink(const boost::shared_ptr<boost::log::sinks::sink> &sinkVal) : sink(sinkVal)
{
    boost::shared_ptr<boost::log::core> core = boost::log::core::get();
    core->set_logging_enabled(true);
    core->add_sink(sink);
}

Logging::ScopedSink::~ScopedSink()
{
    boost::shared_ptr<boost::log::core> core = boost::log::core::get();
    core->remove_sink(sink);
}
