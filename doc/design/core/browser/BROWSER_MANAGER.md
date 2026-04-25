# BrowserManager 详细设计

> 所属层：`core/` 业务层 — 浏览器引擎
> 依赖：`wheel/logger`（日志），无其他 core 模块依赖
> 源码：`src/core/browser/`
>
> **本文分两部分**：第 1-3 节定义抽象规范（与实现无关），第 4-5 节描述当前 Playwright 实现。

---

## 1. 模块定位

BrowserManager 是浏览器引擎的抽象。它定义了 "管理浏览器生命周期、为各平台提供隔离的浏览会话" 的**能力契约**，不绑定任何特定浏览器驱动。

上层模块（PlatformAdapter、Orchestrator）依赖**接口**，不关心底层是 Playwright / Selenium / CDP，换实现只需换工厂。

### 1.1 边界定义

```
                           ┌─────────────────────────┐
                           │     BrowserManager       │
                           │     (抽象接口)             │
                           │                          │
 BrowserConfig ──────────► │  启动 / 停止浏览器引擎     │
                           │                          │
 platform: str ──────────► │  获取/创建浏览上下文       │ ──► 浏览上下文对象
                           │  每个平台一个，互不干扰     │
 platform: str ──────────► │                          │
                           │  会话持久化               │ ──► 会话文件（落盘）
                           │  （从文件恢复 / 写回文件）  │
 (platform, url) ────────► │                          │
                           │  登录引导                 │ ──► 浏览器窗口打开目标页
                           │  （打开页面，等用户操作）    │
                           └─────────────────────────┘
```

> 注：`BrowserContext` 和 `BrowserPage` 是本模块自己定义的 ABC，不依赖任何浏览器驱动。Adapter 通过 `.raw()` 拿到真实驱动对象后再做具体页面操作。抽象归 BrowserManager，实现归 `imp/`。

**输入 — 模块从外部获得什么**：

| 输入 | 来源 | 类型 | 说明 |
|------|------|------|------|
| `BrowserConfig` | 工厂 `create(config)` | dataclass | 是否无头、操作延迟、视口尺寸、语言、超时、会话目录 |
| `platform` | 调用方每次传参 | `str` | 平台标识，如 `"twitter"`、`"rednote"` |
| `url` | 调用方（API 层通过 PlatformConfig 获取） | `str` | `login_flow` 时导航的目标地址 |

**输出 — 模块对外产出什么**：

| 输出 | 去向 | 类型 | 说明 |
|------|------|------|------|
| 浏览上下文 | PlatformAdapter | 浏览器驱动原生上下文对象 | 已加载历史会话（如存在）、且完成反检测注入的隔离上下文。Adapter 用它 `new_page()` 开始工作 |
| 会话文件 | 本地文件系统 | JSON 文件 | 路径 `{sessions_dir}/{platform}_state.json`，存储 cookies + localStorage 以实现免登录 |
| 登录页面 | 浏览器窗口 | 可视化 | `login_flow` 在浏览器中打开目标 URL，供用户手动完成登录 |

**边界外 — 模块不管的事情**：

| 不管 | 谁管 | 说明 |
|------|------|------|
| 页面操作（click、scroll、input） | PlatformAdapter | 上下文交给 Adapter 后，Adapter 自行操作 |
| 数据提取（DOM / API 拦截） | PlatformAdapter | 平台特定逻辑 |
| 风控识别与应对 | PlatformAdapter 感知 → 上报 Orchestrator | BrowserManager 不检测验证码/滑块 |
| 登录表单交互 | 用户手动 | BrowserManager 只把页面打开，不替用户填表单 |
| 何时启动/停止 | Scheduler / API / Orchestrator | BrowserManager 被动响应 `start()` / `stop()` |

### 1.2 核心职责（抽象）

1. **生命周期** — 启动和关闭浏览器引擎
2. **上下文隔离** — 为每个平台创建独立浏览上下文，cookies/localStorage 互不污染
3. **会话恢复** — 如果上次的会话文件存在，加载之（免登录）；不存在则创建全新上下文
4. **会话保存** — 将当前上下文的认证状态持久化到文件
5. **登录引导** — 打开平台登录页，等待用户手动完成，完成后保存会话
6. **反检测** — 在上下文中注入反自动化检测措施

