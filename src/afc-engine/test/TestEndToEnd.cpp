#include "../AfcManager.h"
#include "ratcommon/CsvReader.h"
#include "testcommon/UnitTestHelpers.h"
#include "testcommon/ErrorStats.h"
#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>

class EndToEndTest : public ::testing::Test {
    protected:
        AfcManager _afc;
        QJsonObject inputJsonDeviceData;
        QJsonObject inputJsonConfigData;
        std::string runInputDeviceJsonFile;
        std::string runInputConfigJsonFile;
        std::string runOutputJsonFile;
        std::string expOutputJsonFile;
        std::string runExcThrFile;
        std::string expExcThrFile;

        std::string testDir = "../../../../src/afc-engine/test";

        virtual void SetUp() override {

            inputJsonDeviceData = QJsonObject {
                { "version", "1.1"},
                { "availableSpectrumInquiryRequests",
                    QJsonArray
                    {
                        QJsonObject
                        {
                            { "requestId", "0" },
                            { "deviceDescriptor",
                                QJsonObject
                                {
                                    { "serialNumber", "ABCDEFG" },
                                    { "certificationId",
                                        QJsonArray
                                        {
                                            QJsonObject
                                            {
                                                { "nra", "FCC" },
                                                { "id", "EFGHIJK" }
                                            }
                                        }
                                    },
                                    { "rulesetIds", QJsonArray { "US_47_CFR_PART_15_SUBPART_E" } }
                                }
                            },
                            { "location",
                                QJsonObject
                                {
                                    { "ellipse",
                                        QJsonObject
                                        {
                                            { "center",
                                                QJsonObject
                                                {
                                                    { "longitude", "-73.97434" },
                                                    { "latitude", "40.75924" }
                                                }
                                            },
                                            { "majorAxis", 100 },
                                            { "minorAxis", 50 },
                                            { "orientation", 45 }
                                        }
                                    },
                                    { "elevation",
                                        QJsonObject
                                        {
                                            { "height", 129 },
                                            { "heightType", "AGL" },
                                            { "verticalUncertainty", 5 }
                                        }
                                    },
                                    { "indoorDeployment", 2 }
                                }
                            },
                            { "inquiredFrequencyRange",
                                QJsonArray
                                {
                                    QJsonObject
                                    {
                                        { "lowFrequency", 5925 },
                                        { "highFrequency", 6425 } //Note that these values are split over 5925-6425 and 6525-7125
                                    }
                                }
                            },
                            { "inquiredChannels",
                                QJsonArray
                                {
                                    QJsonObject
                                    {
                                        { "globalOperatingClass", 133 }
                                    },
                                }
                            },
                            { "minDesiredPower", 18 },
                        }
                    }
                }
            };
            inputJsonConfigData = QJsonObject {
                { "freqBands",
                    QJsonArray
                    {
                        QJsonObject
                        {
                            { "name", "UNII5" },
                            { "startFreqMHz", 5925 },
                            { "stopFreqMHz", 6425 }
                        },
                        QJsonObject
                        {
                            { "name", "UNII7" },
                            { "startFreqMHz", 6525 },
                            { "stopFreqMHz", 6875 }
                        }
                    }
                },
                { "antennaPattern",
                    QJsonObject
                    {
                        { "kind", "F.1245" }
                    }
                },
                { "polarizationMismatchLoss",
                    QJsonObject
                    {
                        { "kind", "Fixed Value" },
                        { "value", 3 }
                    }
                },
                { "bodyLoss",
                    QJsonObject
                    {
                        { "kind", "Fixed Value" },
                        { "valueIndoor", 0 },
                        { "valueOutdoor", 0 }
                    }
                },
                { "buildingPenetrationLoss",
                    QJsonObject
                    {
                        { "kind", "Fixed Value" },
                        { "value", 20.5 }
                    }
                },
                { "receiverFeederLoss",
                    QJsonObject
                    {
                        { "UNII5", 3 },
                        { "UNII7", 2.5 },
                        { "other", 2 }
                    }
                },
                { "fsReceiverNoise",
                    QJsonObject
                    {
                        { "UNII5", -110 },
                        { "UNII7", -109.5 },
                        { "other", -109 }
                    }
                },
                { "threshold", -6 },
                { "maxLinkDistance", 10 },
                { "maxEIRP", 36 },
                { "minEIRP", 18 },
                { "propagationModel",
                    QJsonObject
                    {
                        { "kind", "FCC 6GHz Report & Order" },
                        { "win2Confidence", 50 },
                        { "itmConfidence", 50 },
                        { "p2108Confidence", 50 },
                        { "buildingSource", "None" },
                        { "terrainSource", "3DEP (30m)" }
                    }
                },
                { "receiverFeederLoss",
                    QJsonObject
                    {
                        { "UNII5", 3 },
                        { "UNII7", 2.5 },
                        { "other", 2 }
                    }
                },
                { "threshold", -6 },

                { "propagationEnv", "NLCD Point"},
                { "ulsDatabase", "CONUS_ULS_2022-03-02T03_34_41.097782_fixedBPS_sorted.sqlite3"},
                { "regionStr", "CONUS"},
                { "rasDatabase", "RASdatabase.csv"},

                { "APUncertainty",
                    QJsonObject
                    {
                        { "horizontal", 30 },
                        { "height", 5 }
                    }
                },
                { "ITMParameters",
                    QJsonObject
                    {
                        { "polarization", "Vertical" },
                        { "ground", "Good Ground" },
                        { "dielectricConst", 25 },
                        { "conductivity", 0.02 },
                        { "minSpacing", 3 },
                        { "maxPoints", 2000 }
                    }
                },
                { "clutterAtFS", false},
                { "version", "0.0.0-22750m" }
            };
        };

