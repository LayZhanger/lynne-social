# Lynne 总体设计方案 v3.1

> 先定义有哪些模块、每个模块的接口是什么、模块之间如何协作。
> 实现细节不在本文档讨论。

---

## 1. 系统分层

```
┌─────────────────────────────────────────┐
│              Web UI                     │  ← 展示层
│           (HTML 页面)                    │
└────────────────◄ JSON ►─────────────────┘
┌─────────────────────────────────────────┐
│           API Layer                     │  ← 接口层
│        (REST + WebSocket)               │
└────────────────◄ 调用 ►─────────────────┘
┌─────────────────────────────────────────┐
│          Orchestrator                   │  ← 编排层
│     (任务调度 + 流程串联)                │
└──┬──────────┬──────────┬───────────────┘
   │          │          │
   ▼          ▼          ▼
┌──────┐ ┌──────┐ ┌──────────────┐
│Browser│ │Platform│ │  LLM Engine  │        ← 核心引擎层
│Manager│ │Adapters│ │              │
└──┬───┘ └───┬───┘ └──────┬───────┘
   │         │             │
   └────┬────┘       ┌─────▼─────┐
        ▼            ▼           ▼
   ┌──────────────────────────────────┐
   │          FileStorage              │        ← 持久化层
   │         (JSONL 文件)              │
   └──────────────────────────────────┘
```

---

## 2. 模块清单

| 编号 | 模块 | 职责 | 所属层 |
|------|------|------|--------|
| M1 | **Web UI** | 用户交互界面，调用 API 展示数据 | 展示 |
| M2 | **API Layer** | HTTP 路由，请求校验，响应格式化 | 接口 |
| M3 | **Orchestrator** | 采集流程编排，各模块调用串联 | 编排 |
| M4 | **BrowserManager** | Playwright 生命周期 + 会话管理 | 引擎 |
| M5 | **PlatformAdapter** | 平台特定的数据采集（抽象 + 各平台实现） | 引擎 |
| M6 | **LLMEngine** | 大模型调用：筛选、摘要、报告 | 引擎 |
| M7 | **FileStorage** | 数据的文件读写（JSONL / Markdown / JSON） | 持久化 |
| M8 | **ConfigManager** | 配置加载、校验、热更新 | 基础设施 |
| M9 | **Scheduler** | 定时任务调度 | 基础设施 |
| M10 | **Logger** | 日志记录 | 基础设施 |

---

## 3. 模块接口设计

### M1. Web UI

```
职责：浏览器端展示和交互。调用 API 获取数据，渲染页面，处理用户操作。

页面结构：
  /              → 日报首页
  /config        → 任务配置
  /login         → 平台登录引导
  /history       → 历史数据浏览
  /settings      → LLM 与系统设置

不定义 HTML/CSS 细节，明确一点：前端通过 fetch 调用 API Layer 的接口。
```

### M2. API Layer

```
职责：对外暴露 REST 接口，接受前端请求，调用 Orcherstrator/Storage，
     返回 JSON 或 HTML。

接口定义：

  GET  /api/report?date=2026-04-25
    → 返回日报内容（Markdown + 元信息）

  GET  /api/items?date=2026-04-25&tag=模型发布&page=1
    → 返回帖子列表（分页、可筛选）

  GET  /api/tasks
    → 返回当前所有任务配置

  POST /api/tasks
    → 新增或更新任务配置

  DELETE /api/tasks/{task_name}
    → 删除一个任务

  POST /api/run
    Body: { task?: string, topic?: string, platforms?: string[], limit?: int }
    → 触发一次采集。不传 task 则全部执行，传 topic 做即时搜索

  GET  /api/run/status
    → 查询当前采集进度（跑着还是完成了）

  POST /api/login/{platform}
    → 对指定平台启动登录流程，返回操作指引

  GET  /api/login/status/{platform}
    → 查询某平台会话是否有效

  GET  /api/history
    → 返回有数据的日期列表

  GET  /api/settings
    → 返回当前 LLM / 浏览器 / 系统设置

  PUT  /api/settings
    → 更新设置

  POST /api/settings/llm/test
    → 测试 LLM 连接

  WebSocket /ws/run/{task_name}
    → 实时推送采集进度（开始采集 → 数据到达 → LLM处理中 → 完成）
```

