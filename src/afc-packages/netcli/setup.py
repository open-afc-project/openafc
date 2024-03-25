from setuptools import setup, find_packages
from setuptools.command.install import install
import os
import inspect


class InstallCmdWrapper(install):
    def run(self):
        print(f"{inspect.stack()[0][3]}()")
        install.run(self)


setup(
    name='netcli',
    # Label compatible with PEP 440
    version='0.1.0',
    description='AFC packages',
    py_modules=["ncli"],
    cmdclass={
        'install': InstallCmdWrapper,
    }
)
