#!/usr/bin/env python3
#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Description

The acceptor client (aka consumer) registeres own queue within broker
application (aka rabbitmq). Such queue used as a channel for control commands.
"""

from appcfg import BrokerConfigurator, ObjstConfig
import os
import sys
from sys import stdout
import logging
from logging.config import dictConfig
import argparse
import inspect
import gevent
import subprocess
import shutil
import time
from ncli import MsgAcceptor
from hchecks import MsghndHealthcheck, ObjstHealthcheck
from fst import DataIf

dictConfig({
    'version': 1,
    'disable_existing_loggers': False,
    'formatters': {
        'standard': {
            'format': '%(asctime)s - [%(levelname)s] %(name)s [%(module)s.%(funcName)s:%(lineno)d]: %(message)s',
            'datefmt': '%Y-%m-%d %H:%M:%S',
        }
    },
    'handlers': {
        'console': {
            'level': 'DEBUG',
            'class': 'logging.StreamHandler',
            'formatter': 'standard',
        }
    },
    'root': {
        'level': 'INFO',
        'handlers': ['console']
    },
})
app_log = logging.getLogger()


class Configurator(dict):
    __instance = None

    def __new__(cls):
        if cls.__instance is None:
            cls.__instance = dict.__new__(cls)
        return cls.__instance

    def __init__(self):
        dict.__init__(self)
        self.update(BrokerConfigurator().__dict__.items())
        self.update(ObjstConfig().__dict__.items())
        self['OBJST_CERT_CLI_BUNDLE'] = \
            'certificate/client.bundle.pem'
        self['DISPAT_CERT_CLI_BUNDLE'] = \
            '/etc/nginx/certs/client.bundle.pem'
        self['DISPAT_CERT_CLI_BUNDLE_DFLT'] = \
            '/etc/nginx/certs/client.bundle.pem_dflt'


log_level_map = {
    'debug': logging.DEBUG,    # 10
    'info': logging.INFO,      # 20
    'warn': logging.WARNING,   # 30
    'err': logging.ERROR,      # 40
    'crit': logging.CRITICAL,  # 50
}


def set_log_level(opt) -> int:
    app_log.info(f"({os.getpid()}) {inspect.stack()[0][3]}() "
                 f"{app_log.getEffectiveLevel()}")
    app_log.setLevel(log_level_map[opt])
    return log_level_map[opt]


def readiness_check(cfg):
    """Provide readiness check by calling for response preconfigured
       list of subjects (containers)
    """
    app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
    objst_chk = ObjstHealthcheck(cfg)
    msghnd_chk = MsghndHealthcheck.from_hcheck_if()
    checks = [gevent.spawn(objst_chk.healthcheck),
              gevent.spawn(msghnd_chk.healthcheck)]
    gevent.joinall(checks)
    for i in checks:
        if i.value != 0:
            return i.value
    return 0


def run_restart(cfg):
    """Get messages"""
    app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
    with DataIf().open(cfg['OBJST_CERT_CLI_BUNDLE']) as hfile:
        if hfile.head():
            app_log.debug(f"Found cert bundle file.")
            with open(cfg['DISPAT_CERT_CLI_BUNDLE'], 'w') as ofile:
                ofile.write(hfile.read().decode('utf-8'))
            app_log.info(f"{os.path.getctime(cfg['DISPAT_CERT_CLI_BUNDLE'])}, "
                         f"{os.path.getsize(cfg['DISPAT_CERT_CLI_BUNDLE'])}")
        else:
            app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
            # use default certificate (placeholder)
            # in any case of missing file, no more certificates included
            app_log.info(f"Misssing certificate file "
                         f"{cfg['OBJST_CERT_CLI_BUNDLE']}, back to default "
                         f"{cfg['DISPAT_CERT_CLI_BUNDLE_DFLT']}")
            shutil.copy2(cfg['DISPAT_CERT_CLI_BUNDLE_DFLT'],
                         cfg['DISPAT_CERT_CLI_BUNDLE'])
        p = subprocess.Popen("nginx -s reload",
                             stdout=subprocess.PIPE, shell=True)
        app_log.info(f"{p.communicate()}")


def run_remove(cfg):
    """Get messages"""
    app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}() "
                  f"{cfg['DISPAT_CERT_CLI_BUNDLE']}")
    os.unlink(cfg['DISPAT_CERT_CLI_BUNDLE'])
    # restore builtin certifiicate from backup
    app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}() "
                  f"restore default certificate "
                  f"{cfg['DISPAT_CERT_CLI_BUNDLE_DFLT']}")
    shutil.copy2(cfg['DISPAT_CERT_CLI_BUNDLE_DFLT'],
                 cfg['DISPAT_CERT_CLI_BUNDLE'])
    p = subprocess.Popen("nginx -s reload",
                         stdout=subprocess.PIPE, shell=True)
    app_log.info(f"{p.communicate()}")


commands_map = {
    'cmd_restart': run_restart,
    'cmd_remove': run_remove,
}


def get_commands(cfg, msg):
    """Get messages"""
    app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
    commands_map[msg](cfg)


def run_it(cfg):
    """Execute command line run command"""
    app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")

    # backup builtin certifiicate as a default one
    shutil.copy2(cfg['DISPAT_CERT_CLI_BUNDLE'],
                 cfg['DISPAT_CERT_CLI_BUNDLE_DFLT'])
    # check if lucky to find new certificate bundle already
    run_restart(cfg)

    maker = MsgAcceptor(cfg['BROKER_URL'], cfg['BROKER_EXCH_DISPAT'],
                        msg_handler=get_commands, handler_params=cfg)
    app_log.info(f"({os.getpid()}) Connected to {cfg['BROKER_URL']}")
    maker.run()


# available commands to execute in alphabetical order
execution_map = {
    'run': run_it,
    'check': readiness_check,
}


def make_arg_parser():
    """Define command line options"""
    args_parser = argparse.ArgumentParser(
        epilog=__doc__.strip(),
        formatter_class=argparse.RawTextHelpFormatter)
    args_parser.add_argument('--log', type=set_log_level,
                             default='info', dest='log_level',
                             help="<info|debug|warn|err|crit> - set "
                             "logging level (default=info).\n")
    args_parser.add_argument('--cmd', choices=execution_map.keys(),
                             nargs='?',
                             help="run - start accepting commands.\n"
                             "check - run readiness check.\n")

    return args_parser


def prepare_args(parser, cfg):
    """Prepare required parameters"""
    app_log.debug(f"{inspect.stack()[0][3]}() {parser.parse_args()}")
    cfg.update(vars(parser.parse_args()))


def main():
    """Main function of the utility"""
    res = 0
    parser = make_arg_parser()
    config = Configurator()

    if prepare_args(parser, config) == 1:
        # error in preparing arguments
        res = 1
    else:
        if isinstance(config['cmd'], type(None)):
            parser.print_help()

    if res == 0:
        app_log.debug(f"{inspect.stack()[0][3]}() {config}")
        res = execution_map[config['cmd']](config)
    sys.exit(res)


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(1)


# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
