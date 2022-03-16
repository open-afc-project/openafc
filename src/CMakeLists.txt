# Allow includes relative to this "src" directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
# Allow includes for generated headers
include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories(${Boost_INCLUDE_DIRS})

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(BUILD_SHARED_LIBS ON)
set(CMAKE_AUTOMOC ON)

# Helper functions for all target types
include(srcfunctions)
set(TARGET_LIBS "")
set(TARGET_BINS "")
set(TARGET_SBINS "")

# Build in selected sub-directories
add_subdirectory(ratapi)
add_subdirectory(web)
if (CMAKE_BUILD_TYPE STREQUAL "DebugWithTest")
  add_subdirectory(rkfunittest)
endif()
add_subdirectory(ratcommon)
add_subdirectory(rkflogging)
add_subdirectory(rkfsql)
add_subdirectory(coalition_ulsprocessor)
if(BUILD_AFCENGINE)
    add_subdirectory(afc-engine)
endif(BUILD_AFCENGINE)

# Make visible to project scope
set(TARGET_LIBS "${TARGET_LIBS}" PARENT_SCOPE)
