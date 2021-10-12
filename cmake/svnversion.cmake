# This module provides a function to obtain SVN revision information and
# optionally use a local file as a revision name cache.
# The name cache is used in the absence of an actual checkout directory.


FIND_PROGRAM(SVNVERSION_EXECUTABLE svnversion
    DOC "svnversion command line")
MARK_AS_ADVANCED(SVNVERSION_EXECUTABLE)

if(SVNVERSION_EXECUTABLE)
    SET(SVNVERSION_FOUND TRUE)
    
    function(svnversion_info)
        set(PARSE_OPTS DEBUG)
        set(PARSE_ARGS_SINGLE PREFIX DIR SAVEFILE)
        set(PARSE_ARGS_MULTI )
        cmake_parse_arguments(SVNVERSION_INFO "${PARSE_OPTS}" "${PARSE_ARGS_SINGLE}" "${PARSE_ARGS_MULTI}" ${ARGN})
        if("${SVNVERSION_INFO_PREFIX}" STREQUAL "")
            message(FATAL_ERROR "svnversion_info missing PREFIX parameter")
        endif()
        
        # Existing cached revision
        if((NOT "${SVNVERSION_INFO_SAVEFILE}" STREQUAL "")
           AND (EXISTS "${SVNVERSION_INFO_SAVEFILE}"))
            file(READ "${SVNVERSION_INFO_SAVEFILE}" SVNVERSION_SAVEFILE_REVISION)
            string(STRIP ${SVNVERSION_SAVEFILE_REVISION} SVNVERSION_SAVEFILE_REVISION)
        else()
            set(SVNVERSION_SAVEFILE_REVISION "")
        endif()
        if(${SVNVERSION_INFO_DEBUG})
            message("svnversion ${SVNVERSION_INFO_DIR} SAVEFILE ${SVNVERSION_SAVEFILE_REVISION}")
        endif()
        
        # Setup environ
        SET(_SVNVERSION_SAVED_LC_ALL "$ENV{LC_ALL}" )
        SET(ENV{LC_ALL} C)
        # Run procs
        EXECUTE_PROCESS(
            COMMAND ${SVNVERSION_EXECUTABLE} --version
            WORKING_DIRECTORY ${SVNVERSION_INFO_DIR}
            OUTPUT_VARIABLE SVNVERSION_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        EXECUTE_PROCESS(
            COMMAND ${SVNVERSION_EXECUTABLE} -c ${SVNVERSION_INFO_DIR}
            OUTPUT_VARIABLE SVNVERSION_DIR_OUTPUT
            ERROR_VARIABLE SVNVERSION_CMD_ERROR
            RESULT_VARIABLE SVNVERSION_CMD_RESULT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        # Restore environ
        set(ENV{LC_ALL} ${_SVNVERSION_SAVED_LC_ALL})
        
        # Extract revision from result
        if(NOT ${SVNVERSION_CMD_RESULT} EQUAL 0)
            MESSAGE(SEND_ERROR "Command \"${SVNVERSION_EXECUTABLE} -c ${SVNVERSION_INFO_DIR}\" failed with output:\n${SVNVERSION_CMD_ERROR}")
        else(NOT ${SVNVERSION_CMD_RESULT} EQUAL 0)
            if("${SVNVERSION_DIR_OUTPUT}" STREQUAL "Unversioned directory")
                SET(SVNVERSION_DIR_REVISION "")
            else()
                # Extract just the revision number
                STRING(
                    REGEX REPLACE "^(([0-9]+):)([0-9]+[MSP]*)"
                    "\\3" SVNVERSION_DIR_REVISION "${SVNVERSION_DIR_OUTPUT}"
                )
            endif()
        endif()
        if(${SVNVERSION_INFO_DEBUG})
            message("svnversion ${SVNVERSION_INFO_DIR} DIR ${SVNVERSION_DIR_REVISION}")
        endif()
        
        # Prefer directory over savefile
        if(NOT "${SVNVERSION_DIR_REVISION}" STREQUAL "")
            set(SVNVERSION_LAST_REVISION "${SVNVERSION_DIR_REVISION}")
        else()
            set(SVNVERSION_LAST_REVISION "${SVNVERSION_SAVEFILE_REVISION}")
        endif()
        
        # Update savefile if needed
        if((NOT "${SVNVERSION_INFO_SAVEFILE}" STREQUAL "")
           AND (NOT "${SVNVERSION_SAVEFILE_REVISION}" STREQUAL "${SVNVERSION_LAST_REVISION}"))
            file(WRITE "${SVNVERSION_INFO_SAVEFILE}" "${SVNVERSION_LAST_REVISION}")
        endif()
        
        # output variables
        set(${SVNVERSION_INFO_PREFIX}_LAST_REVISION "${SVNVERSION_LAST_REVISION}" PARENT_SCOPE)
        
    endfunction(svnversion_info)
    
endif(SVNVERSION_EXECUTABLE)
