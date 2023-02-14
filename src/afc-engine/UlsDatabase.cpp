#include "UlsDatabase.h"

#include <limits>
#include <QSqlDriver>
#include <QSqlResult>
#include <QSqlError>
#include <rkfsql/SqlConnectionDefinition.h>
#include <rkfsql/SqlTransaction.h>
#include <rkfsql/SqlPreparedQuery.h>
#include <rkfsql/SqlTable.h>
#include <rkfsql/SqlSelect.h>
#include <rkfsql/SqlTransaction.h>
#include <rkflogging/Logging.h>
#include "rkflogging/ErrStream.h"
#include "AfcDefinitions.h"
#include "lininterp.h"
#include "global_defines.h"

namespace
{
// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "UlsDatabase")

} // end namespace

/******************************************************************************************/
/**** CONSTRUCTOR: UlsDatabase::UlsDatabase()                                          ****/
/******************************************************************************************/
UlsDatabase::UlsDatabase()
{
	nullInitialize();

	std::vector<int *> fieldIdxList;
	std::vector<int *> prFieldIdxList;
	std::vector<int *> antnameFieldIdxList;
	std::vector<int *> antaobFieldIdxList;
	std::vector<int *> antgainFieldIdxList;
	std::vector<int *> rasFieldIdxList;

	columns << "fsid";
	fieldIdxList.push_back(&fsidIdx);
	columns << "region";
	fieldIdxList.push_back(&regionIdx);

	columns << "callsign";
	fieldIdxList.push_back(&callsignIdx);
	columns << "radio_service";
	fieldIdxList.push_back(&radio_serviceIdx);
	columns << "name";
	fieldIdxList.push_back(&nameIdx);
	columns << "rx_callsign";
	fieldIdxList.push_back(&rx_callsignIdx);
	columns << "rx_antenna_num";
	fieldIdxList.push_back(&rx_antenna_numIdx);
	columns << "freq_assigned_start_mhz";
	fieldIdxList.push_back(&freq_assigned_start_mhzIdx);
	columns << "freq_assigned_end_mhz";
	fieldIdxList.push_back(&freq_assigned_end_mhzIdx);
	columns << "tx_lat_deg";
	fieldIdxList.push_back(&tx_lat_degIdx);
	columns << "tx_long_deg";
	fieldIdxList.push_back(&tx_long_degIdx);
	columns << "tx_ground_elev_m";
	fieldIdxList.push_back(&tx_ground_elev_mIdx);
	columns << "tx_polarization";
	fieldIdxList.push_back(&tx_polarizationIdx);
	columns << "tx_gain";
	fieldIdxList.push_back(&tx_gainIdx);
	columns << "tx_eirp";
	fieldIdxList.push_back(&tx_eirpIdx);
	columns << "tx_height_to_center_raat_m";
	fieldIdxList.push_back(&tx_height_to_center_raat_mIdx);
	columns << "tx_architecture";
	fieldIdxList.push_back(&tx_architecture_mIdx);
	columns << "azimuth_angle_to_tx";
	fieldIdxList.push_back(&azimuth_angle_to_tx_mIdx);
	columns << "elevation_angle_to_tx";
	fieldIdxList.push_back(&elevation_angle_to_tx_mIdx);
	columns << "rx_lat_deg";
	fieldIdxList.push_back(&rx_lat_degIdx);
	columns << "rx_long_deg";
	fieldIdxList.push_back(&rx_long_degIdx);
	columns << "rx_ground_elev_m";
	fieldIdxList.push_back(&rx_ground_elev_mIdx);
	columns << "rx_height_to_center_raat_m";
	fieldIdxList.push_back(&rx_height_to_center_raat_mIdx);
	columns << "rx_line_loss";
	fieldIdxList.push_back(&rx_line_loss_mIdx);
	columns << "rx_gain";
	fieldIdxList.push_back(&rx_gainIdx);
	columns << "rx_ant_diameter";
	fieldIdxList.push_back(&rx_antennaDiameterIdx);
	columns << "rx_near_field_ant_diameter";
	fieldIdxList.push_back(&rx_near_field_ant_diameterIdx);
	columns << "rx_near_field_dist_limit";
	fieldIdxList.push_back(&rx_near_field_dist_limitIdx);
	columns << "rx_near_field_ant_efficiency";
	fieldIdxList.push_back(&rx_near_field_ant_efficiencyIdx);
	columns << "rx_ant_category";
	fieldIdxList.push_back(&rx_antennaCategoryIdx);
	columns << "status";
	fieldIdxList.push_back(&statusIdx);
	columns << "mobile";
	fieldIdxList.push_back(&mobileIdx);
	columns << "rx_ant_model";
	fieldIdxList.push_back(&rx_ant_modelNameIdx);
	columns << "rx_ant_model_idx";
	fieldIdxList.push_back(&rx_ant_model_idxIdx);

	columns << "rx_diversity_height_to_center_raat_m";
	fieldIdxList.push_back(&rx_diversity_height_to_center_raat_mIdx);
	columns << "rx_diversity_gain";
	fieldIdxList.push_back(&rx_diversity_gainIdx);
	columns << "rx_diversity_ant_diameter";
	fieldIdxList.push_back(&rx_diversity_antennaDiameterIdx);

	columns << "p_rp_num";
	fieldIdxList.push_back(&p_rp_numIdx);

	prColumns << "prSeq";                         prFieldIdxList.push_back(&prSeqIdx);
	prColumns << "pr_ant_type";                   prFieldIdxList.push_back(&prTypeIdx);
	prColumns << "pr_lat_deg";                    prFieldIdxList.push_back(&pr_lat_degIdx);
	prColumns << "pr_lon_deg";                    prFieldIdxList.push_back(&pr_lon_degIdx);
	prColumns << "pr_height_to_center_raat_tx_m"; prFieldIdxList.push_back(&pr_height_to_center_raat_tx_mIdx);
	prColumns << "pr_height_to_center_raat_rx_m"; prFieldIdxList.push_back(&pr_height_to_center_raat_rx_mIdx);

	prColumns << "pr_back_to_back_gain_tx";       prFieldIdxList.push_back(&prTxGainIdx);
	prColumns << "pr_ant_diameter_tx";            prFieldIdxList.push_back(&prTxDiameterIdx);
	prColumns << "pr_back_to_back_gain_rx";       prFieldIdxList.push_back(&prRxGainIdx);
	prColumns << "pr_ant_diameter_rx";            prFieldIdxList.push_back(&prRxDiameterIdx);
	prColumns << "pr_ant_category";               prFieldIdxList.push_back(&prAntCategoryIdx);
	prColumns << "pr_ant_model";                  prFieldIdxList.push_back(&prAntModelNameIdx);
	prColumns << "pr_ant_model_idx";              prFieldIdxList.push_back(&pr_ant_model_idxIdx);
	prColumns << "pr_reflector_height_m";         prFieldIdxList.push_back(&prReflectorHeightIdx);
	prColumns << "pr_reflector_width_m";          prFieldIdxList.push_back(&prReflectorWidthIdx);

	antnameColumns << "ant_idx";                  antnameFieldIdxList.push_back(&antname_ant_idxIdx);
	antnameColumns << "ant_name";                 antnameFieldIdxList.push_back(&antname_ant_nameIdx);

	antaobColumns << "aob_idx";                   antaobFieldIdxList.push_back(&antaob_aob_idxIdx);
	antaobColumns << "aob_deg";                   antaobFieldIdxList.push_back(&antaob_aob_degIdx);

	antgainColumns << "id";                       antgainFieldIdxList.push_back(&antgain_idIdx);
	antgainColumns << "gain_db";                  antgainFieldIdxList.push_back(&antgain_gainIdx);

    rasColumns << "rasid";                        rasFieldIdxList.push_back(&ras_rasidIdx);
    rasColumns << "region";                       rasFieldIdxList.push_back(&ras_regionIdx);
    rasColumns << "name";                         rasFieldIdxList.push_back(&ras_nameIdx);
    rasColumns << "location";                     rasFieldIdxList.push_back(&ras_locationIdx);
    rasColumns << "startFreqMHz";                 rasFieldIdxList.push_back(&ras_startFreqMHzIdx);
    rasColumns << "stopFreqMHz";                  rasFieldIdxList.push_back(&ras_stopFreqMHzIdx);
    rasColumns << "exclusionZone";                rasFieldIdxList.push_back(&ras_exclusionZoneIdx);
    rasColumns << "rect1lat1";                    rasFieldIdxList.push_back(&ras_rect1lat1Idx);
    rasColumns << "rect1lat2";                    rasFieldIdxList.push_back(&ras_rect1lat2Idx);
    rasColumns << "rect1lon1";                    rasFieldIdxList.push_back(&ras_rect1lon1Idx);
    rasColumns << "rect1lon2";                    rasFieldIdxList.push_back(&ras_rect1lon2Idx);
    rasColumns << "rect2lat1";                    rasFieldIdxList.push_back(&ras_rect2lat1Idx);
    rasColumns << "rect2lat2";                    rasFieldIdxList.push_back(&ras_rect2lat2Idx);
    rasColumns << "rect2lon1";                    rasFieldIdxList.push_back(&ras_rect2lon1Idx);
    rasColumns << "rect2lon2";                    rasFieldIdxList.push_back(&ras_rect2lon2Idx);
    rasColumns << "radiusKm";                     rasFieldIdxList.push_back(&ras_radiusKmIdx);
    rasColumns << "centerLat";                    rasFieldIdxList.push_back(&ras_centerLatIdx);
    rasColumns << "centerLon";                    rasFieldIdxList.push_back(&ras_centerLonIdx);
    rasColumns << "heightAGL";                    rasFieldIdxList.push_back(&ras_heightAGLIdx);

	int fIdx;
	for(fIdx=0; fIdx<(int) fieldIdxList.size(); ++fIdx) {
		*fieldIdxList[fIdx] = fIdx;
	}
	for(fIdx=0; fIdx<(int) prFieldIdxList.size(); ++fIdx) {
		*prFieldIdxList[fIdx] = fIdx;
	}
	for(fIdx=0; fIdx<(int) antnameFieldIdxList.size(); ++fIdx) {
		*antnameFieldIdxList[fIdx] = fIdx;
	}
	for(fIdx=0; fIdx<(int) antaobFieldIdxList.size(); ++fIdx) {
		*antaobFieldIdxList[fIdx] = fIdx;
	}
	for(fIdx=0; fIdx<(int) antgainFieldIdxList.size(); ++fIdx) {
		*antgainFieldIdxList[fIdx] = fIdx;
	}
	for(fIdx=0; fIdx<(int) rasFieldIdxList.size(); ++fIdx) {
		*rasFieldIdxList[fIdx] = fIdx;
	}
}
/******************************************************************************************/

