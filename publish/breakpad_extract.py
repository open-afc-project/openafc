#!/usr/bin/python
''' This program walks a "debuginfo" tree and extracts Google
breakpad symbol data into a symbol tree.
'''

import sys
import os
import argparse
import subprocess
import re
import shutil
import logging

def is_elf(filename):
    ''' Read magic header for a file to determine if it has ELF data.
    
    :param filename: The file name to open and check.
    :return: True if the file contains ELF debug data.
    '''
    try:
        fobj = open(filename, 'rb')
    except IOError:
        return False
    
    head = fobj.read(4)
    return (head == '\177ELF')

def dump_syms(outdir, debugdir, filepath, log):
    ''' Manage the output of a "breakpad-dumpsyms" invocation for a single
    input file.

    :param outdir: The symbol output root directory.
    :type outdir: str
    :param debugdir: The directory containing associated ".debug" files.
    :type debugdir: str
    :param filepath: The non-".debug" file name to extract symbols from.
    :type filepath: str
    :param log: The logger object to write.
    :type log: :py:class:`Logger`
    '''
        
    # Match pattern "MODULE operatingsystem architecture id name"
    head_re = re.compile(r'^(MODULE) (\S+) (\S+) (\S+) (.+)')
        
    args = [
        'breakpad-dumpsyms',
        filepath,
        debugdir,
    ]
    log.debug('Running: {0}'.format(' '.join('"{0}"'.format(arg) for arg in args)))
    bucket = open('/dev/null', 'w')
    proc = subprocess.Popen(args, 
                            stdout=subprocess.PIPE,
                            stderr=bucket)
    outfile = None

    # First line is the only one that needs parsing
    line = proc.stdout.readline()
    match = head_re.match(line)
    if match is None:
        status = proc.wait()
        if status != 0:
            log.debug('No debug info for file "{0}"'.format(filepath))
            return
        
        log.error('Header line does not match format: "{0}"'.format(line))
        return

    (rec, opsys, arch, ident, name) = match.groups()
    path = os.path.join(outdir, name, ident, name + '.sym')
    parent = os.path.dirname(path)
    if not os.path.isdir(parent):
        os.makedirs(parent)

    log.info('Writing symbols for "{0}" from "{1}" to "{2}"'.format(name, filepath, path))
    outfile = open(path, 'wb')
    # Write re-named header line
    outfile.write(' '.join((rec, opsys, arch, ident, name)) + '\n')


    # Remaining output is piped
    while proc.poll() is None:
        outfile.write(proc.stdout.read())
    
    
class Extractor(object):
    ''' Extract Breakpad symbols from a file or tree of files.

    :param symbol_root: The output symbol root directory.
    :type symbol_root: str
    :param debug_root: The input debuginfo root directory.
    :type debug_root: str
    :param verbose: Extra output information.
    :type verbose: bool
    '''
    
    def __init__(self, symbol_root, debug_root, rel_root, log):
        self._dst = str(symbol_root)
        self._rel_root = str(rel_root)
        self._debug_root = str(debug_root)
        self._log = log
    
    def _handle(self, arg, dirname, names):
        for name in names:
            path = os.path.join(dirname, name)
            self._process(path)
    
    def _process(self, filepath):
        if not os.path.isfile(filepath) or os.path.islink(filepath):
            self._log.debug('Ignored non-file "{0}"'.format(filepath))
            return
        if filepath.endswith('.debug'):
            self._log.debug('Ignored debug object file "{0}"'.format(filepath))
            return
        if filepath.endswith('.o'):
            self._log.debug('Ignored plain object file "{0}"'.format(filepath))
            return
        if not is_elf(filepath):
            self._log.debug('Ignored non-ELF file "{0}"'.format(filepath))
            return

        self._log.debug('Processing "{0}"'.format(filepath))
        # Generate parentrel as relative path from debug root
        parentrel = os.path.dirname(filepath)
        if self._rel_root != '/':
            parentrel = os.path.relpath(parentrel, self._rel_root)
        else:
            parentrel = parentrel.lstrip('/')
        debugrel = str(self._debug_root).lstrip('/')
        debugdir = os.path.join(self._rel_root, debugrel, parentrel)
        
        dump_syms(self._dst, debugdir, filepath, self._log)
        
    
    def run(self, source):
        ''' Walk the input tree and extract symbols.
        :param scan_root: The file or root directory to walk.
        '''
        if os.path.isfile(source):
            self._process(source)
        else:
            os.path.walk(source, self._handle, None)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--verbose', type=int, default=0,
                        help='''\
Verboisty levek:
0 is none, 1 is INFO, 2 is DEBUG.
Default is %(default)s.''')
    parser.add_argument('--fileroot', type=str, default='/', help='Root path for unpackaged files.''')
    parser.add_argument('--debuginfo', type=str, default='/usr/lib/debug',
                        help='''\
Input debuginfo source tree path.
Default is "%(default)s".'''
                        )
    parser.add_argument('symboldir', type=str,
                        help='Output breakpad symbol tree path.')
    parser.add_argument('source', type=str, nargs='+',
                        help='''\
Input root directory or file. Directories are recursed.
Only ELF-format real files (i.e. not symlinks) are read.''')
    args = parser.parse_args()
    
    if not os.path.isdir(args.symboldir):
        raise ValueError('Bad output path "{0}"'.format(args.symboldir))

    if args.verbose >= 2:
        log_level = logging.DEBUG
    elif args.verbose == 1:
        log_level = logging.INFO
    else:
        log_level = logging.WARNING
    logging.basicConfig(level=log_level)
    logger = logging.getLogger('')

    ext = Extractor(args.symboldir, rel_root=args.fileroot, debug_root=args.debuginfo, log=logger)
    for src in args.source:
        ext.run(src)

if __name__ == '__main__':
    sys.exit(main())
