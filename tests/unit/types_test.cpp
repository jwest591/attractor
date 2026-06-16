#include "attractor_test_support.hpp"
#include <attractor/types.hpp>
#include <nlohmann/json.hpp>

// AC4: strong typedefs must not accept direct string assignment
static_assert(!std::is_assignable_v<attractor::NodeId&, std::string>,
              "NodeId must not be directly assignable from std::string");
static_assert(!std::is_assignable_v<attractor::EdgeLabel&, std::string>,
              "EdgeLabel must not be directly assignable from std::string");
static_assert(!std::is_assignable_v<attractor::ArtifactId&, std::string>,
              "ArtifactId must not be directly assignable from std::string");
static_assert(!std::is_assignable_v<attractor::HandlerTypeName&, std::string>,
              "HandlerTypeName must not be directly assignable from std::string");
static_assert(!std::is_assignable_v<attractor::ContextKey&, std::string>,
              "ContextKey must not be directly assignable from std::string");
static_assert(!std::is_assignable_v<attractor::ThreadId&, std::string>,
              "ThreadId must not be directly assignable from std::string");
static_assert(!std::is_assignable_v<attractor::PromptText&, std::string>,
              "PromptText must not be directly assignable from std::string");
static_assert(!std::is_assignable_v<attractor::GoalText&, std::string>,
              "GoalText must not be directly assignable from std::string");
static_assert(!std::is_assignable_v<attractor::LogsRoot&, std::string>,
              "LogsRoot must not be directly assignable from std::string");

// AC1: string strong typedefs -- construction, equality, ordering

SNITCH_TEST_CASE("[types] NodeId equality and ordering")
{
    using namespace attractor;
    NodeId a{"alpha"};
    NodeId b{"beta"};
    NodeId a2{"alpha"};

    SNITCH_REQUIRE(a == a2);
    SNITCH_REQUIRE(a != b);
    SNITCH_REQUIRE(a < b);
    SNITCH_REQUIRE(b > a);
}

SNITCH_TEST_CASE("[types] all 9 string strong typedefs construct and compare")
{
    using namespace attractor;
    SNITCH_CHECK(NodeId{"x"} == NodeId{"x"});
    SNITCH_CHECK(EdgeLabel{"x"} == EdgeLabel{"x"});
    SNITCH_CHECK(ArtifactId{"x"} == ArtifactId{"x"});
    SNITCH_CHECK(HandlerTypeName{"x"} == HandlerTypeName{"x"});
    SNITCH_CHECK(ContextKey{"x"} == ContextKey{"x"});
    SNITCH_CHECK(ThreadId{"x"} == ThreadId{"x"});
    SNITCH_CHECK(PromptText{"x"} == PromptText{"x"});
    SNITCH_CHECK(GoalText{"x"} == GoalText{"x"});
    SNITCH_CHECK(LogsRoot{"x"} == LogsRoot{"x"});
}

// AC2: constrained types -- valid values construct without assertion

SNITCH_TEST_CASE("[types] MaxRetries valid construction")
{
    using namespace attractor;
    SNITCH_CHECK_NOTHROW(MaxRetries{0});
    SNITCH_CHECK_NOTHROW(MaxRetries{10});
    SNITCH_CHECK_NOTHROW(MaxRetries{1000});
}

SNITCH_TEST_CASE("[types] Weight valid construction")
{
    using namespace attractor;
    SNITCH_CHECK_NOTHROW(Weight{0});
    SNITCH_CHECK_NOTHROW(Weight{100});
}

SNITCH_TEST_CASE("[types] Port valid construction")
{
    using namespace attractor;
    SNITCH_CHECK_NOTHROW(Port{1});
    SNITCH_CHECK_NOTHROW(Port{443});
    SNITCH_CHECK_NOTHROW(Port{8080});
    SNITCH_CHECK_NOTHROW(Port{65535});
}

SNITCH_TEST_CASE("[types] TimeoutDuration valid construction")
{
    using namespace attractor;
    using namespace std::chrono;
    SNITCH_CHECK_NOTHROW(TimeoutDuration{milliseconds{1}});
    SNITCH_CHECK_NOTHROW(TimeoutDuration{milliseconds{5000}});
    SNITCH_CHECK_NOTHROW(TimeoutDuration{milliseconds{900000}});
}

SNITCH_TEST_CASE("[types] port_constraint operator() boundaries")
{
    constexpr attractor::port_constraint c;
    SNITCH_CHECK(c(1));
    SNITCH_CHECK(c(65535));
    SNITCH_CHECK(!c(0));
    SNITCH_CHECK(!c(65536));
    SNITCH_CHECK(!c(-1));
}

SNITCH_TEST_CASE("[types] positive_duration_constraint operator() boundaries")
{
    using namespace std::chrono;
    constexpr attractor::positive_duration_constraint c;
    SNITCH_CHECK(c(milliseconds{1}));
    SNITCH_CHECK(!c(milliseconds{0}));
    SNITCH_CHECK(!c(milliseconds{-1}));
}

