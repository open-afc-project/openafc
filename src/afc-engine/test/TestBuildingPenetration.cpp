
#include "../AfcManager.h"
#include "testcommon/UnitTestHelpers.h"
#include "testcommon/ErrorStats.h"
#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>

class TestBuildingPenetration : public testing::Test {
protected:
    /// The model itself
    AfcManager *_afc;

public:
    TestBuildingPenetration() {
        _afc = new AfcManager();
    }

    virtual void SetUp() override{
        // ASSERT_GT(_ituXpdRows.size(), 10);
    }

    ~TestBuildingPenetration(){
        delete _afc;
    }
};

/// @test Verify that the model is valid for valid inputs
TEST_F(TestBuildingPenetration, fixedValid){
    const int trialCount = 100;
    double fixedValue = 12.345;
    _afc->setFixedBuildingLossFlag(true);
    _afc->setFixedBuildingLossValue(fixedValue);
    CConst::BuildingTypeEnum buildingType = CConst::traditionalBuildingType;
    double frequency = 6.0e9;
    std::string buildingPenetrationModelStr;
    double buildingPenetrationCDF;
    bool fixedProbFlag = false;

    for(int ix = 0; ix < trialCount; ++ix) {
        double elevationAngleDeg = (double) (((ix*183) % 360) - 180);

        double buildingPenetrationLoss = _afc->computeBuildingPenetration(buildingType, elevationAngleDeg, frequency, buildingPenetrationModelStr, buildingPenetrationCDF, fixedProbFlag);

        ASSERT_NEAR(
            buildingPenetrationLoss,
            fixedValue,
            1.0e-6
        );
    }
}

