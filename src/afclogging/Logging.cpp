//

#include "Logging.h"

namespace logging = boost::log;

namespace
{
/// Logger for messages related to Logging namespace
LOGGER_DEFINE_GLOBAL(logger, "Logging");

}

Logging::logger_mt &Logging::getLoggerInstance()
{
	return logger::get();
}

void Logging::flush()
{
	boost::shared_ptr<logging::core> core = logging::core::get();
	core->flush();
}