SNITCH_TEST_CASE("[types] max_retries_constraint operator() boundaries")
{
    constexpr attractor::max_retries_constraint c;
    SNITCH_CHECK(c(0));
    SNITCH_CHECK(c(1));
    SNITCH_CHECK(!c(-1));
}

SNITCH_TEST_CASE("[types] weight_constraint operator() boundaries")
{
    constexpr attractor::weight_constraint c;
    SNITCH_CHECK(c(0));
    SNITCH_CHECK(c(1));
    SNITCH_CHECK(!c(-1));
}

// AC3: JSON round-trips

SNITCH_TEST_CASE("[types] NodeId JSON round-trip")
{
    using namespace attractor;
    NodeId original{"node_a"};
    nlohmann::json j;
    to_json(j, original);
    SNITCH_REQUIRE(j.get<std::string>() == "node_a");
    NodeId restored{""};
    from_json(j, restored);
    SNITCH_REQUIRE(original == restored);
}

SNITCH_TEST_CASE("[types] EdgeLabel JSON round-trip")
{
    using namespace attractor;
    EdgeLabel original{"my_edge"};
    nlohmann::json j;
    to_json(j, original);
    SNITCH_REQUIRE(j.get<std::string>() == "my_edge");
    EdgeLabel restored{""};
    from_json(j, restored);
    SNITCH_REQUIRE(original == restored);
}

SNITCH_TEST_CASE("[types] MaxRetries JSON round-trip")
{
    using namespace attractor;
    MaxRetries original{3};
    nlohmann::json j;
    to_json(j, original);
    SNITCH_REQUIRE(j.get<int>() == 3);
    MaxRetries restored{0};
    from_json(j, restored);
    SNITCH_REQUIRE(restored.get_value() == 3);
}

SNITCH_TEST_CASE("[types] Weight JSON round-trip")
{
    using namespace attractor;
    Weight original{42};
    nlohmann::json j;
    to_json(j, original);
    SNITCH_REQUIRE(j.get<int>() == 42);
    Weight restored{0};
    from_json(j, restored);
    SNITCH_REQUIRE(restored.get_value() == 42);
}

SNITCH_TEST_CASE("[types] Port JSON round-trip")
{
    using namespace attractor;
    Port original{8080};
    nlohmann::json j;
    to_json(j, original);
    SNITCH_REQUIRE(j.get<int>() == 8080);
    Port restored{1};
    from_json(j, restored);
    SNITCH_REQUIRE(restored.get_value() == 8080);
}

SNITCH_TEST_CASE("[types] TimeoutDuration JSON round-trip")
{
    using namespace attractor;
    using namespace std::chrono;
    TimeoutDuration original{milliseconds{5000}};
    nlohmann::json j;
    to_json(j, original);
    SNITCH_REQUIRE(j.get<int64_t>() == 5000);
    TimeoutDuration restored{milliseconds{1}};
    from_json(j, restored);
    SNITCH_REQUIRE(restored.get_value() == milliseconds{5000});
}

SNITCH_TEST_CASE("[types] StageStatus JSON round-trip")
{
    using namespace attractor;
    nlohmann::json j = StageStatus::partial_success;
    SNITCH_REQUIRE(j.get<std::string>() == "partial_success");
    SNITCH_REQUIRE(j.get<StageStatus>() == StageStatus::partial_success);
}

SNITCH_TEST_CASE("[types] all StageStatus values serialize correctly")
{
    using namespace attractor;
    SNITCH_CHECK(nlohmann::json(StageStatus::success).get<std::string>() == "success");
    SNITCH_CHECK(nlohmann::json(StageStatus::partial_success).get<std::string>() == "partial_success");
    SNITCH_CHECK(nlohmann::json(StageStatus::fail).get<std::string>() == "fail");
    SNITCH_CHECK(nlohmann::json(StageStatus::retry).get<std::string>() == "retry");
    SNITCH_CHECK(nlohmann::json(StageStatus::skipped).get<std::string>() == "skipped");
}

SNITCH_TEST_CASE("[types] Severity JSON round-trip")
{
    using namespace attractor;
    SNITCH_CHECK(nlohmann::json(Severity::error).get<std::string>() == "error");
    SNITCH_CHECK(nlohmann::json(Severity::warning).get<std::string>() == "warning");
    SNITCH_CHECK(nlohmann::json(Severity::info).get<std::string>() == "info");
}

SNITCH_TEST_CASE("[types] QuestionType JSON round-trip")
{
    using namespace attractor;
    SNITCH_CHECK(nlohmann::json(QuestionType::yes_no).get<std::string>() == "yes_no");
    SNITCH_CHECK(nlohmann::json(QuestionType::multiple_choice).get<std::string>() == "multiple_choice");
    SNITCH_CHECK(nlohmann::json(QuestionType::freeform).get<std::string>() == "freeform");
    SNITCH_CHECK(nlohmann::json(QuestionType::confirmation).get<std::string>() == "confirmation");
}

SNITCH_TEST_CASE("[types] FidelityMode JSON round-trip")
{
    using namespace attractor;
    nlohmann::json j = FidelityMode::summary_high;
    SNITCH_REQUIRE(j.get<std::string>() == "summary:high");
    SNITCH_REQUIRE(j.get<FidelityMode>() == FidelityMode::summary_high);
}

