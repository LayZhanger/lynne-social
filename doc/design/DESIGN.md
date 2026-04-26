# Lynne 总体设计方案 v4.2

> **v4.2 核心变更**：引入 `LLMAdapter` 中间父类，统一 LLM 提取逻辑。LLMConfig 从 Config 逐层透传（YamlConfigLoader → main.py → AdapterFactory → LLMAdapter），LLMAdapter 内部自建 LLMEngine。工厂只传配置不创建 Engine。

---

## 1. 设计理念

```
v3.1 模型                                v4.0 模型
───────                                  ────────
用户定义 Task（关键词 + 平台）           用户表达 intent（自然语言）
    │                                        │
Orchestrator（硬编码流程）                 Agent（LLM 自主决策）
  ├─ 步骤1: 搜A                        ├─ "先搜 Twitter + RedNote，关键词 X"
  ├─ 步骤2: 搜B                        ├─ "信息不够，换 Y 再搜一次"
  ├─ 步骤3: LLM筛选                    ├─ "够了，出报告"
  ├─ 步骤4: LLM摘要                    └─ 生成日报 → 返回
  └─ 步骤5: LLM报告

LLM 是 pipeline 中的处理节点              LLM 是决策中枢，每一步都是它定的
关键词由人写死                            关键词由 Agent 动态生成
```

**一句话**：用户说"关注什么"，Agent（LLM）自己决定搜什么、怎么搜、何时停、何时出报告。

---

## 2. 系统分层

```
┌─────────────────────────────────────────┐
│              Web UI                     │  展示层
└────────────────◄ JSON ►─────────────────┘
┌─────────────────────────────────────────┐
│           API Layer                     │  接口层
│        (REST + WebSocket)               │
└────────────────◄ 调用 ►─────────────────┘
┌─────────────────────────────────────────┐
│              Agent                      │  决策层 ← 新
│       (ReAct 循环 + Tool 调度)           │
└──┬──────────┬──────────────────────────┘
   │          │
   ▼          ▼
┌──────────┐ ┌─────────────────────────────┐
│ Platform │ │         wheel               │
│ Adapters │ │                             │  引擎 + 基础设施
│ (core)   │ │  BrowserManager   (wheel)   │
│          │ │  LLMEngine        (wheel)   │
│          │ │  FileStorage      (wheel)   │
│          │ │  Config           (wheel)   │
│          │ │  Scheduler        (wheel)   │
│          │ │  Logger           (wheel)   │
└──────────┘ └─────────────────────────────┘
```

---

## 3. 模块清单

| 编号 | 模块 | 所属层 | 职责 |
|------|------|--------|------|
| M1  | **Web UI** | 展示 | 用户交互界面，调用 API 展示数据 |
| M2  | **API Layer** | 接口 | HTTP 路由，请求校验，响应格式化 |
| M3  | **Agent** | **决策（新）** | LLM 驱动的 ReAct 循环，Tool 调度，流程决策 |
| M4  | **PlatformAdapter** | 引擎 | 平台特定的数据采集 |
| W1  | **BrowserManager** | wheel | Playwright 生命周期 + 会话管理 |
| W2  | **LLMEngine** | **wheel（移入）** | 裸 LLM 调用：`chat(messages, tools?) → dict` |
| W3  | **FileStorage** | wheel | JSONL / Markdown / JSON 文件读写 |
| W4  | **Config** | wheel | 配置加载、校验、环境变量替换 |
| W5  | **Scheduler** | wheel | 定时任务调度 |
| W6  | **Logger** | wheel | 日志记录 |

**删除**：`Orchestrator`（被 Agent 替代）

---

## 4. 模块接口设计

### M1. Web UI

```
职责：浏览器端展示和交互。调用 API 获取数据，渲染页面。

页面结构：
  /              → 日报首页
  /config        → 任务配置
  /login         → 平台登录引导
  /history       → 历史数据浏览
  /settings      → LLM 与系统设置
```

### M2. API Layer

