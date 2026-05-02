# Lynne

**LLM-powered personal social media intelligence tool.**

Browser automation collects raw data. LLMs understand it. You read the daily report.

---

## What it does

1. Browses Twitter, RedNote, Douyin on your behalf (CDP + stealth)
2. Feeds collected content to LLM for filtering, summarization, and topic extraction
3. Generates a structured daily report: "These are the 4 things you should know about today"
4. Everything runs locally. Everything stores as plain JSONL files. No databases.

```
You configure topics  →  Lynne scrapes periodically  →  LLM curates  →  Web UI shows report
```

## Architecture

```
          ┌─── Web UI ───┐
          │  (HTML/JS)   │
          └──────┬───────┘
          ┌──────▼───────┐
          │   API Layer  │
          └──────┬───────┘
          ┌──────▼───────┐
          │ Orchestrator │
          └──┬──┬──┬─────┘
      ┌──────┘  │  └──────┐
      ▼         ▼         ▼
 Browser  Platform   LLM Engine
 Manager  Adapters
      │         │         │
      └────┬────┘    ┌────▼────┐
           ▼         ▼         ▼
        ┌──────────────────────┐
        │    FileStorage       │
        │   (JSONL per date)   │
        └──────────────────────┘
```

**Layers** (one-way dependency, never reverse):

| Layer | Role | Status |
|-------|------|--------|
| `common/` | Shared models, Module ABC | Done |
| `wheel/` | Infrastructure: config, logger, storage, scheduler, LLM, WebSocket | Done (7 modules) |
| `core/` | Business: browser, adapters, agent | Not yet built |

**Key design decisions:**
- Strict dependency inversion: every module exposes a pure virtual ABC interface
- Factory pattern for wiring, composition root in `main.cpp`
- All long-lived modules inherit `common::Module` with `start/stop/health_check/name`
- JSONL file storage, one directory per date
- Single libuv event loop; `wheel/scheduler/` is the sole authorized thread pool

## Current status

**v0.2.0** — C++ port complete (7/11 modules). Core business layer in progress.

| Module | Tests | Status |
|--------|-------|--------|
| `common/` — models, Module ABC | 2 (UT+TA) | Done |
| `wheel/config/` — JSON config loader | 2 (UT+TA) | Done |
| `wheel/logger/` — spdlog wrapper | 2 (UT+TA) | Done |
| `wheel/storage/` — JSONL read/write | 2 (UT+TA) | Done |
| `wheel/scheduler/` — libuv scheduler | 2 (UT+TA) | Done |
| `wheel/llm/` — DeepSeek/OpenAI chat | 2 (UT+TA) | Done |
| `wheel/ws_client/` — WebSocket client | 2 (UT+TA) | Done |
| `wheel/browser/` — CDP browser mgr | — | Pending |
| `core/adapters/` — platform scrapers | — | Pending |
| `core/agent/` — ReAct orchestrator | — | Pending |
| `main.cpp` — composition root | — | Pending |

## Quickstart

### Prerequisites
- CMake >= 3.16
- C++17 compiler (GCC 9+, Clang 10+)
- Chromium/Chrome (for CDP browser automation)

### Build dependencies (one time)

```bash
./build-deps.sh
```

### Build & test

```bash
./build.sh --test
```

Or manually:

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure -j$(nproc)
```

### Run (once core is built)

```bash
DEEPSEEK_API_KEY="sk-your-key" ./build/lynne serve
```

## Config example

```json
{
  "server": { "port": 7890 },
  "llm": {
    "provider": "deepseek",
    "api_key": "",
    "model": "deepseek-chat"
  },
  "tasks": [
    {
      "name": "AI industry tracker",
      "platforms": ["twitter", "rednote"],
      "intent": "AI model releases, research breakthroughs, startup funding",
      "schedule": "every 4 hours",
      "limit": 20
    }
  ]
}
```

API key: set via `DEEPSEEK_API_KEY` env var (fallback when `api_key` is empty).

## Project structure

```
src/
├── common/          # Module ABC, UnifiedItem models
├── wheel/           # Infrastructure
│   ├── config/      # ConfigLoader ABC + JSON impl
│   ├── logger/      # spdlog wrapper
│   ├── storage/     # Storage ABC + JSONL impl
│   ├── scheduler/   # Scheduler ABC + libuv impl
│   ├── llm/         # LLMEngine ABC + DeepSeek impl
│   └── ws_client/   # WsClient ABC + IXWebSocket impl
└── core/            # (in progress)
    ├── browser/     # CDP browser manager
    ├── adapters/    # Platform scrapers
    ├── agent/       # ReAct orchestration
    └── api/         # HTTP server

tests/               # mirrors src/, UT + TA per module
doc/                 # Design docs, test plans
dist/                # Build output (libs, bins, headers)
third_party/         # External dependencies
```

## License

MIT
