cmake_minimum_required(VERSION 3.30)

project(attractor
    VERSION 0.1.0
    DESCRIPTION "DOT-driven AI pipeline library"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_subdirectory(src)
add_subdirectory(cli)

enable_testing()
add_subdirectory(tests)

install(DIRECTORY include/attractor
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(TARGETS attractor
    EXPORT attractorTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT attractorTargets
    FILE attractorTargets.cmake
    NAMESPACE attractor::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/attractor
)

configure_package_config_file(
    cmake/attractorConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/attractorConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/attractor
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/attractorConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/attractorConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/attractorConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/attractor
)
