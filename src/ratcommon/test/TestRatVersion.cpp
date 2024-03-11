// 

#include "ratcommon/RatVersion.h"

TEST(TestRatVersion, identicalVersion){
    EXPECT_EQ(
        QString::fromUtf8(RAT_BUILD_VERSION_NAME),
        RatVersion::versionName()
    );
}
