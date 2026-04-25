# Lynne

**LLM-powered personal social media intelligence tool.**

Browser automation collects raw data. LLMs understand it. You read the daily report.

---

## What it does

1. Browses Twitter, RedNote, Douyin on your behalf (Playwright + stealth)
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
| `common/` | Shared models, ABC base classes, factory pattern | Done |
| `wheel/` | Infrastructure: config, logging, storage | Done |
| `core/` | Business: browser, adapters, LLM, orchestrator, scheduler, API | In progress |

**Key design decisions:**
- Strict dependency inversion: every module exposes an ABC interface. No code imports implementations directly.
- Factory pattern for wiring (`XxxFactory(Factory[T])`), composition root in `main.py`
- All long-lived modules inherit `Module(ABC)` with `start()/stop()/health_check()/name`
- JSONL file storage, one directory per date — openable with any text editor

## Current status

**v0.1.0** — early development.

| Module | Tests | Coverage |
|--------|-------|----------|
| `common/` — models, factory, module ABC | 13 UT | 100% |
| `wheel/config/` — YAML config + env var substitution | 39 UT+TA | ~96% |
| `wheel/storage/` — JSONL read/write | 23 UT+TA | ~96% |
| **Total** | **75** | **~98%** |

The `core/` business layer and Web UI are not yet built.

## Quickstart

### Prerequisites
- Python ≥ 3.11
- [Playwright](https://playwright.dev/) browsers installed

### Install

```bash
git clone https://github.com/anomalyco/lynne.git
cd lynne
python3 -m venv .venv
.venv/bin/pip install -e ".[claude]"
playwright install chromium
```

### Configure

```bash
export DEEPSEEK_API_KEY="sk-your-key-here"
cp config.yaml config.local.yaml   # edit as needed
```

`config.yaml` supports `${ENV_VAR}` substitution for secrets.

### Run (once core is built)

```bash
lynne serve      # start web UI on http://localhost:7890
```

### Run tests

```bash
.venv/bin/pip install pytest pytest-asyncio pytest-cov
pytest tests/ -v
pytest tests/ -v --cov=src --cov-report=term-missing
```

## Config example

```yaml
server:
  port: 7890

llm:
  provider: "deepseek"
  api_key: "${DEEPSEEK_API_KEY}"
  model: "deepseek-chat"

tasks:
  - name: "AI industry tracker"
    platforms: [twitter, rednote]
    topic: "AI model releases, research breakthroughs, startup funding"
    schedule: "every 4 hours"
    limit: 20
```

## Project structure

```
src/
├── common/          # Factory[T], Module(ABC), UnifiedItem models
├── wheel/           # Infrastructure
│   ├── config/      # ConfigLoader ABC + YAML impl
│   ├── logger/      # loguru wrapper
│   └── storage/     # Storage ABC + JSONL impl
└── core/            # (in progress)
    ├── browser/     # Playwright manager
    ├── adapters/    # Platform scrapers
    ├── llm/         # LLM engine
    ├── orchestrator/# Task orchestration
    ├── scheduler/   # Cron scheduling
    └── api/         # FastAPI routes

tests/               # mirrors src/, UT + TA per module
doc/
├── design/          # Requirements, architecture, test spec
└── tech/            # Technical guides (pytest, etc.)
```

## License

MIT