---

## 2. 接口定义（ABC）

```python
from abc import ABC, abstractmethod
from typing import Any

from src.common import Module


class BrowserPage(ABC):
    @abstractmethod
    async def goto(self, url: str, **kwargs) -> None: ...

    @abstractmethod
    async def close(self) -> None: ...

    @abstractmethod
    def raw(self) -> Any: ...


class BrowserContext(ABC):
    @abstractmethod
    async def new_page(self) -> BrowserPage: ...

    @abstractmethod
    async def close(self) -> None: ...

    @abstractmethod
    async def storage_state(self) -> dict: ...

    @property
    @abstractmethod
    def pages(self) -> list[BrowserPage]: ...

    @abstractmethod
    def raw(self) -> Any: ...


class BrowserManager(Module):
    @abstractmethod
    async def get_context(self, platform: str) -> BrowserContext: ...

    @abstractmethod
    async def save_session(self, platform: str) -> None: ...

    @abstractmethod
    async def login_flow(self, platform: str, url: str) -> None: ...

    @abstractmethod
    async def set_login_complete(self, platform: str) -> None: ...
```

继承 `Module`，额外定义 4 个方法，模块还定义 `BrowserPage` 和 `BrowserContext` 两个辅助 ABC：

| 类型 | 方法 | 异步 | 语义 |
|------|------|------|------|
| `BrowserPage` | `goto(url)` | ✅ | 导航到目标地址 |
| | `close()` | ✅ | 关闭标签页 |
| | `raw()` | 否 | 返回底层浏览器驱动的真实 Page 对象 |
| `BrowserContext` | `new_page()` | ✅ | 创建新标签页 → BrowserPage |
| | `close()` | ✅ | 关闭上下文及所有页面 |
| | `storage_state()` | ✅ | 导出 cookies + localStorage → dict |
| | `pages` | 否 | 当前所有打开的标签页列表（property） |
| | `raw()` | 否 | 返回底层浏览器驱动的真实 Context 对象 |
| `BrowserManager` | `start()` | ✅ | 继承自 `Module`。启动浏览器引擎 |
| | `stop()` | ✅ | 继承自 `Module`。关闭所有上下文 → 浏览器 → 引擎 |
| | `health_check()` | 否 | 继承自 `Module`。引擎是否可用 |
| | `get_context(platform)` | ✅ | 获取指定平台的浏览上下文（缓存） |
| | `save_session(platform)` | ✅ | 持久化当前认证状态 |
| | `login_flow(platform, url)` | ✅ | 打开登录页，进入等待状态 |
| | `set_login_complete(platform)` | ✅ | 确认登录完成，保存会话 |

---

## 3. 怎么用这个接口

### 3.1 拿到实例

```python
from src.core.browser import BrowserManagerFactory, BrowserConfig

cfg = BrowserConfig(headless=True, sessions_dir="data/sessions")
browser = BrowserManagerFactory().create(cfg)
# browser 的类型是 BrowserManager（接口），不知道底层是 Playwright 还是 Selenium
```

### 3.2 标准采集流程

```python
await browser.start()                              # Step 1: 启动浏览器（幂等，已启则直接返回）

ctx = await browser.get_context("twitter")         # Step 2: 拿隔离上下文（第二次调同 platform 命中缓存）
pw = ctx.raw()                                     #        解包真实 Playwright 对象
page = await pw.new_page()                         # Step 3: 开标签页
await page.goto("https://x.com/search?...")        # Step 4: 导航 → DOM 操作 → 提取
# ... adapter 做具体采集 ...

await browser.save_session("twitter")              # Step 5: 持久化会话（可选，保持登录态）

# stop() 不在这里调 — browser 保持长生命，由上层（Orchestrator / API）决定何时停
```

### 3.3 登录流程

