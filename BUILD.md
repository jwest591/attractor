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
