---
description: Build and run C++ test suite
agent: build
---

Run the full test suite:

```bash
./build.sh --test
```

Or manually:

```bash
cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure -j$(nproc)
```

To run a single test binary:

```bash
dist/bin/test_storage_ta
```

LLM tests need `DEEPSEEK_API_KEY` set:

```bash
DEEPSEEK_API_KEY="sk-xxx" dist/bin/test_llm_ta
```

If there are failures, analyze the error output and suggest fixes.
