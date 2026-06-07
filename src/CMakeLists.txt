find_package(nlohmann_json CONFIG REQUIRED)
find_package(type_safe CONFIG REQUIRED)

add_library(attractor STATIC
    attractor.cpp
    types.cpp
)

add_library(attractor::attractor ALIAS attractor)

target_include_directories(attractor
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(attractor
    PUBLIC
        nlohmann_json::nlohmann_json
        type_safe::type_safe
)

set_target_properties(attractor PROPERTIES
    OUTPUT_NAME "attractor"
)