```python
# ── API 层收到用户点「登录 twitter」──
await browser.login_flow("twitter", "https://x.com/login")
# 浏览器里打开登录页，方法立即返回。用户在浏览器窗口里手动填表单/扫码。

# ── 用户在前端点「我登录好了」──
await browser.set_login_complete("twitter")
# 保存会话 → 关登录页 → 下次 get_context("twitter") 自动恢复登录态
```

两步之间用户手动操作，BrowserManager 不碰表单。

### 3.4 多平台跑完才停

```python
await browser.start()

for platform in ["twitter", "rednote"]:
    ctx = await browser.get_context(platform)
    # ... adapter 采集 ...
    await browser.save_session(platform)

await browser.stop()        # 全部采完才停，不是采一个平台停一次
```

### 3.5 注意事项

| 要点 | 说明 |
|------|------|
| `start()` 幂等 | 已启动时直接 return，调多少次都安全 |
| `get_context` 有缓存 | 同一 platform 第二次直接返回缓存，不重复创建 |
| `get_context` 前必须 `start()` | 否则 `RuntimeError("not started")` |
| `get_context` 不创建标签页 | 只返回上下文对象；开标签页是 `ctx.raw().new_page()` 的事 |
| 会话文件路径固定 | `{sessions_dir}/{platform}_state.json`，不传路径 |
| 会话损坏不阻断 | 降级为全新上下文，记录 warning |
| `stop()` 后一切失效 | 所有 context 关闭，下次需重新 `start()` |

---

## 4. 配置模型

```python
@dataclass
class BrowserConfig:
    headless: bool = False          # 是否有头（调试时为 False）
    slow_mo: int = 500              # 操作间延迟(ms)，模拟人类
    viewport_width: int = 1920      # 视口宽度
    viewport_height: int = 1080     # 视口高度
    locale: str = "zh-CN"           # 浏览器语言区域
    timeout: int = 30000            # 默认超时(ms)
    sessions_dir: str = "data/sessions"  # 会话 JSON 文件目录
```

`config.yaml` 中 `browser` 节点直接映射。所有字段有默认值，用户可零配置启动。

---

# 实现部分 — PlaywrightBrowserManager

> 以下描述当前唯一实现。替换为其他浏览器驱动时，只需实现 `BrowserManager` 接口并提供新的 `BrowserManagerFactory`，其余代码不受影响。

---

## 5. 内部状态

```
PlaywrightBrowserManager
├── _config: BrowserConfig           # 只读配置
├── _playwright: Playwright | None   # playwright 实例（start 后赋值）
├── _browser: Browser | None         # chromium 浏览器实例
├── _contexts: dict[str, BrowserContext]  # platform → 已创建的上下文缓存
├── _login_pending: dict[str, bool]  # 标记正在进行登录的平台
└── _log: Logger                     # loguru bind(module="browser")
```

## 6. 关键流程

### 6.1 启动

```
start()
  ├─ async_playwright().start()      → 获取 playwright 引擎实例
  ├─ playwright.chromium.launch(     → 启动 chromium 浏览器
  │     headless=config.headless,
  │     slow_mo=config.slow_mo       ← 自动操作延迟，模拟人类
  │   )
```

### 6.2 关闭

```
stop()
  ├─ for ctx in _contexts.values(): await ctx.close()   ← 先关上下文
  ├─ _contexts.clear()
  ├─ if _browser: await _browser.close()                ← 再关浏览器
  ├─ if _playwright: await _playwright.stop()           ← 最后停引擎
```

关闭顺序严格：上下文 → 浏览器 → 引擎。

### 6.3 获取上下文

```
get_context(platform)
  ├─ 缓存命中 → 直接返回
  ├─ _browser 不存在 → raise RuntimeError("not started")
  ├─ 构建会话路径: {sessions_dir}/{platform}_state.json
  ├─ 文件存在 → 读取 JSON 作为 storage_state
  │   若 JSON 损坏 → 丢弃，state=None，记录 warning
  ├─ browser.new_context(viewport=..., locale=..., storage_state=state)
  ├─ 缓存: _contexts[platform] = ctx
  └─ 返回 ctx
```

每个平台一个上下文，cookies/localStorage 隔离。会话文件损坏不阻断流程。

### 6.4 保存会话

