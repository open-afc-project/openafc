@rem Script to build each project and install into ./testroot
call vcvarsall.bat x86_amd64
if errorlevel 1 ( pause )

set ROOTDIR=%~dp0
set TESTROOT=%ROOTDIR%\testroot

@rem Use utilities and runtime dependencies
set PATH=%TESTROOT%\bin;%PATH%
set BUILDTYPE=RelWithDebInfo
set CMAKE=cmake ^
    -DCMAKE_BUILD_TYPE=%BUILDTYPE% ^
    -DCMAKE_INSTALL_PREFIX="%TESTROOT%" ^
    -DCMAKE_PREFIX_PATH="%TESTROOT%" ^
    -DCPO_TEST_MODE=BOOL:TRUE ^
    -G "Ninja"
set NINJA=ninja -k10

@rem Set these to "1" to enable build parts
set DO_CONANENV=1
set DO_BUILD=1

if %DO_CONANENV%==1 (
    pushd conanenv

    conan install conanfile.py -s build_type=%BUILDTYPE%
    if errorlevel 1 ( pause )
    rmdir /s /q importroot
    
    @rem unpack components to testroot
    7z x -y -o"%TESTROOT%" uls-deps-runtime.zip
    if errorlevel 1 ( pause )
    
    popd
)

if %DO_BUILD%==1 (
    @rem CPOBG sub-project
    mkdir build
    cd build

    %CMAKE% ..
    if errorlevel 1 ( pause )
    %NINJA%
    if errorlevel 1 ( pause )
    %NINJA% install
    if errorlevel 1 ( pause )
    
    popd
)

pause
