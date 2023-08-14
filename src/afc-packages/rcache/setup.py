from setuptools import setup
from setuptools.command.install import install


class InstallCmdWrapper(install):
    def run(self):
        install.run(self)


setup(
    name='rcache',
    # Label compatible with PEP 440
    version='0.1.0',
    description='AFC packages',
    py_modules=["rcache_client", "rcache_common", "rcache_db", "rcache_models",
                "rcache_rcache", "rcache_rmq"],
    cmdclass={
        'install': InstallCmdWrapper,
    }
)
