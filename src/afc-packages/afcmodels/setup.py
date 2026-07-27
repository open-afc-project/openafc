from setuptools import setup
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
        'Flask==3.1.3',
        'Flask-SQLAlchemy==3.1.1',
        'pydantic>=2.0',
        'pydantic-settings>=2.0',
    ]
)
