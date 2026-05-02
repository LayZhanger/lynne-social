---
description: Run clang-tidy lint (manual)
agent: build
---

No automated lint is configured yet. Run manually:

```bash
find src/ -name '*.cpp' -o -name '*.h' | xargs clang-tidy -p build/ --quiet 2>/dev/null
```

Fix any warnings found. The project follows conventions in `.opencode/skills/lynne-code-conventions/SKILL.md`.