/******************************************************************************************/
/**** DESTRUCTOR: UlsDatabase::~UlsDatabase()                                          ****/
/******************************************************************************************/
UlsDatabase::~UlsDatabase()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: UlsDatabase::nullInitialize                                            ****/
/**** Initialize all indices to -1 so coverity warnings are suppressed.                ****/
/******************************************************************************************/
void UlsDatabase::nullInitialize()
{
	fsidIdx = -1;
	regionIdx = -1;
	callsignIdx = -1;
	radio_serviceIdx = -1;
	nameIdx = -1;
	rx_callsignIdx = -1;
	rx_antenna_numIdx = -1;
	freq_assigned_start_mhzIdx = -1;
	freq_assigned_end_mhzIdx = -1;
	tx_lat_degIdx = -1;
	tx_long_degIdx = -1;
	tx_ground_elev_mIdx = -1;
	tx_polarizationIdx = -1;
	tx_gainIdx = -1;
	tx_eirpIdx = -1;
	tx_height_to_center_raat_mIdx = -1;
	tx_architecture_mIdx = -1;
	azimuth_angle_to_tx_mIdx = -1;
	elevation_angle_to_tx_mIdx = -1;
	rx_lat_degIdx = -1;
	rx_long_degIdx = -1;
	rx_ground_elev_mIdx = -1;
	rx_height_to_center_raat_mIdx = -1;
	rx_line_loss_mIdx = -1;
	rx_gainIdx = -1;
	rx_antennaDiameterIdx = -1;
	rx_near_field_ant_diameterIdx = -1;
	rx_near_field_dist_limitIdx = -1;
	rx_near_field_ant_efficiencyIdx = -1;
	rx_antennaCategoryIdx = -1;
	statusIdx = -1;
	mobileIdx = -1;
	rx_ant_modelNameIdx = -1;
	rx_ant_model_idxIdx = -1;
	p_rp_numIdx = -1;

	rx_diversity_height_to_center_raat_mIdx = -1;
	rx_diversity_gainIdx = -1;
	rx_diversity_antennaDiameterIdx = -1;

	prSeqIdx = -1;
	prTypeIdx = -1;
	pr_lat_degIdx = -1;
	pr_lon_degIdx = -1;
	pr_height_to_center_raat_rx_mIdx = -1;
	pr_height_to_center_raat_tx_mIdx = -1;

	prTxGainIdx = -1;
	prTxDiameterIdx = -1;
	prRxGainIdx = -1;
	prRxDiameterIdx = -1;
	prAntCategoryIdx = -1;
	prAntModelNameIdx = -1;
	pr_ant_model_idxIdx = -1;
	prReflectorHeightIdx = -1;
	prReflectorWidthIdx = -1;

    antname_ant_idxIdx = -1;
    antname_ant_nameIdx = -1;

    antaob_aob_idxIdx = -1;
    antaob_aob_degIdx = -1;

    antgain_idIdx = -1;
    antgain_gainIdx = -1;

    ras_rasidIdx = -1;
    ras_regionIdx = -1;
    ras_nameIdx = -1;
    ras_locationIdx = -1;
    ras_startFreqMHzIdx = -1;
    ras_stopFreqMHzIdx = -1;
    ras_exclusionZoneIdx = -1;
    ras_rect1lat1Idx = -1;
    ras_rect1lat2Idx = -1;
    ras_rect1lon1Idx = -1;
    ras_rect1lon2Idx = -1;
    ras_rect2lat1Idx = -1;
    ras_rect2lat2Idx = -1;
    ras_rect2lon1Idx = -1;
    ras_rect2lon2Idx = -1;
    ras_radiusKmIdx = -1;
    ras_centerLatIdx = -1;
    ras_centerLonIdx = -1;
    ras_heightAGLIdx = -1;

}
/******************************************************************************************/