        void createInputDeviceJsonFile(std::string filename) {
            QJsonDocument jsonDocument = QJsonDocument(inputJsonDeviceData);

            QFile file(QString::fromStdString(filename));
            file.open(QFile::WriteOnly);
            file.write(jsonDocument.toJson());
            file.close();
        };

        void createInputConfigJsonFile(std::string filename) {
            QJsonDocument jsonDocument = QJsonDocument(inputJsonConfigData);

            QFile file(QString::fromStdString(filename));
            file.open(QFile::WriteOnly);
            file.write(jsonDocument.toJson());
            file.close();
        };

        void compareOutputJson(std::string runOutputJsonFile, std::string expOutputJsonFile);
        void compareExcThr(std::string runExcThrFile, std::string expExcThrFile);

        void runTest();
};

void EndToEndTest::compareOutputJson(std::string runOutputJsonFile, std::string expOutputJsonFile)
{
    /********************************************************************************************/
    /* Read output JSON file from test run                                                      */
    /********************************************************************************************/
    QJsonDocument runOutputJsonDoc;
    auto outputJsonStream = FileHelpers::open(QString::fromStdString(runOutputJsonFile), QIODevice::ReadOnly);
    auto gzip_reader = new GzipStream(outputJsonStream.get());
    if (!gzip_reader->open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Gzip failed to open.");
    }
    runOutputJsonDoc = QJsonDocument::fromJson(gzip_reader->readAll());
    QJsonObject runOutputJsonObj = runOutputJsonDoc.object();

    // std::cout << runOutputJsonDoc.toJson();
    /********************************************************************************************/

    /********************************************************************************************/
    /* Read expected output JSON file                                                           */
    /********************************************************************************************/
    QJsonDocument expOutputJsonDoc;
    QFile inFile;
    inFile.setFileName(expOutputJsonFile.c_str());
    if (inFile.open(QFile::ReadOnly | QFile::Text)) {
        expOutputJsonDoc = QJsonDocument::fromJson(inFile.readAll());
        inFile.close();
    } else {
        throw std::runtime_error("Failed to open expected output JSON file.");
    }
    QJsonObject expOutputJsonObj = expOutputJsonDoc.object();

    // std::cout << expOutputJsonDoc.toJson();
    /********************************************************************************************/

    EXPECT_EQ(runOutputJsonObj["version"].toString().toStdString(), expOutputJsonObj["version"].toString().toStdString());

    ASSERT_TRUE(runOutputJsonObj.contains("availableSpectrumInquiryResponses"));

    QJsonArray runAvailableSpectrumInquiryResponsesArr = runOutputJsonObj["availableSpectrumInquiryResponses"].toArray();
    QJsonArray expAvailableSpectrumInquiryResponsesArr = expOutputJsonObj["availableSpectrumInquiryResponses"].toArray();

    if (expAvailableSpectrumInquiryResponsesArr.size() != 1) {
        FAIL();
    }

    ASSERT_EQ(runAvailableSpectrumInquiryResponsesArr.size(), expAvailableSpectrumInquiryResponsesArr.size());

    QJsonObject runResponseObj = runAvailableSpectrumInquiryResponsesArr[0].toObject();
    QJsonObject expResponseObj = expAvailableSpectrumInquiryResponsesArr[0].toObject();

    EXPECT_EQ(runResponseObj["requestId"].toString().toStdString(), expResponseObj["requestId"].toString().toStdString());
    EXPECT_EQ(runResponseObj["rulesetId"].toString().toStdString(), expResponseObj["rulesetId"].toString().toStdString());

    ASSERT_TRUE(runResponseObj.contains("response"));

    QJsonObject runResponseCodeObj = runResponseObj["response"].toObject();
    QJsonObject expResponseCodeObj = expResponseObj["response"].toObject();

    ASSERT_TRUE(runResponseCodeObj.contains("responseCode"));

    ASSERT_EQ(runResponseCodeObj["responseCode"].toInt(), expResponseCodeObj["responseCode"].toInt());
    EXPECT_EQ(runResponseCodeObj["shortDescription"].toString().toStdString(), expResponseCodeObj["shortDescription"].toString().toStdString());

    bool hasFreqInfo = expResponseObj.contains("availableFrequencyInfo");

    ASSERT_EQ(runResponseObj.contains("availableFrequencyInfo"), hasFreqInfo);

    if (hasFreqInfo) {
        QJsonArray runAvailableFrequencyInfoArr = runResponseObj["availableFrequencyInfo"].toArray();
        QJsonArray expAvailableFrequencyInfoArr = expResponseObj["availableFrequencyInfo"].toArray();

        ASSERT_EQ(runAvailableFrequencyInfoArr.size(), expAvailableFrequencyInfoArr.size());

        int i;
        for(i=0; i<expAvailableFrequencyInfoArr.size(); ++i) {
            QJsonObject runAvailableFrequencyInfoObj = runAvailableFrequencyInfoArr[i].toObject();
            QJsonObject expAvailableFrequencyInfoObj = expAvailableFrequencyInfoArr[i].toObject();

            EXPECT_TRUE(runAvailableFrequencyInfoObj["frequencyRange"] == expAvailableFrequencyInfoObj["frequencyRange"]);
            EXPECT_NEAR(runAvailableFrequencyInfoObj["maxPsd"].toDouble(), expAvailableFrequencyInfoObj["maxPsd"].toDouble(), 1.0e-3);
        }
    }

    bool hasChanInfo = expResponseObj.contains("availableChannelInfo");

    ASSERT_EQ(runResponseObj.contains("availableChannelInfo"), hasChanInfo);

    if (hasChanInfo) {
        QJsonArray runAvailableChannelInfoArr = runResponseObj["availableChannelInfo"].toArray();
        QJsonArray expAvailableChannelInfoArr = expResponseObj["availableChannelInfo"].toArray();

        ASSERT_EQ(runAvailableChannelInfoArr.size(), expAvailableChannelInfoArr.size());

        int i;
        for(i=0; i<expAvailableChannelInfoArr.size(); ++i) {
            QJsonObject runAvailableChannelInfoObj = runAvailableChannelInfoArr[i].toObject();
            QJsonObject expAvailableChannelInfoObj = expAvailableChannelInfoArr[i].toObject();

            EXPECT_EQ(runAvailableChannelInfoObj["globalOperatingClass"].toInt(), expAvailableChannelInfoObj["globalOperatingClass"].toInt());

            QJsonArray runChannelCfiArr = runAvailableChannelInfoObj["channelCfi"].toArray();
            QJsonArray expChannelCfiArr = expAvailableChannelInfoObj["channelCfi"].toArray();

            QJsonArray runMaxEirpArr = runAvailableChannelInfoObj["maxEirp"].toArray();
            QJsonArray expMaxEirpArr = expAvailableChannelInfoObj["maxEirp"].toArray();

            int numChan = expChannelCfiArr.size();
            if (expMaxEirpArr.size() != numChan) {
                FAIL();
            }
            ASSERT_EQ(runChannelCfiArr.size(), numChan);
            ASSERT_EQ(runMaxEirpArr.size(), numChan);

            int chanIdx;
            for(chanIdx=0; chanIdx<numChan; ++chanIdx) {
                int runChanCfi = runChannelCfiArr[chanIdx].toInt();
                int expChanCfi = expChannelCfiArr[chanIdx].toInt();
                double runMaxEirp = runMaxEirpArr[chanIdx].toDouble();
                double expMaxEirp = expMaxEirpArr[chanIdx].toDouble();

                EXPECT_EQ(runChanCfi, expChanCfi);
                EXPECT_NEAR(runMaxEirp, expMaxEirp, 1.0e-3);
            }
        }
    }

    return;
}

