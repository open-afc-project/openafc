#include <algorithm>
#include <cstdio>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <sstream>
#include <unordered_map>
// #include <variant>

#include "gtest/gtest.h"

// #include "IPDRMathUtils.hpp"
#include "../readITUFiles.hpp"

#define TOLERANCE 1e-6

class IPDRMathUtilTesting : public ::testing::Test {
 protected:
  IPDRMathUtilTesting() {}

  static void SetUpTestCase() {}

  static void TearDownTestCase() {}

  void TearDown() override { deleteFiles(); }

  static const int64_t SURFACE_REFRACTIVITY_ROWS = 121;
  static const int64_t SURFACE_REFRACTIVITY_COLS = 241;
  static const int64_t RADIO_CLIMATE_ROWS = 360;
  static const int64_t RADIO_CLIMATE_COLS = 720;

  const std::string RADIO_FILE = "radioClimate.txt";
  const std::string SURF_FILE = "surfaceRefractivity.txt";

  std::vector<int> constructEmptyRadioLine() {
    return std::vector<int>(RADIO_CLIMATE_COLS, 0);
  }
  std::vector<int>
  constructRadioClimateLine(std::unordered_map<int, int> insertRowMap) {
    // construct the empty line
    std::vector<int> res = constructEmptyRadioLine();
    // if there are specific values to be inserted at specific columns insert
    // them
    for (auto it : insertRowMap) {
      int64_t col = it.first;
      int64_t val = it.second;
      res[col] = val;
    }
    return res;
  }

  std::vector<std::vector<int>> constructEmptyRadioClimate() {
    return std::vector<std::vector<int>>(
        RADIO_CLIMATE_ROWS, std::vector<int>(RADIO_CLIMATE_COLS, 0));
  }

  std::vector<std::vector<int>>
  constructRadioClimate(std::unordered_map<int, std::vector<int>> insertMap) {
    // construct empty 2d vector
    std::vector<std::vector<int>> res(RADIO_CLIMATE_ROWS,
                                      std::vector<int>(RADIO_CLIMATE_COLS, 0));
    // if there are lines to be replaced, insert them at the appropriate row
    for (auto it : insertMap) {
      int64_t row = it.first;
      std::vector<int> line = it.second;
      res[row] = line;
    }
    return res;
  }

  void outputRadioToFile(std::string filename,
                         std::vector<std::vector<int>> output) {
    std::ofstream outFile;
    outFile.open(filename.c_str());
    for (auto vec : output) {
      std::stringstream ss;
      std::copy(vec.begin(), vec.end(), std::ostream_iterator<int>(ss, " "));
      outFile << ss.str() << "\n";
    }
  }

  std::vector<double> constructEmptySurfRefractLine() {
    return std::vector<double>(SURFACE_REFRACTIVITY_COLS, 0);
  }
  std::vector<double>
  constructSurfRefractLine(std::unordered_map<int, double> insertRowMap) {
    // construct the empty line
    std::vector<double> res(SURFACE_REFRACTIVITY_COLS, 0);
    // if there are specific values to be inserted at specific columns insert
    // them
    for (auto it : insertRowMap) {
      int64_t col = it.first;
      int64_t val = it.second;
      res[col] = val;
    }
    return res;
  }

  std::vector<std::vector<double>> constructEmptySurfRefract() {
    return std::vector<std::vector<double>>(
        SURFACE_REFRACTIVITY_ROWS,
        std::vector<double>(SURFACE_REFRACTIVITY_COLS, 0));
  }

  std::vector<std::vector<double>>
  constructSurfRefract(std::unordered_map<int, std::vector<double>> insertMap) {
    // construct empty 2d vector
    std::vector<std::vector<double>> res(
        RADIO_CLIMATE_ROWS, std::vector<double>(RADIO_CLIMATE_COLS, 0));
    // if there are lines to be replaced, insert them at the appropriate row
    for (auto it : insertMap) {
      int64_t row = it.first;
      std::vector<double> line = it.second;
      res[row] = line;
    }
    return res;
  }

  void outputSurfToFile(std::string filename,
                        std::vector<std::vector<double>> output) {
    std::ofstream outFile;
    outFile.open(filename.c_str());
    for (auto vec : output) {
      std::stringstream ss;
      std::copy(vec.begin(), vec.end(), std::ostream_iterator<double>(ss, " "));
      outFile << ss.str() << "\n";
    }
    outFile.close();
  }

  void deleteFiles() {
    std::ifstream rFile(RADIO_FILE);

    if (rFile.is_open()) {
      rFile.close();
      std::remove(RADIO_FILE.c_str());
    }

    std::ifstream sFile(SURF_FILE);

    if (sFile.is_open()) {
      sFile.close();
      std::remove(SURF_FILE.c_str());
    }
  }
};

