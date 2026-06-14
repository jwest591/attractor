find_package(CLI11 CONFIG REQUIRED)

add_executable(attractor_cli
    main.cpp
    backends/claude_headless_backend.cpp
    backends/claude_tmux_backend.cpp
    backends/handoff_aware_backend.cpp
)

set_target_properties(attractor_cli PROPERTIES
    OUTPUT_NAME "attractor"
)

target_compile_definitions(attractor_cli PRIVATE
    ATTRACTOR_CLI_SCRIPTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scripts"
)

target_link_libraries(attractor_cli
    PRIVATE
        attractor::attractor
        CLI11::CLI11
)
