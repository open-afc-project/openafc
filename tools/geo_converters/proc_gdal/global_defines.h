/******************************************************************************************/
/**** FILE: global_defines.h                                                           ****/
/******************************************************************************************/

#ifndef GLOBAL_DEFINES_H
#define GLOBAL_DEFINES_H

#ifndef __linux__
#include <direct.h>
#endif

#include <cstdio>
#include <cstdlib>

#define MAX_LINE_SIZE             5000
#define CHDELIM                " \t\n" /* Delimiting characters, used for string parsing  */
#define LEFT_BRACE                 "{"
#define RIGHT_BRACE                "}"
#define LEFT_PAREN                 "("
#define RIGHT_PAREN                ")"
#define PI             3.1415926535897932384626433832795029

#define IVECTOR(nn) (int    *) ( (nn) ? malloc((nn)    *sizeof(int)   ) : NULL )
#define DVECTOR(nn) (double *) ( (nn) ? malloc((nn)    *sizeof(double)) : NULL )
#define CVECTOR(nn) (char   *) ( (nn) ? malloc(((nn)+1)*sizeof(char)  ) : NULL )

#define CORE_DUMP      printf("%d", *((int *) NULL))
/******************************************************************************************/

#define BITWIDTH(www, nnn) {                  \
    int tmp;                                  \
    tmp = (nnn);                              \
    www = 0;                                  \
    while (tmp) {                             \
        www++;                                \
        tmp>>=1;                              \
    }                                         \
}

#define MOD(m, n)  ( (m) >= 0 ? (m)%(n) : ((n)-1) - ((-(m)-1)%(n)) )

#define DIV(m, n) (                                      \
    ((m) >= 0)&&((n) >= 0) ? (m)/(n) :                   \
    ((m) <  0)&&((n) <  0) ? (-(m))/(-(n)) :             \
    ((m) <  0)&&((n) >= 0) ? -((-(m)-1)/(n) + 1) :       \
                             -(((m)-1)/(-(n)) + 1) )

#ifdef __linux__
#include <stdint.h>
#define LONGLONG_TYPE long long
#define LONGLONG_VAL(x) x ## LL
#define UNSLONGLONG_TYPE unsigned long long
#define UNSLONGLONG_VAL(x) x ## ULL
#define UNSLONG_TYPE uint32_t
#define FPATH_SEPARATOR '/'
#define SOCKET_ERRNO errno
#define STRCASECMP strcasecmp
#define MKDIR(ddd) mkdir(ddd, 0755)
#else
#include <windows.h>
#define LONGLONG_TYPE __int64
#define LONGLONG_VAL(x) x ## i64
#define UNSLONGLONG_TYPE __uint64
#define UNSLONGLONG_VAL(x) x ## ui64
#define UNSLONG_TYPE u_long
#define FPATH_SEPARATOR '\\'
#define SOCKET_ERRNO WSAGetLastError()
#define STRCASECMP stricmp
#define MKDIR(ddd) mkdir(ddd)
#define snprintf sprintf_s
#endif

#define GUIDBG(mmm) QMessageBox::critical(0, "GUIDBG", mmm,                \
            QMessageBox::Ok | QMessageBox::Default | QMessageBox::Escape,  \
            QMessageBox::NoButton,                                         \
            QMessageBox::NoButton                                          \
        );

#define xstr(s) mfkstr(s)
#define mfkstr(s) #s

#endif
