# This Python file uses the following encoding: utf-8
#
# Portions copyright © 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate
# affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy
# of which is included with this software program.
#

''' Flask application generation.
'''
import appcfg
import sys
import os
import datetime
import logging
import flask
import requests
import platform
from sqlalchemy import exc
from fst import DataIf
from afcmodels.base import db
from afcmodels.aaa import User
import als
import prometheus_utils
import prometheus_client
import secret_utils

#: Logger for this module
LOGGER = logging.getLogger(__name__)

#: Current file path
owndir = os.path.abspath(os.path.dirname(__file__))

# Metrics for autoscaling
if prometheus_utils.multiprocess_prometheus_configured():
    prometheus_metric_flask_workers = \
        prometheus_client.Gauge('msghnd_flask_workers',
                                'Total number of Flask workers in container',
                                ['host'], multiprocess_mode='max')
    prometheus_metric_flask_workers.labels(host=platform.node())
    prometheus_metric_flask_workers = \
        prometheus_metric_flask_workers.labels(host=platform.node()).\
        set(os.environ.get('AFC_MSGHND_WORKERS', 0))
    prometheus_metric_flask_active_reqs = \
        prometheus_client.Gauge('msghnd_flask_active_reqs',
                                'Number of currently processed Flask requests',
                                ['host'], multiprocess_mode='sum')
    prometheus_metric_flask_active_reqs = \
        prometheus_metric_flask_active_reqs.labels(host=platform.node())


