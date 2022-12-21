#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

from _afc_types import *
from _afc_errors import *

UNIT_NAME_CLM = 1
PURPOSE_CLM = 2
TEST_VEC_CLM = 3
TEST_CAT_CLM = 4
VERSION_CLM = 9
REQ_ID_CLM = 10
SER_NBR_CLM = 11
NRA_CLM = 12
ID_CLM = 13
RULESET_CLM = 14
ELLIPSE_LONGITUDE_CLM = 15
ELLIPSE_LATITUDE_CLM = 16
ELLIPSE_MAJORAXIS_CLM = 17
ELLIPSE_MINORAXIS_CLM = 18
ELLIPSE_ORIENTATION_CLM = 19
L_POLYGON_LONGITUDE_CLM = 20 
L_POLYGON_LATITUDE_CLM = 21
R_POLYGON_CENTER_LONG_CLM = 50
R_POLYGON_CENTER_LAT_CLM  = 51
R_POLYGON_LENGTH_CLM = 52
R_POLYGON_ANGLE_CLM = 53
HEIGHT = 82
HEIGHTTYPE = 83
VERTICALUNCERTAINTY = 84
INDOORDEPLOYMENT = 85
LOWFREQUENCY_86 = 86
HIGHFREQUENCY_87 = 87
LOWFREQUENCY_88 = 88
HIGHFREQUENCY_89 = 89
GLOBALOPERATINGCLASS_90 = 90
GLOBALOPERATINGCLASS_92 = 92
GLOBALOPERATINGCLASS_94 = 94
GLOBALOPERATINGCLASS_96 = 96
GLOBALOPERATINGCLASS_98 = 98
MINDESIREDPOWER = 100
VENDOREXTS = 101
COMBINED_CLM = 103

REQ_INQUIRY_HEADER = '{"availableSpectrumInquiryRequests": ['
REQ_INQUIRY_FOOTER = '],'
REQ_FOOTER = '}'
REQ_INQ_CHA_HEADER = '"inquiredChannels": ['
REQ_INQ_CHA_FOOTER = '],'
REQ_INQ_CHA_GL_OPER_CLS = '"globalOperatingClass": '

REQ_DEV_DESC_HEADER = '"deviceDescriptor": {'
REQ_DEV_DESC_FOOTER = '},'
REQ_RULESET = '"rulesetIds": '
REQ_SER_NBR = '"serialNumber": ' 
REQ_CERT_ID_HEADER = '"certificationId": [{'
REQ_CERT_ID_FOOTER = '}]'
REQ_NRA = '"nra": '
REQ_ID = '"id": '

REQ_REQUEST_ID = '"requestId": '
REQ_VENDOR_EXT = '"vendorExtensions": [],'
REQ_VERSION = '"version": '

REQ_INQ_FREQ_RANG_HEADER = '"inquiredFrequencyRange": ['
REQ_INQ_FREQ_RANG_FOOTER = '],'
REQ_LOWFREQUENCY = '"lowFrequency": '
REQ_HIGHFREQUENCY = '"highFrequency": '
REQ_MIN_DESIRD_PWR = '"minDesiredPower": '

REQ_LOC_HEADER = '"location": {'
REQ_LOC_FOOTER = '},'
REQ_LOC_INDOORDEPL = '"indoorDeployment": '
REQ_LOC_ELEV_HEADER = '"elevation": {'
REQ_LOC_VERT_UNCERT = '"verticalUncertainty": '
REQ_LOC_HEIGHT_TYPE = '"heightType": '
REQ_LOC_HEIGHT = '"height": '
REQ_LOC_ELLIP_HEADER = '"ellipse": {'
REQ_LOC_ELLIP_FOOTER = '}'
REQ_LOC_ORIENT = '"orientation": '
REQ_LOC_MINOR_AXIS = '"minorAxis": '
REQ_LOC_CENTER = '"center": '
REQ_LOC_L_POLYGON_HEADER = '"linearPolygon": {'
REQ_LOC_L_POLYGON_FOOTER = '}'
REQ_LOC_L_POLYGON_OUTER_HEADER = '"outerBoundary": ['
REQ_LOC_L_POLYGON_OUTER_FOOTER = ']'
REQ_LOC_R_POLYGON_HEADER = '"radialPolygon": {'
REQ_LOC_R_POLYGON_FOOTER = '}'
REQ_LOC_R_POLYGON_OUTER_HEADER = '"outerBoundary": ['
REQ_LOC_R_POLYGON_OUTER_FOOTER = ']'
REQ_LOC_LATITUDE = '"latitude": '
REQ_LOC_LONGITUDE = '"longitude": '
REQ_LOC_MAJOR_AXIS = '"majorAxis": '
REQ_LOC_ANGLE = '"angle": '
REQ_LOC_LENGTH = '"length": '

