/******************************************************************************************/
/**** Automatically Generated file, DO NOT EDIT.                                       ****/
/**** CPP file generated by gen_parameter_template.pl.                                 ****/
/**** FILE: "parameter_template.cpp"                                                   ****/
/******************************************************************************************/

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <cstring>
#include <string>
#include <math.h>
#include <stdexcept>

#include "parameter_template.h"
#include "global_defines.h"
#include "global_fn.h"
#include "param_proc.h"

#include "inline_fn.h"

/******************************************************************************************/
/**** CONSTRUCTOR: ParameterTemplateClass::ParameterTemplateClass                      ****/
/******************************************************************************************/
ParameterTemplateClass::ParameterTemplateClass()
{
    name = "test";
    function = "combine3D2D";
    srcFile3D.clear();
    srcFile2D.clear();
    srcFileRaster.clear();
    srcFileVector.clear();
    srcHeightFieldName.clear();
    heightFieldName3D.clear();
    heightFieldName2D.clear();
    outputHeightFieldName.clear();
    cmpFileRaster.clear();
    outputFile.clear();
    outputLayer.clear();
    nodataVal = 1.0e30;
    clampMin = -100.0;
    clampMax = 5000.0;
    minMag = 0.0;
    tmpImageFile.clear();
    imageFile.clear();
    imageFile2.clear();
    imageLonLatRes = 0.0001;
    verbose = 0;
    minLon = 0.0;
    maxLon = 0.0;
    minLat = 0.0;
    maxLat = 0.0;
    samplesPerDeg = 120;
    polygonExpansion = 10;
    polygonSimplify = 2;
    kmzFile.clear();
    kmlFile.clear();
    minLonWrap = -180.0;
    seed = 0;
}
/******************************************************************************************/

/******************************************************************************************/
/**** DESTRUCTOR: ParameterTemplateClass::~ParameterTemplateClass                      ****/
/******************************************************************************************/
ParameterTemplateClass::~ParameterTemplateClass()
{
}
/******************************************************************************************/

/******************************************************************************************/
/**** ParameterTemplateClass::readFile()                                            ****/
/******************************************************************************************/
void ParameterTemplateClass::readFile(const char *filename)
{
    int linenum;
    char *str1;
    char *line = (char *) malloc(10000*sizeof(char));
    char *format_str = (char *) NULL;
    FILE *fp = (FILE *) NULL;
    std::ostringstream errStr;

    if (!filename) {
        errStr << "ERROR: No template file specified." << std::endl;
        throw std::runtime_error(errStr.str());
    }

    fp = fopen(filename, "rb");
    if (!fp) {
        errStr << "ERROR: Unable to open template file \"" << filename << "\" for reading." << std::endl;
        throw std::runtime_error(errStr.str());
    }

#if CDEBUG
    printf("Reading template file: \"%s\"\n", filename);
#endif

    enum StateEnum {
        STATE_FORMAT,
        STATE_READ_VERSION
    };

    StateEnum state;

    state = STATE_FORMAT;
    linenum = 0;

    while ( (state != STATE_READ_VERSION) && fgetline(fp, line) ) {
        linenum++;
        str1 = strtok(line, CHDELIM);
        if ( str1 && (str1[0] != '#') ) {
            switch(state) {
                case STATE_FORMAT:
                    if (strcmp(str1, "FORMAT:") != 0) {
                        errStr << "ERROR: Invalid template file \"" << filename << "\":" << linenum
                               << " expecting \"FORMAT:\" NOT \"" << str1 << "\"" << std::endl;
                        throw std::runtime_error(errStr.str());
                    } else {
                        str1 = strtok(NULL, CHDELIM);
                        format_str = strdup(str1);
                    }
                    state = STATE_READ_VERSION;
                    break;
                default:
                    errStr << "ERROR: Invalid template file \"" << filename << "\"" << std::endl;
                    throw std::runtime_error(errStr.str());
                    break;
            }
        }
    }

    if (state != STATE_READ_VERSION) {
        errStr << "ERROR: Invalid template file \"" << filename << "\":" << linenum
            << " premature end of file encountered" << std::endl;
        throw std::runtime_error(errStr.str());
    }

    if (strcmp(format_str,"1_0")==0) {
        readFile_1_0(fp, line, filename, linenum);
    } else {
        errStr << "ERROR: Invalid template file \"" << filename << "\""
            << " format set to illegal value \"" << format_str << "\"" << std::endl;
        throw std::runtime_error(errStr.str());
    }

    free(line);
    if (format_str) { free(format_str); }

    if (fp) { fclose(fp); }
}
/******************************************************************************************/