void EndToEndTest::compareExcThr(std::string runExcThrFile, std::string expExcThrFile)
{
    std::cout << "Comparing exc_thr files" << std::endl;

    /********************************************************************************************/
    /* Read exc_thr file from test run                                                          */
    /********************************************************************************************/
    auto excThrStream = FileHelpers::open(QString::fromStdString(runExcThrFile), QIODevice::ReadOnly);
    auto gzip_reader = new GzipStream(excThrStream.get());
    if (!gzip_reader->open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Gzip failed to open.");
    }
    CsvReader *runExcThrReader = new CsvReader(*gzip_reader);

    ASSERT_FALSE(runExcThrReader->atEnd());

    // while(!runExcThrReader->atEnd()) {
    //     std::cout << (runExcThrReader->readRow()).join("---") << std::endl;
    // }
    /********************************************************************************************/

    /********************************************************************************************/
    /* Read expected exc_thr file                                                               */
    /********************************************************************************************/
    auto expExcThrStream = FileHelpers::open(QString::fromStdString(expExcThrFile), QIODevice::ReadOnly);
    auto exp_gzip_reader = new GzipStream(expExcThrStream.get());
    if (!exp_gzip_reader->open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Gzip failed to open.");
    }
    CsvReader *expExcThrReader = new CsvReader(*exp_gzip_reader);

    ASSERT_FALSE(expExcThrReader->atEnd());

    // while(!expExcThrReader->atEnd()) {
    //     std::cout << (expExcThrReader->readRow()).join("---") << std::endl;
    // }
    /********************************************************************************************/

    /********************************************************************************************/
    /* Compare file Headers                                                                     */
    /********************************************************************************************/
    QStringList runHeader = runExcThrReader->readRow();
    QStringList expHeader = expExcThrReader->readRow();

    ASSERT_FALSE(runHeader.size() == 0);

    ASSERT_EQ(runHeader.size(), expHeader.size());

    int numField = runHeader.size();

    int i;
    for(i=0; i<runHeader.size(); ++i) {
        ASSERT_EQ(runHeader[i].toStdString(), expHeader[i].toStdString());
    }
    /********************************************************************************************/

    /********************************************************************************************/
    /* Compare file Contents                                                                    */
    /********************************************************************************************/
    int linenum = 0;
    bool cont;
    do {
        bool runEndFlag = runExcThrReader->atEnd();
        bool expEndFlag = expExcThrReader->atEnd();
        ASSERT_EQ(runEndFlag, expEndFlag);

        cont = !runEndFlag;

        if (cont) {
            QStringList runRow = runExcThrReader->readRow();
            QStringList expRow = expExcThrReader->readRow();
            linenum++;
            ASSERT_EQ(runRow.size(), numField);
            ASSERT_EQ(expRow.size(), numField);
            int fieldIdx;
            for(fieldIdx=0; fieldIdx<numField; ++fieldIdx) {
                EXPECT_EQ(runRow[fieldIdx].toStdString(), expRow[fieldIdx].toStdString());
            }
        }
    } while(cont);
    /********************************************************************************************/

    return;
}

