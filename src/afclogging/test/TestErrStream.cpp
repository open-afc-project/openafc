//

#include "../ErrStream.h"
#include <iomanip>

TEST(TestErrStream, testSimpleOutput)
{
	ErrStream str;
	str << "test";

	ASSERT_EQ(std::string("test"), std::string(str));
	ASSERT_EQ(QString("test"), QString(str));
}

TEST(TestErrStream, testFormattedOutput)
{
	ErrStream str;
	str << double(3.45);

	ASSERT_EQ(std::string("3.45"), std::string(str));
	ASSERT_EQ(QString("3.45"), QString(str));
}

TEST(TestErrStream, testStreamFunc)
{
	ErrStream str;
	str << "test" << std::flush;

	ASSERT_EQ(std::string("test"), std::string(str));
	ASSERT_EQ(QString("test"), QString(str));
}

TEST(TestErrStream, testIoManip)
{
	ErrStream str;
	str << std::setprecision(2) << double(3.45);

	ASSERT_EQ(std::string("3.5"), std::string(str));
	ASSERT_EQ(QString("3.5"), QString(str));
}
