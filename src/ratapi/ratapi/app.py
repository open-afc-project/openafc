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
import sys
import os
import datetime
import logging
import flask
import requests
from . import config
from . import data_if

#: Logger for this module
LOGGER = logging.getLogger(__name__)

#: Current file path
owndir = os.path.abspath(os.path.dirname(__file__))


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
    from . import models, views, util

    flaskapp = flask.Flask(__name__.split('.')[0])
    flaskapp.response_class = util.Response

    # default config state from module
    flaskapp.config.from_object(config)
    # initial override from system config
    config_path = BaseDirectory.load_first_config('fbrat', 'ratapi.conf')
    if config_path:
        flaskapp.config.from_pyfile(config_path)
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
    root_logger.setLevel(flaskapp.config['AFC_OBJST_LOG_LVL'])
    # Apply handlers to logger
    for handler in flaskapp.config['LOG_HANDLERS']:
        root_logger.addHandler(handler)
    LOGGER.info('Logging at level %s', flaskapp.config['AFC_OBJST_LOG_LVL'])

    flaskapp.config.update(
        BROKER_URL=os.getenv('BROKER_PROT',
                             flaskapp.config['BROKER_DEFAULT_PROT']) +
                   "://" +
                   os.getenv('BROKER_USER',
                             flaskapp.config['BROKER_DEFAULT_USER']) +
                   ":" +
                   os.getenv('BROKER_PWD',
                             flaskapp.config['BROKER_DEFAULT_PWD']) +
                   "@" +
                   os.getenv('BROKER_FQDN',
                             flaskapp.config['BROKER_DEFAULT_FQDN']) +
                   ":" +
                   os.getenv('BROKER_PORT',
                             flaskapp.config['BROKER_DEFAULT_PORT']) +
                   "/fbrat"
    )
    LOGGER.debug('BROKER_URL %s', flaskapp.config['BROKER_URL'])
    # DB and AAA setup
    db = models.base.db

    db.init_app(flaskapp)
    Migrate(
        flaskapp, db, directory=os.path.join(owndir, 'migrations'))

    try:
        # private configuration. Override values from config
        from . import priv_config
        flaskapp.config.from_object(priv_config)
    except:
        pass

    # override config with environment variables
    flaskapp.config['OIDC_LOGIN']=(
       os.getenv('OIDC_LOGIN', str(flaskapp.config['OIDC_LOGIN'])).lower() == "true")

    if flaskapp.config['OIDC_LOGIN']:
        from models.aaa import User
        from flask_login import  LoginManager
        login_manager = LoginManager()
        login_manager.init_app(flaskapp)

        # Override values from config
        flaskapp.config['OIDC_ARG']=os.getenv('OIDC_ARG')
        if flaskapp.config['OIDC_ARG']:
            import json
            with open(flaskapp.config['OIDC_ARG']) as oidc_config:
                data = json.load(oidc_config)

            flaskapp.config['OIDC_CLIENT_SECRET']=data['OIDC_CLIENT_SECRET']
            flaskapp.config['OIDC_CLIENT_ID']=data['OIDC_CLIENT_ID']
            flaskapp.config['OIDC_DISCOVERY_URL']=data['OIDC_DISCOVERY_URL']

        if (flaskapp.config['OIDC_DISCOVERY_URL']):
            endpoints = requests.get(flaskapp.config['OIDC_DISCOVERY_URL'],
                headers={'Accept' : 'application/json'}).json()
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
        user_manager = UserManager(flaskapp, db, models.aaa.User)

    # Check configuration
    state_path = flaskapp.config['STATE_ROOT_PATH']
    if not os.path.exists(state_path):
        try:
            os.makedirs(state_path)
        except OSError:
            LOGGER.error('Failed creating state directory "%s"', state_path)
    if not os.path.exists(os.path.join(state_path, 'responses')):
        os.makedirs(os.path.join(state_path, 'responses'))
    if not os.path.exists(os.path.join(state_path, 'frequency_bands')):
        os.makedirs(os.path.join(state_path, 'frequency_bands'))
    if not os.path.exists(flaskapp.config['TASK_QUEUE']):
        raise Exception('Missing task directory')

    #: configure celery for this instance
    #configure_celery(flaskapp, celery)
    # celery.conf.update(flaskapp.config)

    # Static file dispatchers
    if True:
        from werkzeug.middleware.dispatcher import DispatcherMiddleware
        from wsgidav import wsgidav_app
        from wsgidav.fs_dav_provider import FilesystemProvider

        # get static web file location
        if flaskapp.config['AFC_APP_TYPE'] == 'server':
            webdata_paths = BaseDirectory.load_data_paths('fbrat', 'www')
            # Temporary solution, do not raise exception while web module
            # not installed.
            if not webdata_paths:
                raise RuntimeError(
                    'Web data directory "fbrat/www" is not available')

        # get uls database directory
        uls_databases = os.path.join(
            flaskapp.config['STATE_ROOT_PATH'], 'ULS_Database')
        if not os.path.exists(uls_databases):
            os.makedirs(uls_databases)

        # get static uls data path
        flaskapp.config['DEFAULT_ULS_DIR'] = next(
            BaseDirectory.load_data_paths('fbrat', 'afc-engine',
                                              'ULS_Database'), None)
        if flaskapp.config['DEFAULT_ULS_DIR'] is None:
            LOGGER.error("No default ULS directory found in path search")

        # get static antenna patterns directory
        antenna_patterns = os.path.join(
            flaskapp.config['STATE_ROOT_PATH'], 'AntennaPatterns')
        if not os.path.exists(antenna_patterns):
            os.makedirs(antenna_patterns)

        # List of (URL paths from root URL, absolute local filesystem paths,
        # read-only boolean)
        dav_trees = ()
        if flaskapp.config['AFC_APP_TYPE'] == 'server':
            dav_trees = dav_trees + (('/www', next(webdata_paths), True),)

        dav_trees = dav_trees + (('/ratapi/v1/files/uls_db', uls_databases,
                                  False),
                                 ('/ratapi/v1/files/antenna_pattern',
                                  antenna_patterns, False),)

        dataif = data_if.DataIf(
            fsroot=flaskapp.config['STATE_ROOT_PATH'],
            probeHttps=False)
        if dataif.isFsBackend():
            hist_dir = os.path.join(
                flaskapp.config['STATE_ROOT_PATH'],
                data_if.DataIf.LHISTORYDIR)
            if not os.path.exists(hist_dir):
                os.makedirs(hist_dir)
            tmp = list(dav_trees)
            tmp.append(('/ratapi/v1/files/history', hist_dir, False))
            dav_trees = tuple(tmp)

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
    #set header middleware
    flaskapp.wsgi_app = util.HeadersMiddleware(
        flaskapp.wsgi_app)

    # User authentication wraps all others
    if False:
        # JWK wrapper
        # This is the same procesing as done by jwcrypto v0.4+ but not present in 0.3
        with open(flaskapp.config['SIGNING_KEY_FILE'], 'rb') as pemfile:
            from cryptography.hazmat.backends import default_backend
            from cryptography.hazmat.primitives import serialization
            from jwcrypto.common import base64url_encode
            from binascii import unhexlify
            from jwcrypto import jwk

            def encode_int(val):
                intg = hex(val).rstrip('L').lstrip('0x')
                return base64url_encode(unhexlify((len(intg) % 2) * '0' + intg))

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
        'files.history': '/ratapi/v1/files/history',
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
                raise [exc_type, exc_value, traceback]
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
            from .models.aaa import User
            user = db.session.query(User).first()  # pylint: disable=no-member
        except Exception as exception:
            if 'aaa_user.username does not exist' in str(exception.args) or 'aaa_user.org does not exist' in str(exception.args):
                LOGGER.error("""
DATABASE is in old format.  Upgrade using following command sequence:
rat-manage-api db-upgrade
                """)
                flaskapp.config['UPGRADE_REQ'] = True

    # Actual resources
    flaskapp.add_url_rule(
        '/', 'root', view_func=redirector('www.index', code=302))
    if flaskapp.config['AFC_APP_TYPE'] == 'server':
        views.paws.create_handler(flaskapp, '/paws')
        flaskapp.add_url_rule('/paws', 'paws.browse',
                              view_func=redirector('browse.index', code=302))
    flaskapp.register_blueprint(views.ratapi.module, url_prefix='/ratapi/v1')
    flaskapp.register_blueprint(views.ratafc.module, url_prefix='/ap-afc')
    flaskapp.register_blueprint(views.auth.module, url_prefix='/auth')
    flaskapp.register_blueprint(views.admin.module, url_prefix='/admin')
    # catch all invalid paths and redirect
    if not flaskapp.config['DEBUG']:
        flaskapp.add_url_rule('/<path:p>', 'any',
                              view_func=redirector('www.index', code=302))

    return flaskapp
