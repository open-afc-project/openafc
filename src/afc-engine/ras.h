/******************************************************************************************/
/**** FILE : ras.h                                                                     ****/
/******************************************************************************************/

#ifndef RAS_H
#define RAS_H

#include "Vector3.h"
#include "cconst.h"
#include "pop_grid.h"

class GdalDataDir;
class WorldData;
class AfcManager;
class AntennaClass;

template<class T> class ListClass;

/******************************************************************************************/
/**** CLASS: RASClass                                                                  ****/
/******************************************************************************************/
class RASClass
{
	public:
		RASClass(int idVal);
		virtual ~RASClass();

		/**************************************************************************************/
		/**** RASExclusionZoneType                                                     ****/
		/**************************************************************************************/
		enum RASExclusionZoneTypeEnum {
			nullRASExclusionZoneType,
			rectRASExclusionZoneType,
			rect2RASExclusionZoneType,
			circleRASExclusionZoneType,
			horizonDistRASExclusionZoneType
		};
		/**************************************************************************************/

		virtual RASExclusionZoneTypeEnum type() const = 0;

		virtual bool intersect(double longitude, double latitude, double maxDist, double txHeightAGL) const = 0;

		int getID() const {
			return id;
		}

		void setStartFreq(double startFreqVal) { startFreq = startFreqVal; return; }
		void setStopFreq (double  stopFreqVal) { stopFreq  =  stopFreqVal; return; }
		void setHeightAGL(double heightAGLVal) { heightAGL = heightAGLVal; return; }

		double getStartFreq() const { return startFreq; }
		double getStopFreq()  const { return  stopFreq; }
		double getHeightAGL() const { return heightAGL; }

	protected:
		int id;

		double startFreq;
		double stopFreq;

		double heightAGL;
};
/******************************************************************************************/

/******************************************************************************************/
/**** CLASS: RectRASClass                                                              ****/
/******************************************************************************************/
class RectRASClass : public RASClass
{
	public:
		RectRASClass(int idVal);
		~RectRASClass();

		RASExclusionZoneTypeEnum type() const {
			if (rectList.size() == 1) {
				return rectRASExclusionZoneType;
			} else if (rectList.size() == 2) {
				return rect2RASExclusionZoneType;
			} else {
				return nullRASExclusionZoneType;
			}
		}

		bool intersect(double longitude, double latitude, double maxDist, double txHeightAGL) const;

		int getNumRect() const { return rectList.size(); }
		std::tuple<double, double, double, double> getRect(int rectIdx) const { return rectList[rectIdx]; }

		void addRect(double lon1, double lon2, double lat1, double lat2);

	private:
		std::vector<std::tuple<double, double, double, double> > rectList;
};
/******************************************************************************************/

/******************************************************************************************/
/**** CLASS: CircleRASClass                                                            ****/
/******************************************************************************************/
class CircleRASClass : public RASClass
{
	public:
		CircleRASClass(int idVal, bool horizonDistFlagVal);
		~CircleRASClass();

		RASExclusionZoneTypeEnum type() const {
			if (!horizonDistFlag) {
				return circleRASExclusionZoneType;
			} else {
				return horizonDistRASExclusionZoneType;
			}
		}

		bool intersect(double longitude, double latitude, double maxDist, double txHeightAGL) const;

		void setLongitudeCenter(double longitudeCenterVal) { longitudeCenter = longitudeCenterVal; return; }
		void setLatitudeCenter (double  latitudeCenterVal) {  latitudeCenter =  latitudeCenterVal; return; }
		void setRadius         (double          radiusVal) {          radius =          radiusVal; return; }

		double getLongitudeCenter() const { return longitudeCenter; }
		double getLatitudeCenter()  const { return  latitudeCenter; }
		bool   getHorizonDistFlag() const { return horizonDistFlag; }

		double computeRadius(double txHeightAGL) const;

	private:
		bool horizonDistFlag;
		double longitudeCenter;
		double latitudeCenter;
		double radius;
};
/******************************************************************************************/

#endif
