// Copyright (C) 2017 RKF Engineering Solutions, LLC

#include "EnvironmentFlag.h"
#include <QByteArray>
#include <qglobal.h>
#include <mutex>

EnvironmentFlag::EnvironmentFlag(const std::string &name)
  : _name(name) {
    _mutex.reset(new std::mutex());
}

EnvironmentFlag::~EnvironmentFlag() {}

const std::string & EnvironmentFlag::value() {
    readValue();
    return *_value;
}

bool EnvironmentFlag::operator()() {
    readValue();
    return !(_value->empty());
}

void EnvironmentFlag::readValue() {
    std::lock_guard<std::mutex> lock(*_mutex);
    if(!_value){
        const auto val = ::qgetenv(_name.data());
        _value.reset(new std::string(val.toStdString()));
    }
}
