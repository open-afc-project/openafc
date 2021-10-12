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
    return app_config
