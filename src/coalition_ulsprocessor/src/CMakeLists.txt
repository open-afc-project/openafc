# Allow includes relative to this "src" directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
# Allow includes for generated headers
include_directories(${CMAKE_CURRENT_BINARY_DIR})


set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(BUILD_SHARED_LIBS ON)
set(CMAKE_AUTOMOC ON)

# Helper functions for all target types
include(srcfunctions)
set(TARGET_LIBS "")
set(TARGET_BINS "")
set(TARGET_SBINS "")

# Make visible to project scope
set(TARGET_LIBS "${TARGET_LIBS}" PARENT_SCOPE)
add_subdirectory(uls-script)