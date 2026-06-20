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
set(TYPE_SAFE_BUILD_TEST_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TYPE_SAFE_BUILD_DOC OFF CACHE BOOL "" FORCE)

# type_safe's external.cmake calls find_package(debug_assert) and then links to
# the non-namespaced "debug_assert" target, but the system cmake config only
# creates "debug_assert::debug_assert". Bridge the gap before MakeAvailable runs.
find_package(debug_assert QUIET)
if(debug_assert_FOUND AND TARGET debug_assert::debug_assert AND NOT TARGET debug_assert)
    # type_safe links to the non-namespaced "debug_assert" but the system cmake
    # config only creates "debug_assert::debug_assert". Create an IMPORTED shim
    # (IMPORTED so it's excluded from export-set checks in type_safe's install rules).
    add_library(debug_assert INTERFACE IMPORTED GLOBAL)
    get_target_property(_da_includes debug_assert::debug_assert INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(debug_assert PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_da_includes}")
endif()

FetchContent_MakeAvailable(type_safe)
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