```
职责：对外暴露 REST 接口，接受前端请求，调用 Agent/Storage。

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
    Body: { task?: string, intent?: string, platforms?: string[] }
    → 触发一次采集。传 task 则执行对应任务，传 intent 做即时采集

  GET  /api/run/status
    → 查询当前采集进度（步骤描述 + Agent 思考过程）

  POST /api/login/{platform}
    → 对指定平台启动登录流程

  GET  /api/login/status/{platform}
    → 查询某平台会话是否有效

  GET  /api/history
    → 返回有数据的日期列表

  GET  /api/settings
  PUT  /api/settings
    → LLM / 浏览器 / 系统设置

  POST /api/settings/llm/test
    → 测试 LLM 连接

  WebSocket /ws/run/{task_name}
    → 实时推送 Agent 每一步：Thought → Action → Observation
```

### M3. Agent（新）

```
职责：LLM 驱动的信息采集协调者。接收用户意图，自主规划并执行多步操作，
     直到信息充分后生成日报。

接口：
  class Agent(Module):

    async def run(
        intent: str,
        platforms: list[str],
        max_steps: int = 10,
        history_dates: list[str] | None = None,
    ) -> TaskResult:
        """
        执行一次信息采集任务。

        Agent 内部进入 ReAct 循环：
          while not done and steps < max_steps:
            1. Thought: LLM 分析当前状态，产出推理
            2. Action: LLM 决定调用哪个 Tool + 参数
            3. Observation: 执行 Tool，拿到结果
            4. 注入对话历史，LLM 决定"继续"或"完成"

        done 条件：
          - LLM 输出 Final Answer（包含日报 Markdown）
          - 或达到 max_steps
          - 或用尽了所有搜索可能（LLM 自行判断）
        """

 类型定义：

  TaskConfig {
      name: str
      platforms: list[str]
      intent: str            # 自然语言关注主题（Agent 自己决定搜什么关键词）
      schedule: str          # "every 4 hours" | "at 08:00" | "manual"
  }

  TaskResult {
      task_name: str
      fetched_count: int
      kept_count: int
      llm_calls: int
      report_markdown: str
      duration_seconds: float
      steps: list[AgentStep]  # 每一步的 Thought/Action/Observation，可追溯
      items: list[UnifiedItem]
  }

  AgentStep {
      step: int
      thought: str      # "目前搜到15条，信息不够，需要换关键词再搜"
      action: str       # search(platform="rednote", keywords="AI 突破")
      observation: str  # "搜到 8 条新结果，其中 3 条相关"
  }
```

**Agent 不自己判断"相关性"或"做摘要"**。这些能力通过在 system prompt 中引导 LLM 完成。
LLM 看搜索结果后自己判断质量，决定是否追加搜索。

#### ReAct 循环示意

```
Step 1:
  Thought: "先搜两个平台的 AI 模型相关消息"
  Action: search(platform="twitter", keywords="AI model release breakthrough")
  Action: search(platform="rednote", keywords="大模型 发布 突破")
  Observation: "tw: 12条 rednote: 8条，覆盖了GPT-5和Claude4"

Step 2:
  Thought: "信息够丰富了，出日报"
  Final: "# AI 行业日报\n## GPT-5 发布\n..."
```

**每步的输出是 LLM 自己决定的**，Tool 约束通过 function-calling 的 `tools` 参数传递。

### M4. PlatformAdapter

