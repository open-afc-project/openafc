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
    ./afc_tests.py --cfg addr=1.2.3.4 --db a=test_vector.txt

2. Configure adrress and run all tests
    ./afc_tests.py --cfg addr=1.2.3.4 --test r

3. Configure adrress and log level, run test number 2
    ./afc_tests.py --test r=2 --cfg addr=1.2.3.4
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
from _version import __version__


ADD_AFC_TEST_REQS = 'add_test_req.txt'
AFC_URL_SUFFIX = '/fbrat/ap-afc/1.1/availableSpectrumInquiry'
headers = {'content-type': 'application/json'}
TBL_REQS_NAME = 'test_vectors'
TBL_RESPS_NAME = 'test_data'

AFC_TEST_DB_FILENAME = 'afc_input.sqlite3'
TBL_USERS_NAME = 'user_config'
TBL_AFC_CFG_NAME = 'afc_config'
TBL_AP_CFG_NAME = 'ap_config'

TEST_STATUS = { 'Ok':0, 'Error':1 }

app_log = logging.getLogger(__name__)


class AfcTester:
    """Provide tester API"""
    def __init__(self):
        self.addr = ''
        self.port = 443
        self.url_path = 'https://'
        self.log_level = logging.INFO
        self.db_filename = AFC_TEST_DB_FILENAME
        self.resp = ''

    def run(self, opts):
        """Main entry to find and execute commands"""
        app_log.debug('AfcTester.run()')
        _call_opts = {
            'cfg': self._set_cfg,
            'db': self._set_db,
            'test': self._run_test,
            'ver': self._get_version
        }
        for item in range(len(opts)):
            app_log.debug('AfcTester.run() Item %d , %s', item, list(opts)[item])
            if not _call_opts[list(opts)[item]](opts[list(opts)[item]]):
                return False
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
            'd': dump_db,
            'e': export_admin_cfg,
            'r': run_reqs
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

    def _get_version(self, params):
        """Get AFC test utility version"""
        app_log.info('AFC Test utility version %s', __version__)
        app_log.info('AFC Test db hash %s', get_md5(AFC_TEST_DB_FILENAME))
        return True

    def _send_recv(self, params):
        """Run AFC test and wait for respons"""
        new_req_json = json.loads(params.encode('utf-8'))
        new_req = json.dumps(new_req_json, sort_keys=True)
        app_log.debug(new_req)
        rawresp = requests.post(
            self.url_path, data=new_req,
            headers=headers, timeout=None)
        resp = rawresp.json()
        return new_req, resp


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
                             nargs='+', action=ParseDict,
                             help='<a[=filename]> - add new request record,'
                                  '<d> - dump all DB tables,'
                                  '<e> - export admin config,'
                                  '<r[=filename]> - run request from file')
    args_parser.add_argument('--test', metavar='KEY,KEY=VALUE',
                             nargs='+', action=ParseDict,
                             help='<r=[number]>  - run number of tests '
                             '(0 - all cases)'
                             '<a>  - response aquisition for all tests')
    args_parser.add_argument('-v', '--ver', action = 'store_true',
                             help='get utility version')
    return args_parser


def export_admin_cfg(self, opt):
    """Export admin server configuration"""
    app_log.debug('%s() %s', inspect.stack()[0][3], opt)

    filename = opt
    con = sqlite3.connect(self.db_filename)
    cur = con.cursor()
    cur.execute('SELECT COUNT(*) FROM ' + TBL_USERS_NAME)
    found_rcds = cur.fetchall()
    with open(filename, 'w') as fp_exp:
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
    app_log.info('Server admin config exported to %s', opt)
    return True


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

            new_req, resp = self._send_recv(dataline)

            resp_res = json_lookup('shortDescription', resp, None)
            if resp_res[0] != 'Success':
                app_log.error('Failed in test response - %s', resp_res)
                app_log.debug(resp)
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
    return True