void verifyResult(const QSqlQuery& ulsQueryRes)
{
	LOGGER_DEBUG(logger) << "Is Active: " << ulsQueryRes.isActive();
	LOGGER_DEBUG(logger) << "Is Select: " << ulsQueryRes.isSelect();
	if (!ulsQueryRes.isActive())
	{
		// Query encountered error
		QSqlError err = ulsQueryRes.lastError();
		throw std::runtime_error(ErrStream() << "UlsDatabase.cpp: Database query failed with code " << err.type() << " " << err.text());

	}
}


// construct and run sql query and return result
QSqlQuery runQueryWithBounds(const SqlScopedConnection<SqlExceptionDb>& db,
	const QStringList& columns, const double& minLat, const double& maxLat, const double& minLon, const double& maxLon);
QSqlQuery runQueryById(const SqlScopedConnection<SqlExceptionDb>& db, const QStringList& columns, const int& fsid);

void UlsDatabase::loadFSById(const QString& dbName, std::vector<RASClass *>& rasList, std::vector<AntennaClass *>& antennaList, std::vector<UlsRecord>& target, const int& fsid)
{
	LOGGER_DEBUG(logger) << "FSID: " << fsid;

	// create and open db connection
	SqlConnectionDefinition config;
	config.driverName = "QSQLITE";
	config.dbName = dbName;

	LOGGER_INFO(logger) << "Opening database: " << dbName;
	SqlScopedConnection<SqlExceptionDb> db(new SqlExceptionDb(config.newConnection()));
	db->tryOpen();

	LOGGER_DEBUG(logger) << "Querying uls database";
	QSqlQuery ulsQueryRes = runQueryById(db, columns, fsid);

	verifyResult(ulsQueryRes);

	fillTarget(db, rasList, antennaList, target, ulsQueryRes);

}

