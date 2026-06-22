#include "attractor_test_support.hpp"

#include <attractor/artifact_store.hpp>
#include <attractor/types.hpp>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

using namespace attractor;
using namespace attractor::test;

SNITCH_TEST_CASE("[artifact_store] store and retrieve small artifact in memory -- 2.7-AS-001")
{
    ArtifactStore store;  // no logs_root -- in-memory only

    const nlohmann::json data{{"key", "value"}, {"count", 42}};
    auto result = store.store(ArtifactId{"art1"}, "My Artifact", data);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(result->id == ArtifactId{"art1"});
    SNITCH_CHECK(result->name == "My Artifact");
    SNITCH_CHECK(!result->is_file_backed);

    auto retrieved = store.retrieve(ArtifactId{"art1"});
    SNITCH_REQUIRE(retrieved.has_value());
    SNITCH_CHECK((*retrieved)["key"].get<std::string>() == "value");
    SNITCH_CHECK((*retrieved)["count"].get<int>() == 42);
}

SNITCH_TEST_CASE("[artifact_store] large artifact written to disk at correct path -- 2.7-AS-002")
{
    TempLogsDir logs;
    ArtifactStore store{logs.logs_root()};

    // Build a JSON payload >= 100KB
    std::string big_str(k_artifact_file_threshold, 'x');
    const nlohmann::json big_data{{"payload", std::move(big_str)}};

    auto result = store.store(ArtifactId{"big-art"}, "Big Artifact", big_data);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(result->is_file_backed);
    SNITCH_CHECK(result->size_bytes >= k_artifact_file_threshold);

    // File must exist at expected path
    const auto expected_path = logs.path() / "artifacts" / "big-art.json";
    SNITCH_CHECK(std::filesystem::exists(expected_path));

    // round-trip via retrieve
    auto retrieved = store.retrieve(ArtifactId{"big-art"});
    SNITCH_REQUIRE(retrieved.has_value());
    SNITCH_CHECK((*retrieved)["payload"].get<std::string>().size() >= k_artifact_file_threshold);
}

SNITCH_TEST_CASE("[artifact_store] has returns true for stored false for absent -- 2.7-AS-003")
{
    ArtifactStore store;

    SNITCH_CHECK(!store.has(ArtifactId{"x"}));

    SNITCH_REQUIRE(store.store(ArtifactId{"x"}, "X", nlohmann::json{1}).has_value());
    SNITCH_CHECK(store.has(ArtifactId{"x"}));
    SNITCH_CHECK(!store.has(ArtifactId{"y"}));
}

SNITCH_TEST_CASE("[artifact_store] list returns ArtifactInfo for all stored artifacts -- 2.7-AS-004")
{
    ArtifactStore store;
    SNITCH_REQUIRE(store.store(ArtifactId{"a"}, "Alpha", nlohmann::json{"alpha"}).has_value());
    SNITCH_REQUIRE(store.store(ArtifactId{"b"}, "Beta", nlohmann::json{99}).has_value());

    const auto infos = store.list();
    SNITCH_REQUIRE(infos.size() == 2);

    // ArtifactStore uses std::map<ArtifactId> -- entries are in sorted order
    SNITCH_CHECK(infos[0].id == ArtifactId{"a"});
    SNITCH_CHECK(infos[0].name == "Alpha");
    SNITCH_CHECK(infos[1].id == ArtifactId{"b"});
    SNITCH_CHECK(infos[1].name == "Beta");
}

SNITCH_TEST_CASE("[artifact_store] remove deletes artifact -- 2.7-AS-005")
{
    ArtifactStore store;
    SNITCH_REQUIRE(store.store(ArtifactId{"del"}, "Delete Me", nlohmann::json{true}).has_value());
    SNITCH_REQUIRE(store.has(ArtifactId{"del"}));

    store.remove(ArtifactId{"del"});
    SNITCH_CHECK(!store.has(ArtifactId{"del"}));
    SNITCH_CHECK(!store.retrieve(ArtifactId{"del"}).has_value());
}

SNITCH_TEST_CASE("[artifact_store] retrieve returns error for unknown ArtifactId -- 2.7-AS-006")
{
    ArtifactStore store;
    auto result = store.retrieve(ArtifactId{"nonexistent"});
    SNITCH_CHECK(!result.has_value());
}

SNITCH_TEST_CASE("[artifact_store] clear removes all artifacts -- 2.7-AS-007")
{
    ArtifactStore store;
    SNITCH_REQUIRE(store.store(ArtifactId{"p"}, "P", nlohmann::json{1}).has_value());
    SNITCH_REQUIRE(store.store(ArtifactId{"q"}, "Q", nlohmann::json{2}).has_value());
    SNITCH_REQUIRE(store.list().size() == 2);

    store.clear();
    SNITCH_CHECK(store.list().empty());
    SNITCH_CHECK(!store.has(ArtifactId{"p"}));
}

SNITCH_TEST_CASE("[artifact_store] below-threshold artifact with no logs_root stays in memory -- 2.7-AS-008")
{
    // Even if size is close to threshold, without logs_root all artifacts are in-memory.
    ArtifactStore store;  // no logs_root

    const nlohmann::json data{{"blob", std::string(k_artifact_file_threshold - 1, 'z')}};
    auto result = store.store(ArtifactId{"mem-art"}, "In-Memory", data);

    SNITCH_REQUIRE(result.has_value());
    SNITCH_CHECK(!result->is_file_backed);
}

