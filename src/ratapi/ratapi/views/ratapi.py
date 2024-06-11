#
# This Python file uses the following encoding: utf-8
#
# Portions copyright (C) 2021 Broadcom.
# All rights reserved. The term “Broadcom” refers solely
# to the Broadcom Inc. corporate affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which
# is included with this software program.
#
''' The custom REST api for using the web UI and configuring AFC.
'''

import contextlib
import logging
import os
import shutil
import sys
import pkg_resources
import flask
import json
import glob
import re
import inspect
import gevent
import datetime
import requests
import appcfg
import threading
import time
from flask.views import MethodView
import werkzeug.exceptions
from defs import RNTM_OPT_DBG_GUI, RNTM_OPT_DBG
from afc_worker import run
from fst import DataIf
from ncli import MsgPublisher
from hchecks import RmqHealthcheck, ObjstHealthcheck
from ..util import AFCEngineException, require_default_uls, getQueueDirectory
from afcmodels.aaa import User, AccessPointDeny, AFCConfig, MTLS
from afcmodels.base import db
from .auth import auth
from appcfg import ObjstConfig

#: Logger for this module
LOGGER = logging.getLogger(__name__)
LOGGER.setLevel(appcfg.AFC_RATAPI_LOG_LEVEL)

#: All views under this API blueprint
module = flask.Blueprint('ratapi-v1', 'ratapi')
baseRegions = ['US', 'CA', 'BR', 'GB']


def regions():
    return baseRegions + list(map(lambda s: 'DEMO_' + s, baseRegions)) + \
        list(map(lambda s: 'TEST_' + s, baseRegions))


def rulesets():
    return ['US_47_CFR_PART_15_SUBPART_E',
            'CA_RES_DBS-06',
            'BRAZIL_RULESETID',
            'UNITEDKINGDOM_RULESETID'] + list(map(lambda s: 'DEMO_' + s,
                                                  baseRegions)) + list(map(lambda s: 'TEST_' + s,
                                                                           baseRegions))


# after 1.4 use Ruleset ID
def regionStrToRulesetId(region_str):
    """ Input: region_str: regionStr field of the afc config.
        Output: nra
        nra: can match with the NRA field of the AP, e.g. FCC
        Eg. 'USA' => 'FCC'
    """
    map = {
        'DEFAULT': 'US_47_CFR_PART_15_SUBPART_E',
        'US': 'US_47_CFR_PART_15_SUBPART_E',
        'CA': 'CA_RES_DBS-06',
        'BR': 'BRAZIL_RULESETID',
        'GB': 'UNITEDKINGDOM_RULESETID'

    }
    region_str = region_str.upper()
    try:
        # for test and demo
        if region_str.startswith("DEMO_") or region_str.startswith("TEST_"):
            return region_str

        return map[region_str]
    except BaseException:
        raise werkzeug.exceptions.NotFound('Invalid Region %s' % region_str)


def rulesetIdToRegionStr(rulesetId):
    map = {
        'US_47_CFR_PART_15_SUBPART_E': 'US',
        'CA_RES_DBS-06': 'CA',
        'BRAZIL_RULESETID': 'BR',
        'UNITEDKINGDOM_RULESETID': 'GB'

    }
    rulesetId = rulesetId.upper()
    try:
        if rulesetId.startswith("DEMO_") or rulesetId.startswith("TEST_"):
            if rulesetId in rulesets():
                return rulesetId

        return map[rulesetId]
    except BaseException:
        raise werkzeug.exceptions.NotFound('Invalid ruleset %s' % rulesetId)


def build_task(
        dataif,
        request_type,
        task_id,
        hash_val,
        config_path,
        history_dir,
        runtime_opts=RNTM_OPT_DBG_GUI,
        rcache_queue=None,
        request_str=None,
        config_str=None,
        timeout_sec=600):
    """
    Shared logic between PAWS and All other analysis for constructing and async call to run task
    """

    prot, host, port = dataif.getProtocol()
    kwargs = {
        "prot": prot,
        "host": host,
        "port": port,
        "request_type": request_type,
        "task_id": task_id,
        "hash_val": hash_val,
        "config_path": config_path,
        "history_dir": history_dir,
        "runtime_opts": runtime_opts,
        "mntroot": flask.current_app.config['NFS_MOUNT_PATH'],
        "rcache_queue": rcache_queue,
        "request_str": request_str,
        "config_str": config_str,
        "deadline": time.time() + timeout_sec
    }
    LOGGER.debug(f"build_task({kwargs})")
    run.apply_async(kwargs=kwargs)


