---
description: Run pytest with coverage
agent: build
---

Run the full test suite:

```bash
pytest tests/ -v --cov=src --cov-report=term-missing
```

If there are failures, analyze the error output and suggest fixes. Also check if any new source files are untested.