def create_app(config_override=None):
    ''' Build up a WSGI application for this server.

    :param config_override: Individual variables from `config` to override.
    :type config_override: dict
    :return: A Flask application object.
    :rtype: :py:cls:`flask.Flask`
    '''
    from flask_migrate import Migrate
    from xdg import BaseDirectory

    # Child members
    from . import views, util
    from flask_wtf.csrf import generate_csrf

    flaskapp = flask.Flask(__name__.split('.')[0])
    flaskapp.response_class = util.Response

    # default config state from module
    flaskapp.config.from_object(appcfg)
    flaskapp.config.from_object(appcfg.BrokerConfigurator())
    flaskapp.config.from_object(appcfg.ObjstConfig())
    flaskapp.config.from_object(appcfg.OIDCConfigurator())
    flaskapp.config.from_object(appcfg.RatApiConfigurator())

    # initial override from system config
    config_path = BaseDirectory.load_first_config('fbrat', 'ratapi.conf')
    if config_path:
        flaskapp.config.from_pyfile(config_path)
    # override from FLASK_... environment variables by value ...
    flaskapp.config.from_prefixed_env()
    # ... and by file name
    env_file_prefix = flaskapp.config.get('ENV_FILE_PREFIX')
    if env_file_prefix:
        for env_name, env_value in os.environ.items():
            if not (env_name.startswith(env_file_prefix) and
                    os.path.isfile(env_value)):
                continue
            with open(env_value, encoding="utf-8") as f:
                setting = f.read()
                if len(setting) == 0:
                    continue
                flaskapp.config[env_name[len(env_file_prefix):]] = setting

    # final overrides for this instance
    if config_override:
        flaskapp.config.update(config_override)

    # always autoescape
    flaskapp.select_jinja_autoescape = lambda _filename: True

    # remove any existing flaskapp-specific handlers
    del flaskapp.logger.handlers[:]
    # Logging just after config
    root_logger = logging.getLogger()
    # Root logging level
    root_logger.setLevel(flaskapp.config['AFC_RATAPI_LOG_LEVEL'])
    # Apply handlers to logger
    for handler in flaskapp.config['LOG_HANDLERS']:
        root_logger.addHandler(handler)
    LOGGER.info('Logging at level %s', flaskapp.config['AFC_RATAPI_LOG_LEVEL'])

    als.als_initialize()

    LOGGER.debug('BROKER_URL %s', flaskapp.config['BROKER_URL'])

    # Substitute DB password
    flaskapp.config['SQLALCHEMY_DATABASE_URI'] = \
        secret_utils.substitute_password(
            dsc='fbrat', dsn=flaskapp.config.get('SQLALCHEMY_DATABASE_URI'),
            password=flaskapp.config.get('SQLALCHEMY_DATABASE_PASSWORD'),
            optional=True)

    db.init_app(flaskapp)
    Migrate(
        flaskapp, db, directory=os.path.join(owndir, 'migrations'))

    if flaskapp.config['OIDC_LOGIN']:
        from flask_login import LoginManager
        login_manager = LoginManager()
        login_manager.init_app(flaskapp)

        if (flaskapp.config['OIDC_DISCOVERY_URL']):
            endpoints = requests.get(
                flaskapp.config['OIDC_DISCOVERY_URL'], headers={
                    'Accept': 'application/json'}).json()
            flaskapp.config['OIDC_ORG_AUTH_URL'] = endpoints['authorization_endpoint']
            flaskapp.config['OIDC_ORG_TOKEN_URL'] = endpoints['token_endpoint']
            flaskapp.config['OIDC_ORG_USER_INFO_URL'] = endpoints['userinfo_endpoint']

        @login_manager.user_loader
        def load_user(_id):
            ''' Load user invoked from flask login
            '''
            return User.get(_id)
    else:
        # Non OIDC login.
        from flask_user import UserManager
        user_manager = UserManager(flaskapp, db, User)

        @flaskapp.before_request
        def log_user_access():
            if flask.request.endpoint == 'user.logout':
                from flask_login import current_user
                try:
                    LOGGER.debug('user:%s logout ', current_user.username)
                    als.als_json_log('user_access',
                                     {'action': 'logout',
                                      'user': current_user.username,
                                      'from': flask.request.remote_addr})
                except BaseException:
                    LOGGER.debug('user:%s logout ', 'unknown')
                    als.als_json_log(
                        'user_access', {
                            'action': 'logout', 'user': 'unknown', 'from': flask.request.remote_addr})

        @flaskapp.after_request
        def log_user_accessed(response):
            if flask.request.method == 'POST' and flask.request.endpoint == 'user.login':
                LOGGER.debug(
                    'user:%s login status %d',
                    flask.request.form['username'],
                    response.status_code)
                if response.status_code != 302:
                    als.als_json_log('user_access',
                                     {'action': 'login',
                                      'user': flask.request.form['username'],
                                         'from': flask.request.remote_addr,
                                         'status': response.status_code})
                else:
                    als.als_json_log('user_access',
                                     {'action': 'login',
                                      'user': flask.request.form['username'],
                                         'from': flask.request.remote_addr,
                                         'status': 'success'})

            return response

    # Check configuration
    state_path = flaskapp.config['STATE_ROOT_PATH']
    nfs_mount_path = flaskapp.config['NFS_MOUNT_PATH']
    if not os.path.exists(state_path):
        try:
            os.makedirs(state_path)
        except OSError:
            LOGGER.error('Failed creating state directory "%s"', state_path)
    if not os.path.exists(flaskapp.config['TASK_QUEUE']):
        raise Exception('Missing task directory')

    # Static file dispatchers
    if flaskapp.config['AFC_APP_TYPE'] == 'server':

        @flaskapp.before_request
        def check_csrf_token():
            import werkzeug
            from flask_login import current_user
            if (flask.request.method == 'POST') and \
                    (current_user.is_authenticated) and \
                    (flask.request.headers.get('X-Csrf-Token') !=
                     flask.request.cookies.get('csrf_token')):
                LOGGER.error(f"CSRF Token mismatch. Token from header: "
                             f"{flask.request.headers.get('X-Csrf-Token')}, "
                             f"token from cookie: "
                             f"{flask.request.cookies.get('csrf_token')}")
                raise werkzeug.exceptions.BadRequest("CSRF token mismatch")

        @flaskapp.after_request
        def make_csrf_cookie(response):
            from flask_login import current_user
            response.set_cookie('csrf_token', generate_csrf())
            return response

        if not os.path.exists(os.path.join(
                nfs_mount_path, 'rat_transfer', 'frequency_bands')):
            os.makedirs(os.path.join(nfs_mount_path,
                        'rat_transfer', 'frequency_bands'))

        from werkzeug.middleware.dispatcher import DispatcherMiddleware
        from wsgidav import wsgidav_app
        from wsgidav.fs_dav_provider import FilesystemProvider

        # get static web file location
        webdata_paths = BaseDirectory.load_data_paths('fbrat', 'www')
        # Temporary solution, do not raise exception while web module
        # not installed.
        if not webdata_paths:
            raise RuntimeError(
                'Web data directory "fbrat/www" is not available')

        # get uls database directory
        uls_databases = os.path.join(
            flaskapp.config['NFS_MOUNT_PATH'], 'rat_transfer', 'ULS_Database')
        if not os.path.exists(uls_databases):
            os.makedirs(uls_databases)

        # get static uls data path
        if flaskapp.config['DEFAULT_ULS_DIR'] is None:
            LOGGER.error("No default ULS directory found in path search")

        # get static antenna patterns directory
        antenna_patterns = os.path.join(
            flaskapp.config['NFS_MOUNT_PATH'],
            'rat_transfer',
            'Antenna_Patterns')
        if not os.path.exists(antenna_patterns):
            os.makedirs(antenna_patterns)

        # List of (URL paths from root URL, absolute local filesystem paths,
        # read-only boolean)
        dav_trees = (
            ('/www', next(webdata_paths), True),
            ('/ratapi/v1/files/uls_db', uls_databases, False),
            ('/ratapi/v1/files/antenna_pattern', antenna_patterns, False)
        )

        dav_wsgi_apps = {}
        for (url_path, fs_path, read_only) in dav_trees:
            if fs_path is None:
                flaskapp.logger.debug(
                    'skipping dav export: {0}'.format(url_path))
                continue
            if not os.path.isdir(fs_path):
                flaskapp.logger.error(
                    'Missing DAV export path "{0}"'.format(fs_path))
                continue

            dav_config = wsgidav_app.DEFAULT_CONFIG.copy()
            dav_config.update({
                # Absolute root path for HREFs
                'mount_path': flaskapp.config['APPLICATION_ROOT'] + url_path,
                'provider_mapping': {
                    '/': FilesystemProvider(fs_path, readonly=read_only),
                },
                'http_authenticator.trusted_auth_header': 'REMOTE_USER',
                'verbose': (0, 1)[flaskapp.config['DEBUG']],
                'enable_loggers': ['wsgidav'],
                'property_manager': False,  # True: use property_manager.PropertyManager
                'lock_manager': True,  # True: use lock_manager.LockManager
                # None: domain_controller.WsgiDAVDomainController(user_mapping)
                'http_authenticator.domaincontroller': None,
            })
            # dav_wsgi_apps[app_sub_path] = wsgidav_app.WsgiDAVApp(dav_config)
            dav_wsgi_apps[url_path] = wsgidav_app.WsgiDAVApp(dav_config)
        # Join together all sub-path DAV apps
        flaskapp.wsgi_app = DispatcherMiddleware(
            flaskapp.wsgi_app, dav_wsgi_apps)

        # set prefix middleware
    flaskapp.wsgi_app = util.PrefixMiddleware(
        flaskapp.wsgi_app, prefix=flaskapp.config['APPLICATION_ROOT'])
    # set header middleware
    flaskapp.wsgi_app = util.HeadersMiddleware(
        flaskapp.wsgi_app)

    # User authentication wraps all others
    if False:
        # JWK wrapper
        # This is the same procesing as done by jwcrypto v0.4+ but not present
        # in 0.3
        with open(flaskapp.config['SIGNING_KEY_FILE'], 'rb') as pemfile:
            from cryptography.hazmat.backends import default_backend
            from cryptography.hazmat.primitives import serialization
            from jwcrypto.common import base64url_encode
            from binascii import unhexlify
            from jwcrypto import jwk

            def encode_int(val):
                intg = hex(val).rstrip('L').lstrip('0x')
                return base64url_encode(
                    unhexlify((len(intg) % 2) * '0' + intg))

            keydata = serialization.load_pem_private_key(
                pemfile.read(), password=None, backend=default_backend())
            pn = keydata.private_numbers()
            params = dict(
                kty='RSA',
                n=encode_int(pn.public_numbers.n),
                e=encode_int(pn.public_numbers.e),
                d=encode_int(pn.d),
                p=encode_int(pn.p),
                q=encode_int(pn.q),
                dp=encode_int(pn.dmp1),
                dq=encode_int(pn.dmq1),
                qi=encode_int(pn.iqmp)
            )
            privkey = jwk.JWK(**params)

        unsec = [
            '^/$',
            '^/cpoinfo$',
            '^/favicon.ico$',
            '^/static/?',
        ]
        auth_cfg = dict(
            dbus_conn=flaskapp.extensions['dbus'],
            db_sess=flaskapp.extensions['dbsessmgr'],
            privkey=privkey,
            sess_idle_expire=flaskapp.config['SESSION_IDLE_EXPIRE'],
            unsecured=unsec,
            allow_cookie=flaskapp.config['AUTHN_USE_COOKIE'],
        )
        authn_middle = views.authn.AuthRequiredMiddleware(  # pylint: disable=no-member
            flaskapp, **auth_cfg)
        flaskapp.extensions['authn_middle'] = authn_middle
        flaskapp.wsgi_app = authn_middle
        auth_lookup = authn_middle.get_path_dict()
    else:
        flaskapp.extensions['authn_middle'] = None
        # Dummy data needed for cpoinfo
        auth_lookup = {
            'auth.login': '',
            'auth.logout': '',
            'auth.info': '',
        }

    #: full set of external dotted names
    ext_lookup = dict(auth_lookup)
    ext_lookup.update({
        'www.index': '/www/index.html',
        'files.uls_db': '/ratapi/v1/files/uls_db',
        'files.antenna_pattern': '/ratapi/v1/files/antenna_pattern',
    })

    def external_url_handler(error, endpoint, _values):
        ''' Looks up an external URL when `url_for` cannot build a URL.

        :param endpoint: the endpoint of the URL (name of the function)
        :param values: the variable arguments of the URL rule
        :return: The full URL
        '''
        LOGGER.debug("looking for endpoint: %s", endpoint)
        url = ext_lookup.get(endpoint, None)
        if url is None:
            # External lookup did not have a URL.
            # Re-raise the BuildError, in context of original traceback.
            exc_type, exc_value, traceback = sys.exc_info()
            if exc_value is error:
                raise exc_type(exc_value).with_traceback(traceback)
            else:
                raise error
        # url_for will use this result, instead of raising BuildError.
        val = flaskapp.config['APPLICATION_ROOT'] + url
        LOGGER.debug("found endpoint: %s", val)
        return val

    flaskapp.url_build_error_handlers.append(external_url_handler)

    def redirector(name, code=301):
        ''' A view redirector function.

        :param name: The endpoint name to redirect to.
        :param code: The redirect code.
        :return: The view function, which passes all kwargs to the view.
        '''

        def view(**kwargs):
            from .util import redirect
            return redirect(flask.url_for(name, **kwargs), code=code)

        return view

    # check database
    with flaskapp.app_context():
        try:
            user = db.session.query(User).first()  # pylint: disable=no-member

        except exc.SQLAlchemyError as e:
            if 'relation "aaa_user" does not exist' in str(e.args):
                LOGGER.error("ERROR - Missing users in the database.\n"
                             "Create using following command sequense:\n"
                             "    rat-manage-api db-create\n")
            else:
                LOGGER.error("Database is in old format.\n"
                             "Upgrade using following command sequence:\n"
                             "    rat-manage-api db-upgrade")
                flaskapp.config['UPGRADE_REQ'] = True

    # Actual resources
    flaskapp.add_url_rule(
        '/', 'root', view_func=redirector('www.index', code=302))
    if flaskapp.config['AFC_APP_TYPE'] == 'server':
        views.paws.create_handler(flaskapp, '/paws')
        flaskapp.add_url_rule('/paws', 'paws.browse',
                              view_func=redirector('browse.index', code=302))
    if ('AFC_MSGHND_WORKERS' in os.environ) and \
            (prometheus_utils.multiprocess_prometheus_configured()):
        flaskapp.add_url_rule(
            '/metrics', view_func=prometheus_utils.multiprocess_flask_metrics)

        @flaskapp.before_request
        def inc_active_counter_metric():
            prometheus_metric_flask_active_reqs.inc()

        @flaskapp.after_request
        def dec_active_counter_metric(response):
            prometheus_metric_flask_active_reqs.dec()
            return response

    flaskapp.register_blueprint(views.ratapi.module, url_prefix='/ratapi/v1')
    flaskapp.register_blueprint(views.ratafc.module, url_prefix='/ap-afc')
    flaskapp.register_blueprint(views.auth.module, url_prefix='/auth')
    flaskapp.register_blueprint(views.admin.module, url_prefix='/admin')
    # catch all invalid paths and redirect
    if not flaskapp.config['DEBUG']:
        flaskapp.add_url_rule('/<path:p>', 'any',
                              view_func=redirector('www.index', code=302))

    return flaskapp
