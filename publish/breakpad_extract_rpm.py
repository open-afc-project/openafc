#!/usr/bin/env python
import sys
import os
import argparse
import subprocess
import shutil
import logging
import tempfile

LOGGER = logging.getLogger()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--verbose', type=int, default=0,
                        help='''\
Verboisty levek:
0 is none, 1 is INFO, 2 is DEBUG.
Default is %(default)s.''')
    parser.add_argument('symboldir', type=str,
                        help='Output breakpad symbol tree path.')
    parser.add_argument('packages', type=str, nargs='+',
                        help='''Input packages to extract and read.''')
    args = parser.parse_args()

    if args.verbose >= 2:
        log_level = logging.DEBUG
    elif args.verbose == 1:
        log_level = logging.INFO
    else:
        log_level = logging.WARNING
    logging.basicConfig(level=log_level)

    symbol_path = os.path.abspath(args.symboldir)
    if not os.path.isdir(symbol_path):
        os.path.makedirs(symbol_path)

    tmp_path = tempfile.mkdtemp()
    LOGGER.debug('Temporary package contents under %s', tmp_path)

    bitbucket = open(os.devnull, 'wb')
    for pkg_name in args.packages:
        LOGGER.info('Extracting package %s ...', pkg_name)
        cpio = subprocess.Popen(
            ['cpio', '-idm'], cwd=tmp_path,
            stdin=subprocess.PIPE,
            stderr=bitbucket,
        )
        rpm = subprocess.Popen(
            ['rpm2cpio', pkg_name],
            stdout=cpio.stdin
        )
        (stdout, stderr) = rpm.communicate()
        if rpm.returncode != 0:
            LOGGER.error('rpm2cpio stderr:\n%s', stderr)
            raise RuntimeError('Failed to run rpm2cpio')
        (stdout, stderr) = cpio.communicate()
        if cpio.returncode != 0:
            LOGGER.error('cpio stderr:\n%s', stderr)
            raise RuntimeError('Failed to run cpio')

    LOGGER.info('Extracting all symbols...')
    subprocess.check_call(
        [
            'python', 'breakpad_extract.py',
            '--verbose={0}'.format(args.verbose),
            '--fileroot={0}'.format(tmp_path),
            symbol_path,
            '{0}/usr/lib64'.format(tmp_path),
            '{0}/usr/bin'.format(tmp_path),
            '{0}/usr/sbin'.format(tmp_path),
        ],
    )

    LOGGER.debug('Cleaning up %s', tmp_path)
    shutil.rmtree(tmp_path)


if __name__ == '__main__':
    sys.exit(main())
