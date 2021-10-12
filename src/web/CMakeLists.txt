# Web Site packaging
set(TGT_NAME "web")

file(GLOB_RECURSE DEPS "${CMAKE_CURRENT_SOURCE_DIR}/src/*")
# file(GLOB TEMPLATES "webpack.common.in" "webpack.dev.in" "webpack.prod.in")

add_dist_yarnlibrary(
    TARGET ${TGT_NAME}
    SETUP_TEMPLATES "webpack.common.in;webpack.dev.in;webpack.prod.in"
    SOURCES ${DEPS}
    COMPONENT runtime
)
