/******************************************************************************************/
/**** PROGRAM: param_proc.cpp                                                          ****/
/**** Michael Mandell 10/28/05                                                         ****/
/******************************************************************************************/

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include "global_fn.h"
#include "param_proc.h"

/******************************************************************************************/
/**** FUNCTION: ParamProcClass::ParamProcClass                                         ****/
/******************************************************************************************/
ParamProcClass::ParamProcClass(std::string p_filename, std::string p_filetype) {
    filename = p_filename;
    filetype = p_filetype;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ParamProcClass::~ParamProcClass                                        ****/
/******************************************************************************************/
ParamProcClass::~ParamProcClass() {
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ParamProcClass::getParamVal (std::string versions)                     ****/
/******************************************************************************************/
void ParamProcClass::getParamVal(bool &bvar, const std::string& varname, int linenum, const std::string& strname, const std::string& strval)
{
    int posn = 0;
    std::string fieldVal;
    std::ostringstream s;

    if (strname != varname) {
        s << "ERROR: Invalid " << filetype << " file \"" << filename << "(" << linenum << ")\"" << std::endl
          << "Expecting \"" << varname << "\" NOT \"" << strname << "\"";
        throw std::runtime_error(s.str());
        return;
    }
    fieldVal = getField(strval, posn, CHDELIM);
    if (!fieldVal.empty()) {
        if (fieldVal == "true") {
            bvar = true;
        } else if (fieldVal == "false") {
            bvar = false;
        } else {
            s << "ERROR: Invalid " << filetype << " file \"" << filename << "(" << linenum << ")\"" << std::endl
              << "Variable \"" << varname << "\" set to illegal bool value \"" << fieldVal << "\"";
            throw std::runtime_error(s.str());
        }
    } else {
        s << "ERROR: Invalid " << filetype << " file \"" << filename << "(" << linenum << ")\"" << std::endl
          << "No \"" << varname << "\" specified";
        throw std::runtime_error(s.str());
        return;
    }
}

void ParamProcClass::getParamVal(int &ivar, const std::string& varname, int linenum, const std::string& strname, const std::string& strval)
{
    int posn = 0;
    std::string fieldVal;
    std::ostringstream s;

    if (strname != varname) {
        s << "ERROR: Invalid " << filetype << " file \"" << filename << "(" << linenum << ")\"" << std::endl
          << "Expecting \"" << varname << "\" NOT \"" << strname << "\"";
        throw std::runtime_error(s.str());
        return;
    }
    fieldVal = getField(strval, posn, CHDELIM);
    if (!fieldVal.empty()) {
        ivar = atoi(fieldVal.c_str());
    } else {
        s << "ERROR: Invalid " << filetype << " file \"" << filename << "(" << linenum << ")\"" << std::endl
          << "No \"" << varname << "\" specified";
        throw std::runtime_error(s.str());
        return;
    }
}


#if 0
void ParamProcClass::getParamVal(int &ivar, int idx, const char *varname, int linenum, char *strname, char *strval)
{
    sprintf(tmpstr, "%s_%d", varname, idx);
    getParamVal(ivar, tmpstr, linenum, strname, strval);
}
#endif

void ParamProcClass::getParamVal(double &dvar, const std::string& varname, int linenum, const std::string& strname, const std::string& strval)
{
    int posn = 0;
    std::string fieldVal;
    std::ostringstream s;

    if (strname != varname) {
        s << "ERROR: Invalid " << filetype << " file \"" << filename << "(" << linenum << ")\"" << std::endl
          << "Expecting \"" << varname << "\" NOT \"" << strname << "\"";
        throw std::runtime_error(s.str());
        return;
    }
    fieldVal = getField(strval, posn, CHDELIM);
    if (!fieldVal.empty()) {
        dvar = atof(fieldVal.c_str());
    } else {
        s << "ERROR: Invalid " << filetype << " file \"" << filename << "(" << linenum << ")\"" << std::endl
          << "No \"" << varname << "\" specified";
        throw std::runtime_error(s.str());
        return;
    }
}

void ParamProcClass::getParamVal(double &dvar, int idx, const std::string& varname, int linenum, const std::string& strname, const std::string& strval)
{
    std::ostringstream s;
    s << varname << "_" << idx;
    getParamVal(dvar, s.str(), linenum, strname, strval);
}

void ParamProcClass::getParamVal(char* &svar, const std::string& varname, int linenum, const std::string& strname, const std::string& strval)
{
    int posn = 0;
    std::string fieldVal;
    std::ostringstream s;

    if (strname != varname) {
        s << "ERROR: Invalid " << filetype << " file \"" << filename << "(" << linenum << ")\"" << std::endl
          << "Expecting \"" << varname << "\" NOT \"" << strname << "\"";
        throw std::runtime_error(s.str());
        return;
    }
    fieldVal = remove_quotes(strval);
    if (svar) { free(svar); }
    if (fieldVal != strval) { // Double quotes were removed
        svar = strdup(fieldVal.c_str());
    } else {
        fieldVal = getField(strval, posn, CHDELIM);
        if (fieldVal == "NULL") {
            svar = (char *) NULL;
        } else {
            s << "ERROR: Invalid " << filetype << " file \"" << filename << "(" << linenum << ")\"" << std::endl
              << "Invalid double-quoted string specified for " << varname;
            throw std::runtime_error(s.str());
            return;
        }
    }
}

#if 0
void ParamProcClass::getParamVal(char* &svar, int idx, const char *varname, int linenum, char *strname, char *strval)
{
    sprintf(tmpstr, "%s_%d", varname, idx);
    getParamVal(svar, tmpstr, linenum, strname, strval);
}
#endif

void ParamProcClass::getParamVal(std::string &svar, const std::string& varname, int linenum, const std::string& strname, const std::string& strval)
{
    int posn = 0;
    std::string fieldVal;
    std::ostringstream s;

    if (strname != varname) {
        s << "ERROR: Invalid " << filetype << " file \"" << filename << "(" << linenum << ")\"" << std::endl
          << "Expecting \"" << varname << "\" NOT \"" << strname << "\"";
        throw std::runtime_error(s.str());
        return;
    }
    fieldVal = remove_quotes(strval);
    if (fieldVal != strval) { // Double quotes were removed
        svar = fieldVal;
    } else {
        fieldVal = getField(strval, posn, CHDELIM);
        if (fieldVal == "NULL") {
            svar.clear();
        } else {
            s << "ERROR: Invalid " << filetype << " file \"" << filename << "(" << linenum << ")\"" << std::endl
              << "Invalid double-quoted string specified for " << varname;
            throw std::runtime_error(s.str());
            return;
        }
    }
}

void ParamProcClass::getParamVal(std::complex<double> &cvar, const std::string& varname, int linenum, const std::string& strname, const std::string& strval)
{
    std::string fieldVal;
    std::ostringstream s;

    if (strname != varname) {
        s << "ERROR: Invalid " << filetype << " file \"" << filename << "(" << linenum << ")\"" << std::endl
          << "Expecting \"" << varname << "\" NOT \"" << strname << "\"";
        throw std::runtime_error(s.str());
        return;
    }
    if (!strval.empty()) {
        cvtStrToVal(strval.c_str(), cvar);
    } else {
        s << "ERROR: Invalid " << filetype << " file \"" << filename << "(" << linenum << ")\"" << std::endl
          << "No \"" << varname << "\" specified";
        throw std::runtime_error(s.str());
        return;
    }
}

void ParamProcClass::getParamVal(std::complex<double> &cvar, int idx, const std::string& varname, int linenum, const std::string& strname, const std::string& strval)
{
    std::ostringstream s;
    s << varname << "_" << idx;
    getParamVal(cvar, s.str(), linenum, strname, strval);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: ParamProcClass::checkStr                                               ****/
/******************************************************************************************/
void ParamProcClass::checkStr(const std::string& varname, int linenum, const std::string& strname)
{
    std::ostringstream s;

    if (strname != varname) {
        s << "ERROR: Invalid " << filetype << " file \"" << filename << "(" << linenum << ")\"" << std::endl
          << "Expecting \"" << varname << "\" NOT \"" << strname << "\"";
        throw std::runtime_error(s.str());
        return;
    }
}
void ParamProcClass::checkStr(const std::string& varname, int idx, int linenum, const std::string& strname)
{
    std::ostringstream tmpStream;
    tmpStream << varname << "_" << idx;
    checkStr(tmpStream.str(), linenum, strname);
}
/******************************************************************************************/

