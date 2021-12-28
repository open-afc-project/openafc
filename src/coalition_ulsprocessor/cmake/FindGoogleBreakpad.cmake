
IF(UNIX)
    find_package(PkgConfig)
    pkg_search_module(BREAKPAD REQUIRED google-breakpad)
ENDIF(UNIX)
IF(WIN32)
    find_path(BREAKPAD_HEADER_DIR common/basictypes.h ${BREAKPAD_INCLUDE_DIRS} ${CONAN_INCLUDE_DIRS})

    add_library(breakpad_common STATIC IMPORTED)
    find_library(BREAKPAD_COMMON_LIB common)
    set_target_properties(breakpad_common PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${BREAKPAD_HEADER_DIR}
        IMPORTED_LOCATION "${BREAKPAD_COMMON_LIB}"
    )
    add_library(breakpad_handler STATIC IMPORTED)
    find_library(BREAKPAD_HANDLER_LIB exception_handler)
    set_target_properties(breakpad_handler PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${BREAKPAD_HEADER_DIR}
        IMPORTED_LOCATION "${BREAKPAD_HANDLER_LIB}"
    )
    add_library(breakpad_client STATIC IMPORTED)
    find_library(BREAKPAD_CLIENT_LIB crash_generation_client)
    set_target_properties(breakpad_client PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${BREAKPAD_HEADER_DIR}
        IMPORTED_LOCATION "${BREAKPAD_CLIENT_LIB}"
    )
    
    # handle the QUIETLY and REQUIRED arguments and set JPEG_FOUND to TRUE if
    # all listed variables are TRUE
    include(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(BREAKPAD DEFAULT_MSG BREAKPAD_HEADER_DIR BREAKPAD_COMMON_LIB BREAKPAD_HANDLER_LIB BREAKPAD_CLIENT_LIB)

    if(BREAKPAD_FOUND)
        set(BREAKPAD_INCLUDE_DIRS ${BREAKPAD_HEADER_DIR})
        set(BREAKPAD_LIBRARIES breakpad_common breakpad_handler breakpad_client)
    endif(BREAKPAD_FOUND)
ENDIF(WIN32)