```
职责：平台数据采集。v4.2 引入 LLMAdapter 中间层，所有平台适配器通过它复用 LLM 驱动的自适应提取。

类层次：
  BaseAdapter(ABC)                     ← 接口不变
    └─ LLMAdapter(BaseAdapter, ABC)    ← 中间层（v4.2 新增）
         ├─ 管理 LLMEngine 生命周期（从传入的 LLMConfig 懒创建）
         ├─ _get_or_generate_extract_fn() 通用逻辑
         ├─ _scroll_and_extract() 通用滚动 + 提取循环
         ├─ 缓存（URL host + path → JS extract function）
         │
         ├─→ RedNoteAdapter(LLMAdapter)    ← 平台 shell
         └─→ (未来) TwitterAdapter         ← 平台 shell

Config 传递链（Config → Engine，单向）：
  config.yaml
    → YamlConfigLoader.load()  ← 唯一读文件的地方
      → Config.llm  (Pydantic)
        → main.py: LLMConfig dataclass 转换
          → AdapterFactory.create(browser, adapter_config, llm_config=llm_config)
            → RedNoteAdapter(browser, config, llm_config=...)

关键规则：
  - 只有 YamlConfigLoader 读 config.yaml
  - AdapterFactory 无状态，零依赖注入 —— create(browser, config, *, llm_config)
  - LLMAdapter 内部使用 LLMEngineFactory 自建 engine（懒初始化）
  - llm_config=None 时不创建 LLM → 走 CSS 回退路径

提取策略：
  ┌─ LLM 路径（llm_config 不为 None）─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐
  │  page 加载                                                      │
  │    → 通用 _SKELETON_JS 提取前 N 个帖子的结构骨架                  │
  │    → LLMAdapter._ensure_llm() 懒启动 DeepSeekEngine              │
  │    → LLM 分析骨架，生成 JS extractPosts() 函数                    │
  │    → page.evaluate(extractPosts) 批量提取                         │
  │    → 缓存（按 URL host + path），后续翻页零 LLM 调用              │
  └─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘

  ┌─ CSS 回退路径（llm_config=None 或 LLM 生成失败）─ ─ ─ ─ ─ ┐
  │  硬编码 CSS 选择器（_NOTE_ITEM 等），行为不变                       │
  └─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘

依赖关系：
  LLMAdapter → BrowserManager + LLMEngine  (core → wheel，单向，通过 LLMConfig 注入)

已实现：RedNoteAdapter（CSS 路径完整）
进行中：LLMAdapter + RedNoteAdapter LLM 提取接入
待实现：TwitterAdapter（GraphQL API 拦截）
```

#### M4a. LLMAdapter 实现细节（v4.2）

```
LLMAdapter 是所有平台适配器的中间父类，封装 LLM 驱动的自适应内容提取。
继承 BaseAdapter（接口不变），被 RedNoteAdapter 等平台 shell 继承。

文件：src/core/adapters/imp/llm_adapter.py

核心属性：
  self._browser        BrowserManager    ← 注入
  self._config         AdapterConfig     ← 注入
  self._llm_config     LLMConfig | None  ← 注入（注意：不是 LLMEngine！）
  self._llm            LLMEngine | None  ← _ensure_llm() 懒创建
  self._extract_fn_cache  dict[str, str] ← URL pattern → JS extract function

懒初始化：
  async def _ensure_llm(self):
      if self._llm is None and self._llm_config is not None:
          from src.wheel.llm.llm_factory import LLMEngineFactory
          self._llm = LLMEngineFactory().create(self._llm_config)
          await self._llm.start()
  
  llm_config 由 AdapterFactory 透传（来自 main.py → Config.llm）。
  llm_config=None 时 _ensure_llm() 不创建 Engine → 自动走 CSS 回退。

LLM 提取协议：

[1] 输入：页面骨架 (page.evaluate → dict)
   通用 DOM walker（不依赖平台选择器）遍历 DOM，找包含子文本、链接、图片的容器节点。
   只取前 3 个帖子的结构样本（~500 tokens），发送给 LLM。

   格式：[
     {"tag":"section", "cls":"note-item", "nth":1,
      "children":[
        {"sel":"div.title>span",        "txt":"GPT-5 发布"},
        {"sel":"div.author span.name",  "txt":"科技观察者"},
        {"sel":"div.desc",              "txt":"本文分析..."},
        {"sel":"span.count",            "txt":"12,000"},
        {"sel":"a[href*='/explore/']",  "href":"/explore/123"},
        {"sel":"img",                   "src":"https://pic.jpg"}
      ]},
     ...
   ]

   提示词（send to LLM）：
     "你是浏览器自动化专家。根据以下页面骨架生成 JavaScript 函数 extractPosts()。
      函数遍历 DOM，提取每个帖子的字段：item_id, title, author_name, content,
      likes, url, images。
      字段名必须是英文 camelCase，值必须是字符串或数组。
      返回纯 JavaScript 代码，不含 ``` 标记，不含注释。"