def run_reqs(self, opt):
    """Run one or more requests from provided file"""
    app_log.debug('%s()', inspect.stack()[0][3])
    filename = ADD_AFC_TEST_REQS
    if opt != 'True':
        filename = opt

    if not os.path.isfile(filename):
        app_log.error('Missing raw test data file %s', filename)
        return False

    with open(filename, 'r') as fp_test:
        while True:
            dataline = fp_test.readline()
            if not dataline:
                break

            new_req, resp = self._send_recv(dataline)

            resp_res = json_lookup('shortDescription', resp, None)
            if resp_res[0] != 'Success':
                app_log.error('Failed in test response - %s', resp_res)
                app_log.debug(resp)
                break
            app_log.info('Got response for the request')

            json_lookup('availabilityExpireTime', resp, '0')
            upd_data = json.dumps(resp, sort_keys=True)
            hash_obj = hashlib.sha256(upd_data.encode('utf-8'))
    return True


def dump_db(self, opt):
    """Ful dump from DB tables"""
    app_log.debug('%s() %s', inspect.stack()[0][3], opt)
    find_key = ''
    if opt != 'True':
        find_key = opt

    if not os.path.isfile(self.db_filename):
        app_log.error('Missing DB file %s', self.db_filename)
        return False

    con = sqlite3.connect(self.db_filename)
    cur = con.cursor()
    cur.execute("SELECT name FROM sqlite_master WHERE type='table';")
    found_tables = cur.fetchall()
    for tbl in enumerate(found_tables):
        app_log.info('\n\n\tDump DB table - %s', tbl[1][0])
        cur.execute('SELECT * FROM ' + tbl[1][0])
        found_data = cur.fetchall()
        for val in enumerate(found_data):
            for in_val in val:
                if len(find_key) > 0:
                    if isinstance(in_val, tuple):
                        new_json = json.loads(in_val[0].encode('utf-8'))
                        found_vals = json_lookup(find_key, new_json, '111')
                        app_log.info(found_vals[0])
                else:
                    app_log.info('%s', in_val)
    con.close()
    app_log.info('\n\n\tDump DB table - %s')
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
    try:
        cur.execute('DROP TABLE ' + TBL_RESPS_NAME)
    except Exception as OperationalError:
        app_log.debug('Missing table %s', TBL_RESPS_NAME)
    cur.execute('CREATE TABLE ' + TBL_RESPS_NAME +
                ' (data json, hash varchar(255))')

    row_idx = 0
    found_range = len(found_reqs)
    if len(found_reqs) == 0:
        app_log.error('Missing request records')
        return False
    app_log.info('Acquisition to number of tests - %d', found_range - row_idx)

    while row_idx < found_range:
        # Fetch test vector to create request
        rawresp = requests.post(self.url_path,
                                data=json.dumps(eval(found_reqs[row_idx][0])),
                                headers=headers, timeout=None)
        resp = rawresp.json()
        json_lookup('availabilityExpireTime', resp, '0')
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

    test_res = TEST_STATUS['Ok']
    while row_idx < found_range:
        # Fetch test vector to create request
        req_id = json_lookup('requestId', eval(found_reqs[row_idx][0]), None)
        rawresp = requests.post(self.url_path,
                                data=json.dumps(eval(found_reqs[row_idx][0])),
                                headers=headers, timeout=None)
        resp = rawresp.json()
        json_lookup('availabilityExpireTime', resp, '0')
        upd_data = json.dumps(resp, sort_keys=True)
        hash_obj = hashlib.sha256(upd_data.encode('utf-8'))

        if found_resps[row_idx][1] == hash_obj.hexdigest():
            app_log.info('Test %s is Ok', req_id[0])
        else:
            app_log.error('Test %s is Fail', req_id[0])
            app_log.error(upd_data)
            app_log.error(hash_obj.hexdigest())
            test_res = TEST_STATUS['Error']
        row_idx += 1
    return test_res


def main():
    """Main function of the utility"""
    app_log.setLevel(logging.INFO)
    console_log = logging.StreamHandler()
    console_log.setFormatter(logging.Formatter('%(levelname)s: %(message)s'))
    app_log.addHandler(console_log)
    tester = AfcTester()

    parser = make_arg_parser()
    prms = vars(parser.parse_args())
    res = tester.run(prms)
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