### M3. Orchestrator

```
职责：串联一次完整的 "采集 → 筛选 → 摘要 → 存储 → 报告" 流程。
     是 API Layer 和 引擎层的中间协调者。

接口：
  run_task(config: TaskConfig) -> TaskResult
    执行单个采集任务，返回采集结果。

  run_search(topic: str, platforms: list[str], limit: int) -> SearchResult
    执行即时搜索（类似临时任务），返回结果。

  get_status() -> RunStatus
    返回当前是否正在采集，以及进度。

类型定义：
  TaskConfig {
      name: str
      platforms: list[str]
      topic: str              # 自然语言关注主题
      keywords: list[str]     # 补充搜索关键词
      user_ids: list[str]     # 追踪的特定用户
      limit: int
  }

  TaskResult {
      task_name: str
      fetched_count: int      # 采集了多少条
      kept_count: int         # LLM 筛选后保留了多少条
      llm_calls: int          # LLM 调用次数
      report_markdown: str    # 生成的日报
      duration_seconds: float
      items: list[UnifiedItem]
  }

  RunStatus {
      running: bool
      current_task: str | None
      progress: str           # "采集Twitter" | "LLM筛选" | "完成"
  }
```

### M4. BrowserManager

```
职责：管理 Playwright 的启动/关闭，为各平台创建和管理浏览器会话。

接口：
  start() -> None
    启动 Playwright（默认有头），完成后调用 start_hook。

  stop() -> None
    关闭浏览器，释放资源。

  get_context(platform: str) -> BrowserContext
    获取该平台的浏览器上下文：
    - 如果 data/sessions/{platform}_state.json 存在 → 加载（免登录）
    - 如果不存在 → 创建新的空上下文

  save_session(platform: str) -> None
    保存当前上下文状态到文件。

  login_flow(platform: str) -> None
    启动登录流程：打开平台首页，保持浏览器窗口，等用户手动完成登录。
    内部通过 page.wait_for_url() 或用户确认来判断登录成功。

  set_login_complete(platform: str) -> None
    用户手动确认登录完成，保存会话。

  操作限制：
    - 所有操作间插入随机延迟（slow_mo + random jitter）
    - 每个 page 操作前检查是否被风控（验证码、滑块等）
    - 遇到风控时暂停当前平台，通知上层处理
```

### M5. PlatformAdapter（抽象层 + 各平台实现）

```
抽象基类：
  
  class BaseAdapter(ABC):
      
      platform_name: str  # 平台标识，如 "twitter", "rednote"
      
      async def search(keywords: list[str], limit: int) -> AsyncIterator[UnifiedItem]
          """关键词搜索：输入关键词列表，流式返回统一格式的数据"""
      
      async def get_user_posts(user_id: str, limit: int) -> AsyncIterator[UnifiedItem]
          """获取指定用户的最新帖子"""
      
      async def get_trending(limit: int) -> AsyncIterator[UnifiedItem]
          """获取平台热门/趋势内容"""
      
      async def extract(page_or_data) -> UnifiedItem
          """从原始页面/数据提取为统一数据格式"""
      
      async def health_check() -> bool
          """检查适配器是否可用（会话有效、页面能打开）"""
```

```
具体实现：

  TwitterAdapter(BaseAdapter)
      实现方案：拦截 GraphQL API 请求获取 JSON
      search()  → 调用 SearchTimeline 接口
      get_user_posts() → 调用 UserTweets 接口
      get_trending() → 调用 Trends 接口

  RedNoteAdapter(BaseAdapter)
      实现方案：DOM 选择器 + 滚动加载
      search() → 打开搜索页 → 输入关键词 → 滚动获取笔记列表
      extract() → 通过 CSS 选择器提取标题/正文/互动数据

  DouyinAdapter(BaseAdapter)
      实现方案：DOM + API 结合
      备注：Web 版功能受限，作为后期优先级

  FacebookAdapter(BaseAdapter)
      实现方案：移动版 m.facebook.com + DOM
      备注：后期优先级

每个 adapter 实现时只需覆写上述抽象方法，核心流程不用改。
新加一个平台 = 写一个新 Adapter 类，其他模块不变。
```

