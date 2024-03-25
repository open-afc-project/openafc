#!/usr/bin/python2
import gnupg
import logging
import os
import pexpect
import shutil
import subprocess
import tempfile

LOGGER = logging.getLogger(__name__)


def find_bin(name):
    ''' Search the system paths for an executable.

    :param name: The name to search for.
    :type name: str
    :return: The path to the executable.
    :rtype: str
    :raise RuntimeError: if the name could not be found.
    '''
    import distutils.spawn
    path = distutils.spawn.find_executable(name)
    if not path:
        raise RuntimeError('Executable not found for "{0}"'.format(name))
    return path


def clear_path(path):
    ''' Either create a path or remove all items under (but not including) the path. '''
    if os.path.exists(path):
        for (dirpath, dirnames, filenames) in os.walk(path, topdown=False):
            for dirname in dirnames:
                os.rmdir(os.path.join(dirpath, dirname))
            for filename in filenames:
                os.unlink(os.path.join(dirpath, filename))
    else:
        os.mkdir(path)


class GpgHome(object):
    ''' A class to implement a local, temporary GPG configuration.

    :ivar gpghome: The path to the GPG configuration.
    :ivar gpg: The :py:cls:`gnupg.GPG` object being used in that home.
    '''

    def __init__(self, gpg_bin=None):
        self._gpg_bin = gpg_bin or find_bin('gpg2')
        self.gpghome = os.path.abspath(tempfile.mkdtemp())
        self.gpg = gnupg.GPG(
            gpgbinary=self._gpg_bin,
            gnupghome=self.gpghome
        )

    def __del__(self):
        if self.gpghome:
            self.close()

    def close(self):
        ''' Delete the local configuration.

        This can only be called once per instance.
        '''
        if not self.gpghome:
            raise RuntimeError('Attempt to close multiple times')
        del self.gpg
        shutil.rmtree(self.gpghome)
        self.gpghome = None

    def find_key(self, key_id=None):
        ''' Search for a specific key, or use the only one present.

        :param key_id: The optional key to search for.
            If not provided and the keyring has only a single private key
            then that key is used.
        :type key_id: str or None
        :return: The found key ID.
        :rtype: str
        :raise RuntimeError: if the desired key cannot be found.
        '''
        key_infos = self.gpg.list_keys()
        if not key_infos:
            raise RuntimeError('GPG file has no keys')
        LOGGER.debug('Keyring has %d keys', len(key_infos))

        found_id = None
        for key_info in key_infos:
            LOGGER.debug('Keyring has ID %s', key_info['keyid'])
            if key_id is None or key_info['keyid'] == key_id:
                found_id = key_info['keyid']
                break

        if not found_id:
            if key_id:
                raise RuntimeError('Key ID "{0}" not found'.format(args.keyid))
            else:
                raise RuntimeError('Key ID required but not provided')
        return found_id

    def check_key(self, key_id, passphrase):
        ''' Verify a valid key and passphrase pairing.

        :param str key_id: The GPG key to sign with.
        :param str passphrase: The passphrase to unlock the private key.
        :except Exception: If the verification fails for any reason.
        '''
        sig = self.gpg.sign('test', keyid=key_id, passphrase=passphrase)
        if not sig:
            raise RuntimeError('Failed to verify passphrase')


class RpmSigner(object):
    ''' A class to wrap the `rpmsign` shell utility.

    This is based on example at http://aaronhawley.livejournal.com/10615.html

    :param gpghome: The GPG home directory to sign with.
    '''

    def __init__(self, gpghome, rpmsign_bin=None):
        if isinstance(gpghome, GpgHome):
            self._gpghome = gpghome.gpghome
        else:
            self._gpghome = gpghome
        self._rpmsign_bin = rpmsign_bin or find_bin('rpmsign')

    def sign_package(self, file_path, key_id, passphrase):
        ''' Sign an individual package.

        :param str file_path: The file path to sign.
        :param str key_id: The GPG key to sign with.
        :param str passphrase: The passphrase to unlock the private key.
        :except Exception: If the signing fails for any reason.
        '''
        digest_algo = 'sha256'
        cmd = [
            self._rpmsign_bin,
            '--define', '_gpg_name {0}'.format(key_id),
            '--define', '_gpg_digest_algo {0}'.format(digest_algo),
            '--addsign',
            '-v',
            file_path,
        ]
        LOGGER.debug(
            'Calling ' + ' '.join(['"{0}"'.format(arg.replace('"', '\\"')) for arg in cmd]))

        try:
            os.environ['GNUPGHOME'] = self._gpghome
            # On CentOS-7 wait for prompt then provide password
            proc = pexpect.spawn(cmd[0], cmd[1:])
            proc.expect('Enter pass phrase:')
            proc.sendline(passphrase)

            outstr = ''
            while True:
                try:
                    # Signing may take considerable time on large (100+ MiB)
                    # packages
                    outstr += proc.read_nonblocking(size=100, timeout=300)
                except pexpect.EOF:
                    pass
                if not proc.isalive():
                    break
            outstr += proc.read()
            exitcode = proc.exitstatus

            if exitcode != 0:
                LOGGER.error('Failed with error:\n%s', outstr)
                raise subprocess.CalledProcessError(exitcode, cmd[0], outstr)
            else:
                LOGGER.debug('Success with output:\n%s', outstr)
        finally:
            del os.environ['GNUPGHOME']
