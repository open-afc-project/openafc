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

# OIDC default configurations
OIDC_LOGIN = False
OIDC_CLIENT_ID = ""
OIDC_CLIENT_SECRET = ""
OIDC_DISCOVERY_URL = ""


# CAPTCHA FOR USER REGISTRATION
USE_CAPTCHA=False
CAPTCHA_SECRET=''
CAPTCHA_SITEKEY=''
CAPTCHA_VERIFY=''

# EMAIL FOR USER REGISTRATION
# MAIL SERVER Config.  If MAIL_SERVER is set, all MAIL SERVER config needs to be set.
# If MAIL_SERVER is NOT set, local mail server is used, do not set any of of the MAIL configs below.
#MAIL_SERVER= ''
#MAIL_PORT= 465
#MAIL_USE_TLS = False
#MAIL_USE_SSL = True
#MAIL_USERNAME= ''
#MAIL_PASSWORD = ''

# EMAIL configurations not related to MAIL SERVER. Control contents of the registration email
REGISTRATION_DEST_EMAIL = ''
REGISTRATION_SRC_EMAIL = ""
REGISTRATION_APPROVE_LINK=''
ABOUT_CONTENT=''


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
    """Parent of configuration for filestorage"""
    def __init__(self):
        self.AFC_OBJST_PORT = os.getenv("AFC_OBJST_PORT", "5000")
        if not self.AFC_OBJST_PORT.isdigit():
            raise InvalidEnvVar("Invalid AFC_OBJST_PORT env var.")
        self.AFC_OBJST_HIST_PORT = os.getenv("AFC_OBJST_HIST_PORT", "4999")
        if not self.AFC_OBJST_HIST_PORT.isdigit():
            raise InvalidEnvVar("Invalid AFC_OBJST_HIST_PORT env var.")


class ObjstConfig(ObjstConfigBase):
    """Filestorage internal config"""
    def __init__(self):
        ObjstConfigBase.__init__(self)
        self.AFC_OBJST_LOG_FILE = os.getenv("AFC_OBJST_LOG_FILE", "/proc/self/fd/2")
        self.AFC_OBJST_LOG_LVL = os.getenv("AFC_OBJST_LOG_LVL", "ERROR")
        # supported AFC_OBJST_MEDIA backends are "GoogleCloudBucket" and "LocalFS"
        self.AFC_OBJST_MEDIA = os.getenv("AFC_OBJST_MEDIA", "LocalFS")
        if self.AFC_OBJST_MEDIA not in ("GoogleCloudBucket", "LocalFS"):
            raise InvalidEnvVar("Invalid AFC_OBJST_MEDIA env var.")
        if self.AFC_OBJST_MEDIA == "LocalFS":
            # file download/upload location on the server in case of AFC_OBJST_MEDIA=LocalFS
            self.AFC_OBJST_FILE_LOCATION = os.getenv("AFC_OBJST_LOCAL_DIR", "/storage")
        else:
            self.AFC_OBJST_GOOGLE_CLOUD_CREDENTIALS_JSON = os.getenv("AFC_OBJST_GOOGLE_CLOUD_CREDENTIALS_JSON")
            self.AFC_OBJST_GOOGLE_CLOUD_BUCKET = os.getenv("AFC_OBJST_GOOGLE_CLOUD_BUCKET")