```
UnifiedItem（跨平台统一数据模型）：

  UnifiedItem {
      platform: str
      item_id: str
      item_type: str            # "post" | "video" | "comment"
      author_id: str
      author_name: str
      content_text: str
      content_media: list[str]  # 图片/视频 URL
      url: str
      published_at: str         # ISO 8601
      fetched_at: str           # ISO 8601
      metrics: dict             # {likes, comments, shares, views}

      # 以下字段由 LLM Engine 填充
      llm_relevance_score: int
      llm_relevance_reason: str
      llm_summary: str
      llm_tags: list[str]
      llm_key_points: list[str]
  }
```

### M6. LLMEngine

```
职责：封装大模型调用，提供三个核心能力：筛选、摘要、报告。
     对上层屏蔽 provider 差异（DeepSeek / OpenAI / Claude / Ollama 统一接口）。

接口：
  class LLMEngine:
      
      __init__(config: LLMConfig)
          config 包含 provider / api_key / base_url / 模型名 / 阈值等
          下层通过工厂模式创建对应的 client

      async def filter(topic: str, items: list[UnifiedItem]) -> list[UnifiedItem]
          ① 相关性筛选：
          - 输入：用户关注主题 + 帖子列表
          - 对每条帖子调用 LLM，返回相关度 1-10
          - 低于阈值的丢弃，通过的填充 llm_relevance_score 和 reason
          - 支持并发调用以加速

      async def extract(items: list[UnifiedItem]) -> list[UnifiedItem]
          ② 内容理解：
          - 输入：筛选后的帖子列表
          - 对每条帖子调用 LLM，生成：
            - llm_summary（一句话摘要）
            - llm_tags（话题标签列表）
            - llm_key_points（关键信息点列表）
          - 支持并发调用

      async def generate_report(items: list[UnifiedItem], topic: str) -> str
          ④ 日报生成：
          - 输入：当天所有留存数据 + 关注主题
          - 调用强模型，按话题聚类生成结构化日报（Markdown 格式）
          - 日报包含：话题概述、来源帖子引用、互动热度、跨平台对比

      async def health_check() -> bool
          测试 LLM 连接是否正常

  LLMConfig 定义：
      LLMConfig {
          provider: str          # "deepseek" | "openai" | "claude" | "ollama" | "custom"
          api_key: str
          base_url: str          # 可空（使用默认）
          model: str             # 默认模型
          model_filter: str      # 筛选用模型（可空，空则用 model）
          model_extract: str     # 摘要用模型（可空）
          model_report: str      # 报告用模型（可空）
          relevance_threshold: int   # 默认 6
          temperature: float
          max_tokens: int
          timeout: int
      }

  Prompt 管理：
      所有 LLM 调用的 prompt 模板放在 src/llm/prompts/ 目录，
      使用 Jinja2 模板引擎，用户可修改模板而不改代码。
      filter.j2 / extract.j2 / daily_report.j2
```

### M7. FileStorage

```
职责：读写本地文件。每天一个目录，JSONL 格式存储内容数据，
     Markdown 存储日报，JSON 存储摘要元信息。

接口：
  class FileStorage:
      
      __init__(data_dir: str = "data")

      save_items(date: str, items: list[UnifiedItem]) -> None
          追加写入 data/{date}/items.jsonl

      load_items(date: str, filters: dict | None) -> list[UnifiedItem]
          读取某天数据，支持按 platform / tag / relevance_score 筛选

      save_report(date: str, markdown: str) -> None
          写入 data/{date}/report.md

      load_report(date: str) -> str | None
          读取某天日报

      save_summary(date: str, summary: dict) -> None
          写入 data/{date}/summary.json

      list_dates() -> list[str]
          返回所有有数据的日期列表，按日期倒序
```

### M8. ConfigManager

```
职责：加载和校验 config.yaml，提供类型安全的配置读取，
     支持环境变量注入（如 ${VAR_NAME}）。

接口：
  load_config(path: str = "config.yaml") -> Config
      加载并校验配置文件，替换环境变量占位符

  get_tasks() -> list[TaskConfig]
      返回当前所有任务

  get_llm_config() -> LLMConfig
      返回 LLM 配置

  get_platforms() -> list[PlatformConfig]
      返回已启用的平台列表

  reload() -> None
      重新加载配置（热更新）

Config 类型层次：
  Config
    ├── server: ServerConfig     (端口、是否自动打开浏览器)
    ├── llm: LLMConfig
    ├── browser: BrowserConfig   (headless, slow_mo, viewport)
    ├── platforms: dict[str, PlatformConfig]   (key 为平台名)
    └── tasks: list[TaskConfig]
```

