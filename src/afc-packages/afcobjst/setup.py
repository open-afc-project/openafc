from setuptools import setup
from setuptools.command.install import install


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
    install_requires=["requests==2.34.2", "flask==3.1.3", "werkzeug==3.1.8",
                      "waitress==3.0.1", "google.cloud.storage==2.9.0", "posix_ipc==1.1.1"],
    cmdclass={
        'install': InstallCmdWrapper,
    }
)
