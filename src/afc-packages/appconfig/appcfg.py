# This Python file uses the following encoding: utf-8
#
# Portions copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate
# affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy
# of which is included with this software program.
#

''' Application configuration data.
'''
import abc
import os
import logging
import datetime

#: The externally-visible script root path
APPLICATION_ROOT = '/fbrat'
#: Enable debug mode for flask
DEBUG = False
#: Enable detailed exception stack messages
PROPAGATE_EXCEPTIONS = False
#: Root logger filter
AFC_RATAPI_LOG_LEVEL = os.getenv("AFC_RATAPI_LOG_LEVEL", "WARNING")
# Default request timeout in seconds
AFC_MSGHND_RATAFC_TOUT = 600
#: Set of log handlers to use for root logger
LOG_HANDLERS = [
    logging.StreamHandler(),
]

SECRET_KEY = None  # Must be set in app config

# Flask-SQLAlchemy settings
# postgresql://[user[:password]@][netloc][:port][/dbname][?param1=value1&...]
SQLALCHEMY_DATABASE_URI = None  # Must be set in app config
SQLALCHEMY_TRACK_MODIFICATIONS = False  # Avoids SQLAlchemy warning

# Flask-User settings
USER_APP_NAME = "AFC"  # Shown in and email templates and page footers
USER_ENABLE_EMAIL = True  # Enable email authentication
USER_ENABLE_CONFIRM_EMAIL = False  # Disable email confirmation
USER_ENABLE_USERNAME = True  # Enable username authentication
USER_EMAIL_SENDER_NAME = USER_APP_NAME
USER_EMAIL_SENDER_EMAIL = None
REMEMBER_COOKIE_DURATION = datetime.timedelta(days=30)  # remember me timeout
USER_USER_SESSION_EXPIRATION = 3600  # One hour idle timeout
PERMANENT_SESSION_LIFETIME = datetime.timedelta(
    seconds=USER_USER_SESSION_EXPIRATION)  # idle session timeout
USER_LOGIN_TEMPLATE = 'login.html'
USER_REGISTER_TEMPLATE = 'register.html'

#: API key used for google maps
GOOGLE_APIKEY = None
#: Dynamic system data (both model data and configuration)
STATE_ROOT_PATH = '/var/lib/fbrat'
#: Mount path
NFS_MOUNT_PATH = '/mnt/nfs'
#: Use random PAWS response flag
PAWS_RANDOM = False
#: History directory for log file storage
HISTORY_DIR = None
#: Task queue directory
TASK_QUEUE = '/var/spool/fbrat'

#: Tracks if the daily uls parser ran today. Overwritten by the tasks that use it.
DAILY_ULS_RAN_TODAY = False


class InvalidEnvVar(Exception):
    """Wrong/missing env var exception"""
    pass


class BrokerConfigurator(object):
    """Keep configuration for a broker"""

    def __init__(self) -> None:
        self.BROKER_PROT = os.getenv('BROKER_PROT', 'amqp')
        self.BROKER_USER = os.getenv('BROKER_USER', 'celery')
        self.BROKER_PWD = os.getenv('BROKER_PWD', 'celery')
        self.BROKER_FQDN = os.getenv('BROKER_FQDN', 'rmq')
        self.BROKER_PORT = os.getenv('BROKER_PORT', '5672')
        self.BROKER_VHOST = os.getenv('BROKER_VHOST', 'fbrat')
        self.BROKER_URL = self.BROKER_PROT +\
            "://" +\
            self.BROKER_USER +\
            ":" +\
            self.BROKER_PWD +\
            "@" +\
            self.BROKER_FQDN +\
            ":" +\
            self.BROKER_PORT +\
            "/" +\
            self.BROKER_VHOST
        self.BROKER_EXCH_DISPAT = os.getenv(
            'BROKER_EXCH_DISPAT', 'dispatcher_bcast')


class ObjstConfigBase():
    """Parent of configuration for objstorage"""

    def __init__(self):
        self.AFC_OBJST_PORT = os.getenv("AFC_OBJST_PORT", "5000")
        if not self.AFC_OBJST_PORT.isdigit():
            raise InvalidEnvVar("Invalid AFC_OBJST_PORT env var.")
        self.AFC_OBJST_HIST_PORT = os.getenv("AFC_OBJST_HIST_PORT", "4999")
        if not self.AFC_OBJST_HIST_PORT.isdigit():
            raise InvalidEnvVar("Invalid AFC_OBJST_HIST_PORT env var.")


class ObjstConfig(ObjstConfigBase):
    """Filestorage external config"""

    def __init__(self):
        ObjstConfigBase.__init__(self)
        self.AFC_OBJST_HOST = os.getenv("AFC_OBJST_HOST")

        self.AFC_OBJST_SCHEME = None
        if "AFC_OBJST_SCHEME" in os.environ:
            self.AFC_OBJST_SCHEME = os.environ["AFC_OBJST_SCHEME"]
            if self.AFC_OBJST_SCHEME not in ("HTTPS", "HTTP"):
                raise InvalidEnvVar("Invalid AFC_OBJST_SCHEME env var.")


