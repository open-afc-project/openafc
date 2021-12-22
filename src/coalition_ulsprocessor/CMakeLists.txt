cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0048 NEW)
if(NOT ${CMAKE_VERSION} VERSION_LESS 3.10)
    cmake_policy(SET CMP0071 OLD)
endif()

# External version naming
file(READ "${CMAKE_SOURCE_DIR}/version.txt" VERSIONFILE)
string(STRIP ${VERSIONFILE} VERSIONFILE)
project(uls-script VERSION ${VERSIONFILE})

if(WIN32)
    include(${CMAKE_SOURCE_DIR}/conanenv/conanbuildinfo.cmake)
    conan_basic_setup()
endif(WIN32)

cmake_minimum_required(VERSION 3.4)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

# Shared library ABI versioning
set(SOVERSION "${PROJECT_VERSION}")


# Compiler and linker config
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_EXTENSIONS ON) # No GNU/MSVC extensions
if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif(UNIX)
if(WIN32)
    # Fix use of min()/max() in MSVC
    add_definitions("-D_USE_MATH_DEFINES -DNOMINMAX")
    # Attempt use of cmake auto-export
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    
    # /bigobj Increases Number of Sections in .Obj file (needed for projects with large number of inline functions)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
    # Ignore warning from CppMicroServices lib
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4180")
    # Ignore generic C++ naming warnings and template-interface-export warning
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4503 /wd4251 /wd4275")

    # Search conan-package binaries also
    list(APPEND CMAKE_PROGRAM_PATH ${CONAN_BIN_DIRS})
    
    add_definitions("-DARMA_USE_CXX11") # Workaround MSVC lack of __cplusplus
    add_definitions("-DCPL_DISABLE_DLL") # Workaround issue with "dllexport" in "cpl_port.h"
endif(WIN32)

# Standard installation paths
#  - PKG_INSTALL_LIBDIR directory of windows ".lib" files
#  - PKG_INSTALL_BINDIR directory of windows ".dll" files and unix ".so" files
#  - PKG_INSTALL_DATADIR directory for shared application-specific data
#  - PKG_INSTALL_SYSCONFDIR root directory for system-default config files
#  - PKG_MODULE_LIBDIR directory for storing plugin module shared objects
if(UNIX)
    # Name for config/data files under standard paths (incl. XDG paths)
    set(PKG_APP_NAME "${PROJECT_NAME}")
    include(GNUInstallDirs)
    if(SHARE_INSTALL_PREFIX)
        set(CMAKE_INSTALL_DATADIR ${SHARE_INSTALL_PREFIX})
    endif(SHARE_INSTALL_PREFIX)
    if(SYSCONF_INSTALL_DIR)
        set(CMAKE_INSTALL_SYSCONFDIR ${SYSCONF_INSTALL_DIR})
    endif(SYSCONF_INSTALL_DIR)
    # Directly in system paths
    set(PKG_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
    set(PKG_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
    set(PKG_INSTALL_SBINDIR ${CMAKE_INSTALL_SBINDIR})
    # Suffix under system paths
    set(PKG_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR}/${PKG_APP_NAME})
    set(PKG_INSTALL_DATADIR ${CMAKE_INSTALL_DATADIR}/${PKG_APP_NAME})
    set(PKG_INSTALL_SYSCONFDIR ${CMAKE_INSTALL_SYSCONFDIR})
endif(UNIX)
if(WIN32)
    # Name for config/data files under standard paths (inc. %PROGRAMFILES%) 
    set(PKG_APP_NAME "${PROJECT_NAME}")
    # All files under common PREFIX path (%PROGRAMFILES%/<PKG_APP_NAME>)
    set(PKG_INSTALL_INCLUDEDIR "include")
    # Libaries to link
    set(PKG_INSTALL_LIBDIR "lib")
    # Runtime binaries
    set(PKG_INSTALL_BINDIR "bin")
    set(PKG_INSTALL_SBINDIR "bin")
    # External debug symbols
    set(PKG_INSTALL_DEBUGDIR "debug")
    # To be consistent with QStandardPaths::AppDataLocation
    set(CMAKE_INSTALL_DATADIR "bin/data")
    set(PKG_INSTALL_DATADIR "${CMAKE_INSTALL_DATADIR}/${PKG_APP_NAME}")
    # on windows config is within datadir
    set(PKG_INSTALL_SYSCONFDIR ${CMAKE_INSTALL_DATADIR})
    set(XDG_INSTALL_SYSCONFDIR "${CMAKE_INSTALL_DATADIR}")
endif(WIN32)
# Extended paths
set(CMAKE_MODULE_NAME ${PROJECT_NAME})
set(PKG_INSTALL_CMAKE_CONFIG_DIR "${PKG_INSTALL_LIBDIR}/cmake/${CMAKE_MODULE_NAME}")


# External libraries
find_package(Qt5Core 5.3 REQUIRED)
find_package(Qt5Gui REQUIRED)

# External library search options
if(WIN32)
    SET(CMAKE_FIND_LIBRARY_PREFIXES "")
    SET(CMAKE_FIND_LIBRARY_SUFFIXES ".dll")
    set(PATH_OPTS NO_DEFAULT_PATH NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
endif(WIN32)

# Build/install in source path
add_subdirectory(src)
# add_subdirectory(pkg)