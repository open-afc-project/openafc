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
    parser.add_argument(
        '--log-level',
        dest='log_level',
        default='info',
        metavar='LEVEL',
        help='Console logging lowest level displayed. Defaults to info.')
    parser.add_argument(
        '--keyid',
        help='The key ID to sign with. Mandatory if the keyring has more than one keypair.')
    parser.add_argument(
        '--passphrase-fd',
        type=int,
        default=0,
        help='The FD to read passphrase from. Defaults to stdin.')
    parser.add_argument('gpgkey', type=str,
                        help='GPG-encoded private keypair file.')
    parser.add_argument('repodir',
                        nargs='+',
                        help='RPM repository root to sign.')
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

        failures = 0
        for repo_path in args.repodir:
            LOGGER.info('Signing repository: %s', repo_path)
            repomd_path = os.path.join(repo_path, 'repodata', 'repomd.xml')
            sigfile_path = os.path.join(
                repo_path, 'repodata', 'repomd.xml.asc')
            pubkey_path = os.path.join(repo_path, 'repodata', 'repomd.xml.key')
            try:
                with contextlib.closing(open(repomd_path, 'rb')) as repomd_file:
                    sigdata = gpghome.gpg.sign_file(
                        repomd_file,
                        keyid=use_key_id,
                        passphrase=passphrase,
                        detach=True,
                        binary=False  # armored
                    )
                with contextlib.closing(open(sigfile_path, 'wb')) as sigfile:
                    sigfile.write(str(sigdata))
            except Exception as err:
                LOGGER.error('Failed signing: %s', err)
                failures += 1

            try:
                keydata = gpghome.gpg.export_keys(
                    use_key_id,
                    armor=True
                )
                with contextlib.closing(open(pubkey_path, 'wb')) as keyfile:
                    keyfile.write(str(keydata))
            except Exception as err:
                LOGGER.error('Failed exporting: %s', err)
                failures += 1

        if failures == 0:
            return 0
        else:
            return 2


if __name__ == '__main__':
    sys.exit(main())
