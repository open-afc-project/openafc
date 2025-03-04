#!/usr/bin/env python
''' This program takes a source RPM repository (or collection of packages)
An moves files out of the source tree into a destination directory based
on the package name and architecture.
'''

import os
import argparse
import re
import shutil
import rpm
import logging
import time
from .util import clear_path, find_bin

LOGGER = logging.getLogger(__name__)


def any_match(matchers, value):
    ''' Determine if a matcher function list is non-empty and has any matches
    for a value. '''
    return any(item(value) for item in matchers)


class Matcher(object):
    ''' Keep track of a named item to match, and whether or not any
    candidate text has matched yet.

    :param re_match: A regular expression pattern to match.
    :type re_match: str
    '''

    def __init__(self, re_match):
        self._re = re.compile('^' + re_match + '$')
        self._matched = False

    def matched(self):
        ''' Determine if this matcher has ever matched a value.
        :return: True if any match has occurred.
        '''
        return self._matched

    def __str__(self):
        return self._re.pattern

    def _check(self, text):
        match = self._re.match(text)
        return bool(match is not None)

    def __call__(self, text):
        if self._check(text):
            self._matched = True
            return True
        return False


def main(argv):
    parser = argparse.ArgumentParser(
        argv[0],
        description='Move RPM packages into a repository tree.'
    )
    parser.add_argument(
        '--log-level',
        dest='log_level',
        default='info',
        metavar='LEVEL',
        help='Console logging lowest level displayed. Defaults to info.')
    parser.add_argument(
        '--include-pattern',
        type=str,
        action='append',
        default=[],
        help='A regexp pattern to determine if a package "name.arch" (not file name) is taken.')
    parser.add_argument(
        '--include-list',
        type=str,
        help='A text file containing an include pattern on each line.')
    parser.add_argument('--ignore-dupes', action='store_true', default=False,
                        help='Ignore duplicate versions of a package name.')
    parser.add_argument(
        '--ignore-input-signatures',
        action='store_true',
        default=False,
        help='Ignore package signatures on inputs. Packages may be re-signed and those signatures lost anyway.')
    parser.add_argument('inpath', type=str, nargs='+',
                        help='The input path to scan for files.')
    parser.add_argument('outpath', type=str,
                        help='The output directory to move files into.')
    args = parser.parse_args(argv[1:])

    log_level_name = args.log_level.upper()
    logging.basicConfig(level=log_level_name, format='%(message)s')

    # include if matching
    pkg_incl = []
    for pat in args.include_pattern:
        pkg_incl.append(Matcher(pat))
    if args.include_list:
        with open(args.include_list, 'r') as listfile:
            for line in listfile:
                line = line.strip()
                if not line:
                    continue
                pkg_incl.append(Matcher(line))
    # exclude if matching
    pkg_excl = []

    # read package name, not file name
    rpm_trans = rpm.TransactionSet()
    if args.ignore_input_signatures:
        rpm_trans.setVSFlags(rpm._RPMVSF_NOSIGNATURES)

    # Clean up first
    tgtpath = args.outpath
    if not os.path.exists(tgtpath):
        os.makedirs(tgtpath)
    LOGGER.info('Clearing output: %s', tgtpath)
    clear_path(tgtpath)

    # Error on duplicates
    seen_fullnames = {}

    for inpath in args.inpath:
        LOGGER.info('Scanning input: %s', inpath)
        for (dirpath, dirnames, filenames) in os.walk(inpath):
            for filename in filenames:
                # Only care about RPM files
                if not filename.endswith('.rpm'):
                    continue

                # get package info
                src_path = os.path.join(dirpath, filename)
                rel_path = os.path.relpath(src_path, inpath)
                with open(src_path, 'rb') as rpmfile:
                    try:
                        head = rpm_trans.hdrFromFdno(rpmfile.fileno())
                    except rpm.error as err:
                        LOGGER.warning('Bad header in "%s"', rel_path)
                        raise
                    pkgname = head[rpm.RPMTAG_NAME]
                    # source rpms have no source name,
                    # so use the same name as 'yum' does
                    if not head[rpm.RPMTAG_SOURCERPM]:
                        archname = 'src'
                    else:
                        archname = head[rpm.RPMTAG_ARCH]

                fullname = '{0}.{1}'.format(pkgname, archname)
                LOGGER.info('File %s name %s', rel_path, fullname)

                if not any_match(pkg_incl, fullname):
                    LOGGER.info('Ignoring name %s', fullname)
                    continue
                if pkg_excl and any_match(pkg_excl, fullname):
                    LOGGER.info('Excluding name %s', fullname)
                    continue

                # check for dupes
                if fullname in seen_fullnames and not args.ignore_dupes:
                    raise ValueError(
                        'Duplicate package "{0}" in "{1}" and "{2}"'.format(
                            fullname, rel_path, seen_fullnames[fullname]))
                seen_fullnames[fullname] = rel_path

                # move, preserving permission/time and relative tree structure
                dst_path = os.path.join(tgtpath, rel_path)
                dst_dir = os.path.dirname(dst_path)
                if not os.path.exists(dst_dir):
                    os.makedirs(dst_dir)
                LOGGER.info('Moving %s to %s', fullname, dst_path)
                shutil.move(src_path, dst_path)

    unmatched = set()
    for matcher in pkg_incl:
        if not matcher.matched():
            unmatched.add(str(matcher))
    if unmatched:
        raise RuntimeError(
            'Unmatched include names: {0}'.format(', '.join(unmatched)))


if __name__ == '__main__':
    import sys
    sys.exit(main(sys.argv))
