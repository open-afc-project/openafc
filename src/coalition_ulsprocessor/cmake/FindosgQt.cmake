
IF(UNIX)
    find_package(PkgConfig)
    pkg_search_module(OSGQT REQUIRED openscenegraph-osgQt5 openscenegraph-osgQt)
    if(NOT ${OSGQT_FOUND} EQUAL 1)
        message(FATAL_ERROR "osgQt is missing")
    endif() 
ENDIF(UNIX)
IF(WIN32)
    # Verify headers present
    find_path(OSGQT_MAIN_INCLUDE osgQt/GraphicsWindowQt PATHS ${OSGQT_INCLUDE_DIRS} ${CONAN_INCLUDE_DIRS_OSGQT})

    # Verify dynamic library present
    find_library(OSGQT_MAIN_LIB NAMES osgQt5 osgQt osgQt5d osgQtd PATHS ${OSGQT_LIBDIR} ${CONAN_LIB_DIRS_OSGQT})
    find_file(OSGQT_MAIN_DLL NAMES osgQt5.dll osgQt.dll osgQt5d.dll osgQtd.dll PATHS ${OSGQT_BINDIR} ${CONAN_BIN_DIRS_OSGQT})
    message("-- Found osgQt at ${OSGQT_MAIN_LIB} ${OSGQT_MAIN_DLL}")
    
    add_library(osgQt SHARED IMPORTED)
    set_target_properties(osgQt PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${OSGQT_MAIN_INCLUDE}
    )
    set_target_properties(osgQt PROPERTIES
        IMPORTED_LOCATION "${OSGQT_MAIN_DLL}"
        IMPORTED_IMPLIB "${OSGQT_MAIN_LIB}"
    )

    # handle the QUIETLY and REQUIRED arguments and set JPEG_FOUND to TRUE if
    # all listed variables are TRUE
    include(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(OSGQT DEFAULT_MSG OSGQT_MAIN_LIB OSGQT_MAIN_INCLUDE)

    if(OSGQT_FOUND)
        set(OSGQT_INCLUDE_DIRS ${OSGQT_MAIN_INCLUDE})
        set(OSGQT_LIBRARIES osgQt)
    endif(OSGQT_FOUND)
ENDIF(WIN32)
