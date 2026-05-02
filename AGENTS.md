# AGENTS.md — Lynne

## Project status

v0.2.0 — C++17 project. 7/11 modules implemented (common, config, logger, storage, scheduler, llm, ws_client). `src/core/` (adapters, agent, browser) and `src/main.cpp` (Composition Root) are **not yet built**.

## Architecture (hard rules)

- **Layering**: `core -> wheel -> common` (one-way only, never reverse)
- **Dependency inversion**: every module has ABC interface (pure virtual class) + models + factory + `imp/` implementation. No file outside `main.cpp` may import from `imp/`. Modules depend on interfaces only.
- **Factory pattern**: factories are pure dispatchers — map config to impl class. Factories hold no runtime dependencies. Impl classes create their own sub-dependencies internally (via their own factories, lazy init). `main.cpp` is the sole Composition Root.
- **Module ABC**: all long-lived modules inherit `common::Module` (`start`, `stop`, `health_check`, `name`).

## Thread model (hard rules)

- **Single libuv event loop** (`uv_default_loop()`) for all I/O.
- **`wheel/scheduler/` is the sole authorized thread pool.** No other module may create `std::thread`, `std::async`, or call `uv_queue_work` directly.
- **Blocking work**: use `scheduler.run_blocking(work_fn, done_fn)` — submits to libuv's worker pool via `uv_queue_work`.
- **All I/O is async**: IXWebSocket for CDP/WebSocket, `uv_timer_start` for scheduling, `uv_async_send` for cross-thread notification.
- **No `std::future`/`std::promise`** — callback-based async only (`std::function<void(T)>`).

Per-module file layout:
```
{module}.h           # ABC interface
{module}_models.h    # struct + NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE
{module}_models.cpp  # from_json / to_json
{module}_factory.h   # Factory subclass declaration
{module}_factory.cpp # create() dispatcher
imp/{impl}.h         # concrete implementation header
imp/{impl}.cpp       # concrete implementation
```

Reference docs: `doc/design/DESIGN.md`, `.opencode/skills/lynne-code-conventions/SKILL.md`

## Commands

```bash
# Build everything
./build.sh

# Build & run tests
./build.sh --test

# Run all tests manually
ctest --test-dir build --output-on-failure -j$(nproc)

# Run specific test binary
dist/bin/test_storage_ta

# Run LLM TA test (needs DEEPSEEK_API_KEY)
DEEPSEEK_API_KEY="sk-xxx" dist/bin/test_llm_ta

# Build only (no test)
cmake --build build -j$(nproc)

# Full from scratch
./build.sh --all
```

## Build system

```bash
# Step 1: compile third-party deps (one-time)
./build-deps.sh            # -> dist/include/ dist/lib/

# Step 2: configure & build project
cmake -S . -B build
cmake --build build -j$(nproc)   # -> build/src/ (libs) + dist/bin/ (tests)
```

## Test conventions

| Type | Scope | Framework | Covers |
|------|-------|-----------|--------|
| **UT** | single class/function, pure memory | GTest (`add_lynne_test`) | defaults, serialization, validation, edge cases |
| **TA** | one module full lifecycle | standalone (`add_lynne_ta`) | factory -> impl, all public methods, round-trip consistency |

- Test files mirror source: `src/{module}/{file}.h/cpp` -> `tests/{module}/test_{file}.cpp`
- ABC interfaces and logger utilities are not tested directly
- TA tests use `std::filesystem::temp_directory_path()` for file isolation
- UT tests use `TEST(SuiteName, TestName)` with `EXPECT_EQ` etc.
- TA tests have custom `main()` with manual pass/fail counting
- TA tests register with ctest: `add_test(NAME ... COMMAND ...)`
- LLM TA tests skip API calls when `DEEPSEEK_API_KEY` not set

## Config

- `config.json` at project root (JSON format, read by `JsonConfigLoader`)
- `DEEPSEEK_API_KEY` env var fallback when `api_key` field is empty
- `SSL_CERT_FILE` env var for custom CA cert path; auto-detected from system paths

## Lint / typecheck

No explicit lint config yet. `clang-tidy` can be run manually; no pre-commit hooks or CI.

## Git

This repo has a `.git` directory and is under version control.
