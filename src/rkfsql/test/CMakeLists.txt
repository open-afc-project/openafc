# All source files to same target
file(GLOB ALL_CPP "*.cpp")
add_gtest_executable(${TGT_NAME}-test ${ALL_CPP})
target_link_libraries(${TGT_NAME}-test PUBLIC ${TGT_NAME})
target_link_libraries(${TGT_NAME}-test PUBLIC gtest_main)
target_link_libraries(${TGT_NAME}-test PUBLIC rkfunittest)
