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

	columns << "fsid";
	fieldIdxList.push_back(&fsidIdx);

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
	columns << "emissions_des";
	fieldIdxList.push_back(&emissions_desIdx);
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
	fieldIdxList.push_back(&rx_ant_modelIdx);

	columns << "rx_diversity_height_to_center_raat_m";
	fieldIdxList.push_back(&rx_diversity_height_to_center_raat_mIdx);
	columns << "rx_diversity_gain";
	fieldIdxList.push_back(&rx_diversity_gainIdx);
	columns << "rx_diversity_ant_diameter";
	fieldIdxList.push_back(&rx_diversity_antennaDiameterIdx);

	columns << "p_rp_num";
	fieldIdxList.push_back(&p_rp_numIdx);

	prColumns << "prSeq";                      prFieldIdxList.push_back(&prSeqIdx);
	prColumns << "pr_ant_type";                prFieldIdxList.push_back(&prTypeIdx);
	prColumns << "pr_lat_deg";                 prFieldIdxList.push_back(&pr_lat_degIdx);
	prColumns << "pr_lon_deg";                 prFieldIdxList.push_back(&pr_lon_degIdx);
	prColumns << "pr_height_to_center_raat_m"; prFieldIdxList.push_back(&pr_height_to_center_raat_mIdx);

	prColumns << "pr_back_to_back_gain_tx";    prFieldIdxList.push_back(&prTxGainIdx);
	prColumns << "pr_ant_diameter_tx";         prFieldIdxList.push_back(&prTxDiameterIdx);
	prColumns << "pr_back_to_back_gain_rx";    prFieldIdxList.push_back(&prRxGainIdx);
	prColumns << "pr_ant_diameter_rx";         prFieldIdxList.push_back(&prRxDiameterIdx);
	prColumns << "pr_ant_category";            prFieldIdxList.push_back(&prAntCategoryIdx);
	prColumns << "pr_ant_model";               prFieldIdxList.push_back(&prAntModelIdx);
	prColumns << "pr_reflector_height_m";      prFieldIdxList.push_back(&prReflectorHeightIdx);
	prColumns << "pr_reflector_width_m";       prFieldIdxList.push_back(&prReflectorWidthIdx);

	int fIdx;
	for(fIdx=0; fIdx<(int) fieldIdxList.size(); ++fIdx) {
		*fieldIdxList[fIdx] = fIdx;
	}
	for(fIdx=0; fIdx<(int) prFieldIdxList.size(); ++fIdx) {
		*prFieldIdxList[fIdx] = fIdx;
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
	callsignIdx = -1;
	radio_serviceIdx = -1;
	nameIdx = -1;
	rx_callsignIdx = -1;
	rx_antenna_numIdx = -1;
	freq_assigned_start_mhzIdx = -1;
	freq_assigned_end_mhzIdx = -1;
	emissions_desIdx = -1;
	tx_lat_degIdx = -1;
	tx_long_degIdx = -1;
	tx_ground_elev_mIdx = -1;
	tx_polarizationIdx = -1;
	tx_gainIdx = -1;
	tx_eirpIdx = -1;
	tx_height_to_center_raat_mIdx = -1;
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
	rx_ant_modelIdx = -1;
	p_rp_numIdx = -1;

	rx_diversity_height_to_center_raat_mIdx = -1;
	rx_diversity_gainIdx = -1;
	rx_diversity_antennaDiameterIdx = -1;

	prSeqIdx = -1;
	prTypeIdx = -1;
	pr_lat_degIdx = -1;
	pr_lon_degIdx = -1;
	pr_height_to_center_raat_mIdx = -1;

	prTxGainIdx = -1;
	prTxDiameterIdx = -1;
	prRxGainIdx = -1;
	prRxDiameterIdx = -1;
	prAntCategoryIdx = -1;
	prAntModelIdx = -1;
	prReflectorHeightIdx = -1;
	prReflectorWidthIdx = -1;
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

void UlsDatabase::loadFSById(const QString& dbName, std::vector<UlsRecord>& target, const int& fsid)
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

	fillTarget(db, target, ulsQueryRes);

}