SNITCH_TEST_CASE("[types] all FidelityMode values serialize correctly")
{
    using namespace attractor;
    SNITCH_CHECK(nlohmann::json(FidelityMode::full).get<std::string>() == "full");
    SNITCH_CHECK(nlohmann::json(FidelityMode::truncate).get<std::string>() == "truncate");
    SNITCH_CHECK(nlohmann::json(FidelityMode::compact).get<std::string>() == "compact");
    SNITCH_CHECK(nlohmann::json(FidelityMode::summary_low).get<std::string>() == "summary:low");
    SNITCH_CHECK(nlohmann::json(FidelityMode::summary_medium).get<std::string>() == "summary:medium");
    SNITCH_CHECK(nlohmann::json(FidelityMode::summary_high).get<std::string>() == "summary:high");
}

// AC5 (enum completeness): all values exist and compile

SNITCH_TEST_CASE("[types] all StageStatus values exist")
{
    using namespace attractor;
    [[maybe_unused]] auto s1 = StageStatus::success;
    [[maybe_unused]] auto s2 = StageStatus::partial_success;
    [[maybe_unused]] auto s3 = StageStatus::fail;
    [[maybe_unused]] auto s4 = StageStatus::retry;
    [[maybe_unused]] auto s5 = StageStatus::skipped;
}

SNITCH_TEST_CASE("[types] all Severity values exist")
{
    using namespace attractor;
    [[maybe_unused]] auto v1 = Severity::error;
    [[maybe_unused]] auto v2 = Severity::warning;
    [[maybe_unused]] auto v3 = Severity::info;
}

SNITCH_TEST_CASE("[types] all QuestionType values exist")
{
    using namespace attractor;
    [[maybe_unused]] auto q1 = QuestionType::yes_no;
    [[maybe_unused]] auto q2 = QuestionType::multiple_choice;
    [[maybe_unused]] auto q3 = QuestionType::freeform;
    [[maybe_unused]] auto q4 = QuestionType::confirmation;
}

SNITCH_TEST_CASE("[types] all FidelityMode values exist")
{
    using namespace attractor;
    [[maybe_unused]] auto f1 = FidelityMode::full;
    [[maybe_unused]] auto f2 = FidelityMode::truncate;
    [[maybe_unused]] auto f3 = FidelityMode::compact;
    [[maybe_unused]] auto f4 = FidelityMode::summary_low;
    [[maybe_unused]] auto f5 = FidelityMode::summary_medium;
    [[maybe_unused]] auto f6 = FidelityMode::summary_high;
}

// from_json error paths: unknown enum strings throw

SNITCH_TEST_CASE("[types] StageStatus from_json unknown string throws")
{
    using namespace attractor;
    nlohmann::json j = "bogus";
    StageStatus v{};
    SNITCH_CHECK_THROWS_AS(from_json(j, v), nlohmann::json::exception);
}

SNITCH_TEST_CASE("[types] Severity from_json unknown string throws")
{
    using namespace attractor;
    nlohmann::json j = "bogus";
    Severity v{};
    SNITCH_CHECK_THROWS_AS(from_json(j, v), nlohmann::json::exception);
}

SNITCH_TEST_CASE("[types] QuestionType from_json unknown string throws")
{
    using namespace attractor;
    nlohmann::json j = "bogus";
    QuestionType v{};
    SNITCH_CHECK_THROWS_AS(from_json(j, v), nlohmann::json::exception);
}

SNITCH_TEST_CASE("[types] FidelityMode from_json unknown string throws")
{
    using namespace attractor;
    nlohmann::json j = "bogus";
    FidelityMode v{};
    SNITCH_CHECK_THROWS_AS(from_json(j, v), nlohmann::json::exception);
}

// from_json error paths: constrained type violations throw

SNITCH_TEST_CASE("[types] MaxRetries from_json negative value throws")
{
    using namespace attractor;
    nlohmann::json j = -1;
    MaxRetries v{0};
    SNITCH_CHECK_THROWS_AS(from_json(j, v), nlohmann::json::exception);
}

SNITCH_TEST_CASE("[types] Weight from_json negative value throws")
{
    using namespace attractor;
    nlohmann::json j = -1;
    Weight v{0};
    SNITCH_CHECK_THROWS_AS(from_json(j, v), nlohmann::json::exception);
}

SNITCH_TEST_CASE("[types] Port from_json out-of-range value throws")
{
    using namespace attractor;
    nlohmann::json j = 0;
    Port v{1};
    SNITCH_CHECK_THROWS_AS(from_json(j, v), nlohmann::json::exception);
}

SNITCH_TEST_CASE("[types] TimeoutDuration from_json zero value throws")
{
    using namespace attractor;
    using namespace std::chrono;
    nlohmann::json j = int64_t{0};
    TimeoutDuration v{milliseconds{1}};
    SNITCH_CHECK_THROWS_AS(from_json(j, v), nlohmann::json::exception);
}