void EndToEndTest::runTest()
{
    createInputDeviceJsonFile(runInputDeviceJsonFile);
    createInputConfigJsonFile(runInputConfigJsonFile);

    _afc.setAnalysisType("AP-AFC");
    _afc.setStateRoot("/usr/share/fbrat/rat_transfer/");
    _afc.setConstInputs("");
    _afc.importGUIjson(runInputDeviceJsonFile);
    _afc.importConfigAFCjson(runInputConfigJsonFile);
    _afc.initializeDatabases();
    _afc.compute();

    QString outputPath = QString::fromStdString(runOutputJsonFile);
    _afc.exportGUIjson(outputPath);

    compareOutputJson(runOutputJsonFile, expOutputJsonFile);
    compareExcThr(runExcThrFile, expExcThrFile);
}

//Test 1

TEST_F(EndToEndTest, Test1) {
    QJsonArray availableSpectrumRequestArr = inputJsonDeviceData["availableSpectrumInquiryRequests"].toArray();
    QJsonObject availableSpectrumRequestObj = availableSpectrumRequestArr[0].toObject();
    QJsonObject locationObj = availableSpectrumRequestObj["location"].toObject();
    QJsonArray  inquiredFrequencyRangeArr = availableSpectrumRequestObj["inquiredFrequencyRange"].toArray();
    QJsonObject inquiredFrequencyRangeObj = inquiredFrequencyRangeArr[0].toObject();
    QJsonObject ellipseObj = locationObj["ellipse"].toObject();
    QJsonObject elevationObj = locationObj["elevation"].toObject();
    QJsonObject centerObj = ellipseObj["center"].toObject();

    QJsonArray  inquiredChannelsArr;
    int globalOperatingClass;
    for(globalOperatingClass=131; globalOperatingClass<=134; ++globalOperatingClass) {
        QJsonObject inquiredChannelObj;
        inquiredChannelObj["globalOperatingClass"] = globalOperatingClass;
        inquiredChannelsArr.append(inquiredChannelObj);
    }

    centerObj["latitude"] = 40.75924;
    centerObj["longitude"] = -73.97434;
    ellipseObj["majorAxis"] = 100;
    ellipseObj["minorAxis"] = 50;
    ellipseObj["orientation"] = 45;
    elevationObj["height"] = 129;
    elevationObj["heightType"] = "AGL";
    elevationObj["verticalUncertainty"] = 5;
    locationObj["indoorDeployment"] = 2;
    inquiredFrequencyRangeObj["lowFrequency"] = 5925;
    inquiredFrequencyRangeObj["highFrequency"] = 6425;

    QJsonObject inqFreqUnii7;
    inqFreqUnii7["lowFrequency"] = 6525;
    inqFreqUnii7["highFrequency"] = 6875;

    // availableSpectrumRequestObj.remove("inquiredFrequencyRange");

    ellipseObj["center"] = centerObj;
    locationObj["ellipse"] = ellipseObj;
    locationObj["elevation"] = elevationObj;
    inquiredFrequencyRangeArr[0] = inquiredFrequencyRangeObj;
    inquiredFrequencyRangeArr.push_back(inqFreqUnii7);
    availableSpectrumRequestObj["inquiredFrequencyRange"] = inquiredFrequencyRangeArr;
    availableSpectrumRequestObj["inquiredChannels"] = inquiredChannelsArr;
    availableSpectrumRequestObj["location"] = locationObj;
    availableSpectrumRequestArr[0] = availableSpectrumRequestObj;
    inputJsonDeviceData["availableSpectrumInquiryRequests"] = availableSpectrumRequestArr;

    inputJsonConfigData["maxLinkDistance"] = 10;

    runInputDeviceJsonFile = "/tmp/test1_input_device.json";
    runInputConfigJsonFile = "/tmp/test1_input_config.json";
    runOutputJsonFile = "/tmp/test1_output.json.gz";
    expOutputJsonFile = testDir + "/" + "expected_output_test1.json";
    runExcThrFile = "exc_thr.csv.gz";
    expExcThrFile = testDir + "/" + "expected_exc_thr_test1.csv.gz";

    runTest();
}

