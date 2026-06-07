find_package(snitch CONFIG REQUIRED)

# ── Unit test executable ──────────────────────────────────────────────────────
add_executable(attractor_unit_tests
    unit/types_test.cpp
    unit/dot_parser_test.cpp
    unit/graph_model_test.cpp
    unit/validator_test.cpp
    unit/transform_test.cpp
    unit/engine_test.cpp
    unit/edge_selection_test.cpp
    unit/goal_gate_test.cpp
    unit/retry_test.cpp
    unit/context_test.cpp
    unit/checkpoint_test.cpp
    unit/artifact_store_test.cpp
    unit/events_test.cpp
    unit/handler_registry_test.cpp
    unit/condition_eval_test.cpp
    unit/handlers/start_handler_test.cpp
    unit/handlers/exit_handler_test.cpp
    unit/handlers/codergen_handler_test.cpp
    unit/handlers/wait_for_human_handler_test.cpp
    unit/handlers/conditional_handler_test.cpp
    unit/handlers/parallel_handler_test.cpp
    unit/handlers/fan_in_handler_test.cpp
    unit/handlers/tool_handler_test.cpp
    unit/handlers/manager_loop_handler_test.cpp
    unit/interviewers/auto_approve_interviewer_test.cpp
    unit/interviewers/console_interviewer_test.cpp
    unit/interviewers/callback_interviewer_test.cpp
    unit/interviewers/queue_interviewer_test.cpp
    unit/interviewers/recording_interviewer_test.cpp
)

target_link_libraries(attractor_unit_tests
    PRIVATE
        attractor::attractor
        snitch::snitch
)

target_include_directories(attractor_unit_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/support
)

# ── Integration test executable ───────────────────────────────────────────────
add_executable(attractor_integration_tests
    integration/smoke_test.cpp
    integration/parity_matrix_test.cpp
)

target_link_libraries(attractor_integration_tests
    PRIVATE
        attractor::attractor
        snitch::snitch
)

target_include_directories(attractor_integration_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/support
)

# ── CTest registration ─────────────────────────────────────────────────────────
include(CTest)

add_test(
    NAME unit_tests
    COMMAND attractor_unit_tests
)

add_test(
    NAME integration_tests
    COMMAND attractor_integration_tests
)

# Tag-based filtering examples (document in README):
#   ctest -R unit_tests --test-dir build/debug
#   ./build/debug/tests/attractor_unit_tests [dot_parser]
#   ./build/debug/tests/attractor_unit_tests [engine]
#   ./build/debug/tests/attractor_unit_tests --list-tests
