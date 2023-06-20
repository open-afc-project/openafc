from setuptools import setup, find_packages
from setuptools.command.install import install
import inspect

class InstallCmdWrapper(install):
    def run(self):
        print(f"{inspect.stack()[0][3]}()")
        install.run(self)

setup(
    name='afcmodels',
    # Label compatible with PEP 440
    version='0.1.0',
    description='AFC packages',
    packages=['afcmodels'],
    cmdclass={
        'install': InstallCmdWrapper,
    },
    install_requires=[
        'Flask==2.3.2',
        'Flask-SQLAlchemy==2.5.1' 
    ]
)