class GuiConfig(MethodView):
    ''' Allow the web UI to obtain configuration, including resolved URLs.
    '''

    def get(self):
        ''' GET for gui config
        '''
        LOGGER.debug(f"({threading.get_native_id()})"
                     f" {self.__class__.__name__}::{inspect.stack()[0][3]}()"
                     f" {flask.request.cookies}")

        # Figure out the current server version
        try:
            if sys.version_info.major != 3:
                serververs = pkg_resources.require('ratapi')[0].version
            else:
                serververs = pkg_resources.get_distribution('ratapi').version
        except Exception as err:
            LOGGER.error('Failed to fetch server version: {0}'.format(err))
            serververs = 'unknown'

        # TODO: temporary support python2
        if sys.version_info.major != 3:
            from urlparse import urlparse
        else:
            from urllib.parse import urlparse

        u = urlparse(flask.request.url)

        if 'USE_CAPTCHA' in flask.current_app.config and \
                flask.current_app.config['USE_CAPTCHA']:
            about_sitekey = flask.current_app.config['CAPTCHA_SITEKEY']
        else:
            about_sitekey = None

        if flask.current_app.config['OIDC_LOGIN']:
            login_url = flask.url_for('auth.LoginAPI')
            logout_url = flask.url_for('auth.LogoutAPI')
            about_url = flask.url_for('ratapi-v1.About')
            about_login_url = flask.url_for('auth.AboutLoginAPI')
        else:
            login_url = flask.url_for('user.login'),
            logout_url = flask.url_for('user.logout'),
            about_url = None
            about_login_url = None

        # TODO: temporary support python2
        resp = flask.jsonify(
            uls_url=flask.url_for('ratapi-v1.UlsFiles'),
            antenna_url=flask.url_for('ratapi-v1.AntennaFiles'),
            history_url=flask.url_for("ratapi-v1.History0"),
            afcconfig_defaults=flask.url_for(
                'ratapi-v1.AfcConfigFile', filename='default'),
            lidar_bounds=flask.url_for('ratapi-v1.LiDAR_Bounds'),
            ras_bounds=flask.url_for('ratapi-v1.RAS_Bounds'),
            google_apikey=flask.current_app.config['GOOGLE_APIKEY'],
            rat_api_analysis=flask.url_for('ratapi-v1.Phase1Analysis',
                                           request_type='p_request_type'),
            uls_convert_url=flask.url_for(
                'ratapi-v1.UlsDb', uls_file='p_uls_file'),
            status_url=flask.url_for('auth.UserAPI'),
            login_url=login_url,
            logout_url=logout_url,
            admin_url=flask.url_for('admin.User', user_id=-1),
            ap_deny_admin_url=flask.url_for('admin.AccessPointDeny', id=-1),
            cert_id_admin_url=flask.url_for('admin.CertId', id=-1),
            mtls_admin_url=flask.url_for('admin.MTLS', id=-1),
            dr_admin_url=flask.url_for('admin.DeniedRegion', regionStr="XX"),
            rat_afc=flask.url_for('ap-afc.RatAfcSec'),
            about_url=about_url,
            about_login_url=about_login_url,
            about_sitekey=about_sitekey if about_sitekey else None,
            about_csrf=flask.url_for('ratapi-v1.AboutCSRF'),
            app_name=flask.current_app.config['USER_APP_NAME'],
            version=serververs,
        )
        return resp


class HealthCheck(MethodView):

    def get(self):
        '''GET method for HealthCheck'''
        cert_bdl_name = 'certificate/client.bundle.pem'
        bundle_data = ''
        try:
            with DataIf().open(cert_bdl_name) as hfile:
                if hfile.head():
                    LOGGER.debug(f"Cert bundle already exists.")
                    raise
                # create client bundle certificate file if not exists
                for certs in db.session.query(MTLS).all():
                    LOGGER.info(f"{certs.id}")
                    bundle_data += certs.cert
                db.session.commit()  # pylint: disable=no-member
                if len(bundle_data) == 0:
                    LOGGER.debug(f"No certificates stored.")
                    raise
                # write certificates bundle for clients to objst cache.
                hfile.write(bundle_data)

                # note relevant listen peers
                cmd = 'cmd_restart'
                publisher = MsgPublisher(
                    flask.current_app.config['BROKER_URL'],
                    flask.current_app.config['BROKER_EXCH_DISPAT'])
                publisher.publish(cmd)
                publisher.close()
        except BaseException:
            pass

        msg = 'The ' + flask.current_app.config['AFC_APP_TYPE'] + ' is healthy'
        LOGGER.info(f"{msg}")
        return flask.make_response(msg, 200)


def check_rmq(cfg):
    LOGGER.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()"
                 f" {cfg['BROKER_URL']}")
    hconn = RmqHealthcheck(cfg['BROKER_URL'])
    if hconn.healthcheck():
        return 1
    return 0


class ReadinessCheck(MethodView):

    def get(self):
        '''GET method for Readiness Check'''
        LOGGER.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()"
                     f" cfg: {flask.current_app.config}")
        msg = 'The ' + flask.current_app.config['AFC_APP_TYPE'] + ' is'
        objst_chk = ObjstHealthcheck(flask.current_app.config)
        checks = [gevent.spawn(objst_chk.healthcheck),
                  gevent.spawn(check_rmq, flask.current_app.config)]
        gevent.joinall(checks)
        for i in checks:
            if i.value != 0:
                msg += 'not ready'
                return flask.make_response(msg, 503)
        msg += 'ready'
        return flask.make_response(msg, 200)


