# Session Checkpoint — 2026-05-05 Async Refactor

## Summary

This session refactored the Lynne C++ project's test and async infrastructure in two major phases:

**Phase 1 — Human interaction methods + cross-site testing:**
Added 7 user-interaction methods to `BrowserContext` (click/type/scroll/press_key/hover/exists/wait_for_selector), `dump_page_structure()` utility, anti-bot stealth improvements, and comprehensive TA tests on baidu and xiaohongshu.

**Phase 2 — Remove all `sleep()` from test infrastructure:**
Added `after()` oneshot timer to Scheduler/BrowserContext, changed `step()` to `UV_RUN_ONCE`, and replaced all `run_loop()`+`sleep(25ms)` patterns with `pump()` (zero external sleep). 4/5 test files at 0 sleep calls.

## Current State

```
git log --oneline -5:
7208f45 refactor: UV_RUN_ONCE in step(), zero sleep in pump/wait_started
1663941 refactor: add after() to Scheduler/BrowserContext; replace test sleeps with pump+after
afb0b97 skill: add lynne-browser-lessons — CDP/DOM/anti-bot hard-learned rules
cd21ba8 feat: anti-bot — disable AutomationControlled, fake UA, fix stealth script
1f19286 doc: update browser 测试方案和测试结果 with all TA test details

Untracked: opencode_session/
20/20 tests pass
```

## Architecture Decisions

### 1. `after()` oneshot timer
- **Scheduler ABC**: `after(uint64_t delay_ms, callback)` — fires once, auto-cleanup
- **UvScheduler impl**: Uses `uv_timer_start(handle, cb, delay, 0)` (repeat=0), stored in `timers_` map for cleanup in `stop()`
- **BrowserContext ABC**: `after(delay_ms, callback)` — thin proxy to `manager_->scheduler()->after()`
- **CdpBrowserContext impl**: `shared_ptr<bool> alive` guard stored in `after_guards_` vector. Destructor sets `*g = false` → timer callback checks and skips if context destroyed

### 2. `step()` → `UV_RUN_ONCE`
- Changed `UvScheduler::step()` from `uv_run(UV_RUN_NOWAIT)` to `uv_run(UV_RUN_ONCE)`
- `UV_RUN_ONCE` blocks on poll until next event arrives (timer, WS message, async handle)
- This eliminates the need for external sleep to pace the event loop

### 3. `pump()` replaces `run_loop()`
- Time-based timeout (milliseconds) instead of iteration count
- Calls `step()` in a loop with `UV_RUN_ONCE` blocking internally
- Zero external sleep

### 4. `wait_started()` replaces inline patterns
- Shared function in each test file: polls `step()` + `health_check()` until ready or timeout
- 10-second timeout for network-dependent tests

### 5. Callback delays: `sleep(2s)` → `ctx->after(2000, callback)`
- Chains async operations without blocking the event loop
- Guard-protected against context destruction

## File Status

### Zero sleep (4 files):
- `tests/wheel/browser/test_browser_ta.cpp` — 0 sleep (pump + after + wait_started)
- `tests/wheel/browser/test_browser_human_ta.cpp` — 0 sleep (pump + wait_started)
- `tests/wheel/browser/test_browser_human_ta_edge.cpp` — 0 sleep (pump + wait_started)
- `tests/wheel/browser/test_browser_xiaohongshu_ta.cpp` — 0 sleep (pump + wait_started + wait_for_selector for SPA)

### Pending conversion:
- `tests/wheel/browser/test_browser_baidu_ta.cpp` — 10 sleeps (run_loop + callback sleeps). Needs separate PR to convert callback `sleep(2s/3s)` → `after()` chains.

### ABC changes:
- `src/wheel/scheduler/scheduler.h` — + `after()` pure virtual
- `src/wheel/scheduler/imp/uv_scheduler.h` — + `after()` override + `oneshot_timer_cb`
- `src/wheel/scheduler/imp/uv_scheduler.cpp` — + `after()` impl (uses `timers_` map + `repeat=0`)
- `src/wheel/browser/browser_manager.h` — + `after()` on BrowserContext
- `src/wheel/browser/imp/cdp_browser_manager.h` — + `after()` override + `after_guards_` vector
- `src/wheel/browser/imp/cdp_browser_manager.cpp` — + `after()` impl with shared_ptr guard

### New files:
- `src/wheel/browser/browser_helpers.h` — `dump_page_structure()` declaration
- `src/wheel/browser/browser_helpers.cpp` — flat DOM dump via querySelectorAll
- `tests/wheel/browser/test_browser_human_ta_edge.cpp` — edge case tests (17 checks, 9 suites)
- `tests/wheel/browser/test_browser_xiaohongshu_ta.cpp` — cross-site validation on XHS login page
- `.opencode/skills/lynne-browser-lessons/SKILL.md` — CDP/DOM/anti-bot lessons (7 hard rules)

## Next Steps

1. Convert `test_browser_baidu_ta.cpp` callback `sleep()` → `after()` (bracket nesting is complex)
2. Build `core/adapters/` with LLM page analyzer
3. Implement core/agent/ (ReAct orchestrator)
4. Write `src/main.cpp` (Composition Root)
