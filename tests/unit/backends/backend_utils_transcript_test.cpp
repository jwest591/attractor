#include "backend_utils.hpp"

#include <attractor/types.hpp>
#include <snitch/snitch.hpp>

#include <filesystem>
#include <string>

using namespace attractor;

SNITCH_TEST_CASE("[backend_utils] derive_node_log_dir returns logs_root/node_id-counter -- 7.19-U-BU-001")
{
    const std::filesystem::path logs_root{"/tmp/att-test-logs"};
    const NodeId node_id{"my-node"};
    const int counter = 3;

    const auto result = derive_node_log_dir(logs_root, node_id, counter);

    SNITCH_CHECK(result == logs_root / "my-node-3");
}

SNITCH_TEST_CASE("[backend_utils] derive_node_log_dir counter 1 produces expected name -- 7.19-U-BU-002")
{
    const std::filesystem::path logs_root{"/runs/abc"};
    const NodeId node_id{"step-a"};

    const auto result = derive_node_log_dir(logs_root, node_id, 1);

    SNITCH_CHECK(result.filename().string() == "step-a-1");
    SNITCH_CHECK(result.parent_path() == logs_root);
}