TEST_F(IPDRMathUtilTesting, readBothFiles_NoExcept) {
  outputRadioToFile(RADIO_FILE, constructEmptyRadioClimate());
  outputSurfToFile(SURF_FILE, constructEmptySurfRefract());
  EXPECT_NO_THROW(ITUDataClass(RADIO_FILE, SURF_FILE));
}

TEST_F(IPDRMathUtilTesting, getBothValues_BlankFile_0) {
  outputRadioToFile(RADIO_FILE, constructEmptyRadioClimate());
  outputSurfToFile(SURF_FILE, constructEmptySurfRefract());
  ITUDataClass i(RADIO_FILE, SURF_FILE);

  const int64_t EXPECTED_RADIO_CLIMATE = 0;
  const int64_t ACTUAL_RADIO_CLIMATE = i.getRadioClimateValue(0, 0);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);

  const int64_t EXPECTED_SURFACE_REFRACTIVITY = 0;
  const int64_t ACTUAL_SURFACE_REFRACTIVITY =
      i.getSurfaceRefractivityValue(0, 0);
  EXPECT_EQ(EXPECTED_SURFACE_REFRACTIVITY, ACTUAL_SURFACE_REFRACTIVITY);
}

// BEGIN TESTING RADIO CLIMATE
TEST_F(IPDRMathUtilTesting, test_getRadio_Exception_Latitude_LessThan) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -90.1;
  double radio_lon = 0;
  std::string EXPECTED_EXCEPT_MSG = "Latitude outside [-90.0,90.0]!";

  EXPECT_THROW(
      {
        try {
          i.getRadioClimateValue(radio_lat, radio_lon);
        } catch (const std::range_error &e) {
          EXPECT_EQ(EXPECTED_EXCEPT_MSG, e.what());
          throw;
        }
      },
      std::range_error);
}
TEST_F(IPDRMathUtilTesting, test_getRadio_Exception_Latitude_GreaterThan) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 90.1;
  double radio_lon = 0;
  std::string EXPECTED_EXCEPT_MSG = "Latitude outside [-90.0,90.0]!";

  EXPECT_THROW(
      {
        try {
          i.getRadioClimateValue(radio_lat, radio_lon);
        } catch (const std::range_error &e) {
          EXPECT_EQ(EXPECTED_EXCEPT_MSG, e.what());
          throw;
        }
      },
      std::range_error);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_Exception_Longitude_LessThan) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 0;
  double radio_lon = -180.1;
  std::string EXPECTED_EXCEPT_MSG = "Longitude outside [-180.0,360.0]!";

  EXPECT_THROW(
      {
        try {
          i.getRadioClimateValue(radio_lat, radio_lon);
        } catch (const std::range_error &e) {
          EXPECT_EQ(EXPECTED_EXCEPT_MSG, e.what());
          throw;
        }
      },
      std::range_error);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_Exception_Longitude_GreaterThan) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 0;
  double radio_lon = 360.1;
  std::string EXPECTED_EXCEPT_MSG = "Longitude outside [-180.0,360.0]!";

  EXPECT_THROW(
      {
        try {
          i.getRadioClimateValue(radio_lat, radio_lon);
        } catch (const std::range_error &e) {
          EXPECT_EQ(EXPECTED_EXCEPT_MSG, e.what());
          throw;
        }
      },
      std::range_error);
}
TEST_F(IPDRMathUtilTesting, test_getRadio_row0_col0) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  // Radio's first point is lat = 89.75, lon = -179.75
  double radio_lat = 89.75;
  double radio_lon = -179.75;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row0_lastCol) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][RADIO_CLIMATE_COLS - 1] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][SURFACE_REFRACTIVITY_COLS - 1] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  // Radio's first point is lat = 89.75, lon = -179.75
  double radio_lat = 89.75;
  double radio_lon = 179.75;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_lastRow_col0) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 1][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][0] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  // Radio's first point is lat = 89.75, lon = -179.75
  double radio_lat = -89.75;
  double radio_lon = -179.75;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row0_col0_round75) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  // radio[0][RADIO_CLIMATE_COLS - 1] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  // These points are at the limits, the very beginning) of what's supported,
  // so we expect it to round down properly and still choose the closest
  double radio_lat = 90;
  double radio_lon = 180;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row0_col0_) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 90;
  double radio_lon = -180;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row0_col0_middle_round75) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 89.5001;
  double radio_lon = -179.5001;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row1_col1_middle_round25) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[1][1] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[1][1] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 89.5;
  double radio_lon = -179.5;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row1_col1_close2middle_round25) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[1][1] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[1][1] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 89.4999;
  double radio_lon = -179.4999;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row1_col1_exactPoint_round25) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[1][1] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[1][1] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 89.25;
  double radio_lon = -179.25;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row2_col2_wholeLat_wholeLon_round25) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[2][2] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[2][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 89;
  double radio_lon = -179;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting,
       test_getRadio_rowLast_col0_wholeLat_wholeLon_round75) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 1][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][0] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -90;
  double radio_lon = -180;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_rowLast_col0_exactPoint_round75) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 1][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][0] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -89.75;
  double radio_lon = -179.75;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_rowLast_col0_nearMid_round75) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 1][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -89.5001;
  double radio_lon = -179.5001;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_rowSecondLast_col1_middle_round25) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 1][1] = 1;
  // radio[RADIO_CLIMATE_ROWS - 2][1] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -89.5;
  double radio_lon = -179.5;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_rowSecondLast_col1_onPoint_round25) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 2][1] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -89.25;
  double radio_lon = -179.25;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_rowSecondLast_col1_near0_round25) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 2][1] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -89.001;
  double radio_lon = -179.001;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_rowSecondLast_col1_on0_round75) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 2][2] = 1;
  // radio[RADIO_CLIMATE_ROWS - 3][2] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -89;
  double radio_lon = -179;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row0_colLast_on0_round75) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  // radio[0][RADIO_CLIMATE_COLS - 1] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 90;
  double radio_lon = 180;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row0_colLast_onPoint_round75) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][RADIO_CLIMATE_COLS - 1] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 89.75;
  double radio_lon = 179.75;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row0_colLast_nearMid_round75) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][RADIO_CLIMATE_COLS - 1] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 89.5001;
  double radio_lon = 179.5001;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row1_colsecondLast_Mid_round25) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[1][RADIO_CLIMATE_COLS - 1] = 1;
  // radio[1][RADIO_CLIMATE_COLS - 2] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 89.5;
  double radio_lon = 179.5;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row1_colSecondLast_onPoint_round25) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[1][RADIO_CLIMATE_COLS - 2] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 89.25;
  double radio_lon = 179.25;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row1_colSecondLast_near0_round25) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[1][RADIO_CLIMATE_COLS - 2] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 89.001;
  double radio_lon = 179.001;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_row2_colThirdLast_0_round75) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[2][RADIO_CLIMATE_COLS - 2] = 1;
  // radio[2][RADIO_CLIMATE_COLS - 3] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = 89;
  double radio_lon = 179;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_rowLast_colLast_0_round75) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 1][0] = 1;
  // radio[RADIO_CLIMATE_ROWS - 1][RADIO_CLIMATE_COLS - 1] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -90;
  double radio_lon = 180;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_rowLast_colLast_onPoint_round75) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 1][RADIO_CLIMATE_COLS - 1] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -89.75;
  double radio_lon = 179.75;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting, test_getRadio_rowLast_colLast_nearMid_round75) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 1][RADIO_CLIMATE_COLS - 1] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -89.5001;
  double radio_lon = 179.5001;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting,
       test_getRadio_rowSecondLast_colSecondLast_Mid_round25) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 1][RADIO_CLIMATE_COLS - 1] = 1;
  // radio[RADIO_CLIMATE_ROWS - 2][RADIO_CLIMATE_COLS - 2] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -89.5;
  double radio_lon = 179.5;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting,
       test_getRadio_rowSecondLast_colSecondLast_onPoint_round25) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 2][RADIO_CLIMATE_COLS - 2] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -89.25;
  double radio_lon = 179.25;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting,
       test_getRadio_rowSecondLast_colSecondLast_nearZero_round25) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 2][RADIO_CLIMATE_COLS - 2] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -89.001;
  double radio_lon = 179.001;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}

