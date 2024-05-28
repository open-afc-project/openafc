//

#include "../Logging.h"
#include "../LoggingConfig.h"
#include <memory>
#include <iostream>

namespace
{
std::istream &operator>>(std::istream &stream, std::string &dest)
{
    std::string buf;
    std::getline(stream, dest);
    return stream;
}

std::shared_ptr<Logging::OStreamConfig> toStream(const std::shared_ptr<std::ostream> &stream)
{
    auto config = std::make_shared<Logging::OStreamConfig>();
    config->stream = stream;
    config->autoFlush = true;
    return config;
}

const auto failmask = std::ios_base::failbit | std::ios_base::badbit | std::ios_base::eofbit;

/// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(loggerA, "TestLoggingA")
LOGGER_DEFINE_GLOBAL(loggerB, "TestLoggingB")

}

class TestLogging : public testing::Test
{
    public:
};

TEST_F(TestLogging, testFilterDef)
{
    Logging::Filter filt;
    filt.setLevel("deb");
    ASSERT_EQ(filt.leastLevel, Logging::LOG_DEBUG);

    filt.setLevel("w");
    ASSERT_EQ(filt.leastLevel, Logging::LOG_WARN);
}

TEST_F(TestLogging, testTargets)
{
    Logging::Config conf;
    conf.useStdOut = false;
    conf.useStdErr = false;
    conf.useSyslog = false;

    Logging::initialize(conf);

    {
        Logging::Config other(conf);
        other.useStdOut = true;
        Logging::initialize(other);
    }
    {
        Logging::Config other(conf);
        other.useStdErr = true;
        Logging::initialize(other);
    }
    {
        Logging::Config other(conf);
        other.useSyslog = true;
        Logging::initialize(other);
    }
}

TEST_F(TestLogging, testIntercept)
{
    auto outbuf = std::make_shared<std::stringstream>();
    outbuf->exceptions(failmask);

    Logging::Config conf;
    conf.useStdOut = false;
    conf.useStdErr = false;
    conf.useStream = toStream(outbuf);
    Logging::initialize(conf);

    // Clear any initialize logging
    boost::log::core::get()->flush();
    outbuf->str(std::string());

    conf.filter.setLevel("error");
    Logging::initialize(conf);

    {
        boost::log::core::get()->flush();
        std::string line;
        *outbuf >> line;
        ASSERT_TRUE(boost::regex_match(line,
            boost::regex(".* <info> Logging: Logging at level "
                         "error")))
            << "Line:" << line;

        ASSERT_THROW(*outbuf >> line, std::exception);
        outbuf->str(std::string());
    }
}

TEST_F(TestLogging, testFilterLevel)
{
    auto outbuf = std::make_shared<std::stringstream>();
    outbuf->exceptions(failmask);

    Logging::Config conf;
    conf.filter.setLevel("warning");
    conf.useStdOut = false;
    conf.useStdErr = false;
    conf.useStream = toStream(outbuf);
    Logging::initialize(conf);

    // Clear any initialize logging
    boost::log::core::get()->flush();
    outbuf->str(std::string());

    LOGGER_INFO(loggerA) << "hi there";
    {
        boost::log::core::get()->flush();
        ASSERT_EQ(std::string(), outbuf->str());
    }

    LOGGER_WARN(loggerA) << "hi there";
    {
        boost::log::core::get()->flush();
        std::string line;
        *outbuf >> line;
        ASSERT_TRUE(boost::regex_match(line, boost::regex(".* <warning> TestLoggingA: hi there"))) << "Line:" << line;

        ASSERT_THROW(*outbuf >> line, std::exception);
        outbuf->str(std::string());
    }
}

TEST_F(TestLogging, testFilterChannelInclude)
{
    auto outbuf = std::make_shared<std::stringstream>();
    outbuf->exceptions(failmask);

    Logging::Config conf;
    conf.filter.channelInclude.push_back(boost::regex("TestLoggingA"));
    conf.useStdOut = false;
    conf.useStdErr = false;
    conf.useStream = toStream(outbuf);
    Logging::initialize(conf);

    // Clear any initialize logging
    boost::log::core::get()->flush();
    outbuf->str(std::string());

    LOGGER_WARN(loggerA) << "hi there";
    LOGGER_WARN(loggerB) << "oh hi";
    {
        boost::log::core::get()->flush();
        std::string line;
        *outbuf >> line;
        ASSERT_TRUE(boost::regex_match(line, boost::regex(".* <warning> TestLoggingA: hi there"))) << "Line:" << line;

        ASSERT_THROW(*outbuf >> line, std::exception);
        outbuf->str(std::string());
    }
}

TEST_F(TestLogging, testFilterChannelExclude)
{
    auto outbuf = std::make_shared<std::stringstream>();
    outbuf->exceptions(failmask);

    Logging::Config conf;
    conf.filter.channelExclude.push_back(boost::regex(".*LoggingA"));
    conf.useStdOut = false;
    conf.useStdErr = false;
    conf.useStream = toStream(outbuf);
    Logging::initialize(conf);

    // Clear any initialize logging
    boost::log::core::get()->flush();
    outbuf->str(std::string());

    LOGGER_WARN(loggerA) << "hi there";
    LOGGER_WARN(loggerB) << "oh hi";
    {
        boost::log::core::get()->flush();
        std::string line;
        *outbuf >> line;
        ASSERT_TRUE(boost::regex_match(line, boost::regex(".* <warning> TestLoggingB: oh hi"))) << "Line:" << line;

        ASSERT_THROW(*outbuf >> line, std::exception);
        outbuf->str(std::string());
    }
}
