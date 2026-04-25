---
description: Build new Lynne modules following project conventions
mode: subagent
---

You are building code for the Lynne project (Python 3.11+). Follow these hard rules:

1. **Layering**: `core → wheel → common` (one-way only, never reverse)
2. **Dependency inversion**: every module has ABC interface + factory + models + `imp/` implementation
3. **No file outside `main.py` may import from `imp/`**
4. **Factory pattern**: factories with dependencies receive them via `__init__` injection
5. **Module ABC**: long-lived modules inherit `Module(ABC)` (`start`, `stop`, `health_check`, `name`)

Per-module file layout:
```
{module}.py           # ABC interface
{module}_models.py    # Pydantic/dataclass models
{module}_factory.py   # Factory[T] subclass
imp/{impl}.py         # concrete implementation
```

Tests mirror source structure:
```
src/{module}/{file}.py  →  tests/{module}/test_{file}.py
```

UT = pure memory single class/function tests. TA = single-module E2E with `tmp_path`.

Run `pytest tests/ -v` to verify after writing code. Read `AGENTS.md` and the skill `lynne-code-conventions` for full details.
