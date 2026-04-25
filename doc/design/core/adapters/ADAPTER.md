# ADAPTER — Platform Adapter 模块设计

> 本文档定义 `core/adapters/` 模块的接口设计、架构边界、使用指南和测试策略。

---

## 1. 模块定位

| 维度 | 说明 |
|------|------|
| **所属层** | `core/`（引擎层） |
| **职责** | 平台特定的数据采集：从各社交平台获取原始数据，转为统一格式 `UnifiedItem` |
| **依赖** | `BrowserManager`（获取浏览器上下文/页面），`common/`（UnifiedItem） |
| **被依赖** | 上层 `Orchestrator` 调用 Adapter 执行采集 |
| **不继承 Module** | Adapter 是**短生命周期工具**，由 `search/get_user_posts/get_trending` 每次调用内部获取 context → 创建 page → 操作 → 关闭 page。`start/stop` 生命周期由 `BrowserManager` 管理 |

---

## 2. 接口定义（ABC）

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

---

## 3. 使用指南

### 3.1 标准采集流程

```python
from src.core.adapters import AdapterFactory
from src.core.browser import BrowserManagerFactory, BrowserConfig

browser = BrowserManagerFactory().create(BrowserConfig(headless=True))
await browser.start()

adapter = AdapterFactory(
    browser_factory=BrowserManagerFactory(),
    browser_config=BrowserConfig(headless=True),
).create(None)  # default → rednote

async for item in adapter.search(["AI 模型"], limit=20):
    print(item.content_text)

async for item in adapter.get_user_posts("user_123", limit=10):
    print(item.author_name)

await browser.stop()
```

### 3.2 自定义配置

```python
from src.core.adapters import AdapterConfig

config = AdapterConfig(
    platform="rednote",
    search_url="https://www.xiaohongshu.com/search_result?keyword={keyword}",
    max_scrolls=30,
    scroll_delay=2.0,
    page_timeout=45000,
)
adapter = factory.create(config)
```

### 3.3 数据映射

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

---

## 4. 配置模型

```python
@dataclass
class AdapterConfig:
    platform: str = "rednote"
    base_url: str = "https://www.xiaohongshu.com"
    search_url: str = "https://www.xiaohongshu.com/search_result?keyword={keyword}"
    user_url_template: str = "https://www.xiaohongshu.com/user/profile/{user_id}"
    trending_url: str = "https://www.xiaohongshu.com/explore"
    max_scrolls: int = 20          # 最大滚动次数
    scroll_delay: float = 1.5      # 每次滚动间隔（秒）
    page_timeout: int = 30000      # 页面加载超时（ms）
```

URL 模板支持 `{keyword}` 和 `{user_id}` 占位符，运行时用 `urllib.parse.quote` 替换。

---

## 5. 实现方案

### 5.1 RedNoteAdapter（DOM 选择器 + 滚动加载）

**采集流程**：
1. 从 `BrowserManager.get_context("rednote")` 获取上下文
2. `ctx.new_page()` 创建新页面
3. 注入 stealth（`playwright_stealth.Stealth().apply_stealth_async`），静默跳过 ImportError
4. `page.goto(url)` 导航到搜索/用户/趋势页面
5. 循环滚动 + 提取：
   - `page.raw().query_selector_all(NOTE_ITEM)` 获取当前可见笔记元素
   - 对每个元素调用 `_scrape_element(el) → dict` 提取 DOM 数据
   - `extract(data) → UnifiedItem` 映射为统一格式
   - 去重（按 `item_id`）
   - `window.scrollBy(0, innerHeight)` 滚动
   - 检测新内容是否出现（3 次无新内容则停止）
6. `finally` 块关闭页面

**CSS 选择器**（类常量，不在 `AdapterConfig` 中）：
- `NOTE_ITEM`: `section.note-item, .note-item, article`
- `NOTE_TITLE`: `.title span, .note-title span, .desc, h3 a`
- `NOTE_AUTHOR`: `.author .name, .name, .nickname, .username`
- `NOTE_CONTENT`: `.desc, .note-text, .content`
- `NOTE_LIKES`: `.like-wrapper .count, .count, .like-count`
- `NOTE_COVER`: `.cover img, .img-container img, .feeds-page img`
- `NOTE_LINK`: `a[href*='/explore/'], a[href*='/discovery/']`

**like 数字解析**：支持中文 `1.2万`（×10000）、英文 `3.5k`（×1000）、逗号 `1,234`。

### 5.2 防封策略（分层）

| 层级 | 负责模块 | 措施 |
|------|----------|------|
| 全局 | BrowserManager | slow_mo（操作间隔）、stealth 注入、统一 viewport/locale |
| 平台级 | PlatformAdapter | scroll_delay（滚动间隔）、max_scrolls（上限） |
| 调度级 | Orchestrator | 平台间暂停、重试、风控检测 |

---

## 6. Factory 设计

```python
class AdapterFactory(Factory[BaseAdapter]):
    def __init__(self, browser_factory: Factory[BrowserManager], browser_config: BrowserConfig):
        # 注入 BrowserManager 工厂（不是实例）
        # 注入 BrowserConfig（浏览器启动参数）

    def create(self, config: object) -> BaseAdapter:
        # 根据 config.platform 选择实现
        # 创建 BrowserManager 实例 → 传入 Adapter
```

Factory 接收 `Factory[BrowserManager]` 而非 `BrowserManager` 实例，遵循依赖反转原则。

---

## 7. 错误处理

| 场景 | 行为 |
|------|------|
| page.goto() 网络错误 | 异常传播给调用方 `async for` |
| 页面无笔记元素 | 静默返回（yield 0 条） |
| 单条笔记 DOM 损坏 | `_scrape_element` 返回 None → 跳过 |
| 重复笔记 | 按 `item_id` 去重 |
| `health_check()` 失败 | 捕获所有异常 → 返回 False |
| stealth 不可用（ImportError） | 静默跳过，日志 debug |

---

## 8. 测试策略

### UT（纯内存）
- **models**: `AdapterConfig` 默认值、自定义值、等值比较、URL 模板插值
- **factory**: 创建 `RedNoteAdapter`、未知平台异常、非 AdapterConfig 输入
- **extract**: 全字段映射、最小输入、备用键名回退
- **_parse_int**: 万/k/逗号/负数/空字符串转换

### TA（真实 Chromium headless + HTML 测试页）
- **search + extract**: 12 项全体提取、limit=3 截断、空页面
- **scroll 懒加载**: 15 项分 4 批展示（`start_hidden=3, batch=4`）
- **去重**: 无重复 ID 验证
- **各方法端到端**: search / get_user_posts / get_trending
- **health_check**: OK（文件 URL）和 fail（不可达端口）
- **stealth 注入**: 验证正常运行（不测试 stealth 效果本身）
- **无 data-id 回退**: item_id 回退到 title

测试使用自建 HTML 页面（`file://` URL），嵌入 JavaScript 模拟无限滚动懒加载。

---

*文档版本：v1.0 | 最后更新：2026-04-25*
