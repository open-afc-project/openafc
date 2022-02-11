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

1. Provide test file with new requests to add into DB
    ./afc_tests.py --cfg addr="1.2.3.4" --db a=test_vector.txt

2. Configure adrress and run all tests
    ./afc_tests.py --cfg addr="1.2.3.4" --test r

3. Configure adrress and log level, run test number 2
    ./afc_tests.py --test r=2 --cfg addr="1.2.3.4"
"""

import argparse
import hashlib
import inspect
import json
import logging
import os
import requests
import sqlite3
import sys


ADD_AFC_TEST_REQS = 'add_test_req.txt'
AFC_URL_SUFFIX = '/fbrat/ap-afc/0.6/availableSpectrumInquiry'
headers = {'content-type': 'application/json'}
TBL_REQS_NAME = 'test_vect_0_6'
TBL_RESPS_NAME = 'test_data_0_6'


app_log = logging.getLogger(__name__)


class AfcTester:
    """Provide tester API"""
    def __init__(self):
        self.addr = ''
        self.port = 443
        self.url_path = 'https://'
        self.log_level = logging.INFO
        self.db_filename = 'afc_input.sqlite3'

    def run(self, opts):
        """Main entry to find and execute commands"""
        _call_opts = {
            'cfg': self._set_cfg,
            'db': self._set_db,
            'test': self._run_test
        }
        for item in range(len(opts)):
            if not _call_opts[list(opts)[item]](opts[list(opts)[item]]):
                return False
            app_log.debug('%s() Item %d done',
                          inspect.stack()[0][3], item)
        return True

    def _set_cfg(self, params):
        """Execute configuration rapameters"""
        if not params:
            return True
        if 'log' in params:
            self.log_level = params['log'].upper()
            app_log.setLevel(self.log_level)
        if 'addr' in params:
            self.addr = params['addr']
        if 'port' in params:
            self.port = params['port']
        self.url_path += self.addr + ':' + str(self.port) + AFC_URL_SUFFIX
        return True

    def _set_db(self, params):
        """Execute DB related rapameters"""
        if not params:
            return True
        set_db_opts = {
            'a': add_reqs,
            'd': dump_db
        }
        for k in params:
            if not set_db_opts[k](self, params[k]):
                return False
        return True

    def _run_test(self, params):
        """Execute test related rapameters"""
        if not params:
            return True
        set_run_opts = {
            'r': start_test,
            'a': start_acquisition
        }
        for k in params:
            if not set_run_opts[k](self, params[k]):
                return False
        return True


class ParseDict(argparse.Action):
    """Parse command line options"""
    def __call__(self, parser, namespace, values, option_string=None):
        d = getattr(namespace, self.dest) or {}
        if values:
            for item in values:
                split_items = item.split('=', 1)
                key = split_items[0].strip()
                try:
                    d[key] = split_items[1]
                except IndexError:
                    d[key] = 'True'
        setattr(namespace, self.dest, d)


def json_search(json_keys, json_obj, val):
    """Loookup for key in json and change it value if requireed"""
    found = {}
    if not isinstance(json_keys, list):
        json_keys = [json_keys]

    if isinstance(json_obj, dict):
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
    elif isinstance(json_obj, list):
        for node in json_obj:
            result = json_search(json_keys, node, val)
            if result:
                for k, v in result.items():
                    found[k] = v
    return found


def make_arg_parser():
    """Define command line options"""
    args_parser = argparse.ArgumentParser(
        epilog=__doc__.strip(),
        formatter_class=argparse.RawDescriptionHelpFormatter)

    args_parser.add_argument('--cfg', metavar='KEY=VALUE',
                             nargs='+', action=ParseDict,
                             help='<a=<ip address|hostname>>, [p=<port>], '
                             '[log=<info|debug|warn|error|critical>]')
    args_parser.add_argument('--db', metavar='KEY,KEY=VALUE',
                             nargs='+', action=ParseDict, help='<a[=filename]>'
                             ' - add new request record, '
                             '<d> - dump all DB tables')
    args_parser.add_argument('--test', metavar='KEY=VALUE',
                             nargs='+', action=ParseDict,
                             help='<r=[number]>  - run number of tests '
                             '(0 - all cases)'
                             '<a>  - response aquisition for all tests')
    return args_parser


def make_db(filename):
    """Create DB file only with schema"""
    app_log.debug('%s()', inspect.stack()[0][3])
    if os.path.isfile(filename):
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


def add_reqs(self, opt):
    """Prepare DB source files"""
    app_log.debug('%s()', inspect.stack()[0][3])
    filename = ADD_AFC_TEST_REQS
    if opt != 'True':
        filename = opt

    if not os.path.isfile(filename):
        app_log.error('Missing raw test data file %s', filename)
        return False

    if not make_db(self.db_filename):
        return False

    con = sqlite3.connect(self.db_filename)
    with open(filename, 'r') as fp_test:
        while True:
            dataline = fp_test.readline()
            if not dataline:
                break
            new_req_json = json.loads(dataline.encode('utf-8'))
            new_req = json.dumps(new_req_json, sort_keys=True)
            rawresp = requests.post(
                self.url_path, data=new_req,
                headers=headers, timeout=None, verify=False)

            resp = rawresp.json()
            resp_res = json_search('shortDescription', resp, None)
            if resp_res['shortDescription'] != 'success':
                break
            app_log.info('Got response for the request')
            json_search('availabilityExpireTime', resp, '0')

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
    app_log.info('DB is closed 1')
    con.close()
    app_log.info('DB is closed 2')
    return True


def dump_db(self, opt):
    """Ful dump from DB tables"""
    app_log.debug('%s() %s', inspect.stack()[0][3], opt)

    if not os.path.isfile(self.db_filename):
        app_log.error('Missing DB file %s', self.db_filename)
        return False

    con = sqlite3.connect(self.db_filename)
    cur = con.cursor()
    cur.execute("SELECT name FROM sqlite_master WHERE type='table';")
    found_tables = cur.fetchall()
    app_log.info('\n\tDump DB table - %s\n', found_tables[0][0])
    cur.execute('SELECT * FROM ' + found_tables[0][0])
    found_data = cur.fetchall()
    for val in enumerate(found_data):
        for in_val in val:
            app_log.info('%s', in_val)
    app_log.info('\n\tDump DB table - %s\n', found_tables[1][0])
    cur.execute('SELECT * FROM ' + found_tables[1][0])
    found_data = cur.fetchall()
    for val in enumerate(found_data):
        for in_val in val:
            app_log.info('%s', in_val)
    con.close()
    return True


def start_acquisition(self, test_number):
    """
    Fetch test vectors from the DB, run tests and refill responses in the DB
    """
    app_log.debug('%s() %s', inspect.stack()[0][3], test_number)

    if not os.path.isfile(self.db_filename):
        app_log.error('Missing DB file %s', self.db_filename)
        return False

    con = sqlite3.connect(self.db_filename)
    cur = con.cursor()
    cur.execute('SELECT * FROM %s' % TBL_REQS_NAME)
    found_reqs = cur.fetchall()
    cur.execute('DROP TABLE ' + TBL_RESPS_NAME)
    cur.execute('CREATE TABLE ' + TBL_RESPS_NAME +
                ' (data json, hash varchar(255))')

    row_idx = 0
    found_range = len(found_reqs)
    app_log.info('Acquisition to number of tests - %d', found_range - row_idx)

    while row_idx < found_range:
        # Fetch test vector to create request
        rawresp = requests.post(self.url_path,
                                data=json.dumps(eval(found_reqs[row_idx][1])),
                                headers=headers, timeout=None, verify=False)
        resp = rawresp.json()
        json_search('availabilityExpireTime', resp, '0')
        upd_data = json.dumps(resp, sort_keys=True)
        hash_obj = hashlib.sha256(upd_data.encode('utf-8'))
        cur.execute('INSERT INTO ' + TBL_RESPS_NAME + ' values ( ?, ?)',
                    [upd_data, hash_obj.hexdigest()])
        con.commit()
        row_idx += 1
    con.close()
    return True


def start_test(self, test_number):
    """Fetch test vectors from the DB and run tests"""
    app_log.debug('%s()', inspect.stack()[0][3])
    if test_number == 'True':
        found_range = 0
    else:
        found_range = int(test_number)

    if not os.path.isfile(self.db_filename):
        app_log.error('Missing DB file %s', self.db_filename)
        return False

    con = sqlite3.connect(self.db_filename)
    cur = con.cursor()
    cur.execute('SELECT * FROM %s' % TBL_REQS_NAME)
    found_reqs = cur.fetchall()
    cur.execute('SELECT * FROM %s' % TBL_RESPS_NAME)
    found_resps = cur.fetchall()
    con.close()

    row_idx = 0
    if found_range != 0:
        row_idx = found_range - 1
        app_log.info('Prepare to run test number - %d', found_range)
    else:
        found_range = len(found_reqs)
        app_log.info('Prepare to run number of tests - %d',
                     found_range - row_idx)

    while row_idx < found_range:
        # Fetch test vector to create request
        req_id = json_search('requestId', eval(found_reqs[row_idx][0]), None)
        rawresp = requests.post(self.url_path,
                                data=json.dumps(eval(found_reqs[row_idx][0])),
                                headers=headers, timeout=None, verify=False)
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
    """Main function of the utility"""
    app_log.setLevel(logging.INFO)
    console_log = logging.StreamHandler()
    console_log.setFormatter(logging.Formatter('%(levelname)s: %(message)s'))
    app_log.addHandler(console_log)
    tester = AfcTester()

    parser = make_arg_parser()
    prms = vars(parser.parse_args())
    tester.run(prms)
    sys.exit()


if __name__ == '__main__':
    main()

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
