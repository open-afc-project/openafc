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

import logging
import os
import sys
import traceback
import importlib.metadata
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
from typing import Any, Optional
from flask.views import MethodView
from ratapi.views.auth import public_route
import werkzeug.exceptions
import uuid
import hashlib
from defs import RNTM_OPT_DBG_GUI, RNTM_OPT_GUI, RNTM_OPT_AFCENGINE_HTTP_IO
from afc_worker import run
import afc_worker
from fst import DataIf
import afctask
from ncli import MsgPublisher
from hchecks import RmqHealthcheck, ObjstHealthcheck
from ..util import AFCEngineException, require_default_uls, \
    als_log_afc_config_change

from afcmodels.aaa import User, AFCConfig, MTLS
from afcmodels.base import db
from afcmodels.hardcoded_relations import RulesetVsRegion
from .auth import auth
import urllib.parse

#: Logger for this module
LOGGER = logging.getLogger(__name__)
LOGGER.setLevel(appcfg.AFC_RATAPI_LOG_LEVEL)

#: All views under this API blueprint
module = flask.Blueprint('ratapi-v1', 'ratapi')


def _admin_may_access_task(task_owner_id: Optional[int], caller_user: Any) \
        -> bool:
    """Check if caller may bypass per-task ownership as Admin/Super.

    Super callers have unrestricted access. Admin callers may only access
    tasks submitted by users in the same org (org-scoped bypass).
    """
    if caller_user is None or not caller_user.is_authenticated:
        return False
    roles = {r.name for r in (caller_user.roles or [])}
    if 'Super' in roles:
        return True
    if 'Admin' not in roles:
        return False
    if task_owner_id is None:
        # Fail closed: ownerless (legacy/AP-originated) tasks are
        # Super-only; a null owner must not make the task world-accessible
        # to every Admin across every org.
        return False
    owner = User.query.get(task_owner_id)
    return (owner is not None and
            getattr(owner, 'org', None) == getattr(caller_user, 'org', None))


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
        original_request_str=None,
        config_str=None,
        timeout_sec=600,
        priority: int = afc_worker.CELERY_PRIORITY_NORMAL,
        queue: str = afc_worker.CELERY_QUEUE,
        owner_id=None):
    """
    Shared logic between PAWS and All other analysis for constructing and async call to run task
    """

    prot, host, port = dataif.getProtocol()
    # Write an initial status.json that records the submitting user's id so
    # that AnalysisStatus.get/delete can enforce per-user task ownership.
    task_obj = afctask.Task(task_id, dataif, hash_val=hash_val,
                            history_dir=history_dir, owner_id=owner_id)
    task_obj.toJson(afctask.Task.STAT_PENDING, runtime_opts=runtime_opts)
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
        "original_request_str": original_request_str,
        "config_str": config_str,
        "deadline": time.time() + timeout_sec
    }
    LOGGER.debug(f"build_task({kwargs})")
    run.apply_async(kwargs=kwargs, priority=priority, queue=queue)


@public_route
class GuiConfig(MethodView):
    ''' Allow the web UI to obtain configuration, including resolved URLs.
    '''

    def get(self):
        ''' GET for gui config
        '''
        LOGGER.debug(f"({threading.get_native_id()})"
                     f" {self.__class__.__name__}::{inspect.stack()[0][3]}()"
                     f" cookies={list(flask.request.cookies.keys())}")

        # Figure out the current server version
        try:
            if sys.version_info.major != 3:
                serververs = importlib.metadata.version('ratapi')
            else:
                serververs = importlib.metadata.distribution('ratapi').version
        except Exception as err:
            LOGGER.error('Failed to fetch server version: {0}'.format(err))
            serververs = 'unknown'

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
            admin_eirp_url=flask.url_for('admin.Eirp'),
            admin_frequency_range_url=flask.url_for('admin.Frequency'),
            ap_deny_admin_url=flask.url_for('admin.AccessPointDeny', id=-1),
            cert_id_admin_url=flask.url_for('admin.CertId', id=-1),
            mtls_admin_url=flask.url_for('admin.MTLS', id=-1),
            dr_admin_url=flask.url_for('admin.DeniedRegion', regionStr="XX"),
            rat_afc=flask.url_for('ap-afc.RatAfcSec'),
            about_url=about_url,
            about_login_url=about_login_url,
            about_sitekey=about_sitekey if about_sitekey else None,
            app_name=flask.current_app.config['USER_APP_NAME'],
            version=serververs,
            grafana_enabled=os.environ.get(
                'AFC_GRAFANA_ENABLED', 'false').lower() == 'true',
        )
        return resp


