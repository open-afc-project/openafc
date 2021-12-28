// 

#include "PostSet.h"
#include "rkflogging/Logging.h"

namespace {
/// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "PostSet")
}

void logPostSetError(const char *msg){
    LOGGER_ERROR(logger) << "Failed in assignment: " << (msg ? msg : "non-std::exception type");
}
