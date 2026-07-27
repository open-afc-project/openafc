#!/usr/bin/env python
import sys
import os
import argparse
import subprocess
import shutil
import logging
import tempfile

LOGGER = logging.getLogger()


def _validate_members(pkg_name):
    """Reject any RPM payload member that could escape the extraction dir.

    Containment must be enforced BEFORE extraction: a symlink member
    followed by a member written through it (CVE-2015-1197 class), or a
    '..' member name, commits the out-of-tree write during `cpio -idm`, so
    a post-extraction walk detects it only after the damage is done (and
    never sees '..'-landed files at all, since the walk is rooted at
    tmp_path). List the payload first and reject symlink members and '..'
    path components outright, before any bytes are written to disk.
    """
    rpm = subprocess.Popen(
        ['rpm2cpio', pkg_name],
        stdout=subprocess.PIPE,
    )
    listing = subprocess.Popen(
        ['cpio', '-tv', '--no-absolute-filenames'],
        stdin=rpm.stdout,
        stdout=subprocess.PIPE,
    )
    rpm.stdout.close()
    (stdout, stderr) = listing.communicate()
    if rpm.wait() != 0:
        raise RuntimeError('Failed to run rpm2cpio for listing')
    if listing.returncode != 0:
        raise RuntimeError('Failed to list package payload')
    for line in stdout.decode('utf-8', 'replace').splitlines():
        if not line.strip():
            continue
        # Every entry's listing line starts with its mode string, so a
        # symlink member always yields a line whose first field begins
        # with 'l' (a name containing '\n' can only add spurious extra
        # lines, which at worst reject a hostile archive early).
        if line.split(None, 1)[0].startswith('l'):
            LOGGER.error('Rejected symlink member in %s: %s', pkg_name, line)
            raise RuntimeError(
                'RPM payload contains symlink member (extraction escape risk)')
        # Do NOT trust the name-field position: a member name with an
        # embedded newline splits the -tv line. '..' cannot contain
        # whitespace, so it always survives intact as a '/'-component of
        # some whitespace token on some line - scan them all.
        for token in line.split():
            if '..' in token.split('/'):
                LOGGER.error("Rejected '..' member in %s: %s",
                             pkg_name, line)
                raise RuntimeError(
                    "RPM payload contains '..' member "
                    '(extraction escape risk)')


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
        _validate_members(pkg_name)
        cpio = subprocess.Popen(
            ['cpio', '-idm', '--no-absolute-filenames'], cwd=tmp_path,
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

    # Defence-in-depth: reject any extracted entry whose real path escapes
    # tmp_path (catches symlink-based escapes that --no-absolute-filenames
    # does not block). Mirrors the realpath containment used in
    # split_repos.py and breakpad_extract.py.
    real_tmp = os.path.realpath(tmp_path)
    for root, dirs, files in os.walk(tmp_path):
        for entry in dirs + files:
            full = os.path.join(root, entry)
            if os.path.commonpath([os.path.realpath(full), real_tmp]) != real_tmp:
                LOGGER.error('Rejected path escaping tmp_path: %s', full)
                raise RuntimeError(
                    'RPM payload escapes extraction directory')

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
