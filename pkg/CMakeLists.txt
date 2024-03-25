# Platform-independent CPack configuration
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")


# Combine space-separated list of internal libraries for ...Config.cmake
string(REPLACE ";" " " TARGET_LIBS_SPACESEP "${TARGET_LIBS}")

if(TARGET_LIBS)
  # CMake configuration for this library
  install(
    EXPORT ${CMAKE_MODULE_NAME}Targets
    FILE ${CMAKE_MODULE_NAME}Targets.cmake
    DESTINATION ${PKG_INSTALL_CMAKE_CONFIG_DIR}
    COMPONENT development
  )
endif(TARGET_LIBS)
configure_file(${CMAKE_MODULE_NAME}Config.cmake.in ${CMAKE_MODULE_NAME}Config.cmake @ONLY)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_MODULE_NAME}Config.cmake
    DESTINATION ${PKG_INSTALL_CMAKE_CONFIG_DIR}
    COMPONENT development
)
configure_file(${CMAKE_MODULE_NAME}ConfigVersion.cmake.in ${CMAKE_MODULE_NAME}ConfigVersion.cmake @ONLY)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_MODULE_NAME}ConfigVersion.cmake
    DESTINATION ${PKG_INSTALL_CMAKE_CONFIG_DIR}
    COMPONENT development
)

if(UNIX)
# CPack to create pre-rpm tarball
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
    "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_IGNORE_FILES
    "/build/;/doc/;/puppet/;/.svn/;~$;.sh$;${CPACK_SOURCE_IGNORE_FILES}")
include(CPack)

# RPM spec generator
set(RPM_PKG_NAME ${CMAKE_PROJECT_NAME})
set(RPM_PKG_SRC "${CMAKE_BINARY_DIR}/${CPACK_SOURCE_PACKAGE_FILE_NAME}.tar.bz2")
set(RPM_VERSION ${PROJECT_VERSION})
set(RPM_RELEASE ${SVN_LAST_REVISION})
set(RPM_DIST_DIR "${CMAKE_BINARY_DIR}/dist")

# Create spec file dynamically
set(RPM_SPEC_DOW Sun Mon Tue Wed Thu Fri Sat)
string(TIMESTAMP NOW_DOW_IX "%w")
message(DOW ${NOW_DOW_IX})
list(GET RPM_SPEC_DOW ${NOW_DOW_IX} NOW_DOW)
set(RPM_SPEC_MON NIL Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec)
string(TIMESTAMP NOW_MON_IX "%m")
message(MON ${NOW_MON_IX})
list(GET RPM_SPEC_MON ${NOW_MON_IX} NOW_MON)
string(TIMESTAMP NOW_DAY_YEAR "%d %Y")
set(RPM_CHANGELOG_DATE "${NOW_DOW} ${NOW_MON} ${NOW_DAY_YEAR}")
configure_file(${RPM_PKG_NAME}.spec.in ${RPM_PKG_NAME}.spec @ONLY)

# Target "package_source" defined by CPack
add_custom_target(rpm-prep
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/${RPM_PKG_NAME}.spec
        package_source
    COMMAND mkdir -p ${RPM_DIST_DIR}/SOURCES
    COMMAND mkdir -p ${RPM_DIST_DIR}/SPECS
    COMMAND mkdir -p ${RPM_DIST_DIR}/SRPMS
    COMMAND mkdir -p ${RPM_DIST_DIR}/RPMS
    COMMAND cp ${RPM_PKG_SRC} ${RPM_DIST_DIR}/SOURCES
    COMMAND cp ${RPM_PKG_NAME}.spec ${RPM_DIST_DIR}/SPECS
)
# Prepend to external RPMBUILD_ARGS definition
set(RPMBUILD_ARGS --define \"_topdir ${RPM_DIST_DIR}\" ${RPMBUILD_ARGS})
add_custom_target(rpm 
    DEPENDS rpm-prep
    COMMAND rpmbuild -ba ${RPMBUILD_ARGS} ${RPM_DIST_DIR}/SPECS/${RPM_PKG_NAME}.spec 
)

endif(UNIX)