void UlsDatabase::loadUlsData(const QString& dbName, std::vector<UlsRecord>& target,
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

	fillTarget(db, target, ulsQueryRes);
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

void UlsDatabase::fillTarget(SqlScopedConnection<SqlExceptionDb>& db, std::vector<UlsRecord>& target, QSqlQuery& q)
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

	while (q.next())
	{
		int r = q.at();
		int fsid = q.value(fsidIdx).toInt();
		int numPR = q.value(p_rp_numIdx).toInt();

		target.at(r).fsid = fsid;
		target.at(r).callsign = q.value(callsignIdx).toString().toStdString();
		target.at(r).radioService = q.value(radio_serviceIdx).toString().toStdString();
		target.at(r).entityName = q.value(nameIdx).toString().toStdString();
		target.at(r).rxCallsign = q.value(rx_callsignIdx).toString().toStdString();
		target.at(r).rxAntennaNumber = q.value(rx_antenna_numIdx).toInt();
		target.at(r).startFreq = q.value(freq_assigned_start_mhzIdx).toDouble();
		target.at(r).stopFreq = q.value(freq_assigned_end_mhzIdx).toDouble();
		target.at(r).emissionsDesignator = q.value(emissions_desIdx).toString().toStdString();
		target.at(r).txLatitudeDeg = q.value(tx_lat_degIdx).toDouble();
		target.at(r).txLongitudeDeg = q.value(tx_long_degIdx).toDouble();
		target.at(r).txGroundElevation = q.value(tx_ground_elev_mIdx).isNull() ? quietNaN : q.value(tx_ground_elev_mIdx).toDouble();
		target.at(r).txPolarization = q.value(tx_polarizationIdx).toString().toStdString();
		target.at(r).txGain = q.value(tx_gainIdx).isNull() ? quietNaN : q.value(tx_gainIdx).toDouble();
		target.at(r).txEIRP = q.value(tx_eirpIdx).toDouble();
		target.at(r).txHeightAboveTerrain = q.value(tx_height_to_center_raat_mIdx).isNull() ? quietNaN : q.value(tx_height_to_center_raat_mIdx).toDouble();
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
		target.at(r).rxAntennaModel = q.value(rx_ant_modelIdx).toString().toStdString();
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
			target.at(r).prHeightAboveTerrain = std::vector<double>(numPR);

			target.at(r).prTxGain = std::vector<double>(numPR);
			target.at(r).prTxAntennaDiameter = std::vector<double>(numPR);
			target.at(r).prRxGain = std::vector<double>(numPR);
			target.at(r).prRxAntennaDiameter = std::vector<double>(numPR);
			target.at(r).prAntCategory = std::vector<CConst::AntennaCategoryEnum>(numPR);
			target.at(r).prAntModel = std::vector<std::string>(numPR);

			target.at(r).prReflectorHeight = std::vector<double>(numPR);
			target.at(r).prReflectorWidth = std::vector<double>(numPR);

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
				target.at(r).prHeightAboveTerrain[prIdx] = prQueryRes.value(pr_height_to_center_raat_mIdx).isNull() ? quietNaN : prQueryRes.value(pr_height_to_center_raat_mIdx).toDouble();

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

				target.at(r).prAntModel[prIdx] = prQueryRes.value(prAntModelIdx).toString().toStdString();

				target.at(r).prReflectorHeight[prIdx]   = prQueryRes.value(prReflectorHeightIdx).isNull() ? quietNaN : prQueryRes.value(prReflectorHeightIdx).toDouble();
				target.at(r).prReflectorWidth[prIdx]    = prQueryRes.value(prReflectorWidthIdx ).isNull() ? quietNaN : prQueryRes.value(prReflectorWidthIdx ).toDouble();
			}
		}
	}
	LOGGER_DEBUG(logger) << target.size() << " rows retreived";
}
