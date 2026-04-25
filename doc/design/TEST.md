# Lynne 测试规范 v1.0

> 所有模块的测试必须遵循本规范。
> 新增模块时，按此模板创建 `tests/` 下对应的测试文件。

---

## 1. 测试类型定义

| 缩写 | 全称 | 范围 | 被测目标 |
|------|------|------|----------|
| **UT** | Unit Test | 最小单元，无副作用 | 一个类/一个函数：默认值、序列化、校验、边界 |
| **TA** | 单模块 E2E | 一个模块全链路 | factory → impl → 全部 public 方法 + 生命周期 + 边界 + 错误路径 |

---

## 2. 目录镜像规则

```
src/xxx.py                              → tests/xxx/test_xxx.py
src/{module}/{module}_models.py         → tests/{module}/test_{module}_models.py    (UT)
src/{module}/{module}_factory.py        → tests/{module}/test_{module}_factory.py   (UT)
src/{module}/imp/{impl}.py              → tests/{module}/test_{module}_impl.py      (TA)
```

**规则**：
- 每个源文件对应一个测试文件
- ABC 接口文件（`{module}.py`）不单独测试（无实例可测）
- logger 工具函数类不单独测试（副作用 > 收益）
- 测试文件放在 `tests/` 下，目录层级与 `src/` 镜像

---

## 3. 文件命名

```
test_{模块名}_models.py       ← UT: 数据模型
test_{模块名}_factory.py      ← UT: 工厂行为
test_{模块名}_impl.py         ← TA: 模块全链路
```

---

## 4. UT vs TA 职责边界

| | UT | TA |
|---|---|---|
| 测什么 | 一个类/函数 | 一个模块的完整实现 |
| 隔离方式 | 纯内存，无外部依赖 | 真文件系统（`tmp_path`）、真环境变量 |
| 侧重点 | 边界值、默认值、序列化、类型校验、异常 | 往返验证、状态变更、顺序性、生命周期、错误路径 |
| 示例 | `LLMConfig(provider="deepseek")` 校验字段 | `YamlConfigLoader` load → reload → env 替换全流程 |
| **不测** | 文件 IO、网络、多类协作 | 跨模块协作（那是 IT） |

---

## 5. 每个源文件的覆盖标准

```
_source file_            _test type_    _至少应覆盖_

{module}_models.py         UT           创建 + 默认值、字段必填/可选校验、
                                        序列化/反序列化、边界空值/空列表

{module}_factory.py        UT           create(默认) → 默认实现
                                        create(str) → 指定路径/参数
                                        create(config对象) → 对象参数
                                        返回类型 isinstance 断言

imp/{impl}.py              TA           启动 → 停止 生命周期
                                        health_check 通过/不通过
                                        全部 public 方法 × (正常 + 空 + 异常)
                                        往返一致性: 写 → 读 → 相等
                                        顺序性: list_dates 排序
                                        name 属性
```

---

## 6. 公共 Fixtures（`tests/conftest.py`）

```python
import pytest
from pathlib import Path
from src.common.models import UnifiedItem


@pytest.fixture
def sample_item() -> UnifiedItem:
    """一个标准 UnifiedItem"""
    return UnifiedItem(
        platform="twitter",
        item_id="123",
        author_name="test_user",
        content_text="Hello world",
    )


@pytest.fixture
def sample_items(sample_item) -> list[UnifiedItem]:
    """批量 UnifiedItem"""
    return [
        sample_item,
        UnifiedItem(platform="rednote", item_id="456", content_text="小红书"),
        UnifiedItem(platform="twitter", item_id="789", content_text="Another"),
    ]


@pytest.fixture
def sample_yaml(tmp_path) -> Path:
    """写入临时 YAML 文件，返回路径"""
    content = """server:
  port: 7890
llm:
  provider: deepseek
browser:
  headless: false
"""
    path = tmp_path / "test_config.yaml"
    path.write_text(content)
    return path
```

---

## 7. TA 测试模板（以 Storage 为例）

```python
import pytest
from src.wheel.storage.storage_factory import StorageFactory
from src.wheel.storage.storage_models import StorageConfig


class TestJsonlStorageTA:
    """JsonlStorage 单模块 E2E"""

    @pytest.fixture
    def storage(self, tmp_path):
        return StorageFactory().create(StorageConfig(data_dir=str(tmp_path / "data")))

    @pytest.fixture
    async def started_storage(self, storage):
        await storage.start()
        return storage

    # ── 生命周期 ──────────────────────────

    def test_name(self, storage):
        assert storage.name == "JsonlStorage"

    @pytest.mark.asyncio
    async def test_start_creates_dir(self, storage):
        await storage.start()
        assert storage.health_check()

    @pytest.mark.asyncio
    async def test_stop(self, storage):
        await storage.start()
        await storage.stop()
        # should not raise

    # ── items 往返 ─────────────────────────

    @pytest.mark.asyncio
    async def test_save_and_load_items(self, started_storage, sample_items):
        started_storage.save_items(sample_items, date="2026-01-01")
        loaded = started_storage.load_items(date="2026-01-01")
        assert len(loaded) == len(sample_items)

    @pytest.mark.asyncio
    async def test_load_items_empty（self, started_storage):
        items = started_storage.load_items(date="2099-01-01")
        assert items == []

    @pytest.mark.asyncio
    async def test_load_items_filter_platform(self, started_storage, sample_items):
        started_storage.save_items(sample_items, date="2026-01-01")
        tw = started_storage.load_items(date="2026-01-01", platform="twitter")
        assert all(i.platform == "twitter" for i in tw)

    # ── report 往返 ────────────────────────

    @pytest.mark.asyncio
    async def test_save_and_load_report(self, started_storage):
        started_storage.save_report("# Hello", date="2026-01-01")
        assert started_storage.load_report(date="2026-01-01") == "# Hello"

    @pytest.mark.asyncio
    async def test_load_report_missing(self, started_storage):
        assert started_storage.load_report(date="2099-01-01") is None

    # ── summary 往返 ──────────────────────

    @pytest.mark.asyncio
    async def test_save_and_load_summary(self, started_storage):
        started_storage.save_summary({"count": 5}, date="2026-01-01")
        assert started_storage.load_summary(date="2026-01-01") == {"count": 5}

    # ── list_dates ────────────────────────

    @pytest.mark.asyncio
    async def test_list_dates(self, started_storage, sample_items):
        started_storage.save_items(sample_items, date="2026-01-02")
        started_storage.save_items(sample_items, date="2026-01-01")
        dates = started_storage.list_dates()
        assert dates == ["2026-01-02", "2026-01-01"]

    @pytest.mark.asyncio
    async def test_list_dates_filters_sessions(self, started_storage, tmp_path):
        (tmp_path / "data" / "sessions").mkdir(parents=True)
        started_storage.save_items([], date="2026-01-01")
        assert "sessions" not in started_storage.list_dates()
```

