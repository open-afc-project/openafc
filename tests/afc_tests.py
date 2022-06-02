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

3. Configure adrress and log level, run test number 2
    ./afc_tests.py --test 2 --addr 1.2.3.4 --cmd run
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
import requests
import shutil
import sqlite3
import sys
import time
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
            'tests' : 'all',
            'resp' : '',})

    def _send_recv(self, params):
        """Run AFC test and wait for respons"""
        new_req_json = json.loads(params.encode('utf-8'))
        new_req = json.dumps(new_req_json, sort_keys=True)
        before_ts = time.monotonic()
        rawresp = requests.post(
            self['url_path'], data=new_req, headers=headers,
            timeout=None, verify=self['verif_post'])
        resp = rawresp.json()
        tm_secs = time.monotonic() - before_ts
        app_log.info('Test done at %.1f secs', tm_secs)
        return new_req, resp


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

    wb = oxl.load_workbook(filename)

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

        cell = sheet.cell(row = i, column = PURPOSE_CLM)
        purpose = cell.value
        cell = sheet.cell(row = i, column = TEST_VEC_CLM)
        test_vec = cell.value

        res_str += REQ_DEV_DESC_HEADER
        cell = sheet.cell(row = i, column = RULESET_CLM)
        res_str += REQ_RULESET + '["' + str(cell.value) + '"],'

        serial_id = '"",'
        cell = sheet.cell(row = i, column = SER_NBR_CLM)
        if isinstance(cell.value, str):
            serial_id = '"SN-' + purpose + str(test_vec) + '",'
        res_str += REQ_SER_NBR + serial_id + REQ_CERT_ID_HEADER

        cell = sheet.cell(row = i, column = NRA_CLM)
        res_str += REQ_NRA + '"' + cell.value + '",'

        cell = sheet.cell(row = i, column = ID_CLM)
        cert_id = '""'
        if isinstance(cell.value, str):
            cert_id = '"FCCID-' + purpose + str(test_vec) + '"'
        res_str += REQ_ID + cert_id + REQ_CERT_ID_FOOTER
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
        if isinstance(cell.value, int):
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

        res_str += REQ_REQUEST_ID + '"REQ-' + purpose + str(test_vec) + '"'
        res_str += REQ_INQUIRY_FOOTER
        cell = sheet.cell(row = i, column = VERSION_CLM)
        res_str += REQ_VERSION + '"' + str(cell.value) + '"'
        res_str += REQ_FOOTER
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
    cur.execute('CREATE TABLE ' + TBL_REQS_NAME + ' (data json)')
    cur.execute('CREATE TABLE ' + TBL_RESPS_NAME +
                ' (data json, hash varchar(255))')
    con.close()
    return True


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

    con = sqlite3.connect(cfg['db_filename'])
    with open(filename, 'r') as fp_test:
        while True:
            dataline = fp_test.readline()
            if not dataline:
                break

            new_req, resp = cfg._send_recv(dataline)

            # get request id from a request, response not always has it
            # the request contains test category
            new_req_json = json.loads(new_req.encode('utf-8'))
            req_id = json_lookup('requestId', new_req_json, None)

            resp_res = json_lookup('shortDescription', resp, None)
            if (resp_res[0] != 'Success') and (req_id[0].lower().find('urs') == -1):
                app_log.error('Failed in test response - %s', resp_res)
                break
            app_log.info('Got response for the request')
            json_lookup('availabilityExpireTime', resp, '0')

            app_log.info('Insert new request in DB')
            cur = con.cursor()
            cur.execute('INSERT INTO ' + TBL_REQS_NAME + ' VALUES (?)',
                        (new_req,))
            con.commit()

            app_log.info('Insert new resp in DB')
            upd_data = json.dumps(resp, sort_keys=True)
            hash_obj = hashlib.sha256(upd_data.encode('utf-8'))
            cur = con.cursor()
            cur.execute('INSERT INTO ' + TBL_RESPS_NAME + ' values ( ?, ?)',
                        [upd_data, hash_obj.hexdigest()])
            con.commit()
    con.close()
    return AFC_OK