[2] 输出：JS 提取函数 (LLM → Adapter)
   形如：
     function extractPosts() {
       return [...document.querySelectorAll('.note-item')].map(el => ({
         item_id: el.getAttribute('data-id') || '',
         title: el.querySelector('.title span')?.textContent?.trim() || '',
         author_name: el.querySelector('.author .name')?.textContent?.trim() || '',
         content: el.querySelector('.desc')?.textContent?.trim() || '',
         likes: (el.querySelector('.count')?.textContent||'0').replace(/,/g,''),
         url: (()=>{const a=el.querySelector('a');const h=a?.getAttribute('href')||'';
           return h.startsWith('http')?h:'https://www.xiaohongshu.com'+h;})(),
         images: [...el.querySelectorAll('img')].map(i=>i.getAttribute('src')||'').filter(Boolean)
       }));
     }

[3] 输出：UnifiedItem (page.evaluate(extractPosts) → dict → extract() → UnifiedItem)
   extractPost 的返回值直接映射到现有的 extract(data: dict) → UnifiedItem 方法。
   字段名兼容现有 fallback 逻辑（item_id/id, author_name/username 等）。
```



### W1. BrowserManager（wheel）

```
职责：Playwright 生命周期 + 会话管理。
接口与 v3.1 相同，从 core/ 移至 wheel/。

  class BrowserManager(Module):
      async def start() -> None: ...
      async def stop() -> None: ...
      async def get_context(platform: str) -> BrowserContext: ...
      async def save_session(platform: str) -> None: ...
      async def login_flow(platform: str, url: str) -> None: ...
      async def set_login_complete(platform: str) -> None: ...
      def health_check() -> bool: ...
```

### W2. LLMEngine（wheel，瘦身）

```
职责：封装大模型调用。v4.0 删除 filter/extract/generate_report，
     只保留一个裸调用方法。

接口：
  class LLMEngine(Module):

      async def chat(
          messages: list[dict],
          tools: list[dict] | None = None,
      ) -> dict:
          """
          messages = [{"role": "user", "content": "..."}, ...]
          tools    = [{"type": "function", "function": {...}}, ...]
                      ↑ OpenAI tool-calling 格式
          returns  = {"role": "assistant", "content": "...",
                      "tool_calls": [{...}] | None}
          """

      async def health_check() -> bool: ...

  LLMConfig {
      provider: str          # "deepseek" | "openai" | "custom"
      api_key: str
      base_url: str
      model: str
      temperature: float = 0.7
      max_tokens: int = 4096
      timeout: int = 60
  }

与 v3.1 的差异：
  - 删除 filter() / extract() / generate_report()
  - 删除 model_filter / model_extract / model_report / relevance_threshold
    （这些决策权交给 Agent）
  - 删除 Jinja2 模板目录（prompt 由 Agent 动态构建）
  - 新增 tools 参数（支持 function-calling）

实现：stdlib urllib.request + scheduler.run_blocking()（无需第三方 SDK）
  文件：src/wheel/llm/imp/deepseek_engine.py  —  DeepSeekEngine
```

### W3. FileStorage（wheel）

```
职责：不变。每天一个目录，JSONL 格式存储。

接口：
  async def save_items(date: str, items: list[UnifiedItem]) -> None: ...
  async def load_items(date: str, filters: dict | None) -> list[UnifiedItem]: ...
  async def save_report(date: str, markdown: str) -> None: ...
  async def load_report(date: str) -> str | None: ...
  async def save_summary(date: str, summary: dict) -> None: ...
  async def list_dates() -> list[str]: ...
```

### W4. Config（wheel）

```
职责：不变。

接口：
  load_config(path: str = "config.yaml") -> Config
  get_tasks() -> list[TaskConfig]     # v4.0 TaskConfig 瘦身
  get_agent_config() -> AgentConfig   # 新
  get_platforms() -> list[PlatformConfig]
  reload() -> None
