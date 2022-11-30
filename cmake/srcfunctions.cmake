# Redirect add_... functions to accumulate target names

#
# Define a library from sources with its headers.
# This relies on the pre-existing values:
#  - "VERSION" to set the library PROJECT_VERSION property
#  - "SOVERSION" to set the library SOVERSION property
#
function(add_dist_library)
    set(PARSE_OPTS )
    set(PARSE_ARGS_SINGLE TARGET EXPORTNAME)
    set(PARSE_ARGS_MULTI SOURCES HEADERS)
    cmake_parse_arguments(ADD_DIST_LIB "${PARSE_OPTS}" "${PARSE_ARGS_SINGLE}" "${PARSE_ARGS_MULTI}" ${ARGN})
    if("${ADD_DIST_LIB_TARGET}" STREQUAL "")
        message(FATAL_ERROR "add_dist_library missing TARGET parameter")
    endif()
    if("${ADD_DIST_LIB_EXPORTNAME}" STREQUAL "")
        message(FATAL_ERROR "add_dist_library missing EXPORTNAME parameter")
    endif()
    if("${ADD_DIST_LIB_SOURCES}" STREQUAL "")
        message(FATAL_ERROR "add_dist_library missing SOURCES parameter")
    endif()
    
    if(WIN32 AND BUILD_SHARED_LIBS)
        # Give the DLL version markings
        set(WINRES_COMPANY_NAME_STR "RKF Engineering Solutions, LLC")
        set(WINRES_PRODUCT_NAME_STR ${PROJECT_NAME})
        set(WINRES_PRODUCT_VERSION_RES "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
        set(WINRES_PRODUCT_VERSION_STR "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-${SVN_LAST_REVISION}")
        set(WINRES_INTERNAL_NAME_STR ${ADD_DIST_LIB_TARGET})
        set(WINRES_ORIG_FILENAME "${CMAKE_SHARED_LIBRARY_PREFIX}${ADD_DIST_LIB_TARGET}${CMAKE_SHARED_LIBRARY_SUFFIX}")
        set(WINRES_FILE_DESCRIPTION_STR "Runtime for ${ADD_DIST_LIB_TARGET}")
        set(WINRES_FILE_VERSION_RES ${WINRES_PRODUCT_VERSION_RES})
        set(WINRES_FILE_VERSION_STR ${WINRES_PRODUCT_VERSION_STR})
        set(WINRES_COMMENTS_STR "")
        configure_file("${CMAKE_SOURCE_DIR}/src/libinfo.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/${ADD_DIST_LIB_TARGET}-libinfo.rc" @ONLY)
        list(APPEND ADD_DIST_LIB_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/${ADD_DIST_LIB_TARGET}-libinfo.rc")
    endif(WIN32 AND BUILD_SHARED_LIBS)
    
    add_library(${ADD_DIST_LIB_TARGET} ${ADD_DIST_LIB_SOURCES})
    set_target_properties(
        ${ADD_DIST_LIB_TARGET} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${SOVERSION}
    )
    
    include(GenerateExportHeader)
    generate_export_header(${ADD_DIST_LIB_TARGET})
    target_include_directories(${ADD_DIST_LIB_TARGET} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${PKG_INSTALL_INCLUDEDIR}>
    )
    list(APPEND ADD_DIST_LIB_HEADERS "${CMAKE_CURRENT_BINARY_DIR}/${ADD_DIST_LIB_TARGET}_export.h")
    
    # Source-directory relative path
    get_filename_component(SOURCE_DIRNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    
    # Include headers, with original directory name
    install(
        FILES ${ADD_DIST_LIB_HEADERS}
        DESTINATION ${PKG_INSTALL_INCLUDEDIR}/${SOURCE_DIRNAME}
        COMPONENT development
    )

    if(WIN32)
        # PDB for symbol mapping
        install(
            FILES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${ADD_DIST_LIB_TARGET}.pdb"
            DESTINATION ${PKG_INSTALL_DEBUGDIR}
            COMPONENT debuginfo
        )
        # Sources for debugger (directory name is target name)
        install(
            FILES ${ADD_DIST_LIB_HEADERS} ${ADD_DIST_LIB_SOURCES}
            DESTINATION ${PKG_INSTALL_DEBUGDIR}/${SOURCE_DIRNAME}
            COMPONENT debuginfo
        )
    endif(WIN32)
    
    install(
        TARGETS ${ADD_DIST_LIB_TARGET}
        EXPORT ${ADD_DIST_LIB_EXPORTNAME}
        # For Win32
        RUNTIME
            DESTINATION ${PKG_INSTALL_BINDIR}
            COMPONENT runtime
        ARCHIVE
            DESTINATION ${PKG_INSTALL_LIBDIR}
            COMPONENT development
        # For unix
        LIBRARY 
            DESTINATION ${PKG_INSTALL_LIBDIR}
            COMPONENT runtime
    )
endfunction(add_dist_library)

function(add_dist_module)
    set(PARSE_OPTS )
    set(PARSE_ARGS_SINGLE TARGET EXPORTNAME COMPONENT)
    set(PARSE_ARGS_MULTI SOURCES HEADERS)
    cmake_parse_arguments(ADD_DIST_LIB "${PARSE_OPTS}" "${PARSE_ARGS_SINGLE}" "${PARSE_ARGS_MULTI}" ${ARGN})
    if("${ADD_DIST_LIB_TARGET}" STREQUAL "")
        message(FATAL_ERROR "add_dist_library missing TARGET parameter")
    endif()
    if("${ADD_DIST_LIB_COMPONENT}" STREQUAL "")
        set(ADD_DIST_LIB_COMPONENT runtime)
    endif()
    if("${ADD_DIST_LIB_SOURCES}" STREQUAL "")
        message(FATAL_ERROR "add_dist_library missing SOURCES parameter")
    endif()
    if(NOT PKG_MODULE_LIBDIR)
        message(FATAL_ERROR "Must define PKG_MODULE_LIBDIR for installation")
    endif()

    if(WIN32 AND BUILD_SHARED_LIBS)
        # Give the DLL version markings
        set(WINRES_COMPANY_NAME_STR "RKF Engineering Solutions, LLC")
        set(WINRES_PRODUCT_NAME_STR ${PROJECT_NAME})
        set(WINRES_PRODUCT_VERSION_RES "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
        set(WINRES_PRODUCT_VERSION_STR "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-${SVN_LAST_REVISION}")
        set(WINRES_INTERNAL_NAME_STR ${ADD_DIST_LIB_TARGET})
        set(WINRES_ORIG_FILENAME "${CMAKE_SHARED_LIBRARY_PREFIX}${ADD_DIST_LIB_TARGET}${CMAKE_SHARED_LIBRARY_SUFFIX}")
        set(WINRES_FILE_DESCRIPTION_STR "Runtime for ${ADD_DIST_LIB_TARGET}")
        set(WINRES_FILE_VERSION_RES ${WINRES_PRODUCT_VERSION_RES})
        set(WINRES_FILE_VERSION_STR ${WINRES_PRODUCT_VERSION_STR})
        set(WINRES_COMMENTS_STR "")
        configure_file("${CMAKE_SOURCE_DIR}/src/libinfo.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/${ADD_DIST_LIB_TARGET}-libinfo.rc" @ONLY)
        list(APPEND ADD_DIST_LIB_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/${ADD_DIST_LIB_TARGET}-libinfo.rc")
    endif(WIN32 AND BUILD_SHARED_LIBS)
    
    add_library(${ADD_DIST_LIB_TARGET} MODULE ${ADD_DIST_LIB_SOURCES})
    set_target_properties(
        ${ADD_DIST_LIB_TARGET} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${SOVERSION}
        # no "lib" prefix on unix
        PREFIX ""
    )
    
    include(GenerateExportHeader)
    generate_export_header(${ADD_DIST_LIB_TARGET})
    list(APPEND ADD_DIST_LIB_HEADERS "${CMAKE_CURRENT_BINARY_DIR}/${ADD_DIST_LIB_TARGET}_export.h")
    
    # Source-directory relative path
    get_filename_component(SOURCE_DIRNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    
    if(WIN32)
        # PDB for symbol mapping
        install(
            FILES "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${ADD_DIST_LIB_TARGET}.pdb"
            DESTINATION ${PKG_INSTALL_DEBUGDIR}
            COMPONENT debuginfo
        )
        # Sources for debugger (directory name is target name)
        install(
            FILES ${ADD_DIST_LIB_HEADERS} ${ADD_DIST_LIB_SOURCES}
            DESTINATION ${PKG_INSTALL_DEBUGDIR}/${SOURCE_DIRNAME}
            COMPONENT debuginfo
        )
    endif(WIN32)
    
    install(
        TARGETS ${ADD_DIST_LIB_TARGET}
        EXPORT ${ADD_DIST_LIB_EXPORTNAME}
        # For Win32
        RUNTIME
            DESTINATION ${PKG_MODULE_LIBDIR}
            COMPONENT ${ADD_DIST_LIB_COMPONENT}
        ARCHIVE
            DESTINATION ${PKG_INSTALL_LIBDIR}
            COMPONENT development
        # For unix
        LIBRARY 
            DESTINATION ${PKG_MODULE_LIBDIR}
            COMPONENT ${ADD_DIST_LIB_COMPONENT}
    )
endfunction(add_dist_module)

function(add_dist_executable)
    set(PARSE_OPTS SYSTEMEXEC)
    set(PARSE_ARGS_SINGLE TARGET EXPORTNAME)
    set(PARSE_ARGS_MULTI SOURCES HEADERS)
    cmake_parse_arguments(ADD_DIST_BIN "${PARSE_OPTS}" "${PARSE_ARGS_SINGLE}" "${PARSE_ARGS_MULTI}" ${ARGN})
    if("${ADD_DIST_BIN_TARGET}" STREQUAL "")
        message(FATAL_ERROR "add_dist_executable missing TARGET parameter")
    endif()
    if("${ADD_DIST_BIN_SOURCES}" STREQUAL "")
        message(FATAL_ERROR "add_dist_executable missing SOURCES parameter")
    endif()

    if(WIN32)
        # Give the DLL version markings
        set(WINRES_COMPANY_NAME_STR "RKF Engineering Solutions, LLC")
        set(WINRES_PRODUCT_NAME_STR ${PROJECT_NAME})
        set(WINRES_PRODUCT_VERSION_RES "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
        set(WINRES_PRODUCT_VERSION_STR "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-${SVN_LAST_REVISION}")
        set(WINRES_INTERNAL_NAME_STR ${ADD_DIST_BIN_TARGET})
        set(WINRES_ORIG_FILENAME "${ADD_DIST_BIN_TARGET}${CMAKE_EXECUTABLE_SUFFIX}")
        set(WINRES_FILE_DESCRIPTION_STR "Runtime for ${ADD_DIST_BIN_TARGET}")
        set(WINRES_FILE_VERSION_RES ${WINRES_PRODUCT_VERSION_RES})
        set(WINRES_FILE_VERSION_STR ${WINRES_PRODUCT_VERSION_STR})
        set(WINRES_COMMENTS_STR "")
        configure_file("${CMAKE_SOURCE_DIR}/src/libinfo.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/${ADD_DIST_BIN_TARGET}-libinfo.rc" @ONLY)
        list(APPEND ADD_DIST_BIN_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/${ADD_DIST_BIN_TARGET}-libinfo.rc")
    endif(WIN32)
    
    add_executable(${ADD_DIST_BIN_TARGET} ${ADD_DIST_BIN_SOURCES})
    
    if(TARGET Threads::Threads)
        target_link_libraries(${ADD_DIST_BIN_TARGET} PRIVATE Threads::Threads)
    endif()

    if(${ADD_DIST_BIN_SYSTEMEXEC})
        set(ADD_DIST_BIN_DEST ${PKG_INSTALL_SBINDIR})
    else()
        set(ADD_DIST_BIN_DEST ${PKG_INSTALL_BINDIR})
    endif()
    install(
        TARGETS ${ADD_DIST_BIN_TARGET}
        EXPORT ${ADD_DIST_BIN_EXPORTNAME}
        DESTINATION ${ADD_DIST_BIN_DEST}
        COMPONENT runtime
    )
    if(WIN32)
        get_filename_component(SOURCE_DIRNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
        # PDB for symbol mapping
        install(
            FILES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${ADD_DIST_BIN_TARGET}.pdb"
            DESTINATION ${PKG_INSTALL_DEBUGDIR}
            COMPONENT debuginfo
        )
        # Sources for debugger (directory name is target name)
        install(
            FILES ${ADD_DIST_BIN_HEADERS} ${ADD_DIST_BIN_SOURCES}
            DESTINATION ${PKG_INSTALL_DEBUGDIR}/${SOURCE_DIRNAME}
            COMPONENT debuginfo
        )
    endif(WIN32)
endfunction(add_dist_executable)

#
# Define a python library from sources.
# The named function arguments are:
#  TARGET: The cmake target name to create.
#  SETUP_TEMPLATE: A file to be used as template for setup.py.
#  COMPONENT: The cmake "install" component to install the library as.
#  SOURCEDIR: The root directory of all sources for the target, python files or otherwise.
#
# When processing the setup template, a variable is created for a windows-safe
# escaped file path to the source directory named 
# DIST_LIB_PACKAGE_DIR_ESCAPED.
#
function(add_dist_pythonlibrary)
    set(PARSE_OPTS )
    set(PARSE_ARGS_SINGLE TARGET SETUP_TEMPLATE SOURCEDIR COMPONENT)
    set(PARSE_ARGS_MULTI )
    cmake_parse_arguments(ADD_DIST_LIB "${PARSE_OPTS}" "${PARSE_ARGS_SINGLE}" "${PARSE_ARGS_MULTI}" ${ARGN})
    if("${ADD_DIST_LIB_TARGET}" STREQUAL "")
        message(FATAL_ERROR "add_dist_pythonlibrary missing TARGET parameter")
    endif()
    if("${ADD_DIST_LIB_SETUP_TEMPLATE}" STREQUAL "")
        message(FATAL_ERROR "add_dist_pythonlibrary missing SETUP_TEMPLATE parameter")
    endif()
    if("${ADD_DIST_LIB_SOURCEDIR}" STREQUAL "")
        message(FATAL_ERROR "add_dist_pythonlibrary missing SOURCEDIR parameter")
    endif()

    find_program(PYTHON_BIN "python")
    if(NOT PYTHON_BIN)
        message(FATAL_ERROR "Missing executable for 'python'")
    endif()

    # Setuptools runs on copy of source in the build path
    set(ADD_DIST_LIB_SOURCECOPY "${CMAKE_CURRENT_BINARY_DIR}/pkg")
    # Need to escape the path for windows
    if(WIN32)
        string(REPLACE "/" "\\\\" DIST_LIB_PACKAGE_DIR_ESCAPED ${ADD_DIST_LIB_SOURCECOPY})
    else(WIN32)
        set(DIST_LIB_PACKAGE_DIR_ESCAPED ${ADD_DIST_LIB_SOURCECOPY})
    endif(WIN32)

    # Assemble the actual setup.py input
    configure_file(${ADD_DIST_LIB_SETUP_TEMPLATE} setup.py @ONLY)

    # Command depends on all source files, package-included or not
    # Record an explicit sentinel file for the build
    file(GLOB_RECURSE ALL_PACKAGE_FILES "${ADD_DIST_LIB_SOURCEDIR}/*")
    add_custom_command(
        DEPENDS ${ALL_PACKAGE_FILES}
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/timestamp"
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${ADD_DIST_LIB_SOURCECOPY}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${ADD_DIST_LIB_SOURCEDIR} ${ADD_DIST_LIB_SOURCECOPY}
        COMMAND ${PYTHON_BIN} "${CMAKE_CURRENT_BINARY_DIR}/setup.py" build --quiet
        COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/timestamp"
    )
    add_custom_target(${ADD_DIST_LIB_TARGET} ALL 
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/timestamp"
    )
    
    # Use DESTDIR from actual install environment
    set(ADD_DIST_LIB_INSTALL_CMD "${PYTHON_BIN} \"${CMAKE_CURRENT_BINARY_DIR}/setup.py\" install --root=\$DESTDIR/${CMAKE_INSTALL_PREFIX} --prefix=")
    if(PKG_INSTALL_PYTHONSITEDIR)
        set(ADD_DIST_LIB_INSTALL_CMD "${ADD_DIST_LIB_INSTALL_CMD} --install-lib=${PKG_INSTALL_PYTHONSITEDIR}")
    endif()
    install(
        CODE "execute_process(COMMAND ${ADD_DIST_LIB_INSTALL_CMD})"
        COMPONENT ${ADD_DIST_LIB_COMPONENT}
    )

endfunction(add_dist_pythonlibrary)

# Use qt "lrelease" to generate a translation binary from a source file.
# The named function arguments are:
#  TARGET: The output QM file to create.
#  SOURCE: The input TS file to read.
function(add_qt_translation)
    set(PARSE_OPTS )
    set(PARSE_ARGS_SINGLE TARGET SOURCE)
    set(PARSE_ARGS_MULTI )
    cmake_parse_arguments(ADD_TRANSLATION "${PARSE_OPTS}" "${PARSE_ARGS_SINGLE}" "${PARSE_ARGS_MULTI}" ${ARGN})
    if(NOT ADD_TRANSLATION_TARGET)
        message(FATAL_ERROR "add_qt_translation missing TARGET parameter")
    endif()
    if(NOT ADD_TRANSLATION_SOURCE)
        message(FATAL_ERROR "add_qt_translation missing SOURCE parameter")
    endif()

    find_package(Qt5LinguistTools)
    add_custom_command(
        OUTPUT ${ADD_TRANSLATION_TARGET}
        DEPENDS ${ADD_TRANSLATION_SOURCE}
        COMMAND Qt5::lrelease -qm "${ADD_TRANSLATION_TARGET}" "${ADD_TRANSLATION_SOURCE}"
    )
endfunction(add_qt_translation)

# Common run-time test behavior
set(GTEST_RUN_ARGS "--gtest_output=xml:test-detail.junit.xml")
function(add_gtest_executable TARGET_NAME ...)
    add_executable(${ARGV})
    set_target_properties(${TARGET_NAME} PROPERTIES 
        COMPILE_FLAGS "-DGTEST_LINKED_AS_SHARED_LIBRARY=1" 
    )
    target_include_directories(${TARGET_NAME} PRIVATE ${GTEST_INCLUDE_DIRS})
    target_link_libraries(${TARGET_NAME} PRIVATE ${GTEST_BOTH_LIBRARIES})
    
    find_package(Threads QUIET)
    if(TARGET Threads::Threads)
        target_link_libraries(${TARGET_NAME} PRIVATE Threads::Threads)
    endif()
        
    add_test(
        NAME ${TARGET_NAME}
        COMMAND ${TARGET_NAME} ${GTEST_RUN_ARGS}
    )
    set_property(
        TEST ${TARGET_NAME}
        APPEND PROPERTY
            ENVIRONMENT
                "TEST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
                "TEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    )
    if(UNIX)
        set_property(
            TEST ${TARGET_NAME}
            APPEND PROPERTY
                ENVIRONMENT
                    "XDG_DATA_DIRS=${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}:/usr/share"
        )
    elseif(WIN32)
        set_property(
            TEST ${TARGET_NAME}
            APPEND PROPERTY
                ENVIRONMENT
                    "LOCALAPPDATA=${CMAKE_INSTALL_PREFIX}\\${CMAKE_INSTALL_DATADIR}"
        )

        set(PATH_WIN "${CMAKE_INSTALL_PREFIX}\\bin\;${GTEST_PATH}\;$ENV{PATH}")
        # escape for ctest string processing
        string(REPLACE ";" "\\;" PATH_WIN "${PATH_WIN}")
        string(REPLACE "/" "\\" PATH_WIN "${PATH_WIN}")
        set_property(
            TEST ${TARGET_NAME}
            APPEND PROPERTY
                ENVIRONMENT
                    "PATH=${PATH_WIN}"
                    "QT_PLUGIN_PATH=${CMAKE_INSTALL_PREFIX}\\bin"
        )
    endif()
endfunction(add_gtest_executable)

function(add_nosetest_run TEST_NAME)
    find_program(NOSETEST_BIN "nosetests")
    if(NOT NOSETEST_BIN)
        message(FATAL_ERROR "Missing executable for 'nosetests'")
    endif()
    set(NOSETEST_RUN_ARGS "-v" "--with-xunit" "--xunit-file=test-detail.xunit.xml")
    add_test(
        NAME ${TEST_NAME}
        COMMAND ${NOSETEST_BIN} ${CMAKE_CURRENT_SOURCE_DIR} ${NOSETEST_RUN_ARGS}
    )

    set_property(
        TEST ${TEST_NAME}
        APPEND PROPERTY
            ENVIRONMENT
                "TEST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
                "TEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    )
    if(UNIX)
        set_property(
            TEST ${TEST_NAME}
            APPEND PROPERTY
                ENVIRONMENT
                    "XDG_DATA_DIRS=${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}:/usr/share"
        )
    elseif(WIN32)
        set_property(
            TEST ${TEST_NAME}
            APPEND PROPERTY
                ENVIRONMENT
                    "LOCALAPPDATA=${CMAKE_INSTALL_PREFIX}\\${CMAKE_INSTALL_DATADIR}"
        )
    endif()
endfunction(add_nosetest_run)

# 
# Define a web site library from scources. 
# Yarn build should output to /www directory relative to build directory.
# Function Arguments:
#   TARGET: the cmake target name to create
#   SETUP_TEMPLATES files to be used as templates for webpack.*.js
#   COMPONENT: The cmake "install" component to install the library as.
#   SOURCES: All of the dependencies of the target
#
function(add_dist_yarnlibrary)

    set(PARSE_OPTS)
    set(PARSE_ARGS_SINGLE TARGET COMPONENT)
    set(PARSE_ARGS_MULTI SOURCES SETUP_TEMPLATES)
    cmake_parse_arguments(ADD_DIST_LIB "${PARSE_OPTS}" "${PARSE_ARGS_SINGLE}" "${PARSE_ARGS_MULTI}" ${ARGN})
    if("${ADD_DIST_LIB_TARGET}" STREQUAL "")
        message(FATAL_ERROR "add_dist_yarnlibrary missing TARGET parameter")
    endif()

    if("${ADD_DIST_LIB_SETUP_TEMPLATES}" STREQUAL "")
        message(FATAL_ERROR "add_dist_yarnlibrary missing SETUP_TEMPLATES parameter")
    endif()

    if("${ADD_DIST_LIB_SOURCES}" STREQUAL "")
        message(FATAL_ERROR "add_dist_yarnlibrary missing SOURCES parameter")
    endif()

    # TODO: only build working is build-dev
    #if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(YARN_BUILD_TYPE "build-dev")
        message(STATUS "will build yarn in DEV mode.")
    #else()
    #    set(YARN_BUILD_TYPE "build")
    #endif()

    find_program(YARN "yarn")
    
    # Setuptools runs on copy of source in the build path
    set(ADD_DIST_LIB_SOURCECOPY "${CMAKE_CURRENT_BINARY_DIR}/pkg")

    foreach(SETUP_TEMPLATE ${ADD_DIST_LIB_SETUP_TEMPLATES})
	    message(STATUS "${ADD_DIST_LIB_SOURCECOPY}/${SETUP_TEMPLATE}")
        string(REPLACE ".in" ".js" CONFIG_NAME ${SETUP_TEMPLATE}) 
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${SETUP_TEMPLATE}" "${ADD_DIST_LIB_SOURCECOPY}/${CONFIG_NAME}" @ONLY)
    endforeach(SETUP_TEMPLATE)
    

    # Record an explicit sentinel file for the build
    add_custom_command(
        DEPENDS ${SOURCES}
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/timestamp"
        message(STATUS "Building YARN")
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/*" "${ADD_DIST_LIB_SOURCECOPY}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/src" "${ADD_DIST_LIB_SOURCECOPY}/src"
        COMMAND ${YARN} --cwd ${ADD_DIST_LIB_SOURCECOPY}
        COMMAND ${YARN} --cwd ${ADD_DIST_LIB_SOURCECOPY} version --no-git-tag-version --new-version "${PROJECT_VERSION}-${SVN_LAST_REVISION}"
        COMMAND ${YARN} --cwd ${ADD_DIST_LIB_SOURCECOPY} ${YARN_BUILD_TYPE}
        COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/timestamp"
    )

    add_custom_target(${ADD_DIST_LIB_TARGET} ALL 
        # COMMAND ${CMAKE_COMMAND} -E r ${CMAKE_CURRENT_BINARY_DIR}/timestamp
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/timestamp"
    )

    install(
        DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/www"
        DESTINATION "${PKG_INSTALL_DATADIR}"
        COMPONENT ${ADD_DIST_LIB_COMPONENT}
    )

endfunction(add_dist_yarnlibrary)
