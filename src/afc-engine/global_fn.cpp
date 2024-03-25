#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <complex>
#include <ctype.h>
#include <sys/stat.h>
#include <stdexcept>
#include <sstream>

#ifdef __linux__
	#include <unistd.h>
#else
	#include <direct.h>
	#include <windows.h>
	#include <tchar.h>
	#include <shellapi.h>
#endif

#include "global_fn.h"

/******************************************************************************************/
/**** Read a line into string s, return length.  From "C Programming Language" Pg. 29  ****/
/**** Modified to be able to read both DOS and UNIX files.                             ****/
/**** 2013.03.11: use std::string so not necessary to pre-allocate storage.            ****/
/**** Return value is number of characters read from FILE, which may or may not equal  ****/
/**** the length of string s depending on whether '\r' or '\n' has been removed from   ****/
/**** the string.                                                                      ****/
/******************************************************************************************/
int fgetline(FILE *file, std::string &s, bool keepcr)
{
	int c, i;

	s.clear();
	for (i = 0; (c = fgetc(file)) != EOF && c != '\n'; i++) {
		s += c;
	}
	if ((i >= 1) && (s[i - 1] == '\r')) {
		s.erase(i - 1, 1);
		// i--;
	}
	if (c == '\n') {
		if (keepcr) {
			s += c;
		}
		i++;
	}
	return (i);
}
/******************************************************************************************/

/******************************************************************************************/
/**** Read a line into string s, return length.  From "C Programming Language" Pg. 29  ****/
/**** Modified to be able to read both DOS and UNIX files.                             ****/
/******************************************************************************************/
int fgetline(FILE *file, char *s)
{
	int c, i;

	for (i = 0; (c = fgetc(file)) != EOF && c != '\n'; i++) {
		s[i] = c;
	}
	if ((i >= 1) && (s[i - 1] == '\r')) {
		i--;
	}
	if (c == '\n') {
		s[i] = c;
		i++;
	}
	s[i] = '\0';
	return (i);
}
/******************************************************************************************/

/******************************************************************************************/
/**** Split string into vector of strings using specified delim.                       ****/
/******************************************************************************************/
std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems)
{
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}

std::vector<std::string> split(const std::string &s, char delim)
{
	std::vector<std::string> elems;
	split(s, delim, elems);
	return elems;
}
/******************************************************************************************/

/******************************************************************************************/
/**** Split string into vector of strings for each CSV field properly treating fields  ****/
/**** enclosed in double quotes with embedded commas.  Also, double quotes can be      ****/
/**** embedded in a field using 2 double quotes "".                                    ****/
/**** This format is compatible with excel and libreoffice CSV files.                  ****/
/******************************************************************************************/
std::vector<std::string> splitCSV(const std::string &line)
{
	int i, fieldLength;
	int fieldStartIdx = -1;
	std::ostringstream s;
	std::vector<std::string> elems;
	std::string field;
	bool skipChar = false;

	// state 0 = looking for next field
	// state 1 = found beginning of field, not ", looking for end of field (comma)
	// state 2 = found beginning of field, is ", looking for end of field (").
	// state 3 = found end of field, is ", pass over 0 or more spaces until (comma)
	int state = 0;

	for (i = 0; i < (int)line.length(); i++) {
		if (skipChar) {
			skipChar = false;
		} else {
			switch (state) {
				case 0:
					if (line.at(i) == '\"') {
						fieldStartIdx = i + 1;
						state = 2;
					} else if (line.at(i) == ',') {
						field.clear();
						elems.push_back(field);
					} else if (line.at(i) == ' ') {
						// do nothing
					} else {
						fieldStartIdx = i;
						state = 1;
					}
					break;
				case 1:
					if (line.at(i) == ',') {
						fieldLength = i - fieldStartIdx;
						field = line.substr(fieldStartIdx, fieldLength);

						std::size_t start = field.find_first_not_of(" \n"
											    "\t");
						std::size_t end = field.find_last_not_of(" \n\t");
						if (start == std::string::npos) {
							field.clear();
						} else {
							field = field.substr(start,
									     end - start + 1);
						}

						elems.push_back(field);
						state = 0;
					}
					break;
				case 2:
					if (line.at(i) == '\"') {
						if ((i + 1 < (int)line.length()) &&
						    (line.at(i + 1) == '\"')) {
							skipChar = true;
						} else {
							fieldLength = i - fieldStartIdx;
							field = line.substr(fieldStartIdx,
									    fieldLength);
							std::size_t k = field.find("\"\"");
							while (k != std::string::npos) {
								field.erase(k, 1);
								k = field.find("\"\"");
							}
							elems.push_back(field);
							state = 3;
						}
					}
					break;
				case 3:
					if (line.at(i) == ' ') {
						// do nothing
					} else if (line.at(i) == ',') {
						state = 0;
					} else {
						s << "ERROR: Unable to splitCSV() for command \""
						  << line << "\" invalid quotes.\n";
						throw std::runtime_error(s.str());
					}
					break;
				default:
					CORE_DUMP;
					break;
			}
		}
		if (i == ((int)line.length()) - 1) {
			if (state == 0) {
				field.clear();
				elems.push_back(field);
			} else if (state == 1) {
				fieldLength = i - fieldStartIdx + 1;
				field = line.substr(fieldStartIdx, fieldLength);

				std::size_t start = field.find_first_not_of(" \n\t");
				std::size_t end = field.find_last_not_of(" \n\t");
				if (start == std::string::npos) {
					field.clear();
				} else {
					field = field.substr(start, end - start + 1);
				}

				elems.push_back(field);
				state = 0;
			} else if (state == 2) {
				s << "ERROR: Unable to splitCSV() for command \"" << line
				  << "\" unmatched quote.\n";
				throw std::runtime_error(s.str());
			} else if (state == 3) {
				state = 0;
			}
		}
	}

	if (state != 0) {
		CORE_DUMP;
	}

	return elems;
}
/******************************************************************************************/
