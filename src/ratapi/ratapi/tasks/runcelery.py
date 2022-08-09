import os
import logging
import xdg.BaseDirectory
from flask.config import Config
LOGGER = logging.getLogger(__name__)


def init_config(app_config):
    from .. import config as default_config

    # default config state from module
    app_config.from_object(default_config)
    # initial override from system config
    config_path = xdg.BaseDirectory.load_first_config('fbrat', 'ratapi.conf')
    if config_path:
        app_config.from_pyfile(config_path)

    # build broker url with values from environment variables and
    # default values from configuration as fallback.
    app_config.update(
        BROKER_URL=os.getenv('BROKER_PROT',
                             app_config['BROKER_DEFAULT_PROT']) +
                   "://" +
                   os.getenv('BROKER_USER',
                             app_config['BROKER_DEFAULT_USER']) +
                   ":" +
                   os.getenv('BROKER_PWD',
                             app_config['BROKER_DEFAULT_PWD']) +
                   "@" +
                   os.getenv('BROKER_FQDN',
                             app_config['BROKER_DEFAULT_FQDN']) +
                   ":" +
                   os.getenv('BROKER_PORT',
                             app_config['BROKER_DEFAULT_PORT']) +
                   "/fbrat"
    )
    return app_config
