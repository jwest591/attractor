#include "backend_utils.hpp"

#include <attractor/types.hpp>
#include <snitch/snitch.hpp>

#include <filesystem>
#include <string>

using namespace attractor;

SNITCH_TEST_CASE("[backend_utils] derive_node_log_dir returns logs_root/NNN-node_id -- 7.20-U-BU-001")
{
    const std::filesystem::path logs_root{"/tmp/att-test-logs"};
    const NodeId node_id{"my-node"};
    const int counter = 3;

    const auto result = derive_node_log_dir(logs_root, node_id, counter);

    SNITCH_CHECK(result == logs_root / "003-my-node");
}

SNITCH_TEST_CASE("[backend_utils] derive_node_log_dir counter 1 zero-pads to 001 -- 7.20-U-BU-002")
{
    const std::filesystem::path logs_root{"/runs/abc"};
    const NodeId node_id{"step-a"};

    const auto result = derive_node_log_dir(logs_root, node_id, 1);

    SNITCH_CHECK(result.filename().string() == "001-step-a");
    SNITCH_CHECK(result.parent_path() == logs_root);
}
