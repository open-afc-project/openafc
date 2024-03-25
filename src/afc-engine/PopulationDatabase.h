// PopulationDatabse.h: header file for population database reading on program startup
// author: Sam Smucny

#ifndef AFCENGINE_POP_DATABASE_H_
#define AFCENGINE_POP_DATABASE_H_

#include <vector>
#include <QString>

struct PopulationRecord {
		double latitude;
		double longitude;
		double density;
};

class PopulationDatabase
{
	public:
		// Loads population database from file path dbName into the target vector. Uses
		// bounds to restrict the sections loaded
		static void loadPopulationData(const QString &dbName,
					       std::vector<PopulationRecord> &target,
					       const double &minLat = -90,
					       const double &maxLat = 90,
					       const double &minLon = -180,
					       const double &maxLon = 180);
};

#endif /* AFCENGINE_POP_DATABASE_H_ */
