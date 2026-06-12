find_package(nlohmann_json CONFIG REQUIRED)
find_package(type_safe CONFIG REQUIRED)
find_package(stdexec CONFIG REQUIRED)

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
    events.cpp
    artifact_store.cpp
    handlers/start_handler.cpp
    handlers/exit_handler.cpp
    handlers/conditional_handler.cpp
    handlers/codergen_handler.cpp
    handlers/tool_handler.cpp
    handlers/manager_loop_handler.cpp
    handlers/wait_for_human_handler.cpp
    handlers/parallel_handler.cpp
    interviewers/auto_approve_interviewer.cpp
    interviewers/queue_interviewer.cpp
    interviewers/recording_interviewer.cpp
    interviewers/console_interviewer.cpp
    interviewers/callback_interviewer.cpp
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
        STDEXEC::stdexec
)

set_target_properties(attractor PROPERTIES
    OUTPUT_NAME "attractor"
)

# GCC 16 ICE: -fmodules-ts + stdexec inline constexpr declarations triggers a
# segfault in transfer_defining_module. Opt this TU out of module scanning;
# it uses no named modules.
set_source_files_properties(handlers/parallel_handler.cpp
    PROPERTIES CXX_SCAN_FOR_MODULES OFF)
