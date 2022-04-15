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
#include <rkfsql/SqlScopedConnection.h>
#include <rkfsql/SqlExceptionDb.h>

#include "cconst.h"

const int maxNumPR = 3;

struct UlsRecord
{
	int fsid;

	std::string callsign;
	std::string radioService;
	std::string entityName;
	std::string rxCallsign;
	int rxAntennaNumber;
	double startFreq, stopFreq;
	std::string emissionsDesignator;
	double txLatitudeDeg, txLongitudeDeg;
	double txGroundElevation;
	std::string txPolarization;
	double txGain;
	double txEIRP;
	double txHeightAboveTerrain;
	double rxLatitudeDeg, rxLongitudeDeg;
	double rxGroundElevation;
	double rxHeightAboveTerrain;
	double rxGain;
	std::string status;
	bool mobile;
	std::string rxAntennaModel;
	int numPR;
	std::vector<double> prLatitudeDeg;
	std::vector<double> prLongitudeDeg;
	std::vector<double> prHeightAboveTerrain;
};

class UlsDatabase
{
	public:
		UlsDatabase();
		~UlsDatabase();

		// Loads all FS within lat/lon bounds
		void loadUlsData(const QString& dbName, std::vector<UlsRecord>& target,
				const double& minLat=-90, const double& maxLat=90, const double& minLon=-180, const double& maxLon=180);

		// Loads a single FS by looking up its Id
		void loadFSById(const QString& dbName, std::vector<UlsRecord>& target, const int& fsid);
		UlsRecord getFSById(const QString& dbName, const int& fsid)
		{
			// list of size 1
			auto list = std::vector<UlsRecord>();
			loadFSById(dbName, list, fsid);
			if (list.size() != 1)
				throw std::runtime_error("FS not found");
			return list.at(0);
		};

		void fillTarget(SqlScopedConnection<SqlExceptionDb>& db, std::vector<UlsRecord>& target, QSqlQuery& ulsQueryRes);

		QStringList columns;
		std::vector<int *> fieldIdxList;

		QStringList prColumns;
		std::vector<int *> prFieldIdxList;

		int fsidIdx;
		int callsignIdx;
		int radio_serviceIdx;
		int nameIdx;
		int rx_callsignIdx;
		int rx_antenna_numIdx;
		int freq_assigned_start_mhzIdx;
		int freq_assigned_end_mhzIdx;
		int emissions_desIdx;
		int tx_lat_degIdx;
		int tx_long_degIdx;
		int tx_ground_elev_mIdx;
		int tx_polarizationIdx;
		int tx_gainIdx;
		int tx_eirpIdx;
		int tx_height_to_center_raat_mIdx;
		int rx_lat_degIdx;
		int rx_long_degIdx;
		int rx_ground_elev_mIdx;
		int rx_height_to_center_raat_mIdx;
		int rx_gainIdx;
		int statusIdx;
		int mobileIdx;
		int rx_ant_modelIdx;
		int p_rp_numIdx;

		int prSeqIdx;
		int pr_lat_degIdx;
		int pr_lon_degIdx;
		int pr_height_to_center_raat_mIdx;
};

#endif /* AFCENGINE_ULS_DATABASE_H */
