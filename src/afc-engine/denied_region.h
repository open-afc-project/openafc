/******************************************************************************************/
/**** FILE : denied_region.h                                                           ****/
/******************************************************************************************/

#ifndef DENIED_REGION_H
#define DENIED_REGION_H

#include "Vector3.h"
#include "cconst.h"
#include "pop_grid.h"

class GdalDataDir;
class WorldData;
class AfcManager;
class AntennaClass;

template<class T>
class ListClass;

/******************************************************************************************/
/**** CLASS: DeniedRegionClass                                                         ****/
/******************************************************************************************/
class DeniedRegionClass
{
    public:
        DeniedRegionClass(int idVal);
        virtual ~DeniedRegionClass();

        /**************************************************************************************/
        /**** Geometry ****/
        /**************************************************************************************/
        enum GeometryEnum { nullGeometry, rectGeometry, rect2Geometry, circleGeometry, horizonDistGeometry };
        /**************************************************************************************/

        /**************************************************************************************/
        /**** Type ****/
        /**************************************************************************************/
        enum TypeEnum { nullType, RASType, userSpecifiedType };
        /**************************************************************************************/

        virtual GeometryEnum getGeometry() const = 0;

        virtual bool intersect(double longitude, double latitude, double maxDist, double txHeightAGL) const = 0;

        int getID() const
        {
            return id;
        }

        TypeEnum getType() const
        {
            return (type);
        }
        void setType(TypeEnum typeVal)
        {
            type = typeVal;
        }

        void setStartFreq(double startFreqVal)
        {
            startFreq = startFreqVal;
            return;
        }
        void setStopFreq(double stopFreqVal)
        {
            stopFreq = stopFreqVal;
            return;
        }
        void setHeightAGL(double heightAGLVal)
        {
            heightAGL = heightAGLVal;
            return;
        }

        double getStartFreq() const
        {
            return startFreq;
        }
        double getStopFreq() const
        {
            return stopFreq;
        }
        double getHeightAGL() const
        {
            return heightAGL;
        }

    protected:
        int id;
        TypeEnum type;

        double startFreq;
        double stopFreq;

        double heightAGL;
};
/******************************************************************************************/

/******************************************************************************************/
/**** CLASS: RectDeniedRegionClass                                                     ****/
/******************************************************************************************/
class RectDeniedRegionClass : public DeniedRegionClass
{
    public:
        RectDeniedRegionClass(int idVal);
        ~RectDeniedRegionClass();

        GeometryEnum getGeometry() const
        {
            if (rectList.size() == 1) {
                return rectGeometry;
            } else if (rectList.size() == 2) {
                return rect2Geometry;
            } else {
                return nullGeometry;
            }
        }

        bool intersect(double longitude, double latitude, double maxDist, double txHeightAGL) const;

        int getNumRect() const
        {
            return rectList.size();
        }
        std::tuple<double, double, double, double> getRect(int rectIdx) const
        {
            return rectList[rectIdx];
        }

        void addRect(double lon1, double lon2, double lat1, double lat2);

    private:
        std::vector<std::tuple<double, double, double, double>> rectList;
};
/******************************************************************************************/

/******************************************************************************************/
/**** CLASS: CircleDeniedRegionClass                                                   ****/
/******************************************************************************************/
class CircleDeniedRegionClass : public DeniedRegionClass
{
    public:
        CircleDeniedRegionClass(int idVal, bool horizonDistFlagVal);
        ~CircleDeniedRegionClass();

        GeometryEnum getGeometry() const
        {
            if (!horizonDistFlag) {
                return circleGeometry;
            } else {
                return horizonDistGeometry;
            }
        }

        bool intersect(double longitude, double latitude, double maxDist, double txHeightAGL) const;

        void setLongitudeCenter(double longitudeCenterVal)
        {
            longitudeCenter = longitudeCenterVal;
            return;
        }
        void setLatitudeCenter(double latitudeCenterVal)
        {
            latitudeCenter = latitudeCenterVal;
            return;
        }
        void setRadius(double radiusVal)
        {
            radius = radiusVal;
            return;
        }

        double getLongitudeCenter() const
        {
            return longitudeCenter;
        }
        double getLatitudeCenter() const
        {
            return latitudeCenter;
        }
        bool getHorizonDistFlag() const
        {
            return horizonDistFlag;
        }

        double computeRadius(double txHeightAGL) const;

    private:
        bool horizonDistFlag;
        double longitudeCenter;
        double latitudeCenter;
        double radius;
};
/******************************************************************************************/

#endif