void UlsDatabase::loadUlsData(const QString& dbName, std::vector<RASClass *>& rasList, std::vector<AntennaClass *>& antennaList, std::vector<UlsRecord>& target,
	const double& minLat, const double& maxLat, const double& minLon, const double& maxLon)
{
	LOGGER_DEBUG(logger) << "Bounds: " << minLat << ", " << maxLat << "; " << minLon << ", " << maxLon;

	// create and open db connection
	SqlConnectionDefinition config;
	config.driverName = "QSQLITE";
	config.dbName = dbName;

	LOGGER_INFO(logger) << "Opening database: " << dbName;
	SqlScopedConnection<SqlExceptionDb> db(new SqlExceptionDb(config.newConnection()));
	db->tryOpen();

	LOGGER_DEBUG(logger) << "Querying uls database";
	QSqlQuery ulsQueryRes = runQueryWithBounds(db, columns, minLat, maxLat, minLon, maxLon);

	verifyResult(ulsQueryRes);

	fillTarget(db, rasList, antennaList, target, ulsQueryRes);
}

QSqlQuery runQueryWithBounds(const SqlScopedConnection<SqlExceptionDb>& db,
	const QStringList& columns, const double& minLat, const double& maxLat, const double& minLon, const double& maxLon)
{
	return SqlSelect(*db, "uls")
		.cols(columns)
		.where(QString(
			"(rx_lat_deg BETWEEN %1 AND %2)"
			"AND"
			"(rx_long_deg BETWEEN %3 AND %4)"
		)
		.arg(std::min(minLat, maxLat))
		.arg(std::max(minLat, maxLat))
		.arg(std::min(minLon, maxLon))
		.arg(std::max(minLon, maxLon))
		)
		.order("fsid")
		.run();
}

QSqlQuery runQueryById(const SqlScopedConnection<SqlExceptionDb>& db, const QStringList& columns, const int& fsid)
{
	return SqlSelect(*db, "uls")
		.cols(columns)
		.where(QString("fsid=%1").arg(fsid))
		.topmost(1)
		.run();
}

