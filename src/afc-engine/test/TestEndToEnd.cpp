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
                            { "requestId", "12345678" },
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
                                                    { "longitude", "-122.984157" },
                                                    { "latitude", "37.425056" }
                                                }
                                            },
                                            { "majorAxis", 100 },
                                            { "minorAxis", 50 },
                                            { "orientation", 70 }
                                        }
                                    },
                                    { "elevation",
                                        QJsonObject
                                        {
                                            { "height", 3.0 },
                                            { "heightType", "AGL" },
                                            { "verticalUncertainty", 2 }
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
                                        { "highFrequency", 6425 }
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
                            { "minDesiredPower", 24 },
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
                { "maxLinkDistance", 50 },
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
                { "ulsDatabase", "CONUS_filtered_ULS_21Jan2020_6GHz_1.1.0_fixbps_sort_1record_unii5_7.sqlite3"},
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
    _afc.setStateRoot("/var/lib/fbrat");
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

TEST_F(EndToEndTest, Test1) {
    QJsonArray availableSpectrumRequestArr = inputJsonDeviceData["availableSpectrumInquiryRequests"].toArray();
    QJsonObject availableSpectrumRequestObj = availableSpectrumRequestArr[0].toObject();
    QJsonObject locationObj = availableSpectrumRequestObj["location"].toObject();
    // QJsonArray  inquiredFrequencyRangeArr = availableSpectrumRequestObj["inquiredFrequencyRange"].toArray();
    // QJsonObject inquiredFrequencyRangeObj = inquiredFrequencyRangeArr[0].toObject();
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
    // inquiredFrequencyRangeObj["lowFrequency"] = 5945;
    // inquiredFrequencyRangeObj["highFrequency"] = 7125;

    availableSpectrumRequestObj.remove("inquiredFrequencyRange");

    ellipseObj["center"] = centerObj;
    locationObj["ellipse"] = ellipseObj;
    locationObj["elevation"] = elevationObj;
    // inquiredFrequencyRangeArr[0] = inquiredFrequencyRangeObj;
    // availableSpectrumRequestObj["inquiredFrequencyRange"] = inquiredFrequencyRangeArr;
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

