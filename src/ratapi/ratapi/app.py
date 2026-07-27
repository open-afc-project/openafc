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
import logging
import traceback
import flask
import requests
import urllib.parse
import platform
import hmac
from sqlalchemy import exc
from afcmodels.base import db
from afcmodels.aaa import User
import als
import prometheus_utils
import prometheus_client
import db_utils

#: Logger for this module
LOGGER = logging.getLogger(__name__)

#: Current file path
owndir = os.path.abspath(os.path.dirname(__file__))


def _safe_broker_url(url):
    """ Returns AMQP/broker URL with password redacted for safe logging """
    try:
        parsed = urllib.parse.urlparse(url)
        if parsed.password:
            netloc = f"{parsed.username}:<REDACTED>@{parsed.hostname}"
            if parsed.port:
                netloc += f":{parsed.port}"
            return urllib.parse.urlunparse(parsed._replace(netloc=netloc))
    except Exception:
        pass
    # Fail closed: never return the raw URL on the exception path (or the
    # no-password fall-through) — it may embed a password that
    # urllib.parse could not handle. Mirrors afc_worker.py/acceptor.py's
    # placeholder behaviour (SUB-0138-46 sibling).
    return '<broker-url>'


class _DavBearerTokenGate:
    """WSGI middleware that enforces AFC internal-token auth on WsgiDAV paths.

    Nginx's auth_request gate covers all external traffic; this layer adds
    defence-in-depth for intra-cluster requests that reach rat_server directly.

    The token is read once at first request and cached for the process lifetime.
    Token rotation requires a container restart.
    """

    def __init__(self, app):
        self._app = app
        self._token = None

    def _get_token(self):
        if self._token is None:
            self._token = os.environ.get("AFC_INTERNAL_TOKEN")
        if not self._token:
            token_file = os.environ.get("AFC_INTERNAL_TOKEN_FILE", "")
            if token_file and os.path.isfile(token_file):
                with open(token_file, "r") as f:
                    self._token = f.read().strip()
        return self._token

    def __call__(self, environ, start_response):
        expected = self._get_token()
        auth_header = environ.get("HTTP_X_AFC_INTERNAL_TOKEN", "")
        if not expected or not hmac.compare_digest(auth_header.encode(),
                                                   expected.encode()):
            start_response("403 Forbidden",
                           [("Content-Type", "text/plain")])
            return [b"Forbidden"]
        return self._app(environ, start_response)


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

    if flaskapp.config.get('SECRET_KEY') in (None, '', 'flasksecret_please_override'):
        raise RuntimeError("A valid SECRET_KEY must be provided.")

    if flaskapp.config.get('OIDC_LOGIN') and not flaskapp.config.get('OIDC_REDIRECT_BASE'):
        raise RuntimeError(
            "OIDC_REDIRECT_BASE must be set when OIDC_LOGIN is enabled.")

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
    # Install credential-redacting filter on the root logger so no DSN/URL
    # password reaches any log sink regardless of call site.
    import appcfg as _appcfg
    _appcfg.install_credential_redact_filter()
    LOGGER.info('Logging at level %s', flaskapp.config['AFC_RATAPI_LOG_LEVEL'])

    als.als_initialize()

    LOGGER.debug('BROKER_URL %s', _safe_broker_url(flaskapp.config['BROKER_URL']))

    # Substitute DB password
    flaskapp.config['SQLALCHEMY_DATABASE_URI'] = \
        db_utils.substitute_password(
            dsn=flaskapp.config.get('SQLALCHEMY_DATABASE_URI'),
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
                    'Accept': 'application/json'}, timeout=30).json()
            flaskapp.config['OIDC_ORG_AUTH_URL'] = endpoints['authorization_endpoint']
            flaskapp.config['OIDC_ORG_TOKEN_URL'] = endpoints['token_endpoint']
            flaskapp.config['OIDC_ORG_USER_INFO_URL'] = endpoints['userinfo_endpoint']
            flaskapp.config['OIDC_ORG_ISSUER'] = endpoints['issuer']
            flaskapp.config['OIDC_ORG_JWKS_URL'] = endpoints['jwks_uri']

        @login_manager.user_loader
        def load_user(_id):
            ''' Load user invoked from flask login
            '''
            try:
                return User.get(_id)
            except exc.DataError:
                LOGGER.warning("Invalid user_id in session cookie "
                               "(secret key mismatch?), forcing re-login")
                db.session.rollback()
                return None
    else:
        from flask_security import Security, SQLAlchemyUserDatastore
        from afcmodels.aaa import Role

        flaskapp.config.setdefault('SECURITY_BLUEPRINT_NAME', 'user')
        flaskapp.config.setdefault('SECURITY_LOGIN_URL', '/user/sign-in')
        flaskapp.config.setdefault('SECURITY_LOGOUT_URL', '/user/sign-out')
        flaskapp.config.setdefault('SECURITY_REGISTER_URL', '/user/register')
        flaskapp.config.setdefault('SECURITY_REGISTERABLE', False)
        flaskapp.config.setdefault('SECURITY_SEND_REGISTER_EMAIL', False)
        flaskapp.config.setdefault('SECURITY_PASSWORD_HASH', 'bcrypt')
        # Operators should provision a dedicated SECURITY_PASSWORD_SALT (e.g.
        # via FLASKFILE_SECURITY_PASSWORD_SALT, mirroring FLASKFILE_SECRET_KEY)
        # so the password pepper and the session-signing key are independent
        # secrets. When that has not been done, do NOT reuse SECRET_KEY
        # verbatim as the pepper: that gives two unrelated security domains
        # (itsdangerous session signing; flask-security password peppering)
        # identical key material with no domain separation, so a single
        # SECRET_KEY disclosure both forges sessions and strips the pepper
        # protecting every stored password hash. Derive a distinct,
        # non-interchangeable value instead.
        if 'SECURITY_PASSWORD_SALT' not in flaskapp.config:
            LOGGER.warning(
                "SECURITY_PASSWORD_SALT not explicitly configured; deriving "
                "one from SECRET_KEY with domain separation. For full key "
                "independence, provision a dedicated "
                "FLASKFILE_SECURITY_PASSWORD_SALT secret.")
            import hashlib as _hl
            flaskapp.config['SECURITY_PASSWORD_SALT'] = hmac.new(
                flaskapp.config.get(
                    'SECRET_KEY', 'super-secret-salt').encode(),
                b'ratapi:SECURITY_PASSWORD_SALT:v1',
                _hl.sha256).hexdigest()
        flaskapp.config.setdefault(
            'SECURITY_LOGIN_USER_TEMPLATE', 'security/login_user.html')
        flaskapp.config.setdefault(
            'SECURITY_REGISTER_USER_TEMPLATE', 'security/register_user.html')
        flaskapp.config.setdefault('SECURITY_CSRF_IGNORE_UNAUTH_ENDPOINTS',
                                   True)
        flaskapp.config.setdefault('SECURITY_POST_LOGIN_VIEW', '/')
        flaskapp.config.setdefault('SECURITY_POST_LOGOUT_VIEW', '/')
        flaskapp.config.setdefault('SECURITY_USERNAME_ENABLE', True)
        flaskapp.config.setdefault('WTF_CSRF_CHECK_DEFAULT', False)

        from flask_wtf import CSRFProtect
        CSRFProtect(flaskapp)

        user_datastore = SQLAlchemyUserDatastore(db, User, Role)
        Security(flaskapp, user_datastore)

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
                except Exception:
                    LOGGER.debug('user:%s logout ', 'unknown')
                    als.als_json_log(
                        'user_access', {
                            'action': 'logout', 'user': 'unknown', 'from': flask.request.remote_addr})

        @flaskapp.after_request
        def log_user_accessed(response):
            if flask.request.method == 'POST' and flask.request.endpoint == 'user.login':
                LOGGER.debug(
                    'user:%s login status %d',
                    flask.request.form.get('username', ''),
                    response.status_code)
                username = flask.request.form.get('username',
                                                  flask.request.form.get('email', ''))
                if response.status_code != 302:
                    als.als_json_log('user_access',
                                     {'action': 'login',
                                      'user': username,
                                      'from': flask.request.remote_addr,
                                      'status': response.status_code})
                else:
                    als.als_json_log('user_access',
                                     {'action': 'login',
                                      'user': username,
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
        raise RuntimeError('Missing task directory')

    # Static file dispatchers
    if flaskapp.config['AFC_APP_TYPE'] == 'server':
        @flaskapp.before_request
        def enforce_auth():
            import werkzeug
            from flask_login import current_user
            # 1. No route matched (404 will be returned by Flask normally).
            if flask.request.endpoint is None:
                return
            # 2. Static file serving.
            if flask.request.endpoint.startswith('static'):
                return
            # 3. Flask-Security built-in auth views (login, register, confirm, etc.).
            if flask.request.endpoint.startswith(('user.', 'security.')):
                return
            # 4. Check @public_route annotation on class-based or function views.
            view_func = flaskapp.view_functions.get(flask.request.endpoint)
            if view_func:
                # Class-based view: check only the class's own __dict__ to avoid
                # inheriting is_public=True from a public parent class into a
                # session-authenticated subclass (e.g. RatAfcSec < RatAfc).
                if hasattr(view_func, 'view_class'):
                    view_class = view_func.view_class
                    if view_class.__dict__.get('is_public', False):
                        return
                    method_func = getattr(view_class, flask.request.method.lower(), None)
                    if method_func and method_func.__dict__.get('is_public', False):
                        return
                # Function-based view.
                elif view_func.__dict__.get('is_public', False):
                    return
            # 5. Check for internal-token auth (intra-cluster services such as
            # uls_downloader, als_siphon, rcache authenticate with
            # x-afc-internal-token instead of a session cookie).
            expected_token = os.environ.get("AFC_INTERNAL_TOKEN")
            if not expected_token:
                token_file = os.environ.get("AFC_INTERNAL_TOKEN_FILE", "")
                if token_file and os.path.isfile(token_file):
                    try:
                        with open(token_file, "r") as f:
                            expected_token = f.read().strip()
                    except Exception:
                        pass
            # 6. Fail-closed: require authenticated, active user.
            supplied_token = flask.request.headers.get("x-afc-internal-token") or ""
            if expected_token and hmac.compare_digest(supplied_token, expected_token):
                return
            if not current_user.is_authenticated:
                raise werkzeug.exceptions.Unauthorized()
            if not current_user.active:
                raise werkzeug.exceptions.Forbidden("Inactive user")

        @flaskapp.before_request
        def check_csrf_token():
            import werkzeug
            from flask_login import current_user
            if (flask.request.method in ('POST', 'PUT', 'DELETE')) and \
                    current_user.is_authenticated:
                header_token = flask.request.headers.get('X-Csrf-Token')
                cookie_token = flask.request.cookies.get('csrf_token')
                # Double-submit cookie check: the header must be present and
                # equal to the cookie value.  The SameSite=Strict cookie is
                # not accessible from different origins, so the header/cookie
                # match provides CSRF protection.  Rejecting a missing header
                # also prevents the None==None loophole.
                if not header_token or header_token != cookie_token:
                    LOGGER.error("CSRF token mismatch or missing header")
                    raise werkzeug.exceptions.BadRequest("CSRF token mismatch")
            flask.g.csrf_valid = True

        @flaskapp.after_request
        def make_csrf_cookie(response):
            from flask_login import current_user
            if current_user.is_authenticated:
                # SameSite=Strict + Secure flags applied for best cookie security posture.
                response.set_cookie(
                    'csrf_token', generate_csrf(),
                    samesite='Strict', secure=True)
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
            # Links to directories, generated by UlsFiles/AntennaFiles
            ('/ratapi/v1/files/uls_db', uls_databases, True),
            ('/ratapi/v1/files/antenna_pattern', antenna_patterns, True),
            # Links to files, generated by newfangled WsgiDav
            ('/ratapi/v1/ulsFiles', uls_databases, True),
            ('/ratapi/v1/antennaFiles', antenna_patterns, True),
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
                # Do NOT set trusted_auth_header (e.g. REMOTE_USER): it would let
                # a client assert an authenticated identity via a request header.
                # Authentication/authorization for these paths is enforced at the
                # nginx dispatcher via `auth_request /fbrat/_auth_check`
                # (see dispatcher/nginx.conf.template). All providers are
                # readonly=True.
                'verbose': (0, 1)[flaskapp.config['DEBUG']],
                'logging': {'enable_loggers': ['wsgidav']},
                'property_manager': False,  # True: use property_manager.PropertyManager
                'lock_storage': True,  # True: use lock_manager.LockManager
                # None: domain_controller.WsgiDAVDomainController(user_mapping)
                'simple_dc': {'user_mapping': {'*': True}},
            })
            # dav_wsgi_apps[app_sub_path] = wsgidav_app.WsgiDAVApp(dav_config)
            dav_wsgi_apps[url_path] = wsgidav_app.WsgiDAVApp(dav_config)
        # Wrap DAV apps that serve sensitive data in a bearer-token gate so
        # that intra-cluster callers still require the AFC internal token.
        # The /www path serves public static UI files to browsers that never
        # carry the internal token, so it is intentionally excluded.
        _PUBLIC_DAV_PATHS = {'/www'}
        _dav_app_with_auth = {}
        for url_path, dav_app in dav_wsgi_apps.items():
            if url_path in _PUBLIC_DAV_PATHS:
                _dav_app_with_auth[url_path] = dav_app
            else:
                _dav_app_with_auth[url_path] = _DavBearerTokenGate(dav_app)
        dav_wsgi_apps = _dav_app_with_auth
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
            exc_type, exc_value, tb = sys.exc_info()
            if exc_value is error:
                assert exc_type is not None  # exc_type is None iff exc_value is None
                raise exc_type(exc_value).with_traceback(tb)
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
            db.session.query(User).first()  # pylint: disable=no-member

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
    # The root and catch-all redirectors point at the React SPA's index.html
    # (served by WsgiDAV DispatcherMiddleware, which bypasses Flask routing).
    # They must be publicly accessible so unauthenticated users can load the
    # React app and see the login form before they have a session.
    _root_view = redirector('www.index', code=302)
    _root_view.is_public = True
    flaskapp.add_url_rule('/', 'root', view_func=_root_view)
    if ('AFC_MSGHND_WORKERS' in os.environ) and \
            (prometheus_utils.multiprocess_prometheus_configured()):
        def _token_gated_flask_metrics():
            expected = os.environ.get('AFC_INTERNAL_TOKEN', '')
            if not expected:
                token_file = os.environ.get('AFC_INTERNAL_TOKEN_FILE', '')
                if token_file and os.path.isfile(token_file):
                    with open(token_file, 'r') as f:
                        expected = f.read().strip()
            provided = flask.request.headers.get('X-AFC-Internal-Token', '')
            if not expected or not hmac.compare_digest(provided, expected):
                return flask.Response('Forbidden', status=403,
                                      mimetype='text/plain')
            return prometheus_utils.multiprocess_flask_metrics()
        flaskapp.add_url_rule(
            '/metrics', view_func=_token_gated_flask_metrics)

        @flaskapp.before_request
        def inc_active_counter_metric():
            prometheus_metric_flask_active_reqs.inc()

        @flaskapp.after_request
        def dec_active_counter_metric(response):
            prometheus_metric_flask_active_reqs.dec()
            return response

    if flaskapp.config['AFC_APP_TYPE'] == 'msghnd':
        # msghnd binds 0.0.0.0 on the Docker bridge with no nginx deny-list
        # in front of it; register only the AP-AFC blueprint this service
        # actually handles on the bridge interface.
        flaskapp.register_blueprint(views.ratafc.module, url_prefix='/ap-afc')
    else:
        flaskapp.register_blueprint(views.ratapi.module, url_prefix='/ratapi/v1')
        flaskapp.register_blueprint(views.ratafc.module, url_prefix='/ap-afc')
        flaskapp.register_blueprint(views.auth.module, url_prefix='/auth')
        flaskapp.register_blueprint(views.admin.module, url_prefix='/admin')
    # catch all invalid paths and redirect — must be public so the React SPA
    # loads for unauthenticated users (the SPA itself handles the login flow).
    if not flaskapp.config['DEBUG']:
        _any_view = redirector('www.index', code=302)
        _any_view.is_public = True
        flaskapp.add_url_rule('/<path:p>', 'any', view_func=_any_view)

    @flaskapp.errorhandler(400)
    def handle_bad_request(e):
        """Log 400 errors; return plain-text description so the WebUI can
        display it without rendering raw HTML."""
        LOGGER.error(
            "400 BadRequest on %s %s — %s\n%s",
            flask.request.method,
            flask.request.full_path,
            getattr(e, 'description', str(e)),
            traceback.format_exc())
        description = getattr(e, 'description', str(e))
        # Safe: Content-Type is text/plain (not HTML), so the browser will not
        # execute any markup in the body.  The description is a server-generated
        # Werkzeug exception message (e.g. "CSRF token mismatch"), not echoed
        # user input, so it is safe to render.
        return flask.make_response(description, 400,
                                   {'Content-Type': 'text/plain'})

    return flaskapp