META_HEADER = '{'
META_TESTCASE_ID = '"testCaseId": '
META_FOOTER = '}'

LOC_TYPE_ELLIPSE = 'ellipse'
LOC_ELLIPSE_NBR_POS = 1
LOC_TYPE_LINEAR_POL = 'linear polygon'
LOC_LINEAR_POL_NBR_POS = 15
LOC_TYPE_RADIAL_POL = 'radial polygon'
LOC_RADIAL_POL_NBR_POS = 20

NEW_AFC_TEST_SUFX = '_afc_test_reqs.txt'
AFC_TEST_IDENT = { 'all':0, 'srs':1, 'urs':2, 'sri':3, 'fsp':4, 'ibp':5, 'sip':6 }

WFA_TEST_DIR = 'wfa_test'

class AfcFreqRange:
    """Afc Frequency range"""
    def __init__(self):
        self.low = 0
        self.high = 0
    def set_range_limit(self, cell, type):
        if isinstance(cell.value, int):
            setattr(self, type, cell.value)
    def append_range(self):
        if (not self.low or not self.high):
            raise IncompleteFreqRange(self.low, self.high)
        if (self.low + self.high):
            return '{' + REQ_LOWFREQUENCY + str(self.low) + ', ' +\
                   REQ_HIGHFREQUENCY + str(self.high) + '}'

class AfcCell:
    """Keep current cell from excel document."""
    def __init__(self, sheet, row):
        self.sh = sheet
        self.row = row


class AfcCoordinates:
    """Create Afc position - pair of latitude and longitude."""
    def __init__(self):
        self.coordinates = {'latitude':'', 'longitude':''}
    def _set_coordinates(self, excell_obj, lat_col, long_col):
        cell = excell_obj.sh.cell(row = excell_obj.row, column = lat_col)
        if isinstance(cell.value, float):
            self.coordinates['latitude'] = '{' + REQ_LOC_LATITUDE +\
                                           str(cell.value) + ', '
        cell = excell_obj.sh.cell(row = excell_obj.row, column = long_col)
        if isinstance(cell.value, float):
            self.coordinates['longitude'] = REQ_LOC_LONGITUDE +\
                                            str(cell.value) + '}'
        return self.coordinates['latitude'] + self.coordinates['longitude']


class AfcCoordRadPol:
    """Create Afc position - pair of angle and length."""
    def __init__(self):
        self.coordinates = {'angle':'', 'length':''}
    def _set_coordinates(self, excell_obj, ang_col, len_col):
        cell = excell_obj.sh.cell(row = excell_obj.row, column = ang_col)
        if isinstance(cell.value, int):
            self.coordinates['angle'] = REQ_LOC_ANGLE +\
                                           str(cell.value) + '}'
        cell = excell_obj.sh.cell(row = excell_obj.row, column = len_col)
        if isinstance(cell.value, int):
            self.coordinates['length'] = '{' + REQ_LOC_LENGTH +\
                                            str(cell.value) + ', '
        return self.coordinates['length'] + self.coordinates['angle']