//Test 2

TEST_F(EndToEndTest, Test2) {
    QJsonArray availableSpectrumRequestArr = inputJsonDeviceData["availableSpectrumInquiryRequests"].toArray();
    QJsonObject availableSpectrumRequestObj = availableSpectrumRequestArr[0].toObject();
    QJsonObject locationObj = availableSpectrumRequestObj["location"].toObject();
    QJsonArray  inquiredFrequencyRangeArr = availableSpectrumRequestObj["inquiredFrequencyRange"].toArray();
    QJsonObject inquiredFrequencyRangeObj = inquiredFrequencyRangeArr[0].toObject();
    QJsonObject ellipseObj = locationObj["ellipse"].toObject();
    QJsonObject elevationObj = locationObj["elevation"].toObject();
    QJsonObject centerObj = ellipseObj["center"].toObject();

    
    
    QJsonArray  inquiredChannelsArr;
    int globalOperatingClass;
    for(globalOperatingClass=131; globalOperatingClass<=134; ++globalOperatingClass) {
        QJsonObject inquiredChannelObj;
        inquiredChannelObj["globalOperatingClass"] = globalOperatingClass;
                QJsonArray chanList;
        if (globalOperatingClass == 131) {
            chanList.push_back(5);
        } else if (globalOperatingClass == 132) {
            chanList.push_back(3);
        } else if (globalOperatingClass == 133) {
            chanList.push_back(7);
        } else if (globalOperatingClass == 134) {
            chanList.push_back(15);
        }
        inquiredChannelObj["channelCfi"] = chanList;

        inquiredChannelsArr.append(inquiredChannelObj);
    }

    centerObj["latitude"] = 37.59735;
    centerObj["longitude"] = -121.95034;
    ellipseObj["majorAxis"] = 100;
    ellipseObj["minorAxis"] = 60;
    ellipseObj["orientation"] = 70;
    elevationObj["height"] = 1.5;
    elevationObj["heightType"] = "AGL";
    elevationObj["verticalUncertainty"] = 0;
    locationObj["indoorDeployment"] = 2;
    inquiredFrequencyRangeObj["lowFrequency"] = 6745;
    inquiredFrequencyRangeObj["highFrequency"] = 6825;

    // availableSpectrumRequestObj.remove("inquiredFrequencyRange");

    ellipseObj["center"] = centerObj;
    locationObj["ellipse"] = ellipseObj;
    locationObj["elevation"] = elevationObj;
    inquiredFrequencyRangeArr[0] = inquiredFrequencyRangeObj;
    //inquiredFrequencyRangeArr.push_back(inqFreqUnii7);
    availableSpectrumRequestObj["inquiredFrequencyRange"] = inquiredFrequencyRangeArr;
    availableSpectrumRequestObj["inquiredChannels"] = inquiredChannelsArr;
    availableSpectrumRequestObj["location"] = locationObj;
    availableSpectrumRequestArr[0] = availableSpectrumRequestObj;
    inputJsonDeviceData["availableSpectrumInquiryRequests"] = availableSpectrumRequestArr;

    inputJsonConfigData["maxLinkDistance"] = 10;
    inputJsonConfigData["clutterAtFS"] = true;
    
    runInputDeviceJsonFile = "/tmp/test2_input_device.json";
    runInputConfigJsonFile = "/tmp/test2_input_config.json";
    runOutputJsonFile = "/tmp/test2_output.json.gz";
    expOutputJsonFile = testDir + "/" + "expected_output_test2.json";
    runExcThrFile = "exc_thr.csv.gz";
    expExcThrFile = testDir + "/" + "expected_exc_thr_test2.csv.gz";

    runTest();
}

//Test 3

