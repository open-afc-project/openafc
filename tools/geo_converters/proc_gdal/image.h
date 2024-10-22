/******************************************************************************************/
/**** FILE : image.h                                                                   ****/
/******************************************************************************************/

#ifndef IMAGE_H
#define IMAGE_H

class PolygonClass;

/******************************************************************************************/
/**** CLASS: ImageClass                                                                 ****/
/******************************************************************************************/
class ImageClass
{
public:
    ImageClass(int lonN0Val, int lonN1Val, int latN0Val, int latN1Val, int samplesPerDegVal);
    ~ImageClass();

    void processSegment(double lon0, double lat0, int nx0, int ny0,
                         double lon1, double lat1, int nx1, int ny1);

    void fill();

    int getVal(int lonIdx, int latIdx) { return(scan[lonIdx][latIdx]); }
    int changeVal(int origVal, int newVal);
    int expand(int val, int count);
    std::vector<PolygonClass *> createPolygonList();

private:
    PolygonClass *createPolygon(int lonIdx, int latIdx, int polyVal);

    /**************************************************************************************/
    /**** Data                                                                         ****/
    /**************************************************************************************/
    int lonN0, lonN1, latN0, latN1;
    int numLon, numLat;
    int samplesPerDeg;

    int **scan;
    /**************************************************************************************/

};
/******************************************************************************************/

#endif
