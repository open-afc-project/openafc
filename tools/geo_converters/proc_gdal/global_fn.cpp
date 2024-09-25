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
int fgetline(FILE *file, std::string& s, bool keepcr)
{
    int c, i;

    s.clear();
    for (i=0; (c=fgetc(file)) != EOF && c != '\n'; i++) {
        s += c;
    }
    if ( (i >= 1) && (s[i-1] == '\r') ) {
        s.erase(i-1,1);
        // i--;
    }
    if (c == '\n') {
        if (keepcr) {
            s += c;
        }
        i++;
    }
    return(i);
}
/******************************************************************************************/

/******************************************************************************************/
/**** Read a line into string s, return length.  From "C Programming Language" Pg. 29  ****/
/**** Modified to be able to read both DOS and UNIX files.                             ****/
/******************************************************************************************/
int fgetline(FILE *file, char *s)
{
    int c, i;

    for (i=0; (c=fgetc(file)) != EOF && c != '\n'; i++) {
        s[i] = c;
    }
    if ( (i >= 1) && (s[i-1] == '\r') ) {
        i--;
    }
    if (c == '\n') {
        s[i] = c;
        i++;
    }
    s[i] = '\0';
    return(i);
}
/******************************************************************************************/

/******************************************************************************************/
/**** Split string into vector of strings using specified delim.                       ****/
/******************************************************************************************/
std::vector<std::string>& split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}
/******************************************************************************************/

/******************************************************************************************/
/**** Split string into vector of strings for command line processing.                 ****/
/******************************************************************************************/
std::vector<std::string> splitOptions(const std::string &cmd)
{
    int i, fieldLength;
    int fieldStartIdx = -1;
    std::ostringstream s;
    std::vector<std::string> elems;
    std::string field;

    // state 0 = looking for next field
    // state 1 = found beginning of field, not "'", looking for end of field (space)
    // state 2 = found beginning of field, is "'", looking for end of field ("'").
    int state = 0;

    for(i=0; i<(int) cmd.length(); i++) {
        switch(state) {
            case 0:
                if (cmd.at(i) == ' ') {
                    // do nothing
                } else if (cmd.at(i) == '\'') {
                    fieldStartIdx = i+1;
                    state = 2;
                } else {
                    fieldStartIdx = i;
                    state = 1;
                }
                break;
            case 1:
                if (cmd.at(i) == ' ') {
                    fieldLength = i-fieldStartIdx;
                    field = cmd.substr(fieldStartIdx, fieldLength);
                    elems.push_back(field);
                    state = 0;
                }
                break;
            case 2:
                if (cmd.at(i) == '\'') {
                    fieldLength = i-fieldStartIdx;
                    field = cmd.substr(fieldStartIdx, fieldLength);
                    elems.push_back(field);
                    state = 0;
                }
                break;
            default:
                CORE_DUMP;
                break;
        }
        if (i == ((int) cmd.length())-1) {
            if (state == 1) {
                fieldLength = i-fieldStartIdx+1;
                field = cmd.substr(fieldStartIdx, fieldLength);
                elems.push_back(field);
                state = 0;
            } else if (state == 2) {
                s << "ERROR: Unable to splitOptions() for command \"" << cmd << "\" unmatched single quote.\n";
                throw std::runtime_error(s.str());
            }
        }
    }

    if (state != 0) {
        CORE_DUMP;
    }

    return elems;
}
/******************************************************************************************/