TEST_F(IPDRMathUtilTesting,
       test_getRadio_rowThirdLast_colThirdLast_Zero_round75) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[RADIO_CLIMATE_ROWS - 2][RADIO_CLIMATE_COLS - 2] = 1;
  // radio[RADIO_CLIMATE_ROWS - 3][RADIO_CLIMATE_COLS - 3] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][2] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double radio_lat = -89;
  double radio_lon = 179;
  const int64_t EXPECTED_RADIO_CLIMATE = 1;
  const int64_t ACTUAL_RADIO_CLIMATE =
      i.getRadioClimateValue(radio_lat, radio_lon);
  EXPECT_EQ(EXPECTED_RADIO_CLIMATE, ACTUAL_RADIO_CLIMATE);
}
// END TESTING RADIO CLIMATE

// BEGIN TESTING SURFACE REFRACTIVITY
// Note: Bilinear interpolation is tested separately, so we're only testing the
// values received from
//       the surface refractivity
TEST_F(IPDRMathUtilTesting, test_getSurf_row0_col0) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double surf_lat = 90;
  double surf_lon = 0;
  const double EXPECTED_SURF_REFRACT = 1;
  const double ACTUAL_SURF_REFRACT =
      i.getSurfaceRefractivityValue(surf_lat, surf_lon);
  EXPECT_EQ(EXPECTED_SURF_REFRACT, ACTUAL_SURF_REFRACT);
}

