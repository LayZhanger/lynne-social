---
description: Build new Lynne modules following C++ project conventions
mode: subagent
---

You are building code for the Lynne project (C++17). Follow these hard rules:

1. **Layering**: `core -> wheel -> common` (one-way only, never reverse)
2. **Dependency inversion**: every module has ABC (pure virtual) + models + factory + `imp/` implementation
3. **No file outside `main.cpp` may import from `imp/`**
4. **Factory pattern**: factories are pure dispatchers — map config to impl class, hold no runtime deps
5. **Module ABC**: long-lived modules inherit `lynne::common::Module` (`start`, `stop`, `health_check`, `name`)
6. **Callback async**: no `std::future`, no `std::thread`. Use `std::function<void(T)>` callbacks. Blocking work goes through `scheduler.run_blocking()`.
7. **Single libuv event loop**: `uv_default_loop()`. No direct `std::thread` creation.
8. **Syntax constraints**: no templates, no CRTP, no concepts, no SFINAE. `nlohmann/json` allowed (macro expansion, not templates). `std::function`, lambdas, standard containers allowed.

Per-module file layout:
```
{module}.h           # ABC interface (pure virtual)
{module}_models.h    # struct + NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE
{module}_models.cpp  # from_json / to_json
{module}_factory.h   # Factory subclass
{module}_factory.cpp # create() dispatcher
imp/{impl}.h         # concrete impl header
imp/{impl}.cpp       # concrete impl
```

CMake: add `add_subdirectory()` in `src/CMakeLists.txt`, module-level `CMakeLists.txt` with `add_lynne_library()`.

Tests mirror source: `src/{module}/{file}.h/cpp` -> `tests/{module}/test_{file}.cpp`

UT = GTest (`add_lynne_test`, `TEST(Suite, Case)`). TA = standalone (`add_lynne_ta`, custom main with pass/fail counting).

Run `./build.sh --test` to verify. Read `AGENTS.md` and skill `lynne-code-conventions` for full details.
