/******************************************************************************************/
/**** FILE: global_defines.h                                                           ****/
/**** Michael Mandell 2006.06.16                                                       ****/
/******************************************************************************************/

#ifndef GLOBAL_DEFINES_H
#define GLOBAL_DEFINES_H

#define MAX_LINE_SIZE             5000
#define CHDELIM                " \t\n" /* Delimiting characters, used for string parsing  */

#define IVECTOR(nn) (int    *) ( (nn) ? malloc((nn)    *sizeof(int)   ) : NULL )
#define DVECTOR(nn) (double *) ( (nn) ? malloc((nn)    *sizeof(double)) : NULL )
#define CVECTOR(nn) (char   *) ( (nn) ? malloc(((nn)+1)*sizeof(char)  ) : NULL )

#define CORE_DUMP      printf("%d", *((int *) NULL))
/******************************************************************************************/

#endif
