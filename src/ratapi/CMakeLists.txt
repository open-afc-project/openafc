# Python packaging
set(TGT_NAME "ratapi")

add_dist_pythonlibrary(
    TARGET ${TGT_NAME}
    SETUP_TEMPLATE setup.py.in
    SOURCEDIR "${CMAKE_CURRENT_SOURCE_DIR}"
    COMPONENT runtime
)
