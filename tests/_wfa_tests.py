#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

from _afc_errors import *

UNIT_NAME_CLM = 1
PURPOSE_CLM = 2
TEST_VEC_CLM = 3
VERSION_CLM = 9
REQ_ID_CLM = 10
SER_NBR_CLM = 11
NRA_CLM = 12
ID_CLM = 13
RULESET_CLM = 14
LONGITUDE_CLM = 15
LATITUDE_CLM = 16
MAJORAXIS_CLM = 17
MINORAXIS_CLM = 18
ORIENTATION_CLM = 19
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

REQ_HEADER = '{'
REQ_INQUIRY_HEADER = '"availableSpectrumInquiryRequests": [{'
REQ_INQUIRY_FOOTER = '}],'
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
REQ_LOC_CENTER = '"center": {'
REQ_LOC_LATITUDE = '"latitude": '
REQ_LOC_LONGITUDE = '"longitude": '
REQ_LOC_MAJOR_AXIS = '"majorAxis": '

NEW_AFC_TEST_SUFX = '_afc_test_reqs.json'
AFC_TEST_IDENT = { 'all':0, 'srs':1, 'urs':2, 'sri':3, 'fsp':4, 'ibp':5, 'sip':6 }


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


class AfcGeoCoordinates:
    """Afc Geo coordinates"""
    def __init__(self):
        self.latitude = 0
        self.longitude = 0
    def set_coordinates(self, cell, type):
        if isinstance(cell.value, float):
            setattr(self, type, cell.value)
    def append_coordinates(self):
        if (not self.latitude or not self.longitude):
            raise IncompleteGeoCoordinates(self.latitude, self.longitude)
        if (self.latitude + self.longitude):
            return REQ_LOC_CENTER + REQ_LOC_LATITUDE +\
                   str(self.latitude) + ', ' + REQ_LOC_LONGITUDE +\
                   str(self.longitude) + '}'

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
