# AGENTS.md — Lynne

## Project status

Early development (v0.2.0). `src/core/` (business layer) and `src/main.py` (Composition Root) are **not yet built**. `src/wheel/` modules are mostly complete (browser, config, storage, logger, scheduler), with `llm/` still skeleton.

## Architecture (hard rules)

- **Layering**: `core → wheel → common` (one-way only, never reverse)
- **Dependency inversion**: every module has ABC interface + factory + models + `imp/` implementation. No file outside `main.py` may import from `imp/`. Modules depend on interfaces only.
- **Factory pattern**: all wiring happens via factories. Factories with dependencies receive them via `__init__` injection. `main.py` is the sole Composition Root.
- **Module ABC**: all long-lived modules inherit `Module(ABC)` (`start`, `stop`, `health_check`, `name`).

## Thread model (hard rules)

- **Single asyncio event loop** for all I/O. Everything runs on it by default.
- **`wheel/scheduler/` is the sole authorized thread pool.** No other module may import `threading`, `concurrent.futures`, `asyncio.to_thread`, or `loop.run_in_executor`.
- **Blocking work**: if a module has unavoidable blocking work (CPU-heavy or sync library), use `scheduler.run_blocking(func, *args, **kwargs)` — never spawn your own thread. This method submits to the scheduler's semaphore-governed worker pool and returns an awaitable.
- **All I/O must be async**: `aiofiles` / async Playwright / `httpx` / etc.
- **Future direction**: `JsonlStorage` and `YamlConfigLoader` currently use sync file I/O — they'll be migrated to async (either native async or via `scheduler.run_blocking`) when the scheduler is wired into `main.py`.

Per-module file layout:
```
{module}.py           # ABC interface
{module}_models.py    # Pydantic/dataclass models
{module}_factory.py   # Factory[T] subclass
imp/{impl}.py         # concrete implementation
```

Reference docs: `doc/design/DESIGN.md`, `.opencode/skills/lynne-code-conventions.md`

## Commands

```bash
# Run all tests
pytest tests/ -v

# UT only (no I/O, pure memory)
pytest tests/ -v -m "not asyncio"

# TA only (single-module E2E with real tmp_path)
pytest tests/ -v -m asyncio

# With coverage
pytest tests/ -v --cov=src --cov-report=term-missing

# Run a single test file/module
pytest tests/common/ -v
pytest tests/wheel/config/ -v
```

Python venv is at `.venv`. `pytest-asyncio` is `auto` mode (see `pyproject.toml`).

## Test conventions

| Type | Scope | Covers |
|------|-------|--------|
| **UT** | single class/function, pure memory | defaults, serialization, validation, edge cases |
| **TA** | one module full lifecycle | factory → impl, all public methods, round-trip consistency |

- Test files mirror source: `src/{module}/{file}.py` → `tests/{module}/test_{file}.py`
- ABC interfaces and logger utilities are not tested directly
- TA tests use `tmp_path` for file isolation; never real disk paths
- Async tests must be marked `@pytest.mark.asyncio`
- Shared fixtures live in `tests/conftest.py` (`sample_item`, `sample_items`, `sample_yaml_file`, `sample_yaml_content`)
- Some config tests need `monkeypatch` to set `DEEPSEEK_API_KEY`

## Config

- `config.yaml` at project root; supports `${ENV_VAR}` substitution
- The YAML loader `_resolve_env` raises `ValueError` if an env var is not set — ensure `DEEPSEEK_API_KEY` is exported before loading any config that references it

## Lint / typecheck

Ruff and mypy are used but have no explicit config files (default settings). Run them manually; no pre-commit hooks or CI currently set up.

## Git

This repo has a `.git` directory and is under version control.
