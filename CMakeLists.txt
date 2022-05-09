cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0048 NEW)
if(NOT ${CMAKE_VERSION} VERSION_LESS 3.10)
    cmake_policy(SET CMP0071 OLD)
endif()

# Debug
# Release
# DebugWithTest
set(default_build_type "Debug")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "${default_build_type}")
endif()

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

# External version naming
file(READ "${CMAKE_SOURCE_DIR}/version.txt" VERSIONFILE)
string(STRIP ${VERSIONFILE} VERSIONFILE)
project(fbrat VERSION ${VERSIONFILE})

cmake_minimum_required(VERSION 3.4)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
option(BUILD_AFCENGINE "Build the AFC engine portion of the project" ON)

# Shared library ABI versioning
set(SOVERSION "${PROJECT_VERSION}")

# Allow retrieving local source revision with cache
include(svnversion)
svnversion_info(
    PREFIX SVN
    DIR "${CMAKE_SOURCE_DIR}"
    SAVEFILE "${CMAKE_SOURCE_DIR}/svnrevision.txt"
)
message("${CMAKE_SOURCE_DIR} VERS ${SVN_LAST_REVISION}")

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

# debug build flags
if (DEBUG_AFC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG_AFC=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=format-extra-args")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=format")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=shadow")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
    message(STATUS "Using DEBUG_AFC build mode")
endif()

# Coverage analysis
include(CheckCoverage)

# Flag for special compilation flag for Fedora Build
if (OS_FEDORA)
  add_definitions("-DOS_FEDORA")
endif(OS_FEDORA)

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
    set(XDG_INSTALL_SYSCONFDIR "${CMAKE_INSTALL_SYSCONFDIR}/xdg")
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
set(XDG_ICONS_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/icons")
set(XDG_MIME_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/mime")
set(XDG_APPS_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/applications")
set(CMAKE_MODULE_NAME ${PROJECT_NAME})
set(PKG_INSTALL_CMAKE_CONFIG_DIR "${PKG_INSTALL_LIBDIR}/cmake/${CMAKE_MODULE_NAME}")


# External libraries
find_package(Qt5Core 5.3 REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Sql REQUIRED)
find_package(Qt5Test REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5Widgets REQUIRED)

find_package(iturp452 REQUIRED)
find_package(Armadillo REQUIRED)
find_package(ZLIB REQUIRED)
find_package(minizip REQUIRED)

find_package(GDAL REQUIRED)
if(NOT GDAL_FOUND)
    message(FATAL_ERROR "Missing GDAL library")
endif()
if(GDAL_FOUND AND NOT TARGET GDAL::GDAL)
  add_library(GDAL::GDAL UNKNOWN IMPORTED)
  set_target_properties(GDAL::GDAL PROPERTIES
    IMPORTED_LOCATION "${GDAL_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${GDAL_INCLUDE_DIR}"
  )
endif()

set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.54 REQUIRED COMPONENTS log program_options regex system thread)
add_definitions("-DBOOST_ALL_DYN_LINK")


if (CMAKE_BUILD_TYPE STREQUAL "DebugWithTest")
  find_package(GTest 1.6 REQUIRED)
  # Run tests with CTest
  include(CTest)
  enable_testing()
  # Alias "check" target for verbose test run
  add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -C ${CMAKE_BUILD_TYPE} --verbose)
endif()


# External library search options
if(WIN32)
    SET(CMAKE_FIND_LIBRARY_PREFIXES "")
    SET(CMAKE_FIND_LIBRARY_SUFFIXES ".dll")
    set(PATH_OPTS NO_DEFAULT_PATH NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
endif(WIN32)

# Build/install in source path
add_subdirectory(src)
add_subdirectory(pkg)

if(APIDOC_INSTALL_PATH)
  configure_file(Doxyfile.in Doxyfile @ONLY)
endif()
