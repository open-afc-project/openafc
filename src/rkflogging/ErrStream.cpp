// Copyright (C) 2017 RKF Engineering Solutions, LLC

#include "ErrStream.h"

ErrStream::ErrStream(){}

ErrStream::operator QString () const{
    return QString::fromStdString(_str.str());
}
