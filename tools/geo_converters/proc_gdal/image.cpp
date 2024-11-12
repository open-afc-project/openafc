/******************************************************************************************/
/**** FILE : image.cpp                                                                 ****/
/******************************************************************************************/

#include <iostream>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <tuple>
#include "polygon.h"
#include "global_defines.h"
#include "image.h"

/******************************************************************************************/
/**** CONSTRUCTOR: ImageClass::ImageClass()                                            ****/
/******************************************************************************************/
ImageClass::ImageClass(int lonN0Val, int lonN1Val, int latN0Val, int latN1Val, int samplesPerDegVal) :
    lonN0(lonN0Val), lonN1(lonN1Val), latN0(latN0Val), latN1(latN1Val), samplesPerDeg(samplesPerDegVal)
{
    numLon = lonN1 - lonN0 + 1;
    numLat = latN1 - latN0 + 1;

    /**************************************************************************************/
    /**** Allocate and initialize image array                                          ****/
    /**************************************************************************************/
    scan = (int **) malloc(numLon*sizeof(int *));
    for(int lonIdx=0; lonIdx<numLon; ++lonIdx) {
        scan[lonIdx] = (int *) malloc(numLat*sizeof(int));
        for(int latIdx=0; latIdx<numLat; ++latIdx) {
            scan[lonIdx][latIdx] = 0;
        }
    }
    /**************************************************************************************/
}
/******************************************************************************************/

