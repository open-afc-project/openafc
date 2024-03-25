/******************************************************************************************/
/**** FILE: inline_fn.h                                                                ****/
/******************************************************************************************/

#ifndef INLINE_FN_H
#define INLINE_FN_H

#include "global_defines.h"
#include <cstdlib>
#include <string.h>
#include <stdio.h>

/******************************************************************************************/
/**** Inline functions for reading data                                                ****/
/******************************************************************************************/
inline void checkStr(const char *varname, int linenum, char *strname, const char *filename)
{
    std::ostringstream errStr;

    if (strcmp(strname, varname) != 0) {
        errStr << "ERROR: Invalid file \"" << filename << "\":" << linenum
            << " expecting \"" << varname << "\" NOT \"" << strname << "\"" << std::endl;
        throw std::runtime_error(errStr.str());
        return;
    }
}

inline void checkStr(const char *varname, int idx, int linenum, char *strname, const char *filename)
{
    char *tmpstr = (char *) malloc(100);
    sprintf(tmpstr, "%s_%d", varname, idx);
    checkStr(tmpstr, linenum, strname, filename);
    free(tmpstr);
}

inline void checkStr(const char *varname, int idx1, int idx2, int linenum, char *strname, const char *filename)
{
    char *tmpstr = (char *) malloc(100);
    sprintf(tmpstr, "%s_%d_%d", varname, idx1, idx2);
    checkStr(tmpstr, linenum, strname, filename);
    free(tmpstr);
}

inline void getParamVal(int &ivar, const char *varname, int linenum, char *strname, char *strval, const char *filename)
{
    char *chptr;
    std::ostringstream errStr;

    checkStr(varname, linenum, strname, filename);

    chptr = strtok(strval, CHDELIM);
    if (chptr) {
        ivar = atoi(chptr);
    } else {
        errStr << "ERROR: Invalid file \"" << filename << "\":" << linenum
            << " variable \"" << varname << "\" not specified" << std::endl;
        throw std::runtime_error(errStr.str());
        return;
    }
}

inline void getParamVal(double &dvar, const char *varname, int linenum, char *strname, char *strval, const char *filename)
{
    char *chptr;
    std::ostringstream errStr;

    checkStr(varname, linenum, strname, filename);

    chptr = strtok(strval, CHDELIM);
    if (chptr) {
        dvar = atof(chptr);
    } else {
        errStr << "ERROR: Invalid file \"" << filename << "\":" << linenum
            << " variable \"" << varname << "\" not specified" << std::endl;
        throw std::runtime_error(errStr.str());
        return;
    }
}

inline void getParamVal(char *&svar, const char *varname, int linenum, char *strname, char *strval, const char *filename)
{
    char *chptr;
    std::ostringstream errStr;

    checkStr(varname, linenum, strname, filename);

    chptr = strtok(strval, CHDELIM);
    if (chptr) {
        svar = strdup(chptr);
    } else {
        errStr << "ERROR: Invalid file \"" << filename << "\":" << linenum
            << " variable \"" << varname << "\" not specified" << std::endl;
        throw std::runtime_error(errStr.str());
        return;
    }
}
/******************************************************************************************/

#endif