@public_route
class HealthCheck(MethodView):

    def get(self):
        '''GET method for HealthCheck'''
        msg = 'The ' + flask.current_app.config['AFC_APP_TYPE'] + ' is healthy'
        LOGGER.info(f"{msg}")
        return flask.make_response(msg, 200)


def check_rmq(cfg):
    LOGGER.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()"
                 f" {re.sub(r':[^:@/]+@', ':***@', cfg['BROKER_URL'])}")
    hconn = RmqHealthcheck(cfg['BROKER_URL'])
    if hconn.healthcheck():
        return 1
    return 0


@public_route
class ReadinessCheck(MethodView):

    def get(self):
        '''GET method for Readiness Check'''
        LOGGER.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()"
                     f" cfg: {flask.current_app.config.get('AFC_APP_TYPE')}")
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
            os.chmod(file_path, 0o644)

        return handle

    def get(self):
        ''' GET method for afc config
        '''
        LOGGER.debug(f"({threading.get_native_id()})"
                     f" {self.__class__}::{inspect.stack()[0][3]}()")
        LOGGER.debug('getting analysisRequest')
        user_id = auth(roles=['AP', 'Analysis', 'Admin'])
        user = User.query.filter_by(id=user_id).first()
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
        LOGGER.debug(json_resp.get('deviceDesc', {}).get('serialNumber'))
        LOGGER.debug(resp.headers.get('AnalysisType'))
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
        auth(roles=['AP', 'Analysis', 'Super'])
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
        RulesetVsRegion.region_to_ruleset(filename,
                                          exc=werkzeug.exceptions.NotFound)
        # make sure the config region string is upper case
        rcrd['regionStr'] = filename
        try:
            config = AFCConfig.query.filter(
                AFCConfig.config['regionStr'].astext == filename).first()
            als_log_afc_config_change(
                old_config=config.config if config else None,
                new_config=rcrd, user=current_user.username,
                region=rcrd['regionStr'], source=flask.request.remote_addr)
            if not config:
                config = AFCConfig(rcrd)
                db.session.add(config)
            else:
                config.config = rcrd
                config.created = datetime.datetime.now()
            db.session.commit()

        except BaseException as ex:
            LOGGER.error(f"Error updating AFC Config: {ex}")
            raise werkzeug.exceptions.NotFound()

        return flask.make_response('AFC configuration file updated', 204)


