find_package(nlohmann_json CONFIG REQUIRED)
find_package(type_safe CONFIG REQUIRED)

# vcpkg's type_safe port creates 'type_safe'; local source-install creates 'type_safe::type_safe'
if(TARGET type_safe::type_safe)
    set(TYPE_SAFE_TARGET type_safe::type_safe)
else()
    set(TYPE_SAFE_TARGET type_safe)
endif()

add_library(attractor STATIC
    attractor.cpp
    types.cpp
    graph.cpp
    dot_parser.cpp
    validator.cpp
    transform.cpp
    handler_registry.cpp
    engine.cpp
    context.cpp
    checkpoint.cpp
    handlers/start_handler.cpp
    handlers/exit_handler.cpp
    handlers/conditional_handler.cpp
    handlers/codergen_handler.cpp
    backends/noop_backend.cpp
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
        ${TYPE_SAFE_TARGET}
)

set_target_properties(attractor PROPERTIES
    OUTPUT_NAME "attractor"
)
