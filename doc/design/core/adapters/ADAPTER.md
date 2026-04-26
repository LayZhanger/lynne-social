# ADAPTER — Platform Adapter 模块设计

> 本文档定义 `core/adapters/` 模块的接口设计、架构边界、使用指南和测试策略。

---

## 1. 模块定位

| 维度 | 说明 |
|------|------|
| **所属层** | `core/`（引擎层） |
| **职责** | 平台数据采集 + LLM 驱动的自适应内容提取 |
| **依赖** | `BrowserManager`（获取浏览器上下文/页面），`LLMEngine`（v4.2 新增，通过 LLMAdapter 中间层），`common/`（UnifiedItem） |
| **被依赖** | `Agent`（通过 SearchTool 调用 Adapter 执行采集） |
| **不继承 Module** | Adapter 是**短生命周期工具**，每次采集内部获取 context → 创建 page → 操作 → 关闭 page。`start/stop` 由 BrowserManager 和 LLMAdapter 内部的 LLMEngine 分别管理 |

---

## 2. 接口定义（ABC）

### 2.1 BaseAdapter

```python
class BaseAdapter(ABC):
    platform_name: str  # 类属性

    async def search(keywords: list[str], limit: int) -> AsyncIterator[UnifiedItem]: ...
    async def get_user_posts(user_id: str, limit: int) -> AsyncIterator[UnifiedItem]: ...
    async def get_trending(limit: int) -> AsyncIterator[UnifiedItem]: ...
    def extract(data: dict) -> UnifiedItem: ...
    async def health_check() -> bool: ...
```

| 方法 | 异步 | 返回 | 语义 |
|------|------|------|------|
| `search(kw, limit)` | ✅ | `AsyncIterator[UnifiedItem]` | 关键词搜索，流式返回统一格式数据 |
| `get_user_posts(uid, limit)` | ✅ | `AsyncIterator[UnifiedItem]` | 获取指定用户的最新帖子 |
| `get_trending(limit)` | ✅ | `AsyncIterator[UnifiedItem]` | 获取平台热门内容 |
| `extract(data)` | 否 | `UnifiedItem` | 将原始数据 dict 映射为 `UnifiedItem` |
| `health_check()` | ✅ | `bool` | 检查平台页面是否可达 |

### 2.2 LLMAdapter（v4.2 新增）

```python
class LLMAdapter(BaseAdapter, ABC):
    """所有需要 LLM 驱动提取的平台适配器的中间父类。"""

    def __init__(self, browser, config, *, llm_config=None, **url_kwargs):
        self._browser = browser
        self._config = config
        self._llm_config: LLMConfig | None = llm_config
        self._llm: LLMEngine | None = None
        self._extract_fn_cache: dict[str, str] = {}

    async def _ensure_llm(self): ...
    async def _get_or_generate_extract_fn(self, page) -> str | None: ...
    async def _scroll_and_extract(self, page, extract_fn, limit) -> AsyncIterator[UnifiedItem]: ...
```

LLMAdapter 封装了 LLM 驱动的自适应内容提取：
- **懒创建 LLMEngine**：`_ensure_llm()` 从 `self._llm_config` 通过 `LLMEngineFactory` 创建，不读文件
- **骨架提取**：通用 `_SKELETON_JS` 遍历 DOM 取结构样本 → LLM 生成 `extractPosts()` → `page.evaluate`
- **缓存**：按 URL host + path 缓存 JS 提取函数，后续翻页零 LLM 调用
- **CSS 回退**：`llm_config=None` 时走 CSS 选择器路径

类层次：
```
BaseAdapter(ABC)
  └─ LLMAdapter(BaseAdapter, ABC)       ← 中间层（LLM 提取通用逻辑）
       └─ RedNoteAdapter(LLMAdapter)    ← 平台 shell
       └─ (未来) TwitterAdapter         ← 平台 shell
```

---

## 3. 配置模型

### 3.1 AdapterConfig（平台无关）

