/******************************************************************************************/
/**** FILE: antenna.cpp                                                                ****/
/**** Michael Mandell 9/16/03                                                          ****/
/******************************************************************************************/

#include <iostream>
#include <sstream>
#include <math.h>
#include <cstring>
#include <time.h>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>

#include "cconst.h"
#include "antenna.h"
#include "spline.h"
#include "global_fn.h"
#include "list.h"
#include "lininterp.h"
#include "rkflogging/Logging.h"
#include "rkflogging/ErrStream.h"

#define PRMSG(fff, xxx) fprintf(fff, "%s", xxx)

// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "AntennaClass")

/******************************************************************************************/
/**** FUNCTION: AntennaClass::AntennaClass                                             ****/
/******************************************************************************************/
AntennaClass::AntennaClass(int p_type, const char *p_strid)
{
    if (p_strid) {
        strid = strdup(p_strid);
    } else {
        strid = (char *) NULL;
    }
    type = p_type;

    filename = (char *) NULL;
    horizGainTable = (LinInterpClass *) NULL;
    vertGainTable  = (LinInterpClass *) NULL;
    is_omni = (type == CConst::antennaOmni ? 1 : 0);
    h_width = 360.0;
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AntennaClass::~AntennaClass                                            ****/
/******************************************************************************************/
AntennaClass::~AntennaClass()
{
    if (strid) {
        free(strid);
    }
    if (filename) {
        free(filename);
    }
    if (horizGainTable) {
        delete horizGainTable;
    }
    if (vertGainTable) {
        delete vertGainTable;
    }
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: AntennaClass:: "get_" functions                                        ****/
/******************************************************************************************/
char *AntennaClass::get_strid()    { return(strid);    }
int   AntennaClass::get_type()     { return(type);     }
int   AntennaClass::get_is_omni()  { return(is_omni);  }
char *AntennaClass::get_filename() { return(filename); }

void AntennaClass::setBoresightGainTable(LinInterpClass *offBoresightGainTableVal) { offBoresightGainTable = offBoresightGainTableVal; }
/******************************************************************************************/
/**** FUNCTION: AntennaClass::readFile                                                 ****/
/******************************************************************************************/
int AntennaClass::readFile(const char *filepath, const char *p_filename)
{
    int i;
    double x_start, x_stop, u, xval, phase0, phase, lossDB;
    char *full_path_filename;
    FILE *fp;
    DblDblClass pt;

    filename = strdup(p_filename);

    full_path_filename = (char *) malloc( (strlen(filepath) + strlen(filename) + 1)*sizeof(char) );
    sprintf(full_path_filename, "%s%s", filepath, filename);

    if ( !(fp = fopen(full_path_filename, "rb")) ) {
        throw std::runtime_error(ErrStream() << "ERROR: cannot open antenna file %s" << full_path_filename);
    }

    LOGGER_INFO(logger) << "Reading antenna file: \"" << full_path_filename << "\"";

    free(full_path_filename);

    enum state_enum {
        STATE_HEADER,
        STATE_HORIZONTAL,
        STATE_VERTICAL,
        STATE_DONE
    };

    int state = STATE_HEADER;
    int linenum = 0;
    int num_h = -1;
    int num_v = -1;
    int idx   = -1;
    double gain_db = 0.0;
    double tilt_deg;

    ListClass<DblDblClass> *horizGain = (ListClass<DblDblClass> *) NULL;
    ListClass<DblDblClass> *vertGain = (ListClass<DblDblClass> *) NULL;

    // double *f_phs_h  = (double *) NULL;
    // double *f_gain_h = (double *) NULL;
    // double *f_phs_v  = (double *) NULL;
    // double *f_gain_v = (double *) NULL;

    char *line = (char *) malloc(1500*sizeof(char));
    char *str1, *str2;

    while ( fgetline(fp, line) ) {
#if 0
        printf("%s", line);
#endif
        linenum++;
        str1 = strtok(line, CHDELIM);
        if ( str1 && (str1[0] != '#') ) {
            str2 = strtok(NULL, CHDELIM);
            switch(state) {
                case STATE_HEADER:
                    if (strcmp(str1, "NAME") == 0) {
                        if (strid) {
                            free(strid);
                        }
                        strid = strdup(str2);
                    } else if (strcmp(str1, "FREQUENCY") == 0) {
                    } else if (strcmp(str1, "H_WIDTH") == 0) {
                        h_width = atof(str2);
                        is_omni = ( (h_width == 360.0) ? 1 : 0 );
                    } else if (strcmp(str1, "V_WIDTH") == 0) {
                    } else if (strcmp(str1, "FRONT_TO_BACK") == 0) {
                    } else if (strcmp(str1, "GAIN") == 0) {
                        int n = strlen(str2);
                        if ((n>3) && strcmp(str2+n-3, "dBi") == 0) {
                            str2[n-3] = (char) NULL;
                            gain_db = atof(str2);
                        } else {
                            throw std::runtime_error(ErrStream() << "ERROR: invalid antenna file \"" << filename << "(" << linenum << ")\" Gain: \"" << str2 << "\" must be in dBi");
                        }
                    } else if (strcmp(str1, "TILT") == 0) {
                        tilt_deg = -fabs(atof(str2));  /* Force tilt to be interpreted as pointing down */
                        tilt_rad = tilt_deg * M_PI / 180.0;
                        while(tilt_rad >= M_PI/2) { tilt_rad -= 2*M_PI; }
                        while(tilt_rad < -M_PI/2) { tilt_rad += 2*M_PI; }
                        if ( (tilt_rad < -M_PI/2) || (tilt_rad > M_PI/2) ) {
                            throw std::runtime_error(ErrStream() << "ERROR: invalid antenna file \"" << filename << "(" << linenum << ")\" TILT: \"" << str2 << "\" must be between +/- 90 degrees");
                        }
                    } else if (strcmp(str1, "POLARIZATION") == 0) {
                    } else if (strcmp(str1, "HORIZONTAL") == 0) {
                        num_h = atoi(str2);
                        horizGain = new ListClass<DblDblClass>(0);
                        idx = 0;
                        state = STATE_HORIZONTAL;
                    } else {
                        throw std::runtime_error(ErrStream() << "ERROR: invalid antenna file \"" << filename << "(" << linenum << ")\" Unrecognized keyword in header: \"" << str1 << "\"");
                    }
                    break;
                case STATE_HORIZONTAL:
                    if (idx <= num_h-1) {
                        horizGain->append(DblDblClass(atof(str1)*M_PI/180.0, gain_db - atof(str2)));
                        idx++;
                    } else if (strcmp(str1, "VERTICAL") == 0) {
                        num_v = atoi(str2);
                        vertGain = new ListClass<DblDblClass>(0);
                        idx = 0;
                        state = STATE_VERTICAL;
                    } else {
                        throw std::runtime_error(ErrStream() << "ERROR: invalid antenna file \"" << filename << "(" << linenum << ")\" Unrecognized keyword in header: \"" << str1 << "\"");
                    }
                    break;
                case STATE_VERTICAL:
                    if (idx <= num_v-1) {
                        phase = atof(str1)*M_PI/180.0;
                        lossDB = atof(str2);
                        vertGain->append(DblDblClass(phase, gain_db - lossDB));
                        if (idx == 0) {
                            vg0 = -lossDB;
                        }
                        idx++;
                    } else {
                        throw std::runtime_error(ErrStream() << "ERROR reading Antenna File: idx = " << idx << " num_v = " << num_v << " INVALID values");
                    }
                    if (idx == num_v) {
                        state = STATE_DONE;
                    }
                    break;
                case STATE_DONE:
                    throw std::runtime_error(ErrStream() << "ERROR: invalid antenna file \"" << filename << "(" << linenum << ")\" False additional data encountered");
                    break;
                default:
                    throw std::runtime_error(ErrStream() << "ERROR: invalid antenna file \"" << filename << "(" << linenum << ")\" Invalid state (" << state << ") encountered");
                    break;
            }
        }
    }

    if (state != STATE_DONE) {
        throw std::runtime_error(ErrStream() << "ERROR: invalid antenna file \"" << filename << "\", premature EOF encountered");
    }

    if ( (type == CConst::antennaLUT) || (type == CConst::antennaLUT_H) ) {
        SplineClass *spline = new SplineClass(horizGain);

        ListClass<DblDblClass> *horizSampledData = new ListClass<DblDblClass>(0);

        x_start = -M_PI;
        x_stop  =  M_PI;

        phase0 = (*horizGain)[0].x();
        for (i=0; i<=CConst::antenna_num_interp_pts-1; i++) {
            u = (double) i/(CConst::antenna_num_interp_pts-1);
            xval = x_start*(1.0-u) + x_stop*u;
            pt.setX(xval);
            while(xval >= phase0+2*M_PI) { xval -= 2*M_PI; }
            while(xval < phase0)       { xval += 2*M_PI; }
            pt.setY(spline->splineval(xval));
            horizSampledData->append(pt);
        }

        horizGainTable = new LinInterpClass(horizSampledData);

        delete spline;
        delete horizSampledData;
    } else {
        horizGainTable = (LinInterpClass *) NULL;
    }

    if ( (type == CConst::antennaLUT) || (type == CConst::antennaLUT_V) ) {
        SplineClass *spline = new SplineClass(vertGain);

        ListClass<DblDblClass> *vertSampledData = new ListClass<DblDblClass>(0);

        x_start = -M_PI;
        x_stop  =  M_PI;

        phase0 = (*vertGain)[0].x();
        for (i=0; i<=CConst::antenna_num_interp_pts-1; i++) {
            u = (double) i/(CConst::antenna_num_interp_pts-1);
            xval = x_start*(1.0-u) + x_stop*u;
            pt.setX(xval);
            while(xval >= phase0+2*M_PI) { xval -= 2*M_PI; }
            while(xval < phase0)       { xval += 2*M_PI; }
            pt.setY(spline->splineval(xval));
            vertSampledData->append(pt);
        }

        vertGainTable = new LinInterpClass(vertSampledData);

        delete spline;
        delete vertSampledData;

        double pi_minus_tilt = M_PI - tilt_rad;
        while(pi_minus_tilt >= M_PI) { pi_minus_tilt -= 2*M_PI; }

        gain_fwd_db  = vertGainTable->lininterpval(tilt_rad);
        gain_back_db = vertGainTable->lininterpval(pi_minus_tilt);
    } else {
        vertGainTable = (LinInterpClass *) NULL;
    }

    if (horizGain) {
        delete horizGain;
    }

    if (vertGain) {
        delete vertGain;
    }

    free(line);

    fclose(fp);

    return(1);
}
/******************************************************************************************/

/******************************************************************************************/
std::vector<AntennaClass *> AntennaClass::readMultipleBoresightAntennas(std::string filename)
{
    int i, fieldIdx, antIdx;
    double x_start, x_stop, u, xval, phase0, phaseRad;
    char *chptr;
    FILE *fp;
    DblDblClass pt;
    std::ostringstream errStr;
    

    if (filename.empty()) {
        throw std::runtime_error(ErrStream() << "ERROR: No multiple boresight antenna file specified");
    }

    if ( !(fp = fopen(filename.c_str(), "rb")) ) {
        throw std::runtime_error(ErrStream() << "ERROR: Unable to open multiple boresight antenna file \"" << filename << "\"");
    }

    enum LineTypeEnum {
         labelLineType,
          dataLineType,
        ignoreLineType,
       unknownLineType
    };

    LineTypeEnum lineType;

    LOGGER_INFO(logger) << "Reading multiple boresight antenna file: " << filename;

    int linenum = 0;
    bool foundLabelLine = false;

    std::vector<AntennaClass *> antennaList;

    std::vector<ListClass<DblDblClass> *> lutGainList;

    std::string line;

    while ( fgetline(fp, line, false) ) {
        linenum++;
        std::vector<std::string> fieldList = splitCSV(line);

        lineType = unknownLineType;
        /**************************************************************************/
        /**** Determine line type                                              ****/
        /**************************************************************************/
        if (fieldList.size() == 0) {
            lineType = ignoreLineType;
        } else {
            int fIdx = fieldList[0].find_first_not_of(' ');
            if (fIdx == (int) std::string::npos) {
                if (fieldList.size() == 1) {
                    lineType = ignoreLineType;
                }
            } else {
                if (fieldList[0].at(fIdx) == '#') {
                    lineType = ignoreLineType;
                }
            }
        }

        if ((lineType == unknownLineType)&&(!foundLabelLine)) {
            lineType = labelLineType;
            foundLabelLine = 1;
        }
        if ((lineType == unknownLineType)&&(foundLabelLine)) {
            lineType = dataLineType;
        }
        /**************************************************************************/

        /**************************************************************************/
        /**** Process Line                                                     ****/
        /**************************************************************************/
        std::string field;
        switch(lineType) {
            case   labelLineType:
                for(fieldIdx=0; fieldIdx<(int) fieldList.size(); fieldIdx++) {
                    field = fieldList.at(fieldIdx);
                    if (fieldIdx == 0) {
                        if (field != "Off-axis angle (deg)") {
                            throw std::runtime_error(ErrStream() << "ERROR: Invalid antenna data file \"" << filename << "(" << linenum
                                                                 << ")\" invalid \"Off-axis angle (deg)\" label = " << field);
                        }
                    } else {
                        ListClass<DblDblClass> *lutGain = new ListClass<DblDblClass>(0);
                        lutGainList.push_back(lutGain);
                        AntennaClass *antenna = new AntennaClass(CConst::antennaLUT_Boresight, field.c_str());
                        antennaList.push_back(antenna);
                    }
                }
                break;
            case dataLineType:
                phaseRad = strtod(fieldList.at(0).c_str(), &chptr)*M_PI/180;
                for(fieldIdx=1; fieldIdx<(int) fieldList.size(); fieldIdx++) {
                    field = fieldList.at(fieldIdx);
                    double gainVal = strtod(fieldList.at(fieldIdx).c_str(), &chptr);
                    lutGainList[fieldIdx-1]->append(DblDblClass(phaseRad, gainVal));

                }
                break;

            case  ignoreLineType:
            case unknownLineType:
                // do nothing
                break;
            default:
                throw std::runtime_error(ErrStream() << "ERROR reading Antenna File: lineType = " << lineType << " INVALID value");
                break;
        }
    }

    if (fp) { fclose(fp); }

    for(antIdx=0; antIdx < static_cast<int>(lutGainList.size()); antIdx++) {
        SplineClass *spline = new SplineClass(lutGainList[antIdx]);

        ListClass<DblDblClass> *sampledData = new ListClass<DblDblClass>(0);

        x_start = 0;
        x_stop  =  M_PI;

        phase0 = (*lutGainList[antIdx])[0].x();
        for (i=0; i<=CConst::antenna_num_interp_pts-1; i++) {
            u = (double) i/(CConst::antenna_num_interp_pts-1);
            xval = x_start*(1.0-u) + x_stop*u;
            pt.setX(xval);
            while(xval >= phase0+2*M_PI) { xval -= 2*M_PI; }
            while(xval < phase0)       { xval += 2*M_PI; }
            pt.setY(spline->splineval(xval));
            sampledData->append(pt);
        }

        LinInterpClass *gainTable = new LinInterpClass(sampledData);

        antennaList[antIdx]->setBoresightGainTable(gainTable);

        delete spline;
        delete sampledData;
        delete lutGainList[antIdx];
    }

    return(antennaList);
}
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION:  AntennaClass::gainDB                                                  ****/
/**** This routine computes the antenna power gain for the specified sectorted antenna ****/
/**** in the direction of the vector(dx, dy, dz).                                      ****/
/******************************************************************************************/
double AntennaClass::gainDB(double dx, double dy, double dz, double h_angle_rad)
{
    double theta   = 0.0;
    double phi     = 0.0;
    double gain_db = 0.0;

    if (type == CConst::antennaOmni) {
        gain_db = 0.0;
    } else if (type == CConst::antennaLUT_H) {
        phi = atan2(dy, dx);
        phi -= h_angle_rad;
        while(phi >= M_PI) { phi -= 2*M_PI; }
        while(phi < -M_PI) { phi += 2*M_PI; }
        gain_db = horizGainTable->lininterpval(phi);
    } else if (type == CConst::antennaLUT_V) {
        theta = atan2(dz, sqrt(dx*dx + dy*dy));
        gain_db = vertGainTable->lininterpval(theta);
    } else if (type == CConst::antennaLUT) {
        phi = atan2(dy, dx);
        phi -= h_angle_rad;
        while(phi >= M_PI) { phi -= 2*M_PI; }
        while(phi < -M_PI) { phi += 2*M_PI; }
        theta = atan2(dz, sqrt(dx*dx + dy*dy));

        double pi_minus_theta = M_PI - theta;
        while(pi_minus_theta >= M_PI) { pi_minus_theta -= 2*M_PI; }

        double gv1 = vertGainTable->lininterpval(theta);
        double gv2 = vertGainTable->lininterpval(pi_minus_theta);
        double gh  = horizGainTable->lininterpval(phi);

        gain_db = (1.0 - fabs(phi)/M_PI)*(gv1 - gain_fwd_db)
                + (fabs(phi)/M_PI)*(gv2 - gain_back_db)
                + gh;
    } else {
        throw std::runtime_error(ErrStream() << "ERROR in AntennaClass::gainDB: type = " << type << " INVALID value");
    }

    return(gain_db);
}
/******************************************************************************************/
/**** FUNCTION:  AntennaClass::gainDB                                                  ****/
/**** This routine computes the antenna power gain for the specified sectorted antenna ****/
/**** in the direction of phi, theta.                                                  ****/
/******************************************************************************************/
double AntennaClass::gainDB(double phi, double theta)
{
    double gain_db = 0.0;

    if (type == CConst::antennaOmni) {
        gain_db = 0.0;
    } else if (type == CConst::antennaLUT_H) {
        while(phi >= M_PI) { phi -= 2*M_PI; }
        while(phi < -M_PI) { phi += 2*M_PI; }
        gain_db = horizGainTable->lininterpval(phi);
    } else if (type == CConst::antennaLUT_V) {
        gain_db = vertGainTable->lininterpval(theta);
    } else if (type == CConst::antennaLUT) {
        while(phi >= M_PI) { phi -= 2*M_PI; }
        while(phi < -M_PI) { phi += 2*M_PI; }

        double pi_minus_theta = M_PI - theta;
        while(pi_minus_theta >= M_PI) { pi_minus_theta -= 2*M_PI; }

        double gv1 = vertGainTable->lininterpval(theta);
        double gv2 = vertGainTable->lininterpval(pi_minus_theta);
        double gh  = horizGainTable->lininterpval(phi);

        gain_db = (1.0 - fabs(phi)/M_PI)*(gv1 - gain_fwd_db)
                + (fabs(phi)/M_PI)*(gv2 - gain_back_db)
                + gh;
    } else {
        throw std::runtime_error(ErrStream() << "ERROR in AntennaClass::gainDB: type = " << type << " INVALID value");
    }

    return(gain_db);
}
/******************************************************************************************/
/**** FUNCTION:  AntennaClass::gainDB                                                  ****/
/**** This routine computes the antenna power gain for the specified sectorted antenna ****/
/**** for an angle off boresight of theta.                                             ****/
/******************************************************************************************/
double AntennaClass::gainDB(double theta)
{
    double gain_db = 0.0;

    if (type == CConst::antennaLUT_Boresight) {
        gain_db = offBoresightGainTable->lininterpval(theta);
    } else {
        throw std::runtime_error(ErrStream() << "ERROR in AntennaClass::gainDB: type = " << type << " INVALID value");
    }

    return(gain_db);
}
/******************************************************************************************/
/**** FUNCTION: check_antenna_gain                                                     ****/
/**** This routine writes antenna gain in two column format to the specified file.     ****/
/**** The first column is angle in degrees from -180 to 180 and the second column is   ****/
/**** antenna gain in dB.  The purpose of this routine is to privide a means of        ****/
/**** verifying the integrity of the spline interpolation used on the antenna data.    ****/
/****     orient == 0 : Horizontal pattern                                             ****/
/****     orient == 1 : Vertical   pattern                                             ****/
/******************************************************************************************/
int AntennaClass::checkGain(const char *flname, int orient, int numpts)
{
    int i;
    double phase_deg, phase_rad, dx, dy, dz, gain_db;
    char *chptr;
    FILE *fp;

    if (numpts <= 0) {
        throw std::runtime_error(ErrStream() << "ERROR in routine check_antenna_gain(), numpts = " << numpts << " must be > 0");
    }

    if (!flname) {
        throw std::runtime_error(ErrStream() << "ERROR in routine check_antenna_gain(), No filename specified");
    }

    if ( !(fp = fopen(flname, "w")) ) {
        throw std::runtime_error(ErrStream() << "ERROR in routine check_antenna_gain(), unable to write to file \"" << flname << "\"");
    }

    LOGGER_INFO(logger) << "Checking " << (orient==0 ? "HORIZONTAL" : "VERTICAL") << " antenna gain.  Writing " << numpts << " points to file \"" << flname << "\"";

    for (i=0; i<=numpts-1; i++) {
        phase_deg = -180.0 + 360.0*i/numpts;
        phase_rad = phase_deg * M_PI / 180.0;
        dx = cos(phase_rad);
        dy = sin(phase_rad);
        if (orient == 0) {
            dz = sin(tilt_rad);
            gain_db = gainDB(dx, dy, dz, 0.0);
        } else {
            gain_db = gainDB(dx, 0.0, dy, 0.0);
        }
        LOGGER_DEBUG(logger) << i << " " << phase_deg << " " << gain_db;
    }

    fclose(fp);

    return(1);
}
/******************************************************************************************/
