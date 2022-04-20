# External version naming
file(READ "${CMAKE_SOURCE_DIR}/version.txt" VERSIONFILE)
string(STRIP ${VERSIONFILE} VERSIONFILE)
project(uls-script VERSION ${VERSIONFILE})

set(SOVERSION "${PROJECT_VERSION}")

# Build/install in source path
add_subdirectory(src)

