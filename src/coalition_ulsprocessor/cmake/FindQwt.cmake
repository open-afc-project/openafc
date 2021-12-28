
IF(UNIX)
    find_package(PkgConfig)
    pkg_search_module(QWT REQUIRED Qt5Qwt6)
    if(NOT ${QWT_FOUND} EQUAL 1)
        message(FATAL_ERROR "Qwt is missing")
    endif() 
    set(QWT_LIBNAME "qwt-qt5")
ENDIF(UNIX)
IF(WIN32)
    # Always using shared library
    add_definitions(-DQWT_DLL)
    
    # Verify headers present
    find_path(QWT_MAIN_INCLUDE qwt.h PATHS ${QWT_INCLUDE_DIRS} ${CONAN_INCLUDE_DIRS_QWT})

    # Verify dynamic library present
    find_library(QWT_MAIN_LIB NAMES qwt qwtd PATHS ${QWT_LIBDIR} ${CONAN_LIB_DIRS_QWT})
    find_file(QWT_MAIN_DLL NAMES qwt.dll qwtd.dll PATHS ${QWT_BINDIR} ${CONAN_BIN_DIRS_QWT})
    message("-- Found Qwt at ${QWT_MAIN_LIB} ${QWT_MAIN_DLL}")
    
    add_library(qwt-qt5 SHARED IMPORTED)
    set_target_properties(qwt-qt5 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${QWT_MAIN_INCLUDE}
    )
    set_target_properties(qwt-qt5 PROPERTIES
        IMPORTED_LOCATION "${QWT_MAIN_DLL}"
        IMPORTED_IMPLIB "${QWT_MAIN_LIB}"
    )

    # handle the QUIETLY and REQUIRED arguments and set JPEG_FOUND to TRUE if
    # all listed variables are TRUE
    include(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(QWT DEFAULT_MSG QWT_MAIN_LIB QWT_MAIN_INCLUDE)

    if(QWT_FOUND)
        set(QWT_INCLUDE_DIRS ${QWT_MAIN_INCLUDE})
        set(QWT_LIBRARIES qwt-qt5)
    endif(QWT_FOUND)
ENDIF(WIN32)