---

## 8. UT 测试模板（以 Config Models 为例）

```python
import pytest
from pydantic import ValidationError
from src.wheel.config.config_models import ServerConfig, LLMConfig, Config


class TestServerConfigUT:
    def test_defaults(self):
        cfg = ServerConfig()
        assert cfg.port == 7890
        assert cfg.auto_open_browser is True

    def test_custom(self):
        cfg = ServerConfig(port=3000)
        assert cfg.port == 3000


class TestLLMConfigUT:
    def test_defaults(self):
        cfg = LLMConfig()
        assert cfg.provider == "deepseek"
        assert cfg.model == "deepseek-chat"

    def test_api_key_required_for_full_config(self):
        cfg = LLMConfig(api_key="sk-xxx")
        assert cfg.api_key == "sk-xxx"


class TestConfigUT:
    def test_empty_config(self):
        cfg = Config()
        assert cfg.server.port == 7890
        assert cfg.llm.provider == "deepseek"
        assert cfg.platforms == {}
        assert cfg.tasks == []

    def test_validate_from_dict(self):
        data = {
            "server": {"port": 9999},
            "llm": {"provider": "openai", "api_key": "sk-test"},
        }
        cfg = Config.model_validate(data)
        assert cfg.server.port == 9999
        assert cfg.llm.provider == "openai"
```

---

## 9. 运行命令

```bash
# 全部测试
pytest tests/ -v

# 仅 UT
pytest tests/ -v -m "not asyncio"

# 仅 TA (需要异步支持)
pytest tests/ -v -m asyncio

# 带覆盖率
pytest tests/ -v --cov=src --cov-report=term-missing
```

---

## 10. 测试检查清单

编写每个模块时，确认：

- [ ] UT 覆盖 `_models.py` — 至少 创建默认值 + 序列化/反序列化
- [ ] UT 覆盖 `_factory.py` — 至少 3 种 create 入参
- [ ] TA 覆盖 `imp/{impl}.py` — 生命周期 + 所有 public 方法 × (正常 + 空 + 异常)
- [ ] TA 包含往返验证 — 写后立即读，数据一致
- [ ] 使用 `tmp_path` 隔离文件系统
- [ ] async 方法用 `@pytest.mark.asyncio`
- [ ] `conftest.py` 中提取公共 fixture 避免重复

---

## 11. 测试文档规范

> 每个模块完成测试后，必须在 `doc/tests/` 下产出两份文档。

### 目录结构（与 `src/` 镜像）

```
doc/tests/
├── common/
│   ├── 测试方案.md          # common/models UT
│   └── 测试结果.md
├── wheel/
│   ├── config/
│   │   ├── 测试方案.md      # config 模块 UT + TA
│   │   └── 测试结果.md
│   ├── storage/
│   │   ├── 测试方案.md      # storage 模块 UT + TA
│   │   └── 测试结果.md
│   └── browser/
│       ├── 测试方案.md      # browser 模块 UT + TA
│       └── 测试结果.md
└── core/
    └── adapters/
        ├── 测试方案.md      # adapters 模块 UT + TA
        └── 测试结果.md
```

### 测试方案.md 模板

```markdown
# {模块名} 测试方案

## 1. 被测目标
| 源文件 | 类型 | 测试文件 |
|--------|------|---------|
| ... | UT/TA | ... |

## 2. 测试点清单
### {类名/方法组}
- [ ] 场景1 — 描述
- [ ] 场景2 — 描述

## 3. 公共依赖
- fixture: xxx — 用途
- monkeypatch: xxx — 用途

## 4. 运行命令
pytest tests/{path}/ -v
```

### 测试结果.md 模板

```markdown
# {模块名} 测试结果

## 1. 汇总
| 总数 | 通过 | 失败 | 覆盖率 |
|------|------|------|--------|
| N | N | 0 | XX% |

## 2. 用例明细
| 用例名 | 输入 | 预期 | 状态 |
|--------|------|------|------|
| test_xxx | ... | ... | ✅ |

## 3. 未覆盖说明
| 文件:行号 | 原因 |
|-----------|------|
| ... | 错误路径/不可达 |
```

### 文档检查清单

- [ ] 每个模块 `doc/tests/{module}/` 下存在 `测试方案.md`
- [ ] 每个模块 `doc/tests/{module}/` 下存在 `测试结果.md`
- [ ] 方案.md 的测试点清单与测试文件一一对应
- [ ] 结果.md 的用例明细表完整列出所有用例
- [ ] 未覆盖行有合理解释

---

*文档版本：v1.2 | 最后更新：2026-04-25*