void UlsDatabase::fillTarget(SqlScopedConnection<SqlExceptionDb>& db, std::vector<RASClass *>& rasList, std::vector<AntennaClass *>& antennaList, std::vector<UlsRecord>& target, QSqlQuery& q)
{
	// resize vector to fit result
	if (q.driver()->hasFeature(QSqlDriver::QuerySize))
	{
		// if the driver supports .size() then use it because is is more efficient
		LOGGER_DEBUG(logger) << target.size() << " to " << q.size();
		target.resize(q.size());
		q.setForwardOnly(true);
	}
	else
	{
		if (!q.last())
		{
			// throw std::runtime_error(ErrStream() << "UlsDatabase.cpp: Failed to get last item. Check that lat/lon are within CONUS : " << q.at());
			// No FS's within 150 Km, return with empty list
			return;
		}
		LOGGER_DEBUG(logger) << target.size() << " to last " << q.at();
		target.resize(q.at()+1);
		q.first();
		q.previous();
	}

	/**************************************************************************************/
	/* Read RAS Table                                                                     */
	/**************************************************************************************/
	QSqlQuery rasQueryRes = SqlSelect(*db, "ras")
									.cols(rasColumns)
									.run();
	int numRAS;
	// resize vector to fit result
	if (rasQueryRes.driver()->hasFeature(QSqlDriver::QuerySize)) {
		// if the driver supports .size() then use it because is is more efficient
		numRAS = rasQueryRes.size();
		rasQueryRes.setForwardOnly(true);
	} else {
		if (!rasQueryRes.last()) {
			numRAS = 0;
		} else {
			numRAS = rasQueryRes.at()+1;
			rasQueryRes.first();
			rasQueryRes.previous();
		}
	}

	while (rasQueryRes.next()) {
		int rasid = rasQueryRes.value(ras_rasidIdx).toInt();
		std::string exclusionZoneStr = rasQueryRes.value(ras_exclusionZoneIdx).toString().toStdString();
		RASClass::RASExclusionZoneTypeEnum exclusionZoneType;

		if (exclusionZoneStr == "One Rectangle") {
			exclusionZoneType = RASClass::rectRASExclusionZoneType;
		} else if (exclusionZoneStr == "Two Rectangles") {
			exclusionZoneType = RASClass::rect2RASExclusionZoneType;
		} else if (exclusionZoneStr == "Circle") {
			exclusionZoneType = RASClass::circleRASExclusionZoneType;
		} else if (exclusionZoneStr == "Horizon Distance") {
			exclusionZoneType = RASClass::horizonDistRASExclusionZoneType;
		} else {
			CORE_DUMP;
		}

        RASClass *ras;
        switch(exclusionZoneType) {
            case RASClass::rectRASExclusionZoneType:
            case RASClass::rect2RASExclusionZoneType:
            {
                ras = (RASClass *) new RectRASClass(rasid);

			    double rect1lat1 = rasQueryRes.value(ras_rect1lat1Idx).toDouble();
			    double rect1lat2 = rasQueryRes.value(ras_rect1lat2Idx).toDouble();
			    double rect1lon1 = rasQueryRes.value(ras_rect1lon1Idx).toDouble();
			    double rect1lon2 = rasQueryRes.value(ras_rect1lon2Idx).toDouble();

                ((RectRASClass *) ras)->addRect(rect1lon1, rect1lon2, rect1lat1, rect1lat2);

                if (exclusionZoneType == RASClass::rect2RASExclusionZoneType) {
			        double rect2lat1 = rasQueryRes.value(ras_rect2lat1Idx).toDouble();
			        double rect2lat2 = rasQueryRes.value(ras_rect2lat2Idx).toDouble();
			        double rect2lon1 = rasQueryRes.value(ras_rect2lon1Idx).toDouble();
			        double rect2lon2 = rasQueryRes.value(ras_rect2lon2Idx).toDouble();

                    ((RectRASClass *) ras)->addRect(rect2lon1, rect2lon2, rect2lat1, rect2lat2);
                }
            }
                break;
            case RASClass::circleRASExclusionZoneType:
            case RASClass::horizonDistRASExclusionZoneType:
            {
                double lonCircle = rasQueryRes.value(ras_centerLonIdx).toDouble();
                double latCircle = rasQueryRes.value(ras_centerLatIdx).toDouble();

                bool horizonDistFlag = (exclusionZoneType == RASClass::horizonDistRASExclusionZoneType);

                ras = (RASClass *) new CircleRASClass(rasid, horizonDistFlag);

                ((CircleRASClass *) ras)->setLongitudeCenter(lonCircle);
                ((CircleRASClass *) ras)->setLatitudeCenter(latCircle);

                if (!horizonDistFlag) {
                    double radius = rasQueryRes.value(ras_radiusKmIdx).isNull() ? quietNaN
                                  : rasQueryRes.value(ras_radiusKmIdx).toDouble()*1.0e3; // Convert km to m

                    ((CircleRASClass *) ras)->setRadius(radius);
                } else {
                    /**************************************************************************/
                    /* heightAGL                                                              */
                    /**************************************************************************/
                    double heightAGL = rasQueryRes.value(ras_heightAGLIdx).isNull() ? quietNaN
                                  : rasQueryRes.value(ras_heightAGLIdx).toDouble()*1.0e3; // Convert km to m
                    ras->setHeightAGL(heightAGL);
                    /**************************************************************************/
                }
            }
                break;
            default:
                CORE_DUMP;
                break;
        }
        double startFreq = rasQueryRes.value(ras_startFreqMHzIdx).isNull() ? quietNaN
                      : rasQueryRes.value(ras_startFreqMHzIdx).toDouble()*1.0e6; // Convert MHz to Hz
        double stopFreq = rasQueryRes.value(ras_stopFreqMHzIdx).isNull() ? quietNaN
                      : rasQueryRes.value(ras_stopFreqMHzIdx).toDouble()*1.0e6; // Convert MHz to Hz

        ras->setStartFreq(startFreq);
        ras->setStopFreq(stopFreq);

        rasList.push_back(ras);
	}
	LOGGER_DEBUG(logger) << "READ " << numRAS << " entries from database ";
	/**************************************************************************************/

	/**************************************************************************************/
	/* Get list of antenna names                                                          */
	/**************************************************************************************/
	QSqlQuery antnameQueryRes = SqlSelect(*db, "antname")
									.cols(antnameColumns)
									.run();
	int numAntennaDB;
	// resize vector to fit result
	if (antnameQueryRes.driver()->hasFeature(QSqlDriver::QuerySize)) {
		// if the driver supports .size() then use it because is is more efficient
		numAntennaDB = antnameQueryRes.size();
		antnameQueryRes.setForwardOnly(true);
	} else {
		if (!antnameQueryRes.last()) {
			numAntennaDB = 0;
		} else {
			numAntennaDB = antnameQueryRes.at()+1;
			antnameQueryRes.first();
			antnameQueryRes.previous();
		}
	}

	std::vector<int> antennaIdxMap;
	std::vector<std::string> antennaNameList;

	for(int antIdxDB=0; antIdxDB < numAntennaDB; ++antIdxDB) {
		antennaIdxMap.push_back(-1);
		antennaNameList.push_back("");
	}

	while (antnameQueryRes.next()) {
		int antIdxDB = antnameQueryRes.value(antname_ant_idxIdx).toInt();
		std::string antennaName = antnameQueryRes.value(antname_ant_nameIdx).toString().toStdString();
		antennaNameList[antIdxDB] = antennaName;
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Get list of antenna aob values                                                     */
	/**************************************************************************************/
	std::vector<double> antennaAOBList;
	if (numAntennaDB) {
		QSqlQuery antaobQueryRes = SqlSelect(*db, "antaob")
										.cols(antaobColumns)
										.run();
		int numAntennaAOB;
		// resize vector to fit result
		if (antaobQueryRes.driver()->hasFeature(QSqlDriver::QuerySize)) {
			// if the driver supports .size() then use it because is is more efficient
			numAntennaAOB = antaobQueryRes.size();
			antaobQueryRes.setForwardOnly(true);
		} else {
			if (!antaobQueryRes.last()) {
				numAntennaAOB = 0;
			} else {
				numAntennaAOB = antaobQueryRes.at()+1;
				antaobQueryRes.first();
				antaobQueryRes.previous();
			}
		}

		for(int aobIdx=0; aobIdx < numAntennaAOB; ++aobIdx) {
			antennaAOBList.push_back(quietNaN);
		}

		while (antaobQueryRes.next()) {
			int aobIdx = antaobQueryRes.value(antaob_aob_idxIdx).toInt();
			double aobRad = antaobQueryRes.value(antaob_aob_degIdx).toDouble()*M_PI/180.0;
			antennaAOBList[aobIdx] = aobRad;
		}
	}
	/**************************************************************************************/

	while (q.next())
	{
		int r = q.at();
		int fsid = q.value(fsidIdx).toInt();
		int numPR = q.value(p_rp_numIdx).toInt();

		target.at(r).fsid = fsid;
		target.at(r).region= q.value(regionIdx).toString().toStdString();
		target.at(r).callsign = q.value(callsignIdx).toString().toStdString();
		target.at(r).radioService = q.value(radio_serviceIdx).toString().toStdString();
		target.at(r).entityName = q.value(nameIdx).toString().toStdString();
		target.at(r).rxCallsign = q.value(rx_callsignIdx).toString().toStdString();
		target.at(r).rxAntennaNumber = q.value(rx_antenna_numIdx).toInt();
		target.at(r).startFreq = q.value(freq_assigned_start_mhzIdx).toDouble();
		target.at(r).stopFreq = q.value(freq_assigned_end_mhzIdx).toDouble();
		target.at(r).txLatitudeDeg  = q.value(tx_lat_degIdx).isNull() ? quietNaN : q.value(tx_lat_degIdx).toDouble();
		target.at(r).txLongitudeDeg = q.value(tx_long_degIdx).isNull() ? quietNaN : q.value(tx_long_degIdx).toDouble();
		target.at(r).txGroundElevation = q.value(tx_ground_elev_mIdx).isNull() ? quietNaN : q.value(tx_ground_elev_mIdx).toDouble();
		target.at(r).txPolarization = q.value(tx_polarizationIdx).toString().toStdString();
		target.at(r).txGain = q.value(tx_gainIdx).isNull() ? quietNaN : q.value(tx_gainIdx).toDouble();
		target.at(r).txEIRP = q.value(tx_eirpIdx).toDouble();
		target.at(r).txHeightAboveTerrain = q.value(tx_height_to_center_raat_mIdx).isNull() ? quietNaN : q.value(tx_height_to_center_raat_mIdx).toDouble();
		target.at(r).txArchitecture = q.value(tx_architecture_mIdx).toString().toStdString();
		target.at(r).azimuthAngleToTx = q.value(azimuth_angle_to_tx_mIdx).isNull() ? quietNaN : q.value(azimuth_angle_to_tx_mIdx).toDouble();
		target.at(r).elevationAngleToTx = q.value(elevation_angle_to_tx_mIdx).isNull() ? quietNaN : q.value(elevation_angle_to_tx_mIdx).toDouble();
		target.at(r).rxLatitudeDeg = q.value(rx_lat_degIdx).toDouble();
		target.at(r).rxLongitudeDeg = q.value(rx_long_degIdx).toDouble();
		target.at(r).rxGroundElevation = q.value(rx_ground_elev_mIdx).isNull() ? quietNaN : q.value(rx_ground_elev_mIdx).toDouble();
		target.at(r).rxHeightAboveTerrain = q.value(rx_height_to_center_raat_mIdx).isNull() ? quietNaN : q.value(rx_height_to_center_raat_mIdx).toDouble();
		target.at(r).rxLineLoss = q.value(rx_line_loss_mIdx).isNull() ? quietNaN : q.value(rx_line_loss_mIdx).toDouble();
		target.at(r).rxGain = q.value(rx_gainIdx).isNull() ? quietNaN : q.value(rx_gainIdx).toDouble();
		target.at(r).rxAntennaDiameter = q.value(rx_antennaDiameterIdx).isNull() ? quietNaN : q.value(rx_antennaDiameterIdx).toDouble();

		target.at(r).rxNearFieldAntDiameter = q.value(rx_near_field_ant_diameterIdx).isNull() ? quietNaN : q.value(rx_near_field_ant_diameterIdx).toDouble();
		target.at(r).rxNearFieldDistLimit = q.value(rx_near_field_dist_limitIdx).isNull() ? quietNaN : q.value(rx_near_field_dist_limitIdx).toDouble();
		target.at(r).rxNearFieldAntEfficiency = q.value(rx_near_field_ant_efficiencyIdx).isNull() ? quietNaN : q.value(rx_near_field_ant_efficiencyIdx).toDouble();

		target.at(r).hasDiversity = q.value(rx_diversity_gainIdx).isNull() ? false : true;
		target.at(r).diversityGain = q.value(rx_diversity_gainIdx).isNull() ? quietNaN : q.value(rx_diversity_gainIdx).toDouble();
		target.at(r).diversityHeightAboveTerrain = q.value(rx_diversity_height_to_center_raat_mIdx).isNull() ? quietNaN : q.value(rx_diversity_height_to_center_raat_mIdx).toDouble();
		target.at(r).diversityAntennaDiameter = q.value(rx_diversity_antennaDiameterIdx).isNull() ? quietNaN : q.value(rx_diversity_antennaDiameterIdx).toDouble();

		target.at(r).status = q.value(statusIdx).toString().toStdString();
		target.at(r).mobile = q.value(mobileIdx).toBool();
		target.at(r).rxAntennaModelName = q.value(rx_ant_modelNameIdx).toString().toStdString();

		int rxAntennaIdxDB = q.value(rx_ant_model_idxIdx).toInt();

		AntennaClass *antennaPattern = (AntennaClass *) NULL;

		if (rxAntennaIdxDB != -1) {
			if (antennaIdxMap[rxAntennaIdxDB] == -1) {
				antennaPattern = createAntennaPattern(db, rxAntennaIdxDB, antennaAOBList, antennaNameList[rxAntennaIdxDB]);
				antennaIdxMap[rxAntennaIdxDB] = antennaList.size();
				antennaList.push_back(antennaPattern);
			} else {
				antennaPattern = antennaList[antennaIdxMap[rxAntennaIdxDB]];
			}
		}
		target.at(r).rxAntenna = antennaPattern;

		target.at(r).numPR = numPR;

		std::string rxAntennaCategoryStr = q.value(rx_antennaCategoryIdx).toString().toStdString();
		CConst::AntennaCategoryEnum rxAntennaCategory;
		if (rxAntennaCategoryStr == "B1") {
			rxAntennaCategory = CConst::B1AntennaCategory;
		} else if (rxAntennaCategoryStr == "HP") {
			rxAntennaCategory = CConst::HPAntennaCategory;
		} else if (rxAntennaCategoryStr == "OTHER") {
			rxAntennaCategory = CConst::OtherAntennaCategory;
		} else {
			rxAntennaCategory = CConst::UnknownAntennaCategory;
		}
		target.at(r).rxAntennaCategory = rxAntennaCategory;

		if (numPR) {
			target.at(r).prType = std::vector<std::string>(numPR);
			target.at(r).prLatitudeDeg = std::vector<double>(numPR);
			target.at(r).prLongitudeDeg = std::vector<double>(numPR);
			target.at(r).prHeightAboveTerrainTx = std::vector<double>(numPR);
			target.at(r).prHeightAboveTerrainRx = std::vector<double>(numPR);

			target.at(r).prTxGain = std::vector<double>(numPR);
			target.at(r).prTxAntennaDiameter = std::vector<double>(numPR);
			target.at(r).prRxGain = std::vector<double>(numPR);
			target.at(r).prRxAntennaDiameter = std::vector<double>(numPR);
			target.at(r).prAntCategory = std::vector<CConst::AntennaCategoryEnum>(numPR);
			target.at(r).prAntModelName = std::vector<std::string>(numPR);

			target.at(r).prReflectorHeight = std::vector<double>(numPR);
			target.at(r).prReflectorWidth = std::vector<double>(numPR);
			target.at(r).prAntenna = std::vector<AntennaClass *>(numPR);

			QSqlQuery prQueryRes = SqlSelect(*db, "pr")
										.cols(prColumns)
										.where(QString("fsid=%1").arg(fsid))
										.run();

			int querySize;
			// resize vector to fit result
			if (prQueryRes.driver()->hasFeature(QSqlDriver::QuerySize)) {
				// if the driver supports .size() then use it because is is more efficient
				querySize = prQueryRes.size();
				prQueryRes.setForwardOnly(true);
			} else {
				if (!prQueryRes.last()) {
					querySize = 0;
				} else {
					querySize = prQueryRes.at()+1;
					prQueryRes.first();
					prQueryRes.previous();
				}
			}

			if (querySize != numPR) {
				throw std::runtime_error(ErrStream() << "UlsDatabase.cpp: Inconsistent numPR for FSID = " << fsid);
			}

			while (prQueryRes.next()) {
				int prSeq = prQueryRes.value(prSeqIdx).toInt();
				int prIdx = prSeq - 1;

				target.at(r).prType[prIdx]  = prQueryRes.value(prTypeIdx).isNull() ? "" : prQueryRes.value(prTypeIdx).toString().toStdString();
				target.at(r).prLatitudeDeg[prIdx]  = prQueryRes.value(pr_lat_degIdx).isNull() ? quietNaN : prQueryRes.value(pr_lat_degIdx).toDouble();
				target.at(r).prLongitudeDeg[prIdx] = prQueryRes.value(pr_lon_degIdx).isNull() ? quietNaN : prQueryRes.value(pr_lon_degIdx).toDouble();
				target.at(r).prHeightAboveTerrainTx[prIdx] = prQueryRes.value(pr_height_to_center_raat_tx_mIdx).isNull() ? quietNaN : prQueryRes.value(pr_height_to_center_raat_tx_mIdx).toDouble();
				target.at(r).prHeightAboveTerrainRx[prIdx] = prQueryRes.value(pr_height_to_center_raat_rx_mIdx).isNull() ? quietNaN : prQueryRes.value(pr_height_to_center_raat_rx_mIdx).toDouble();

				target.at(r).prTxGain[prIdx]            = prQueryRes.value(prTxGainIdx    ).isNull() ? quietNaN : prQueryRes.value(prTxGainIdx    ).toDouble();
				target.at(r).prTxAntennaDiameter[prIdx] = prQueryRes.value(prTxDiameterIdx).isNull() ? quietNaN : prQueryRes.value(prTxDiameterIdx).toDouble();
				target.at(r).prRxGain[prIdx]            = prQueryRes.value(prRxGainIdx    ).isNull() ? quietNaN : prQueryRes.value(prRxGainIdx    ).toDouble();
				target.at(r).prRxAntennaDiameter[prIdx] = prQueryRes.value(prRxDiameterIdx).isNull() ? quietNaN : prQueryRes.value(prRxDiameterIdx).toDouble();

				std::string prAntCategoryStr = prQueryRes.value(prAntCategoryIdx).toString().toStdString();
				CConst::AntennaCategoryEnum prAntCategory;
				if (prAntCategoryStr == "B1") {
					prAntCategory = CConst::B1AntennaCategory;
				} else if (prAntCategoryStr == "HP") {
					prAntCategory = CConst::HPAntennaCategory;
				} else if (prAntCategoryStr == "OTHER") {
					prAntCategory = CConst::OtherAntennaCategory;
				} else {
					prAntCategory = CConst::UnknownAntennaCategory;
				}
				target.at(r).prAntCategory[prIdx] = prAntCategory;

				target.at(r).prAntModelName[prIdx] = prQueryRes.value(prAntModelNameIdx).toString().toStdString();

				target.at(r).prReflectorHeight[prIdx]   = prQueryRes.value(prReflectorHeightIdx).isNull() ? quietNaN : prQueryRes.value(prReflectorHeightIdx).toDouble();
				target.at(r).prReflectorWidth[prIdx]    = prQueryRes.value(prReflectorWidthIdx ).isNull() ? quietNaN : prQueryRes.value(prReflectorWidthIdx ).toDouble();

				int prAntennaIdxDB = prQueryRes.value(pr_ant_model_idxIdx).toInt();

				antennaPattern = (AntennaClass *) NULL;

				if (prAntennaIdxDB != -1) {
					if (antennaIdxMap[prAntennaIdxDB] == -1) {
						antennaPattern = createAntennaPattern(db, prAntennaIdxDB, antennaAOBList, antennaNameList[prAntennaIdxDB]);
						antennaIdxMap[prAntennaIdxDB] = antennaList.size();
						antennaList.push_back(antennaPattern);
					} else {
						antennaPattern = antennaList[antennaIdxMap[prAntennaIdxDB]];
					}
				}
				target.at(r).prAntenna[prIdx] = antennaPattern;
			}
		}
	}
	LOGGER_DEBUG(logger) << target.size() << " rows retreived";
}

AntennaClass *UlsDatabase::createAntennaPattern(SqlScopedConnection<SqlExceptionDb>& db, int rxAntennaIdxDB, std::vector<double> antennaAOBList, std::string antennaName)
{
    int numAntennaAOB = antennaAOBList.size();
    int idmin = numAntennaAOB*rxAntennaIdxDB;
    int idmax = idmin + numAntennaAOB - 1;
	QSqlQuery antgainQueryRes = SqlSelect(*db, "antgain")
		.cols(antgainColumns)
		.where(QString(
			"(id BETWEEN %1 AND %2)"
		)
		.arg(idmin)
		.arg(idmax)
		)
		.order("id")
		.run();

	int querySize;
	// resize vector to fit result
	if (antgainQueryRes.driver()->hasFeature(QSqlDriver::QuerySize)) {
		// if the driver supports .size() then use it because is is more efficient
		querySize = antgainQueryRes.size();
		antgainQueryRes.setForwardOnly(true);
	} else {
		if (!antgainQueryRes.last()) {
			querySize = 0;
		} else {
			querySize = antgainQueryRes.at()+1;
			antgainQueryRes.first();
			antgainQueryRes.previous();
		}
	}

	if (querySize != numAntennaAOB) {
	    LOGGER_DEBUG(logger) << "ERROR Creating antenna " << antennaName << ": numAntennaAOB = " << numAntennaAOB << ", querySize = " << querySize;
	}

    std::vector<std::tuple<double, double>> sampledData;

	std::tuple<double, double> pt;
	std::get<1>(pt) = quietNaN;
	for(int aobIdx=0; aobIdx<numAntennaAOB-1; ++aobIdx) {
        std::get<0>(pt) = antennaAOBList[aobIdx];
        sampledData.push_back(pt);
	}

	while (antgainQueryRes.next()) {
		int id = antgainQueryRes.value(antgain_idIdx).toInt();
		double gain = antgainQueryRes.value(antgain_gainIdx).toDouble();
        int aobIdx = id - idmin;
        std::get<1>(sampledData[aobIdx]) = gain;
	}

	AntennaClass *antenna = new AntennaClass(CConst::antennaLUT_Boresight, antennaName.c_str());

	LinInterpClass *gainTable = new LinInterpClass(sampledData);

	antenna->setBoresightGainTable(gainTable);

    return(antenna);
}