// Since the file goes from 0-360 instead of -180 to 180, if the value is
// negative and >= -360 we add 360 to map the ranges appropriately. We test if
// they give the same value.
TEST_F(IPDRMathUtilTesting, test_getSurf_row0_col0_negLon_lon_Same) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][180] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double surf_lat = 90;
  double surf_lon_neg = -90;
  double surf_lon = 270;
  const double EXPECTED_SURF_REFRACT = 1;
  const double SURF_REFRACT = i.getSurfaceRefractivityValue(surf_lat, surf_lon);
  const double SURF_REFRACT_NEG =
      i.getSurfaceRefractivityValue(surf_lat, surf_lon_neg);
  EXPECT_EQ(EXPECTED_SURF_REFRACT, SURF_REFRACT);
  EXPECT_EQ(SURF_REFRACT_NEG, SURF_REFRACT_NEG);
}

TEST_F(IPDRMathUtilTesting, test_getSurf_row0_lastCol) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][SURFACE_REFRACTIVITY_COLS - 1] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double surf_lat = 90;
  double surf_lon = 360;
  const double EXPECTED_SURF_REFRACT = 1;
  const double ACTUAL_SURF_REFRACT =
      i.getSurfaceRefractivityValue(surf_lat, surf_lon);
  EXPECT_EQ(EXPECTED_SURF_REFRACT, ACTUAL_SURF_REFRACT);
}

TEST_F(IPDRMathUtilTesting, test_getSurf_lastRow_lastCol) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][SURFACE_REFRACTIVITY_COLS - 1] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double surf_lat = -90;
  double surf_lon = 360;
  const double EXPECTED_SURF_REFRACT = 1;
  const double ACTUAL_SURF_REFRACT =
      i.getSurfaceRefractivityValue(surf_lat, surf_lon);
  EXPECT_EQ(EXPECTED_SURF_REFRACT, ACTUAL_SURF_REFRACT);
}

TEST_F(IPDRMathUtilTesting, test_getSurf_lastRow_0Col) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[SURFACE_REFRACTIVITY_ROWS - 1][0] = 1;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double surf_lat = -90;
  double surf_lon = 0;
  const double EXPECTED_SURF_REFRACT = 1;
  const double ACTUAL_SURF_REFRACT =
      i.getSurfaceRefractivityValue(surf_lat, surf_lon);
  EXPECT_EQ(EXPECTED_SURF_REFRACT, ACTUAL_SURF_REFRACT);
}

TEST_F(IPDRMathUtilTesting, test_getSurf_1D_interp_50_50_row) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  surf[0][1] = 2;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double surf_lat = 90;
  double surf_lon = .75;
  const double EXPECTED_SURF_REFRACT = 1.5;
  const double ACTUAL_SURF_REFRACT =
      i.getSurfaceRefractivityValue(surf_lat, surf_lon);
  EXPECT_NEAR(EXPECTED_SURF_REFRACT, ACTUAL_SURF_REFRACT, TOLERANCE);
}

TEST_F(IPDRMathUtilTesting, test_getSurf_1D_interp_50_50_col) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  surf[1][0] = 2;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double surf_lat = 89.25;
  double surf_lon = 0;
  const double EXPECTED_SURF_REFRACT = 1.5;
  const double ACTUAL_SURF_REFRACT =
      i.getSurfaceRefractivityValue(surf_lat, surf_lon);
  EXPECT_NEAR(EXPECTED_SURF_REFRACT, ACTUAL_SURF_REFRACT, TOLERANCE);
}

TEST_F(IPDRMathUtilTesting, test_getSurf_1D_interp_40_60_row) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  surf[0][1] = 2;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double surf_lat = 90;
  double surf_lon = .6;
  const double EXPECTED_SURF_REFRACT = 1.4;
  const double ACTUAL_SURF_REFRACT =
      i.getSurfaceRefractivityValue(surf_lat, surf_lon);
  EXPECT_NEAR(EXPECTED_SURF_REFRACT, ACTUAL_SURF_REFRACT, TOLERANCE);
}

TEST_F(IPDRMathUtilTesting, test_getSurf_1D_interp_40_60_col) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  surf[1][0] = 2;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double surf_lat = 89.4;
  double surf_lon = 0;
  const double EXPECTED_SURF_REFRACT = 1.4;
  const double ACTUAL_SURF_REFRACT =
      i.getSurfaceRefractivityValue(surf_lat, surf_lon);
  EXPECT_NEAR(EXPECTED_SURF_REFRACT, ACTUAL_SURF_REFRACT, TOLERANCE);
}

TEST_F(IPDRMathUtilTesting, test_getSurf_2D_interp_50_50) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  surf[0][1] = 2;
  surf[1][0] = 3;
  surf[1][1] = 4;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double surf_lat = 89.25;
  double surf_lon = .75;
  const double EXPECTED_SURF_REFRACT = 2.5;
  const double ACTUAL_SURF_REFRACT =
      i.getSurfaceRefractivityValue(surf_lat, surf_lon);
  EXPECT_NEAR(EXPECTED_SURF_REFRACT, ACTUAL_SURF_REFRACT, TOLERANCE);
}

