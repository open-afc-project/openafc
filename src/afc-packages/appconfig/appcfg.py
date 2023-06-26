# This Python file uses the following encoding: utf-8
#
# Portions copyright © 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate
# affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy
# of which is included with this software program.
#

''' Application configuration data.
'''
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
USER_APP_NAME = "RLAN AFC Tool"  # Shown in and email templates and page footers
USER_ENABLE_EMAIL = True  # Enable email authentication
USER_ENABLE_CONFIRM_EMAIL = False # Disable email confirmation
USER_ENABLE_USERNAME = True  # Enable username authentication
USER_EMAIL_SENDER_NAME = USER_APP_NAME
USER_EMAIL_SENDER_EMAIL = None
REMEMBER_COOKIE_DURATION = datetime.timedelta(days=30) # remember me timeout
USER_USER_SESSION_EXPIRATION = 3600 # One hour idle timeout
PERMANENT_SESSION_LIFETIME = datetime.timedelta(seconds=USER_USER_SESSION_EXPIRATION) # idle session timeout

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
        self.BROKER_PWD  = os.getenv('BROKER_PWD', 'celery')
        self.BROKER_FQDN = os.getenv('BROKER_FQDN', 'rmq')
        self.BROKER_PORT = os.getenv('BROKER_PORT', '5672')
        self.BROKER_VHOST = os.getenv('BROKER_VHOST','fbrat')
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
        self.BROKER_EXCH_DISPAT = os.getenv('BROKER_EXCH_DISPAT', 'dispatcher_bcast')

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


class OIDCConfigurator(object):

    def __init__(self):
        # OIDC default configurations
        self.OIDC_LOGIN = False
        self.OIDC_CLIENT_ID = ""
        self.OIDC_CLIENT_SECRET = ""
        self.OIDC_DISCOVERY_URL = ""

        bool_attr = ['OIDC_LOGIN']
        str_attr = ['OIDC_CLIENT_ID', 'OIDC_CLIENT_SECRET', 'OIDC_DISCOVERY_URL']
        attr = bool_attr + str_attr

        # load priv config if available.
        try:
           from ratapi import priv_config
           for k in attr:
               val = getattr(priv_config, k, None)
               if not val is None:
                   setattr(self, k, val)
        except:
           priv_config = None

        # override boolean config with environment variables
        for k in bool_attr:
            # Override with environment variables
            ret = os.getenv(k)
            if ret:
                setattr(self, k, (ret.lower() == 'true'))

        # override string config with environment variables
        for k in str_attr:
            ret = os.getenv(k)
            if ret:
                setattr(self, k, ret)

        # Override values from config with secret file
        if self.OIDC_LOGIN:
            OIDC_ARG=os.getenv('OIDC_ARG')
            if OIDC_ARG:
                import json
                with open(OIDC_ARG) as oidc_config:
                    data = json.load(oidc_config)
                    self.OIDC_CLIENT_SECRET=data['OIDC_CLIENT_SECRET']
                    self.OIDC_CLIENT_ID=data['OIDC_CLIENT_ID']
                    self.OIDC_DISCOVERY_URL=data['OIDC_DISCOVERY_URL']
