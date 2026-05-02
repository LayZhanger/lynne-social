# 文档迁移计划 — Python → C++

> 将 README.md、AGENTS.md、.opencode/、doc/ 从 Python 版迁移到 C++ 版。

---

## 1. README.md — 完全重写

**源文件**: `/home/lay/code/lynne-social/README.md`

**变更要点**:
- Quickstart: 删 `python3 -m venv`/`pip install`/`playwright install` → `./build-deps.sh` / `cmake -S . -B build` / `cmake --build build`
- 测试: 删 `pytest` → `./build.sh --test` 或 `ctest --test-dir build`
- 架构图保持不变（core→wheel→common 分层仍然有效）
- 模块状态表: 更新为 C++ 实际状态（7/11 模块已完成，列出 common/config/logger/storage/scheduler/llm/ws_client）
- config.yaml → config.json 示例
- 项目结构: `.h/.cpp` 代替 `.py`，新增 `dist/` 和 `third_party/`
- 环境: CMake 3.16+, C++17 compiler, Chromium (CDP)

**完整内容**: 见 SESSION_EXPORT 中的 README.md 草稿。

---

## 2. AGENTS.md — 完全重写

**源文件**: `/home/lay/code/lynne-social/AGENTS.md`

**变更要点**:
- 命令: 删 `pytest tests/ -v` → `./build.sh --test` / `ctest --test-dir build --output-on-failure -j$(nproc)`
- 架构规则保留（core→wheel→common、依赖倒置、工厂模式、Module ABC）— 语言无关
- 线程模型: 删 asyncio → libuv (`uv_default_loop()`, `uv_queue_work`)
- 模块文件布局: `.py`/`__init__.py` → `.h/.cpp`/`imp/{impl}.h+.cpp`
- 测试: 删 pytest/pytest-asyncio → GTest UT + standalone TA
- 删 `config.yaml` `${ENV_VAR}` → `config.json` + `DEEPSEEK_API_KEY` env fallback
- Lint: 删 ruff/mypy → "no lint config yet"
- 新增 build-deps.sh / build.sh 说明

---

## 3. `.opencode/skills/lynne-code-conventions/SKILL.md` — 完全重写

**源文件**: `/home/lay/code/lynne-social/.opencode/skills/lynne-code-conventions/SKILL.md`

**变更要点**:
- 目录分层: 保留 core→wheel→common，更新文件路径
- 模块结构: 删 `__init__.py`/`.py` → `.h` + `_models.h` + `_models.cpp` + `_factory.h` + `_factory.cpp` + `imp/{impl}.h+.cpp`
- 依赖倒置: ABC → 纯虚类 (`virtual ... = 0`), `#pragma once`
- Factory: `Factory[T]` 泛型 → 具体工厂类，返回裸指针
- Module: `async def start/stop` → `virtual void start/stop() = 0`
- 测试: pytest → `add_lynne_test` (UT/GTest) + `add_lynne_ta` (standalone)
- 命名: PascalCase/snake_case/尾下划线规则
- 公共数据: struct + `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE`

---

## 4. `.opencode/commands/test.md` — 重写

**源文件**: `/home/lay/code/lynne-social/.opencode/commands/test.md`

```yaml
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
```

---

## 5. `.opencode/commands/lint.md` — 重写

**源文件**: `/home/lay/code/lynne-social/.opencode/commands/lint.md`

```yaml
---
description: Run clang-tidy lint (manual)
agent: build
---

No automated lint is configured yet. Run manually:

```bash
# Run clang-tidy on all source files
find src/ -name '*.cpp' -o -name '*.h' | xargs clang-tidy -p build/ --quiet 2>/dev/null
```

Fix any warnings found. The project follows the conventions in `.opencode/skills/lynne-code-conventions/SKILL.md`.
```

---

## 6. `.opencode/agents/lynne-builder.md` — 重写

**源文件**: `/home/lay/code/lynne-social/.opencode/agents/lynne-builder.md`

**变更要点**:
- Python 3.11+ → C++17
- 删 async def / await → std::function void(T) 回调
- 删 Factory[T] → 具体工厂类
- 模块文件布局: `.h/.cpp` 代替 `.py`/`__init__.py`
- CMake: `add_lynne_library()` + 在 `src/CMakeLists.txt` 注册
- 测试: GTest + standalone TA
- 语法约束: 禁模板、禁 std::future、禁 std::thread
- 构建命令: `./build.sh --test`

---

## 7. `doc/design/DESIGN.md` — 部分更新

**源文件**: `/home/lay/code/lynne-social/doc/design/DESIGN.md` (613 行)

