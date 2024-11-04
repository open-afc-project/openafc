from setuptools import setup, find_packages
from setuptools.command.install import install
import os


class InstallCmdWrapper(install):
    def run(self):
        install.run(self)


setup(
    name='afcobjst',
    # Label compatible with PEP 440
    version='1.0.0',
    description='AFC packages',
    py_modules=["afcobjst"],
    packages=["afcobjst"],
    install_requires=["requests==2.32.3", "flask==2.3.2", "werkzeug==3.0.6",
                      "waitress==2.1.2", "google.cloud.storage==2.9.0", "posix_ipc==1.1.1"],
    cmdclass={
        'install': InstallCmdWrapper,
    }
)