```
save_session(platform)
  ├─ 上下文不存在 → raise RuntimeError("no context")
  ├─ state = await ctx.storage_state()      → Playwright 原生序列化
  ├─ sessions_dir 不存在则 mkdir -p
  └─ 写入 {platform}_state.json
```

### 6.5 登录流程

```
┌─ login_flow(platform, url) ─────────────────────────────┐
│  ├─ ctx = await get_context(platform)                    │
│  ├─ page = await ctx.new_page()                          │
│  ├─ 注入反检测脚本                                        │
│  ├─ await page.goto(url)                                 │
│  └─ _login_pending[platform] = True                      │
└──────────────────────────────────────────────────────────┘
                         │
                         ▼
          ┌─── 用户手动在浏览器中登录 ───┐
                         │
                         ▼
┌─ set_login_complete(platform) ──────────────────────────┐
│  ├─ platform 不在 _login_pending → raise RuntimeError   │
│  ├─ await save_session(platform)    → 保存认证信息       │
│  ├─ _login_pending.pop(platform)    → 退出等待状态       │
│  └─ 关闭该上下文下所有页面                               │
└──────────────────────────────────────────────────────────┘
```

**设计意图**：登录是人工操作，BrowserManager 只负责 "打开页面" 和 "确认完成后的保存"。中间表单填写、扫码、验证码等全部由用户在浏览器中手动完成。`_login_pending` 可供 API 层暴露 `GET /api/login/status/{platform}` 查询。

### 6.6 反检测（Stealth）

尝试在 `login_flow` 打开新页面时注入反检测脚本。若对应依赖不可用，仅记录 warning，不阻断流程。

---

## 7. 会话文件格式 (Playwright storage_state)

```json
{
  "cookies": [
    {"name": "auth_token", "value": "xxx", "domain": ".x.com", ...}
  ],
  "origins": [
    {"origin": "https://x.com", "localStorage": [...]}
  ]
}
```

文件路径约定：`{BrowserConfig.sessions_dir}/{platform}_state.json`

---

## 8. 工厂

```python
class BrowserManagerFactory(Factory[BrowserManager]):
    def create(self, config: object) -> BrowserManager:
        if isinstance(config, BrowserConfig):
            return PlaywrightBrowserManager(config)
        return PlaywrightBrowserManager(BrowserConfig())
```

- 无构造注入 — BrowserManager 不依赖其他 core 模块
- 返回类型是 `BrowserManager`（接口），调用方不感知具体实现
- 支持扩展：将来可加 `elif type == "selenium"` 分支

---

## 9. 调用时序

```
Orchestrator
  ├─ await browser.start()
  │
  ├─ ctx = await browser.get_context("twitter")   ← BrowserManager 管
  ├─ page = await ctx.new_page()                  ← PlatformAdapter 管
  ├─ ... adapter 采集逻辑 ...
  │
  ├─ await browser.get_context("rednote")          ← 切换平台
  │
  ├─ await browser.save_session("twitter")         ← 可选：刷新 Cookie
  │
  └─ await browser.stop()
```

分界线：`get_context` 返回后，页面级操作归 Adapter；上下文生命周期归 BrowserManager。

---

## 10. 错误处理

| 场景 | 行为 |
|------|------|
| `get_context` 时引擎未启动 | `RuntimeError("not started")` |
| 会话 JSON 损坏 | 丢弃，以全新上下文启动，记录 warning |
| `save_session` 时上下文不存在 | `RuntimeError("no context")` |
| `set_login_complete` 时无 login flow | `RuntimeError("no login flow active")` |
| 反检测组件不可用 | 跳过注入，记录 warning，继续执行 |

---

## 11. 测试

TA 使用真实 Chromium headless，走完整 start → context → page → save → stop 链路。

| 类型 | 文件 | 覆盖 |
|------|------|------|
| UT | `test_browser_models.py` | 默认值、自定义、相等性 |
| UT | `test_browser_factory.py` | 创建、配置传递、接口返回 |
| TA | `test_browser_impl.py` | 生命周期、context 管理、session 读写、login flow、反检测缺失降级 |

---

*文档版本：v1.2 | 最后更新：2026-04-25*
