from setuptools import setup, find_packages
from setuptools.command.install import install
import os

class InstallCmdWrapper(install):
    def run(self):
        install.run(self)

setup(
    name='als',
    # Label compatible with PEP 440
    version='0.1.0',
    description='ALS Client module',
    py_modules=["als"],
    cmdclass={
        'install': InstallCmdWrapper,
    }
)