### M9. Scheduler

```
职责：管理定时任务，根据配置中的 schedule 字段，周期性地
     调用 Orchestrator 执行采集。

接口：
  class Scheduler:
      
      __init__(orchestrator: Orchestrator)
      
      add_job(task: TaskConfig) -> None
          添加一个定时采集任务
      
      remove_job(task_name: str) -> None
          移除任务
      
      start() -> None
          启动调度器
      
      stop() -> None
          停止调度器
      
      get_jobs() -> list[JobStatus]
          查看所有调度任务状态
      
      sync_from_config() -> None
          从 ConfigManager 同步最新任务配置

  支持 schedule 格式：
      "every 4 hours" | "every 30 minutes" | "at 08:00" | "manual"
```

### M10. Logger

```
职责：统一的日志记录，按模块区分，同时输出到文件和控制台。

接口：
  get_logger(module: str) -> Logger
      获取带模块前缀的 logger 实例

  日志文件：data/lynne.log
  级别：可配置（默认 INFO）
  脱敏：日志中不出现 API Key、Cookie、Token 等敏感字段
```

---

## 4. 模块协作时序

```
用户点"立即采集" → API → Orchestrator  → 一次完整流程：

  Orchestrator                   BrowserManager     PlatformAdapter    LLMEngine       FileStorage
      │                               │                   │                │               │
      │── start() ──────────────────►│                   │                │               │
      │── get_context(platform) ────►│                   │                │               │
      │                               │                   │                │               │
      │── search(keywords, limit) ─────────────────────►│                │               │
      │                               │                   │                │               │
      │◄── UnifiedItem stream ─────────────────────────│                │               │
      │                               │                   │                │               │
      │── filter(topic, items) ──────────────────────────────────────►│               │
      │◄── filtered items ──────────────────────────────────────────│               │
      │                               │                   │                │               │
      │── extract(items) ───────────────────────────────────────────►│               │
      │◄── enriched items ──────────────────────────────────────────│               │
      │                               │                   │                │               │
      │── save_items(items) ──────────────────────────────────────────────────────►│
      │                               │                   │                │               │
      │── generate_report(items, topic) ─────────────────────────────►│               │
      │◄── report markdown ─────────────────────────────────────────│               │
      │                               │                   │                │               │
      │── save_report(markdown) ────────────────────────────────────────────────►│
      │                               │                   │                │               │
      │◄── TaskResult                                                                              API
```

---

## 5. 启动流程

```
1. 加载 config.yaml (ConfigManager)
2. 初始化各个模块（FileStorage / LLMEngine / BrowserManager / Orchestrator / Scheduler）
3. 启动浏览器引擎 (BrowserManager.start)
4. 同步定时任务 (Scheduler.sync_from_config)
5. 启动 Web 服务 (FastAPI + uvicorn)
6. 可选：自动打开浏览器访问首页
```

---

## 6. 模块依赖关系

```
                    ┌──────────┐
                    │  Config  │  ← 被所有模块依赖（只读）
                    └────┬─────┘
                         │
  ┌──────────┐    ┌──────▼─────┐    ┌──────────┐
  │ Scheduler│───►│Orchestrator│◄───│ API Layer│
  └──────────┘    └──┬───┬───┬─┘    └──────────┘
                     │   │   │
          ┌──────────┘   │   └──────────┐
          ▼              ▼              ▼
    ┌──────────┐  ┌──────────┐  ┌───────────┐
    │ Browser  │  │ Platform │  │  LLM      │
    │ Manager  │  │ Adapter  │  │  Engine   │
    └────┬─────┘  └──────────┘  └─────┬─────┘
         │                            │
         └──────────┬─────────────────┘
                    ▼
              ┌──────────┐
              │  Storage │
              └──────────┘
```

---

*文档版本：v3.1 | 最后更新：2026-04-25*
