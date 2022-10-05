#!/usr/bin/env python3
#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Description

The execution always done in specific order: configuration, database, testing.
In case there are not available valid responses to compare with
need to make acquisition and create new database as following.
    ./afc_tests.py --addr <address> --log debug --cmd run

EXAMPLES

1. Provide test file with new requests to add into DB
    ./afc_tests.py --addr 1.2.3.4 --cmd add_reqs --infile test_vector.txt

2. Configure adrress and run all tests
    ./afc_tests.py --addr 1.2.3.4 --cmd run

3. Configure adrress and log level, run tests based on row index
    ./afc_tests.py --testcase_indexes 2 --addr 1.2.3.4 --cmd run --log debug
    
4. Configure adrress and log level, run tests based on test case ID
    ./afc_tests.py --testcase_ids AFCS.IBP.5,AFCS.FSP.18 --addr 1.2.3.4 --cmd run --log debug

5. Refresh responses by reacquisition tests from the DB
    ./afc_tests.py --addr 1.2.3.4 --cmd reacq
"""

import argparse
import csv
import datetime
import hashlib
import inspect
import io
import json
import logging
import os
import openpyxl as oxl
import re
import requests
import shutil
import sqlite3
import sys
import time
from deepdiff import DeepDiff
from _afc_errors import *
from _version import __version__
from _wfa_tests import *

AFC_URL_SUFFIX = '/fbrat/ap-afc/1.1/'
AFC_REQ_NAME = 'availableSpectrumInquiry'
headers = {'content-type': 'application/json'}

AFC_TEST_DB_FILENAME = 'afc_input.sqlite3'
TBL_REQS_NAME = 'test_vectors'
TBL_RESPS_NAME = 'test_data'
TBL_USERS_NAME = 'user_config'
TBL_AFC_CFG_NAME = 'afc_config'
TBL_AP_CFG_NAME = 'ap_config'

AFC_TEST_STATUS = { 'Ok':0, 'Error':1 }
AFC_OK = AFC_TEST_STATUS['Ok']
AFC_ERR = AFC_TEST_STATUS['Error']

AFC_PROT_NAME = 'https'

# metadata variables
TESTCASE_ID = "testCaseId"
# mandatory keys that need to be read from input text file
MANDATORY_METADATA_KEYS = {TESTCASE_ID}

app_log = logging.getLogger(__name__)

class TestCfg(dict):
    """Keep test configuration"""
    def __init__(self):
        dict.__init__(self)
        self.update({
            'port' : 443,
            'url_path' : AFC_PROT_NAME + '://',
            'log_level' : logging.INFO,
            'db_filename' : AFC_TEST_DB_FILENAME,
            'tests' : None,
            'is_test_by_index': True,
            'resp' : '',
            'precision': None})

    def _send_recv(self, params):
        """Run AFC test and wait for respons"""
        data = params.split("'")
        get_req = ''
        for item in data:
            try:
                json.loads(item)
            except ValueError as e:
                continue
            get_req = item
            break

        new_req_json = json.loads(get_req.encode('utf-8'))
        new_req = json.dumps(new_req_json, sort_keys=True)
        params_data = {
            'conn_type':self['conn_type'],
            'debug':self['debug'],
            'gui':self['gui']
            }
        before_ts = time.monotonic()
        rawresp = requests.post(
            self['url_path'],
            params=params_data,
            data=new_req,
            headers=headers,
            timeout=600,    #10 min
            verify=self['verif_post'])
        resp = rawresp.json()

        tId = resp.get('taskId')
        if (self['conn_type'] == 'async') and (not isinstance(tId, type(None))):
            tState = resp.get('taskState')
            params_data['task_id'] = tId
            while (tState == 'PENDING') or (tState == 'PROGRESS'):
                app_log.debug('_run_test() state %s, tid %s, status %d',
                          tState, tId, rawresp.status_code)
                time.sleep(2)
                rawresp = requests.get(self['url_path'],
                                       params=params_data)
                if rawresp.status_code == 200:
                    resp = rawresp.json()
                    break

        tm_secs = time.monotonic() - before_ts
        app_log.info('Test done at %.1f secs', tm_secs)
        return new_req, resp


class TestResultComparator:
    """ AFC Response comparator

    Private instance attributes:
    _precision -- Precision for results' comparison in dB. 0 means exact match
    """
    def __init__(self, precision):
        """ Constructor

        Arguments:
        precision -- Precision for results' comparison in dB. 0 means exact
                     match
        """
        assert precision >= 0
        self._precision = precision

    def compare_results(self, ref_str, result_str):
        """ Compares reference and actual AFC responses

        Arguments:
        ref_str    -- Reference response JSON in string representation
        result_str -- Actual response json in string representation
        Returns list of difference description strings. Empty list means match
        """
        # List of difference description strings
        diffs = []
        # Reference and actual JSON dictionaries
        jsons = []
        for s, kind in [(ref_str, "reference"), (result_str, "result")]:
            try:
                jsons.append(json.loads(s))
            except json.JSONDecodeError as ex:
                diffs.append(f"Failed to decode {kind} JSON data: {ex}")
                return diffs
        self._recursive_compare(jsons[0], jsons[1], [], diffs)
        return diffs

    def _recursive_compare(self, ref_json, result_json, path, diffs):
        """ Recursive comparator of JSON nodes

        Arguments:
        ref_json    -- Reference response JSON dictionary
        result_json -- Actual response JSON dictionary
        path        -- Path (sequence of indices) to node in question
        diffs       -- List of difference description strings to update
        """
        # Items in questions in JSON dictionaries
        ref_item = self._get_item(ref_json, path)
        result_item = self._get_item(result_json, path)
        # Human readable path representation for difference messages
        path_repr = f"[{']['.join(str(idx) for idx in path)}]"
        if ref_item == result_item:
            return  # Items are equal - nothing to do
        # So, items are different. What's the difference?
        if isinstance(ref_item, dict):
            # One item is dictionary. Other should also should be dictionary...
            if not isinstance(result_item, dict):
                diffs.append(f"Different item types at {path_repr}")
                return
            # ... with same set of keys
            ref_keys = set(ref_item.keys())
            result_keys = set(result_item.keys())
            for unique_key in (ref_keys ^ result_keys):
                if self._compare_channel_lists(ref_json, result_json,
                                               path + [unique_key], diffs):
                    ref_keys -= {unique_key}
                    result_keys -= {unique_key}
            if ref_keys != result_keys:
                msg = f"Different set of keys at {path_repr}"
                for kind, elems in [("reference", ref_keys - result_keys),
                                    ("result", result_keys - ref_keys)]:
                    if elems:
                        msg += \
                            f" Unique {kind} keys: {', '.join(sorted(elems))}."
                diffs.append(msg)
                return
            # Comparing values for individual keys
            for key in sorted(ref_keys):
                self._recursive_compare(ref_json, result_json, path + [key],
                                        diffs)
        elif isinstance(ref_item, list):
            # One item is list. Other should also be list...
            if not isinstance(result_item, list):
                diffs.append(f"Different item types at {path_repr}")
                return
            # If this is a channel list (or part thereof - handle it)
            if self._compare_channel_lists(ref_json, result_json, path, diffs):
                return
            # Proceeding with comparison of other list kinds
            if len(ref_item) != len(result_item):
                diffs.append(
                    (f"Different list lengths at at {path_repr}: "
                     f"{len(ref_item)} elements in reference vs "
                     f"{len(result_item)} elements in result"))
                return
            # Comparing individual elements
            for i in range(len(ref_item)):
                self._recursive_compare(ref_json, result_json, path + [i],
                                        diffs)
        else:
            # Items should be scalars
            for item, kind in [(ref_item, "Reference"),
                               (result_item, "Result")]:
                if not isinstance(item, (int, float, str)):
                    diffs.append((f"{kind} data contains unrecognized item "
                                  f"type at {path_repr}"))
                    return
            diffs.append((f"Difference at {path}: reference content is "
                          f"{ref_item}, result content is {result_item}"))

    def _compare_channel_lists(self, ref_json, result_json, path, diffs):
        """ Trying to compare channel lists

        Arguments:
        ref_json    -- Reference response JSON dictionary
        result_json -- Actual response JSON dictionary
        path        -- Path (sequence of indices) to node in question
        diffs       -- List of difference description strings to update
        Returns true if channel list comparison was done, no further action
        required, False if node is not a channel list, should be compared as
        usual
        """
        if path[-1] == "channelCfi":
            # Comparison will be made at "maxEirp"
            return True
        # Human readable path representation for difference messages
        path_repr = f"[{']['.join(str(idx) for idx in path)}]"
        # EIRP dictionaries, indexed by channel identification (number or
        # frequency range)
        ref_channels = {}
        result_channels = {}
        if path[-1] == "maxEirp":
            # Channel numbers
            for kind, src, chan_dict in \
                    [("reference", ref_json, ref_channels),
                     ("result", result_json, result_channels)]:
                try:
                    numbers = self._get_item(src, path[:-1] + ["channelCfi"],
                                             default_last=[])
                    chan_dict.update(
                        dict(zip([str(n) for n in numbers],
                                 self._get_item(src, path, default_last=[]))))
                except (TypeError, ValueError, KeyError):
                    diffs.append((f"Unrecognized  channel list structure at "
                                  f"{path_repr} in {kind}"))
                    return True
        elif path[-1] == "availableFrequencyInfo":
            # Channel frequencies
            for kind, src, chan_dict in \
                    [("reference", ref_json, ref_channels),
                     ("result", result_json, result_channels)]:
                try:
                    for freq_info in self._get_item(src, path,
                                                    default_last=[]):
                        fr = freq_info["frequencyRange"]
                        chan_dict[
                            f"[{fr['lowFrequency']}-{fr['highFrequency']}"] = \
                            float(freq_info.get("maxPSD") or
                                  freq_info.get("maxPsd"))
                except (TypeError, ValueError, KeyError):
                    diffs.append((f"Unrecognized frequency list structure at "
                                  f"{path_repr} in {kind}"))
                    return True
        else:
            return False
        # Now will compare two channel dictionaries
        # First looking for unique channels
        for this_kind, this_channels, other_kind, other_channels in \
                [("reference", ref_channels, "result", result_channels),
                 ("result", result_channels, "reference", ref_channels)]:
            for channel in sorted(set(this_channels.keys()) -
                                  set(other_channels.keys())):
                diffs.append(
                    (f"Channel {channel} present in {path_repr} of "
                     f"{this_kind} with EIRP limit of "
                     f"{this_channels[channel]}dBm, but absent in "
                     f"{other_kind}"))
        # Then looking for different EIRPs on common channels
        for channel in sorted(set(ref_channels.keys()) &
                              set(result_channels.keys())):
            diff = abs(ref_channels[channel] - result_channels[channel])
            if diff <= self._precision:
                continue
            diffs.append(
                (f"Different values in {path_repr} for channel {channel}: "
                 f"reference has EIRP of {ref_channels[channel]}dBm, "
                 f"result has EIRP of {result_channels[channel]}dBm, "
                 f"difference is: {diff:g}dB"))
        return True

    def _get_item(self, j, path, default_last=None):
        """ Retrieves item by sequence of indices

        Arguments:
        j            -- JSON dictionary
        path         -- Sequence of indices
        default_last -- What to return if item at last index is absent. None
                        means throw exception (if nonlast item is absent -
                        exception is also thrown)
        Returns retrieved item
        """
        for path_idx, elem_idx in enumerate(path):
            try:
                j = j[elem_idx]
            except (KeyError, IndexError):
                if (default_last is not None) and \
                        (path_idx == (len(path) - 1)):
                    return default_last
                raise
        return j


def json_lookup(key, json_obj, val):
    """Loookup for key in json and change it value if required"""
    keepit = []

    def lookup(key, json_obj, val, keepit):
        if isinstance(json_obj, dict):
            for k, v in json_obj.items():
                if isinstance(v, (dict, list)):
                    lookup(key, v, val, keepit)
                elif k == key:
                    keepit.append(v)
                    if val:
                        json_obj[k] = val
        elif isinstance(json_obj, list):
            for node in json_obj:
                lookup(key, node, val, keepit)
        return keepit

    found = lookup(key, json_obj, val, keepit)
    return found


def get_md5(fname):
    hash_md5 = hashlib.md5()
    with open(fname, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()


def parse_tests(cfg):
    app_log.debug('%s()\n', inspect.stack()[0][3])
    filename = ''
    out_fname = ''

    if isinstance(cfg['infile'], type(None)):
        app_log.error('Missing input file')
        return AFC_ERR

    filename = cfg['infile'][0]

    if not os.path.isfile(filename):
        app_log.error('Missing raw test data file %s', filename)
        return AFC_ERR

    test_ident = cfg['test_id']

    if isinstance(cfg['outfile'], type(None)):
        out_fname = test_ident + NEW_AFC_TEST_SUFX
    else:
        out_fname = cfg['outfile'][0]

    wb = oxl.load_workbook(filename, data_only=True)

    for sh in wb:
        app_log.debug('Sheet title: %s', sh.title)
        app_log.debug('rows %d, cols %d\n', sh.max_row, sh.max_column)

    app_log.info('Export tests into file: %s', out_fname)
    fp_new = open(out_fname, 'w')
    sheet = wb.active
    nbr_rows = sheet.max_row
    app_log.debug('Rows range 1 - %d', nbr_rows + 1)
    for i in range(1, nbr_rows + 1):
        cell = sheet.cell(row = i, column = PURPOSE_CLM)
        if ((cell.value is None) or
            (AFC_TEST_IDENT.get(cell.value.lower()) is None) or
            (cell.value == 'SRI')):
            continue

        if (test_ident != 'all') and (cell.value.lower() != test_ident):
            continue

        res_str = REQ_HEADER + REQ_INQUIRY_HEADER + REQ_INQ_CHA_HEADER
        cell = sheet.cell(row = i, column = GLOBALOPERATINGCLASS_90)
        res_str += '{' + REQ_INQ_CHA_GL_OPER_CLS + str(cell.value) + '}, '
        cell = sheet.cell(row = i, column = GLOBALOPERATINGCLASS_92)
        res_str += '{' + REQ_INQ_CHA_GL_OPER_CLS + str(cell.value) + '}, '
        cell = sheet.cell(row = i, column = GLOBALOPERATINGCLASS_94)
        res_str += '{' + REQ_INQ_CHA_GL_OPER_CLS + str(cell.value) + '}, '
        cell = sheet.cell(row = i, column = GLOBALOPERATINGCLASS_96)
        res_str += '{' + REQ_INQ_CHA_GL_OPER_CLS + str(cell.value) + '}, '
        cell = sheet.cell(row = i, column = GLOBALOPERATINGCLASS_98)
        res_str += '{' + REQ_INQ_CHA_GL_OPER_CLS + str(cell.value) + '}'
        res_str += REQ_INQ_CHA_FOOTER + ' '

        cell = sheet.cell(row = i, column = UNIT_NAME_CLM)
        uut = cell.value
        cell = sheet.cell(row = i, column = PURPOSE_CLM)
        purpose = cell.value
        cell = sheet.cell(row = i, column = TEST_VEC_CLM)
        test_vec = cell.value

        test_case_id = uut + "." + purpose + "." + str(test_vec)

        res_str += REQ_DEV_DESC_HEADER
        cell = sheet.cell(row = i, column = RULESET_CLM)
        res_str += REQ_RULESET + '["' + str(cell.value) + '"],'

        cell = sheet.cell(row = i, column = SER_NBR_CLM)
        if isinstance(cell.value, str):
            serial_id = cell.value
        else:
            serial_id = ""
        res_str += REQ_SER_NBR + '"' + serial_id + '",' + REQ_CERT_ID_HEADER

        cell = sheet.cell(row = i, column = NRA_CLM)
        res_str += REQ_NRA + '"' + cell.value + '",'

        cell = sheet.cell(row = i, column = ID_CLM)
        if isinstance(cell.value, str):
            cert_id = cell.value
        else:
            cert_id = ""
        res_str += REQ_ID + '"' + cert_id + '"' + REQ_CERT_ID_FOOTER
        res_str += REQ_DEV_DESC_FOOTER + REQ_INQ_FREQ_RANG_HEADER

        freq_range = AfcFreqRange()
        freq_range.set_range_limit(sheet.cell(row = i, column = LOWFREQUENCY_86), 'low')
        freq_range.set_range_limit(sheet.cell(row = i, column = HIGHFREQUENCY_87), 'high')
        try:
            res_str += freq_range.append_range()
        except IncompleteFreqRange as e:
            app_log.debug(e)
        freq_range = AfcFreqRange()
        freq_range.set_range_limit(sheet.cell(row = i, column = LOWFREQUENCY_88), 'low')
        freq_range.set_range_limit(sheet.cell(row = i, column = HIGHFREQUENCY_89), 'high')
        try:
            tmp_str = freq_range.append_range()
            res_str += ', ' + tmp_str
        except IncompleteFreqRange as e:
            app_log.debug(e)

        res_str += REQ_INQ_FREQ_RANG_FOOTER

        cell = sheet.cell(row = i, column = MINDESIREDPOWER)
        if (cell.value):
            res_str += REQ_MIN_DESIRD_PWR + str(cell.value) + ', '

        #
        # Location
        #
        res_str += REQ_LOC_HEADER
        cell = sheet.cell(row = i, column = INDOORDEPLOYMENT)
        res_str += REQ_LOC_INDOORDEPL + str(cell.value) + ', '

        # Location - elevation
        res_str += REQ_LOC_ELEV_HEADER
        cell = sheet.cell(row = i, column = VERTICALUNCERTAINTY)
        if isinstance(cell.value, int):
            res_str += REQ_LOC_VERT_UNCERT + str(cell.value) + ', '
        cell = sheet.cell(row = i, column = HEIGHTTYPE)
        res_str += REQ_LOC_HEIGHT_TYPE + '"' + str(cell.value) + '"'
        cell = sheet.cell(row = i, column = HEIGHT)
        if isinstance(cell.value, int) or isinstance(cell.value, float):
            res_str += ', ' + REQ_LOC_HEIGHT + str(cell.value)
        res_str += '}, '

        # Location - uncertainty
        is_set = 0
        res_str += REQ_LOC_ELLIP_HEADER
        geo_coor = AfcGeoCoordinates()
        geo_coor.set_coordinates(sheet.cell(row = i, column = LATITUDE_CLM),
                                 'latitude')
        geo_coor.set_coordinates(sheet.cell(row = i, column = LONGITUDE_CLM),
                                 'longitude')
        try:
            res_str += geo_coor.append_coordinates()
            is_set = 1
        except IncompleteGeoCoordinates as e:
            app_log.debug(e)

        cell = sheet.cell(row = i, column = ORIENTATION_CLM)
        uncert = ''
        if isinstance(cell.value, int):
            if is_set:
                res_str += ', '
            is_set = 1
            uncert = REQ_LOC_ORIENT + str(cell.value)
        res_str += uncert

        cell = sheet.cell(row = i, column = MINORAXIS_CLM)
        if isinstance(cell.value, int):
            if is_set:
                res_str += ', '
            is_set = 1
            uncert = REQ_LOC_MINOR_AXIS + str(cell.value)
        res_str += uncert

        cell = sheet.cell(row = i, column = MAJORAXIS_CLM)
        if isinstance(cell.value, int):
            if is_set:
                res_str += ', '
            uncert = REQ_LOC_MAJOR_AXIS + str(cell.value)
        res_str += uncert
        res_str += REQ_LOC_ELLIP_FOOTER + REQ_LOC_FOOTER
        
        cell = sheet.cell(row = i, column = REQ_ID_CLM)
        if isinstance(cell.value, str):
            req_id = cell.value
        else:
            req_id = ""
        res_str += REQ_REQUEST_ID + '"' + req_id + '"'
        res_str += REQ_INQUIRY_FOOTER
        cell = sheet.cell(row = i, column = VERSION_CLM)
        res_str += REQ_VERSION + '"' + str(cell.value) + '"'
        res_str += REQ_FOOTER

        # adding metadata parameters
        res_str += ', '

        # adding test case id
        res_str += META_HEADER
        res_str += META_TESTCASE_ID + '"' + test_case_id + '"'
        res_str += META_FOOTER

        app_log.debug(res_str)
        fp_new.write(res_str+'\n')

    fp_new.close()
    return AFC_OK


def make_db(filename):
    """Create DB file only with schema"""
    app_log.debug('%s()', inspect.stack()[0][3])
    if os.path.isfile(filename):
        app_log.debug('%s() The db file is exists, no need to create new one.',
                      inspect.stack()[0][3])
        return True

    app_log.info('Create DB tables (%s, %s) from source files',
                 TBL_REQS_NAME, TBL_RESPS_NAME)
    con = sqlite3.connect(filename)
    cur = con.cursor()
    cur.execute('CREATE TABLE IF NOT EXISTS ' + TBL_REQS_NAME + 
                ' (test_id varchar(50), data json)')
    cur.execute('CREATE TABLE IF NOT EXISTS ' + TBL_RESPS_NAME +
                ' (test_id varchar(50), data json, hash varchar(255))')
    con.close()
    return True


def compare_afc_config(cfg):
    """
    Compare AFC configuration from the DB with provided one.
    """
    app_log.debug('%s()', inspect.stack()[0][3])

    if not os.path.isfile(cfg['db_filename']):
        app_log.error('Missing DB file %s', cfg['db_filename'])
        return AFC_ERR

    con = sqlite3.connect(cfg['db_filename'])
    cur = con.cursor()
    cur.execute('SELECT * FROM %s' % TBL_AFC_CFG_NAME)
    found_cfgs = cur.fetchall()
    con.close()

    # get record from the input file
    if isinstance(cfg['infile'], type(None)):
        app_log.debug('Missing input file to compare with.')
        return AFC_OK

    filename = cfg['infile'][0]
    with open(filename, 'r') as fp_test:
        while True:
            rec = fp_test.read()
            if not rec:
                break
            try:
                get_rec = json.loads(rec)
            except (ValueError, TypeError) as e:
                continue
            break
    app_log.debug(json.dumps(get_rec, sort_keys=True, indent = 4))

    get_cfg = ''
    app_log.debug('Found %d config records', len(found_cfgs))
    idx = 0
    max_idx = len(found_cfgs)
    if not isinstance(cfg['idx'], type(None)):
        idx = cfg['idx']
        if idx >= max_idx:
            app_log.error("The index (%d) is out of range (0 - %d).",
                          idx, max_idx - 1)
            return AFC_ERR
        max_idx = idx + 1
    while idx < max_idx:
        for item in list(found_cfgs[idx]):
            try:
                get_cfg = json.loads(item)
            except (ValueError, TypeError) as e:
                continue
            break
        app_log.debug("Record %d:\n%s",
                      idx,
                      json.dumps(get_cfg['afcConfig'],
                                 sort_keys=True,
                                 indent = 4))

        get_diff = DeepDiff(get_cfg['afcConfig'],
                            get_rec,
                            report_repetition=True)
        app_log.info("rec %d:\n%s", idx, get_diff)
        idx += 1

    return AFC_OK


def start_acquisition(cfg):
    """
    Fetch test vectors from the DB, drop previous response table,
    run tests and fill responses in the DB with hash values
    """
    app_log.debug('%s()', inspect.stack()[0][3])

    if not os.path.isfile(cfg['db_filename']):
        app_log.error('Missing DB file %s', cfg['db_filename'])
        return AFC_ERR

    con = sqlite3.connect(cfg['db_filename'])
    cur = con.cursor()
    cur.execute('SELECT * FROM %s' % TBL_REQS_NAME)
    found_reqs = cur.fetchall()
    try:
        cur.execute('DROP TABLE ' + TBL_RESPS_NAME)
    except Exception as OperationalError:
        app_log.debug('Missing table %s', TBL_RESPS_NAME)
    cur.execute('CREATE TABLE ' + TBL_RESPS_NAME +
                ' (test_id varchar(50), data json, hash varchar(255))')

    row_idx = 0
    found_range = len(found_reqs)
    if len(found_reqs) == 0:
        app_log.error('Missing request records')
        return AFC_ERR
    app_log.info('Acquisition to number of tests - %d', found_range - row_idx)

    while row_idx < found_range:
        # Fetch test vector to create request
        new_req, resp = cfg._send_recv(found_reqs[row_idx][1])
        json_lookup('availabilityExpireTime', resp, '0')
        upd_data = json.dumps(resp, sort_keys=True)
        hash_obj = hashlib.sha256(upd_data.encode('utf-8'))
        cur.execute('INSERT INTO ' + TBL_RESPS_NAME + ' values ( ?, ?, ?)',
                    [found_reqs[row_idx][0],
                     upd_data,
                     hash_obj.hexdigest()])
        con.commit()
        row_idx += 1
    con.close()
    return AFC_OK


def process_jsonline(line):
    """
    Function to process the input line from .txt file containing comma 
    separated json strings
    """

    # convert the input line to list of dictioanry/dictionaries
    line_list = json.loads("[" + line + "]")
    request_dict = line_list[0]
    metadata_dict = line_list[1] if len(line_list)>1 else {}

    return request_dict, metadata_dict


def get_db_req_resp(cfg):
    """
    Function to retrieve request and response records from the database
    """
    con = sqlite3.connect(cfg['db_filename'])
    cur = con.cursor()
    cur.execute('SELECT * FROM %s' % TBL_REQS_NAME)
    found_reqs = cur.fetchall()
    db_reqs_list = [ row[0] for row in found_reqs ]

    cur.execute('SELECT * FROM %s' % TBL_RESPS_NAME)
    found_resps = cur.fetchall()
    db_resp_list = [ row[0] for row in found_resps ]
    con.close()

    return db_reqs_list, db_resp_list


def add_reqs(cfg):
    """Prepare DB source files"""

    if isinstance(cfg['infile'], type(None)):
        app_log.error('Missing input file')
        return AFC_ERR

    filename = cfg['infile'][0]
    app_log.debug('%s() %s', inspect.stack()[0][3], filename)

    if not os.path.isfile(filename):
        app_log.error('Missing raw test data file %s', filename)
        return AFC_ERR

    if not make_db(cfg['db_filename']):
        return AFC_ERR

    # fetch available requests and responses
    db_reqs_list, db_resp_list = get_db_req_resp(cfg)

    con = sqlite3.connect(cfg['db_filename'])
    with open(filename, 'r') as fp_test:
        while True:
            dataline = fp_test.readline()
            if not dataline:
                break

            # process dataline arguments
            request_json, metadata_json = process_jsonline(dataline)

            # reject the request if mandatory metadata arguments are not present
            if not MANDATORY_METADATA_KEYS.issubset(
                    set(metadata_json.keys())):
                # missing mandatory keys in test case input
                app_log.error("Test case input does not contain required"
                    " mandatory arguments: %s",
                    ", ".join(list(
                        MANDATORY_METADATA_KEYS - set(metadata_json.keys()))))
                return AFC_ERR

            # check if the test case already exists in the database test vectors
            if metadata_json[TESTCASE_ID] in db_reqs_list:
                app_log.error("Test case: %s already exists in database", 
                    metadata_json[TESTCASE_ID])
                break

            app_log.info("Executing test case: %s", metadata_json[TESTCASE_ID])
            new_req, resp = cfg._send_recv(json.dumps(request_json))

            # get request id from a request, response not always has it
            # the request contains test category
            new_req_json = json.loads(new_req.encode('utf-8'))
            req_id = json_lookup('requestId', new_req_json, None)

            resp_res = json_lookup('shortDescription', resp, None)
            if (resp_res[0] != 'Success') \
                and (req_id[0].lower().find('urs') == -1) \
                    and (req_id[0].lower().find('ibp') == -1):
                app_log.error('Failed in test response - %s', resp_res)
                break

            app_log.info('Got response for the request')
            json_lookup('availabilityExpireTime', resp, '0')

            app_log.info('Insert new request in DB')
            cur = con.cursor()
            cur.execute('INSERT INTO ' + TBL_REQS_NAME + ' VALUES ( ?, ?)',
                        (metadata_json[TESTCASE_ID], new_req,))
            con.commit()

            app_log.info('Insert new resp in DB')
            upd_data = json.dumps(resp, sort_keys=True)
            hash_obj = hashlib.sha256(upd_data.encode('utf-8'))
            cur = con.cursor()
            cur.execute('INSERT INTO ' + TBL_RESPS_NAME + ' values ( ?, ?, ?)',
                        [metadata_json[TESTCASE_ID], 
                        upd_data, hash_obj.hexdigest()])
            con.commit()
    con.close()
    return AFC_OK


def dump_table(conn, tbl_name, out_file):
    app_log.debug(f"{inspect.stack()[0][3]}() {tbl_name}")
    app_log.info(f"\n\tDump DB table ({tbl_name}) to file.\n")
    fp_new = ''

    if 'single' in out_file:
        fp_new = open(out_file['single'], 'w')

    conn.execute(f"SELECT * FROM {tbl_name}")
    found_data = conn.fetchall()
    for val in enumerate(found_data):
        if isinstance(fp_new, io.IOBase):
            fp_new.write(f"{str(val)}\n")
        elif 'split' in out_file:
            tbl_fname = {
                TBL_REQS_NAME: '_Request.txt',
                TBL_RESPS_NAME: '_Response.txt'
            }
            new_json = json.loads(val[1][1].encode('utf-8'))
            req_id = json_lookup('requestId', new_json, None)
            # omit URS testcases
            if (req_id[0].lower().find('urs') != -1 or
                len(req_id[0]) == 0):
                continue

            name, nbr = re.findall(r'(\w+?)(\d+)', req_id[0])[0];
            fp_test = open(f"{out_file['split']}/AFCS_{name}_{nbr}" +\
                           f"{tbl_fname[tbl_name]}", 'a')
            fp_test.write(f"{val[1][1]}\n")
            fp_test.close()
        else:
            # Just dump to the console
            app_log.info(f"{val[1]}")

    if isinstance(fp_new, io.IOBase):
        fp_new.close()


def dump_database(cfg):
    """Dump data from test DB tables"""
    app_log.debug(f"{inspect.stack()[0][3]}()")
    find_key = ''
    found_tables = []
    # keep configuration for output path and files
    # 'single' - only single file for whole output
    # 'split' - separate file for each response
    out_file = {}

    if not os.path.isfile(cfg['db_filename']):
        app_log.error(f"Missing DB file {cfg['db_filename']}")
        return AFC_ERR

    set_dump_db_opts = {
        'wfa': [(TBL_REQS_NAME,), (TBL_RESPS_NAME,)],
        'req': [(TBL_REQS_NAME,)],
        'resp': [(TBL_RESPS_NAME,)],
        'ap': [('ap_config',)],
        'cfg': [('afc_config',)],
        'user': [('user_config',)]
    }

    tbl = 'True'
    if isinstance(cfg['table'], list):
        tbl = cfg['table'][0]
    con = sqlite3.connect(cfg['db_filename'])
    cur = con.cursor()

    if tbl in set_dump_db_opts:
        # Dump only tables with requests and responses
        found_tables.extend(set_dump_db_opts[tbl])
    elif tbl == 'True':
        # Dump all tables if no options provided
        cur.execute(f"SELECT name FROM sqlite_master WHERE type='table';")
        found_tables = cur.fetchall()

    if tbl == 'wfa':
        out_file['split'] = './'
        if not isinstance(cfg['outpath'], type(None)):
            out_file['split'] = cfg['outpath'][0] + '/'

        out_file['split'] += WFA_TEST_DIR
        if os.path.exists(out_file['split']):
            shutil.rmtree(out_file['split'])
        os.mkdir(out_file['split'])
    elif isinstance(cfg['outfile'], type(None)):
        app_log.error(f"Missing output filename.\n")
        return AFC_ERR
    else:
        out_file['single'] = cfg['outfile'][0]

    for tbl in enumerate(found_tables):
        app_log.debug(f"Dump {tbl} to {out_file}")
        dump_table(cur, tbl[1][0], out_file)
    con.close()
    return AFC_OK


def export_admin_config(cfg):
    """Export admin server configuration"""
    app_log.debug('%s()', inspect.stack()[0][3])

    con = sqlite3.connect(cfg['db_filename'])
    cur = con.cursor()
    cur.execute('SELECT COUNT(*) FROM ' + TBL_USERS_NAME)
    found_rcds = cur.fetchall()
    with open(cfg['outfile'][0], 'w') as fp_exp:
        for i in range(1, found_rcds[0][0] + 1):
            query = 'SELECT * FROM ' + TBL_USERS_NAME + ' WHERE '\
                    'user_id=\"' + str(i) + "\""
            cur.execute(query)
            found_user = cur.fetchall()

            query = 'SELECT * FROM ' + TBL_AFC_CFG_NAME + ' WHERE '\
                    'user_id=\"' + str(i) + "\""
            cur.execute(query)
            found_cfg = cur.fetchall()

            query = 'SELECT * FROM ' + TBL_AP_CFG_NAME + ' WHERE '\
                    'user_id=\"' + str(i) + "\""
            cur.execute(query)
            found_aps = cur.fetchall()

            aps = ''
            for val in enumerate(found_aps):
                aps += val[1][1]
                if val[1][0] != found_aps[-1][0]:
                    aps += ','
            out_str = '{"afcAdminConfig":' + found_cfg[0][1] + ', '\
                      '"userConfig":' + found_user[0][1] + ', '\
                      '"apConfig":[' + aps + ']}'
            if i != range(found_rcds[0][0]):
                out_str += '\n'
            fp_exp.write(out_str)
    con.close()
    app_log.info('Server admin config exported to %s', cfg['outfile'][0])
    return AFC_OK


def dry_run_test(cfg):
    """Run one or more requests from provided file"""
    if isinstance(cfg['infile'], type(None)):
        app_log.error('Missing input file')
        return AFC_ERR

    filename = cfg['infile'][0]
    app_log.debug('%s() %s', inspect.stack()[0][3], filename)

    if not os.path.isfile(filename):
        app_log.error('Missing raw test data file %s', filename)
        return AFC_ERR

    with open(filename, 'r') as fp_test:
        while True:
            dataline = fp_test.readline()
            if not dataline:
                break
            app_log.info('Request:')
            app_log.info(dataline)

            # process dataline arguments
            request_json, _ = process_jsonline(dataline)

            new_req, resp = cfg._send_recv(json.dumps(request_json))

            # get request id from a request, response not always has it
            # the request contains test category
            new_req_json = json.loads(new_req.encode('utf-8'))
            req_id = json_lookup('requestId', new_req_json, None)

            resp_res = json_lookup('shortDescription', resp, None)
            if (resp_res[0] != 'Success') \
                and (req_id[0].lower().find('urs') == -1):
                app_log.error('Failed in test response - %s', resp_res)
                app_log.debug(resp)
                break

            app_log.info('Got response for the request')
            app_log.info('Resp:')
            app_log.info(resp)
            app_log.info('\n\n')

            json_lookup('availabilityExpireTime', resp, '0')
            upd_data = json.dumps(resp, sort_keys=True)
            hash_obj = hashlib.sha256(upd_data.encode('utf-8'))
    return AFC_OK


def get_nbr_testcases(cfg):
    """ Find APs count on DB table """
    if not os.path.isfile(cfg['db_filename']):
        print ('INFO: Missing DB file %s', cfg['db_filename'])
        return False
    con = sqlite3.connect(cfg['db_filename'])
    cur = con.cursor()
    cur.execute('SELECT count("requestId") from ' + TBL_REQS_NAME)
    found_data = cur.fetchall()
    db_inquiry_count = found_data[0][0]
    con.close()
    app_log.debug("found %s ap lists from db table",db_inquiry_count)
    return db_inquiry_count


def test_report(fname, runtimedata, testnumdata, testvectordata,
                test_result, upd_data):
    """Procedure to generate AFC test results report.
    Args:
        runtimedata(str) : Tested case running time.
        testnumdata(str): Tested case id.
        testvectordata(int): Tested vector id.
        test_result(str: Test result Pass is 0 or Fail is 1)
        upd_data(list): List for test response data
    Return:
        Create test results file.
    """
    ts_time = datetime.datetime.fromtimestamp(time.time()).\
        strftime('%Y_%m_%d_%H%M%S')
    # Test results output args
    data_names = ['Date', 'Test Number', 'Test Vector', 'Running Time',
                  'Test Result', 'Response data']
    data = {'Date': [ts_time], 'Test Number': [testnumdata],
            'Test Vector': [testvectordata], 'Running Time': [runtimedata],
            'Test Result': [test_result], 'Response data': [upd_data]}
    with open(fname, "a") as f:
        file_writer = csv.DictWriter(f, fieldnames=data_names)
        if os.stat(fname).st_size == 0: file_writer.writeheader()
        file_writer.writerow(data)


def _run_test(cfg, reqs, resps, test_cases):
    """
    Run tests
    reqs: {testcaseid: [request_json_str]}
    resps: {testcaseid: [response_json_str, response_hash]}
    test_cases: [testcaseids]
    """
    app_log.debug(f"{inspect.stack()[0][3]}() {test_cases} "
                  f"{cfg['url_path']}")

    accum_secs = 0
    results_comparator = TestResultComparator(precision=cfg['precision'] or 0)
    for test_case in test_cases:
        test_res = AFC_OK
        app_log.info('Prepare to run test - %s', test_case)
        if test_case not in reqs:
            app_log.warning("The requested test case %s is invalid/not "
                            "available in database", test_case)
            continue
            
        request_data = reqs[test_case][0]

        req_id = json_lookup('requestId', request_data, None)
        params_data = {
            'conn_type':cfg['conn_type'],
            'debug':cfg['debug'],
            'gui':cfg['gui']
            }
        if (cfg['cache'] == False):
            params_data['nocache'] = 'True'
        before_ts = time.monotonic()
        rawresp = requests.post(cfg['url_path'],
                                params=params_data,
                                data=json.dumps(request_data),
                                headers=headers,
                                timeout=600,    #10 min
                                verify=cfg['verif_post'])
        resp = rawresp.json()

        tId = resp.get('taskId')
        if (cfg['conn_type'] == 'async') and (not isinstance(tId, type(None))):
            tState = resp.get('taskState')
            params_data['task_id'] = tId
            while (tState == 'PENDING') or (tState == 'PROGRESS'):
                app_log.debug('_run_test() state %s, tid %s, status %d',
                          tState, tId, rawresp.status_code)
                time.sleep(2)
                rawresp = requests.get(cfg['url_path'],
                                       params=params_data)
                if rawresp.status_code == 200:
                    resp = rawresp.json()
                    break

        tm_secs = time.monotonic() - before_ts

        json_lookup('availabilityExpireTime', resp, '0')
        upd_data = json.dumps(resp, sort_keys=True)

        diffs = []
        hash_obj = hashlib.sha256(upd_data.encode('utf-8'))
        diffs = results_comparator.compare_results(ref_str=resps[test_case][0],
                                                   result_str=upd_data)
        if (resps[test_case][1] == hash_obj.hexdigest()) \
                if cfg['precision'] is None else (not diffs):
            app_log.info('Test %s is Ok', req_id[0])
        else:
            app_log.error('Test %s (%s) is Fail', test_case, req_id[0])
            for line in diffs:
                app_log.error(f"  Difference: {line}")
            app_log.error(hash_obj.hexdigest())
            test_res = AFC_ERR
        
        accum_secs += tm_secs
        app_log.info('Test done at %.1f secs', tm_secs)

        # For saving test results option
        if isinstance(cfg['outfile'], list):
            test_report(cfg['outfile'][0], float(tm_secs),
                        test_case, req_id[0],
                        ("PASS" if test_res == AFC_OK else "FAIL"), upd_data)

    app_log.info("Total testcases runtime : %s secs", round(accum_secs, 1))
    return test_res



def run_test(cfg):
    """Fetch test vectors from the DB and run tests"""
    app_log.debug(f"{inspect.stack()[0][3]}() {cfg['tests']}, "
                  f"{cfg['url_path']}")

    if not os.path.isfile(cfg['db_filename']):
        app_log.error('Missing DB file %s', cfg['db_filename'])
        return AFC_ERR

    con = sqlite3.connect(cfg['db_filename'])
    cur = con.cursor()
    cur.execute('SELECT * FROM %s' % TBL_REQS_NAME)
    found_reqs = cur.fetchall()
    cur.execute('SELECT * FROM %s' % TBL_RESPS_NAME)
    found_resps = cur.fetchall()

    if cfg["is_test_by_index"]:
        # reformat the reqs_dict and resp_dict accordingly
        reqs_dict = {
            str(req_index+1): [json.loads(row[1])]
            for req_index, row in enumerate(found_reqs)
        }
        resp_dict = {
            str(resp_index+1): [row[1], row[2]]
            for resp_index, row in enumerate(found_resps)
        }
    else:
        reqs_dict = {
            row[0]: [json.loads(row[1])]
            for row in found_reqs
        }
        resp_dict = {
            row[0]: [row[1], row[2]]
            for row in found_resps
        }

    con.close()

    if isinstance(cfg['tests'], type(None)):
        test_cases = 0
    else:
        test_cases = list(map(str.strip, cfg['tests'].split(',')))

    if test_cases == 0:
        if cfg["is_test_by_index"]:
            test_cases = [
                str(item) for item in list(range(1, len(reqs_dict)+1))
            ]
        else:
            test_cases = list(reqs_dict.keys())

    # run required test cases
    return _run_test(cfg, reqs_dict, resp_dict, test_cases)


log_level_map = {
    'debug' : logging.DEBUG,    # 10
    'info' : logging.INFO,      # 20
    'warn' : logging.WARNING,   # 30
    'err' : logging.ERROR,      # 40
    'crit' : logging.CRITICAL,  # 50
}

def set_log_level(opt):
    app_log.setLevel(log_level_map[opt])
    return log_level_map[opt]


def get_version(cfg):
    """Get AFC test utility version"""
    app_log.info('AFC Test utility version %s', __version__)
    app_log.info('AFC Test db hash %s', get_md5(AFC_TEST_DB_FILENAME))


# available commands to execute in alphabetical order
execution_map = {
    'add_reqs' : add_reqs,
    'cmp_cfg' : compare_afc_config,
    'dry_run' : dry_run_test,
    'dump_db': dump_database,
    'get_nbr_testcases' : get_nbr_testcases,
    'exp_adm_cfg': export_admin_config,
    'parse_tests': parse_tests,
    'reacq' : start_acquisition,
    'run' : run_test,
    'ver' : get_version
}


def make_arg_parser():
    """Define command line options"""
    args_parser = argparse.ArgumentParser(
        epilog=__doc__.strip(),
        formatter_class=argparse.RawTextHelpFormatter)

    args_parser.add_argument('--addr', nargs=1, type=str,
                         help="<address> - set AFC Server address.\n")
    args_parser.add_argument('--prot', nargs='?', choices=['https', 'http'],
                         default='https',
                         help="<http|https> - set connection protocol "
                              "(default=https).\n")
    args_parser.add_argument('--port', nargs='?', default='443',
                         type=int,
                         help="<port> - set connection port "
                              "(default=443).\n")
    args_parser.add_argument('--conn_type', nargs='?',
                         choices=['sync', 'async'], default='sync',
                         help="<sync|async> - set connection to be "
                              "synchronous or asynchronous (default=sync).\n")
    args_parser.add_argument('--conn_tm', nargs='?', default=None, type=int,
                         help="<secs> - set timeout for asynchronous "
                              "connection (default=None). \n")
    args_parser.add_argument('--verif_post', action='store_false',
                         help="<verif_post> - verify SSL on post request.\n")
    args_parser.add_argument('--outfile', nargs=1, type=str,
                         help="<filename> - set filename for test "
                              "results output.\n")
    args_parser.add_argument('--outpath', nargs=1, type=str,
                         help="<filepath> - set path to output filename for "
                              "results output.\n")
    args_parser.add_argument('--infile', nargs=1, type=str,
                         help="<filename> - set filename as a source "
                              "for test requests.\n")
    args_parser.add_argument('--debug', action='store_true',
                         help="during a request make files "
                              "with details for debugging.\n")
    args_parser.add_argument('--gui', action='store_true',
                         help="during a request make files "
                              "with details for debugging.\n")
    args_parser.add_argument('--log', type=set_log_level,
                         default='info', dest='log_level',
                         help="<info|debug|warn|err|crit> - set "
                         "logging level (default=info).\n")
    args_parser.add_argument('--testcase_indexes', nargs='?',
                         help="<N1,...> - set single or group of tests "
                         "to run.\n")
    args_parser.add_argument('--testcase_ids', nargs='?',
                         help="<N1,...> - set single or group of test case ids "
                         "to run.\n")
    args_parser.add_argument('--table', nargs=1, type=str,
                         help="<wfa|req|resp|ap|cfg|user> - set "
                         "database table name.\n")
    args_parser.add_argument('--idx', nargs='?',
                         type=int,
                         help="<index> - set table record index.\n")
    args_parser.add_argument('--test_id',
                         default='all',
                         help="WFA test identifier, for example "
                         "srs, urs, fsp, ibp, sip, etc (default=all).\n")
    args_parser.add_argument("--precision", metavar="PRECISION_DB", type=float,
                         help="Maximum allowed deviation of power limits from "
                         "reference values in dB. 0 means exact match is "
                         "required. Default is to use hash-based exact match "
                         "comparison")
    args_parser.add_argument('--cache', action='store_true',
                         help="enable cache during a request, otherwise "
                         "disabled.\n")

    args_parser.add_argument('--cmd', choices=execution_map.keys(), nargs='?',
        help="run - run test from DB and compare.\n"
        "dry_run - run test from file and show response.\n"
        "exp_adm_cfg - export admin config into a file.\n"
        "add_reqs - run test from provided file and insert with response into "
        "the databsse.\n"
        "dump_db - dump tables from the database.\n"
        "get_nbr_testcases - return number of testcases.\n"
        "parse_tests - parse WFA provided tests into a files.\n"
        "reqca - reacquision every test from the database and insert new "
        "responses.\n"
        "cmp_cfg - compare AFC config from the DB to provided from a file.\n"
        "ver - get version.\n")

    return args_parser


def prepare_args(parser, cfg):
    """Prepare required parameters"""
    app_log.debug(f"{inspect.stack()[0][3]}() {parser.parse_args()}")
    cfg.update(vars(parser.parse_args()))

    # check if test indexes and test ids are given
    if cfg["testcase_indexes"] and cfg["testcase_ids"]:
        # reject the request
        app_log.error('Please use either "--testcase_indexes"'
                      ' or "--testcase_ids" but not both')
        return AFC_ERR

    if cfg["testcase_indexes"]:
        cfg["tests"] = cfg.pop("testcase_indexes")
        cfg.pop("testcase_ids")
    elif cfg["testcase_ids"]:
        cfg["is_test_by_index"] = False
        cfg["tests"] = cfg.pop("testcase_ids")
        cfg.pop("testcase_indexes")

    if isinstance(cfg['addr'], list):
        # set URL if protocol is not the default
        if cfg['prot'] != AFC_PROT_NAME:
            cfg['url_path'] = cfg['prot'] + '://'

        cfg['url_path'] += str(cfg['addr'][0]) + ':' + str(cfg['port']) +\
                           AFC_URL_SUFFIX +\
                           '/' + AFC_REQ_NAME


def main():
    """Main function of the utility"""
    app_log.setLevel(logging.INFO)
    console_log = logging.StreamHandler()
    console_log.setFormatter(logging.Formatter('%(levelname)s: %(message)s'))
    app_log.addHandler(console_log)

    res = AFC_OK
    parser = make_arg_parser()
    test_cfg = TestCfg()
    if prepare_args(parser, test_cfg) == AFC_ERR:
        # error in preparing arguments
        res = AFC_ERR
    else:
        if isinstance(test_cfg['cmd'], type(None)):
            parser.print_help()
        else:
            res = execution_map[test_cfg['cmd']](test_cfg)
    sys.exit(res)


if __name__ == '__main__':
    main()


# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
