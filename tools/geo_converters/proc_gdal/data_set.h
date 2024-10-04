/******************************************************************************************/
/**** FILE : data_set.h                                                                ****/
/******************************************************************************************/

#ifndef DATA_SET_H
#define DATA_SET_H

#include "parameter_template.h"

/******************************************************************************************/
/**** CLASS: DataSetClass                                                               ****/
/******************************************************************************************/
class DataSetClass
{
public:
    DataSetClass();
    ~DataSetClass();
    void run();

    /**************************************************************************************/
    /**** Template Parameters                                                          ****/
    /**************************************************************************************/
    ParameterTemplateClass parameterTemplate;
    /**************************************************************************************/
    
private:
     void combine3D2D();
     void fixRaster();
     void fixVector();
     void vectorCvg();
     void vectorCvgKMZ();
     void mbRasterCvg();
     void rasterDumpText();
     void diffRaster();
     void cmpRaster();
     void createSampleTiff();
     void procNLCD();
     void procBoundary();

    /**************************************************************************************/
    /**** Data                                                                         ****/
    /**************************************************************************************/

    /**************************************************************************************/

};
/******************************************************************************************/

#endif
