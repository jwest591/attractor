# Building attractor

All commands run from the project root (`polestar/attractor/`).

## Prerequisites

- CMake 3.25+
- Ninja
- GCC 15+ or Clang 20+
- vcpkg (installed at `/home/agent/vcpkg` or any path; the toolchain file handles discovery)

## Debug build (development)

```sh
cmake --preset debug
cmake --build build/debug
```

## Run examples

After a debug build, run any `.dot` file from the `examples/` directory:

```sh
./build/debug/cli/attractor run examples/feature-pipeline.dot
```

The CLI parses and validates the graph, then executes it with the `noop` backend
(codergen nodes return a simulated response instead of calling an LLM).  Events
are printed to stdout as each stage starts and completes; run artefacts are
written to `./logs/` by default.

```
[stage 1] started: start
[stage 1] completed: start
[stage 2] started: plan
[stage 2] completed: plan
...
```

Useful flags:

| Flag | Default | Description |
|------|---------|-------------|
| `--logs-root <dir>` | `./logs` | Directory for artefacts (`manifest.json`, per-node `prompt.md` / `response.md` / `status.json`, `checkpoint.json`) |
| `--backend <name>` | `noop` | Codergen backend (`noop` = simulated responses) |

Inspect what was produced after a run:

```sh
ls logs/
cat logs/manifest.json
cat logs/plan/status.json
```

## Run tests

```sh
ctest --preset debug --output-on-failure
```

Or run the test binary directly to filter by tag:

```sh
./build/debug/tests/attractor_unit_tests [validator]
./build/debug/tests/attractor_unit_tests --list-tests
```

## Release build

```sh
cmake --preset release
cmake --build build/release
```

## Other presets

| Preset | Purpose |
|--------|---------|
| `debug` | Debug build, `-Wall -Wextra -Wpedantic -Werror` |
| `release` | Optimised build |
| `asan` | AddressSanitizer |
| `ubsan` | UndefinedBehaviorSanitizer |

## Code style

```sh
clang-format --style=file --dry-run -Werror <file>
clang-format --style=file -i <file>           # apply in-place
```
