/******************************************************************************************/
/**** PROGRAM: polygon.cpp                                                             ****/
/******************************************************************************************/
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <fstream>

#include "global_defines.h"
#include "global_fn.h"
#include "polygon.h"

/******************************************************************************************/
/**** FUNCTION: PolygonClass::PolygonClass                                             ****/
/******************************************************************************************/
PolygonClass::PolygonClass()
{
	num_segment = 0;
	num_bdy_pt  = (int  *) NULL;
	bdy_pt_x    = (int **) NULL;
	bdy_pt_y    = (int **) NULL;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PolygonClass::PolygonClass                                             ****/
/******************************************************************************************/
PolygonClass::PolygonClass(std::vector<std::tuple<int, int>> *ii_list)
{
	int i;

	num_segment = 1;
	num_bdy_pt  = IVECTOR(num_segment);
	num_bdy_pt[0] = ii_list->size();

	bdy_pt_x    = (int **) malloc(num_segment*sizeof(int *));
	bdy_pt_y    = (int **) malloc(num_segment*sizeof(int *));

	bdy_pt_x[0] = IVECTOR(num_bdy_pt[0]);
	bdy_pt_y[0] = IVECTOR(num_bdy_pt[0]);

	for (i=0; i<=num_bdy_pt[0]-1; i++) {
		std::tie (bdy_pt_x[0][i], bdy_pt_y[0][i]) = (*ii_list)[i];
	}
}
/******************************************************************************************/
/**** FUNCTION: PolygonClass::PolygonClass                                             ****/
/******************************************************************************************/
PolygonClass::PolygonClass(std::string kmlFilename, double resolution)
{

	std::ostringstream errStr;

	/**************************************************************************************/
	/* Read entire contents of kml file into sval                                         */
	/**************************************************************************************/
	std::ifstream fstr(kmlFilename);
	std::stringstream istream;
	istream << fstr.rdbuf();
	std::string sval = istream.str();
	/**************************************************************************************/

	std::size_t found;

	/**************************************************************************************/
	/* Grab contents of <Placemark> ... </Placemark>                                      */
	/**************************************************************************************/
	found = sval.find("<Placemark>");
	if (found==std::string::npos) {
		std::cout << "SVAL: " << sval << std::endl;
		errStr << "ERROR: unable to find <Placemark>";
		throw std::runtime_error(errStr.str());
	}
	sval.replace(0, found+11, "");

	found = sval.find("<Placemark>");
	if (found!=std::string::npos) {
		std::cout << "SVAL: " << sval << std::endl;
		errStr << "ERROR: multiple <Placemark>'s found";
		throw std::runtime_error(errStr.str());
	}

	found = sval.find("</Placemark>");
	if (found==std::string::npos) {
		std::cout << "SVAL: " << sval << std::endl;
		errStr << "ERROR: unable to find </Placemark>";
		throw std::runtime_error(errStr.str());
	}
	sval.replace(found, sval.size()-found, "");
	/**************************************************************************************/

	/**************************************************************************************/
	/* Extract name                                                                       */
	/**************************************************************************************/
	found = sval.find("<name>");
	if (found==std::string::npos) {
		std::cout << "SVAL: " << sval << std::endl;
		errStr << "ERROR: unable to find <name>";
		throw std::runtime_error(errStr.str());
	}
	std::size_t nameStart = found + 6;

	found = sval.find("</name>");
	if (found==std::string::npos) {
		std::cout << "SVAL: " << sval << std::endl;
		errStr << "ERROR: unable to find </name>";
		throw std::runtime_error(errStr.str());
	}
	std::size_t nameEnd = found;

	int nameLength = nameEnd - nameStart;

	if (nameLength) {
		name = sval.substr(nameStart, nameLength);
	} else {
		name.clear();
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Grab contents of <Polygon> ... </Polygon>                                          */
	/**************************************************************************************/
	found = sval.find("<Polygon>");
	if (found==std::string::npos) {
		std::cout << "SVAL: " << sval << std::endl;
		errStr << "ERROR: unable to find <Polygon>";
		throw std::runtime_error(errStr.str());
	}
	sval.replace(0, found+9, "");

	found = sval.find("<Polygon>");
	if (found!=std::string::npos) {
		std::cout << "SVAL: " << sval << std::endl;
		errStr << "ERROR: File " << kmlFilename << " multiple <Polygon>'s found";
		throw std::runtime_error(errStr.str());
	}

	found = sval.find("</Polygon>");
	if (found==std::string::npos) {
		std::cout << "SVAL: " << sval << std::endl;
		errStr << "ERROR: unable to find </Polygon>";
		throw std::runtime_error(errStr.str());
	}
	sval.replace(found, sval.size()-found, "");
	/**************************************************************************************/

	/**************************************************************************************/
	/* Grab contents of <outerBoundaryIs> ... </outerBoundaryIs>                          */
	/**************************************************************************************/
	found = sval.find("<outerBoundaryIs>");
	if (found==std::string::npos) {
		std::cout << "SVAL: " << sval << std::endl;
		errStr << "ERROR: unable to find <outerBoundaryIs>";
		throw std::runtime_error(errStr.str());
	}
	sval.replace(0, found+17, "");

	found = sval.find("</outerBoundaryIs>");
	if (found==std::string::npos) {
		std::cout << "SVAL: " << sval << std::endl;
		errStr << "ERROR: unable to find </outerBoundaryIs>";
		throw std::runtime_error(errStr.str());
	}
	sval.replace(found, sval.size()-found, "");
	/**************************************************************************************/

	/**************************************************************************************/
	/* Grab contents of <coordinates> ... </coordinates>                          */
	/**************************************************************************************/
	found = sval.find("<coordinates>");
	if (found==std::string::npos) {
		std::cout << "SVAL: " << sval << std::endl;
		errStr << "ERROR: unable to find <coordinates>";
		throw std::runtime_error(errStr.str());
	}
	sval.replace(0, found+13, "");

	found = sval.find("</coordinates>");
	if (found==std::string::npos) {
		std::cout << "SVAL: " << sval << std::endl;
		errStr << "ERROR: unable to find </coordinates>";
		throw std::runtime_error(errStr.str());
	}
	sval.replace(found, sval.size()-found, "");
	/**************************************************************************************/

	/**************************************************************************************/
	/* Remove whitespace from beginning and end of sval                                   */
	/**************************************************************************************/
	std::size_t start = sval.find_first_not_of(" \n\t");
	std::size_t end = sval.find_last_not_of(" \n\t");
	if (start == std::string::npos) {
		sval.clear();
	} else {
		sval = sval.substr(start, end-start+1);
	}
	/**************************************************************************************/

	/**************************************************************************************/
	/* Replace consecutive whitespace characters with a single space                      */
	/**************************************************************************************/
	std::size_t posn = 0;
	bool cont = true;
	do {
		std::size_t posnA = sval.find_first_of(" \n\t", posn);
		if (posnA == std::string::npos) {
			cont = false;
		} else {
			std::size_t posnB = sval.find_first_not_of(" \n\t", posnA);
			if (posnB - posnA == 1) {
				if (sval[posnA] != ' ') { sval[posnA] = ' '; }
			} else {
				sval.replace(posnA, posnB-posnA, " ");
			}
			posn = posnA + 1;
		}
	} while(cont);
	/**************************************************************************************/

	std::vector<std::string> clist = split(sval, ' ');

	/**************************************************************************************/
	/* Remove duplicate endpoint (in kml polygons are closed with last pt = first pt)     */
	/**************************************************************************************/
	if ((clist.size() > 1) && (clist[0] == clist[clist.size()-1])) {
		clist.pop_back();
	}
	/**************************************************************************************/

	num_segment = 1;
	num_bdy_pt  = IVECTOR(num_segment);
	num_bdy_pt[0] = clist.size();

	bdy_pt_x    = (int **) malloc(num_segment*sizeof(int *));
	bdy_pt_y    = (int **) malloc(num_segment*sizeof(int *));

	bdy_pt_x[0] = IVECTOR(num_bdy_pt[0]);
	bdy_pt_y[0] = IVECTOR(num_bdy_pt[0]);

	int ptIdx;
	for (ptIdx=0; ptIdx<=num_bdy_pt[0]-1; ptIdx++) {
		char *chptr;

		std::vector<std::string> lonlatStrList = split(clist[ptIdx], ',');
		double longitude = std::strtod(lonlatStrList[0].c_str(), &chptr);
		double latitude  = std::strtod(lonlatStrList[1].c_str(), &chptr);

		int xval = (int) floor(((longitude)/resolution) + 0.5);
		int yval = (int) floor(((latitude )/resolution) + 0.5);

		bdy_pt_x[0][ptIdx] = xval;
		bdy_pt_y[0][ptIdx] = yval;
	}
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PolygonClass::~PolygonClass                                            ****/
/******************************************************************************************/
PolygonClass::~PolygonClass()
{
	int segment_idx;

	for (segment_idx=0; segment_idx<=num_segment-1; segment_idx++) {
		free(bdy_pt_x[segment_idx]);
		free(bdy_pt_y[segment_idx]);
	}
	if (num_segment) {
		free(bdy_pt_x);
		free(bdy_pt_y);
		free(num_bdy_pt);
	}
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PolygonClass::readMultiGeometry()                                      ****/
/******************************************************************************************/
std::vector<PolygonClass *> PolygonClass::readMultiGeometry(std::string kmlFilename, double resolution)
{

    std::ostringstream errStr;
    std::string tag;
    std::string polystr;

    /**************************************************************************************/
    /* Read entire contents of kml file into sval                                         */
    /**************************************************************************************/
    std::ifstream fstr(kmlFilename);
    std::stringstream istream;
    istream << fstr.rdbuf();
    std::string sval = istream.str();
    /**************************************************************************************/

    std::size_t found;

    /**************************************************************************************/
    /* Grab contents of <Placemark> ... </Placemark>                                      */
    /**************************************************************************************/
    tag = "<Placemark>";
    found = sval.find(tag); 
    if (found==std::string::npos) {
        std::cout << "SVAL: " << sval << std::endl;
        errStr << "ERROR: unable to find " << tag << " while reading file " << kmlFilename;
        throw std::runtime_error(errStr.str());
    }
    sval.replace(0, found+tag.size(), "");

    found = sval.find(tag);
    if (found!=std::string::npos) {
        std::cout << "SVAL: " << sval << std::endl;
        errStr << "ERROR: multiple " << tag << "'s found while reading file " << kmlFilename;
        throw std::runtime_error(errStr.str());
    }

    tag = "</Placemark>";
    found = sval.find(tag);
    if (found==std::string::npos) {
        std::cout << "SVAL: " << sval << std::endl;
        errStr << "ERROR: unable to find " << tag << " while reading file " << kmlFilename;
        throw std::runtime_error(errStr.str());
    }
    sval.replace(found, sval.size()-found, "");
    /**************************************************************************************/

    /**************************************************************************************/
    /* Extract name                                                                       */
    /**************************************************************************************/
    tag = "<name>";
    found = sval.find(tag);
    if (found==std::string::npos) {
        std::cout << "SVAL: " << sval << std::endl;
        errStr << "ERROR: unable to find " << tag << " while reading file " << kmlFilename;
        throw std::runtime_error(errStr.str());
    }
    std::size_t nameStart = found + tag.size();

    tag = "</name>";
    found = sval.find(tag);
    if (found==std::string::npos) {
        std::cout << "SVAL: " << sval << std::endl;
        errStr << "ERROR: unable to find " << tag << " while reading file " << kmlFilename;
        throw std::runtime_error(errStr.str());
    }
    std::size_t nameEnd = found;

    int nameLength = nameEnd - nameStart;

    std::string namePfx;
    if (nameLength) {
        namePfx = sval.substr(nameStart, nameLength);
    } else {
        namePfx = "P";
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /* Grab contents of <MultiGeometry> ... </MultiGeometry>                                          */
    /**************************************************************************************/
    tag = "<MultiGeometry>";

    found = sval.find(tag);
    if (found==std::string::npos) {
        std::cout << "SVAL: " << sval << std::endl;
        errStr << "ERROR: unable to find " << tag << " while reading file " << kmlFilename;
        throw std::runtime_error(errStr.str());
    }
    sval.replace(0, found+tag.size(), "");

    found = sval.find(tag);
    if (found!=std::string::npos) {
        std::cout << "SVAL: " << sval << std::endl;
        errStr << "ERROR: multiple " << tag << "'s found";
        throw std::runtime_error(errStr.str());
    }

    tag = "</MultiGeometry>";
    found = sval.find(tag);
    if (found==std::string::npos) {
        std::cout << "SVAL: " << sval << std::endl;
        errStr << "ERROR: unable to find " << tag << " while reading file " << kmlFilename;
        throw std::runtime_error(errStr.str());
    }
    sval.replace(found, sval.size()-found, "");
    /**************************************************************************************/

    std::vector<PolygonClass *> polygonList;

    bool cont;

    do {
    /**************************************************************************************/
    /* Grab contents of <Polygon> ... </Polygon>                                          */
    /**************************************************************************************/
    tag = "<Polygon>";
    found = sval.find(tag);
    if (found==std::string::npos) {
        cont = false;
    } else {
        cont = true;
    }
    sval.replace(0, found+tag.size(), "");

    if (cont) {
        tag = "</Polygon>";
        found = sval.find(tag);
        if (found==std::string::npos) {
            std::cout << "SVAL: " << sval << std::endl;
            errStr << "ERROR: unable to find " << tag << " while reading file " << kmlFilename;
            throw std::runtime_error(errStr.str());
        }
        polystr = sval.substr(0, found);
        sval.replace(0, found+tag.size(), "");
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /* Grab contents of <outerBoundaryIs> ... </outerBoundaryIs>                          */
    /**************************************************************************************/
    if (cont) {
        tag = "<outerBoundaryIs>";
        found = polystr.find(tag);
        if (found==std::string::npos) {
            std::cout << "POLYSTR: " << polystr << std::endl;
            errStr << "ERROR: unable to find " << tag << " while reading file " << kmlFilename;
            throw std::runtime_error(errStr.str());
        }
        polystr.replace(0, found+tag.size(), "");

        tag = "</outerBoundaryIs>";
        found = polystr.find(tag);
        if (found==std::string::npos) {
            std::cout << "POLYSTR: " << polystr << std::endl;
            errStr << "ERROR: unable to find " << tag << " while reading file " << kmlFilename;
            throw std::runtime_error(errStr.str());
        }
        polystr.replace(found, sval.size()-found, "");
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /* Grab contents of <coordinates> ... </coordinates>                          */
    /**************************************************************************************/
    if (cont) {
        tag = "<coordinates>";
        found = polystr.find(tag);
        if (found==std::string::npos) {
            std::cout << "POLYSTR: " << polystr << std::endl;
            errStr << "ERROR: unable to find " << tag << " while reading file " << kmlFilename;
            throw std::runtime_error(errStr.str());
        }
        polystr.replace(0, found+tag.size(), "");

        tag = "</coordinates>";
        found = polystr.find(tag);
        if (found==std::string::npos) {
            std::cout << "POLYSTR: " << polystr << std::endl;
            errStr << "ERROR: unable to find " << tag << " while reading file " << kmlFilename;
            throw std::runtime_error(errStr.str());
        }
        polystr.replace(found, polystr.size()-found, "");
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /* Remove whitespace from beginning and end of polystr                                   */
    /**************************************************************************************/
    if (cont) {
        std::size_t start = polystr.find_first_not_of(" \n\t");
        std::size_t end = polystr.find_last_not_of(" \n\t");
        if (start == std::string::npos) {
            polystr.clear();
        } else {
            polystr = polystr.substr(start, end-start+1);
        }
    }
    /**************************************************************************************/

    /**************************************************************************************/
    /* Replace consecutive whitespace characters with a single space                      */
    /**************************************************************************************/
    if (cont) {
        std::size_t posn = 0;
        bool foundWhitespace = true;
        do {
            std::size_t posnA = polystr.find_first_of(" \n\t", posn);
            if (posnA == std::string::npos) {
                foundWhitespace = false;
            } else {
                std::size_t posnB = polystr.find_first_not_of(" \n\t", posnA);
                if (posnB - posnA == 1) {
                    if (polystr[posnA] != ' ') { polystr[posnA] = ' '; }
                } else {
                    polystr.replace(posnA, posnB-posnA, " ");
                }
                posn = posnA + 1;
            }
        } while(foundWhitespace);
    }
    /**************************************************************************************/

    if (cont) {
        std::vector<std::string> clist = split(polystr, ' ');

        /**********************************************************************************/
        /* Remove duplicate endpoint (in kml polygons are closed with last pt = first pt) */
        /**********************************************************************************/
        if ((clist.size() > 1) && (clist[0] == clist[clist.size()-1])) {
            clist.pop_back();
        }
        /**********************************************************************************/

        PolygonClass *poly = new PolygonClass();

        poly->name = namePfx + "_" + std::to_string(polygonList.size());

        poly->num_segment = 1;
        poly->num_bdy_pt  = IVECTOR(poly->num_segment);
        poly->num_bdy_pt[0] = clist.size();

        poly->bdy_pt_x    = (int **) malloc(poly->num_segment*sizeof(int *));
        poly->bdy_pt_y    = (int **) malloc(poly->num_segment*sizeof(int *));

        poly->bdy_pt_x[0] = IVECTOR(poly->num_bdy_pt[0]);
        poly->bdy_pt_y[0] = IVECTOR(poly->num_bdy_pt[0]);

        int ptIdx;
        for (ptIdx=0; ptIdx<=poly->num_bdy_pt[0]-1; ptIdx++) {
            char *chptr;

            std::vector<std::string> lonlatStrList = split(clist[ptIdx], ',');
            double longitude = std::strtod(lonlatStrList[0].c_str(), &chptr);
            double latitude  = std::strtod(lonlatStrList[1].c_str(), &chptr);

            int xval = (int) floor(((longitude)/resolution) + 0.5);
            int yval = (int) floor(((latitude )/resolution) + 0.5);

            poly->bdy_pt_x[0][ptIdx] = xval;
            poly->bdy_pt_y[0][ptIdx] = yval;
        }
        polygonList.push_back(poly);
    }
    } while (cont);

    return(polygonList);
}
/******************************************************************************************/


/******************************************************************************************/
/**** FUNCTION: PolygonClass::comp_bdy_min_max                                         ****/
/**** Find minx, maxx, miny, maxy for a list of bdy points.                            ****/
/******************************************************************************************/
void PolygonClass::comp_bdy_min_max(int &minx, int &maxx, int &miny, int &maxy, const int segment_idx)
{
	int i;
	int n = num_bdy_pt[segment_idx];
	int *x = bdy_pt_x[segment_idx];
	int *y = bdy_pt_y[segment_idx];

	minx = x[0];
	maxx = x[0];
	miny = y[0];
	maxy = y[0];
	for (i=1; i<=n-1; i++) {
		minx = (x[i] < minx ? x[i] : minx);
		maxx = (x[i] > maxx ? x[i] : maxx);
		miny = (y[i] < miny ? y[i] : miny);
		maxy = (y[i] > maxy ? y[i] : maxy);
	}

	return;
}
/******************************************************************************************/
/**** FUNCTION: PolygonClass::comp_bdy_min_max                                         ****/
/**** Find minx, maxx, miny, maxy for a list of bdy points.                            ****/
/******************************************************************************************/
void PolygonClass::comp_bdy_min_max(int &minx, int &maxx, int &miny, int &maxy)
{
	int segment_idx;
	int i_minx, i_maxx, i_miny, i_maxy;

	comp_bdy_min_max(minx, maxx, miny, maxy, 0);

	for (segment_idx=1; segment_idx<=num_segment-1; segment_idx++) {
		comp_bdy_min_max(i_minx, i_maxx, i_miny, i_maxy, segment_idx);
		minx = (i_minx < minx ? i_minx : minx);
		maxx = (i_maxx > maxx ? i_maxx : maxx);
		miny = (i_miny < miny ? i_miny : miny);
		maxy = (i_maxy > maxy ? i_maxy : maxy);
	}

	return;
}
/******************************************************************************************/
/**** FUNCTION: PolygonClass::translate                                                ****/
/******************************************************************************************/
void PolygonClass::translate(int x, int y)
{
	int i, segment_idx, n;

	for (segment_idx=0; segment_idx<=num_segment-1; segment_idx++) {
		n = num_bdy_pt[segment_idx];
		for (i=0; i<=n-1; i++) {
			bdy_pt_x[segment_idx][i] += x;
			bdy_pt_y[segment_idx][i] += y;
		}
	}

	return;
}
/******************************************************************************************/
/**** FUNCTION: PolygonClass::reverse                                                  ****/
/******************************************************************************************/
void PolygonClass::reverse()
{
	int i, segment_idx, n, tmp_x, tmp_y;

	for (segment_idx=0; segment_idx<=num_segment-1; segment_idx++) {
		n = num_bdy_pt[segment_idx];
		for (i=0; i<=n/2-1; i++) {
			tmp_x = bdy_pt_x[segment_idx][i];
			tmp_y = bdy_pt_y[segment_idx][i];
			bdy_pt_x[segment_idx][i] = bdy_pt_x[segment_idx][n-1-i];
			bdy_pt_y[segment_idx][i] = bdy_pt_y[segment_idx][n-1-i];
			bdy_pt_x[segment_idx][n-1-i] = tmp_x;
			bdy_pt_y[segment_idx][n-1-i] = tmp_y;
		}
	}

	return;
}
/******************************************************************************************/
/**** FUNCTION: PolygonClass::comp_bdy_area                                            ****/
/**** Compute area of a PolygonClass                                                   ****/
/******************************************************************************************/
double PolygonClass::comp_bdy_area()
{
	int segment_idx;
	double area;

	area = 0.0;

	for (segment_idx=0; segment_idx<=num_segment-1; segment_idx++) {
		area += comp_bdy_area(num_bdy_pt[segment_idx], bdy_pt_x[segment_idx], bdy_pt_y[segment_idx]);
	}

	return(area);
}
/******************************************************************************************/
/**** FUNCTION: in_bdy_area                                                            ****/
/**** Determine whether or not a given point lies within the bounded area              ****/
/******************************************************************************************/
bool PolygonClass::in_bdy_area(const int a, const int b, bool *edge)
{
	int segment_idx, n;
	int is_edge;

	if (edge) { *edge = false; }
	n = 0;
	for (segment_idx=0; segment_idx<=num_segment-1; segment_idx++) {
		n += in_bdy_area(a, b, num_bdy_pt[segment_idx], bdy_pt_x[segment_idx], bdy_pt_y[segment_idx], &is_edge);
		if (is_edge) {
			if (edge) { *edge = true; }
			return(0);
		}
	}

	return(n&1 ? true : false);
}
/******************************************************************************************/
/**** FUNCTION: PolygonClass::duplicate                                                ****/
/******************************************************************************************/
PolygonClass *PolygonClass::duplicate()
{
	PolygonClass *new_polygon = new PolygonClass();

	new_polygon->num_segment = num_segment;

	new_polygon->num_bdy_pt = IVECTOR(num_segment);
	new_polygon->bdy_pt_x   = (int **) malloc(num_segment*sizeof(int *));
	new_polygon->bdy_pt_y   = (int **) malloc(num_segment*sizeof(int *));

	for (int segment_idx=0; segment_idx<=num_segment-1; segment_idx++) {
		new_polygon->num_bdy_pt[segment_idx] = num_bdy_pt[segment_idx];
		new_polygon->bdy_pt_x[segment_idx]   = IVECTOR(num_bdy_pt[segment_idx]);
		new_polygon->bdy_pt_y[segment_idx]   = IVECTOR(num_bdy_pt[segment_idx]);
		for (int bdy_pt_idx=0; bdy_pt_idx<=num_bdy_pt[segment_idx]-1; bdy_pt_idx++) {
			new_polygon->bdy_pt_x[segment_idx][bdy_pt_idx]   = bdy_pt_x[segment_idx][bdy_pt_idx];
			new_polygon->bdy_pt_y[segment_idx][bdy_pt_idx]   = bdy_pt_y[segment_idx][bdy_pt_idx];
		}
	}

	return(new_polygon);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PolygonClass::comp_bdy_area                                            ****/
/**** Compute area from list of boundary points                                        ****/
/******************************************************************************************/
double PolygonClass::comp_bdy_area(const int n, const int *x, const int *y)
{
	int i, x1, y1, x2, y2;
	double area;

	area = 0.0;

	for (i=1; i<=n-2; i++) {
		x1 = x[i] - x[0];
		y1 = y[i] - y[0];
		x2 = x[i+1] - x[0];
		y2 = y[i+1] - y[0];

		area += ( (double) x1)*y2 - ( (double) x2)*y1;
	}

	area /= 2.0;

	return(area);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: PolygonClass::comp_bdy_area                                            ****/
/**** Compute area from list of boundary points                                        ****/
/******************************************************************************************/
double PolygonClass::comp_bdy_area(std::vector<std::tuple<int, int>> *ii_list)
{
	int i, x1, y1, x2, y2, n;
	double area;

	area = 0.0;

	n = ii_list->size();
	for (i=1; i<=n-2; i++) {
		x1 = std::get<0>((*ii_list)[i]) - std::get<0>((*ii_list)[0]);
		y1 = std::get<1>((*ii_list)[i]) - std::get<1>((*ii_list)[0]);
		x2 = std::get<0>((*ii_list)[i+1]) - std::get<0>((*ii_list)[0]);
		y2 = std::get<1>((*ii_list)[i+1]) - std::get<1>((*ii_list)[0]);

		area += ( (double) x1)*y2 - ( (double) x2)*y1;
	}

	area /= 2.0;

	return(area);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: in_bdy_area                                                            ****/
/**** Determine whether or not a given point lies within the bounded area              ****/
/******************************************************************************************/
int PolygonClass::in_bdy_area(const int a, const int b, const int n, const int *x, const int *y, int *edge)
{
	int i, num_left, num_right, same_y, index;
	int x1, y1, x2, y2, eps;

	index = -1;
	do {
		index++;
		if (index == n) { return(0); }
		x2 = x[index];
		y2 = y[index];
	} while (y2 == b);

	if (edge) { *edge = 0; }
	same_y   = 0;
	num_left = 0;
	num_right = 0;
	for (i=0; i<=n-1; i++) {
		if (index == n-1) {
			index = 0;
		} else {
			index++;
		}
		x1 = x2;
		y1 = y2;
		x2 = x[index];
		y2 = y[index];

		if ( (x2 == a) && (y2 == b) ) {
			if (edge) { *edge = 1; }
			return(0);
		}

		if (!same_y) {
			if (    ((y1 < b) && (b < y2))
					|| ((y1 > b) && (b > y2)) ) {
				if ( (x1 > a) && (x2 > a) ) {
					num_right++;
				} else if ( (x1 < a) && (x2 < a) ) {
					num_left++;
				} else {
					eps = ((x2-x1)*(b-y1)-(a-x1)*(y2-y1));
					if (eps == 0) {
						if (edge) { *edge = 1; }
						return(0);
					}
					if ( ((y1<y2) && (eps > 0)) || ((y1>y2) && (eps < 0)) ) {
						num_right++;
					} else {
						num_left++;
					}
				}
			} else if (y2 == b) {
				same_y = (y1 > b) ? 1 : -1;
			}
		} else {
			if (y2 == b) {
				if (  ((x1 <= a) && (a <= x2))
						||((x2 <= a) && (a <= x1)) ) {
					if (edge) { *edge = 1; }
					return(0);
				}
			} else {
				if (  ((y2 < b) && (same_y == 1))
						||((y2 > b) && (same_y == -1)) ) {
					if (x1 < a) {
						num_left++;
					} else {
						num_right++;
					}
				}
				same_y = 0;
			}
		}
	}

	if ((num_left + num_right) & 1) {
		printf("ERROR in routine in_bdy_area()\n");
		CORE_DUMP;
	}

	return(num_left&1);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: closestPoint                                                           ****/
/**** Determine whether or not a given point lies within the bounded area              ****/
/******************************************************************************************/
std::tuple<double, double> PolygonClass::closestPoint(std::tuple<int, int> point)
{
	std::tuple<double, double> cPoint;
	double cDistSq;

	bool initFlag = true;
	int xval, yval;
	std::tie(xval, yval) = point;
	for (int segment_idx=0; segment_idx<=num_segment-1; segment_idx++) {
		for (int bdy_pt_idx=0; bdy_pt_idx<=num_bdy_pt[segment_idx]-1; bdy_pt_idx++) {
			int bdy_pt_idx2 = (bdy_pt_idx+1) % num_bdy_pt[segment_idx];
			int x0 = bdy_pt_x[segment_idx][bdy_pt_idx];
			int y0 = bdy_pt_y[segment_idx][bdy_pt_idx];
			int x1 = bdy_pt_x[segment_idx][bdy_pt_idx2];
			int y1 = bdy_pt_y[segment_idx][bdy_pt_idx2];
			double Lsq = (x1-x0)*(x1-x0)+(y1-y0)*(y1-y0);
			double alpha = ((xval-x0)*(x1-x0)+(yval-y0)*(y1-y0))/Lsq;
			double ptx, pty, dsq;
			if (alpha <= 0.0) {
				ptx = (double) x0;
				pty = (double) y0;
			} else if (alpha >= 1.0) {
				ptx = (double) x1;
				pty = (double) y1;
			} else {
				ptx = (1.0-alpha)*x0 + alpha*x1;
				pty = (1.0-alpha)*y0 + alpha*y1;
			}
			dsq = (ptx - xval)*(ptx - xval) + (pty - yval)*(pty - yval);

			if (initFlag || (dsq < cDistSq)) {
				cDistSq = dsq;
				cPoint = std::tuple<double, double>(ptx, pty);
				initFlag = false;
			}
		}
	}

	return(cPoint);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: calcHorizExtents                                                       ****/
/******************************************************************************************/
void PolygonClass::calcHorizExtents(double yVal, double& xA, double& xB, bool& flag) const
{
	flag = false;
	for (int segment_idx=0; segment_idx<=num_segment-1; segment_idx++) {
		for (int bdy_pt_idx=0; bdy_pt_idx<=num_bdy_pt[segment_idx]-1; bdy_pt_idx++) {
			int bdy_pt_idx2 = (bdy_pt_idx+1) % num_bdy_pt[segment_idx];
			int x0 = bdy_pt_x[segment_idx][bdy_pt_idx];
			int y0 = bdy_pt_y[segment_idx][bdy_pt_idx];
			int x1 = bdy_pt_x[segment_idx][bdy_pt_idx2];
			int y1 = bdy_pt_y[segment_idx][bdy_pt_idx2];

			double xVal;
			bool found = false;
			if ((double) y0 == yVal) {
				found = true;
				xVal = (double) x0;
			} else if (    (((double) y0 < yVal) && ((double) y1 >= yVal))
					|| (((double) y0 > yVal) && ((double) y1 <= yVal)) ) {
				found = true;
				xVal = (x0*(y1-yVal) + x1*(yVal-y0))/(y1-y0);
			}
			if (found) {
				if (!flag) {
					flag = true;
					xA = xVal;
					xB = xVal;
				} else if (xVal < xA) {
					xA = xVal;
				} else if (xVal > xB) {
					xB = xVal;
				}
			}
		}
	}

	return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: calcHorizExtents                                                       ****/
/******************************************************************************************/
void PolygonClass::calcVertExtents (double xVal, double& yA, double& yB, bool& flag) const
{
	flag = false;
	for (int segment_idx=0; segment_idx<=num_segment-1; segment_idx++) {
		for (int bdy_pt_idx=0; bdy_pt_idx<=num_bdy_pt[segment_idx]-1; bdy_pt_idx++) {
			int bdy_pt_idx2 = (bdy_pt_idx+1) % num_bdy_pt[segment_idx];
			int x0 = bdy_pt_x[segment_idx][bdy_pt_idx];
			int y0 = bdy_pt_y[segment_idx][bdy_pt_idx];
			int x1 = bdy_pt_x[segment_idx][bdy_pt_idx2];
			int y1 = bdy_pt_y[segment_idx][bdy_pt_idx2];

			double yVal;
			bool found = false;
			if ((double) x0 == xVal) {
				found = true;
				yVal = (double) y0;
			} else if (    (((double) x0 < xVal) && ((double) x1 >= xVal))
					|| (((double) x0 > xVal) && ((double) x1 <= xVal)) ) {
				found = true;
				yVal = (y0*(x1-xVal) + y1*(xVal-x0))/(x1-x0);
			}
			if (found) {
				if (!flag) {
					flag = true;
					yA = yVal;
					yB = yVal;
				} else if (yVal < yA) {
					yA = yVal;
				} else if (yVal > yB) {
					yB = yVal;
				}
			}
		}
	}

	return;
}
/******************************************************************************************/


