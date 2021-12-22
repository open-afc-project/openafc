# Add a target to allow test coverage analysis (all tests together).
# This include fails on non-unix systems

if(NOT UNIX)
    message(FATAL_ERROR "Unable to coverage-check non-unix")
endif(NOT UNIX)


FIND_PROGRAM(LCOV_PATH lcov)
FIND_PROGRAM(GENHTML_PATH genhtml)

IF(NOT LCOV_PATH)
    MESSAGE(FATAL_ERROR "lcov not found")
ENDIF(NOT LCOV_PATH)

IF(NOT GENHTML_PATH)
    MESSAGE(FATAL_ERROR "genhtml not found!")
ENDIF(NOT GENHTML_PATH)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

# Target to check with pre- and post-steps to capture coverage results
SET(OUT_RAW "${CMAKE_BINARY_DIR}/coverage-raw.lcov")
SET(OUT_CLEAN "${CMAKE_BINARY_DIR}/coverage-clean.lcov")
add_custom_target(check-coverage
    # Before any tests
    COMMAND ${LCOV_PATH} --directory . --zerocounters
    
    # Actually run the tests, ignoring exit code
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose || :
    
    # Pull together results, ignoring system files and auto-built files
    COMMAND ${LCOV_PATH} --directory . --capture --output-file ${OUT_RAW}
    COMMAND ${LCOV_PATH} --remove ${OUT_RAW} '*/test/*' '${CMAKE_BINARY_DIR}/*' '/usr/*' --output-file ${OUT_CLEAN}
    COMMAND ${GENHTML_PATH} -o coverage ${OUT_CLEAN}
    COMMAND ${CMAKE_COMMAND} -E remove ${OUT_RAW} ${OUT_CLEAN}
    
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
