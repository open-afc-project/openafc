# All source files to same target
set(TGT_NAME "uls-script")

file(GLOB ALL_CPP "*.[ch]pp")
file(GLOB ALL_HEADER "*.h")
add_dist_executable(TARGET ${TGT_NAME} SOURCES ${ALL_CPP} ${ALL_HEADER})

target_link_libraries(${TGT_NAME} PUBLIC Qt5::Core)