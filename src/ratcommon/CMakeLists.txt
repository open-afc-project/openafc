# All source files to same target
set(TGT_NAME "ratcommon")

file(GLOB ALL_CPP "*.cpp")
list(APPEND ALL_CPP "${CMAKE_CURRENT_BINARY_DIR}/RatVersion.cpp")
file(GLOB ALL_HEADER "*.h")
list(APPEND ALL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/RatVersion.h")
add_dist_library(TARGET ${TGT_NAME} SOURCES ${ALL_CPP} HEADERS ${ALL_HEADER} EXPORTNAME fbratTargets)
set(TARGET_LIBS ${TARGET_LIBS} ${TGT_NAME} PARENT_SCOPE)

configure_file(RatVersion.h.in RatVersion.h @ONLY)
configure_file(RatVersion.cpp.in RatVersion.cpp @ONLY)

target_link_libraries(${TGT_NAME} PUBLIC rkflogging)
target_link_libraries(${TGT_NAME} PUBLIC Qt5::Core)
target_link_libraries(${TGT_NAME} PUBLIC Qt5::Network)
target_link_libraries(${TGT_NAME} PUBLIC minizip)

add_subdirectory(test)
