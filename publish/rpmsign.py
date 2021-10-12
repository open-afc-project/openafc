#!/usr/bin/python2
# Uses CentOS packages: python2-gnupg pexpect rpm-sign
import argparse
import contextlib
import gnupg
import logging
import os
import pexpect
import shutil
import subprocess
import sys
import tempfile
from .util import GpgHome, RpmSigner

LOGGER = logging.getLogger(__name__)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--log-level', dest='log_level', default='info',
                        metavar='LEVEL',
                        help='Console logging lowest level displayed. Defaults to info.')
    parser.add_argument('--keyid',
                        help='The key ID to sign with. Mandatory if the keyring has more than one keypair.')
    parser.add_argument('--passphrase-fd', type=int, default=0,
                        help='The FD to read passphrase from. Defaults to stdin.')
    parser.add_argument('gpgkey', type=str,
                        help='GPG-encoded private keypair file.')
    parser.add_argument('rpmfile',
                        nargs='+',
                        help='RPM file(s) to sign.')
    args = parser.parse_args()

    log_level_name = args.log_level.upper()
    logging.basicConfig(level=log_level_name)
    
    with contextlib.closing(GpgHome()) as gpghome:
        with contextlib.closing(open(args.gpgkey, 'rb')) as gpgfile:
            gpghome.gpg.import_keys(gpgfile.read())
        with contextlib.closing(os.fdopen(args.passphrase_fd, 'rb')) as ppfile:
            passphrase = ppfile.read()

        # Ensure desired ID is present
        use_key_id = gpghome.find_key(args.keyid)
        # Verify passphrase before use
        gpghome.check_key(use_key_id, passphrase)
        LOGGER.info('Signing with key ID "%s"', use_key_id)

        signer = RpmSigner(gpghome)
        failures = 0
        for file_path in args.rpmfile:
            LOGGER.info('Signing file: %s', file_path)
            try:
                signer.sign_package(file_path, use_key_id, passphrase)
            except Exception as err:
                LOGGER.error('Failed signing: %s', err)
                failures += 1

        if failures == 0:
            return 0
        else:
            return 2

if __name__ == '__main__':
    sys.exit(main())
