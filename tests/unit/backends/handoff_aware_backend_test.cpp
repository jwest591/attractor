#include "handoff_aware_backend.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <snitch/snitch.hpp>
#include <type_safe/strong_typedef.hpp>

#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace attractor;

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
struct TmpDir {
    explicit TmpDir(std::filesystem::path p) : path{std::move(p)} {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }
    TmpDir(const TmpDir&) = delete;
    TmpDir& operator=(const TmpDir&) = delete;
    ~TmpDir() noexcept { std::error_code ec; std::filesystem::remove_all(path, ec); }
    std::filesystem::path path;
};

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class MockBackend : public CodergenBackend {
  public:
    using RunFn = std::function<std::expected<LlmResponse, Outcome>(
        const Node&, const PromptText&, Context&)>;

    explicit MockBackend(RunFn fn) : m_fn{std::move(fn)} {}

    mutable std::vector<std::string> received_prompts;
    mutable std::vector<int> observed_counters;

    [[nodiscard]] auto run(const Node& node,
                           const PromptText& prompt,
                           Context& ctx) const
        -> std::expected<LlmResponse, Outcome> override
    {
        received_prompts.push_back(type_safe::get(prompt));
        observed_counters.push_back(ctx.current_execution_counter());
        return m_fn(node, prompt, ctx);
    }

  private:
    RunFn m_fn;
};

Node make_node(const std::string& id_val)
{
    Node node{};
    node.id = NodeId{id_val};
    return node;
}

}  // namespace

SNITCH_TEST_CASE("[handoff_aware] engine counter is read (not incremented) by backend -- 7.20-U-005")
{
    TmpDir logs_root{std::filesystem::temp_directory_path() / "att_hab_7_20_005"};
    const Node node = make_node("n005");

    auto mock = std::make_unique<MockBackend>(
        [](const Node&, const PromptText&, Context&)
            -> std::expected<LlmResponse, Outcome> {
            return LlmResponse{"ok"};
        });
    MockBackend* mock_ptr = mock.get();

    HandoffAwareBackend backend{std::move(mock), logs_root.path};
    Context ctx;
    (void)ctx.next_execution_counter();  // simulate engine increment
    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_REQUIRE(mock_ptr->observed_counters.size() == 1u);
    SNITCH_CHECK(mock_ptr->observed_counters[0] == 1);
    SNITCH_CHECK(ctx.current_execution_counter() == 1);  // HAB must not change the counter
}

SNITCH_TEST_CASE("[handoff_aware] handoff.md in 001/ triggers second call with file content -- 7.20-U-006")
{
    TmpDir logs_root{std::filesystem::temp_directory_path() / "att_hab_7_20_006"};
    const Node node = make_node("n006");

    int call_count = 0;
    auto mock = std::make_unique<MockBackend>(
        [&call_count, &logs_root](const Node& n, const PromptText&, Context& ctx)
            -> std::expected<LlmResponse, Outcome> {
            ++call_count;
            if (call_count == 1) {
                const int counter = ctx.current_execution_counter();
                const auto node_dir = logs_root.path /
                    std::format("{:03d}-{}", counter, type_safe::get(n.id));
                const auto handoff_subdir = node_dir / "001";
                std::filesystem::create_directories(handoff_subdir);
                std::ofstream{handoff_subdir / "handoff.md"} << "next prompt content";
                return LlmResponse{"wrote handoff"};
            }
            return LlmResponse{"final"};
        });
    MockBackend* mock_ptr = mock.get();

    HandoffAwareBackend backend{std::move(mock), logs_root.path};
    Context ctx;
    (void)ctx.next_execution_counter();  // simulate engine increment
    auto result = backend.run(node, PromptText{"initial"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "final");
    SNITCH_REQUIRE(mock_ptr->received_prompts.size() == 2u);
    SNITCH_CHECK(mock_ptr->received_prompts[1] == "next prompt content");
    SNITCH_REQUIRE(mock_ptr->observed_counters.size() == 2u);
    SNITCH_CHECK(mock_ptr->observed_counters[0] == 1);
    SNITCH_CHECK(mock_ptr->observed_counters[1] == 1);  // counter unchanged by HAB
}

SNITCH_TEST_CASE("[handoff_aware] empty handoff.md in 001/ returns fail -- 7.20-U-007")
{
    TmpDir logs_root{std::filesystem::temp_directory_path() / "att_hab_7_20_007"};
    const Node node = make_node("n007");

    auto mock = std::make_unique<MockBackend>(
        [&logs_root](const Node& n, const PromptText&, Context& ctx)
            -> std::expected<LlmResponse, Outcome> {
            const int counter = ctx.current_execution_counter();
            const auto node_dir = logs_root.path /
                std::format("{:03d}-{}", counter, type_safe::get(n.id));
            const auto handoff_subdir = node_dir / "001";
            std::filesystem::create_directories(handoff_subdir);
            std::ofstream{handoff_subdir / "handoff.md"};  // empty file
            return LlmResponse{"wrote empty handoff"};
        });

    HandoffAwareBackend backend{std::move(mock), logs_root.path};
    Context ctx;
    (void)ctx.next_execution_counter();
    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("empty") != std::string::npos);
}

