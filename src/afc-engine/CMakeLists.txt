# All source files to same target
set(TGT_NAME "afc-engine")

file(GLOB ALL_CPP "*.cpp")
file(GLOB ALL_HEADER "*.h")
add_dist_executable(TARGET ${TGT_NAME} SOURCES ${ALL_CPP} ${ALL_HEADER})

target_link_libraries(${TGT_NAME} PUBLIC Qt5::Core)
target_link_libraries(${TGT_NAME} PUBLIC Qt5::Concurrent)
target_link_libraries(${TGT_NAME} PUBLIC Qt5::Gui)
target_link_libraries(${TGT_NAME} PUBLIC Qt5::Widgets)
target_link_libraries(${TGT_NAME} PUBLIC Qt5::Sql)
target_link_libraries(${TGT_NAME} PUBLIC GDAL::GDAL)
target_link_libraries(${TGT_NAME} PUBLIC ${ARMADILLO_LIBRARIES})
target_link_libraries(${TGT_NAME} PUBLIC ${ZLIB_LIBRARIES})
target_link_libraries(${TGT_NAME} PUBLIC Boost::program_options)
target_link_libraries(${TGT_NAME} PUBLIC ratcommon)
target_link_libraries(${TGT_NAME} PUBLIC afclogging)
target_link_libraries(${TGT_NAME} PUBLIC afcsql)
