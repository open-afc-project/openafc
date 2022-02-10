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
    ./afc_tests.py --cfg addr=<address> log=debug --test a

EXAMPLES

1. Configure adrress and run all tests
    ./afc_tests.py --cfg addr="1.2.3.4" --test r

2. Configure adrress and log level, run test number 2
    ./afc_tests.py --test r=2 --cfg addr="1.2.3.4"

3. Provide SQl file to add new test vector
    ./afc_tests.py --cfg addr="1.2.3.4" --db a=test_vector.sql
"""

import argparse
import logging
import sys
import os
import shutil
import requests
import json
import ast
import random
import pprint
import sqlite3
import hashlib


AFC_TEST_REQS="afc_test_reqs.txt"
AFC_TEST_REQS_TMP="afc_test_reqs_tmp.txt"
AFC_INP_DB="afc_input.sqlite3"
AFC_TEST_REQS_SQL="add_vect_0_6.sql"
AFC_TEST_RESPS="afc_test_resps.txt"
AFC_TEST_RESPS_TMP="afc_test_resps_tmp.txt"
AFC_URL_SUFFIX='/fbrat/ap-afc/0.6/availableSpectrumInquiry'
headers = {'content-type': 'application/json'}
tbl_reqs_name = "test_vect_0_6"
tbl_resps_name = "test_data_0_6"


app_log = logging.getLogger(__name__)


class AfcTester():
    def __init__(self):
        self.addr = ''
        self.port = 443
        self.url_path = 'https://'
        self.log_level = logging.INFO

    def set_cfg(self, params):
        if not params:
            app_log.debug('%s() Missing parameters', sys._getframe().f_code.co_name)
            return True
        if 'log' in params:
            self.log_level = params['log'].upper()
            app_log.setLevel(self.log_level)
        if 'addr' in params:
            self.addr = params['addr']
        if 'port' in params:
            self.port = params['port']
        self.url_path += self.addr+':'+str(self.port)+AFC_URL_SUFFIX
        return True

    def set_db(self, params):
        if not params:
            app_log.debug('%s() Missing parameters', sys._getframe().f_code.co_name)
            return True
        set_db_opts = {
            'p': prep_db,
            'a': add_reqs,
            'm': make_db,
            'd': dump_db
        }
        for k in params:
            set_db_opts[k](self, params[k])
        return True

    def run_test(self, params):
        if not params:
            app_log.debug('%s() Missing parameters', sys._getframe().f_code.co_name)
            return True
        set_run_opts = {
            'r': _run_test,
            'a': _run_acquisition
        }
        for k in params:
            if not set_run_opts[k](self.url_path, params[k]):
                return False
        return True


class ParseDict(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        d = getattr(namespace, self.dest) or {}
        if values:
            for item in values:
                split_items = item.split("=", 1)
                key = split_items[0].strip()
                try:
                    d[key] = split_items[1]
                except IndexError:
                    d[key] = "True"
        setattr(namespace, self.dest, d)


def json_search(json_keys, json_obj, val):
    found = {}
    if type(json_keys) != type([]):
        json_keys = [json_keys]

    if type(json_obj) == type(dict()):
        for needle in json_keys:
            if needle in json_obj.keys():
                found[needle] = json_obj[needle]
                if val:
                    json_obj[needle] = val
            elif len(json_obj.keys()) > 0:
                for key in json_obj.keys():
                    result = json_search(needle, json_obj[key], val)
                    if result:
                        for k, v in result.items():
                            found[k] = v
    elif type(json_obj) == type([]):
        for node in json_obj:
            result = json_search(json_keys, node, val)
            if result:
                for k, v in result.items():
                    found[k] = v
    return found


def make_arg_parser():
    args_parser = argparse.ArgumentParser(
        epilog=__doc__.strip(),
        formatter_class=argparse.RawDescriptionHelpFormatter)

    args_parser.add_argument('--cfg', metavar='KEY=VALUE',
        nargs='+', action=ParseDict,
        help='<a=<ip address|hostname>>, [p=<port>], '
             '[log=<info|debug|warn|error|critical>]')
    args_parser.add_argument('--db', metavar='KEY,KEY=VALUE',
        nargs='+', action=ParseDict,
        help='<a[=filename]> - add new request record,'
             '<m>            - make database from text files,'
             '<d>            - dump all DB tables')
    args_parser.add_argument('--test', metavar='KEY=VALUE',
        nargs='+', action=ParseDict,
        help='<r=[number]>  - run number of tests (0 - all cases)'
             '<a>  - response aquisition for all tests')

    return args_parser


def make_db(self, opt):
    app_log.debug('%s()', sys._getframe().f_code.co_name)
    if not os.path.isfile(AFC_TEST_REQS):
        app_log.error('Missing raw test data file %s', AFC_TEST_REQS)
        return False

    if os.path.isfile(AFC_INP_DB):
        os.remove(AFC_INP_DB)

    app_log.info('Create DB tables (%s, %s) from source files',
        tbl_reqs_name, tbl_resps_name)
    cnt_idx = 0
    con = sqlite3.connect(AFC_INP_DB)
    cur = con.cursor()
    cur.execute("create table "+tbl_reqs_name+" (data json)")
    with open(AFC_TEST_REQS, 'r') as fp_test:
        while True:
            dataline = fp_test.readline()
            if not dataline:
                break
            cnt_idx += 1
            cur.execute("INSERT INTO "+tbl_reqs_name+" VALUES (?)",
                (dataline,))
            con.commit()

    cnt_idx = 0
    cur.execute("CREATE TABLE "+tbl_resps_name+" (data json, hash varchar(255))")
    with open(AFC_TEST_RESPS, 'r') as fp_test:
        while True:
            dataline = fp_test.readline()
            if not dataline:
                break
            cnt_idx += 1
            json_obj = json.loads(dataline.encode('utf-8'))
            json_search('availabilityExpireTime', json_obj, '0')
            upd_data = json.dumps(json_obj, sort_keys=True)
            hash_obj = hashlib.sha256(upd_data.encode('utf-8'))
            cur.execute("INSERT INTO "+tbl_resps_name+" values (?, ?)",
                [upd_data, hash_obj.hexdigest()])
            con.commit()
    con.close()
    return True


def prep_db(self, opt):
    """Prepare DB source files"""
    app_log.debug('%s()', sys._getframe().f_code.co_name)
    if not os.path.isfile(AFC_TEST_REQS):
        app_log.error('Missing raw test data file %s', AFC_TEST_REQS)
        return False

    cnt_idx = 0
    fp_tmp = open(AFC_TEST_REQS_TMP, 'w')
    with open(AFC_TEST_REQS, 'r') as fp_test:
        while True:
            dataline = fp_test.readline()
            if not dataline:
                break
            cnt_idx += 1
            json_obj = json.loads(dataline.encode('utf-8'))
            json_search('availabilityExpireTime', json_obj, '0')
            upd_data = json.dumps(json_obj, sort_keys=True)
            fp_tmp.write(upd_data+'\n')
    fp_tmp.close()

    cnt_idx = 0
    fp_tmp = open(AFC_TEST_RESPS_TMP, 'w')
    with open(AFC_TEST_RESPS, 'r') as fp_test:
        while True:
            dataline = fp_test.readline()
            if not dataline:
                break
            cnt_idx += 1
            json_obj = json.loads(dataline.encode('utf-8'))
            json_search('availabilityExpireTime', json_obj, '0')
            upd_data = json.dumps(json_obj, sort_keys=True)
            fp_tmp.write(upd_data+'\n')
    fp_tmp.close()
    return True


def add_reqs(self, opt):
    """Prepare DB source files"""
    app_log.debug('%s()', sys._getframe().f_code.co_name)
    filename = AFC_TEST_REQS_SQL
    if opt != 'True':
        filename = opt
    app_log.debug(filename)

    if not os.path.isfile(filename):
        app_log.error('Missing raw test data file %s', filename)
        return False

    con = sqlite3.connect(AFC_INP_DB)
    cur = con.cursor()
    with open(filename, 'r') as fp_test:
        sqldata = fp_test.read()
        cur.executescript(sqldata)
        cur.execute("SELECT * FROM "+tbl_reqs_name+" ORDER BY rowid DESC LIMIT 1")
        last_record=cur.fetchall()
        app_log.debug(last_record[0][0])
        app_log.debug(type(list(last_record)))
        rawresp = requests.post(
                  self.url_path, data=json.dumps(eval(last_record[0][0])), 
                  headers=headers, timeout=None, verify=False)
        resp = rawresp.json()
        json_search('availabilityExpireTime', resp, '0')
        upd_data = json.dumps(resp, sort_keys=True)
        hash_obj = hashlib.sha256(upd_data.encode('utf-8'))
        cur.execute("INSERT INTO "+tbl_resps_name+" values ( ?, ?)",
            [upd_data, hash_obj.hexdigest()])
        con.commit()
    con.close()
    return True


def dump_db(self,opt):
    """Ful dump from DB tables"""
    app_log.debug('%s()', sys._getframe().f_code.co_name)
    if not os.path.isfile(AFC_INP_DB):
        app_log.debug('Missing DB file %s', AFC_INP_DB)

    con = sqlite3.connect(AFC_INP_DB)
    cur = con.cursor()
    cur.execute("SELECT name FROM sqlite_master WHERE type='table';")
    found_tables=cur.fetchall()
    app_log.info('\nDump DB table - %s\n', found_tables[0][0])
    cur.execute("SELECT * FROM "+found_tables[0][0])
    found_data=cur.fetchall()
    for row_idx in range(len(found_data)):
        for in_idx in range(len(found_data[row_idx])):
            app_log.info('%s', found_data[row_idx][in_idx])
    app_log.info('\nDump DB table - %s\n', found_tables[1][0])
    cur.execute("SELECT * FROM "+found_tables[1][0])
    found_data=cur.fetchall()
    for row_idx in range(len(found_data)):
        for in_idx in range(len(found_data[row_idx])):
            app_log.info('%s', found_data[row_idx][in_idx])
    con.close()


def _run_acquisition(url_cfg, test_number):
    """Fetch test vectors from the DB, run tests and refill responses in the DB"""
    app_log.debug('%s()', sys._getframe().f_code.co_name)

    if not os.path.isfile(AFC_INP_DB):
        app_log.debug('Missing DB file %s', AFC_INP_DB)

    con = sqlite3.connect(AFC_INP_DB)
    cur = con.cursor()
    cur.execute("SELECT * FROM %s" % tbl_reqs_name)
    found_reqs=cur.fetchall()
    cur.execute("DROP TABLE "+tbl_resps_name)
    cur.execute("CREATE TABLE "+tbl_resps_name+" (data json, hash varchar(255))")

    row_idx = 0
    found_range = len(found_reqs)
    app_log.info('Acquisition to number of tests - %d', found_range-row_idx)

    while row_idx < found_range:
        # Fetch test vector to create request
        req_id = json_search('requestId', eval(found_reqs[row_idx][1]), None)
        rawresp = requests.post(
                  url_cfg, data=json.dumps(eval(found_reqs[row_idx][1])), 
                  headers=headers, timeout=None, verify=False)
        resp = rawresp.json()
        json_search('availabilityExpireTime', resp, '0')
        upd_data = json.dumps(resp, sort_keys=True)
        hash_obj = hashlib.sha256(upd_data.encode('utf-8'))
        cur.execute("INSERT INTO "+tbl_resps_name+" values ( ?, ?)",
            [upd_data, hash_obj.hexdigest()])
        con.commit()
        row_idx += 1
    con.close()
    return True


def _run_test(url_cfg, test_number):
    """Fetch test vectors from the DB and run tests"""
    app_log.debug('%s()', sys._getframe().f_code.co_name)
    if test_number == 'True':
        found_range = 0
    else:
        found_range = int(test_number)

    if not os.path.isfile(AFC_INP_DB):
        app_log.debug('Missing DB file %s', AFC_INP_DB)

    headers = {'content-type': 'application/json'}

    con = sqlite3.connect(AFC_INP_DB)
    cur = con.cursor()
    cur.execute("SELECT * FROM %s" % tbl_reqs_name)
    found_reqs=cur.fetchall()
    cur.execute("SELECT * FROM %s" % tbl_resps_name)
    found_resps=cur.fetchall()
    con.close()

    row_idx = 0
    if found_range != 0:
        row_idx = found_range-1
        app_log.info('Prepare to run test number - %d', found_range)
    else:
        found_range = len(found_reqs)
        app_log.info('Prepare to run number of tests - %d', found_range-row_idx)

    while row_idx < found_range:
        # Fetch test vector to create request
        req_id = json_search('requestId', eval(found_reqs[row_idx][0]), None)
        rawresp = requests.post(
                  url_cfg, data=json.dumps(eval(found_reqs[row_idx][0])), 
                  headers=headers, timeout=None, verify=False)
                  #headers=headers, timeout=None, verify="./cacert.pem")
        resp = rawresp.json()
        json_search('availabilityExpireTime', resp, '0')
        upd_data = json.dumps(resp, sort_keys=True)
        hash_obj = hashlib.sha256(upd_data.encode('utf-8'))

        if found_resps[row_idx][1] == hash_obj.hexdigest():
            app_log.info('Test %s is Ok', req_id.get('requestId'))
        else:
            app_log.error('Test %s is Fail', req_id.get('requestId'))
            app_log.error(upd_data)
            app_log.error(hash_obj.hexdigest())
        row_idx += 1


def main():
    app_log.setLevel(logging.INFO)
    console_log = logging.StreamHandler()
    fmtr = logging.Formatter('{levelname}: {message}')
    console_log.setFormatter(logging.Formatter('%(levelname)s: %(message)s'))
    app_log.addHandler(console_log)
    tester = AfcTester()

    call_opts = {
        'cfg':  tester.set_cfg,
        'db':   tester.set_db,
        'test': tester.run_test
    }
    parser = make_arg_parser()
    prms = vars(parser.parse_args())
    for item in range(len(prms)):
        if not call_opts[list(prms)[item]](prms[list(prms)[item]]):
            break
    sys.exit()


if __name__ == "__main__":
    main()
