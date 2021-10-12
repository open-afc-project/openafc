/******************************************************************************************/
/**** FILE: polygon.h                                                                  ****/
/******************************************************************************************/

#ifndef POLYGON_H
#define POLYGON_H

#include <tuple>
#include <vector>

/******************************************************************************************/
/**** CLASS: PolygonClass                                                              ****/
/******************************************************************************************/
class PolygonClass
{
public:
    PolygonClass();
    PolygonClass(std::vector<std::tuple<int, int>> *ii_list);
    PolygonClass(std::string kmlFilename, double resolution);
    ~PolygonClass();
    bool in_bdy_area(const int a, const int b, bool *edge = (bool *) NULL);
    double comp_bdy_area();
    void comp_bdy_min_max(int &minx, int &maxx, int &miny, int &maxy);
    void remove_duplicate_points(int segment_idx);
    void translate(int x, int y);
    void reverse();
    PolygonClass *duplicate();
    std::tuple<double, double> closestPoint(std::tuple<int, int> point);

    static double comp_bdy_area(const int n, const int *x, const int *y);
    static double comp_bdy_area(std::vector<std::tuple<int, int>> *ii_list);
    static int in_bdy_area(const int a, const int b, const int n, const int *x, const int *y, int *edge = (int *) NULL);

    std::string name;
    int num_segment;
    int *num_bdy_pt;
    int **bdy_pt_x, **bdy_pt_y;

private:
    void comp_bdy_min_max(int &minx, int &maxx, int &miny, int &maxy, const int segment_idx);
};
/******************************************************************************************/

#endif