class ReloadAnalysis(MethodView):

    ACCEPTABLE_FILES = {
        'analysisRequest.json.gz': dict(
            content_type='application/json',
        )
    }

    def _open(self, rel_path, mode, username, user=None):
        ''' Open a configuration file.

        :param rel_path: The specific config name to open.
        :param mode: The file open mode.
        :return: The opened file.
        :rtype: file-like
        need to find the latest file? how to do that? - Glob
        '''
        files = glob.glob(os.path.join(
            flask.current_app.config['HISTORY_DIR'], username + "*"))
        dates = []
        for x in files:

            dateMatch = re.search(
                '\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}.\\d{6}', x)
            if dateMatch:
                date = datetime.datetime.strptime(
                    dateMatch.group(), '%Y-%m-%dT%H:%M:%S.%f')
                dates.append(date)  # dates loaded

        strDate = str(max(dates))
        strDate = strDate.replace(" ", "T")
        fileName = username + '-' + strDate

        LOGGER.debug(os.path.join(
            flask.current_app.config['HISTORY_DIR'], fileName))
        if mode == 'wb' and user is not None and not os.path.exists(
                os.path.join(flask.current_app.config['HISTORY_DIR'], fileName)):
            # create scoped user directory so people don't clash over each
            # others config
            os.mkdir(os.path.join(
                flask.current_app.config['HISTORY_DIR'], fileName))

        config_path = ''
        if user is not None and os.path.exists(os.path.join(
                flask.current_app.config['HISTORY_DIR'], fileName)):
            config_path = os.path.join(
                flask.current_app.config['HISTORY_DIR'], fileName)
        else:
            config_path = os.path.join(
                flask.current_app.config['HISTORY_DIR'], fileName)
        if not os.path.exists(config_path):
            os.makedirs(config_path)

        file_path = os.path.join(config_path, rel_path)
        LOGGER.debug('Opening analysisRequest file "%s"', file_path)
        if not os.path.exists(file_path) and mode != 'wb':
            raise werkzeug.exceptions.NotFound()

        handle = open(file_path, mode)

        if mode == 'wb':
            os.chmod(file_path, 0o666)

        return handle

    def get(self):
        ''' GET method for afc config
        '''
        LOGGER.debug(f"({threading.get_native_id()})"
                     f" {self.__class__}::{inspect.stack()[0][3]}()")
        LOGGER.debug('getting analysisRequest')
        user_id = auth(roles=['AP', 'Analysis', 'Admin'])
        user = User.query.filter_by(id=user_id).first()
        responseObject = {
            'status': 'success',
            'data': {
                'userId': user.id,
                'email': user.email,
                'roles': user.roles,
                'email_confirmed_at': user.email_confirmed_at,
                'active': user.active,
                'firstName': user.first_name,
                'lastName': user.last_name,
            }
        }
        # ensure that webdav is populated with default files
        require_default_uls()
        filename = 'analysisRequest.json.gz'
        if filename not in self.ACCEPTABLE_FILES:
            raise werkzeug.exceptions.NotFound()
        filedesc = self.ACCEPTABLE_FILES[filename]

        resp = flask.make_response()
        with self._open(filename, 'rb', user.email, user_id) as conf_file:
            resp.data = conf_file.read()
        LOGGER.debug(resp.data)
        json_resp = json.loads(resp.data)
        LOGGER.debug(json_resp)
        # has key deviceDesc and deviceDesc.serialNumber == "analysis-ap" => PointAnalysis
        # has key deviceDesc and deviceDesc.serialNumber != "analysis-ap" => Virtual AP
        # has key spacing => HeatMap
        # has key FSID => ExclusionZone
        if ('deviceDesc' in json_resp and json_resp['deviceDesc']
                ['serialNumber'] == "analysis-ap"):
            resp.headers['AnalysisType'] = 'PointAnalysis'

        elif ('deviceDesc' in json_resp and json_resp['deviceDesc']['serialNumber'] != "analysis-ap"):
            resp.headers['AnalysisType'] = 'VirtualAP'

        elif ('key spacing' in json_resp):
            resp.headers['AnalysisType'] = 'HeatMap'

        elif ('FSID' in json_resp):
            resp.headers['AnalysisType'] = 'ExclusionZone'
        else:
            resp.headers['AnalysisType'] = 'None'
        LOGGER.debug(json_resp['deviceDesc']['serialNumber'])
        LOGGER.debug(resp.headers['AnalysisType'])
        # json.dumps(json_resp, resp.data)
        # LOGGER.debug(resp.data)
        resp.content_type = filedesc['content_type']
        return resp


