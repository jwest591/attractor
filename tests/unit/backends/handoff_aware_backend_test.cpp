#include "backend_utils.hpp"
#include "handoff_aware_backend.hpp"

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/types.hpp>
#include <snitch/snitch.hpp>
#include <type_safe/strong_typedef.hpp>

#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace attractor;

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
struct TmpFile {
    explicit TmpFile(std::filesystem::path p) : path{std::move(p)} {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    TmpFile(const TmpFile&) = delete;
    TmpFile& operator=(const TmpFile&) = delete;
    ~TmpFile() noexcept { std::error_code ec; std::filesystem::remove(path, ec); }
    std::filesystem::path path;
};

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class MockBackend : public CodergenBackend {
  public:
    using RunFn = std::function<std::expected<LlmResponse, Outcome>(const std::string& prompt)>;

    explicit MockBackend(RunFn fn) : m_fn{std::move(fn)} {}

    mutable std::vector<std::string> received_prompts;

    [[nodiscard]] auto run(const Node& /*node*/,
                           const PromptText& prompt,
                           Context& /*ctx*/) const
        -> std::expected<LlmResponse, Outcome> override
    {
        received_prompts.push_back(type_safe::get(prompt));
        return m_fn(type_safe::get(prompt));
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

std::filesystem::path handoff_path_for(const std::string& id_val)
{
    const Node node = make_node(id_val);
    return std::filesystem::path{compute_handoff_path(derive_session_name(node)).value()};
}

}  // namespace

SNITCH_TEST_CASE("[handoff_aware_backend] clean completion returns response unchanged -- 5.5-U-001")
{
    const Node node = make_node("n001");
    TmpFile guard{handoff_path_for("n001")};

    auto mock = std::make_shared<MockBackend>(
        [](const std::string& /*prompt*/) -> std::expected<LlmResponse, Outcome> {
            return LlmResponse{"ok"};
        });

    HandoffAwareBackend backend{mock};
    Context ctx{};
    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "ok");
    SNITCH_CHECK(mock->received_prompts.size() == 1u);
}

SNITCH_TEST_CASE("[handoff_aware_backend] single handoff then success returns final response -- 5.5-U-002")
{
    const Node node = make_node("n002");
    const std::filesystem::path hpath = handoff_path_for("n002");
    TmpFile guard{hpath};

    int call = 0;
    auto mock = std::make_shared<MockBackend>(
        [&](const std::string& /*prompt*/) -> std::expected<LlmResponse, Outcome> {
            // Real backends delete the stale handoff file before running
            { std::error_code ec; std::filesystem::remove(hpath, ec); }
            ++call;
            if (call == 1) {
                std::filesystem::create_directories(hpath.parent_path());
                std::ofstream f{hpath};
                f << "handoff summary";
                return LlmResponse{"wrote handoff"};
            }
            return LlmResponse{"final"};
        });

    HandoffAwareBackend backend{mock};
    Context ctx{};
    auto result = backend.run(node, PromptText{"initial prompt"}, ctx);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(type_safe::get(*result) == "final");
    SNITCH_REQUIRE(mock->received_prompts.size() == 2u);
    SNITCH_CHECK(mock->received_prompts[1] == "handoff summary");
}

SNITCH_TEST_CASE("[handoff_aware_backend] max handoffs exhausted returns Outcome::fail -- 5.5-U-003")
{
    const Node node = make_node("n003");
    const std::filesystem::path hpath = handoff_path_for("n003");
    TmpFile guard{hpath};

    auto mock = std::make_shared<MockBackend>(
        [&](const std::string& /*prompt*/) -> std::expected<LlmResponse, Outcome> {
            // Real backends delete the stale handoff file before running
            { std::error_code ec; std::filesystem::remove(hpath, ec); }
            std::filesystem::create_directories(hpath.parent_path());
            std::ofstream f{hpath};
            f << "handoff content";
            return LlmResponse{"wrote handoff"};
        });

    HandoffAwareBackend backend{mock, /*max_handoffs=*/2};
    Context ctx{};
    auto result = backend.run(node, PromptText{"initial"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    SNITCH_CHECK(mock->received_prompts.size() == 3u);  // attempt 0, 1, 2
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("max handoffs (2)") != std::string::npos);
    SNITCH_CHECK(reason.find(hpath.string()) != std::string::npos);
}

SNITCH_TEST_CASE("[handoff_aware_backend] inner error without handoff file is propagated immediately -- 5.5-U-004")
{
    const Node node = make_node("n004");
    TmpFile guard{handoff_path_for("n004")};

    auto mock = std::make_shared<MockBackend>(
        [](const std::string& /*prompt*/) -> std::expected<LlmResponse, Outcome> {
            return std::unexpected(Outcome::fail(DiagnosticMessage{"backend exploded"}));
        });

    HandoffAwareBackend backend{mock};
    Context ctx{};
    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    SNITCH_CHECK(mock->received_prompts.size() == 1u);
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("backend exploded") != std::string::npos);
}

SNITCH_TEST_CASE("[handoff_aware_backend] empty handoff file returns Outcome::fail -- 5.5-U-005")
{
    const Node node = make_node("n005");
    const std::filesystem::path hpath = handoff_path_for("n005");
    TmpFile guard{hpath};

    auto mock = std::make_shared<MockBackend>(
        [&](const std::string& /*prompt*/) -> std::expected<LlmResponse, Outcome> {
            // Real backends delete the stale handoff file before running
            { std::error_code ec; std::filesystem::remove(hpath, ec); }
            std::filesystem::create_directories(hpath.parent_path());
            std::ofstream f{hpath};  // empty file -- no content written
            return LlmResponse{"wrote empty handoff"};
        });

    HandoffAwareBackend backend{mock};
    Context ctx{};
    auto result = backend.run(node, PromptText{"hello"}, ctx);

    SNITCH_REQUIRE_FALSE(result.has_value());
    const auto& reason = type_safe::get(result.error().failure_reason);
    SNITCH_CHECK(reason.find("handoff file is empty") != std::string::npos);
}
