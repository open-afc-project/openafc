/******************************************************************************************/
/**** FILE: global_fn.h                                                                ****/
/**** Michael Mandell 1/15/02                                                          ****/
/******************************************************************************************/

#ifndef GLOBAL_FN_H
#define GLOBAL_FN_H

#include <cstdio>
#include <complex>
#include <vector>
#include "global_defines.h"

int  fgetline(FILE *file, std::string& s, bool keepcr = true);
int  fgetline(FILE *, char *);
std::vector<std::string> split(const std::string &s, char delim);
std::vector<std::string> splitCSV(const std::string &cmd);

#endif
