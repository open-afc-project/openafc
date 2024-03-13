#include "../EnvironmentFlag.h"

class TestEnvironmentFlag : public testing::Test {
    virtual void SetUp() override {
        ::qputenv("TEST_ENVIRONMENT_FLAG", QByteArray());
    }
};

TEST_F(TestEnvironmentFlag, testReadEmpty){
    EnvironmentFlag flag("TEST_ENVIRONMENT_FLAG");
    EXPECT_FALSE(flag());
    EXPECT_EQ(std::string(), flag.value());
}

TEST_F(TestEnvironmentFlag, testReadNonempty){
    ::qputenv("TEST_ENVIRONMENT_FLAG", QByteArray("0"));
    EnvironmentFlag flag("TEST_ENVIRONMENT_FLAG");
    EXPECT_TRUE(flag());
    EXPECT_EQ(std::string("0"), flag.value());
}

TEST_F(TestEnvironmentFlag, testChangeValue){
    EnvironmentFlag flag("TEST_ENVIRONMENT_FLAG");
    EXPECT_FALSE(flag());
    EXPECT_EQ(std::string(), flag.value());

    ::qputenv("TEST_ENVIRONMENT_FLAG", QByteArray("0"));
    EXPECT_FALSE(flag());
    EXPECT_EQ(std::string(), flag.value());
}