TEST_F(IPDRMathUtilTesting, test_getSurf_Exception_Latitude_LessThen) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  surf[0][1] = 2;
  surf[1][0] = 3;
  surf[1][1] = 4;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double surf_lat = -90.1;
  double surf_lon = 0;
  std::string EXPECTED_EXCEPT_MSG = "Latitude outside [-90.0,90.0]!";
  // std::string EXPECTED_EXCEPT_MSG = "Latitude outside valid bounds!";
  EXPECT_THROW(
      {
        try {
          i.getSurfaceRefractivityValue(surf_lat, surf_lon);
        } catch (const std::range_error &e) {
          EXPECT_EQ(EXPECTED_EXCEPT_MSG, e.what());
          throw;
        }
      },
      std::range_error);
}

TEST_F(IPDRMathUtilTesting, test_getSurf_Exception_Latitude_GreaterThen) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  surf[0][1] = 2;
  surf[1][0] = 3;
  surf[1][1] = 4;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double surf_lat = 90.1;
  double surf_lon = 0;
  std::string EXPECTED_EXCEPT_MSG = "Latitude outside [-90.0,90.0]!";
  // std::string EXPECTED_EXCEPT_MSG = "Latitude outside valid bounds!";
  EXPECT_THROW(
      {
        try {
          i.getSurfaceRefractivityValue(surf_lat, surf_lon);
        } catch (const std::range_error &e) {
          EXPECT_EQ(EXPECTED_EXCEPT_MSG, e.what());
          throw;
        }
      },
      std::range_error);
}

TEST_F(IPDRMathUtilTesting, test_getSurf_Exception_Longitude_LessThen) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  surf[0][1] = 2;
  surf[1][0] = 3;
  surf[1][1] = 4;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double surf_lat = 0;
  double surf_lon = -180.1;
  std::string EXPECTED_EXCEPT_MSG = "Longitude outside [-180.0,360.0]!";
  // std::string EXPECTED_EXCEPT_MSG = "Longitude outside valid bounds!";
  EXPECT_THROW(
      {
        try {
          i.getSurfaceRefractivityValue(surf_lat, surf_lon);
        } catch (const std::range_error &e) {
          EXPECT_EQ(EXPECTED_EXCEPT_MSG, e.what());
          throw;
        }
      },
      std::range_error);
}

TEST_F(IPDRMathUtilTesting, test_getSurf_Exception_Longitude_GreaterThen) {
  std::vector<std::vector<int>> radio = constructEmptyRadioClimate();
  radio[0][0] = 1;
  outputRadioToFile(RADIO_FILE, radio);

  std::vector<std::vector<double>> surf = constructEmptySurfRefract();
  surf[0][0] = 1;
  surf[0][1] = 2;
  surf[1][0] = 3;
  surf[1][1] = 4;
  outputSurfToFile(SURF_FILE, surf);

  ITUDataClass i(RADIO_FILE, SURF_FILE);

  double surf_lat = 0;
  double surf_lon = 360.1;
  std::string EXPECTED_EXCEPT_MSG = "Longitude outside [-180.0,360.0]!";
  // std::string EXPECTED_EXCEPT_MSG = "Longitude outside valid bounds!";
  EXPECT_THROW(
      {
        try {
          i.getSurfaceRefractivityValue(surf_lat, surf_lon);
        } catch (const std::range_error &e) {
          EXPECT_EQ(EXPECTED_EXCEPT_MSG, e.what());
          throw;
        }
      },
      std::range_error);
}
// END TESTING SURFACE REFRACTIVITY

// BEGIN TESTING BILINEAR INTERPOLATION
TEST_F(IPDRMathUtilTesting, Test_Interpolation_q11_Only) {
  double q11 = 1;
  double q12 = 2;
  double q21 = 3;
  double q22 = 4;

  double x1 = 1;
  double x2 = 2;
  double y1 = 1;
  double y2 = 2;

  double x = 1;
  double y = 1;

  const double ACTUAL_VAL = (   q11*(x2-x)*(y2-y)
                              + q12*(x2-x)*(y-y1)
                              + q21*(x-x1)*(y2-y)
                              + q22*(x-x1)*(y-y1) ) / ((x2-x1)*(y2-y1));

//  const double ACTUAL_VAL = IPDRMathUtils::twoDInterpolation(
//      q11, q12, q21, q22, x1, x2, y1, y2, x, y);

  const double EXPECTED_VAL = 1;
  EXPECT_EQ(EXPECTED_VAL, ACTUAL_VAL);
}

TEST_F(IPDRMathUtilTesting, Test_Interpolation_q12_Only) {
  double q11 = 1;
  double q12 = 2;
  double q21 = 3;
  double q22 = 4;

  double x1 = 1;
  double x2 = 2;
  double y1 = 1;
  double y2 = 2;

  double x = 1;
  double y = 2;

  const double ACTUAL_VAL = (   q11*(x2-x)*(y2-y)
                              + q12*(x2-x)*(y-y1)
                              + q21*(x-x1)*(y2-y)
                              + q22*(x-x1)*(y-y1) ) / ((x2-x1)*(y2-y1));

//  const double ACTUAL_VAL = IPDRMathUtils::twoDInterpolation(
//      q11, q12, q21, q22, x1, x2, y1, y2, x, y);

  const double EXPECTED_VAL = 2;
  EXPECT_EQ(EXPECTED_VAL, ACTUAL_VAL);
}

