#!/usr/bin/env python
import sys
import os
import argparse
import subprocess
import shutil
import logging
import tempfile

LOGGER = logging.getLogger()

# Decompression-bomb caps enforced by _validate_members() before any bytes
# are extracted: maximum total listed payload size and member count.
# Overridable via environment for unusually large legitimate packages.
MAX_EXTRACTED_BYTES = int(os.environ.get(
    'BREAKPAD_RPM_MAX_EXTRACTED_BYTES', str(4 * 1024 ** 3)))
MAX_MEMBERS = int(os.environ.get('BREAKPAD_RPM_MAX_MEMBERS', '100000'))
# Per-line cap for the cpio -tv listing: a newc member name can be up to
# ~4 GiB (8-hex-digit c_namesize) of newline-free bytes that compress to a
# few MB on disk, so an unbounded line read would materialize it before the
# caps above are consulted.  A member name longer than PATH_MAX (4096) can
# never extract successfully, so 64 KiB is safe for all legitimate packages.
MAX_LISTING_LINE_BYTES = 65536


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
    member_count = 0
    total_size = 0
    # Stream the -tv listing line-by-line instead of communicate(): an
    # archive with tens of millions of members must not balloon this
    # process's memory, and the decompression-bomb caps below must reject
    # a hostile payload before any bytes are written to disk.
    while True:
        # readline() with an explicit limit bounds the per-line allocation
        # (a member-name bomb would otherwise OOM this process before any
        # cap below fires); a line that reaches the limit without a
        # terminating newline marks a hostile archive.
        raw_line = listing.stdout.readline(MAX_LISTING_LINE_BYTES)
        if not raw_line:
            break
        if len(raw_line) >= MAX_LISTING_LINE_BYTES and \
                not raw_line.endswith(b'\n'):
            LOGGER.error('Rejected %s: listing line exceeds %d bytes',
                         pkg_name, MAX_LISTING_LINE_BYTES)
            raise RuntimeError(
                'RPM payload listing line exceeds length cap '
                '(member-name bomb risk)')
        line = raw_line.decode('utf-8', 'replace')
        if not line.strip():
            continue
        member_count += 1
        if member_count > MAX_MEMBERS:
            LOGGER.error('Rejected %s: more than %d payload members',
                         pkg_name, MAX_MEMBERS)
            raise RuntimeError(
                'RPM payload exceeds member-count cap '
                '(decompression bomb risk)')
        # Every entry's listing line starts with its mode string, so a
        # symlink member always yields a line whose first field begins
        # with 'l' (a name containing '\n' can only add spurious extra
        # lines, which at worst reject a hostile archive early).
        fields = line.split()
        if fields[0].startswith('l'):
            LOGGER.error('Rejected symlink member in %s: %s', pkg_name, line)
            raise RuntimeError(
                'RPM payload contains symlink member (extraction escape risk)')
        # cpio -tv lines are ls(1)-style: mode nlinks owner group size ...
        # Every entry's size field is on its FIRST listing line (the mode
        # string leads), so an embedded newline in a member name can only
        # add spurious fragment lines that inflate member_count (rejecting
        # a hostile archive early) - it can never shrink the size total.
        if len(fields) >= 5 and fields[4].isdigit():
            total_size += int(fields[4])
            if total_size > MAX_EXTRACTED_BYTES:
                LOGGER.error('Rejected %s: listed payload exceeds %d bytes',
                             pkg_name, MAX_EXTRACTED_BYTES)
                raise RuntimeError(
                    'RPM payload exceeds extracted-size cap '
                    '(decompression bomb risk)')
        # Do NOT trust the name-field position: a member name with an
        # embedded newline splits the -tv line. '..' cannot contain
        # whitespace, so it always survives intact as a '/'-component of
        # some whitespace token on some line - scan them all.
        for token in fields:
            if '..' in token.split('/'):
                LOGGER.error("Rejected '..' member in %s: %s",
                             pkg_name, line)
                raise RuntimeError(
                    "RPM payload contains '..' member "
                    '(extraction escape risk)')
    listing.stdout.close()
    if rpm.wait() != 0:
        raise RuntimeError('Failed to run rpm2cpio for listing')
    if listing.wait() != 0:
        raise RuntimeError('Failed to list package payload')


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
    snap_dir = tempfile.mkdtemp()
    for pkg_name in args.packages:
        LOGGER.info('Extracting package %s ...', pkg_name)
        # TOCTOU guard: snapshot the package into a private directory
        # (outside the extraction cwd) so the validation read and the
        # extraction read see the same bytes.  Previously each rpm2cpio
        # invocation re-opened pkg_name by path, so a concurrent writer
        # on a shared filesystem could swap in a hostile payload after
        # _validate_members had passed the benign one.
        pkg_snap = os.path.join(snap_dir, os.path.basename(pkg_name))
        shutil.copyfile(pkg_name, pkg_snap)
        _validate_members(pkg_snap)
        cpio = subprocess.Popen(
            ['cpio', '-idm', '--no-absolute-filenames'], cwd=tmp_path,
            stdin=subprocess.PIPE,
            stderr=bitbucket,
        )
        rpm = subprocess.Popen(
            ['rpm2cpio', pkg_snap],
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
        os.remove(pkg_snap)

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
    shutil.rmtree(snap_dir, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
