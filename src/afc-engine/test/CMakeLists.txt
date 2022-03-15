# All source files to same target
file(GLOB ALL_CPP "*.cpp" "../*.cpp")

file(GLOB MAIN "../main.cpp")

list(REMOVE_ITEM ALL_CPP ${MAIN})

add_gtest_executable(${TGT_NAME}-test ${ALL_CPP} ${ALL_HEADER})
target_link_libraries(${TGT_NAME}-test PUBLIC Qt5::Core)
target_link_libraries(${TGT_NAME}-test PUBLIC Qt5::Concurrent)
target_link_libraries(${TGT_NAME}-test PUBLIC Qt5::Gui)
target_link_libraries(${TGT_NAME}-test PUBLIC Qt5::Widgets)
target_link_libraries(${TGT_NAME}-test PUBLIC Qt5::Sql)
target_link_libraries(${TGT_NAME}-test PUBLIC GDAL::GDAL)
target_link_libraries(${TGT_NAME}-test PUBLIC iturp452)
target_link_libraries(${TGT_NAME}-test PUBLIC ${ARMADILLO_LIBRARIES})
target_link_libraries(${TGT_NAME}-test PUBLIC Boost::program_options)
target_link_libraries(${TGT_NAME}-test PUBLIC rkfsql)
target_link_libraries(${TGT_NAME}-test PUBLIC rkfunittest)
target_link_libraries(${TGT_NAME}-test PUBLIC gtest_main)