def dump_table(conn, tbl_name, out_file):
    app_log.debug('%s() %s', inspect.stack()[0][3], tbl_name)
    app_log.info('\n\tDump DB table (%s) to the file (%s).\n',
                 tbl_name, out_file)
    fp_new = ''

    if (out_file) and (out_file != 'split'):
        fp_new = open(out_file, 'w')

    conn.execute('SELECT * FROM ' + tbl_name)
    found_data = conn.fetchall()
    for val in enumerate(found_data):
        if isinstance(fp_new, io.IOBase): 
            fp_new.write('%s\n' % str(val))
        elif out_file == 'split':
            app_log.debug(type(val))
            app_log.debug(val[1][0])
            req_id = json_lookup('requestId', eval(val[1][0]), None)
            fp_test = open(WFA_TEST_DIR + '/' + req_id[0], 'a')
            fp_test.write(val[1][0] + '\n')
            fp_test.close()
        else:
            # Just dump to the console
            app_log.info('%s', val[1])

    if isinstance(fp_new, io.IOBase): 
        fp_new.close()
    

def dump_database(cfg):
    """Dump data from test DB tables"""
    app_log.debug('%s()', inspect.stack()[0][3])
    find_key = ''
    found_tables = []
    out_file = ''

    if not os.path.isfile(cfg['db_filename']):
        app_log.error('Missing DB file %s', cfg['db_filename'])
        return AFC_ERR

    set_dump_db_opts = {
        'wfa': [('test_vectors',), ('test_data',)],
        'req': [('test_vectors',)],
        'resp': [('test_data',)],
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
        app_log.debug('Dump from DB table %s', tbl)
        found_tables.extend(set_dump_db_opts[tbl])
    elif tbl == 'True':
        # Dump all tables if no options provided
        cur.execute("SELECT name FROM sqlite_master WHERE type='table';")
        found_tables = cur.fetchall()

    if tbl == 'wfa':
        out_file = 'split'
        if os.path.exists(WFA_TEST_DIR):
            shutil.rmtree(WFA_TEST_DIR)
        os.mkdir(WFA_TEST_DIR)
    elif isinstance(cfg['outfile'], type(None)):
        app_log.error('Missing output filename.\n')
        return AFC_ERR
    else:
        out_file = cfg['outfile'][0]

    for tbl in enumerate(found_tables):
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

            new_req, resp = cfg._send_recv(dataline)

            # get request id from a request, response not always has it
            # the request contains test category
            new_req_json = json.loads(new_req.encode('utf-8'))
            req_id = json_lookup('requestId', new_req_json, None)
            
            resp_res = json_lookup('shortDescription', resp, None)
            if (resp_res[0] != 'Success') and (req_id[0].lower().find('urs') == -1):
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


def _run_test(cfg, reqs, resps, row_idx, idx):
    """Run tests"""
    app_log.debug('%s() %s (%s)', inspect.stack()[0][3],
                  cfg['tests'], cfg['url_path'])

    test_res = AFC_OK
    while row_idx < idx:
        # Fetch test vector to create request
        req_id = json_lookup('requestId', eval(reqs[row_idx][0]), None)
        before_ts = time.monotonic()
        rawresp = requests.post(cfg['url_path'],
                                data=json.dumps(eval(reqs[row_idx][0])),
                                headers=headers,
                                timeout=None,
                                verify=cfg['verif_post'])
        resp = rawresp.json()
        tm_secs = time.monotonic() - before_ts
        app_log.info('Test done at %.1f secs', tm_secs)
        json_lookup('availabilityExpireTime', resp, '0')
        upd_data = json.dumps(resp, sort_keys=True)
        hash_obj = hashlib.sha256(upd_data.encode('utf-8'))

        if resps[row_idx][1] == hash_obj.hexdigest():
            app_log.info('Test %s is Ok', req_id[0])
        else:
            app_log.error('Test %s (%s) is Fail', test_number, req_id[0])
            app_log.error(upd_data)
            app_log.error(hash_obj.hexdigest())
            test_res = AFC_ERR
        row_idx += 1

        # For saving test results option
        if isinstance(cfg['outfile'], list):
            test_report(cfg['outfile'][0], float(tm_secs),
                        cfg['tests'], req_id[0],
                        ("PASS" if test_res == AFC_OK else "FAIL"), upd_data)
    return test_res


def run_test(cfg):
    """Fetch test vectors from the DB and run tests"""
    app_log.debug('%s() %s (%s)', inspect.stack()[0][3],
                  cfg['tests'], cfg['url_path'])

    if not os.path.isfile(cfg['db_filename']):
        app_log.error('Missing DB file %s', cfg['db_filename'])
        return AFC_ERR

    con = sqlite3.connect(cfg['db_filename'])
    cur = con.cursor()
    cur.execute('SELECT * FROM %s' % TBL_REQS_NAME)
    found_reqs = cur.fetchall()
    cur.execute('SELECT * FROM %s' % TBL_RESPS_NAME)
    found_resps = cur.fetchall()
    con.close()

    if isinstance(cfg['tests'], type(None)):
        found_range = 0
    else:
        found_range = cfg['tests'].split(',')

    row_idx = 0
    if found_range != 0:
        for i in found_range:
            test_idx = int(i)
            row_idx = test_idx - 1
            app_log.info('Prepare to run test number - %d', test_idx)
            _run_test(cfg, found_reqs, found_resps, row_idx, test_idx)
    else:
        found_range = len(found_reqs)
        app_log.info('Prepare to run number of tests - %d',
                     found_range - row_idx)
        _run_test(cfg, found_reqs, found_resps, row_idx, found_range)


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


execution_map = {
    'run' : run_test,
    'dry_run' : dry_run_test,
    'exp_adm_cfg': export_admin_config,
    'add_reqs' : add_reqs,
    'dump_db': dump_database,
    'parse_tests': parse_tests,
    'ver' : get_version
}


def make_arg_parser():
    """Define command line options"""
    args_parser = argparse.ArgumentParser(
        epilog=__doc__.strip(),
        formatter_class=argparse.RawTextHelpFormatter)

    args_parser.add_argument('--addr', nargs=1, type=str,
                         help="<address> - set AFC Server address.\n")
    args_parser.add_argument('--prot', nargs='?', choices=['htttps', 'http'],
                         default='https',
                         help="<http|https> - set connection protocol.\n")
    args_parser.add_argument('--port', nargs='?', default='443',
                         type=int,
                         help="<port> - set connection port.\n")
    args_parser.add_argument('--conn_type', nargs='?',
                         choices=['sync', 'async'], default='sync',
                         help="<sync|async> - set connection to be "
                              "synchronous or asynchronous.\n")
    args_parser.add_argument('--conn_tm', nargs='?', default=None, type=int,
                         help="<secs> - set timeout for asynchronous"
                              "connection.\n")
    args_parser.add_argument('--verif_post', action='store_false',
                         help="<verif_post> - verify SSL on post request.\n")
    args_parser.add_argument('--outfile', nargs=1, type=str,
                         help="<filename> - set filename for test "
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
                         "logging level.\n")
    args_parser.add_argument('--tests', nargs='?',
                         help="<N1,...> - set single or group of tests "
                         "to run.\n")
    args_parser.add_argument('--table', nargs=1, type=str,
                         help="<wfa|req|resp|ap|cfg|user> - set "
                         "database table name.\n")
    args_parser.add_argument('--test_id',
                         default='all',
                         help="WFA test identifier, for example "
                         "srs, urs, fsp, ibp, sip, etc\n")

    args_parser.add_argument('--cmd', choices=execution_map.keys(), nargs='?')

    return args_parser


def prepare_args(parser, cfg):
    """Prepare required parameters"""
    cfg.update(vars(parser.parse_args()))

    if isinstance(cfg['addr'], list):
        # set URL if protocol is not the default 
        if cfg['prot'] != AFC_PROT_NAME:
            cfg['url_path'] = cfg['prot'] + '://'

        cfg['url_path'] += str(cfg['addr'][0]) + ':' + str(cfg['port']) +\
                           AFC_URL_SUFFIX +\
                           ('dbg' if cfg['debug'] else 'nodbg') +\
                           '_' + ('gui' if cfg['gui'] else 'nogui') +\
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
    prepare_args(parser, test_cfg)

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
