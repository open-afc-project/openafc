
IF(UNIX)
    # Verify headers present
    find_path(V8_MAIN_INCLUDE v8.h ${V8_INCLUDE_DIR})

    # Verify dynamic library present
    find_library(V8_MAIN_LIB v8 ${V8_LIBDIR})
    message("-- Found V8 at ${V8_MAIN_LIB}")
    add_library(v8 SHARED IMPORTED)
    set_target_properties(v8 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${V8_MAIN_INCLUDE}
    )
    set_target_properties(v8 PROPERTIES
        IMPORTED_LOCATION "${V8_MAIN_LIB}"
    )

    find_library(V8_LIBBASE_LIB v8_libbase ${V8_LIBDIR})
    message("-- Found v8_libbase at ${V8_LIBBASE_LIB}")
    add_library(v8_libbase SHARED IMPORTED)
    set_target_properties(v8_libbase PROPERTIES
        IMPORTED_LOCATION "${V8_LIBBASE_LIB}"
    )
    
    find_library(V8_LIBPLATFORM_LIB v8_libplatform ${V8_LIBDIR})
    message("-- Found v8_libplatform at ${V8_LIBPLATFORM_LIB}")
    add_library(v8_libplatform SHARED IMPORTED)
    set_target_properties(v8_libplatform PROPERTIES
        IMPORTED_LOCATION "${V8_LIBPLATFORM_LIB}"
    )
    
    # handle the QUIETLY and REQUIRED arguments and set V8_FOUND to TRUE if
    # all listed variables are TRUE
    include(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(V8 DEFAULT_MSG V8_MAIN_INCLUDE V8_MAIN_LIB V8_LIBBASE_LIB V8_LIBPLATFORM_LIB)

    if(V8_FOUND)
        set(V8_INCLUDE_DIRS ${V8_MAIN_INCLUDE})
        set(V8_LIBRARIES v8 v8_libplatform v8_libbase)
    endif(V8_FOUND)
ENDIF(UNIX)
IF(WIN32)
    # Verify headers present
    find_path(V8_MAIN_INCLUDE v8.h ${V8_INCLUDE_DIR})

    # Verify dynamic library present
    find_library(V8_MAIN_LIB v8.dll.lib ${V8_LIBDIR})
    find_file(V8_MAIN_DLL v8.dll ${V8_BINDIR})
    message("-- Found V8 at ${V8_MAIN_LIB}")
    add_library(v8 SHARED IMPORTED)
    set_target_properties(v8 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${V8_MAIN_INCLUDE}
    )
    set_target_properties(v8 PROPERTIES
        IMPORTED_LOCATION "${V8_MAIN_DLL}"
        IMPORTED_IMPLIB "${V8_MAIN_LIB}"
    )
    
    find_library(V8_LIBBASE_LIB v8_libbase.dll.lib ${V8_LIBDIR})
    find_library(V8_LIBBASE_DLL v8_libbase.dll ${V8_BINDIR})
    add_library(v8_libbase SHARED IMPORTED)
    set_target_properties(v8_libbase PROPERTIES
        IMPORTED_LOCATION "${V8_LIBBASE_DLL}"
        IMPORTED_IMPLIB "${V8_LIBBASE_LIB}"
    )
    
    find_library(V8_LIBPLATFORM_LIB v8_libplatform.dll.lib ${V8_LIBDIR})
    find_library(V8_LIBPLATFORM_DLL v8_libplatform.dll ${V8_BINDIR})
    add_library(v8_libplatform SHARED IMPORTED)
    set_target_properties(v8_libplatform PROPERTIES
        IMPORTED_LOCATION "${V8_LIBPLATFORM_DLL}"
        IMPORTED_IMPLIB "${V8_LIBPLATFORM_LIB}"
    )
    
    # handle the QUIETLY and REQUIRED arguments and set V8_FOUND to TRUE if
    # all listed variables are TRUE
    include(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(V8 DEFAULT_MSG V8_MAIN_INCLUDE V8_MAIN_LIB V8_LIBBASE_LIB V8_LIBPLATFORM_LIB)

    if(V8_FOUND)
        set(V8_INCLUDE_DIRS ${V8_MAIN_INCLUDE})
        set(V8_LIBRARIES v8 v8_libplatform v8_libbase winmm)
    endif(V8_FOUND)
ENDIF(WIN32)
