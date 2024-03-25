# File based on boilerplate at https://github.com/bincrafters/conan-templates/blob/master/conanfile.py
# When 'install'ed this conanfile produces a 'cpodeps-runtime.zip' file in
# the current directory

import contextlib
import os.path
import shutil
import subprocess
import tempfile
import zipfile
from conans import ConanFile, CMake
from conans.errors import ConanException
from conans.client import tools


def path_append(paths, suffix):
    return tuple(
        os.path.join(path, suffix)
        for path in paths
    )


class UlsDepsConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    options = {
        'import_runtime': [True, False],
        'import_symstore': [True, False],
    }
    default_options = {
        'import_runtime': True,
        'import_symstore': False,
    }
    generators = "cmake"

    requires = (
        'qt/5.9.0@cpo/stable',
    )

    def _copy_to_zipfile(self, relroot, outzip_name):
        ''' Recursively copy a file tree into a zip file with relative paths.
        '''
        with zipfile.ZipFile(outzip_name, 'w') as zip:
            for (rootpath, dirs, files) in os.walk(relroot):
                for filename in files:
                    filepath = os.path.join(rootpath, filename)
                    relpath = os.path.relpath(filepath, relroot)
                    zip.write(filepath, relpath)

    def _import_runtime(self, tmpdir):
        ''' Copy runtime dependencies. '''
        outzip_name = 'uls-deps-runtime.zip'

        # Do nothing if output zip exists and is newer than this spec
        if os.path.exists(outzip_name):
            own_mtime = os.path.getmtime(os.path.realpath(__file__))
            outzip_mtime = os.path.getmtime(outzip_name)
            if own_mtime < outzip_mtime:
                self.output.info(
                    'Skipping imports because file newer file exists: {0}'.format(outzip_name))
                return

        bindir = os.path.join(tmpdir, 'bin')
        if not os.path.exists(bindir):
            os.makedirs(bindir)
        # MSVC runtime redistributable DLLs
        msvcrt_libs = ('msvcrt', 'vcruntime140',
                       'vcruntime140d', 'msvcp140', 'msvcp140d')
        for libname in msvcrt_libs:
            filename = '{0}.dll'.format(libname)
            shutil.copyfile(
                os.path.join(os.environ['SYSTEMROOT'], 'system32', filename),
                os.path.join(bindir, filename)
            )
        # All-deps DLLs
        self.copy("Qt5Core.dll", dst=bindir, src="bin")
        self.copy("zlib*.dll", dst=bindir, src="bin")
        self.copy("icu*.dll", dst=bindir, src="bin")

        # Write this zip file last
        self._copy_to_zipfile(tmpdir, outzip_name)

    def _import_symstore(self, tmpdir):
        ''' Extract runtime debugging info into a symbol store. '''
        outzip_name = 'uls-script-symstore.zip'

        # Do nothing if output zip exists and is newer than this spec
        if os.path.exists(outzip_name):
            own_mtime = os.path.getmtime(os.path.realpath(__file__))
            outzip_mtime = os.path.getmtime(outzip_name)
            if own_mtime < outzip_mtime:
                self.output.info(
                    'Skipping imports because file newer file exists: {0}'.format(outzip_name))
                return

        # Normalize all files into a single symbol store
        for (name, cppinfo) in self.deps_cpp_info.dependencies:
            self.output.info('Building symbol store for "{0}"'.format(name))
            subprocess.check_call([
                'symstore', 'add', '/r',
                '/compress',
                '/f', cppinfo.rootpath,
                '/s', tmpdir,
                '/t', name,
                '/v', cppinfo.version,
            ])

        # Write this zip file last
        self._copy_to_zipfile(tmpdir, outzip_name)

    def imports(self):
        ''' Copy only runtime-required DLLs and resources into local zip file.
        A bit of an abuse of the conan imports system but ensures isolation from testroot.

        Conan uses imported files to build a manifest (with checksum) after
        this function exits, so the "importroot" cannot disappear after zipping.
        '''
        importdir = os.path.abspath('importroot')
        if os.path.exists(importdir):
            shutil.rmtree(importdir)
        os.makedirs(importdir)

        if self.options.import_runtime:
            self._import_runtime(os.path.join(importdir, 'runtime'))
        if self.options.import_symstore:
            self._import_symstore(os.path.join(importdir, 'symstore'))