```python
@dataclass
class AdapterConfig:
    platform: str = ""              # 平台标识（discriminator），必填
    max_scrolls: int = 20           # 最大滚动次数
    scroll_delay: float = 1.5       # 每次滚动间隔（秒）
    page_timeout: int = 30000       # 页面加载超时（ms）
```

v4.2 变更：删除了 RedNote 特定的 URL 字段（`base_url`、`search_url`、`user_url_template`、`trending_url`）。URL 现在是各平台的类常量（如 `RedNoteAdapter._SEARCH_URL`）。

### 3.2 LLMConfig（来自 wheel/llm，由 main.py 注入）

```python
# 来自 src/wheel/llm/llm_models.py
@dataclass
class LLMConfig:
    provider: str = "deepseek"
    api_key: str = ""
    base_url: str = ""
    model: str = "deepseek-chat"
    temperature: float = 0.7
    max_tokens: int = 4096
    timeout: int = 60
```

### 3.3 Config 传递链

```
config.yaml → YamlConfigLoader.load()  ← 唯一读文件的地方
  → Config.llm (Pydantic)
    → main.py: LLMConfig dataclass 转换
      → AdapterFactory.__init__(..., llm_config=LLMConfig)  ← 只传不建
        → LLMAdapter.__init__(..., llm_config=LLMConfig)  ← 只存不读
          → _ensure_llm(): LLMEngineFactory().create(self._llm_config)  ← 父类自建
```

---

## 4. 使用指南

### 4.1 CSS 路径（无 LLM，当前可用）

```python
from src.core.adapters import AdapterFactory
from src.wheel.browser import BrowserManagerFactory, BrowserConfig

browser = BrowserManagerFactory().create(BrowserConfig(headless=True))
await browser.start()

adapter = AdapterFactory().create(
    browser,
    AdapterConfig(platform="rednote"),
    llm_config=None,                            # 不传 → CSS 路径
)

async for item in adapter.search(["AI 模型"], limit=20):
    print(item.content_text)

await browser.stop()
```

### 4.2 LLM 路径（v4.2 新增）

```python
from src.wheel.llm.llm_models import LLMConfig

llm_cfg = LLMConfig(api_key="sk-xxx", model="deepseek-chat")

adapter = AdapterFactory().create(
    browser,
    AdapterConfig(platform="rednote"),
    llm_config=llm_cfg,                        # 传入 → LLM 路径
)

async for item in adapter.search(["AI 模型"], limit=20):
    # 内部自动：骨架提取 → LLM 生成 extractPosts() → 缓存 → 批量提取
    print(item.content_text)
```

### 4.3 数据映射

```python
# 外部数据 → extract() → UnifiedItem
data = {
    "item_id": "N001",
    "author_name": "用户A",
    "title": "标题文本",
    "likes": 1234,
}
item = adapter.extract(data)
# → UnifiedItem(platform="rednote", item_id="N001", ...)
```

`extract()` 方法在 CSS 路径和 LLM 路径下共用——`page.evaluate(extractPosts)` 的输出 dict 和 CSS 选择器的输出 dict 都用同一个 `extract()` 映射。

---

## 5. 实现方案

### 5.1 LLM 提取协议

```
page 加载
  → 通用 _SKELETON_JS 提取前 3 个帖子的结构骨架（tag + cls + txt）  [~500 tokens]
  → LLM 分析骨架，生成 JS extractPosts() 函数                      [1 次 LLM 调用]
  → page.evaluate(extractPosts) 批量提取全部帖子                    [零 LLM，规则执行]
  → 缓存按 URL host + path，后续翻页直接复用
```

三层数据格式：

**输入：页面骨架**
```json
[{
  "tag": "section", "cls": "note-item",
  "children": [
    {"sel": "div.title>span",       "txt": "GPT-5 发布"},
    {"sel": "div.author span.name", "txt": "科技观察者"},
    {"sel": "div.desc",             "txt": "本文分析..."},
    {"sel": "span.count",           "txt": "12,000"},
    {"sel": "a[href*='/explore/']", "href": "/explore/123"},
    {"sel": "img",                  "src": "https://pic.jpg"}
  ]
}, ...]
```