TEST_F(EndToEndTest, Test3) {
    QJsonArray availableSpectrumRequestArr = inputJsonDeviceData["availableSpectrumInquiryRequests"].toArray();
    QJsonObject availableSpectrumRequestObj = availableSpectrumRequestArr[0].toObject();
    QJsonObject locationObj = availableSpectrumRequestObj["location"].toObject();
    QJsonArray  inquiredFrequencyRangeArr = availableSpectrumRequestObj["inquiredFrequencyRange"].toArray();
    QJsonObject inquiredFrequencyRangeObj = inquiredFrequencyRangeArr[0].toObject();
    QJsonObject elevationObj = locationObj["elevation"].toObject();
    locationObj.remove("ellipse");
    

    QJsonArray  inquiredChannelsArr;
    int globalOperatingClass;
    for(globalOperatingClass=131; globalOperatingClass<=134; ++globalOperatingClass) {
        QJsonObject inquiredChannelObj;
        inquiredChannelObj["globalOperatingClass"] = globalOperatingClass;
                QJsonArray chanList;
        if (globalOperatingClass == 131) {
            chanList.push_back(49);
            chanList.push_back(53);
            chanList.push_back(57);
            chanList.push_back(61);
            chanList.push_back(65);
        } else if (globalOperatingClass == 132) {
            chanList.push_back(3);
            chanList.push_back(51);
            chanList.push_back(67);
        } else if (globalOperatingClass == 133) {
            chanList.push_back(71);
        } else if (globalOperatingClass == 134) {
            chanList.push_back(47);
        }
        inquiredChannelObj["channelCfi"] = chanList;

        inquiredChannelsArr.append(inquiredChannelObj);
     }
    QJsonObject centerObj;
    centerObj["latitude"] = 29.7573483;
    centerObj["longitude"] = -95.4308149;
    QJsonArray OuterBoundaryArr;
    QJsonObject vec;
    vec["length"] = 64;
    vec["angle"] = 0;
    OuterBoundaryArr.push_back(vec);
    vec["length"] = 104.6;
    vec["angle"] = 45;
    OuterBoundaryArr.push_back(vec);
    vec["length"] = 104;
    vec["angle"] = 90;
    OuterBoundaryArr.push_back(vec);
    vec["length"] = 72;
    vec["angle"] = 135;
    OuterBoundaryArr.push_back(vec);
    vec["length"] = 75;
    vec["angle"] = 180;
    OuterBoundaryArr.push_back(vec);
    vec["length"] = 95.3;
    vec["angle"] = 225;
    OuterBoundaryArr.push_back(vec);
    vec["length"] = 103;
    vec["angle"] = 270;
    OuterBoundaryArr.push_back(vec);
    vec["length"] = 68;
    vec["angle"] = 315;
    OuterBoundaryArr.push_back(vec);
    QJsonObject radialPolygonObj;
    radialPolygonObj["outerBoundary"] = OuterBoundaryArr;	 
    elevationObj["heightType"] = "AGL";
    elevationObj["height"] = 1.5;
    elevationObj["verticalUncertainty"] = 0;
    locationObj["indoorDeployment"] = 0;

    availableSpectrumRequestObj.remove("inquiredFrequencyRange");
    radialPolygonObj["center"] = centerObj;
    locationObj["radialPolygon"] = radialPolygonObj;
    locationObj["elevation"] = elevationObj;
    //inquiredFrequencyRangeArr[0] = inquiredFrequencyRangeObj;
    //inquiredFrequencyRangeArr.push_back(inqFreqUnii7);
    // availableSpectrumRequestObj["inquiredFrequencyRange"] = inquiredFrequencyRangeArr;
    availableSpectrumRequestObj["inquiredChannels"] = inquiredChannelsArr;
    availableSpectrumRequestObj["location"] = locationObj;
    availableSpectrumRequestArr[0] = availableSpectrumRequestObj;
    inputJsonDeviceData["availableSpectrumInquiryRequests"] = availableSpectrumRequestArr;
   
    inputJsonConfigData["maxLinkDistance"] = 10;
    
    runInputDeviceJsonFile = "/tmp/test3_input_device.json";
    runInputConfigJsonFile = "/tmp/test3_input_config.json";
    runOutputJsonFile = "/tmp/test3_output.json.gz";
    expOutputJsonFile = testDir + "/" + "expected_output_test3.json";
    runExcThrFile = "exc_thr.csv.gz";
    expExcThrFile = testDir + "/" + "expected_exc_thr_test3.csv.gz";
    
   runTest();
}

//Test 4 

