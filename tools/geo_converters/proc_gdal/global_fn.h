/******************************************************************************************/
/**** FILE: global_fn.h                                                                ****/
/******************************************************************************************/

#ifndef GLOBAL_FN_H
#define GLOBAL_FN_H

#include <cstdio>
#include <complex>
#include <vector>
#include "global_defines.h"

int  gcd(int a, int b);
void extended_euclid(int a, int b, int& gcd, int& p1, int& p2);
int  fgetline(FILE *file, std::string& s, bool keepcr = true);
std::string getField(const std::string& string, int& posn, const char *chdelim);
std::vector<std::string> split(const std::string &s, char delim);
std::vector<std::string> splitOptions(const std::string &cmd);
std::vector<std::string> splitCSV(const std::string &cmd);
int  fgetline(FILE *, char *);
int  copyFile(const char *src, const char *dest);
int  fileExists(const char *filename);
char *remove_quotes(char *str);
std::string remove_quotes(const std::string& str);
char *escape_quotes(char *&str);
const char *getCSVField(const char *str, const char *&startPtr, int& fieldLen, const bool rmWhitsSpaceFlag = false, const char fs = ',');
void set_current_dir_from_file(char *filename);
char *insertFilePfx(const char *pfx, const char *filename);
std::string insertFilePfx(const std::string& pfx, const std::string& filename);
int  stringcmp(const char *s1, const char *s2);
void lowercase(char *s1);
void lowercase(std::string& s1);
void uppercase(std::string& s1);
void get_bits(char *str, int n, int num_bits, int insNull = 1);
void get_bits(char *str, LONGLONG_TYPE n, int num_bits, int insNull = 1);
void get_hex(char *str, int n, int num_hex, int insNull = 1);
int  is_big_endian();
bool deleteFile(const char *filename, bool noRecycleBin = true);
bool isFirstInstance(const char *mutexName);
int readTwoCol(double *x, double *y, int& numPts, const char *flname, int maxNumPts, int maxLineSize=500);
int readThreeCol(double *x, double *y, double *z, int& numPts, const char *flname, int maxNumPts, int maxLineSize=500);
void writeTwoCol(const double *x, const double *y, int n, const char *fmt, const char *flname);
void writeOneCol(const double *x, int n, const char *fmt, const char *flname);
int cvtStrToVal(char const* strptr, double& val);
int cvtStrToVal(char const* strptr, std::complex<double>& val);

#endif
