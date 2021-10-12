''' Application configuration data.
'''
import distutils.spawn
import logging


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
USER_ENABLE_USERNAME = False  # Disable username authentication
USER_EMAIL_SENDER_NAME = USER_APP_NAME
USER_EMAIL_SENDER_EMAIL = None

#: API key used for google maps
GOOGLE_APIKEY = None
#: Dynamic system data (both model data and configuration)
STATE_ROOT_PATH = '/var/lib/fbrat'
#: AFC Engine executable
AFC_ENGINE = distutils.spawn.find_executable('afc-engine')
#: Use random PAWS response flag
PAWS_RANDOM = False
#: History directory for log file storage
HISTORY_DIR = None
#: Task queue directory
TASK_QUEUE = '/var/spool/fbrat'

# Celery configuration
#: protocol to use to commuicate with rabbitmq
CELERY_RESULT_BACKEND = 'file:///var/celery/results'
#: address of rabbitmq server
BROKER_URL = 'amqp://celery:celery@localhost:5672/fbrat'