**输出：JS 提取函数（LLM 生成）**
```javascript
function extractPosts() {
  return [...document.querySelectorAll('.note-item')].map(el => ({
    item_id: el.getAttribute('data-id') || '',
    title: el.querySelector('.title span')?.textContent?.trim() || '',
    author_name: el.querySelector('.author .name')?.textContent?.trim() || '',
    content: el.querySelector('.desc')?.textContent?.trim() || '',
    likes: (el.querySelector('.count')?.textContent||'0').replace(/,/g,''),
    url: ..., images: ...
  }));
}
```

**输出：UnifiedItem** — JS 返回值 → `extract(data)` → `UnifiedItem`

### 5.2 RedNoteAdapter（平台 shell）

继承 `LLMAdapter`，提供：
- 平台 URL 常量（`_SEARCH_URL`、`_USER_URL_TEMPLATE`、`_TRENDING_URL`、`_BASE_URL`）
- CSS 选择器常量（`_NOTE_ITEM` 等，fallback 用）
- `search()` / `get_user_posts()` / `get_trending()` — 调 `LLMAdapter._get_or_generate_extract_fn()`
- `extract()` — 映射逻辑（CSS 和 LLM 路径共用）

### 5.3 防封策略（分层）

| 层级 | 负责模块 | 措施 |
|------|----------|------|
| 全局 | BrowserManager | slow_mo、stealth 注入、统一 viewport/locale |
| 平台级 | AdapterConfig | scroll_delay、max_scrolls |
| 调度级 | Agent | 平台间暂停、重试 |

---

## 6. Factory 设计

```python
class AdapterFactory:
    """无状态分发器。平台标识 → 适配器实现。"""

    def create(self, browser: BrowserManager, config: AdapterConfig,
               *, llm_config: LLMConfig | None = None) -> BaseAdapter:
        if config.platform == "rednote":
            return RedNoteAdapter(browser, config, llm_config=llm_config)
        raise ValueError(f"unknown platform: {config.platform}")
```

- 零构造注入 —— `AdapterFactory` 是纯函数类
- `browser` 由外部（`main.py`）创建后传入，工厂不持有
- `llm_config` 只传不建；Engine 由 `LLMAdapter._ensure_llm()` 懒创建
- 返回类型是 `BaseAdapter`，调用方不感知具体实现

---

## 7. 错误处理

| 场景 | 行为 |
|------|------|
| `page.goto()` 网络错误 | 异常传播给调用方 `async for` |
| 页面无帖子元素 | 静默返回（yield 0 条） |
| 单条帖子 DOM 损坏 | `_scrape_element` 返回 None → 跳过 |
| LLM 生成 extractPosts 失败 | 返回 None → 自动 fallback 到 CSS 路径 |
| LLM 生成的 JS 不可执行 | `page.evaluate` 抛异常 → 记录 log → fallback CSS |
| 重复帖子 | 按 `item_id` 去重 |
| `health_check()` 失败 | 捕获所有异常 → 返回 False |
| stealth 不可用（ImportError） | 静默跳过，日志 debug |

---

## 8. 测试策略

### UT（纯内存）
- **models**: `AdapterConfig` 默认值、自定义值、等值比较
- **factory**: 创建、未知平台异常、非 AdapterConfig 输入、llm_config 透传
- **extract**: 全字段映射、最小输入、备用键名回退
- **_parse_int**: 万/k/逗号/负数/空字符串转换

### TA（真实 Chromium headless + HTML 测试页）
- **search + extract**: 12 项全体提取、limit=3 截断、空页面
- **scroll 懒加载**: 15 项分 4 批展示
- **去重**: 无重复 ID 验证
- **各方法端到端**: search / get_user_posts / get_trending
- **health_check**: OK（文件 URL）和 fail（不可达端口）
- **stealth 注入**: 验证正常运行
- **LLM 提取**（待 LLMAdapter 完成后）：Mock LLM → 验证 extractPosts 生成和缓存

---

*文档版本：v2.0 | 最后更新：2026-04-26*