TEST_F(EndToEndTest, Test4) {
    QJsonArray availableSpectrumRequestArr = inputJsonDeviceData["availableSpectrumInquiryRequests"].toArray();
    QJsonObject availableSpectrumRequestObj = availableSpectrumRequestArr[0].toObject();
    QJsonObject locationObj = availableSpectrumRequestObj["location"].toObject();
    QJsonArray  inquiredFrequencyRangeArr = availableSpectrumRequestObj["inquiredFrequencyRange"].toArray();
    QJsonObject inquiredFrequencyRangeObj = inquiredFrequencyRangeArr[0].toObject();
    QJsonObject elevationObj = locationObj["elevation"].toObject();
    locationObj.remove("ellipse");
    
   // QJsonObject centerObj;
   // centerObj["latitude"] = 29.7573483;
   // centerObj["longitude"] = -95.4308149;
    QJsonArray OuterBoundaryArr;
    QJsonObject vec;
    vec["latitude"] = 37.546067;
    vec["longitude"] = -122.083744;
    OuterBoundaryArr.push_back(vec);
    vec["latitude"] = 37.546067;
    vec["longitude"] = -122.083064;
    OuterBoundaryArr.push_back(vec);   
    vec["latitude"] = 37.546336;
    vec["longitude"] = -122.082385;
    OuterBoundaryArr.push_back(vec);
    vec["latitude"] = 37.546875;
    vec["longitude"] = -122.082045;
    OuterBoundaryArr.push_back(vec);
    vec["latitude"] = 37.547145;
    vec["longitude"] = -122.083064;
    OuterBoundaryArr.push_back(vec);   
    vec["latitude"] = 37.546875;
    vec["longitude"] = -122.083744;
    OuterBoundaryArr.push_back(vec);
    vec["latitude"] = 37.546606;
    vec["longitude"] = -122.084084;
    OuterBoundaryArr.push_back(vec);
    QJsonObject linearPolygonObj;
    linearPolygonObj["outerBoundary"] = OuterBoundaryArr;	 
    elevationObj["heightType"] = "AGL";
    elevationObj["height"] = 1.5;
    elevationObj["verticalUncertainty"] = 0;
    locationObj["indoorDeployment"] = 2;
    inquiredFrequencyRangeObj["lowFrequency"] = 5925;
    inquiredFrequencyRangeObj["highFrequency"] = 6425;
    

    QJsonObject inqFreqUnii7;
    inqFreqUnii7["lowFrequency"] = 6525;
    inqFreqUnii7["highFrequency"] = 6875;


    // linearPolygonObj["center"] = centerObj;
    locationObj["linearPolygon"] = linearPolygonObj;
    locationObj["elevation"] = elevationObj;
    inquiredFrequencyRangeArr[0] = inquiredFrequencyRangeObj;
    inquiredFrequencyRangeArr.push_back(inqFreqUnii7);
    availableSpectrumRequestObj["inquiredFrequencyRange"] = inquiredFrequencyRangeArr;
    availableSpectrumRequestObj.remove("inquiredChannels");
    availableSpectrumRequestObj["location"] = locationObj;
    availableSpectrumRequestArr[0] = availableSpectrumRequestObj;
    inputJsonDeviceData["availableSpectrumInquiryRequests"] = availableSpectrumRequestArr;
   
    inputJsonConfigData["maxLinkDistance"] = 10;
    
    runInputDeviceJsonFile = "/tmp/test4_input_device.json";
    runInputConfigJsonFile = "/tmp/test4_input_config.json";
    runOutputJsonFile = "/tmp/test4_output.json.gz";
    expOutputJsonFile = testDir + "/" + "expected_output_test4.json";
    runExcThrFile = "exc_thr.csv.gz";
    expExcThrFile = testDir + "/" + "expected_exc_thr_test4.csv.gz";
    
   runTest();
}

//Test 5

TEST_F(EndToEndTest, Test5) {
    QJsonArray availableSpectrumRequestArr = inputJsonDeviceData["availableSpectrumInquiryRequests"].toArray();
    QJsonObject availableSpectrumRequestObj = availableSpectrumRequestArr[0].toObject();
    QJsonObject locationObj = availableSpectrumRequestObj["location"].toObject();
    QJsonArray  inquiredFrequencyRangeArr = availableSpectrumRequestObj["inquiredFrequencyRange"].toArray();
    QJsonObject inquiredFrequencyRangeObj = inquiredFrequencyRangeArr[0].toObject();
    QJsonObject ellipseObj = locationObj["ellipse"].toObject();
    QJsonObject elevationObj = locationObj["elevation"].toObject();
    QJsonObject centerObj = ellipseObj["center"].toObject();

    QJsonArray  inquiredChannelsArr;
    int globalOperatingClass;
    for(globalOperatingClass=131; globalOperatingClass<=134; ++globalOperatingClass) {
        if(globalOperatingClass!=132){
            QJsonObject inquiredChannelObj;
            inquiredChannelObj["globalOperatingClass"] = globalOperatingClass;
                    QJsonArray chanList;
#if 0
            if (globalOperatingClass == 131) {
                chanList.push_back(173);
	    }else if (globalOperatingClass == 133) {
                chanList.push_back(167);
            } else if (globalOperatingClass == 134) {
                chanList.push_back(175);
            }
            inquiredChannelObj["channelCfi"] = chanList;
#endif
    
            inquiredChannelsArr.append(inquiredChannelObj);
        
        }
    }
    centerObj["latitude"] = 40.75940000579217;
    centerObj["longitude"] = -73.97364799433059;
    ellipseObj["majorAxis"] = 20;
    ellipseObj["minorAxis"] = 8;
    ellipseObj["orientation"] = 120;
    elevationObj["height"] = 130;
    elevationObj["heightType"] = "AGL";
    elevationObj["verticalUncertainty"] = 0;
    locationObj["indoorDeployment"] = 1;
    inquiredFrequencyRangeObj["lowFrequency"] = 6525;
    inquiredFrequencyRangeObj["highFrequency"] = 6585;

    // availableSpectrumRequestObj.remove("inquiredFrequencyRange");

    ellipseObj["center"] = centerObj;
    locationObj["ellipse"] = ellipseObj;
    locationObj["elevation"] = elevationObj;
    inquiredFrequencyRangeArr[0] = inquiredFrequencyRangeObj;
    //inquiredFrequencyRangeArr.push_back(inqFreqUnii7);
    availableSpectrumRequestObj["inquiredFrequencyRange"] = inquiredFrequencyRangeArr;
    availableSpectrumRequestObj["inquiredChannels"] = inquiredChannelsArr;
    availableSpectrumRequestObj["location"] = locationObj;
    availableSpectrumRequestArr[0] = availableSpectrumRequestObj;
    inputJsonDeviceData["availableSpectrumInquiryRequests"] = availableSpectrumRequestArr;

    inputJsonConfigData["maxLinkDistance"] = 10;

    runInputDeviceJsonFile = "/tmp/test5_input_device.json";
    runInputConfigJsonFile = "/tmp/test5_input_config.json";
    runOutputJsonFile = "/tmp/test5_output.json.gz";
    expOutputJsonFile = testDir + "/" + "expected_output_test5.json";
    runExcThrFile = "exc_thr.csv.gz";
    expExcThrFile = testDir + "/" + "expected_exc_thr_test5.csv.gz";

    runTest();
}