TEST_F(IPDRMathUtilTesting, Test_Interpolation_q21_Only) {
  double q11 = 1;
  double q12 = 2;
  double q21 = 3;
  double q22 = 4;

  double x1 = 1;
  double x2 = 2;
  double y1 = 1;
  double y2 = 2;

  double x = 2;
  double y = 1;

  const double ACTUAL_VAL = (   q11*(x2-x)*(y2-y)
                              + q12*(x2-x)*(y-y1)
                              + q21*(x-x1)*(y2-y)
                              + q22*(x-x1)*(y-y1) ) / ((x2-x1)*(y2-y1));

//  const double ACTUAL_VAL = IPDRMathUtils::twoDInterpolation(
//      q11, q12, q21, q22, x1, x2, y1, y2, x, y);

  const double EXPECTED_VAL = 3;
  EXPECT_EQ(EXPECTED_VAL, ACTUAL_VAL);
}

TEST_F(IPDRMathUtilTesting, Test_Interpolation_q22_Only) {
  double q11 = 1;
  double q12 = 2;
  double q21 = 3;
  double q22 = 4;

  double x1 = 1;
  double x2 = 2;
  double y1 = 1;
  double y2 = 2;

  double x = 2;
  double y = 2;

  const double ACTUAL_VAL = (   q11*(x2-x)*(y2-y)
                              + q12*(x2-x)*(y-y1)
                              + q21*(x-x1)*(y2-y)
                              + q22*(x-x1)*(y-y1) ) / ((x2-x1)*(y2-y1));

//  const double ACTUAL_VAL = IPDRMathUtils::twoDInterpolation(
//      q11, q12, q21, q22, x1, x2, y1, y2, x, y);

  const double EXPECTED_VAL = 4;
  EXPECT_EQ(EXPECTED_VAL, ACTUAL_VAL);
}

TEST_F(IPDRMathUtilTesting, Test_Interpolation_ExactCenter) {
  double q11 = 1;
  double q12 = 2;
  double q21 = 3;
  double q22 = 4;

  double x1 = 1;
  double x2 = 2;
  double y1 = 1;
  double y2 = 2;

  double x = 1.5;
  double y = 1.5;

  const double ACTUAL_VAL = (   q11*(x2-x)*(y2-y)
                              + q12*(x2-x)*(y-y1)
                              + q21*(x-x1)*(y2-y)
                              + q22*(x-x1)*(y-y1) ) / ((x2-x1)*(y2-y1));

//  const double ACTUAL_VAL = IPDRMathUtils::twoDInterpolation(
//      q11, q12, q21, q22, x1, x2, y1, y2, x, y);

  const double EXPECTED_VAL = 2.5;
  EXPECT_EQ(EXPECTED_VAL, ACTUAL_VAL);
}

TEST_F(IPDRMathUtilTesting, Test_Interpolation_XCenter_Y1) {
  double q11 = 1;
  double q12 = 2;
  double q21 = 3;
  double q22 = 4;

  double x1 = 1;
  double x2 = 2;
  double y1 = 1;
  double y2 = 2;

  double x = 1.5;
  double y = 1;

  const double ACTUAL_VAL = (   q11*(x2-x)*(y2-y)
                              + q12*(x2-x)*(y-y1)
                              + q21*(x-x1)*(y2-y)
                              + q22*(x-x1)*(y-y1) ) / ((x2-x1)*(y2-y1));

//  const double ACTUAL_VAL = IPDRMathUtils::twoDInterpolation(
//      q11, q12, q21, q22, x1, x2, y1, y2, x, y);

  const double EXPECTED_VAL = 2;
  EXPECT_EQ(EXPECTED_VAL, ACTUAL_VAL);
}

TEST_F(IPDRMathUtilTesting, Test_Interpolation_XCenter_Y2) {
  double q11 = 1;
  double q12 = 2;
  double q21 = 3;
  double q22 = 4;

  double x1 = 1;
  double x2 = 2;
  double y1 = 1;
  double y2 = 2;

  double x = 1.5;
  double y = 2;

  const double ACTUAL_VAL = (   q11*(x2-x)*(y2-y)
                              + q12*(x2-x)*(y-y1)
                              + q21*(x-x1)*(y2-y)
                              + q22*(x-x1)*(y-y1) ) / ((x2-x1)*(y2-y1));

//  const double ACTUAL_VAL = IPDRMathUtils::twoDInterpolation(
//      q11, q12, q21, q22, x1, x2, y1, y2, x, y);

  const double EXPECTED_VAL = 3;
  EXPECT_EQ(EXPECTED_VAL, ACTUAL_VAL);
}

