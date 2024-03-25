#!/usr/bin/python
''' This program is used to run a command in the background, with
its output redirected to a log file. The program prints periodic
status messages to avoid timeouts of the runner of this program.
The exit code of the subprocess is used as this program's exit.name
'''

import sys
import optparse
import threading
import os
import subprocess
import time
import datetime

if sys.version_info[0] >= 3:
    # Python 3 specific definitions
    echo_outfile = sys.stdout.buffer
else:
    # Python 2 specific definitions
    echo_outfile = sys.stdout


class StatusTimer(threading.Thread):
    def __init__(self, timeoutSecs):
        threading.Thread.__init__(self)
        self.daemon = True  # Exit with the parent process

        self._began = datetime.datetime.now()
        self._timeout = timeoutSecs

    def run(self):
        while True:
            time.sleep(self._timeout)
            diff = datetime.datetime.now() - self._began
            sys.stdout.write('Running for %.1f minutes...\n' %
                             (diff.seconds / 60.0))
            sys.stdout.flush()


class Monitor(threading.Thread):
    def __init__(self, stream, outFile):
        ''' Process lines from a stream and write to the file.
        '''
        threading.Thread.__init__(self)
        self.daemon = True  # Exit with the parent process

        self._stream = stream
        self._file = outFile

    def run(self):
        ''' Run within the thread context. '''
        for line in iter(self._stream.readline, b''):
            # Avoid duplicate line endings
            line = line.rstrip()
            # Tee the output per-line
            for out in (self._file, echo_outfile):
                out.write(line + b'\n')
                out.flush()


def main(argv):
    parser = optparse.OptionParser(
        usage='usage: %prog [options] <out file> <command> <cmd args>'
    )
    parser.add_option(
        '-a', dest='append', action='store_true', default=False,
        help='Append to the output file'
    )

    (options, args) = parser.parse_args(argv[1:])
    if len(args) < 2:
        parser.print_usage()
        return 1

    outFileName = args[0]
    cmdParts = args[1:]

    # Prepare output file
    openMode = 'wb'
    if options.append:
        openMode += '+'
    outFile = open(outFileName, openMode)

    if os.name == 'nt':
        useShell = True
    else:
        useShell = False

    # Spawn the child
    proc = subprocess.Popen(
        cmdParts, shell=useShell,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        close_fds=('posix' in sys.builtin_module_names)
    )

    # Wait for completion with background threads
    mon = Monitor(proc.stdout, outFile)
    mon.start()

    exitCode = proc.wait()
    mon.join()
    return exitCode


if __name__ == '__main__':
    sys.exit(main(sys.argv))