class SecretConfigurator(object):

    def __init__(self, secret_env, file_env_prefix, bool_attr, str_attr,
                 int_attr):
        attr = bool_attr + str_attr + int_attr

        # Initialize to false and empty
        for k in bool_attr:
            setattr(self, k, False)

        for k in str_attr:
            setattr(self, k, "")

        for k in int_attr:
            setattr(self, k, 0)

        # load priv config if available.
        try:
            from ratapi import priv_config
            for k in attr:
                val = getattr(priv_config, k, None)
                if val is not None:
                    setattr(self, k, val)
        except BaseException:
            priv_config = None

        # override boolean config with environment variables
        for k in bool_attr:
            # Override with environment variables
            ret = self._getenv(k, file_env_prefix)
            if ret:
                setattr(self, k, (ret.lower() == 'true'))

        # override string config with environment variables
        for k in str_attr:
            ret = self._getenv(k, file_env_prefix)
            if ret:
                setattr(self, k, ret)

        # override int config with environment variables
        for k in int_attr:
            ret = self._getenv(k, file_env_prefix)
            if ret:
                setattr(self, k, int(ret))

        # Override values from config with secret file
        secret_file = os.getenv(secret_env)
        if secret_file:
            import json
            with open(secret_file) as secret_content:
                data = json.load(secret_content)
                for k in bool_attr:
                    if k in data:
                        setattr(self, k, data[k].lower() == 'true')
                for k in str_attr:
                    if k in data:
                        setattr(self, k, data[k])
                for k in int_attr:
                    if k in data:
                        setattr(self, k, int(data[k]))

    def _getenv(self, attr, file_env_prefix):
        ret = os.environ.get(attr)
        if ret:
            return ret
        filename = os.environ.get(file_env_prefix + attr)
        if filename and os.path.isfile(filename):
            with open(filename, encoding="utf-8") as f:
                return f.read()
        return None


class OIDCConfigurator(SecretConfigurator):
    def __init__(self):
        oidc_bool_attr = ['OIDC_LOGIN']
        oidc_str_attr = ['OIDC_CLIENT_ID',
                         'OIDC_CLIENT_SECRET', 'OIDC_DISCOVERY_URL']
        oidc_env = 'OIDC_ARG'
        super().__init__(secret_env=oidc_env, file_env_prefix='OIDCFILE_',
                         bool_attr=oidc_bool_attr, str_attr=oidc_str_attr,
                         int_attr=[])


class RatApiConfigurator(SecretConfigurator):
    def __init__(self):
        ratapi_bool_attr = ['MAIL_USE_TLS', 'MAIL_USE_SSL', 'USE_CAPTCHA']
        ratapi_str_attr = [
            'REGISTRATION_APPROVE_LINK',
            'REGISTRATION_DEST_EMAIL',
            'REGISTRATION_DEST_PDL_EMAIL',
            'REGISTRATION_SRC_EMAIL',
            'MAIL_PASSWORD',
            'MAIL_USERNAME',
            'MAIL_SERVER',
            'CAPTCHA_SECRET',
            'CAPTCHA_SITEKEY',
            'CAPTCHA_VERIFY',
            'USER_APP_NAME']
        ratapi_int_attr = ['MAIL_PORT']
        ratapi_env = 'RATAPI_ARG'
        super().__init__(secret_env=ratapi_env, file_env_prefix='RATAPIFILE_',
                         bool_attr=ratapi_bool_attr, str_attr=ratapi_str_attr,
                         int_attr=ratapi_int_attr)


# Msghnd configuration interfaces

class MsghndConfiguration(abc.ABC):
    @abc.abstractmethod
    def get_name(self):
        # AFC_MSGHND_NAME
        pass

    @abc.abstractmethod
    def get_port(self):
        # AFC_MSGHND_PORT
        pass

    @abc.abstractmethod
    def get_bind(self):
        # AFC_MSGHND_BIND
        pass

    @abc.abstractmethod
    def get_access_log(self):
        # AFC_MSGHND_ACCESS_LOG
        pass

    @abc.abstractmethod
    def get_error_log(self):
        # AFC_MSGHND_ERROR_LOG
        pass

    @abc.abstractmethod
    def get_workers(self):
        # AFC_MSGHND_WORKERS
        pass

    @abc.abstractmethod
    def get_timeout(self):
        # AFC_MSGHND_TIMEOUT
        pass


class HealthchecksMsghndCfgIface(MsghndConfiguration):
    def __init__(self):
        setattr(self, 'AFC_MSGHND_NAME', os.getenv('AFC_MSGHND_NAME'))
        setattr(self, 'AFC_MSGHND_PORT', os.getenv('AFC_MSGHND_PORT'))

    def get_name(self):
        return self.AFC_MSGHND_NAME

    def get_port(self):
        return self.AFC_MSGHND_PORT

    def get_bind(self):
        pass

    def get_access_log(self):
        pass

    def get_error_log(self):
        pass

    def get_workers(self):
        pass

    def get_timeout(self):
        pass


class RatafcMsghndCfgIface(MsghndConfiguration):
    def get_name(self):
        pass

    def get_port(self):
        pass

    def get_bind(self):
        pass

    def get_access_log(self):
        pass

    def get_error_log(self):
        pass

    def get_workers(self):
        pass

    def get_timeout(self):
        pass
