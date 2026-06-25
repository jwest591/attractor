#include "attractor_test_support.hpp"

#include <attractor/context.hpp>
#include <attractor/types.hpp>
#include <limits>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

using namespace attractor;
using namespace attractor::test;

SNITCH_TEST_CASE("[context] set and get round-trips value -- 2.6-C-001")
{
    Context ctx;
    auto result = ctx.set(ContextKey{"mykey"}, nlohmann::json("hello"));
    SNITCH_REQUIRE(result.has_value());

    const auto val = ctx.get(ContextKey{"mykey"});
    SNITCH_CHECK(val.is_string());
    SNITCH_CHECK(val.get<std::string>() == "hello");
}

SNITCH_TEST_CASE("[context] get returns null json for missing key -- 2.6-C-002")
{
    Context ctx;
    const auto val = ctx.get(ContextKey{"nonexistent"});
    SNITCH_CHECK(val.is_null());
}

SNITCH_TEST_CASE("[context] set overwrites existing key -- 2.6-C-003")
{
    Context ctx;
    (void)ctx.set(ContextKey{"k"}, nlohmann::json(1));
    (void)ctx.set(ContextKey{"k"}, nlohmann::json(2));
    SNITCH_CHECK(ctx.get(ContextKey{"k"}).get<int>() == 2);
}

SNITCH_TEST_CASE("[context] merge_updates sets all keys from json object -- 2.6-C-004")
{
    Context ctx;
    nlohmann::json updates{{"a", 1}, {"b", "hello"}, {"c", true}};
    ctx.merge_updates(updates);

    SNITCH_CHECK(ctx.get(ContextKey{"a"}).get<int>() == 1);
    SNITCH_CHECK(ctx.get(ContextKey{"b"}).get<std::string>() == "hello");
    SNITCH_CHECK(ctx.get(ContextKey{"c"}).get<bool>() == true);
}

SNITCH_TEST_CASE("[context] snapshot returns all KV pairs -- 2.6-C-005")
{
    Context ctx;
    (void)ctx.set(ContextKey{"x"}, nlohmann::json(42));
    (void)ctx.set(ContextKey{"y"}, nlohmann::json("world"));

    const auto snap = ctx.snapshot();
    SNITCH_REQUIRE(snap.is_object());
    SNITCH_CHECK(snap.contains("x"));
    SNITCH_CHECK(snap["x"].get<int>() == 42);
    SNITCH_CHECK(snap.contains("y"));
    SNITCH_CHECK(snap["y"].get<std::string>() == "world");
}

SNITCH_TEST_CASE("[context] set returns error for NaN float (NFR2) -- 2.6-C-006")
{
    Context ctx;
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    auto result = ctx.set(ContextKey{"bad"}, nlohmann::json(nan_val));

    SNITCH_CHECK(!result.has_value());
    // Key must not be stored
    SNITCH_CHECK(ctx.get(ContextKey{"bad"}).is_null());
}

SNITCH_TEST_CASE("[context] set returns error for infinite float (NFR2) -- 2.6-C-007")
{
    Context ctx;
    const double inf_val = std::numeric_limits<double>::infinity();
    auto result = ctx.set(ContextKey{"bad"}, nlohmann::json(inf_val));

    SNITCH_CHECK(!result.has_value());
    SNITCH_CHECK(ctx.get(ContextKey{"bad"}).is_null());
}

SNITCH_TEST_CASE("[context] next_execution_counter pre-increments from zero -- 7.19-C-001")
{
    Context ctx;
    SNITCH_CHECK(ctx.next_execution_counter() == 1);
    SNITCH_CHECK(ctx.next_execution_counter() == 2);
    SNITCH_CHECK(ctx.next_execution_counter() == 3);
}

SNITCH_TEST_CASE("[context] current_execution_counter reads without incrementing -- 7.19-C-002")
{
    Context ctx;
    SNITCH_CHECK(ctx.current_execution_counter() == 0);
    SNITCH_CHECK(ctx.next_execution_counter() == 1);
    SNITCH_CHECK(ctx.current_execution_counter() == 1);
    SNITCH_CHECK(ctx.current_execution_counter() == 1);
}

SNITCH_TEST_CASE("[context] set_execution_counter establishes counter for resume -- 7.20-C-001")
{
    Context ctx;
    ctx.set_execution_counter(5);
    SNITCH_CHECK(ctx.current_execution_counter() == 5);
    SNITCH_CHECK(ctx.next_execution_counter() == 6);
    SNITCH_CHECK(ctx.current_execution_counter() == 6);
}

SNITCH_TEST_CASE("[context] concurrent set and get do not data-race -- 2.6-C-008")
{
    // Two threads: one writes, one reads; no undefined behavior.
    // Run under ThreadSanitizer (ubsan/asan CI matrix) to verify.
    Context ctx;
    (void)ctx.set(ContextKey{"counter"}, nlohmann::json(0));

    std::vector<std::thread> threads;
    threads.reserve(4);

    for (int i = 0; i < 2; ++i) {
        threads.emplace_back([&ctx, i]() {
            for (int j = 0; j < 50; ++j) {
                (void)ctx.set(ContextKey{"counter"}, nlohmann::json(i * 50 + j));
            }
        });
    }
    for (int i = 0; i < 2; ++i) {
        threads.emplace_back([&ctx]() {
            for (int j = 0; j < 50; ++j) {
                (void)ctx.get(ContextKey{"counter"});
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
    // If we reach here without a crash/sanitizer error, the test passes.
    SNITCH_CHECK(true);
}
