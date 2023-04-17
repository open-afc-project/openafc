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

from appcfg import BrokerConfigurator
import os
import sys
import logging
from logging.config import dictConfig
import argparse
import inspect
import subprocess
from ncli import MsgAcceptor
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
   'handlers' : {
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


log_level_map = {
    'debug' : logging.DEBUG,    # 10
    'info' : logging.INFO,      # 20
    'warn' : logging.WARNING,   # 30
    'err' : logging.ERROR,      # 40
    'crit' : logging.CRITICAL,  # 50
}

def set_log_level(opt) -> int:
    app_log.info(f"({os.getpid()}) {inspect.stack()[0][3]}() "
                 f"{app_log.getEffectiveLevel()}")
    app_log.setLevel(log_level_map[opt])
    return log_level_map[opt]

def get_commands(msg):
    """Get messages"""
    app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
    app_log.info(f"{msg}")
    dataif = DataIf()
    with dataif.open("/certificate/client.bundle.pem") as hfile:
        if not hfile.head():
            app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}() ")
            app_log.error(f"Misssing certificate file")
        else:
            with open('/certificates/clients/client.bundle.pem', 'w') as ofile:
                ofile.write(hfile.read().decode('utf-8'))
            app_log.info(f"{os.path.getctime('/certificates/clients/client.bundle.pem')}")
            p = subprocess.Popen("nginx -s reload",
                                 stdout=subprocess.PIPE, shell=True)
            app_log.info(f"{p.communicate()}")

def run_it(cfg):
    """Execute command line run command"""
    app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
    maker = MsgAcceptor(cfg['BROKER_URL'], cfg['BROKER_EXCH_DISPAT'],
                        msg_handler=get_commands)
    app_log.info(f"({os.getpid()}) Connected to {cfg['BROKER_URL']}")
    maker.run()


# available commands to execute in alphabetical order
execution_map = {
    'run' : run_it,
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
        help="run - start accepting commands.\n")

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
