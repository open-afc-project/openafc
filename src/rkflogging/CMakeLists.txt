# All source files to same target
set(TGT_NAME "rkflogging")

file(GLOB ALL_CPP "*.cpp")
file(GLOB ALL_HEADER "*.h")
add_dist_library(TARGET ${TGT_NAME} SOURCES ${ALL_CPP} HEADERS ${ALL_HEADER} EXPORTNAME fbratTargets)
set(TARGET_LIBS ${TARGET_LIBS} ${TGT_NAME} PARENT_SCOPE)

target_link_libraries(${TGT_NAME} PUBLIC Qt5::Core)
target_link_libraries(${TGT_NAME} PUBLIC Boost::log)
target_link_libraries(${TGT_NAME} PUBLIC Boost::regex)

if (CMAKE_BUILD_TYPE STREQUAL "DebugWithTest")
  add_subdirectory(test)
endif()