//Test 6

TEST_F(EndToEndTest, Test6) {
    QJsonArray availableSpectrumRequestArr = inputJsonDeviceData["availableSpectrumInquiryRequests"].toArray();
    QJsonObject availableSpectrumRequestObj = availableSpectrumRequestArr[0].toObject();
    QJsonObject locationObj = availableSpectrumRequestObj["location"].toObject();
    QJsonArray  inquiredFrequencyRangeArr = availableSpectrumRequestObj["inquiredFrequencyRange"].toArray();
    QJsonObject inquiredFrequencyRangeObj = inquiredFrequencyRangeArr[0].toObject();
    QJsonObject ellipseObj = locationObj["ellipse"].toObject();
    QJsonObject elevationObj = locationObj["elevation"].toObject();
    QJsonObject centerObj = ellipseObj["center"].toObject();

    QJsonArray  inquiredChannelsArr;
    int globalOperatingClass;
    for(globalOperatingClass=131; globalOperatingClass<=134; ++globalOperatingClass) {
        QJsonObject inquiredChannelObj;
        inquiredChannelObj["globalOperatingClass"] = globalOperatingClass;
                QJsonArray chanList;
        if (globalOperatingClass == 131) {
            chanList.push_back(93);
        } else if (globalOperatingClass == 132) {
            chanList.push_back(91); 
	}else if (globalOperatingClass == 133) {
            chanList.push_back(87);
        } else if (globalOperatingClass == 134) {
            chanList.push_back(79);
        }
        inquiredChannelObj["channelCfi"] = chanList;

        inquiredChannelsArr.append(inquiredChannelObj);
    }
    centerObj["latitude"] = 36.79947675671799;
    centerObj["longitude"] = -118.89539271593094;
    ellipseObj["majorAxis"] = 100;
    ellipseObj["minorAxis"] = 60;
    ellipseObj["orientation"] = 150;
    elevationObj["height"] = 30;
    elevationObj["heightType"] = "AGL";
    elevationObj["verticalUncertainty"] = 3;
    locationObj["indoorDeployment"] = 0;
    inquiredFrequencyRangeObj["lowFrequency"] = 6525;
    inquiredFrequencyRangeObj["highFrequency"] = 6875;

    // availableSpectrumRequestObj.remove("inquiredFrequencyRange");

    ellipseObj["center"] = centerObj;
    locationObj["ellipse"] = ellipseObj;
    locationObj["elevation"] = elevationObj;
    inquiredFrequencyRangeArr[0] = inquiredFrequencyRangeObj;
    //inquiredFrequencyRangeArr.push_back(inqFreqUnii7);
    availableSpectrumRequestObj["inquiredFrequencyRange"] = inquiredFrequencyRangeArr;
    availableSpectrumRequestObj["inquiredChannels"] = inquiredChannelsArr;
    availableSpectrumRequestObj["location"] = locationObj;
    availableSpectrumRequestArr[0] = availableSpectrumRequestObj;
    inputJsonDeviceData["availableSpectrumInquiryRequests"] = availableSpectrumRequestArr;

    inputJsonConfigData["maxLinkDistance"] = 50;

    runInputDeviceJsonFile = "/tmp/test6_input_device.json";
    runInputConfigJsonFile = "/tmp/test6_input_config.json";
    runOutputJsonFile = "/tmp/test6_output.json.gz";
    expOutputJsonFile = testDir + "/" + "expected_output_test6.json";
    runExcThrFile = "exc_thr.csv.gz";
    expExcThrFile = testDir + "/" + "expected_exc_thr_test6.csv.gz";

    runTest();
}