SNITCH_TEST_CASE("[artifact_store] remove deletes backing file from disk -- 7.9-AS-001")
{
    TempLogsDir logs;
    ArtifactStore store{logs.logs_root()};

    const nlohmann::json big_data{{"payload", std::string(k_artifact_file_threshold, 'x')}};
    SNITCH_REQUIRE(store.store(ArtifactId{"deleteme"}, "Delete Me", big_data).has_value());

    const auto backing_file = logs.path() / "artifacts" / "deleteme.json";
    SNITCH_REQUIRE(std::filesystem::exists(backing_file));

    store.remove(ArtifactId{"deleteme"});

    SNITCH_CHECK(!store.has(ArtifactId{"deleteme"}));
    SNITCH_CHECK(!std::filesystem::exists(backing_file));
}

SNITCH_TEST_CASE("[artifact_store] clear deletes all backing files from disk -- 7.9-AS-002")
{
    TempLogsDir logs;
    ArtifactStore store{logs.logs_root()};

    const nlohmann::json big_data{{"payload", std::string(k_artifact_file_threshold, 'x')}};
    SNITCH_REQUIRE(store.store(ArtifactId{"fileA"}, "File A", big_data).has_value());
    SNITCH_REQUIRE(store.store(ArtifactId{"fileB"}, "File B", big_data).has_value());

    const auto pathA = logs.path() / "artifacts" / "fileA.json";
    const auto pathB = logs.path() / "artifacts" / "fileB.json";
    SNITCH_REQUIRE(std::filesystem::exists(pathA));
    SNITCH_REQUIRE(std::filesystem::exists(pathB));

    store.clear();

    SNITCH_CHECK(store.list().empty());
    SNITCH_CHECK(!std::filesystem::exists(pathA));
    SNITCH_CHECK(!std::filesystem::exists(pathB));
}

SNITCH_TEST_CASE("[artifact_store] overwriting file-backed entry with in-memory entry deletes old backing file -- 7.9-AS-003")
{
    TempLogsDir logs;
    ArtifactStore store{logs.logs_root()};

    const nlohmann::json big_data{{"payload", std::string(k_artifact_file_threshold, 'y')}};
    auto first = store.store(ArtifactId{"myart"}, "My Art", big_data);
    SNITCH_REQUIRE(first.has_value());
    SNITCH_CHECK(first->is_file_backed);

    const auto backing_file = logs.path() / "artifacts" / "myart.json";
    SNITCH_REQUIRE(std::filesystem::exists(backing_file));

    auto second = store.store(ArtifactId{"myart"}, "My Art (small)", nlohmann::json{{"tiny", true}});
    SNITCH_REQUIRE(second.has_value());
    SNITCH_CHECK(!second->is_file_backed);

    SNITCH_CHECK(!std::filesystem::exists(backing_file));

    auto retrieved = store.retrieve(ArtifactId{"myart"});
    SNITCH_REQUIRE(retrieved.has_value());
    SNITCH_CHECK((*retrieved)["tiny"].get<bool>() == true);
}

SNITCH_TEST_CASE("[artifact_store] store cleans up stale file on open failure -- 7.9-AS-004")
{
    TempLogsDir logs;
    ArtifactStore store{logs.logs_root()};

    // Pre-create a stale file at the artifact path and make it read-only so
    // std::ofstream fails to open it, exercising the open-failure cleanup path.
    const auto artifacts_dir = logs.path() / "artifacts";
    std::filesystem::create_directories(artifacts_dir);
    const auto stale_file = artifacts_dir / "stale.json";
    {
        std::ofstream f{stale_file};
        f << "stale";
    }
    std::filesystem::permissions(
        stale_file,
        std::filesystem::perms::owner_read | std::filesystem::perms::group_read
            | std::filesystem::perms::others_read);

    const nlohmann::json big_data{{"payload", std::string(k_artifact_file_threshold, 'z')}};
    auto result = store.store(ArtifactId{"stale"}, "Stale", big_data);

    SNITCH_CHECK(!result.has_value());
    SNITCH_CHECK(!std::filesystem::exists(stale_file));
}

SNITCH_TEST_CASE("[artifact_store] store rejects ArtifactId containing path separators or dot-dot -- 7.9-AS-005")
{
    TempLogsDir logs;
    ArtifactStore store{logs.logs_root()};

    auto r1 = store.store(ArtifactId{"sub/dir"}, "Bad", nlohmann::json{1});
    SNITCH_CHECK(!r1.has_value());

    auto r2 = store.store(ArtifactId{"sub\\dir"}, "Bad", nlohmann::json{1});
    SNITCH_CHECK(!r2.has_value());

    auto r3 = store.store(ArtifactId{"../evil"}, "Bad", nlohmann::json{1});
    SNITCH_CHECK(!r3.has_value());

    auto r4 = store.store(ArtifactId{"foo..bar"}, "Bad", nlohmann::json{1});
    SNITCH_CHECK(!r4.has_value());

    // Single dot is valid — only the ".." substring is rejected
    auto r5 = store.store(ArtifactId{"foo.bar"}, "Good", nlohmann::json{1});
    SNITCH_CHECK(r5.has_value());
}

SNITCH_TEST_CASE("[artifact_store] store rejects empty ArtifactId -- 7.9-AS-006")
{
    TempLogsDir logs;
    ArtifactStore store{logs.logs_root()};

    auto result = store.store(ArtifactId{""}, "Hidden", nlohmann::json{true});
    SNITCH_CHECK(!result.has_value());
}
