---
description: Run ruff lint and mypy typecheck
agent: build
---

Run lint and typecheck for this Python project:

```bash
.venv/bin/ruff check src/ tests/ && .venv/bin/mypy src/ tests/
```

Fix any issues found.
