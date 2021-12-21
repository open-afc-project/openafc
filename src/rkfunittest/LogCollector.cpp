// Copyright (C) 2017 RKF Engineering Solutions, LLC

#include "LogCollector.h"
#include <boost/log/core/core.hpp>

namespace sinks = boost::log::sinks;

LogCollector::PtrT LogCollector::overrideSinks(){
    boost::shared_ptr<boost::log::core> core = boost::log::core::get();
    // clear initial state
    core->remove_all_sinks();

    //core->set_filter(
    //);

    typedef ::LogCollector backend_t;
    typedef sinks::synchronous_sink<backend_t> sink_t;

    auto backend = boost::make_shared<backend_t>();
    auto sink = boost::make_shared<sink_t>(backend);
    core->add_sink(sink);

    return backend;
}

LogCollector::Entry::Entry(const boost::log::record_view &rec){
    const auto attrs = rec.attribute_values();
    level = attrs["Severity"].extract_or_throw<Logging::severity_level>();
    channel = QString::fromStdString(attrs["Channel"].extract_or_throw<std::string>());
    message = QString::fromStdString(attrs["Message"].extract_or_throw<std::string>());
}

bool operator==(const LogCollector::Entry &entry, const LogCollector::Match &match){
    if(match.level_eq){
        if(*match.level_eq != entry.level){
            return false;
        }
    }
    if(match.channel_eq){
        if(*match.channel_eq != entry.channel){
            return false;
        }
    }
    if(match.message_re){
        if(!match.message_re->exactMatch(entry.message)){
            return false;
        }
    }
    return true;
}

std::ostream & operator<<(std::ostream &out, const LogCollector::Entry &entry){
    out
        << "Channel=\"" << entry.channel.toStdString() << "\""
        << " Severity=" << entry.level
        << " Message=\"" << entry.message.toStdString() << "\"";
    return out;
}

std::ostream & operator<<(std::ostream &out, const LogCollector::Match &match){
    if(match.channel_eq){
        out << " Channel=\"" << match.channel_eq->toStdString() << "\"";
    }
    if(match.level_eq){
        out << " Severity=" << *match.level_eq;
    }
    if(match.message_re){
        out << " Message~\"" << match.message_re->pattern().toStdString() << "\"";
    }
    return out;
}
