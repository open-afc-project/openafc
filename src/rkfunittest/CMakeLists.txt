set(TGT_NAME rkfunittest)
# All source files to same target
file(GLOB ALL_CPP "*.cpp")
file(GLOB ALL_HEADER "*.h")
add_dist_library(TARGET ${TGT_NAME} SOURCES ${ALL_CPP} HEADERS ${ALL_HEADER} EXPORTNAME fbratTargets)
set(TARGET_LIBS ${TARGET_LIBS} ${TGT_NAME} PARENT_SCOPE)

target_link_libraries(${TGT_NAME} PUBLIC ratcommon)
target_link_libraries(${TGT_NAME} PUBLIC rkflogging)
target_link_libraries(${TGT_NAME} PUBLIC Qt5::Core)
target_link_libraries(${TGT_NAME} PUBLIC Qt5::Network)
target_link_libraries(${TGT_NAME} PUBLIC Qt5::Sql)
target_link_libraries(${TGT_NAME} PUBLIC GTest::GTest)

#target_link_libraries(${TGT_NAME} ${Boost_LIBRARIES})
