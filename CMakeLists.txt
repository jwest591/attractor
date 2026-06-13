cmake_minimum_required(VERSION 3.30)

project(attractor
    VERSION 0.1.0
    DESCRIPTION "DOT-driven AI pipeline library"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(FetchContent)

FetchContent_Declare(
    type_safe
    GIT_REPOSITORY https://github.com/jwest591/type_safe.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(type_safe)

# Workaround: CMake 4.x module-dependency scanning does not propagate transitive
# INTERFACE include dirs from vendored sub-targets (debug_assert lives inside
# type_safe's source tree). Add the include globally for the scan phase, and
# strip the dangling LINK_LIBRARIES entry on type_safe since debug_assert is
# header-only and produces no archive (CMake 4 mistakenly emits -ldebug_assert).
FetchContent_GetProperties(type_safe SOURCE_DIR _type_safe_src)
include_directories(SYSTEM "${_type_safe_src}/external/debug_assert")
set_target_properties(type_safe PROPERTIES
    INTERFACE_LINK_LIBRARIES ""
)

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