class AfcGeoCoordinates:
    """Afc Geo coordinates"""
    def __init__(self, sheet, row):
        self.positions = []
        self.region_type = 'unknown'
        self.exc = AfcCell(sheet, row)
        self.find_region_type()

    def _valid_cell(self, clm):
        cell = self.exc.sh.cell(row = self.exc.row, column = clm)
        if (str(cell.value) == 'NULL'):
            return AFC_ERR
        return AFC_OK

    def find_region_type(self):
        if self._valid_cell(ELLIPSE_LONGITUDE_CLM) == AFC_OK:
            self.region_type = LOC_TYPE_ELLIPSE
            self.nbr_loc_pos = LOC_ELLIPSE_NBR_POS
            self.start_col = ELLIPSE_LONGITUDE_CLM
        elif self._valid_cell(L_POLYGON_LONGITUDE_CLM) == AFC_OK:
            self.region_type = LOC_TYPE_LINEAR_POL
            self.nbr_loc_pos = LOC_LINEAR_POL_NBR_POS
            self.start_col = L_POLYGON_LONGITUDE_CLM
        elif self._valid_cell(R_POLYGON_CENTER_LONG_CLM) == AFC_OK:
            self.region_type = LOC_TYPE_RADIAL_POL
            self.nbr_loc_pos = LOC_RADIAL_POL_NBR_POS
            self.start_col = R_POLYGON_LENGTH_CLM

    def _add_positions(self):
        """
           Build list of strings in following format
           {"latitude": 30.5725933, "longitude": -102.229316},
        """
        for i in range(0, self.nbr_loc_pos, 2):
            if self.region_type == LOC_TYPE_RADIAL_POL:
                coor = AfcCoordinates()._set_coordinates(self.exc,
                                                         self.start_col - 1 + i,
                                                         self.start_col - 2 + i)
                if len(coor):
                    self.positions.append(coor)
                coor = AfcCoordRadPol()._set_coordinates(self.exc,
                                                         self.start_col + 1 + i,
                                                         self.start_col + i)
            else:
                coor = AfcCoordinates()._set_coordinates(self.exc,
                                                         self.start_col + 1 + i,
                                                         self.start_col + i)
            if len(coor):
                self.positions.append(coor)

    def _set_orientation(self):
        cell = self.exc.sh.cell(row = self.exc.row,
                                   column = ELLIPSE_ORIENTATION_CLM)
        if isinstance(cell.value, int):
            setattr(self, 'orientation', REQ_LOC_ORIENT + str(cell.value))

    def _set_minoraxis(self):
        cell = self.exc.sh.cell(row = self.exc.row,
                                column = ELLIPSE_MINORAXIS_CLM)
        if isinstance(cell.value, int):
            setattr(self, 'minoraxis', REQ_LOC_MINOR_AXIS + str(cell.value))

    def _set_majoraxis(self):
        cell = self.exc.sh.cell(row = self.exc.row,
                                column = ELLIPSE_MAJORAXIS_CLM)
        if isinstance(cell.value, int):
            setattr(self, 'majoraxis', REQ_LOC_MAJOR_AXIS + str(cell.value))

    def _collect_ellipse(self):
        self._add_positions()
        self._set_orientation()
        self._set_minoraxis()
        self._set_majoraxis()

    def _collect_linear_polygon(self):
        self._add_positions()

    def _collect_radial_polygon(self):
        self._add_positions()

    def _append_ellipse_coordinates(self):
        res = ''
        if (len(self.positions)):
            res += REQ_LOC_CENTER + ''.join(map(str,self.positions)) + ','
        if hasattr(self, 'orientation'):
            res += ' ' + self.orientation + ','
        if hasattr(self, 'minoraxis'):
            res += ' ' + self.minoraxis + ','
        if hasattr(self, 'majoraxis'):
            res += ' ' + self.majoraxis
        if res[-1] == ',':
            res = res[:-1]
        return res + REQ_LOC_ELLIP_FOOTER

    def _append_l_polygon_coordinates(self):
        res = ''
        if (len(self.positions)):
            res += REQ_LOC_L_POLYGON_OUTER_HEADER +\
                   ', '.join(map(str,self.positions))
        return res + REQ_LOC_L_POLYGON_OUTER_FOOTER

    def _append_r_polygon_coordinates(self):
        res = ''
        if (len(self.positions)):
            center_lat_long = self.positions.pop(0)
        if (len(self.positions)):
            res += REQ_LOC_R_POLYGON_OUTER_HEADER +\
                   ', '.join(map(str,self.positions)) +\
                   REQ_LOC_R_POLYGON_OUTER_FOOTER
        if (len(center_lat_long)):
            res += ', ' + REQ_LOC_CENTER +\
                   center_lat_long
        return res

    def collect_coordinates(self):
        res = ''
        if self.region_type == LOC_TYPE_ELLIPSE:
            self._collect_ellipse()
            res = REQ_LOC_ELLIP_HEADER + self._append_ellipse_coordinates()
        elif self.region_type == LOC_TYPE_LINEAR_POL:
            self._collect_linear_polygon()
            res = REQ_LOC_L_POLYGON_HEADER +\
                  self._append_l_polygon_coordinates() +\
                  REQ_LOC_L_POLYGON_FOOTER
        elif self.region_type == LOC_TYPE_RADIAL_POL:
            self._collect_radial_polygon()
            res = REQ_LOC_R_POLYGON_HEADER +\
                  self._append_r_polygon_coordinates() +\
                  REQ_LOC_R_POLYGON_FOOTER
        return res


# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
