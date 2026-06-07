find_package(CLI11 CONFIG REQUIRED)

add_executable(attractor_cli
    main.cpp
)

set_target_properties(attractor_cli PROPERTIES
    OUTPUT_NAME "attractor"
)

target_link_libraries(attractor_cli
    PRIVATE
        attractor::attractor
        CLI11::CLI11
)
