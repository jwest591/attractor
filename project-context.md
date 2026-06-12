---
project_name: 'attractor'
user_name: 'jon'
date: '2026-06-11'
sections_completed: ['technology_stack', 'language_rules', 'testing_rules', 'quality_rules', 'anti_patterns']
status: 'complete'
rule_count: 32
optimized_for_llm: true
---

# Project Context for AI Agents

_This file contains critical rules and patterns that AI agents must follow when implementing code in this project. Focus on unobvious details that agents might otherwise miss._

---

## Technology Stack & Versions

- **Language:** C++26 (required, extensions off, `-Wall -Wextra -Wpedantic -Werror` always)
- **Build:** CMake 3.30 + Ninja via CMakePresets.json; presets: `debug`, `release`, `asan`, `ubsan`
- **Packages:** vcpkg (`vcpkg.json`) — no manual dependency management
- **type-safe** — strong typedef and constrained types (version from vcpkg)
- **nlohmann-json** — JSON serialization (used project-wide)
- **snitch** — test framework (NOT Catch2/GTest/doctest)
- **cli11** — CLI argument parsing
- **stdexec** — sender/receiver async
- **cpp-httplib** — embedded HTTP server

## Critical Implementation Rules

### Language-Specific Rules

**Strong Type System (type-safe library) — mandatory throughout**
- Every domain string uses a `ts::strong_typedef` wrapper (NodeId, EdgeLabel, ContextKey, etc.) — never pass raw `std::string` where a domain type is expected
- Extract the underlying value with `type_safe::get(x)`, not a cast
- Boolean struct fields use `ts::boolean`, not `bool`
- Constrained numeric types: `MaxRetries` (≥0), `Weight` (≥0), `Port` (1–65535), `TimeoutDuration` (>0 ms) — construct with the raw value and let the assertion fire on violation
- New domain types go in `include/attractor/types.hpp` alongside existing ones, following the same pattern; add `to_json`/`from_json` declarations there and definitions in `src/types.cpp`

**Error Handling**
- Use `std::expected<T, E>` for all fallible operations — no exceptions at API boundaries
- Handler exceptions are caught by `safe_execute()` in the engine and converted to `Outcome::fail(DiagnosticMessage{...})` — handlers may throw, but public APIs must not
- `[[nodiscard]]` is required on every method or function that returns a meaningful value

**Ownership**
- Handlers are owned by `HandlerRegistry` via `unique_ptr<Handler>`
- Polymorphic types expose virtual interfaces — always `virtual ~T() = default`
- Data structs (`Node`, `Edge`, `Graph`, `Outcome`, `CheckpointData`) are plain aggregates — no constructors, no private members

### Testing Rules

**Framework: snitch (not Catch2/GTest)**
- Macros: `SNITCH_TEST_CASE`, `SNITCH_CHECK`, `SNITCH_REQUIRE`, `SNITCH_REQUIRE_FALSE`
- Include snitch via `<snitch/snitch.hpp>`
- Test binary entry point comes from `tests/support/main_stub.cpp` — do not add `main()` to test files

**Test naming convention**
- Format: `"[component] description -- story-id"`
- Example: `"[engine] 3-node linear pipeline returns SUCCESS -- 2.3-I-001"`
- Tag (`[component]`) must match the subsystem under test

**Test support helpers — use these, don't reinvent**
- `attractor::test::parse_ok(dot)` — parses DOT inline and `SNITCH_REQUIRE`s success; call only inside a test case
- `attractor::test::TempLogsDir` — RAII temp directory; `logs.logs_root()` returns `LogsRoot`
- Both live in `tests/support/attractor_test_support.hpp`
- Graph builder helpers in `tests/support/graph_builders.hpp`

**Test file placement**
- Unit tests: `tests/unit/<subsystem>_test.cpp`
- Integration tests: `tests/integration/`
- One test file per header/subsystem

**Using declarations in tests**
- `using namespace attractor;` and `using namespace attractor::test;` are standard in test files

### Code Quality & Style Rules

**Naming conventions**
- Types/classes: `PascalCase` (Node, EdgeLabel, HandlerRegistry)
- Functions/methods: `snake_case` (find_start_node, save_checkpoint)
- Member variables: `m_snake_case` (m_registry, m_data, m_mutex)
- Free constants: `k_snake_case` (k_artifact_file_threshold)
- Enumerators: `snake_case` (StageStatus::success, Severity::error)
- Namespaces: `snake_case` — everything lives in `namespace attractor`; anonymous `namespace {}` for TU-local helpers

**File layout**
- Public headers: `include/attractor/<name>.hpp`
- Implementation: `src/<name>.cpp`
- Handler headers: `include/attractor/handlers/<name>_handler.hpp`
- Backend headers: `include/attractor/backends/<name>_backend.hpp`
- Include guards: `#ifndef ATTRACTOR_<PATH>_HPP` (no `#pragma once`)

**Comments**
- No comments explaining what code does — only why (hidden invariant, workaround, non-obvious constraint)
- No docstrings or multi-line comment blocks

**Compiler compliance**
- All four warning flags always active: `-Wall -Wextra -Wpedantic -Werror`
- `NOLINT` annotations (clang-tidy) used sparingly — always include the rule name and a brief reason

### Critical Don't-Miss Rules

**DOT attribute name vs. internal field name**
- The external DOT attribute is `type` (e.g. `node [type="wait.human"]`)
- The internal C++ struct field is `node_type` (to avoid reserved-word conflicts)
- Never name a new struct field `type`

**`Node.shape` default**
- `NodeShape` defaults to `"box"` — preserve this default in struct construction or the handler registry will misroute nodes

**Context keys**
- Context values must be JSON-serializable; `Context::set()` returns `std::expected<void, std::string>` — always check or propagate the error
- Use dotted-namespace keys matching spec conventions: `context.*`, `graph.*`, `internal.*`, `parallel.*`, `stack.*`, `human.gate.*`

**Outcome construction**
- Use designated initializers: `Outcome{.status = StageStatus::fail, .failure_reason = DiagnosticMessage{"..."}}`
- Use `Outcome::fail(DiagnosticMessage{"..."})` factory for failure-only outcomes
- `context_updates` must be a JSON object (`nlohmann::json::object()`), never null or array

**Checkpoint writes are atomic**
- `save_checkpoint()` writes to a temp file then renames — never write `checkpoint.json` directly

**Thread safety**
- `Context` and `ArtifactStore` are internally synchronized via `std::shared_mutex`; callers must not hold external locks around them
- `Engine::run()` is `const` and stateless — do not add mutable state to `Engine`

**Handler registry default**
- `HandlerRegistry::set_default_handler()` must be called before any node that lacks a registered type resolves — failing to do so is UB (assert fires in `resolve()`)

**`std::expected` propagation**
- Do not `.value()` an expected without checking — use `if (!result) return std::unexpected(result.error());` or structured bindings

---

## Usage Guidelines

**For AI Agents:** Read this file before implementing any code. Follow all rules exactly. When in doubt, prefer the more restrictive interpretation.

**For Humans:** Keep this file lean. Update when the technology stack or core patterns change. Remove rules that become obvious over time.

_Last Updated: 2026-06-11_
