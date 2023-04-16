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
import distutils.spawn
import logging
import datetime

#: The externally-visible script root path
APPLICATION_ROOT = '/fbrat'
#: Enable debug mode for flask
DEBUG = False
#: Enable detailed exception stack messages
PROPAGATE_EXCEPTIONS = False
#: Root logger filter
LOG_LEVEL = logging.WARNING
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
#: AFC Engine executable
AFC_ENGINE = distutils.spawn.find_executable('afc-engine')
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

class BrokerConfigurator(object):
    """Keep configuration for a broker"""
    BROKER_DEFAULT_PROT = 'amqp'
    BROKER_DEFAULT_USER = 'celery'
    BROKER_DEFAULT_PWD  = 'celery'
    BROKER_DEFAULT_FQDN = 'rmq'
    BROKER_DEFAULT_PORT = '5672'
    BROKER_DEFAULT_VHOST = 'fbrat'

    def __init__(self) -> None:
        self.BROKER_PROT = os.getenv('BROKER_PROT',
                                BrokerConfigurator.BROKER_DEFAULT_PROT)
        self.BROKER_USER = os.getenv('BROKER_USER',
                                BrokerConfigurator.BROKER_DEFAULT_USER)
        self.BROKER_PWD  = os.getenv('BROKER_PWD',
                                BrokerConfigurator.BROKER_DEFAULT_PWD)
        self.BROKER_FQDN = os.getenv('BROKER_FQDN',
                                BrokerConfigurator.BROKER_DEFAULT_FQDN)
        self.BROKER_PORT = os.getenv('BROKER_PORT',
                                BrokerConfigurator.BROKER_DEFAULT_PORT)
        self.BROKER_VHOST = os.getenv('BROKER_VHOST',
                                BrokerConfigurator.BROKER_DEFAULT_VHOST)
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

