// UlsDatabase.h: header file for reading ULS database data on startup
// author: Sam Smucny

#ifndef AFCENGINE_ULS_DATABASE_H_
#define AFCENGINE_ULS_DATABASE_H_

#include <vector>
#include <string>
#include <QString>
#include <QStringList>
#include <QSqlQuery>
#include <exception>
#include <afcsql/SqlScopedConnection.h>
#include <afcsql/SqlExceptionDb.h>

#include "denied_region.h"
#include "antenna.h"
#include "cconst.h"

const int maxNumPR = 3;

struct UlsRecord
{
	int fsid;

	std::string region;
	std::string callsign;
	int pathNumber;
	std::string radioService;
	std::string entityName;
	std::string rxCallsign;
	int rxAntennaNumber;
	double startFreq, stopFreq;
	double txLatitudeDeg, txLongitudeDeg;
	double txGroundElevation;
	std::string txPolarization;
	double txGain;
	double txEIRP;
	double txHeightAboveTerrain;
	std::string txArchitecture;
	double azimuthAngleToTx;
	double elevationAngleToTx;
	double rxLatitudeDeg, rxLongitudeDeg;
	double rxGroundElevation;
	double rxHeightAboveTerrain;
	double rxLineLoss;
	double rxGain;
	CConst::AntennaCategoryEnum rxAntennaCategory;
	double rxAntennaDiameter;
	double rxNearFieldAntDiameter;
	double rxNearFieldDistLimit;
	double rxNearFieldAntEfficiency;

	bool hasDiversity;
	double diversityGain;
	double diversityAntennaDiameter;
	double diversityHeightAboveTerrain;

	std::string status;
	bool mobile;
	std::string rxAntennaModelName;
	AntennaClass *rxAntenna;
	int numPR;
	std::vector<double> prLatitudeDeg;
	std::vector<double> prLongitudeDeg;
	std::vector<double> prHeightAboveTerrainTx;
	std::vector<double> prHeightAboveTerrainRx;
	std::vector<std::string> prType;

	std::vector<double> prTxGain;
	std::vector<double> prTxAntennaDiameter;
	std::vector<double> prRxGain;
	std::vector<double> prRxAntennaDiameter;
	std::vector<CConst::AntennaCategoryEnum> prAntCategory;
	std::vector<std::string> prAntModelName;
	std::vector<AntennaClass *> prAntenna;

	std::vector<double> prReflectorHeight;
	std::vector<double> prReflectorWidth;

};

class UlsDatabase
{
public:
	UlsDatabase();
	~UlsDatabase();

	void nullInitialize();

	// Loads all FS within lat/lon bounds
	void loadUlsData(const QString& dbName, std::vector<DeniedRegionClass *>& deniedRegionList, std::vector<AntennaClass *>& antennaList, std::vector<UlsRecord>& target,
	                 const double& minLat=-90, const double& maxLat=90, const double& minLon=-180, const double& maxLon=180);

	// Loads a single FS by looking up its Id
	void loadFSById(const QString& dbName, std::vector<DeniedRegionClass *>& deniedRegionList, std::vector<AntennaClass *>& antennaList, std::vector<UlsRecord>& target, const int& fsid);
	UlsRecord getFSById(const QString& dbName, std::vector<DeniedRegionClass *>& deniedRegionList, std::vector<AntennaClass *>& antennaList, const int& fsid)
	{
		// list of size 1
		auto list = std::vector<UlsRecord>();
		loadFSById(dbName, deniedRegionList, antennaList, list, fsid);
		if (list.size() != 1)
			throw std::runtime_error("FS not found");
		return list.at(0);
	};

	void fillTarget(SqlScopedConnection<SqlExceptionDb>& db, std::vector<DeniedRegionClass *>& deniedRegionList, std::vector<AntennaClass *>& antennaList, std::vector<UlsRecord>& target, QSqlQuery& ulsQueryRes);

	AntennaClass *createAntennaPattern(SqlScopedConnection<SqlExceptionDb>& db, int rxAntennaIdxDB, std::vector<double> antennaAOBList, std::string antennaName);

	QStringList columns;
	QStringList prColumns;
	QStringList rasColumns;
	QStringList antnameColumns;
	QStringList antaobColumns;
	QStringList antgainColumns;

	int fsidIdx;
	int regionIdx;
	int callsignIdx;
	int pathNumberIdx;
	int radio_serviceIdx;
	int nameIdx;
	int rx_callsignIdx;
	int rx_antenna_numIdx;
	int freq_assigned_start_mhzIdx;
	int freq_assigned_end_mhzIdx;
	int tx_lat_degIdx;
	int tx_long_degIdx;
	int tx_ground_elev_mIdx;
	int tx_polarizationIdx;
	int tx_gainIdx;
	int tx_eirpIdx;
	int tx_height_to_center_raat_mIdx;
	int tx_architecture_mIdx;
	int azimuth_angle_to_tx_mIdx;
	int elevation_angle_to_tx_mIdx;
	int rx_lat_degIdx;
	int rx_long_degIdx;
	int rx_ground_elev_mIdx;
	int rx_height_to_center_raat_mIdx;
	int rx_line_loss_mIdx;
	int rx_gainIdx;
	int rx_antennaDiameterIdx;
	int rx_near_field_ant_diameterIdx;
	int rx_near_field_dist_limitIdx;
	int rx_near_field_ant_efficiencyIdx;
	int rx_antennaCategoryIdx;
	int statusIdx;
	int mobileIdx;
	int rx_ant_modelNameIdx;
	int rx_ant_model_idxIdx;
	int p_rp_numIdx;

	int rx_diversity_height_to_center_raat_mIdx;
	int rx_diversity_gainIdx;
	int rx_diversity_antennaDiameterIdx;

	int prSeqIdx;
	int prTypeIdx;
	int pr_lat_degIdx;
	int pr_lon_degIdx;
	int pr_height_to_center_raat_tx_mIdx;
	int pr_height_to_center_raat_rx_mIdx;

	int prTxGainIdx;
	int prTxDiameterIdx;
	int prRxGainIdx;
	int prRxDiameterIdx;
	int prAntCategoryIdx;
	int prAntModelNameIdx;
	int pr_ant_model_idxIdx;
	int prReflectorHeightIdx;
	int prReflectorWidthIdx;

    int antname_ant_idxIdx;
    int antname_ant_nameIdx;

    int antaob_aob_idxIdx;
    int antaob_aob_degIdx;

    int antgain_idIdx;
    int antgain_gainIdx;

    int ras_rasidIdx;
    int ras_regionIdx;
    int ras_nameIdx;
    int ras_locationIdx;
    int ras_startFreqMHzIdx;
    int ras_stopFreqMHzIdx;
    int ras_exclusionZoneIdx;
    int ras_rect1lat1Idx;
    int ras_rect1lat2Idx;
    int ras_rect1lon1Idx;
    int ras_rect1lon2Idx;
    int ras_rect2lat1Idx;
    int ras_rect2lat2Idx;
    int ras_rect2lon1Idx;
    int ras_rect2lon2Idx;
    int ras_radiusKmIdx;
    int ras_centerLatIdx;
    int ras_centerLonIdx;
    int ras_heightAGLIdx;
};

#endif /* AFCENGINE_ULS_DATABASE_H */
