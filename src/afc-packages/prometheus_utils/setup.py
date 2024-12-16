""" Prometheus client utility stuff for Flask """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

from setuptools import setup
from setuptools.command.install import install


class InstallCmdWrapper(install):
    def run(self):
        install.run(self)


setup(
    name='prometheus_utils',
    # Label compatible with PEP 440
    version='0.1.0',
    description='AFC packages',
    py_modules=["prometheus_utils"],
    cmdclass={
        'install': InstallCmdWrapper,
    }
)
