// 

#include "ratcommon/RatVersion.h"
#include "rkfunittest/UnitTestHelpers.h"
#include "rkfunittest/GtestShim.h"

TEST(TestRatVersion, identicalVersion){
    EXPECT_EQ(
        QString::fromUtf8(RAT_BUILD_VERSION_NAME),
        RatVersion::versionName()
    );
}