class AfcConfigFile(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def get(self, filename):
        ''' GET method for afc config
        '''
        filename = filename.upper()
        LOGGER.debug('AfcConfigFile.get({})'.format(filename))
        user_id = auth(roles=['AP', 'Analysis', 'Super'])
        # ensure that webdav is populated with default files
        require_default_uls()

        resp = flask.make_response()
        config = AFCConfig.query.filter(
            AFCConfig.config['regionStr'].astext == filename).first()
        if config:
            resp.data = json.dumps(config.config)
            resp.content_type = 'application/json'
            return resp
        else:
            raise werkzeug.exceptions.NotFound()

    def put(self, filename):
        ''' PUT method for afc config
        '''
        import als
        from flask_login import current_user

        user_id = auth(roles=['Super'])
        LOGGER.debug("current user: %s", user_id)

        if flask.request.content_type != 'application/json':
            raise werkzeug.exceptions.UnsupportedMediaType()

        bytes = flask.request.stream.read()
        rcrd = json.loads(bytes)
        filename = rcrd['regionStr'].upper()
        LOGGER.debug('AfcConfigFile.put({})'.format(filename))
        # validate the region string
        regionStrToRulesetId(filename)
        # make sure the config region string is upper case
        rcrd['regionStr'] = filename
        ordered_bytes = json.dumps(rcrd, sort_keys=True)
        try:
            config = AFCConfig.query.filter(
                AFCConfig.config['regionStr'].astext == filename).first()
            if not config:
                config = AFCConfig(rcrd)
                db.session.add(config)
                als.als_json_log('afc_config',
                                 {'action': 'create',
                                  'user': current_user.username,
                                  'region': filename,
                                  'from': flask.request.remote_addr,
                                  'content': ordered_bytes})
            else:
                config.config = rcrd
                config.created = datetime.datetime.now()
                als.als_json_log('afc_config',
                                 {'action': 'update',
                                  'user': current_user.username,
                                  'region': filename,
                                  'from': flask.request.remote_addr,
                                  'content': ordered_bytes})
            db.session.commit()

        except BaseException:
            raise werkzeug.exceptions.NotFound()

        return flask.make_response('AFC configuration file updated', 204)


class AfcRegions(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def get(self):
        ''' GET method for afc config
        '''
        resp = flask.make_response()
        resp.data = ' '.join(regions())
        resp.content_type = 'text/plain'
        return resp


class About(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def get(self):
        ''' GET method for About
        '''

        resp = flask.make_response()
        about_content = "about.html"
        resp.data = flask.render_template(about_content)
        resp.content_type = 'text/html'
        return resp

    def post(self):
        ''' POST method for About
        '''

        from flask import request
        from flask_mail import Mail, Message

        try:
            dest_email = flask.current_app.config['REGISTRATION_DEST_EMAIL']
            dest_pdl_email = flask.current_app.config['REGISTRATION_DEST_PDL_EMAIL']
            src_email = flask.current_app.config['REGISTRATION_SRC_EMAIL']
            approve_link = flask.current_app.config['REGISTRATION_APPROVE_LINK']

            if 'USE_CAPTCHA' in flask.current_app.config and \
                    flask.current_app.config['USE_CAPTCHA']:
                captcha_secret = flask.current_app.config['CAPTCHA_SECRET']
                captcha_verify = flask.current_app.config['CAPTCHA_VERIFY']
            else:
                captcha_secret = None

            bytes = flask.request.stream.read()
            rcrd = json.loads(bytes)
            name = rcrd['name']
            email = rcrd['email']
            org = rcrd['org']

            # verify captcha
            if captcha_secret:
                token = rcrd['token']
                dictToSend = {'secret': captcha_secret,
                              'response': token}
                res = requests.post(captcha_verify, data=dictToSend)

                LOGGER.debug("Got verify response " +
                             str(res.json()["success"]))

                if not res.json()["success"]:
                    return flask.make_response("No valid captcha", 400)

            recipients = [dest_email]
            if dest_pdl_email:
                recipients.append(dest_pdl_email)

            mail = Mail(flask.current_app)
            msg = Message(f"AFC Access Request by {email}",
                          sender=src_email,
                          recipients=recipients)

            msg.body = f'''Name: {name}\nEmail: {email}\nOrg: {org}
Approve request at: {approve_link}'''
            mail.send(msg)
            return flask.make_response(
                f"Thank you {name}. An access request for {email} has been submitted", 204)
        except BaseException:
            raise werkzeug.exceptions.NotFound()


class AboutCSRF(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def get(self):
        ''' GET method for About
        '''
        LOGGER.debug(f"({threading.get_native_id()})"
                     f" {self.__class__.__name__}::{inspect.stack()[0][3]}()")

        resp = flask.make_response()
        about_content = "about_csrf.html"

        resp.data = flask.render_template(about_content)
        resp.content_type = 'text/html'
        return resp


class LiDAR_Bounds(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def _open(self, abs_path, mode, user=None):
        ''' Open a file.

        :param abs_path: The specific config name to open.
        :param mode: The file open mode.
        :return: The opened file.
        :rtype: file-like
        '''

        LOGGER.debug('Opening file "%s"', abs_path)
        if not os.path.exists(abs_path) and mode != 'wb':
            raise werkzeug.exceptions.NotFound()

        handle = open(abs_path, mode)

        return handle

    def get(self):
        ''' GET method for LiDAR_Bounds
        '''
        LOGGER.debug("LiDAR_Bounds.get()")
        user_id = auth(roles=['AP', 'Analysis'])

        import xdg.BaseDirectory

        try:
            resp = flask.make_response()
            datapath = next(xdg.BaseDirectory.load_data_paths(
                'fbrat', 'rat_transfer', 'proc_lidar_2019'))
            full_path = os.path.join(datapath, 'LiDAR_Bounds.json.gz')
            if not os.path.exists(full_path):
                raise werkzeug.exceptions.NotFound(
                    'LiDAR bounds file not found')
            with self._open(full_path, 'rb', user_id) as data_file:
                resp.data = data_file.read()
            resp.content_type = 'application/json'
            resp.content_encoding = 'gzip'
            return resp
        except StopIteration:
            raise werkzeug.exceptions.NotFound('Path not found to file')


class RAS_Bounds(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def _open(self, abs_path, mode, user=None):
        ''' Open a file.

        :param abs_path: The specific config name to open.
        :param mode: The file open mode.
        :return: The opened file.
        :rtype: file-like
        '''

        LOGGER.debug('Opening file "%s"', abs_path)
        if not os.path.exists(abs_path) and mode != 'wb':
            raise werkzeug.exceptions.NotFound()

        handle = open(abs_path, mode)

        return handle

    def get(self):
        ''' GET method for RAS_Bounds
        '''
        LOGGER.debug("RAS_Bounds.get()")
        LOGGER.debug('getting RAS bounds')
        user_id = auth(roles=['AP', 'Analysis'])

        import xdg.BaseDirectory

        try:
            resp = flask.make_response()
            datapath = next(xdg.BaseDirectory.load_data_paths(
                'fbrat', 'rat_transfer', 'proc_lidar_2019'))
            full_path = os.path.join(datapath, 'RAS_ExclusionZone.json')
            if not os.path.exists(full_path):
                raise werkzeug.exceptions.NotFound(
                    'RAS exclusion zone file not found')
            with self._open(full_path, 'rb', user_id) as data_file:
                resp.data = data_file.read()
            resp.content_type = 'application/json'
            # resp.content_encoding = 'gzip'
            return resp
        except StopIteration:
            raise werkzeug.exceptions.NotFound('Path not found to file')


class Phase1Analysis(MethodView):
    ''' Run analysis using AFC engine and display graphical results on map and graphs
    '''

    methods = ['POST']

    def _open(self, abs_path, mode):
        ''' Open a response file.

        :param rel_path: The specific file name to open.

        :return: The opened file.
        :rtype: file-like
        '''

        LOGGER.debug('Attempting to open response file "%s"', abs_path)
        if not os.path.exists(abs_path):
            raise werkzeug.exceptions.InternalServerError(
                description='Your analysis was unable to be processed.')
        return open(abs_path, mode)

    def post(self, request_type):
        ''' Run analysis

            :param request_type: 'PointAnalysis', 'ExclusionZoneAnalysis', or 'HeatmapAnalysis'
        '''
        LOGGER.debug(f"({threading.get_native_id()})"
                     f" {self.__class__.__name__}::{inspect.stack()[0][3]}()")

        user_id = auth(roles=['Analysis'])
        user = User.query.filter_by(id=user_id).first()

        args = flask.request.data
        LOGGER.debug("Running phase 1 analysis with params: %s", args)

        valid_requests = ['PointAnalysis',
                          'ExclusionZoneAnalysis', 'HeatmapAnalysis']
        if request_type not in valid_requests:
            raise werkzeug.exceptions.BadRequest('Invalid request type')
        temp_dir = getQueueDirectory(
            flask.current_app.config['TASK_QUEUE'], request_type)
        request_file_path = os.path.join(temp_dir, 'analysisRequest.json.gz')

        response_file_path = os.path.join(
            temp_dir, 'analysisResponse.json.gz')

        LOGGER.debug("Writing request file: %s", request_file_path)
        with open(request_file_path, "w") as fle:
            fle.write(args)  # write JSON to request file
        LOGGER.debug("Request file written")

        task = build_task(request_file_path, response_file_path,
                          request_type, user_id, user, temp_dir)

        if task.state == 'FAILURE':
            raise werkzeug.exceptions.InternalServerError(
                'Task was unable to be started', dict(
                    id=task.id, info=str(
                        task.info)))

        include_kml = request_type in [
            'ExclusionZoneAnalysis', 'PointAnalysis']

        return flask.jsonify(
            taskId=task.id,
            statusUrl=flask.url_for(
                'ratapi-v1.AnalysisStatus', task_id=task.id),
            kmlUrl=(flask.url_for('ratapi-v1.AnalysisKmlResult',
                                  task_id=task.id) if include_kml else None)
        )


class AnalysisKmlResult(MethodView):
    ''' Get a KML result file from AFC Engine '''

    methods = ['GET']

    def _open(self, abs_path, mode):
        ''' Open a response file.

        :param rel_path: The specific file name to open.

        :return: The opened file.
        :rtype: file-like
        '''

        LOGGER.debug('Attempting to open response file "%s"', abs_path)
        if not os.path.exists(abs_path):
            raise werkzeug.exceptions.InternalServerError(
                description='Your analysis was unable to be processed.')
        return open(abs_path, mode)

    def get(self, task_id):
        ''' GET method for KML Result '''
        LOGGER.debug(f"({threading.get_native_id()})"
                     f" {self.__class__.__name__}::{inspect.stack()[0][3]}()")

        task = run.AsyncResult(task_id)
        LOGGER.debug('state: %s', task.state)

        if task.successful() and task.result['status'] == 'DONE':
            if not os.path.exists(task.result['result_path']):
                return flask.make_response(flask.json.dumps(
                    dict(message='Resource already deleted')), 410)
            if 'kml_path' not in task.result or not os.path.exists(
                    task.result['kml_path']):
                return werkzeug.exceptions.NotFound(
                    'This task did not produce a KML')
            resp = flask.make_response()
            LOGGER.debug("Reading kml file: %s", task.result['kml_path'])
            with self._open(task.result['kml_path'], 'rb') as resp_file:
                resp.data = resp_file.read()
            resp.content_type = 'application/octet-stream'
            return resp

        raise werkzeug.exceptions.NotFound('KML not found')


class AnalysisStatus(MethodView):
    ''' Check status of task '''

    methods = ['GET', 'DELETE']

    def _open(self, abs_path, mode):
        ''' Open a response file.

        :param rel_path: The specific file name to open.
        :return: The opened file.
        :rtype: file-like
        '''

        LOGGER.debug('Attempting to open response file "%s"', abs_path)
        if not os.path.exists(abs_path):
            raise werkzeug.exceptions.InternalServerError(
                description='Your analysis was unable to be processed.')
        return open(abs_path, mode)

    def get(self, task_id):
        ''' GET method for Analysis Status '''
        LOGGER.debug(f"({threading.get_native_id()})"
                     f" {self.__class__.__name__}::{inspect.stack()[0][3]}()")

        task = run.AsyncResult(task_id)

        LOGGER.debug('state: %s', task.state)

        if task.info and task.info.get('progress_file') is not None and \
                not os.path.exists(os.path.dirname(task.info['progress_file'])):
            return flask.make_response(flask.json.dumps(
                dict(percent=0, message="Try Again")), 503)

        if task.state == 'PROGRESS':
            auth(is_user=task.info['user_id'])
            # get progress and ETA
            if not os.path.exists(task.info['progress_file']):
                return flask.jsonify(
                    percent=0,
                    message='Initializing...'
                ), 202
            progress_file = task.info['progress_file']
            with open(progress_file, 'r') as prog:
                lines = prog.readlines()
                if len(lines) <= 0:
                    return flask.make_response(flask.json.dumps(
                        dict(percent=0, message="Try Again")), 503)
                LOGGER.debug(lines)
                return flask.jsonify(
                    percent=float(lines[0]),
                    message=lines[1],
                ), 202

        if not task.ready():
            return flask.make_response(flask.json.dumps(
                dict(percent=0, message='Pending...')), 202)

        if task.failed():
            raise werkzeug.exceptions.InternalServerError(
                'Task execution failed')

        if task.successful() and task.result['status'] == 'DONE':
            auth(is_user=task.result['user_id'])
            if not os.path.exists(task.result['result_path']):
                return flask.make_response(flask.json.dumps(
                    dict(message='Resource already deleted')), 410)
            # read temporary file generated by afc-engine
            resp = flask.make_response()
            LOGGER.debug("Reading result file: %s", task.result['result_path'])
            with self._open(task.result['result_path'], 'rb') as resp_file:
                resp.data = resp_file.read()
            resp.content_type = 'application/json'
            resp.content_encoding = "gzip"
            return resp

        elif task.successful() and task.result['status'] == 'ERROR':
            auth(is_user=task.result['user_id'])
            if not os.path.exists(task.result['error_path']):
                return flask.make_response(flask.json.dumps(
                    dict(message='Resource already deleted')), 410)
            # read temporary file generated by afc-engine
            LOGGER.debug("Reading error file: %s", task.result['error_path'])
            with self._open(task.result['error_path'], 'rb') as error_file:
                raise AFCEngineException(
                    description=error_file.read(),
                    exit_code=task.result['exit_code'])

        else:
            raise werkzeug.exceptions.NotFound('Task not found')

    def delete(self, task_id):
        ''' DELETE method for Analysis Status '''

        task = run.AsyncResult(task_id)

        if not task.ready():
            # task is still running, terminate it
            LOGGER.debug('Terminating %s', task_id)
            task.revoke(terminate=True)
            return flask.make_response(flask.json.dumps(
                dict(message='Task deleted')), 200)

        if task.failed():
            task.forget()
            return flask.make_response(flask.json.dumps(
                dict(message='Task deleted')), 200)

        if task.successful() and task.result['status'] == 'DONE':
            auth(is_user=task.result['user_id'])
            if not os.path.exists(task.info['result_path']):
                return flask.make_response(flask.json.dumps(
                    dict(message='Resource already deleted')), 410)
            LOGGER.debug('Deleting %s', task.info.get('result_path'))
            os.remove(task.info['result_path'])
            if 'kml_path' in task.info and os.path.exists(
                    task.info['kml_path']):
                os.remove(task.info['kml_path'])
            task.forget()
            return flask.make_response(flask.json.dumps(
                dict(message='Task deleted')), 200)

        elif task.successful() and task.result['status'] == 'ERROR':
            auth(is_user=task.result['user_id'])
            if not os.path.exists(task.info['error_path']):
                return flask.make_response(flask.json.dumps(
                    dict(message='Resource already deleted')), 410)
            LOGGER.debug('Deleting %s', task.info.get('result_path'))
            os.remove(task.info['error_path'])
            task.forget()
            return flask.make_response(flask.json.dumps(
                dict(message='Task deleted')), 200)

        else:
            raise werkzeug.exceptions.NotFound('Task not found')


class UlsDb(MethodView):
    ''' Resource for converting uls files '''

    methods = ['POST']

    def post(self, uls_file):
        ''' POST method for ULS Db convert '''

        auth(roles=['Super'])

        from ..db.generators import create_uls_db

        uls_path = os.path.join(
            flask.current_app.config['NFS_MOUNT_PATH'],
            'rat_transfer',
            'ULS_Database',
            uls_file)
        if not os.path.exists(uls_path):
            raise werkzeug.exceptions.BadRequest(
                "File does not exist: " + uls_file)

        file_base = os.path.splitext(uls_path)[0]
        LOGGER.debug('converting uls from csv(%s) to sqlite3(%s)',
                     uls_path, file_base + '.sqlite3')
        try:
            invalid_rows, errors = create_uls_db(file_base, uls_path)
            LOGGER.debug('conversion complete')

            return flask.jsonify(
                invalidRows=invalid_rows,
                errors=errors
            )

        except Exception as err:
            raise werkzeug.exceptions.InternalServerError(
                description=err.message)


class UlsParse(MethodView):
    ''' Resource for daily parse of ULS data '''

    methods = ['GET', 'POST', 'PUT']

    def get(self):
        ''' GET method for last successful runtime of uls parse
        '''
        LOGGER.debug('getting last successful runtime of uls parse')
        user_id = auth(roles=['Admin'])

        try:
            datapath = flask.current_app.config["STATE_ROOT_PATH"] + \
                '/daily_uls_parse/data_files/lastSuccessfulRun.txt'
            if not os.path.exists(datapath):
                raise werkzeug.exceptions.NotFound(
                    'last succesful run file not found')
            lastSuccess = ''
            with open(datapath, 'r') as data_file:
                lastSuccess = data_file.read()
            return flask.jsonify(
                lastSuccessfulRun=lastSuccess,
            )
        except Exception as err:
            raise werkzeug.exceptions.InternalServerError(
                description=err.message)

    def post(self):
        ''' POST method for manual daily ULS Db parsing '''

        auth(roles=['Super'])

        LOGGER.debug('Kicking off daily uls parse')
        try:
            task = parseULS.apply_async(
                args=[flask.current_app.config["STATE_ROOT_PATH"], True])

            LOGGER.debug('uls parse started')

            if task.state == 'FAILURE':
                raise werkzeug.exceptions.InternalServerError(
                    'Task was unable to be started', dict(id=task.id, info=str(task.info)))

            return flask.jsonify(
                taskId=task.id,

                statusUrl=flask.url_for(
                    'ratapi-v1.DailyULSStatus', task_id=task.id),
            )

        except Exception as err:
            raise werkzeug.exceptions.InternalServerError(
                description=err.message)

    def put(self):
        ''' Put method for setting the automatic daily ULS time '''

        auth(roles=['Super'])
        args = json.loads(flask.request.data)
        LOGGER.debug('Recieved arg %s', args)
        hours = args["hours"]
        mins = args["mins"]
        if hours == 0:
            hours = "00"
        if mins == 0:
            mins = "00"
        timeStr = str(hours) + ":" + str(mins)
        LOGGER.debug('Updating automated ULS time to ' + timeStr + " UTC")
        datapath = flask.current_app.config["STATE_ROOT_PATH"] + \
            '/daily_uls_parse/data_files/nextRun.txt'
        if not os.path.exists(datapath):
            raise werkzeug.exceptions.NotFound('next run file not found')
        with open(datapath, 'w') as data_file:
            data_file.write(timeStr)
        try:
            return flask.jsonify(
                newTime=timeStr
            ), 200
        except Exception as err:
            raise werkzeug.exceptions.InternalServerError(
                description=err.message)


class DailyULSStatus(MethodView):
    ''' Check status of task '''

    methods = ['GET', 'DELETE']

    def resetManualParseFile(self):
        ''' Overwrites the file for manual task id with a blank string '''
        datapath = flask.current_app.config["STATE_ROOT_PATH"] + \
            '/daily_uls_parse/data_files/currentManualId.txt'
        with open(datapath, 'w') as data_file:
            data_file.write("")

    def get(self, task_id):
        ''' GET method for uls parse Status '''
        LOGGER.debug("Getting ULS Parse status with task id: " + task_id)
        task = parseULS.AsyncResult(task_id)
        # LOGGER.debug('state: %s', task.state)

        if task.state == 'PROGRESS':
            LOGGER.debug("Found Task in progress")
            # todo: add percent progress
            return flask.jsonify(
                percent="WIP",
            ), 202
        if not task.ready():
            LOGGER.debug("Found Task pending")
            return flask.make_response(flask.json.dumps(
                dict(percent=0, message='Pending...')), 202)
        if task.state == 'REVOKED':
            LOGGER.debug("Found task already in progress")
            # LOGGER.debug("task info %s", task.info)
            raise werkzeug.exceptions.ServiceUnavailable()

        elif task.failed():
            LOGGER.debug("Found failed task")
            self.resetManualParseFile()
            raise werkzeug.exceptions.InternalServerError(
                'Task excecution failed')

        if task.successful():
            self.resetManualParseFile()
            results = task.result
            return flask.jsonify(
                entriesUpdated=results[0],
                entriesAdded=results[1],
                finishTime=results[2]
            ), 200

        else:
            raise werkzeug.exceptions.NotFound('Task not found')

    def delete(self, task_id):
        ''' DELETE method for ULS Status '''

        task = parseULS.AsyncResult(task_id)

        if not task.ready():
            # task is still running, terminate it
            LOGGER.debug('Terminating %s', task_id)
            task.revoke(terminate=True)
            return flask.make_response(flask.json.dumps(
                dict(message='Task deleted')), 200)
        if task.failed():
            task.forget()
            return flask.make_response(flask.json.dumps(
                dict(message='Task deleted')), 200)

        if task.successful() and task.result['status'] == 'DONE':
            auth(is_user=task.result['user_id'])
            task.forget()
            return flask.make_response(flask.json.dumps(
                dict(message='Task deleted')), 200)

        elif task.successful() and task.result['status'] == 'ERROR':
            auth(is_user=task.result['user_id'])
            task.forget()
            return flask.make_response(flask.json.dumps(
                dict(message='Task deleted')), 200)

        else:
            raise werkzeug.exceptions.NotFound('Task not found')


class BackendFiles():
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def get(self, url):
        ''' GET method for afc config
        '''
        import requests
        headers = {
            'accept': 'text/html,application/xhtml+xml,application/xml',
            'Accept-Encoding': 'gzip, deflate',
            'cache-control': 'max-age=0',
            'content-type': 'application/x-www-form-urlencoded',
            'user-agent': 'rat_server/1.0'
        }
        resp = requests.get(url, headers)
        response = flask.make_response()
        response.content_type = 'text/html'
        response.data = resp.content
        return response


class UlsFiles(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def get(self):
        ''' GET method for uls
        '''
        user_id = auth(roles=['AP', 'Analysis', 'Admin'])
        url = "http://localhost/" + flask.url_for('files.uls_db')
        be = BackendFiles()
        return be.get(url)


class AntennaFiles(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def get(self):
        ''' GET method for uls
        '''
        user_id = auth(roles=['AP', 'Analysis', 'Admin'])
        url = "http://localhost/" + flask.url_for('files.antenna_pattern')
        be = BackendFiles()
        return be.get(url)


class AfcRulesetIds(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def get(self):
        ''' GET method for afc config
        '''
        resp = flask.make_response()
        resp.data = ' '.join(rulesets())
        resp.content_type = 'text/plain'
        return resp


class History(MethodView):
    def get(self, path=None):
        LOGGER.debug(f"History::get({path})")
        user_id = auth(roles=['Analysis', 'Trial', 'Admin'])
        conf = appcfg.ObjstConfig()
        fwd_proto = flask.request.headers.get('X-Forwarded-Proto')
        if not fwd_proto:
            forward_proto = flask.request.scheme
        try:
            rurl = flask.request.base_url
            if path is not None:
                path_len = len(path)
                rurl = flask.request.base_url[:-path_len]
            response = requests.get(
                conf.AFC_OBJST_SCHEME +
                "://" +
                conf.AFC_OBJST_HOST +
                ":" +
                conf.AFC_OBJST_HIST_PORT +
                (
                    ("/" +
                     path) if path is not None else ""),
                headers={
                    'X-Forwarded-Proto': fwd_proto},
                params={
                    "url": rurl},
                stream=True)
            if response.headers['Content-Type'].startswith("application/octet-stream") \
                    and "Content-Encoding" not in response.headers:
                # results.kmz case. Apache can't decompress it.
                resp = flask.make_response()
                resp.data = response.raw.read()
                return resp
            else:
                return flask.render_template_string(response.text)
        except Exception as exc:
            LOGGER.error(f"Unreachable history host. {exc}")
            return f"Unreachable history host. {exc}"


class History0(History):
    pass


class GetRuleset(MethodView):
    """ Get all active rulesets """

    def get(self):
        try:
            configs = AFCConfig.query.all()
            regionStrs = set()
            for config in configs:
                regionStrs.add(regionStrToRulesetId(
                    config.config['regionStr']))
        except BaseException:
            return flask.make_response('DB error', 404)
        resp = flask.make_response()
        resp.data = "{ \n\t\"rulesetId\": [" + ", ".join(
            '"{0}"'.format(x) for x in regionStrs) + "]\n}\n"
        resp.content_type = 'application/json'
        return resp


class GetAfcConfigByRuleset(MethodView):
    """ Get afc_config by rulesets """

    def get(self, ruleset):
        regionStr = rulesetIdToRegionStr(ruleset)  # returns 404 if not found
        try:
            config = AFCConfig.query.filter(
                AFCConfig.config['regionStr'].astext == regionStr).first()
        except BaseException:
            return flask.make_response('DB error', 404)
        if config is None:
            return flask.make_response("Ruleset unactive", 404)
        resp = flask.make_response()
        resp.data = json.dumps(config.config, sort_keys=True, indent=4) + "\n"
        resp.content_type = 'application/json'
        return resp


module.add_url_rule('/guiconfig', view_func=GuiConfig.as_view('GuiConfig'))
module.add_url_rule('/afcconfig/<path:filename>',
                    view_func=AfcConfigFile.as_view('AfcConfigFile'))
module.add_url_rule('/files/lidar_bounds',
                    view_func=LiDAR_Bounds.as_view('LiDAR_Bounds'))
module.add_url_rule('/files/ras_bounds',
                    view_func=RAS_Bounds.as_view('RAS_Bounds'))
module.add_url_rule('/analysis/p1/<request_type>',
                    view_func=Phase1Analysis.as_view('Phase1Analysis'))
module.add_url_rule('/analysis/status/p1/<task_id>',
                    view_func=AnalysisStatus.as_view('AnalysisStatus'))
module.add_url_rule('/analysis/kml/p1/<task_id>',
                    view_func=AnalysisKmlResult.as_view('AnalysisKmlResult'))
module.add_url_rule('/convert/uls/csv/sql/<uls_file>',
                    view_func=UlsDb.as_view('UlsDb'))
module.add_url_rule('/replay',
                    view_func=ReloadAnalysis.as_view('ReloadAnalysis'))
module.add_url_rule('/regions',
                    view_func=AfcRegions.as_view('AfcRegions'))
module.add_url_rule('/about',
                    view_func=About.as_view('About'))
module.add_url_rule('/rulesetIds',
                    view_func=AfcRulesetIds.as_view('AfcRulesetIds'))
module.add_url_rule('/healthy',
                    view_func=HealthCheck.as_view('HealthCheck'))
module.add_url_rule('/ready',
                    view_func=ReadinessCheck.as_view('ReadinessCheck'))
module.add_url_rule('/history',
                    view_func=History0.as_view('History0'))
module.add_url_rule('/history/<path:path>',
                    view_func=History.as_view('History'))
module.add_url_rule('/ulsFiles/',
                    view_func=UlsFiles.as_view('UlsFiles'))
module.add_url_rule('/antennaFiles/',
                    view_func=AntennaFiles.as_view('AntennaFiles'))
module.add_url_rule('/GetRulesetIDs',
                    view_func=GetRuleset.as_view('GetRuleset'))
module.add_url_rule(
    '/GetAfcConfigByRulesetID/<ruleset>',
    view_func=GetAfcConfigByRuleset.as_view('GetAfcConfigByRuleset'))
module.add_url_rule('/about_csrf',
                    view_func=AboutCSRF.as_view('AboutCSRF'))
