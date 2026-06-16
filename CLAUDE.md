# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Essential reading

`project-context.md` contains the authoritative rule set for this codebase (types, error handling, naming, testing, anti-patterns). Read it before writing any code.

## Build and test

All commands run from the project root (`polestar/attractor/`).

```sh
# Configure + build (debug is the everyday preset)
cmake --preset debug
cmake --build build/debug

# Run all tests
ctest --preset debug --output-on-failure

# Run only one test binary
ctest -R unit_tests  --test-dir build/debug
ctest -R cli_tests   --test-dir build/debug

# Filter by snitch tag (faster than ctest for iteration)
./build/debug/tests/attractor_unit_tests [dot_parser]
./build/debug/tests/attractor_unit_tests [engine]
./build/debug/tests/attractor_unit_tests --list-tests

# Sanitizer presets
cmake --preset asan  && cmake --build build/asan  && ctest --preset asan
cmake --preset ubsan && cmake --build build/ubsan && ctest --preset ubsan

# Format check / apply
clang-format --style=file --dry-run -Werror <file>
clang-format --style=file -i <file>

# Slow test (rate-limit retry, ~30 s) — excluded by default
cmake --preset debug -DATTRACTOR_ENABLE_SLOW_TESTS=ON
cmake --build build/debug --target attractor_cli_tests
```

## Architecture

Attractor is a DOT-driven AI pipeline executor. Users write `.dot` files that describe directed graphs of processing stages; the engine traverses those graphs, dispatching each node to a registered handler.

### Layers

```
.dot file
   ↓ dot_parser (include/attractor/dot_parser.hpp)
Graph  (nodes + edges, all fields are strong typedefs)
   ↓ Engine::run()  (include/attractor/engine.hpp)
HandlerRegistry::resolve(node) → Handler
   ↓ Handler::execute() → Outcome
Context  (thread-safe KV store, include/attractor/context.hpp)
```

**Core library** (`src/`, `include/attractor/`): everything except the CLI binaries.

**Built-in handlers** (`src/handlers/`, `include/attractor/handlers/`):

| Handler | Activated by |
|---|---|
| `StartHandler` / `ExitHandler` | `start` / `exit` node type |
| `CodergenHandler` | `codergen` type; delegates to a `CodergenBackend` |
| `ConditionalHandler` | `conditional` type; evaluates `ConditionExpr` on `Context` |
| `WaitForHumanHandler` | `wait.human` type; goes through an `Interviewer` |
| `ParallelHandler` / `FanInHandler` | fan-out / fan-in for parallel branches |
| `ToolHandler` | `tool` type; spawns a shell command |
| `ManagerLoopHandler` | `manager.loop` type; runs a sub-cycle |

**CodergenBackend implementations** (CLI-only, `cli/backends/`):

- `NoOpBackend` — deterministic stub (library default)
- `ClaudeCodeHeadlessBackend` — pipes to `claude` CLI via `bash -c "... | ctx-usage.sh parse-stream"`; requires `jq` on PATH
- `ClaudeCodeTmuxBackend` — drives an interactive `claude` session inside tmux
- `HandoffAwareBackend` — wrapper that reads/writes a handoff markdown file around the inner backend

**Interviewer implementations** (`src/interviewers/`): `ConsoleInterviewer`, `CallbackInterviewer`, `QueueInterviewer`, `RecordingInterviewer`, `AutoApproveInterviewer`.

**CLI scripts** (`cli/scripts/`): `ctx-usage.sh` (parse-stream + update), `context-ceiling.sh` (fires at 85% context), `att-session-start.sh`, `att-status-line.sh`. All require `jq`.

### Data flow details

- `Engine::run()` is `const` and stateless — all mutable state lives in `Context` and the filesystem logs.
- Edge selection: after a handler returns `Outcome`, the engine picks the outgoing edge by matching `preferred_label`, then `suggested_next_ids`, then `ConditionExpr` evaluation, then highest `Weight`.
- Checkpoint/resume: `save_checkpoint()` writes atomically (temp + rename) to `logs_root/checkpoint.json`; `RunConfig::resume = true` reloads it.
- Events (`StageStarted`, `StageCompleted`) are dispatched through the `EventObserver` callback injected at `Engine` construction.

## Test structure

Three CTest-registered binaries:

| Binary | Sources | What it covers |
|---|---|---|
| `attractor_unit_tests` | `tests/unit/` | core library, all handlers, interviewers |
| `attractor_integration_tests` | `tests/integration/` | smoke test, parity matrix |
| `attractor_cli_tests` | `tests/unit/backends/` | headless, tmux, handoff-aware backends |

**Test support** (`tests/support/`):
- `attractor_test_support.hpp` — `parse_ok(dot)` (parse + REQUIRE), `TempLogsDir` (RAII temp dir)
- `graph_builders.hpp` — pre-built `Graph` fixtures
- `main_stub.cpp` — snitch entry point; do not add `main()` to test files

Test naming: `"[component] description -- story-id"` (e.g. `"[engine] 3-node linear pipeline returns SUCCESS -- 2.3-I-001"`).

## Critical non-obvious rules

- **`node_type` ≠ `type`**: The DOT attribute is `type`; the C++ struct field on `Node` is `node_type`. Never name a new struct field `type`.
- **Strong typedefs everywhere**: Extract with `type_safe::get(x)`, never cast. New domain string types go in `include/attractor/types.hpp` with JSON declarations; definitions in `src/types.cpp`.
- **Include guards**: `#ifndef ATTRACTOR_<UPPER_PATH>_HPP` — no `#pragma once`.
- **ASCII-only source**: No Unicode in `.cpp`/`.hpp` files.
- **`[[nodiscard]]`** on every function returning a meaningful value.
- **`Outcome::fail(DiagnosticMessage{"..."})`** is the failure factory; `context_updates` must always be `nlohmann::json::object()`, never null or array.
- **`std::expected` propagation**: never `.value()` without checking; use `if (!result) return std::unexpected(result.error());`.
- **`HandlerRegistry::set_default_handler()`** must be called before any `resolve()` call on an unregistered node type or shape.