SNITCH_TEST_CASE("[handoff_aware] max_handoffs=1 exhausted returns fail -- 7.20-U-008")
{
    TmpDir logs_root{std::filesystem::temp_directory_path() / "att_hab_7_20_008"};
    const Node node = make_node("n008");

    int call_count = 0;
    auto mock = std::make_unique<MockBackend>(
        [&call_count, &logs_root](const Node& n, const PromptText&, Context& ctx)
            -> std::expected<LlmResponse, Outcome> {
            const int idx = ++call_count;
            const int counter = ctx.current_execution_counter();
            const auto node_dir = logs_root.path /
                std::format("{:03d}-{}", counter, type_safe::get(n.id));
            const auto handoff_subdir = node_dir / std::format("{:03d}", idx);
            std::filesystem::create_directories(handoff_subdir);
            std::ofstream{handoff_subdir / "handoff.md"} << "handoff content";
            return LlmResponse{"wrote handoff"};
        });
    MockBackend* mock_ptr = mock.get();

    HandoffAwareBackend backend{std::move(mock), logs_root.path, /*max_handoffs=*/1};
    Context ctx;
    (void)ctx.next_execution_counter();
    auto result = backend.run(node, PromptText{"initial"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    SNITCH_CHECK(mock_ptr->received_prompts.size() == 2u);  // initial + 1 handoff attempt
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("max handoff") != std::string::npos);
}

SNITCH_TEST_CASE("[handoff_aware] max_handoffs=0 inner called once; handoff triggers immediate fail -- 7.8-U-001")
{
    TmpDir logs_root{std::filesystem::temp_directory_path() / "att_hab_7_8_001"};
    const Node node = make_node("n001");

    auto mock = std::make_unique<MockBackend>(
        [&logs_root](const Node& n, const PromptText&, Context& ctx)
            -> std::expected<LlmResponse, Outcome> {
            const int counter = ctx.current_execution_counter();
            const auto node_dir = logs_root.path /
                std::format("{:03d}-{}", counter, type_safe::get(n.id));
            const auto handoff_subdir = node_dir / "001";
            std::filesystem::create_directories(handoff_subdir);
            std::ofstream{handoff_subdir / "handoff.md"} << "handoff content";
            return LlmResponse{"wrote handoff"};
        });
    MockBackend* mock_ptr = mock.get();

    HandoffAwareBackend backend{std::move(mock), logs_root.path, /*max_handoffs=*/0};
    Context ctx;
    (void)ctx.next_execution_counter();
    auto result = backend.run(node, PromptText{"initial"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    SNITCH_CHECK(mock_ptr->received_prompts.size() == 1u);
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("max handoff") != std::string::npos);
}

SNITCH_TEST_CASE("[handoff_aware] max_handoffs=0 no handoff file returns success -- 7.8-U-002")
{
    TmpDir logs_root{std::filesystem::temp_directory_path() / "att_hab_7_8_002"};
    const Node node = make_node("n002");

    auto mock = std::make_unique<MockBackend>(
        [](const Node&, const PromptText&, Context&)
            -> std::expected<LlmResponse, Outcome> {
            return LlmResponse{"clean result"};
        });
    MockBackend* mock_ptr = mock.get();

    HandoffAwareBackend backend{std::move(mock), logs_root.path, /*max_handoffs=*/0};
    Context ctx;
    (void)ctx.next_execution_counter();
    auto result = backend.run(node, PromptText{"initial"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "clean result");
    SNITCH_CHECK(mock_ptr->received_prompts.size() == 1u);
}