```

### W5. Scheduler（wheel）

```
职责：通用定时回调调度器。管理定时任务，按 schedule 触发 callback。不对 Agent 有任何导入。

接口：
  class Scheduler(Module):
      add_job(name: str, schedule: str, callback: Callable, *args, **kwargs) -> None
      remove_job(name: str) -> None
      start() -> None
      stop() -> None
      get_jobs() -> list[JobStatus]
      sync_from_config(tasks: list[TaskConfig], callback: Callable) -> None

v4.0 变化：Scheduler 是纯通用调度器。Agent 的绑定在 main.py 完成：
  scheduler.add_job("AI日报", "every 4 hours", agent.run, intent="AI动态", platforms=[...])
```

### W6. Logger（wheel）

```
职责：不变。
  get_logger(module: str) -> Logger
  日志文件：data/lynne.log
  脱敏：API Key、Cookie、Token
```

---

## 5. Tool 系统

Agent 通过 Tool 与外界交互。每个 Tool 包装一个已开发的模块方法。

```python
class Tool(ABC):
    name: str           # "search", "read_storage", "generate_report"
    description: str    # Agent 读这段文字来决定什么时候用它
    args_schema: dict   # OpenAI function-calling 参数格式

    @abstractmethod
    async def execute(self, **kwargs) -> str: ...
    # 返回 str（Observation 必须是文本）
```

### 内置 Tool

| Tool | name | 包装 | 参数 | Observation |
|------|------|------|------|-------------|
| SearchTool | `search` | `Adapter.search()` | `platform`, `keywords`, `limit` | JSON 列表摘要 |
| ReadStorageTool | `read_storage` | `Storage.load_items()` | `date`, `filters` | JSON 历史数据 |
| SaveStorageTool | `save_storage` | `Storage.save_items()` | `date`, `items_json` | "saved N items" |

**SearchTool 说明**（v4.1）：
  SearchTool 调用 Adapter.search()，Adapter 内部已使用 LLM 动态生成提取规则。
  Tool 层无感——它收到的仍是 `UnifiedItem[]`，只负责序列化为 JSON 摘要发给 Agent。

**搜索结果的 JSON 摘要格式**（Agent 不需要看到完整 UnifiedItem）：

```json
{
  "total": 8,
  "items": [
    {"id": "N001", "author": "科技观察者", "title": "GPT-5 发布",
     "likes": 12000, "url": "https://..."},
    ...
  ]
}
```

Tool 负责将 `UnifiedItem` 序列化为 Agent 可读的文本摘要。不传完整数据以节省 token。

---

## 6. 模块协作时序（新）

```
用户 → Web UI → API → Agent

Agent                           LLMEngine       Tool(search)      Tool(storage)
  │                                │               │                  │
  │── chat(prompt+tools) ────────►│               │                  │
  │◄── tool_calls: [search×2] ────│               │                  │
  │                                │               │                  │
  │── search("tw","AI模型") ───────────────────►│                  │
  │◄── "12条" ─────────────────────────────────│                  │
  │── search("rednote","大模型") ──────────────►│                  │
  │◄── "8条" ─────────────────────────────────│                  │
  │                                │               │                  │
  │── chat([...+结果]) ───────────►│               │                  │
  │◄── "够了，日报如下：\n# AI日报"│               │                  │
  │                                │               │                  │
  │── save_items(20条) ────────────────────────────────────►│
  │── save_report(markdown) ───────────────────────────────►│
  │◄── TaskResult ────────────────────────────────────────────────── API
