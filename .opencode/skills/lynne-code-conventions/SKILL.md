---
name: lynne-code-conventions
description: Lynne project code conventions — layering, dependency inversion, factory pattern, testing standards, file layout
---

# Lynne 代码规范

> 本项目的所有代码必须遵循以下规范。

---

## 1. 目录分层架构

```
src/
├── common/          # 公共资源 — 所有模块共享的基类和规约
├── wheel/           # 通用基础设施 — 与业务无关，可跨项目复用
└── core/            # 业务逻辑 — Lynne 特有功能
```

**依赖方向**：`core → wheel → common` （单向，不可反向）

---

## 2. 模块内部结构

每个模块目录统一包含以下文件：

```
{module}/
├── __init__.py                 # 导出：接口 + 工厂 + 关键类型
├── {module}.py                 # 抽象接口 (ABC)
├── {module}_models.py          # 模块专属数据结构 (dataclass / Pydantic)
├── {module}_factory.py         # 工厂类 XxxFactory(Factory[T])
└── imp/                        # 具体实现目录
    ├── __init__.py              # 空文件
    └── {impl_name}.py          # 实现类（继承抽象接口）
```

| 文件 | 内容 | 示例 |
|------|------|------|
| `{module}.py` | 抽象接口类（ABC） | `class Storage(ABC, Module)` |
| `{module}_models.py` | 本模块数据结构 | `class StorageConfig(dataclass)` |
| `{module}_factory.py` | 工厂类 | `class StorageFactory(Factory[Storage])` |
| `imp/{impl_name}.py` | 具体实现 | `class JsonlStorage(Storage)` |

---

## 3. 依赖倒置原则（最重要）

1. **模块之间只依赖接口（ABC），不依赖具体实现**
2. **任何模块的代码中，不允许出现 `from ..imp import ...`**
3. **具体实现通过工厂在 `main.py` 中注入**

```python
# ✅ 正确：构造参数用接口
class DefaultOrchestrator(Orchestrator):
    def __init__(
        self,
        browser: BrowserManager,     # 接口，不是 PlaywrightBrowserManager
        llm: LLMEngine,              # 接口，不是 OpenAICompatEngine
        storage: Storage,            # 接口，不是 JsonlStorage
    ):
        ...

# ❌ 禁止：直接引用实现
from core.browser.imp.playwright_browser_manager import PlaywrightBrowserManager
browser = PlaywrightBrowserManager(...)
```

---

## 4. 工厂模式（工厂不持有依赖）

基类 (`common/factory.py`):

```python
class Factory(ABC, Generic[T]):
    @abstractmethod
    def create(self, config: object) -> T: ...
```

**硬规则**：工厂是纯分发器，不持有、不创建任何运行时依赖。

```
工厂只做一件事：config → 选择实现类 → new 返回。
实现的依赖（BrowserManager、LLMEngine、Scheduler 等）由实现自身内部创建，
或由 main.py 直接传给实现构造器，不经过工厂。
```

```python
# ✅ 正确：工厂零依赖，纯分发
class AdapterFactory:
    def create(self, browser, config, *, llm_config=None) -> BaseAdapter:
        if config.platform == "rednote":
            return RedNoteAdapter(browser, config, llm_config=llm_config)
        raise ValueError(...)

# ✅ 正确：实现内部自建子依赖
class LLMAdapter(BaseAdapter):
    def __init__(self, ..., llm_config=None):
        self._llm_config = llm_config  # 存配置，不建 engine

    async def _ensure_llm(self):
        self._llm = LLMEngineFactory().create(self._llm_config)  # 父类自建

# ❌ 禁止：工厂持有依赖
class AdapterFactory(Factory[BaseAdapter]):
    def __init__(self, browser_factory: Factory[BrowserManager], browser_config):
        self._browser_factory = browser_factory  # ❌ 工厂不应持 browser
```

**工厂可以 import 实现文件**（这是它存在的目的——把 platform 映射到具体类）。除此以外的代码禁止跨层 import `imp/`。

---

## 5. Module 基类

所有需要生命周期的模块继承 `Module(ABC)`：

```python
class Module(ABC):
    @abstractmethod
    async def start(self) -> None: ...
    @abstractmethod
    async def stop(self) -> None: ...
    @abstractmethod
    def health_check(self) -> bool: ...
    @property
    @abstractmethod
    def name(self) -> str: ...
```

---

## 6. 公共数据规约

`common/models.py` 定义跨模块共享的数据结构：
- `UnifiedItem` — 跨平台统一内容
- `TaskResult` — 任务执行结果
- `RunStatus` — 运行状态

跨模块共享的结构体必须放 `common/models.py`，不能放各自模块的 `_models.py`。

---

## 7. 启动入口职责

`main.py` 是唯一的 **组合根（Composition Root）**，负责加载配置、创建工厂、组装依赖。`main.py` 是唯一可以引用具体实现和工厂的地方。

---

## 8. 测试规范

| 缩写 | 全称 | 范围 |
|------|------|------|
| **UT** | Unit Test | 一个类/函数：默认值、序列化、校验、边界 |
| **TA** | 单模块 E2E | 一个模块全链路：factory → impl → 全部方法 + 生命周期 |

目录镜像：
```
src/{module}/{module}_models.py  →  tests/{module}/test_{module}_models.py   (UT)
src/{module}/{module}_factory.py →  tests/{module}/test_{module}_factory.py  (UT)
src/{module}/imp/{impl}.py       →  tests/{module}/test_{module}_impl.py      (TA)
```

运行命令：
```bash
pytest tests/ -v                                  # 全部
pytest tests/ -v -m "not asyncio"                  # UT only
pytest tests/ -v -m asyncio                        # TA only
pytest tests/ -v --cov=src --cov-report=term-missing  # 带覆盖率
```
