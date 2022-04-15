/******************************************************************************************/
/**** FILE: local_defines.h                                                            ****/
/******************************************************************************************/

#ifndef LOCAL_DEFINES_H
#define LOCAL_DEFINES_H

#ifndef APPLICATION_RELEASE
#define APPLICATION_RELEASE          "NON_RELEASE" /* APPLICATION_RELEASE version, ex "040531.1" */
#endif

#ifndef CDEBUG
#define CDEBUG                       0 /* Whether or not to compile in debug mode         */
/* Note that DEBUG is used by Qt, so CDEBUG is     */
/* used to avoid conflict.                         */
#endif

#define FORCE_EQV_EXCEL              0 /* Force computation to be exactly same as excel   */

#define INVALID_METRIC     0x3FFFFFFF

#endif