**保留不变的内容**:
- §1 设计理念 (ReAct loop, Agent 概念)
- §2 系统分层图 (core→wheel→common, 模块结构图)
- §5 Tool 系统 (SearchTool/ReadStorageTool 接口)
- §6 模块协作时序图 (Agent 流程)
- §8 模块依赖关系图

**需更新的内容**:

| 位置 | 当前 | 改后 |
|------|------|------|
| §3 模块清单 W2 | LLMEngine wheel（移入） | LLMEngine wheel ✅ |
| §3 模块清单 W5 | Scheduler wheel | Scheduler wheel ✅ |
| §3 (新增) | 无 | ws_client wheel ✅ |
| §3 (新增) | 无 | browser wheel 待实现 |
| §4 W1 Browser | `async def start()` Playwright | CDP + IXWebSocket |
| §4 W2 LLMEngine | `async def chat()` → dict | `void chat(messages, on_ok, on_err)` 回调 |
| §4 W2 LLMEngine | `stdlib urllib` | `cpp-httplib::SSLClient` |
| §4 W2 LLMEngine | `scheduler.run_blocking()` | `scheduler->run_blocking(work, done)` |
| §4 W2 LLMEngine | `__init__.py` | 无 |
| §4 W4 Config | `YamlConfigLoader` | `JsonConfigLoader` |
| §4 W4 Config | `config.yaml` | `config.json` |
| §4 W5 Scheduler | `apscheduler_impl.py` | `uv_scheduler` libuv |
| §4 W6 Logger | `loguru` | `spdlog` |
| §4 M4a LLMAdapter | `async def _ensure_llm()` | 同步 `_ensure_llm()` + 回调 |
| §4 M4a LLMAdapter | `LLMEngineFactory` | `LLMFactory` |
| §4 M4a LLMAdapter | `.py` 文件路径 | `.h/.cpp` 文件路径 |
| §7 启动流程 | `main.py` | `main.cpp` |
| §9 实现阶段 | Stage 1-6 列表 (Python) | 更新为 C++ 实际状态 |
| §10 执行模型 | asyncio | libuv |
| §10 执行模型 | `asyncio.to_thread` | `uv_queue_work` |
| 全文 | `async def` 方法 | 纯虚函数 + 回调参数 |
| 全文 | `__init__.py` | 删 |
| 全文 | `Pydantic` / `BaseModel` | `struct` + `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` |
| 全文 | Python 代码示例 | C++ 等效代码示例 |

---

## 8. `doc/design/TEST.md` — 重写

**源文件**: `/home/lay/code/lynne-social/doc/design/TEST.md` (379 行)

- §1 测试类型: pytest + pytest-asyncio → GTest UT + standalone TA
- §2 目录镜像: `.py` → `.cpp`
- §3 文件命名: `.py` → `.cpp`
- §4 UT vs TA 职责: 保留（语言无关），更新框架名
- §5 覆盖标准: 保留内容，改 C++ 代码示例
- §6 公共 Fixtures: 删 Python conftest.py → C++ 测试内建 fixture 函数
- §7 TA 模板: pytest → standalone main() + CHECK 宏
- §8 UT 模板: pytest → GTest TEST()
- §9 运行命令: `pytest` → `./build.sh --test` / `ctest`
- §10 检查清单: 保留（语言无关）
- §11 测试文档规范: 保留

---

## 9. `doc/tech/pytest-guide.md` — 删除

**源文件**: `/home/lay/code/lynne-social/doc/tech/pytest-guide.md` (234 行)

删除原因: 已无 Python 项目。C++ 测试规范已由 `.opencode/skills/lynne-cpp-migration/SKILL.md` 和 `TEST.md` 覆盖。

---

## 10. `doc/tests/` — 测试方案/结果

**源目录**: `/home/lay/code/lynne-social/doc/tests/`

保留现有文件。需新增加以下模块的测试方案/结果:
- `wheel/scheduler/` — 测试方案.md + 测试结果.md
- `wheel/llm/` — 测试方案.md + 测试结果.md
- `wheel/ws_client/` — 测试方案.md + 测试结果.md

---

## 执行顺序

1. 删除 `doc/tech/pytest-guide.md`
2. 写完整替换 `README.md`
3. 写完整替换 `AGENTS.md`
4. 写完整替换 `.opencode/skills/lynne-code-conventions/SKILL.md`
5. 写完整替换 `.opencode/commands/test.md`
6. 写完整替换 `.opencode/commands/lint.md`
7. 写完整替换 `.opencode/agents/lynne-builder.md`
8. 在 `doc/design/DESIGN.md` 中做靶向编辑 (~30 处替换)
9. 写完整替换 `doc/design/TEST.md`
10. 新建 `doc/tests/wheel/scheduler/`、`wheel/llm/`、`wheel/ws_client/` 测试方案/结果
