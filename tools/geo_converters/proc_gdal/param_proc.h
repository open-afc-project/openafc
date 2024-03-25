/******************************************************************************************/
/**** FILE: param_proc.h                                                               ****/
/******************************************************************************************/

#ifndef PARAM_PROC_H
#define PARAM_PROC_H

#include <string>
#include <complex>

/******************************************************************************************/
/**** CLASS: ParamProcClass                                                            ****/
/******************************************************************************************/
class ParamProcClass
{
public:
    ParamProcClass(std::string p_filename, std::string p_filetype);
    ~ParamProcClass();

    /**************************************************************************************/
    /* Use std::string                                                                    */
    /**************************************************************************************/
    void getParamVal(bool   &bvar, const std::string& varname, int linenum, const std::string& strname, const std::string& strval);
    void getParamVal(int    &ivar, const std::string& varname, int linenum, const std::string& strname, const std::string& strval);
    // void getParamVal(int    &ivar, int idx, const char *varname, int linenum, char *strname, char *strval);
    void getParamVal(double &dvar, const std::string& varname, int linenum, const std::string& strname, const std::string& strval);
    void getParamVal(double &dvar, int idx, const std::string& varname, int linenum, const std::string& strname, const std::string& strval);
    void getParamVal(char*  &svar, const std::string& varname, int linenum, const std::string& strname, const std::string& strval);
    // void getParamVal(char*  &svar, int idx, const char *varname, int linenum, char *strname, char *strval);
    void getParamVal(std::string &svar, const std::string& varname, int linenum, const std::string& strname, const std::string& strval);
    void getParamVal(std::complex<double> &cvar, const std::string& varname, int linenum, const std::string& strname, const std::string& strval);
    void getParamVal(std::complex<double> &cvar, int idx, const std::string& varname, int linenum, const std::string& strname, const std::string& strval);

    void checkStr(const std::string& varname, int linenum, const std::string& strname);
    void checkStr(const std::string& varname, int idx, int linenum, const std::string& strname);
    /**************************************************************************************/

private:
    int *error_state_ptr;
    std::string filename;
    std::string filetype;
};
/******************************************************************************************/

#endif