TEST_F(IPDRMathUtilTesting, Test_Interpolation_YCenter_X1) {
  double q11 = 1;
  double q12 = 2;
  double q21 = 3;
  double q22 = 4;

  double x1 = 1;
  double x2 = 2;
  double y1 = 1;
  double y2 = 2;

  double x = 1;
  double y = 1.5;

  const double ACTUAL_VAL = (   q11*(x2-x)*(y2-y)
                              + q12*(x2-x)*(y-y1)
                              + q21*(x-x1)*(y2-y)
                              + q22*(x-x1)*(y-y1) ) / ((x2-x1)*(y2-y1));

//  const double ACTUAL_VAL = IPDRMathUtils::twoDInterpolation(
//      q11, q12, q21, q22, x1, x2, y1, y2, x, y);

  const double EXPECTED_VAL = 1.5;
  EXPECT_EQ(EXPECTED_VAL, ACTUAL_VAL);
}

TEST_F(IPDRMathUtilTesting, Test_Interpolation_YCenter_X2) {
  double q11 = 1;
  double q12 = 2;
  double q21 = 3;
  double q22 = 4;

  double x1 = 1;
  double x2 = 2;
  double y1 = 1;
  double y2 = 2;

  double x = 2;
  double y = 1.5;

  const double ACTUAL_VAL = (   q11*(x2-x)*(y2-y)
                              + q12*(x2-x)*(y-y1)
                              + q21*(x-x1)*(y2-y)
                              + q22*(x-x1)*(y-y1) ) / ((x2-x1)*(y2-y1));

//  const double ACTUAL_VAL = IPDRMathUtils::twoDInterpolation(
//      q11, q12, q21, q22, x1, x2, y1, y2, x, y);

  const double EXPECTED_VAL = 3.5;
  EXPECT_EQ(EXPECTED_VAL, ACTUAL_VAL);
}

TEST_F(IPDRMathUtilTesting, Test_Interpolation_X4_Y6) {
  double q11 = 1;
  double q12 = 2;
  double q21 = 3;
  double q22 = 4;

  double x1 = 1;
  double x2 = 2;
  double y1 = 1;
  double y2 = 2;

  double x = 1.4;
  double y = 1.6;

  const double ACTUAL_VAL = (   q11*(x2-x)*(y2-y)
                              + q12*(x2-x)*(y-y1)
                              + q21*(x-x1)*(y2-y)
                              + q22*(x-x1)*(y-y1) ) / ((x2-x1)*(y2-y1));

//  const double ACTUAL_VAL = IPDRMathUtils::twoDInterpolation(
//      q11, q12, q21, q22, x1, x2, y1, y2, x, y);

  const double EXPECTED_VAL = 2.4;
  EXPECT_EQ(EXPECTED_VAL, ACTUAL_VAL);
}

TEST_F(IPDRMathUtilTesting, Test_Interpolation_X6_Y4) {
  double q11 = 1;
  double q12 = 2;
  double q21 = 3;
  double q22 = 4;

  double x1 = 1;
  double x2 = 2;
  double y1 = 1;
  double y2 = 2;

  double x = 1.6;
  double y = 1.4;

  const double ACTUAL_VAL = (   q11*(x2-x)*(y2-y)
                              + q12*(x2-x)*(y-y1)
                              + q21*(x-x1)*(y2-y)
                              + q22*(x-x1)*(y-y1) ) / ((x2-x1)*(y2-y1));

//  const double ACTUAL_VAL = IPDRMathUtils::twoDInterpolation(
//      q11, q12, q21, q22, x1, x2, y1, y2, x, y);

  const double EXPECTED_VAL = 2.6;
  EXPECT_EQ(EXPECTED_VAL, ACTUAL_VAL);
}

TEST_F(IPDRMathUtilTesting, Test_Interpolation_X9_Y1) {
  double q11 = 1;
  double q12 = 2;
  double q21 = 3;
  double q22 = 4;

  double x1 = 1;
  double x2 = 2;
  double y1 = 1;
  double y2 = 2;

  double x = 1.9;
  double y = 1.1;

  const double ACTUAL_VAL = (   q11*(x2-x)*(y2-y)
                              + q12*(x2-x)*(y-y1)
                              + q21*(x-x1)*(y2-y)
                              + q22*(x-x1)*(y-y1) ) / ((x2-x1)*(y2-y1));

//  const double ACTUAL_VAL = IPDRMathUtils::twoDInterpolation(
//      q11, q12, q21, q22, x1, x2, y1, y2, x, y);

  const double EXPECTED_VAL = 2.9;
  ASSERT_NEAR(
      EXPECTED_VAL,
      ACTUAL_VAL,
      1.0e-10
  );
  // EXPECT_EQ(EXPECTED_VAL, ACTUAL_VAL);
}