```

每一步搜索/存储都是 Agent **自主决定**的，不是硬编码序列。

---

## 7. 启动流程

```
1. 加载 config.yaml (Config)
2. 初始化 wheel 模块（BrowserManager / LLMEngine / FileStorage / Logger）
3. 初始化 core 模块（Agent + Adapters）
4. Agent.start() → LLMEngine.start() → BrowserManager.start()
5. Scheduler.sync_from_config() → 添加定时任务
6. 启动 Web 服务 (FastAPI + uvicorn)
7. 可选：自动打开浏览器访问首页
```

---

## 8. 模块依赖关系（v4.2）

```
                         ┌──────────┐
                         │  Config  │
                         └────┬─────┘
                              │ LLMConfig 逐层透传
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
  ┌──────────┐        ┌──────────┐          ┌──────────────┐
  │ API Layer│        │  Agent   │          │   Platform   │   ← core
  └────┬─────┘        └──┬───┬───┘          │   Adapters   │
       │                 │   │              └──┬───────┬───┘
       │                 │   │                 │       │
       ▼                 ▼   ▼                 ▼       ▼
┌──────────────────────────────────────────────────────────┐
│                        wheel                             │
│                                                          │
│  LLMEngine ◄─────── BrowserManager    Scheduler          │
│     ▲          LLMAdapter._ensure_llm() 懒创建 engine     │
│     └──── LLMConfig 经 AdapterFactory 透传（不创建）      │
│                                                          │
│  FileStorage  Logger             Config                  │
└──────────────────────────────────────────────────────────┘
```

**依赖方向**：`core → wheel`（单向，不可逆）。

**工厂规则（硬）**：工厂是纯分发器，不持有、不创建任何运行时依赖。实现的依赖由实现自身内部创建（如 `LLMAdapter._ensure_llm()` 懒创建 `LLMEngine`），或由 `main.py` 直接传入实现构造器。工厂只在 import 层面接触 `imp/`。
  - `Agent → wheel`（调 LLMEngine/Storage/Adapter）
  - `LLMAdapter → LLMEngine`（通过 LLMEngineFactory 自建，LLMConfig 来自 main.py）
  - `API → Agent`（封装 HTTP 请求）
  - `Scheduler` 是通用定时回调器，不含 Agent import，绑定在 `main.py` 完成。

**Config 传递规则**：
  - 唯一读文件：`YamlConfigLoader`
  - 构建 LLMConfig：`main.py`（唯一 Composition Root）
  - 透传：`AdapterFactory`（只传不建 Engine）
  - 自建 Engine：`LLMAdapter._ensure_llm()`（从 LLMConfig 懒创建）

---

## 9. 实现阶段

| 阶段 | 内容 | 说明 |
|------|------|------|
| 1 | **LLMEngine 实现** | `src/wheel/llm/imp/deepseek_engine.py` — DeepSeekEngine（stdlib urllib + scheduler） ✅ |
| 2 | **LLMAdapter + 提取接入** | `src/core/adapters/imp/llm_adapter.py` — 中间父类；RedNoteAdapter 继承 LLMAdapter |
| 3 | **Agent 实现** | `src/core/agent/imp/react_agent.py`，ReAct 循环 |
| 4 | **Tool 具体实现** | SearchTool / StorageTool / ReportTool |
| 5 | **Scheduler 实现** | `src/wheel/scheduler/imp/apscheduler_impl.py`，通用定时回调器 |
| 6 | **API Layer 对接 Agent** | `POST /api/run` → `Agent.run()` |

---

## 10. 执行模型（线程模型）

**硬规则**：

1. **单一 asyncio 事件循环** — 所有 I/O 都跑在同一个 event loop 上。
2. **`wheel/scheduler/` 是唯一授权的线程池** — 其他模块禁止 import `threading`、`concurrent.futures`、`asyncio.to_thread`、`loop.run_in_executor`。
3. **阻塞工作** — 如果模块必须执行阻塞操作（CPU 密集 / 同步库），通过 `scheduler.run_blocking(func, *args, **kwargs)` 提交到 Scheduler 的 semaphore 管理线程池，返回 awaitable 取回结果。
4. **所有 I/O 必须异步** — `aiofiles` / async Playwright / `httpx` 等。

**架构理由**：单事件循环避免竞态条件、简化调试。Scheduler 集中管理线程资源防止线程泄漏。`run_blocking` 提供统一的异步→同步桥接，让 CPU 工作和 I/O 工作在同一调度框架下。

---

*文档版本：v4.2 | 最后更新：2026-04-26*
