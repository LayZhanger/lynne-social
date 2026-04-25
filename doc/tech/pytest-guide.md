# pytest 使用指南

> 面向 Lynne 项目的测试体系，介绍 pytest 的核心概念和本项目中的惯用写法。

---

## 1. 核心概念

### fixture — 依赖注入

代替 `setUp`/`tearDown`，通过函数参数注入依赖。

```python
import pytest

@pytest.fixture
def storage(tmp_path):
    """tmp_path 是 pytest 内置 fixture，提供临时目录"""
    return StorageFactory().create(StorageConfig(data_dir=str(tmp_path / "data")))

def test_name(storage):         # ← storage 自动注入
    assert storage.name == "JsonlStorage"
```

| fixture | 来源 | 用途 |
|---------|------|------|
| `tmp_path` | 内置 | 隔离文件系统，每个测试独立目录 |
| `monkeypatch` | 内置 | 临时修改环境变量、属性 |
| `sample_item` | `conftest.py` 自定义 | 跨测试文件共享 |

fixture 可以依赖其他 fixture：
```python
@pytest.fixture
async def started_storage(self, storage):  # ← 依赖 storage fixture
    await storage.start()
    return storage
```

### conftest.py — 全局 fixture

项目根 `tests/conftest.py` 中的 fixture 对同级及子目录所有测试文件可见。
子目录的 `conftest.py` 仅对该子目录可见，允许覆盖。

### mark — 标签/标记

```python
@pytest.mark.asyncio    # 声明为异步测试
async def test_start(self, storage):
    await storage.start()
    assert storage.health_check()
```

常用 mark：
- `@pytest.mark.asyncio` — 异步测试（需要 `pytest-asyncio`）
- `@pytest.mark.skip(reason="...")` — 跳过
- `@pytest.mark.parametrize("input,expected", [(1,2), (2,4)])` — 参数化

---

## 2. 断言

pytest 用纯 `assert`，失败时自动展开对比：

```python
# 基础断言
assert cfg.port == 7890

# 类型断言
assert isinstance(loader, ConfigLoader)

# 异常断言
with pytest.raises(ValueError, match="未设置"):
    _resolve_env("${MISSING_VAR}")

# 集合断言
assert "sessions" not in storage.list_dates()
assert all(i.platform == "twitter" for i in items)
```

---

## 3. 测试组织

### 文件命名

```
tests/
├── common/
│   └── test_common_models.py    # test_ 前缀 → 自动发现
├── wheel/
│   └── config/
│       └── test_config_impl.py
```

- 文件名必须 `test_*.py` 或 `*_test.py`
- 测试类必须 `Test*` 前缀
- 测试函数/方法必须 `test_*` 前缀

### 类组织

```python
class TestUnifiedItemUT:        # 一个类对应一个被测对象
    def test_create_defaults(self):  # 每个方法测试一个场景
        ...

    def test_from_dict_empty(self):
        ...
```

---

## 4. 异步测试（pytest-asyncio）

Lynne 使用 `asyncio_mode = "auto"`（配置在 `pyproject.toml`）。

三种模式：

| 模式 | 行为 |
|------|------|
| `strict` | 只有 `@pytest.mark.asyncio` 标记的测试才用事件循环 |
| `auto` | **本项目使用** — 自动检测，async 函数自动获得事件循环 |

异步 fixture 也自动支持：
```python
@pytest.fixture
async def started_storage(self, storage):
    await storage.start()       # await 正常工作
    return storage

@pytest.mark.asyncio
async def test_save(self, started_storage):
    started_storage.save_items(...)  # 不需要 await (save_items 是同步的)
```

---

## 5. monkeypatch — 环境变量注入

```python
def test_env_substitution(self, monkeypatch):
    monkeypatch.setenv("API_KEY", "sk-test")
    # 后续代码读取 os.environ["API_KEY"] → "sk-test"
    # 测试结束后自动恢复原值
```

适用于：
- 注入 API Key（避免依赖真实环境变量）
- 临时修改模块属性
- 替换函数实现

---

## 6. tmp_path — 文件系统隔离

```python
def test_save(self, tmp_path):
    # tmp_path 是 pathlib.Path，每个测试独立
    yaml_path = tmp_path / "config.yaml"
    yaml_path.write_text("server:\n  port: 9999\n")

    loader = YamlConfigLoader(str(yaml_path))
    cfg = loader.load()
    assert cfg.server.port == 9999
    # 测试结束，tmp_path 自动清理
```

---

## 7. 覆盖率

```bash
# 生成覆盖率报告
pytest tests/ -v --cov=src --cov-report=term-missing

# HTML 报告
pytest tests/ -v --cov=src --cov-report=html
# → htmlcov/index.html
```

输出示例：
```
Name                    Stmts  Miss  Cover   Missing
----------------------------------------------------
src/common/models.py       41     0   100%
src/wheel/.../impl.py      73     3    96%   34,51,85
```

- `Stmts` — 可执行语句数
- `Miss` — 未执行语句数
- `Missing` — 具体未覆盖行号

---

## 8. 运行命令速查

```bash
# 全部测试
pytest tests/ -v

# 单个文件
pytest tests/common/test_common_models.py -v

# 单个类
pytest tests/common/test_common_models.py::TestUnifiedItemUT -v

# 单个用例
pytest tests/common/test_common_models.py::TestUnifiedItemUT::test_from_dict_empty -v

# 仅异步测试
pytest tests/ -v -k "asyncio"

# 首次失败即停止
pytest tests/ -v -x

# 并行执行（需 pip install pytest-xdist）
pytest tests/ -v -n auto
```

---

## 9. Lynne 项目约定

| 约定 | 说明 |
|------|------|
| 测试文件镜像 `src/` 结构 | `src/common/models.py` → `tests/common/test_common_models.py` |
| TA 用 `tmp_path` 隔离 | 不碰项目根目录，不依赖真实文件 |
| 工厂创建被测对象 | `StorageFactory().create(StorageConfig(...))` |
| UT 测 models/factory, TA 测 impl | models 纯内存，impl 全链路 |
| `asyncio_mode = "auto"` | `pyproject.toml` 中配置 |
| 公共 fixture 放 `conftest.py` | `sample_item`, `sample_items`, `sample_yaml_file` |

---

*最后更新：2026-04-25*