/******************************************************************************************/
/**** ParameterTemplateClass::readFile_1_0()                                        ****/
/******************************************************************************************/
void ParameterTemplateClass::readFile_1_0(FILE *fp, char *line, const char *filename, int linenum)
{
    int paramIdx, numParam;
    double scale, dval;
    bool found;
    int valid;
    char *str1, *str2;
    char *paramName, *paramVal, *paramUnit;
    std::ostringstream errStr;

    ParamProcClass *paramProc = new ParamProcClass(filename, "Template File");

#if CDEBUG
    printf("Reading template file format 1.0: \"%s\"\n", filename);
#endif

    enum StateEnum {
        STATE_NUM_PARAM,
        STATE_PARAM,

        STATE_DONE
    };

    StateEnum state;

    state = STATE_NUM_PARAM;

    while (fgetline(fp, line)) {
#if 0
        printf("%s", line);
#endif
        linenum++;
        str1 = strtok(line, CHDELIM ":");
        if ( str1 && (str1[0] != '#') ) {
            str2 = strtok(NULL, "\n");
            while((str2) && (*str2 == ' ')) { str2++; }
            switch(state) {
                case STATE_NUM_PARAM:
                    paramProc->getParamVal(numParam, "NUM_PARAM", linenum, str1, str2);
                    if (numParam) {
                        paramIdx = 0;
                        state = STATE_PARAM;
                    } else {
                        state = STATE_DONE;
                    }
                    break;
                case STATE_PARAM:
                    checkStr("PARAM", paramIdx, linenum, str1, filename);
                    paramName = strtok(str2, CHDELIM);
                    paramVal  = strtok(NULL, "\n");
                    found = false;
                    valid = true;

                    if ((!found) && (strcmp(paramName, "NAME")==0)) {
                        paramProc->getParamVal(name, "NAME", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "FUNCTION")==0)) {
                        paramProc->getParamVal(function, "FUNCTION", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "SRC_FILE_3D")==0)) {
                        paramProc->getParamVal(srcFile3D, "SRC_FILE_3D", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "SRC_FILE_2D")==0)) {
                        paramProc->getParamVal(srcFile2D, "SRC_FILE_2D", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "SRC_FILE_RASTER")==0)) {
                        paramProc->getParamVal(srcFileRaster, "SRC_FILE_RASTER", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "SRC_FILE_VECTOR")==0)) {
                        paramProc->getParamVal(srcFileVector, "SRC_FILE_VECTOR", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "SRC_HEIGHT_FIELD_NAME")==0)) {
                        paramProc->getParamVal(srcHeightFieldName, "SRC_HEIGHT_FIELD_NAME", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "HEIGHT_FIELD_NAME_3D")==0)) {
                        paramProc->getParamVal(heightFieldName3D, "HEIGHT_FIELD_NAME_3D", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "HEIGHT_FIELD_NAME_2D")==0)) {
                        paramProc->getParamVal(heightFieldName2D, "HEIGHT_FIELD_NAME_2D", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "OUTPUT_HEIGHT_FIELD_NAME")==0)) {
                        paramProc->getParamVal(outputHeightFieldName, "OUTPUT_HEIGHT_FIELD_NAME", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "CMP_FILE_RASTER")==0)) {
                        paramProc->getParamVal(cmpFileRaster, "CMP_FILE_RASTER", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "OUTPUT_FILE")==0)) {
                        paramProc->getParamVal(outputFile, "OUTPUT_FILE", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "OUTPUT_LAYER")==0)) {
                        paramProc->getParamVal(outputLayer, "OUTPUT_LAYER", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "NODATA_VAL")==0)) {
                        paramProc->getParamVal(nodataVal, "NODATA_VAL", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "CLAMP_MIN")==0)) {
                        paramProc->getParamVal(clampMin, "CLAMP_MIN", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "CLAMP_MAX")==0)) {
                        paramProc->getParamVal(clampMax, "CLAMP_MAX", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "MIN_MAG")==0)) {
                        paramProc->getParamVal(minMag, "MIN_MAG", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "TMP_IMAGE_FILE")==0)) {
                        paramProc->getParamVal(tmpImageFile, "TMP_IMAGE_FILE", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "IMAGE_FILE")==0)) {
                        paramProc->getParamVal(imageFile, "IMAGE_FILE", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "IMAGE_FILE_2")==0)) {
                        paramProc->getParamVal(imageFile2, "IMAGE_FILE_2", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "IMAGE_LON_LAT_RES")==0)) {
                        paramProc->getParamVal(imageLonLatRes, "IMAGE_LON_LAT_RES", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "VERBOSE")==0)) {
                        paramProc->getParamVal(verbose, "VERBOSE", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "MIN_LON")==0)) {
                        paramProc->getParamVal(minLon, "MIN_LON", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "MAX_LON")==0)) {
                        paramProc->getParamVal(maxLon, "MAX_LON", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "MIN_LAT")==0)) {
                        paramProc->getParamVal(minLat, "MIN_LAT", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "MAX_LAT")==0)) {
                        paramProc->getParamVal(maxLat, "MAX_LAT", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "SAMPLES_PER_DEG")==0)) {
                        paramProc->getParamVal(samplesPerDeg, "SAMPLES_PER_DEG", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "POLYGON_EXPANSION")==0)) {
                        paramProc->getParamVal(polygonExpansion, "POLYGON_EXPANSION", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "POLYGON_SIMPLIFY")==0)) {
                        paramProc->getParamVal(polygonSimplify, "POLYGON_SIMPLIFY", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "KMZ_FILE")==0)) {
                        paramProc->getParamVal(kmzFile, "KMZ_FILE", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "KML_FILE")==0)) {
                        paramProc->getParamVal(kmlFile, "KML_FILE", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "MIN_LON_WRAP")==0)) {
                        paramProc->getParamVal(minLonWrap, "MIN_LON_WRAP", linenum, paramName, paramVal);
                        found = true;
                    }
                    if ((!found) && (strcmp(paramName, "SEED")==0)) {
                        paramProc->getParamVal(seed, "SEED", linenum, paramName, paramVal);
                        found = true;
                    }

                    if (!found) {
                        errStr << "ERROR: Invalid template file \"" << filename << "\":" << linenum
                               << ", invalid parameter name \"" << paramName << "\"" << std::endl;
                        throw std::runtime_error(errStr.str());
                    }
                    if (!valid) {
                        errStr << "ERROR: Invalid template file \"" << filename << "\":" << linenum
                               << ", invalid parameter value \"" << paramName << "\" = \"" << paramVal << "\"" << std::endl;
                        throw std::runtime_error(errStr.str());
                    }
                    paramIdx++;
                    if (paramIdx == numParam) {
                        state = STATE_DONE;
                    }
                    break;
                default:
                    errStr << "ERROR: Invalid template file \"" << filename << "\":" << linenum
                           << ", invalid state encountered." << std::endl;
                    throw std::runtime_error(errStr.str());
                    break;
            }
        }
    }

    if (state != STATE_DONE) {
        errStr << "ERROR: Invalid template file \"" << filename << "\":" << linenum
            << " premature end of file encountered" << std::endl;
        throw std::runtime_error(errStr.str());
    }

    delete paramProc;

    return;
}
/******************************************************************************************/

