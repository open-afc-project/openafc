/******************************************************************************************/
/**** FILE: str_type.h                                                                 ****/
/******************************************************************************************/

#ifndef STR_TYPE_H
#define STR_TYPE_H

#include <string>

class StrTypeClass
{
    public:
        int type_num;
        const char *type_str;

        int str_to_type(const char *typestr, int &validFlag, int err = 0) const;
        int str_to_type(const std::string &typestr, int &validFlag, int err = 0) const;
        const char *type_to_str(int type) const;
        int valid(int type, int *idxPtr = (int *)0) const;
};

#endif
