''' Flask application generation.
'''
import sys
import os
import flask
import logging
from . import config

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
    from flask_user import UserManager
    import xdg.BaseDirectory
    # Child members
    from . import models, views, util

    flaskapp = flask.Flask(__name__)
    flaskapp.response_class = util.Response

    # default config state from module
    flaskapp.config.from_object(config)
    # initial override from system config
    config_path = xdg.BaseDirectory.load_first_config('fbrat', 'ratapi.conf')
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
    root_logger.setLevel(flaskapp.config['LOG_LEVEL'])
    # Apply handlers to logger
    for handler in flaskapp.config['LOG_HANDLERS']:
        root_logger.addHandler(handler)
    LOGGER.info('Logging at level %s', flaskapp.config['LOG_LEVEL'])

    # DB and AAA setup
    db = models.base.db
    db.init_app(flaskapp)
    Migrate(
        flaskapp, db, directory=os.path.join(owndir, 'migrations'))
    UserManager(flaskapp, db, models.aaa.User)

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
    if not os.path.exists(flaskapp.config['AFC_ENGINE']):
        LOGGER.error('Missing afc-engine executable "%s"',
                     flaskapp.config['AFC_ENGINE'])
    if not os.path.exists(flaskapp.config['TASK_QUEUE']):
        raise Exception('Missing task directory')

    #: configure celery for this instance
    #configure_celery(flaskapp, celery)
    # celery.conf.update(flaskapp.config)

    # Static file dispatchers
    if True:
        from werkzeug.wsgi import DispatcherMiddleware
        from wsgidav import wsgidav_app
        from wsgidav.fs_dav_provider import FilesystemProvider

        # get static web file location
        webdata_paths = xdg.BaseDirectory.load_data_paths('fbrat', 'www')
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
            xdg.BaseDirectory.load_data_paths('fbrat', 'afc-engine', 'ULS_Database'), None)
        if flaskapp.config['DEFAULT_ULS_DIR'] is None:
            LOGGER.error("No default ULS directory found in path search")

        # get static antenna patterns directory
        antenna_patterns = os.path.join(
            flaskapp.config['STATE_ROOT_PATH'], 'AntennaPatterns')
        if not os.path.exists(antenna_patterns):
            os.makedirs(antenna_patterns)

        # create history directory if config is set to use history files
        if (not flaskapp.config['HISTORY_DIR'] is None) and (not os.path.exists(flaskapp.config['HISTORY_DIR'])):
            os.makedirs(flaskapp.config['HISTORY_DIR'])

        # List of (URL paths from root URL, absolute local filesystem paths, read-only boolean)
        dav_trees = (
            ('/www', next(webdata_paths), True),
            ('/ratapi/v1/files/uls_db', uls_databases, False),
            ('/ratapi/v1/files/antenna_pattern', antenna_patterns, False),
            ('/ratapi/v1/files/history',
             flaskapp.config['HISTORY_DIR'], False),
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
                raise exc_type, exc_value, traceback
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

    # Actual resources
    flaskapp.add_url_rule(
        '/', 'root', view_func=redirector('www.index', code=302))
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