/******************************************************************************************/
/**** ParameterTemplateClass::print()                                                  ****/
/******************************************************************************************/
void ParameterTemplateClass::print(FILE *fp) const
{
    fprintf(fp, "NAME: %s\n", (name.empty() ? "NONE" : name.c_str()));
    fprintf(fp, "FUNCTION: %s\n", (function.empty() ? "NONE" : function.c_str()));
    fprintf(fp, "SRC_FILE_3D: %s\n", (srcFile3D.empty() ? "NONE" : srcFile3D.c_str()));
    fprintf(fp, "SRC_FILE_2D: %s\n", (srcFile2D.empty() ? "NONE" : srcFile2D.c_str()));
    fprintf(fp, "SRC_FILE_RASTER: %s\n", (srcFileRaster.empty() ? "NONE" : srcFileRaster.c_str()));
    fprintf(fp, "SRC_FILE_VECTOR: %s\n", (srcFileVector.empty() ? "NONE" : srcFileVector.c_str()));
    fprintf(fp, "SRC_HEIGHT_FIELD_NAME: %s\n", (srcHeightFieldName.empty() ? "NONE" : srcHeightFieldName.c_str()));
    fprintf(fp, "HEIGHT_FIELD_NAME_3D: %s\n", (heightFieldName3D.empty() ? "NONE" : heightFieldName3D.c_str()));
    fprintf(fp, "HEIGHT_FIELD_NAME_2D: %s\n", (heightFieldName2D.empty() ? "NONE" : heightFieldName2D.c_str()));
    fprintf(fp, "OUTPUT_HEIGHT_FIELD_NAME: %s\n", (outputHeightFieldName.empty() ? "NONE" : outputHeightFieldName.c_str()));
    fprintf(fp, "CMP_FILE_RASTER: %s\n", (cmpFileRaster.empty() ? "NONE" : cmpFileRaster.c_str()));
    fprintf(fp, "OUTPUT_FILE: %s\n", (outputFile.empty() ? "NONE" : outputFile.c_str()));
    fprintf(fp, "OUTPUT_LAYER: %s\n", (outputLayer.empty() ? "NONE" : outputLayer.c_str()));
    fprintf(fp, "NODATA_VAL: %15.10e\n", nodataVal);
    fprintf(fp, "CLAMP_MIN (m): %15.10e\n", clampMin);
    fprintf(fp, "CLAMP_MAX (m): %15.10e\n", clampMax);
    fprintf(fp, "MIN_MAG (m): %15.10e\n", minMag);
    fprintf(fp, "TMP_IMAGE_FILE: %s\n", (tmpImageFile.empty() ? "NONE" : tmpImageFile.c_str()));
    fprintf(fp, "IMAGE_FILE: %s\n", (imageFile.empty() ? "NONE" : imageFile.c_str()));
    fprintf(fp, "IMAGE_FILE_2: %s\n", (imageFile2.empty() ? "NONE" : imageFile2.c_str()));
    fprintf(fp, "IMAGE_LON_LAT_RES (deg): %15.10e\n", imageLonLatRes);
    fprintf(fp, "VERBOSE: %d\n", verbose);
    fprintf(fp, "MIN_LON (deg): %15.10e\n", minLon);
    fprintf(fp, "MAX_LON (deg): %15.10e\n", maxLon);
    fprintf(fp, "MIN_LAT (deg): %15.10e\n", minLat);
    fprintf(fp, "MAX_LAT (deg): %15.10e\n", maxLat);
    fprintf(fp, "SAMPLES_PER_DEG: %d\n", samplesPerDeg);
    fprintf(fp, "POLYGON_EXPANSION: %d\n", polygonExpansion);
    fprintf(fp, "POLYGON_SIMPLIFY: %d\n", polygonSimplify);
    fprintf(fp, "KMZ_FILE: %s\n", (kmzFile.empty() ? "NONE" : kmzFile.c_str()));
    fprintf(fp, "KML_FILE: %s\n", (kmlFile.empty() ? "NONE" : kmlFile.c_str()));
    fprintf(fp, "MIN_LON_WRAP: %15.10e\n", minLonWrap);
    fprintf(fp, "SEED: %d\n", seed);
    fprintf(fp, "\n");
}
/******************************************************************************************/


/******************************************************************************************/
/**** END of CPP file generated by gen_parameter_template.pl                           ****/
/******************************************************************************************/
