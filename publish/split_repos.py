#!/usr/bin/env python
''' This program takes a source RPM repository (or collection of packages)
An divides them into two categories:
 - public, including only noarch and binary
 - private, including SRPMs and debuginfo/debugsource binary

'''

import os
import argparse
import re
import shutil
import subprocess
import tempfile
import rpm
import logging
import time
from .util import clear_path, find_bin

CREATEREPO_BIN = find_bin('createrepo')

LOGGER = logging.getLogger(__name__)


def pat_match(pattern, full, positive=True):
    ''' Construct a function to match a pattern or not match.
    @param pattern The pattern to check.
    @param full Determine if a full or partial match is required.
    @param positive Determine if the function returns true or false upon a match.
    @return A matching function.
    '''
    re_obj = re.compile(pattern)
    if full:
        re_func = re_obj.match
    else:
        re_func = re_obj.search

    if positive:
        return lambda fn: re_func(fn) is not None
    else:
        return lambda fn: re_func(fn) is None


def any_match(matchers, value):
    ''' Determine if a matcher function list is non-empty and has any matches
    for a value. '''
    return matchers and any(item(value) for item in matchers)


class ListItem(object):
    ''' Keep track of anamed item to match, and whether or not any candiate text has matched yet.
    '''

    def __init__(self, exact_match):
        self._text = exact_match
        self._matched = False

    def matched(self):
        return self._matched

    def name(self):
        return self._text

    def __call__(self, text):
        if self._text == text:
            self._matched = True
            return True
        return False


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--log-level',
        dest='log_level',
        default='info',
        metavar='LEVEL',
        help='Console logging lowest level displayed. Defaults to info.')
    parser.add_argument(
        '--include',
        type=str,
        action='append',
        default=[],
        help='A regexp pattern to include if a file name matches')
    parser.add_argument(
        '--exclude',
        type=str,
        action='append',
        default=[],
        help='A regexp pattern to exclude if a file name matches')
    parser.add_argument(
        '--private',
        type=str,
        action='append',
        default=[],
        help='A regexp pattern to determine if a package name is private.')
    parser.add_argument(
        '--public',
        type=str,
        action='append',
        default=[],
        help='A regexp pattern to determine if a package name is public. This overrides --private arguments.')
    parser.add_argument(
        '--public-list',
        type=str,
        help='A file containing a whitelist of public package names (not file name patterns).')
    parser.add_argument('--ignore-dupes', action='store_true', default=False,
                        help='Ignore duplicate versions of a package name.')
    parser.add_argument(
        '--ignore-input-signatures',
        action='store_true',
        default=False,
        help='Ignore package signatures on inputs. Packages may be re-signed and those signatures lost anyway.')
    parser.add_argument('inpath', type=str,
                        help='The input path to scan for files')
    parser.add_argument('outpath', type=str,
                        help='The output path to create repositories')
    args = parser.parse_args(argv[1:])

    log_level_name = args.log_level.upper()
    logging.basicConfig(level=log_level_name, format='%(message)s')

    if not os.path.exists(CREATEREPO_BIN):
        raise RuntimeError('Missing "createrepo" executable')

    # include if matching
    fn_incl = []
    for pat in args.include:
        fn_incl.append(pat_match(pat, full=True))
    # include if not matching
    fn_excl = []
    for pat in args.exclude:
        fn_excl.append(pat_match(pat, full=True))

    # patterns for private package names
    priv_pat = [r'-devel$', r'-debuginfo$', r'-debugsource$']
    if args.private:
        priv_pat += args.private
    priv_pname = []
    for pat in priv_pat:
        priv_pname.append(pat_match(pat, full=False))
    pub_pname = []
    for pat in args.public:
        pub_pname.append(pat_match(pat, full=False))
    exact_names = set()
    if args.public_list:
        with open(args.public_list, 'r') as listfile:
            for line in listfile:
                line = line.strip()
                if not line:
                    continue
                matcher = ListItem(line)
                exact_names.add(matcher)
                pub_pname.append(matcher)

    # read package name, not file name
    rpm_trans = rpm.TransactionSet()
    if args.ignore_input_signatures:
        rpm_trans.setVSFlags(rpm._RPMVSF_NOSIGNATURES)

    # Clean up first
    if not os.path.exists(args.outpath):
        os.makedirs(args.outpath)
    pub_path = os.path.join(args.outpath, 'public')
    priv_path = os.path.join(args.outpath, 'private')

    for repopath in (pub_path, priv_path):
        LOGGER.info('Clearing path {0}'.format(repopath))
        clear_path(repopath)

    # Error on duplicates
    seen_src = {}
    seen_pkg = {}

    for (dirpath, dirnames, filenames) in os.walk(args.inpath):
        for filename in filenames:
            # Only care about RPM files
            if not filename.endswith('.rpm'):
                continue
            if fn_incl and not any_match(fn_incl, filename):
                LOGGER.info('Ignoring {0}'.format(filename))
                continue
            if any_match(fn_excl, filename):
                LOGGER.info('Excluding {0}'.format(filename))
                continue

            # get package info
            src_path = os.path.join(dirpath, filename)
            with open(src_path, 'rb') as rpmfile:
                try:
                    head = rpm_trans.hdrFromFdno(rpmfile.fileno())
                except rpm.error as err:
                    LOGGER.warning('Bad header in "{0}"'.format(filename))
                    raise
                pkgname = head[rpm.RPMTAG_NAME]
                # source rpms have no source name
                is_src = not head[rpm.RPMTAG_SOURCERPM]
                is_pub = any_match(pub_pname, pkgname)
                is_priv = any_match(priv_pname, pkgname)

                labels = []
                if is_src:
                    labels.append('src')
                if is_pub:
                    labels.append('pub')
                if is_priv:
                    labels.append('priv')
                LOGGER.info('File {fn} pkg {pkg} {labels}'.format(
                    fn=filename,
                    pkg=pkgname,
                    labels=','.join(labels)
                ))

                if is_src:
                    pkggrp = seen_src
                    archname = 'SRPM'
                else:
                    pkggrp = seen_pkg
                    archname = head[rpm.RPMTAG_ARCH]

                # check for dupes
                if pkgname in pkggrp and not args.ignore_dupes:
                    raise ValueError(
                        'Duplicate package "{0}" in "{1}" and "{2}"'.format(
                            pkgname, filename, pkggrp[pkgname]))
                pkggrp[pkgname] = filename

                # SRPMs are always in private repos
                # Any not-explicitly-public packages are private
                if is_src or not is_pub:
                    tgtpath = priv_path
                else:
                    tgtpath = pub_path

            arch_path = os.path.join(tgtpath, archname)
            if not os.path.exists(arch_path):
                os.mkdir(arch_path)
            # copy, preserving permission/time
            dst_path = os.path.join(tgtpath, archname, filename)
            shutil.copyfile(src_path, dst_path)
            shutil.copystat(src_path, dst_path)

    unmatched = set()
    for matcher in exact_names:
        if not matcher.matched():
            unmatched.add(matcher.name())
    if unmatched:
        raise RuntimeError(
            'Unmatched whitelist names: {0}'.format(', '.join(unmatched)))

    # Build repo information
    for repopath in (pub_path, priv_path):
        LOGGER.info('Publishing {0}'.format(repopath))
        subprocess.check_call([
            CREATEREPO_BIN,
            '--no-database',
            '--simple-md-filenames',
            repopath
        ])


if __name__ == '__main__':
    import sys
    sys.exit(main(sys.argv))