/******************************************************************************************/
/**** Split string into vector of strings for each CSV field properly treating fields  ****/
/**** enclosed in double quotes with embedded commas.  Also, double quotes can me      ****/
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

    for(i=0; i<(int) line.length(); i++) {
        if (skipChar) {
            skipChar = false;
        } else {
            switch(state) {
                case 0:
                    if (line.at(i) == '\"') {
                        fieldStartIdx = i+1;
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
                        fieldLength = i-fieldStartIdx;
                        field = line.substr(fieldStartIdx, fieldLength);

                        std::size_t start = field.find_first_not_of(" \n\t");
                        std::size_t end = field.find_last_not_of(" \n\t");
                        if (start == std::string::npos) {
                            field.clear();
                        } else {
                            field = field.substr(start, end-start+1);
                        }

                        elems.push_back(field);
                        state = 0;
                    }
                    break;
                case 2:
                    if (line.at(i) == '\"') {
                        if ( (i+1 < (int) line.length()) && (line.at(i+1) == '\"') ) {
                            skipChar = true;
                        } else {
                            fieldLength = i-fieldStartIdx;
                            field = line.substr(fieldStartIdx, fieldLength);
                            std::size_t k = field.find("\"\"");
                            while(k != std::string::npos) {
                                field.erase(k,1);
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
                        s << "ERROR: Unable to splitCSV() for command \"" << line << "\" invalid quotes.\n";
                        throw std::runtime_error(s.str());
                    }
                    break;
                default:
                    CORE_DUMP;
                    break;
            }
        }
        if (i == ((int) line.length())-1) {
            if (state == 0) {
                field.clear();
                elems.push_back(field);
            } else if (state == 1) {
                fieldLength = i-fieldStartIdx+1;
                field = line.substr(fieldStartIdx, fieldLength);

                std::size_t start = field.find_first_not_of(" \n\t");
                std::size_t end = field.find_last_not_of(" \n\t");
                if (start == std::string::npos) {
                    field.clear();
                } else {
                    field = field.substr(start, end-start+1);
                }

                elems.push_back(field);
                state = 0;
            } else if (state == 2) {
                s << "ERROR: Unable to splitCSV() for command \"" << line << "\" unmatched quote.\n";
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

/******************************************************************************************/

/******************************************************************************************/
/**** Get field from a std::string, equivalent to strtok.                              ****/
/******************************************************************************************/
std::string getField(const std::string& strVal, int& posn, const char *chdelim)
{
    int fstart, fstop;
    std::string field;

    fstart = strVal.find_first_not_of(chdelim, posn);
    if (fstart == (int) std::string::npos) {
        field.clear();
        fstop = fstart;
    } else {
        fstop  = strVal.find_first_of(chdelim, fstart);
        if (fstop == (int) std::string::npos) {
            field = strVal.substr(fstart);
            fstop = strVal.length();
        } else {
            field = strVal.substr(fstart, fstop-fstart);
        }
    }
    posn = fstop;

    return(field);
}
/******************************************************************************************/

/******************************************************************************************/
/**** Copy file src to file dest.  Return 1 if successful, 0 otherwise.                ****/
/******************************************************************************************/
int copyFile(const char *src, const char *dest)
{
    int c;
    FILE *fsrc, *fdest;

    if ( !(fsrc = fopen(src, "rb")) ) {
        printf("ERROR: Unable to read file \"%s\"\n", src);
        return(0);
    }

    if ( !(fdest = fopen(dest, "wb")) ) {
        printf("ERROR: Unable to write to file \"%s\"\n", dest);
        fclose(fsrc);
        return(0);
    }

    while((c=getc(fsrc))!=EOF) {
       putc(c, fdest);
    }

    fclose(fsrc);
    fclose(fdest);

    return(1);
}
/******************************************************************************************/
/**** Check if file or directory exists.                                               ****/
/**** Return 0 if doesn't exist, 1 if it is a file, 2 if it is a directory             ****/
/******************************************************************************************/
int fileExists(const char *filename)
{
    struct stat statInfo;
    int exists;
    int intStat;

    intStat = stat(filename, &statInfo);

    exists = (intStat ? 0 :
              (statInfo.st_mode & S_IFDIR) ? 2 : 1);

    return(exists);
}
/******************************************************************************************/

/******************************************************************************************/
/**** Remove double quotes that surround a string.  Note that spaces are removed from  ****/
/**** the beginning of the string before the first quote and both spaces and <cr> are  ****/
/**** removed from the end of the string after the final quote.  If a matching pair of ****/
/**** quotes is not found the string is left unchanged.  Versions for char* and        ****/
/**** std::string                                                                      ****/
/******************************************************************************************/
char *remove_quotes(char *str)
{
    int n;
    char *start, *end;
    char *ret_str = str;

    if (str) {
        start = str;
        while( (*start) == ' ' ) { start++; }
        n = strlen(str);
        end = str + n - 1;
        while( (end > start) && (((*end) == ' ') || ((*end) == '\n')) ) { end--; }
        if ( (end > start) && ((*start) == '\"') && ((*end) == '\"') ) {
            ret_str = start+1;
            (*end) = (char) 0;
        }
    }

    return(ret_str);
}

std::string remove_quotes(const std::string& str) 
{
    std::size_t start, end;
    std::string ret_str = str;

    if (!str.empty()) {
// printf("REMOVE QUOTES FROM ::%s::\n", str.c_str());
        start = str.find_first_not_of(' ');
        end = str.find_last_not_of(" \n");
        if (    (start != std::string::npos)
             && (end   != std::string::npos)
             && (end > start)
             && (str[start] == '\"')
             && (str[end] == '\"')
           ) {
            ret_str = str.substr(start+1, end-start-1);
        }
    }

    return(ret_str);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: escape_quotes                                                          ****/
/**** INPUTS: str                                                                      ****/
/**** OUTPUTS: possibly modified str                                                   ****/
/**** Insert backslash '\' in from of each double quote character '"' in str.          ****/
char *escape_quotes(char *&str)
{
    char *ptrA, *ptrB;

    int num = 0;
    ptrA = str;
    while(*ptrA) {
        if ((*ptrA) == '"') {
            num++;
        }
        ptrA++;
    }

    if (num) {
        char *tmpstr = CVECTOR(strlen(str) + num);
        ptrA = str;
        ptrB = tmpstr;

        while(*ptrA) {
            if ((*ptrA) == '"') {
                (*ptrB) = '\\';
                ptrB++;
            }
            *ptrB = *ptrA;
            ptrA++;
            ptrB++;
        }
        *ptrB = *ptrA;

        free(str);
        str = tmpstr;
    }

    return(str);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: getCSVField                                                            ****/
/**** INPUTS: str, rmWhiteSpaceFlag, fs                                                ****/
/**** OUTPUTS: startPtr, fieldLen                                                      ****/
/**** Return val is ptr to beginning of next field, or NULL if no more fields.  This   ****/
/**** is diffenent than strtok() in that multiple fs characters will be interpreted as ****/
/**** separate fields.  The number of fields is always the number of fs characters     ****/
/**** plus one.  Examples:                                                             ****/
/**** "A,B,C":  3 Fields => "A", "B", "C".                                             ****/
/**** ",,":     3 Fields => "", "", "".                                                ****/
/**** "":       1 Fields => ""                                                         ****/
/****                                                                                  ****/
/**** Iterate over fields like this:                                                   ****/
/****    chptr = str;                                                                  ****/
/****    do {                                                                          ****/
/****        chptr = getCSVField(chptr, startPtr, fieldLen);                           ****/
/****        fieldStr = strndup(startPtr, fieldLen);                                   ****/
/****        free(fieldStr);                                                           ****/
/****    } while(chptr);                                                               ****/
/******************************************************************************************/
const char *getCSVField(const char *str, const char *&startPtr, int& fieldLen, const bool rmWhiteSpaceFlag, const char fs)
{
    startPtr = str;

    const char *endPtr = startPtr;

    while((*endPtr)&&(*endPtr != fs)) { endPtr++; }

    fieldLen = endPtr-startPtr;

    if(rmWhiteSpaceFlag) {
        while((fieldLen)&&isspace(*startPtr))  { startPtr++; fieldLen--; }
        while((fieldLen)&&isspace(*(startPtr+fieldLen-1))) { fieldLen--; }
    }

    if (*endPtr == fs) {
        endPtr++;
    } else {
        endPtr = (char *) NULL;
    }

    return(endPtr);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: gcd                                                                    ****/
/**** INPUTS: a, b                                                                     ****/
/**** OUTPUTS: gcd(a,b)                                                                ****/
/**** Returns the greatest common divisor of two integers.                             ****/
int gcd(int a, int b)
{   int c;

    a = abs(a); b = abs(b);
    while (a) {
        c = b;
        b = a;
        a = c % a;
    }

    return(b);
}
/******************************************************************************************/
void extended_euclid(int a, int b, int& gcd, int& p1, int& p2)
{
    int next_gcd, next_p1, next_p2;
    int  new_gcd,  new_p1,  new_p2;

    if ( (a<0) || (b<0) ) {
        printf("ERROR in routine extended_euclid():");
        printf(" values must be positive %d, %d\n", a,b);
        CORE_DUMP;
    }

         gcd = a;      p1 = 1;      p2 = 0;
    next_gcd = b; next_p1 = 0; next_p2 = 1;

    while(next_gcd) {
        new_gcd = gcd % next_gcd;
        new_p1  = p1 - next_p1*(gcd/next_gcd);
        new_p2  = p2 - next_p2*(gcd/next_gcd);
        gcd = next_gcd;
        p1  = next_p1;
        p2  = next_p2;
        next_gcd = new_gcd;
        next_p1  = new_p1;
        next_p2  = new_p2;
    }

    return;
}
/******************************************************************************************/
/**** FUNCTION: set_current_dir_from_file                                              ****/
/**** INPUTS: filename                                                                 ****/
/**** OUTPUTS:                                                                         ****/
/**** Set current working directory to that of the specified filename.                 ****/
void set_current_dir_from_file(char *filename)
{
    char *path = strdup(filename);
    int n = strlen(filename);
    int found = 0;
    int i;

    for (i=n-1; (i>=0)&&(!found); i--) {
        if ( (path[i] == '\\') || (path[i] == '/') ) {
            found = 1;
            path[i+1] = (char) 0;
        }
    }

    if (found) {
        if (chdir(path) != 0) {
#if CDEBUG
            CORE_DUMP;
#endif
        }
    }

    free(path);
}
/******************************************************************************************/
/**** FUNCTION: insertFilePfx                                                          ****/
/**** INPUTS: pfx, filename                                                            ****/
/**** OUTPUTS:                                                                         ****/
/**** For filename = "d0/d1/d2/f.txt", output is "d0/d1/d2/pfx_f.txt".                 ****/
char *insertFilePfx(const char *pfx, const char *filename)
{
    int n = strlen(filename);
    bool found = false;
    int posn = 0;
    int i;

    for (i=n-1; (i>=0)&&(!found); i--) {
        if ( (filename[i] == '\\') || (filename[i] == '/') ) {
            found = true;
            if (i==n-1) {
#if CDEBUG
                CORE_DUMP;
#endif
            } else {
                posn = i+1;
            }
        }
    }

    char *pfxFilename = (char *) malloc(strlen(filename) + strlen(pfx) + 1);

    int k = 0;
    for(i=0; i<=posn-1; i++) {
        pfxFilename[k] = filename[i];
        k++;
    }
    for(i=0; i<=((int) strlen(pfx))-1; i++) {
        pfxFilename[k] = pfx[i];
        k++;
    }
    for(i=posn; i<=n-1; i++) {
        pfxFilename[k] = filename[i];
        k++;
    }
    pfxFilename[k] = '\0';

    return(pfxFilename);
}
/******************************************************************************************/
/**** FUNCTION: insertFilePfx                                                          ****/
/**** INPUTS: pfx, filename                                                            ****/
/**** OUTPUTS:                                                                         ****/
/**** For filename = "d0/d1/d2/f.txt", output is "d0/d1/d2/pfx_f.txt".                 ****/
std::string insertFilePfx(const std::string& pfx, const std::string& filename)
{
    std::string pfxFilename;
    size_t posn = filename.find_last_of("/\\");

    if (posn == std::string::npos) {
        pfxFilename = pfx + filename;
    } else {
        posn++;
        pfxFilename = filename.substr(0, posn) + pfx + filename.substr(posn);
    }

    return(pfxFilename);
}
/******************************************************************************************/
int stringcmp(const char *s1, const char *s2)
{
    int n1 = strlen(s1);
    int n2 = strlen(s2);

    int done = 0;
    int d1, d2;
    int nd1=0, nd2=0;
    int v1, v2, num_eq;
    int retval = 0;
    char c1, c2;

    if ( (n1 == 0) && (n2 == 0) ) {
        return(0);
    } else if (n1 == 0) {
        return(-1);
    } else if (n2 == 0) {
        return(1);
    }

    int i1 = 0;
    int i2 = 0;

    while(!done) {
        c1 = s1[i1];
        d1 = ((c1 >= '0') && (c1 <= '9')) ? 1 : 0;
        if (d1) {
            nd1 = 1;
            while ((i1+nd1<n1) && (s1[i1+nd1] >= '0') && (s1[i1+nd1] <= '9')) {
                nd1++;
            }
        }

        c2 = s2[i2];
        d2 = ((c2 >= '0') && (c2 <= '9')) ? 1 : 0;
        if (d2) {
            nd2 = 1;
            while ((i2+nd2<n2) && (s2[i2+nd2] >= '0') && (s2[i2+nd2] <= '9')) {
                nd2++;
            }
        }

        if ((!d1) && (!d2)) {
            if (c1 < c2) {
                retval = -1;
                done = 1;
            } else if (c1 > c2) {
                retval = 1;
                done = 1;
            } else if ((i1 == n1-1) && (i2 < n2-1)) {
                retval = -1;
                done = 1;
            } else if ((i2 == n2-1) && (i1 < n1-1)) {
                retval = 1;
                done = 1;
            } else if ((i1 == n1-1) && (i2 == n2-1)) {
                retval = 0;
                done = 1;
            } else {
                i1++;
                i2++;
            }
        } else if ((!d1) && (d2)) {
            retval = 1;
            done = 1;
        } else if ((d1) && (!d2)) {
            retval = -1;
            done = 1;
        } else if ((d1) && (d2)) {
            while ((nd1 > nd2)&&(!done)) {
                if (s1[i1] > '0') {
                    retval = 1;
                    done = 1;
                } else {
                    i1++;
                    nd1--;
                }
            }
            while ((nd2 > nd1)&&(!done)) {
                if (s2[i2] > '0') {
                    retval = -1;
                    done = 1;
                } else {
                    i2++;
                    nd2--;
                }
            }
            num_eq = 0;
            while( (!done) && (!num_eq) ) {
                if (nd1==0) {
                    if ((i1 == n1) && (i2 < n2)) {
                        retval = -1;
                        done = 1;
                    } else if ((i2 == n2) && (i1 < n1)) {
                        retval = 1;
                        done = 1;
                    } else if ((i1 == n1) && (i2 == n2)) {
                        retval = 0;
                        done = 1;
                    } else {
                        num_eq = 1;
                    }
                } else {
                    v1 = s1[i1] - '0';
                    v2 = s2[i2] - '0';
                    if (v1 < v2) {
                        retval = -1;
                        done = 1;
                    } else if (v1 > v2) {
                        retval = 1;
                        done = 1;
                    } else {
                        i1++;
                        i2++;
                        nd1--;
                        nd2--;
                    }
                }
            }
        }
    }

    // printf("S1 = \"%s\"   S2 = \"%s\"   RETVAL = %d\n", s1.latin1(), s2.latin1(), retval);

    return(retval);
}
/******************************************************************************************/
/**** FUNCTION: lowercase                                                              ****/
/**** Convert string to lowercase.                                                     ****/
/******************************************************************************************/
void lowercase(char *s1)
{
    char *chptr = s1;

    while(*chptr) {
        if ( ((*chptr) >= 'A') && ((*chptr) <= 'Z') ) {
            *chptr += 'a' - 'A';
        }
        chptr++;
    }
}
void lowercase(std::string& s1)
{
    int i;
    for(i=0; i<=(int) s1.length()-1; i++) {
        s1[i] = std::tolower(s1[i]);
    }
    return;
}
/******************************************************************************************/
/**** FUNCTION: uppercase                                                              ****/
/**** Convert string to uppercase.                                                     ****/
/******************************************************************************************/
void uppercase(std::string& s1)
{
    int i;
    for(i=0; i<=(int) s1.length()-1; i++) {
        s1[i] = std::toupper(s1[i]);
    }
    return;
}
/******************************************************************************************/
void get_bits(char *str, int n, int num_bits, int insNull)
{
    int i, bit;

    for (i=num_bits-1; i>=0; i--)
    {   bit = (n >> i)&1;
        str[(num_bits-1) - i] = (bit ? '1' : '0');
    }

    if (insNull) {
        str[num_bits] = '\0';
    }
}
/******************************************************************************************/
void get_bits(char *str, LONGLONG_TYPE n, int num_bits, int insNull)
{
    int i, bit;

    for (i=num_bits-1; i>=0; i--) {
        bit = (int) ((n >> i)&1);
        str[(num_bits-1) - i] = (bit ? '1' : '0');
    }

    if (insNull) {
        str[num_bits] = '\0';
    }
}
/******************************************************************************************/
void get_hex(char *str, int n, int num_hex, int insNull)
{
    int i, hex;

    for (i=num_hex-1; i>=0; i--)
    {   hex = (n >> 4*i)&0x0F;
        str[(num_hex-1) - i] = (hex<=9 ? '0'+hex : 'A' + hex - 10);
    }

    if (insNull) {
        str[num_hex] = '\0';
    }
}
/******************************************************************************************/
int is_big_endian()
{
    int big_endian;
    int n = 0x89abcdef;
    unsigned char *ch_ptr;

    ch_ptr = (unsigned char *) &n;

    if (    (ch_ptr[0] == 0x89)
         && (ch_ptr[1] == 0xab)
         && (ch_ptr[2] == 0xcd)
         && (ch_ptr[3] == 0xef) ) {
        big_endian = 1;
    } else if (
            (ch_ptr[3] == 0x89)
         && (ch_ptr[2] == 0xab)
         && (ch_ptr[1] == 0xcd)
         && (ch_ptr[0] == 0xef) ) {
        big_endian = 0;
    } else {
        big_endian = -1;
    }

    return(big_endian);
}
/******************************************************************************************/
#ifdef __linux__
bool deleteFile(const char *filename, bool)
{
    char *cmd = (char *) malloc(strlen(filename) + 20);
    sprintf(cmd, "rm -fr \"%s\"", filename);
    int ret = system(cmd);
    free(cmd);

    return(ret==-1 ? false : true);
}
#else
bool deleteFile(const char *filename, bool noRecycleBin)
{
    int i;
    int len = strlen(filename);
    TCHAR *pszFrom = new TCHAR[len+2];
    if (sizeof(TCHAR)==1) {
        _tcscpy((char *) pszFrom, filename);
    } else {
        MultiByteToWideChar(
            CP_ACP,       // code page
            NULL,         // character-type options
            filename,       // string to map
            -1,           // number of bytes in string
            (wchar_t *) pszFrom, // wide-character buffer
            len+2         // size of buffer
        );
    }

    /**************************************************************************************/
    /**** Replace '/' with '\', SHFileOperation does NOT work with '/' (BUG ??)        ****/
    /**************************************************************************************/
    for(i=0; i<=len-1; i++) {
        if (pszFrom[i] == '/') {
            pszFrom[i] = '\\';
        }
    }
    /**************************************************************************************/

    pszFrom[len] = 0;
    pszFrom[len+1] = 0;

    SHFILEOPSTRUCT fileop;
    fileop.hwnd   = NULL;    // no status display
    fileop.wFunc  = FO_DELETE;  // delete operation
    fileop.pFrom  = pszFrom;  // source file name as double null terminated string
    fileop.pTo    = NULL;    // no destination needed
    fileop.fFlags = FOF_NOCONFIRMATION|FOF_SILENT;  // do not prompt the user

    if(!noRecycleBin) {
        fileop.fFlags |= FOF_ALLOWUNDO;
    }

    fileop.fAnyOperationsAborted = FALSE;
    fileop.lpszProgressTitle     = NULL;
    fileop.hNameMappings         = NULL;

    int ret = SHFileOperation(&fileop);
    delete [] pszFrom;
    return (ret == 0);
}
#endif
/******************************************************************************************/
#ifdef __linux__
bool isFirstInstance(const char *)
{
    return(true);
}
#else
bool isFirstInstance(const char *mutexName)
{
    bool retval;
    int i;
    int len = strlen(mutexName);
    TCHAR *mutexNameTCHAR = new TCHAR[len+2];
    if (sizeof(TCHAR)==1) {
        _tcscpy((char *) mutexNameTCHAR, mutexName);
    } else {
        MultiByteToWideChar(
            CP_ACP,                     // code page
            NULL,                       // character-type options
            mutexName,                  // string to map
            -1,                         // number of bytes in string
            (wchar_t *) mutexNameTCHAR, // wide-character buffer
            len+2                       // size of buffer
        );
    }
    mutexNameTCHAR[len] = 0;
    mutexNameTCHAR[len+1] = 0;

    HANDLE hMutex = CreateMutex(NULL, TRUE, mutexNameTCHAR);

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance of the app exists
        retval = false;
    } else {
        // This is the first instance
        retval = true;
    }

    delete [] mutexNameTCHAR;

    return retval;
}
#endif
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: readTwoCol                                                             ****/
/**** INPUTS: file "flname"                                                            ****/
/**** OUTPUTS: n, x[0...n-1], y[0...n-1]                                               ****/
/**** Reads two column tabular data from the file "flname".  The number of lines of    ****/
/**** data in the file is n.  This first column is read into the variables x[i] and    ****/
/**** the second column is read into the variables y[i] (i=0,1,...,n-1).  Lines        ****/
/**** beginning with the character '#' are ignored (to allow for comments).  The       ****/
/**** parameters maxLineSize, and maxPts specify the maximum number of characters per  ****/
/**** line and the maximum number of points in the file respectively.                  ****/
/**** Return value: 1 if successfull, 0 if not successful                              ****/
/******************************************************************************************/
int readTwoCol(double *x, double *y, int& numPts, const char *flname, int maxNumPts, int maxLineSize)
{
    char *line, *lnptr, *chptr, *errmsg;
    FILE *fp;

    line   = CVECTOR(maxLineSize);
    errmsg = CVECTOR(maxLineSize);

#define TMP_NEDELIM (lnptr[0] != ',')&&(lnptr[0] != ' ')&&(lnptr[0] != '\t')
#define TMP_EQDELIM (lnptr[0] == ',')||(lnptr[0] == ' ')||(lnptr[0] == '\t')

    if ( strcmp(flname,"stdin") == 0 ) {
        fp = stdin;
    } else if ( !(fp = fopen(flname, "rb")) ) {
        sprintf(errmsg, "ERROR: Unable to read from file %s\n", flname);
        printf("%s", errmsg);
        return(0);
    }

    numPts = 0;

    while ( fgetline(fp, line) ) {
        lnptr = line;
        while ( (lnptr[0] == ' ') || (lnptr[0] == '\t') ) lnptr++;
        if ( (lnptr[0] != '#') && (lnptr[0] != '\n') ) {
            if ((numPts) >= maxNumPts) {
                chptr = errmsg;
                chptr += sprintf(chptr, "ERROR in routine readTwoCol()\n");
                chptr += sprintf(chptr, "Number of points in file %s exceeds maxNumPts = %d\n", flname, maxNumPts);
                printf("%s", errmsg);
                return(0);
            }

            x[numPts] = atof(lnptr);
            while ( TMP_NEDELIM ) lnptr++;
            while ( TMP_EQDELIM ) lnptr++;
            y[numPts] = atof(lnptr);

            numPts++;
        }
    }
    if (fp != stdin) {
        fclose(fp);
    }

    free(line);
    free(errmsg);
#undef TMP_NEDELIM
#undef TMP_EQDELIM

    return(1);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: readThreeCol                                                           ****/
/**** INPUTS: file "flname"                                                            ****/
/**** OUTPUTS: n, x[0...n-1], y[0...n-1], z[0...n-1]                                   ****/
/**** Reads three column tabular data from the file "flname".  The number of lines of  ****/
/**** data in the file is n.  This first column is read into the variables x[i],       ****/
/**** the second column is read into the variables y[i], and the third column is read  ****/
/**** into the variables z[i] (i=0,1,...,n-1).  Lines                                  ****/
/**** beginning with the character '#' are ignored (to allow for comments).  The       ****/
/**** parameters maxLineSize, and maxPts specify the maximum number of characters per  ****/
/**** line and the maximum number of points in the file respectively.                  ****/
/**** Return value: 1 if successfull, 0 if not successful                              ****/
/******************************************************************************************/
int readThreeCol(double *x, double *y, double *z, int& numPts, const char *flname, int maxNumPts, int maxLineSize)
{
    char *line, *lnptr, *chptr, *errmsg;
    FILE *fp;

    line   = CVECTOR(maxLineSize);
    errmsg = CVECTOR(maxLineSize);

#define TMP_NEDELIM (lnptr[0] != ',')&&(lnptr[0] != ' ')&&(lnptr[0] != '\t')
#define TMP_EQDELIM (lnptr[0] == ',')||(lnptr[0] == ' ')||(lnptr[0] == '\t')

    if ( strcmp(flname,"stdin") == 0 ) {
        fp = stdin;
    } else if ( !(fp = fopen(flname, "rb")) ) {
        sprintf(errmsg, "ERROR: Unable to read from file %s\n", flname);
        printf("%s", errmsg);
        return(0);
    }

    numPts = 0;

    while ( fgetline(fp, line) ) {
        lnptr = line;
        while ( (lnptr[0] == ' ') || (lnptr[0] == '\t') ) lnptr++;
        if ( (lnptr[0] != '#') && (lnptr[0] != '\n') ) {
            if ((numPts) >= maxNumPts) {
                chptr = errmsg;
                chptr += sprintf(chptr, "ERROR in routine readTwoCol()\n");
                chptr += sprintf(chptr, "Number of points in file %s exceeds maxNumPts = %d\n", flname, maxNumPts);
                printf("%s", errmsg);
                return(0);
            }

            x[numPts] = atof(lnptr);
            while ( TMP_NEDELIM ) lnptr++;
            while ( TMP_EQDELIM ) lnptr++;
            y[numPts] = atof(lnptr);
            while ( TMP_NEDELIM ) lnptr++;
            while ( TMP_EQDELIM ) lnptr++;
            z[numPts] = atof(lnptr);

            numPts++;
        }
    }
    if (fp != stdin) {
        fclose(fp);
    }

    free(line);
    free(errmsg);
#undef TMP_NEDELIM
#undef TMP_EQDELIM

    return(1);
}
/******************************************************************************************/

/******************************************************************************************/
/***  FUNCTION: writeTwoCol                                                             ***/
/***  INPUTS: x[], y[], n, file "flname"                                                ***/
/***  OUTPUTS: n, x[0...n-1], y[0...n-1]                                                ***/
/***  Writes two column tabular data to the file "flname".  The number of lines of      ***/
/***  data in the file is n.  This first column is written from the variables x[i] and  ***/
/***  the second column is written from the variables y[i] (i=0,1,...,n-1).  The double ***/
/***  values of x[i] and y[i] are written with the format of the string fmt.  Thus to   ***/
/***  write each set of values on a separate line with a %12.10f format and tab         ***/
/***  delimited, fmt should be "%12.10f\t%12.10f\n".                                    ***/
void writeTwoCol(const double *x, const double *y, int n, const char *fmt, const char *flname)
{
    FILE *fp;
    int i;
    std::ostringstream ss;

    if ( !(fp = fopen(flname, "w")) ) {
        ss << "Error writing to file: \"" << flname << "\"";
        throw std::runtime_error(ss.str());
    }

    for (i=0; i<=n-1; i++) {
        fprintf(fp, fmt, x[i], y[i]);
    }
    fprintf(fp, "\n");
    fclose(fp);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: writeOneCol                                                            ****/
/**** INPUTS: x[], n, file "flname"                                                    ****/
/**** OUTPUTS: n, x[0...n-1]                                                           ****/
/**** Writes one column tabular data to the file "flname".  The number of lines of     ****/
/**** data in the file is n.  This single column data values are written from the      ****/
/**** variables x[i] (i=0,1,...,n-1).  The double values of x[i] are written with the  ****/
/**** format of the string fmt.                                                        ****/
/******************************************************************************************/
void writeOneCol(const double *x, int n, const char *fmt, const char *flname)
{
    FILE *fp;
    int i;
    std::ostringstream ss;

    if (flname) {
        if ( !(fp = fopen(flname, "w")) ) {
            ss << "Error writing to file: \"" << flname << "\"";
            throw std::runtime_error(ss.str());
        }
    } else {
        fp = stdout;
    }

    for (i=0; i<=n-1; i++) {
        fprintf(fp, fmt, x[i]);
    }
    fprintf(fp, "\n");

    if (flname) {
        fclose(fp);
    }
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: cvtStrToVal (std::complex<double>)                                     ****/
/******************************************************************************************/
int cvtStrToVal(char const* strptr, std::complex<double>& val)
{
    const char *chptr = strptr;
    char *nptr;
    int i;
    double rval, ival;

    for(i=0; i<=1; i++) {
        switch(i) {
            case 0: rval = strtod(chptr, &nptr); break;
            case 1: ival = strtod(chptr, &nptr); break;
        }
        if (nptr == chptr) {
            std::stringstream errorStr;
            errorStr << "ERROR in cvtStrToVal() : Unable to cvt to std::complex<double> \"" << strptr << "\"";
            throw std::runtime_error(errorStr.str());
            return(0);

        }
        chptr = nptr;
    }
    val = std::complex<double>(rval, ival);

    return(chptr-strptr);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: cvtStrToVal (double)                                                   ****/
/******************************************************************************************/
int cvtStrToVal(char const* strptr, double& val)
{
    const char *chptr = strptr;
    char *nptr;

    val = strtod(chptr, &nptr);
    if (nptr == chptr) {
        std::stringstream errorStr;
        errorStr << "ERROR in cvtStrToVal() : Unable to cvt to double \"" << strptr << "\"";
        throw std::runtime_error(errorStr.str());
        return(0);

    }

    return(chptr-strptr);
}
/******************************************************************************************/

