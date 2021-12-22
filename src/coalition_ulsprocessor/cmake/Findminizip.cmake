
IF(UNIX)
    find_package(PkgConfig)
    pkg_search_module(MINIZIP REQUIRED minizip)
    if(NOT ${MINIZIP_FOUND} EQUAL 1)
        message(FATAL_ERROR "minizip is missing")
    endif() 
ENDIF(UNIX)
IF(WIN32)
    # Verify headers present
    find_path(MINIZIP_MAIN_INCLUDE minizip/zip.h PATHS ${MINIZIP_INCLUDE_DIRS} ${CONAN_INCLUDE_DIRS})

    # Verify link and dynamic library present
    find_library(MINIZIP_MAIN_LIB NAMES minizip minizipd PATHS ${MINIZIP_LIBDIR} ${CONAN_LIB_DIRS})
    find_file(MINIZIP_MAIN_DLL NAMES minizip.dll minizipd.dll PATHS ${MINIZIP_BINDIR} ${CONAN_BIN_DIRS})
    message("-- Found minizip at ${MINIZIP_MAIN_LIB} ${MINIZIP_MAIN_DLL}")
    
    add_library(minizip SHARED IMPORTED)
    set_target_properties(minizip PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${MINIZIP_MAIN_INCLUDE}
    )
    set_target_properties(minizip PROPERTIES
        IMPORTED_LOCATION "${MINIZIP_MAIN_DLL}"
        IMPORTED_IMPLIB "${MINIZIP_MAIN_LIB}"
    )

    # handle the QUIETLY and REQUIRED arguments and set JPEG_FOUND to TRUE if
    # all listed variables are TRUE
    include(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(MINIZIP DEFAULT_MSG MINIZIP_MAIN_LIB MINIZIP_MAIN_INCLUDE)

    if(MINIZIP_FOUND)
        set(MINIZIP_INCLUDE_DIRS ${MINIZIP_MAIN_INCLUDE})
        set(MINIZIP_LIBRARIES minizip)
    endif(MINIZIP_FOUND)
ENDIF(WIN32)