TEST_F(IPDRMathUtilTesting, Test_Interpolation_X1_Y9) {
  double q11 = 1;
  double q12 = 2;
  double q21 = 3;
  double q22 = 4;

  double x1 = 1;
  double x2 = 2;
  double y1 = 1;
  double y2 = 2;

  double x = 1.1;
  double y = 1.9;

  const double ACTUAL_VAL = (   q11*(x2-x)*(y2-y)
                              + q12*(x2-x)*(y-y1)
                              + q21*(x-x1)*(y2-y)
                              + q22*(x-x1)*(y-y1) ) / ((x2-x1)*(y2-y1));

//  const double ACTUAL_VAL = IPDRMathUtils::twoDInterpolation(
//      q11, q12, q21, q22, x1, x2, y1, y2, x, y);

  const double EXPECTED_VAL = 2.1;
  EXPECT_EQ(EXPECTED_VAL, ACTUAL_VAL);
}

// TEST_F(IPDRMathUtilTesting, Test_Interpolation_Exception_X_less) {
//   double q11 = 1;
//   double q12 = 2;
//   double q21 = 3;
//   double q22 = 4;
// 
//   double x1 = 1;
//   double x2 = 2;
//   double y1 = 1;
//   double y2 = 2;
// 
//   double x = 0;
//   double y = 1;
// 
//   const std::string EXPECTED_EXCEPT_MSG =
//       "Error! Value to interpolate exceeds the bounds. Check your bounding x/y "
//       "parameters!";
//   EXPECT_THROW(
//       {
//         try {
//           IPDRMathUtils::twoDInterpolation(q11, q12, q21, q22, x1, x2, y1, y2,
//                                            x, y);
//         } catch (const std::runtime_error &e) {
//           EXPECT_EQ(EXPECTED_EXCEPT_MSG, e.what());
//           throw;
//         }
//       },
//       std::runtime_error);
// }

// TEST_F(IPDRMathUtilTesting, Test_Interpolation_Exception_X_greater) {
//   double q11 = 1;
//   double q12 = 2;
//   double q21 = 3;
//   double q22 = 4;
// 
//   double x1 = 1;
//   double x2 = 2;
//   double y1 = 1;
//   double y2 = 2;
// 
//   double x = 3;
//   double y = 1;
// 
//   const std::string EXPECTED_EXCEPT_MSG =
//       "Error! Value to interpolate exceeds the bounds. Check your bounding x/y "
//       "parameters!";
//   EXPECT_THROW(
//       {
//         try {
//           IPDRMathUtils::twoDInterpolation(q11, q12, q21, q22, x1, x2, y1, y2,
//                                            x, y);
//         } catch (const std::runtime_error &e) {
//           EXPECT_EQ(EXPECTED_EXCEPT_MSG, e.what());
//           throw;
//         }
//       },
//       std::runtime_error);
// }

// TEST_F(IPDRMathUtilTesting, Test_Interpolation_Exception_Y_less) {
//   double q11 = 1;
//   double q12 = 2;
//   double q21 = 3;
//   double q22 = 4;
// 
//   double x1 = 1;
//   double x2 = 2;
//   double y1 = 1;
//   double y2 = 2;
// 
//   double x = 1;
//   double y = 0;
// 
//   const std::string EXPECTED_EXCEPT_MSG =
//       "Error! Value to interpolate exceeds the bounds. Check your bounding x/y "
//       "parameters!";
//   EXPECT_THROW(
//       {
//         try {
//           IPDRMathUtils::twoDInterpolation(q11, q12, q21, q22, x1, x2, y1, y2,
//                                            x, y);
//         } catch (const std::runtime_error &e) {
//           EXPECT_EQ(EXPECTED_EXCEPT_MSG, e.what());
//           throw;
//         }
//       },
//       std::runtime_error);
// }

// TEST_F(IPDRMathUtilTesting, Test_Interpolation_Exception_Y_greater) {
//   double q11 = 1;
//   double q12 = 2;
//   double q21 = 3;
//   double q22 = 4;
// 
//   double x1 = 1;
//   double x2 = 2;
//   double y1 = 1;
//   double y2 = 2;
// 
//   double x = 1;
//   double y = 3;
// 
//   const std::string EXPECTED_EXCEPT_MSG =
//       "Error! Value to interpolate exceeds the bounds. Check your bounding x/y "
//       "parameters!";
//   EXPECT_THROW(
//       {
//         try {
//           IPDRMathUtils::twoDInterpolation(q11, q12, q21, q22, x1, x2, y1, y2,
//                                            x, y);
//         } catch (const std::runtime_error &e) {
//           EXPECT_EQ(EXPECTED_EXCEPT_MSG, e.what());
//           throw;
//         }
//       },
//       std::runtime_error);
// }

// int main(int argc, char **argv) {
//   ::testing::InitGoogleTest(&argc, argv);
//   return RUN_ALL_TESTS();
// }
// END TESTING BILINEAR INTERPOLATION