class AfcRegions(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def get(self):
        ''' GET method for afc config
        '''
        user_id = auth(roles=['Admin'])
        LOGGER.debug('Getting AFC regions for user %s', user_id)
        resp = flask.make_response()
        resp.data = ' '.join(RulesetVsRegion.region_list())
        resp.content_type = 'text/plain'
        return resp


@public_route
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

        from flask_mail import Mail, Message

        # Simple per-IP rate limit: allow at most 5 registration requests per
        # hour without captcha to mitigate mailbox-flooding of the operator
        # approval queue.
        _ABOUT_MAX_REQUESTS_PER_HOUR = 5
        # Key the rate-limit bucket on (WSGI peer address, X-Real-IP). Behind
        # the dispatcher nginx every client shares the same remote_addr, so
        # the nginx-supplied X-Real-IP distinguishes proxied clients; keeping
        # remote_addr in the key means a direct-to-rat_server caller cannot
        # forge X-Real-IP to collide with a proxied client's bucket.
        remote_ip = flask.request.remote_addr or "unknown"
        real_ip = flask.request.headers.get("X-Real-IP", "") or "-"
        cache_key = f"about_post_{remote_ip}_{real_ip}"
        # Use a simple in-process counter stored in app.extensions (falls back
        # to a no-limit passthrough when the app has no cache configured).
        _rate_counter = flask.current_app.extensions.get("about_rate_counter")
        if _rate_counter is None:
            import threading
            _rate_counter = {}
            flask.current_app.extensions["about_rate_counter"] = _rate_counter
            flask.current_app.extensions["about_rate_lock"] = threading.Lock()
        _rate_lock = flask.current_app.extensions["about_rate_lock"]
        import time as _time
        now = _time.time()
        with _rate_lock:
            entry = _rate_counter.get(cache_key, (0, now))
            count, window_start = entry
            if now - window_start > 3600:
                count, window_start = 0, now
            count += 1
            _rate_counter[cache_key] = (count, window_start)
            rate_exceeded = count > _ABOUT_MAX_REQUESTS_PER_HOUR

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

            # Apply rate limit when captcha is not configured.
            if not captcha_secret and rate_exceeded:
                LOGGER.warning("About.post: rate limit exceeded for %s", remote_ip)
                return flask.make_response("Too Many Requests", 429)

            bytes = flask.request.stream.read()
            rcrd = json.loads(bytes)
            name = re.sub(r'[\r\n]', ' ', rcrd['name'])
            email = re.sub(r'[\r\n]', ' ', rcrd['email'])
            org = re.sub(r'[\r\n]', ' ', rcrd['org'])

            # verify captcha
            if captcha_secret:
                token = rcrd['token']
                dictToSend = {'secret': captcha_secret,
                              'response': token}
                res = requests.post(captcha_verify, data=dictToSend, timeout=30)

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

        auth(roles=['Analysis'])

        args = flask.request.data
        LOGGER.debug("Running phase 1 analysis with params: %s", args)

        valid_requests = ['PointAnalysis',
                          'ExclusionZoneAnalysis', 'HeatmapAnalysis']
        if request_type not in valid_requests:
            raise werkzeug.exceptions.BadRequest('Invalid request type')

        # Decode request body
        request_str = args.decode('utf-8') if isinstance(args, bytes) else args

        # Apply the same input filters as RatAfc.post() so both ingress paths
        # into afc_worker.run() funnel through identical validation.
        from .ratafc import drop_unwanted_extensions, \
            _validate_request_location_geo
        try:
            request_json = json.loads(request_str)
            from afcmodels import afc_server_models
            try:
                afc_server_models.Rest_ReqMsg_1_4.model_validate(request_json)
            except Exception as exc:
                LOGGER.warning("Pydantic validation failed: %s", exc)
                raise werkzeug.exceptions.BadRequest(f"Invalid request format: {exc}")

            MAX_REQUESTS_PER_MSG = 16
            _raw_reqs = request_json.get(
                'availableSpectrumInquiryRequests', []) or []
            if len(_raw_reqs) > MAX_REQUESTS_PER_MSG:
                raise werkzeug.exceptions.BadRequest(
                    f"Too many requests in one message: {len(_raw_reqs)} "
                    f"(max {MAX_REQUESTS_PER_MSG})")
            drop_unwanted_extensions(json_dict=request_json, is_input=True,
                                     is_gui=True, is_internal=False)
            for _req in request_json.get(
                    'availableSpectrumInquiryRequests', []) or []:
                _validate_request_location_geo(_req)
        except werkzeug.exceptions.HTTPException:
            raise
        except Exception as ex:
            raise werkzeug.exceptions.BadRequest(str(ex))
        request_str = json.dumps(request_json)

        # Retrieve AFC config from DB (use first available)
        config = AFCConfig.query.first()
        if config is None:
            raise werkzeug.exceptions.InternalServerError(
                'No AFC configuration found in database')
        config_str = json.dumps(config.config)

        # Generate unique task ID and request/config hash
        task_id = str(uuid.uuid4())
        hash_val = hashlib.md5(
            (request_str + config_str).encode('utf-8'), usedforsecurity=False).hexdigest()
        config_path = os.path.join("/afc_config", hash_val, "afc_config.json")

        dataif = DataIf()

        # Write request and config to object storage
        with dataif.open(
                os.path.join("/responses", hash_val,
                             "analysisRequest.json")) as hfile:
            hfile.write(request_str)
        with dataif.open(config_path) as hfile:
            hfile.write(config_str)

        # RNTM_OPT_GUI enables KMZ/progress files; exclude RNTM_OPT_DBG because
        # history_dir=None causes the worker to crash when debug mode is on
        runtime_opts = RNTM_OPT_GUI | RNTM_OPT_AFCENGINE_HTTP_IO
        from flask_login import current_user as _cu
        owner_id = _cu.id if _cu and _cu.is_authenticated else None
        build_task(dataif=dataif, request_type=request_type,
                   task_id=task_id, hash_val=hash_val,
                   config_path=config_path, history_dir=None,
                   runtime_opts=runtime_opts, owner_id=owner_id)

        include_kml = request_type in [
            'ExclusionZoneAnalysis', 'PointAnalysis']

        return flask.jsonify(
            taskId=task_id,
            statusUrl=flask.url_for(
                'ratapi-v1.AnalysisStatus', task_id=task_id),
            kmlUrl=(flask.url_for('ratapi-v1.AnalysisKmlResult',
                                  task_id=task_id) if include_kml else None)
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
        from .admin import auth
        from flask_login import current_user as _cu
        user_id = auth(roles=['Analysis', 'Admin', 'Super'])

        dataif = DataIf()
        task = afctask.Task(task_id, dataif)
        stat = task.get()

        # Ownership check: only the submitting user or a Super/Admin may read.
        task_owner = stat.get('owner_id')
        if task_owner is None or user_id != task_owner:
            if not _admin_may_access_task(task_owner, _cu):
                raise werkzeug.exceptions.Forbidden(
                    'You do not have access to this task')

        if stat['status'] != afctask.Task.STAT_SUCCESS:
            raise werkzeug.exceptions.NotFound('KML not found')

        try:
            with dataif.open(
                    os.path.join("/responses", task_id, "results.kmz")) \
                    as hfile:
                kml_data = hfile.read()
        except Exception:
            raise werkzeug.exceptions.NotFound(
                'This task did not produce a KML')

        resp = flask.make_response()
        resp.data = kml_data
        resp.content_type = 'application/octet-stream'
        return resp


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
        from .admin import auth
        from flask_login import current_user as _cu
        user_id = auth(roles=['Analysis', 'Admin', 'Super'])

        dataif = DataIf()
        task = afctask.Task(task_id, dataif)
        stat = task.get()
        status = stat['status']

        # Ownership check: only the submitting user or a Super/Admin may read.
        task_owner = stat.get('owner_id')
        if task_owner is None or user_id != task_owner:
            # Verify caller has Admin/Super (auth already checked roles, but
            # need to distinguish Analysis-only from Admin/Super).
            if not _admin_may_access_task(task_owner, _cu):
                raise werkzeug.exceptions.Forbidden(
                    'You do not have access to this task')

        LOGGER.debug('task status: %s', status)

        if status in (afctask.Task.STAT_PENDING, afctask.Task.STAT_PROGRESS):
            return flask.jsonify(percent=0, message='In progress...'), 202

        if status == afctask.Task.STAT_FAILURE:
            error_data = None
            try:
                with dataif.open(
                        os.path.join("/responses", task_id,
                                     "engine-error.txt")) as hfile:
                    error_data = hfile.read().decode("utf-8", errors="replace")
            except Exception:
                pass
            raise AFCEngineException(
                description=error_data or 'Task execution failed',
                exit_code=stat.get('exit_code', -1))

        if status == afctask.Task.STAT_SUCCESS:
            hash_val = stat.get('hash')
            if not hash_val:
                raise werkzeug.exceptions.InternalServerError(
                    'No result hash in task status')
            resp = flask.make_response()
            with dataif.open(
                    os.path.join("/responses", hash_val,
                                 "analysisResponse.json.gz")) as hfile:
                resp.data = hfile.read()
            resp.content_type = 'application/json'
            resp.content_encoding = "gzip"
            return resp

        raise werkzeug.exceptions.NotFound('Task not found')

    def delete(self, task_id):
        ''' DELETE method for Analysis Status '''
        from .admin import auth
        from flask_login import current_user as _cu
        user_id = auth(roles=['Analysis', 'Admin', 'Super'])

        dataif = DataIf()
        task = afctask.Task(task_id, dataif)

        # Verify ownership before allowing deletion.
        stat = task.get()
        task_owner = stat.get('owner_id')
        if task_owner is None or user_id != task_owner:
            if not _admin_may_access_task(task_owner, _cu):
                raise werkzeug.exceptions.Forbidden(
                    'You do not have access to this task')
        try:
            task.forget()
        except Exception:
            pass
        return flask.jsonify(message='Task deleted'), 200


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
        auth(roles=['Admin'])

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
        auth(roles=['Super'])
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
            return flask.jsonify(percent=0, message='Pending...'), 202
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

        auth(roles=['Super'])
        task = parseULS.AsyncResult(task_id)

        if not task.ready():
            # task is still running, terminate it
            LOGGER.debug('Terminating %s', task_id)
            task.revoke(terminate=True)
            return flask.jsonify(message='Task deleted'), 200
        if task.failed():
            task.forget()
            return flask.jsonify(message='Task deleted'), 200

        if task.successful() and task.result['status'] == 'DONE':
            auth(is_user=task.result['user_id'])
            task.forget()
            return flask.jsonify(message='Task deleted'), 200

        elif task.successful() and task.result['status'] == 'ERROR':
            auth(is_user=task.result['user_id'])
            task.forget()
            return flask.jsonify(message='Task deleted'), 200

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
        resp = requests.get(url, headers, timeout=30)
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
        auth(roles=['AP', 'Analysis', 'Admin'])
        url = "http://localhost/" + flask.url_for('files.uls_db')
        be = BackendFiles()
        return be.get(url)


class AntennaFiles(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def get(self):
        ''' GET method for uls
        '''
        auth(roles=['AP', 'Analysis', 'Admin'])
        url = "http://localhost/" + flask.url_for('files.antenna_pattern')
        be = BackendFiles()
        return be.get(url)


class AfcRulesetIds(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def get(self):
        ''' GET method for afc config
        '''
        user_id = auth(roles=['AP', 'Analysis', 'Admin'])
        LOGGER.debug('getting ruleset ids with user is %s', user_id)
        resp = flask.make_response()
        resp.data = ' '.join(RulesetVsRegion.ruleset_list())
        resp.content_type = 'text/plain'
        return resp


def _load_objst_api_key():
    """ Return the objst/hist bearer token from AFC_OBJST_API_KEY_FILE. """
    key_file = os.environ.get("AFC_OBJST_API_KEY_FILE")
    if key_file and os.path.isfile(key_file):
        with open(key_file) as f:
            return f.read().strip() or None
    return None


class History(MethodView):
    def get(self, path=None):
        LOGGER.debug(f"History::get({path})")
        try:
            auth(roles=['Super'])
        except Exception as auth_exc:
            LOGGER.error(
                f"History auth failed: {type(auth_exc).__name__}: {auth_exc}\n"
                f"{traceback.format_exc()}")
            raise
        conf = appcfg.ObjstConfig()
        fwd_proto = \
            flask.request.headers.get('X-Forwarded-Proto') or \
            flask.request.scheme
        try:
            rurl = flask.request.base_url
            if path is not None:
                path_len = len(path)
                rurl = flask.request.base_url[:-path_len]
            # Authenticate to the history service with the objst bearer token.
            # hist_app rejects unauthenticated requests.
            # Pass Host so the hist service same-origin check compares against
            # the external hostname, not the internal container address.
            hist_headers = {
                'X-Forwarded-Proto': fwd_proto,
                'Host': flask.request.host,
            }
            objst_key = _load_objst_api_key()
            if objst_key:
                hist_headers['Authorization'] = f'Bearer {objst_key}'
            objst_url = urllib.parse.urlunparse(
                (conf.AFC_OBJST_SCHEME,
                 f'{conf.AFC_OBJST_HOST}:{conf.AFC_OBJST_HIST_PORT}',
                 f'/{path or ""}', '', '', ''))
            LOGGER.debug(
                f"History proxying to {objst_url}?url={rurl}")
            response = requests.request(
                method=flask.request.method,
                url=objst_url,
                params={'url': rurl},
                headers=hist_headers, stream=True,
                allow_redirects=False,
                timeout=(10, 60))
            LOGGER.debug(
                f"History objst response: {response.status_code} "
                f"content-type={response.headers.get('Content-Type')}")
            if response.headers.get('Content-Type', '').startswith("application/octet-stream") \
                    and "Content-Encoding" not in response.headers:
                # results.kmz case. Apache can't decompress it.
                # Content-Type guard above ensures this is only reached for
                # application/octet-stream binary data, not HTML.
                resp = flask.make_response(
                    response.raw.read(), response.status_code)
                return resp
            else:
                return flask.Response(response.content, status=response.status_code,
                                      content_type=response.headers.get('Content-Type'))
        except Exception as exc:
            LOGGER.error(
                f"History request failed: {type(exc).__name__}: {exc}\n"
                f"{traceback.format_exc()}")
            return f"Unreachable history host. {exc}"


class History0(History):
    pass


@public_route
class GetRuleset(MethodView):
    """ Get all active rulesets """

    def get(self):
        try:
            configs = AFCConfig.query.all()
            regionStrs = set()
            for config in configs:
                regionStrs.add(
                    RulesetVsRegion.region_to_ruleset(
                        config.config['regionStr'],
                        exc=werkzeug.exceptions.NotFound))
        except BaseException:

            return flask.make_response('DB error', 404)
        resp = flask.make_response()
        resp.data = "{ \n\t\"rulesetId\": [" + ", ".join(
            '"{0}"'.format(x) for x in regionStrs) + "]\n}\n"
        resp.content_type = 'application/json'
        return resp


@public_route
class GetAfcConfigByRuleset(MethodView):
    """ Get afc_config by rulesets """

    def get(self, ruleset):
        # Ungated: returns non-sensitive regulatory config; rcache-service
        # calls this without a session (sibling GetRulesetIDs is also ungated).
        regionStr = \
            RulesetVsRegion.ruleset_to_region(ruleset,
                                              exc=werkzeug.exceptions.NotFound)
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