/******************************************************************************************/
/**** DESTRUCTOR: ImageClass::~ImageClass()                                            ****/
/******************************************************************************************/
ImageClass::~ImageClass()
{
    for(int lonIdx=0; lonIdx<numLon; ++lonIdx) {
        free(scan[lonIdx]);
    }
    free(scan);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ImageClass::processSegment()                                           ****/
/******************************************************************************************/
void ImageClass::processSegment(double lon0, double lat0, int nx0, int ny0,
                                double lon1, double lat1, int nx1, int ny1)
{
    scan[nx0][ny0] = 1;

    int sx = (nx1 > nx0 ? 1 : nx1 < nx0 ? -1 : 0);
    int sy = (ny1 > ny0 ? 1 : ny1 < ny0 ? -1 : 0);

    if ((sx == 0) && (sy == 0)) {
        return;
    }

    double deltax = lon1 - lon0;
    double deltay = lat1 - lat0;
    double xval = lon0;
    double yval = lat0;
    int nx = nx0;
    int ny = ny0;

    while((nx != nx1) || (ny != ny1)) {
        double epsx = (sx ? ((lonN0 + nx + (sx+1)/2) - xval*samplesPerDeg)/(deltax*samplesPerDeg) : 1.0);
        double epsy = (sy ? ((latN0 + ny + (sy+1)/2) - yval*samplesPerDeg)/(deltay*samplesPerDeg) : 1.0);
        if (epsx < epsy) {
            xval += epsx*deltax;
            yval += epsx*deltay;
            nx+=sx;
        } else if (epsy < epsx) {
            xval += epsy*deltax;
            yval += epsy*deltay;
            ny+=sy;
        } else {
            xval += epsx*deltax;
            yval += epsx*deltay;
            scan[nx+sx][ny] = 1;
            scan[nx][ny+sy] = 1;
            nx+=sx;
            ny+=sy;
        }
        scan[nx][ny] = 1;
    }

    return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ImageClass::fill()                                                     ****/
/******************************************************************************************/
void ImageClass::fill()
{
    std::vector<std::tuple<int,int>> worklist;
    std::vector<std::tuple<int,int>> grouplist;

    /**************************************************************************************/
    /**** Create worklist as list of points around edge that have 0                    ****/
    /**************************************************************************************/
    for(int lonIdx=0; lonIdx<numLon; ++lonIdx) {
        if (scan[lonIdx][0] == 0) {
            worklist.push_back(std::make_tuple(lonIdx, 0));
        }
        if (scan[lonIdx][numLat-1] == 0) {
            worklist.push_back(std::make_tuple(lonIdx, numLat-1));
        }
    }
    for(int latIdx=1; latIdx<numLat-1; ++latIdx) {
        if (scan[0][latIdx] == 0) {
            worklist.push_back(std::make_tuple(0, latIdx));
        }
        if (scan[numLon-1][latIdx] == 0) {
            worklist.push_back(std::make_tuple(numLon-1, latIdx));
        }
    }
    /**************************************************************************************/

    int lonIdx, latIdx;
    while(worklist.size()) {
        std::tie(lonIdx, latIdx) = worklist.back();
        worklist.pop_back();
        if (scan[lonIdx][latIdx] == 0) {
            grouplist.push_back(std::make_tuple(lonIdx, latIdx));
            scan[lonIdx][latIdx] = 2;

            int checkIdx = 0;
            while(checkIdx <= grouplist.size()-1) {
                int ix, iy;
                std::tie(lonIdx, latIdx) = grouplist[checkIdx];
                for(int rotIdx=0; rotIdx<4; ++rotIdx) {
                    if (rotIdx == 0) {
                        ix = 1;
                        iy = 0;
                    } else {
                        int tmp = ix;
                        ix = -iy;
                        iy = tmp;
                    }
                    if (    (lonIdx+ix >= 0) && (lonIdx+ix < numLon)
                         && (latIdx+iy >= 0) && (latIdx+iy < numLat) ) {
                        if (scan[lonIdx+ix][latIdx+iy] == 0) {
                            scan[lonIdx+ix][latIdx+iy] = 2;
                            grouplist.push_back(std::make_tuple(lonIdx+ix, latIdx+iy));
                        }
                    }
                }
                checkIdx++;
            }
            grouplist.clear();
        }
    }
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ImageClass::changeVal()                                                ****/
/******************************************************************************************/
int ImageClass::changeVal(int origVal, int newVal)
{
    int numChange = 0;

    /**************************************************************************************/
    /**** Create worklist as list of points around edge that have 0                    ****/
    /**************************************************************************************/
    for(int lonIdx=0; lonIdx<numLon; ++lonIdx) {
        for(int latIdx=1; latIdx<numLat-1; ++latIdx) {
            if (scan[lonIdx][latIdx] == origVal) {
                scan[lonIdx][latIdx] = newVal;
                numChange++;
            }
        }
    }
    /**************************************************************************************/

    return(numChange);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ImageClass::expand()                                                   ****/
/******************************************************************************************/
int ImageClass::expand(int val, int count)
{
    int lonIdx, latIdx, ix, iy;
    int numChange = 0;
    std::vector<std::tuple<int,int>> pointlist;
    std::vector<std::tuple<int,int>> changelist;

    std::cout << "EXPANDING IMAGE: count = " << count << std::endl;

    int markVal = -1;
    /**************************************************************************************/
    for(lonIdx=0; lonIdx<numLon; ++lonIdx) {
        for(latIdx=1; latIdx<numLat-1; ++latIdx) {
            if (scan[lonIdx][latIdx] == val) {
                for(int rotIdx=0; rotIdx<4; ++rotIdx) {
                    if (rotIdx == 0) {
                        ix = 1;
                        iy = 0;
                    } else {
                        int tmp = ix;
                        ix = -iy;
                        iy = tmp;
                    }
                    if (    (lonIdx+ix >= 0) && (lonIdx+ix < numLon)
                         && (latIdx+iy >= 0) && (latIdx+iy < numLat) ) {
                        if ((scan[lonIdx+ix][latIdx+iy] != val) && (scan[lonIdx+ix][latIdx+iy] != markVal)) {
                            pointlist.push_back(std::make_tuple(lonIdx+ix, latIdx+iy));
                            scan[lonIdx+ix][latIdx+iy] = markVal;
                        }
                    }
                }
            }
        }
    }

    for(int i=0; i<count; ++i) {
        for(int ptIdx=0; ptIdx<pointlist.size(); ++ptIdx) {
            std::tie(lonIdx, latIdx) = pointlist[ptIdx];
            scan[lonIdx][latIdx] = val;
            numChange++;
            if (i < count-1) {
                for(int rotIdx=0; rotIdx<4; ++rotIdx) {
                    if (rotIdx == 0) {
                        ix = 1;
                        iy = 0;
                    } else {
                        int tmp = ix;
                        ix = -iy;
                        iy = tmp;
                    }
                    if (    (lonIdx+ix >= 0) && (lonIdx+ix < numLon)
                         && (latIdx+iy >= 0) && (latIdx+iy < numLat) ) {
                        if ((scan[lonIdx+ix][latIdx+iy] != val) && (scan[lonIdx+ix][latIdx+iy] != markVal)) {
                            changelist.push_back(std::make_tuple(lonIdx+ix, latIdx+iy));
                            scan[lonIdx+ix][latIdx+iy] = markVal;
                        }
                    }
                }
            }
        }
        pointlist.swap(changelist);
        changelist.clear();
    }
    /**************************************************************************************/

    std::cout << "DONE EXPANDING IMAGE: numChange = " << numChange << std::endl;

    return(numChange);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ImageClass::createPolygonList()                                        ****/
/******************************************************************************************/
std::vector<PolygonClass *> ImageClass::createPolygonList()
{
    int polyVal = 2;
    std::vector<PolygonClass *> polygonList;

    for(int lonIdx=0; lonIdx<numLon; ++lonIdx) {
        for(int latIdx=1; latIdx<numLat-1; ++latIdx) {
            if (scan[lonIdx][latIdx] == 1) {
                PolygonClass *polygon = createPolygon(lonIdx, latIdx, polyVal);
                polygonList.push_back(polygon);
                polyVal++;
            }
        }
    }

    return(polygonList);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ImageClass::createPolygon()                                            ****/
/******************************************************************************************/
PolygonClass * ImageClass::createPolygon(int lonIdx, int latIdx, int polyVal)
{
    std::vector<int> rotx{ 1,  0, -1,  0};
    std::vector<int> roty{ 0,  1,  0, -1};
    std::vector<int> offx{ 1,  1,  0,  0};
    std::vector<int> offy{ 0,  1,  1,  0};
    std::vector<std::tuple<int, int>> ii_list;

    if (scan[lonIdx][latIdx] != 1) {
        CORE_DUMP;
    }

    if (lonIdx > 0) {
        if (scan[lonIdx-1][latIdx] != 0) {
            CORE_DUMP;
        }
    }

    if (latIdx > 0) {
        if (scan[lonIdx][latIdx-1] != 0) {
            CORE_DUMP;
        }
    }

    int px_0 = lonIdx;
    int py_0 = latIdx;
    ii_list.push_back(std::make_tuple(lonN0 + px_0, latN0 + py_0));

    int px = px_0;
    int py = py_0;
    int rot = 3;

    bool doneFlag = false;
    while(!doneFlag) {
        int ix0 = rotx[rot];
        int iy0 = roty[rot];
        int ox = offx[rot];
        int oy = offy[rot];
        int ix1 = rotx[(rot+3)%4];
        int iy1 = roty[(rot+3)%4];
        int v0 = scan[px+ix0-ox][py+iy0-oy];
        int v1 = scan[px+ix0+ix1-ox][py+iy0+iy1-oy];
        if (v0 == 0) {
            rot = (rot + 1) % 4;
        } else if (v1 == 1) {
            rot = (rot + 3) % 4;
        } else {
            CORE_DUMP;
        }

        ix0 = rotx[rot];
        iy0 = roty[rot];
        ox = offx[rot];
        oy = offy[rot];
        ix1 = rotx[(rot+3)%4];
        iy1 = roty[(rot+3)%4];
        bool cont = true;
        while(cont) {
            v0 = scan[px+ix0-ox][py+iy0-oy];
            v1 = scan[px+ix0+ix1-ox][py+iy0+iy1-oy];
            if ((v0 == 1) && (v1 == 0)) {
                px += ix0;
                py += iy0;
            } else {
                cont = false;
            }
        };

        if ((px != px_0) || (py != py_0)) {
            ii_list.push_back(std::make_tuple(lonN0 + px, latN0 + py));
        } else {
            doneFlag = true;
        }
    };

    std::vector<std::tuple<int,int>> grouplist;

    grouplist.push_back(std::make_tuple(lonIdx, latIdx));
    scan[lonIdx][latIdx] = polyVal;

    int checkIdx = 0;
    while(checkIdx <= grouplist.size()-1) {
        int ix, iy;
        std::tie(lonIdx, latIdx) = grouplist[checkIdx];
        for(int rotIdx=0; rotIdx<4; ++rotIdx) {
            ix = rotx[rotIdx];
            iy = roty[rotIdx];
            if (    (lonIdx+ix >= 0) && (lonIdx+ix < numLon)
                 && (latIdx+iy >= 0) && (latIdx+iy < numLat) ) {
                if (scan[lonIdx+ix][latIdx+iy] == 1) {
                    scan[lonIdx+ix][latIdx+iy] = polyVal;
                    grouplist.push_back(std::make_tuple(lonIdx+ix, latIdx+iy));
                }
            }
        }
        checkIdx++;
    }
    grouplist.clear();
    
    PolygonClass *polygon = new PolygonClass(&ii_list);

    return(polygon);
}
/******************************************************************************************/

