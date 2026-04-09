/******************************************************************************************/
/**** PROGRAM: str_type.cpp                                                            ****/
/******************************************************************************************/
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <string>

#include "global_defines.h"

#include "str_type.h"

/******************************************************************************************/
/**** FUNCTION: StrTypeClass::str_to_type                                              ****/
/******************************************************************************************/
int StrTypeClass::str_to_type(const char *typestr, int &validFlag, int err) const
{
	int type = -1, i;

	i = 0;
	validFlag = 0;

	if (typestr) {
		while ((this[i].type_str) && (!validFlag)) {
			if (strcmp(typestr, this[i].type_str) == 0) {
				type = this[i].type_num;
				validFlag = 1;
			} else {
				i++;
			}
		}
	}

	if (!validFlag) {
		type = -1;
		if (err) {
			// Invalid external input must be rejected by exception (caught
			// and converted to an AFC error response in main.cpp), not by
			// process abort.
			throw std::runtime_error(std::string("str_to_type(): invalid value \"") +
						 (typestr ? typestr : "(null)") +
						 "\" not found in string type list");
		}
	}

	return (type);
}

int StrTypeClass::str_to_type(const std::string &typestr, int &validFlag, int err) const
{
	int type = -1, i;

	i = 0;
	validFlag = 0;

	if (!typestr.empty()) {
		while ((this[i].type_str) && (!validFlag)) {
			if (strcmp(typestr.c_str(), this[i].type_str) == 0) {
				type = this[i].type_num;
				validFlag = 1;
			} else {
				i++;
			}
		}
	}

	if (!validFlag) {
		type = -1;
		if (err) {
			// Invalid external input must be rejected by exception (caught
			// and converted to an AFC error response in main.cpp), not by
			// process abort.
			throw std::runtime_error("str_to_type(): invalid value \"" + typestr +
						 "\" not found in string type list");
		}
	}

	return (type);
}
/******************************************************************************************/
/**** FUNCTION: StrTypeClass::type_to_str                                              ****/
/******************************************************************************************/
const char *StrTypeClass::type_to_str(int type) const
{
	int i, found;
	const char *typestr = (const char *)NULL;

	i = 0;
	found = 0;

	while ((this[i].type_str) && (!found)) {
		if (this[i].type_num == type) {
			typestr = this[i].type_str;
			found = 1;
		} else {
			i++;
		}
	}

	if (!found) {
		throw std::runtime_error("type_to_str(): invalid type " + std::to_string(type) +
					 " not found in string type list");
	}

	return (typestr);
}
/******************************************************************************************/
/**** FUNCTION: StrTypeClass::valid                                                    ****/
/******************************************************************************************/
int StrTypeClass::valid(int type, int *idxPtr) const
{
	int i, found;

	i = 0;
	found = 0;
	if (idxPtr) {
		*idxPtr = -1;
	}

	while ((this[i].type_str) && (!found)) {
		if (this[i].type_num == type) {
			found = 1;
			if (idxPtr) {
				*idxPtr = i;
			}
		} else {
			i++;
		}
	}

	return (found);
}
/******************************************************************************************/
