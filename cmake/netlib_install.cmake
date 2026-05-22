# @file netlib_install.cmake
# install + package config. Требует target netlib и NETLIB_ENABLE_INSTALL=ON.

if(NOT NETLIB_ENABLE_INSTALL)
    return()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS netlib EXPORT netlibTargets)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/netlib
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT netlibTargets
        FILE netlibTargets.cmake
        NAMESPACE netlib::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/netlib)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/netlibConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/netlibConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/netlib)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/netlibConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ${NETLIB_PACKAGE_COMPATIBILITY})

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/netlibConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/netlibConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/netlib)
