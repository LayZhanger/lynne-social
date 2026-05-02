# Lynne 总体设计方案 v4.2

> **v4.2 核心变更**：引入 `LLMAdapter` 中间父类，统一 LLM 提取逻辑。LLMConfig 从 Config 逐层透传（JsonConfigLoader → main.cpp → AdapterFactory → LLMAdapter），LLMAdapter 内部自建 LLMEngine。工厂只传配置不创建 Engine。

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
| W1  | **BrowserManager** | wheel | CDP 生命周期 + 会话管理 (待实现) |
| W2  | **LLMEngine** | wheel | 裸 LLM 调用：`chat(messages, tools?, on_ok, on_err)` ✅ |
| W3  | **FileStorage** | wheel | JSONL / Markdown / JSON 文件读写 ✅ |
| W4  | **Config** | wheel | 配置加载、校验 ✅ |
| W5  | **Scheduler** | wheel | libuv 定时任务调度 ✅ |
| W6  | **Logger** | wheel | spdlog 日志记录 ✅ |
| W7  | **WsClient** | wheel | IXWebSocket 封装 ✅ |

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
  class Agent : public common::Module {

    // 异步回调模式（替代 async/await）
    void run(
        const std::string& intent,
        const std::vector<std::string>& platforms,
        std::function<void(TaskResult)> on_done,
        std::function<void(const std::string&)> on_error,
        int max_steps = 10
    );
  };

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

 类型定义（C++ struct）：

  TaskConfig {
      std::string name;
      std::vector<std::string> platforms;
      std::string intent;              // 自然语言关注主题
      std::string schedule;            // "every 4 hours" | "at 08:00" | "manual"
  };

  TaskResult {
      std::string task_name;
      int fetched_count;
      int kept_count;
      int llm_calls;
      std::string report_markdown;
      double duration_seconds;
      std::vector<AgentStep> steps;
      std::vector<UnifiedItem> items;
  };

  AgentStep {
      int step;
      std::string thought;       // "目前搜到15条，信息不够"
      std::string action;        // search(platform="rednote", keywords="AI 突破")
      std::string observation;   // "搜到 8 条新结果，其中 3 条相关"
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
         ├─ 缓存（URL host + path → JS extract function）
         │
         ├─→ RedNoteAdapter(LLMAdapter)    ← 平台 shell
         └─→ (未来) TwitterAdapter         ← 平台 shell

Config 传递链（Config → Engine，单向）：
  config.json
    → JsonConfigLoader.load()  ← 唯一读文件的地方
      → Config.llm  (struct)
        → main.cpp: LLMConfig 转换
          → AdapterFactory::create(browser, adapter_config, llm_config)
            → RedNoteAdapter(browser, config, llm_config)

关键规则：
  - 只有 JsonConfigLoader 读 config.json
  - AdapterFactory 无状态，零依赖注入 —— create(browser, config, llm_config)
  - LLMAdapter 内部使用 LLMFactory 自建 engine（懒初始化）
  - llm_config 为空时不创建 LLM → 走 CDP DOM 回退路径

提取策略：
  ┌─ LLM 路径（llm_config 不为空）─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐
  │  CDP Page.navigate(url)                                           │
  │    → CDP Runtime.evaluate(SKELETON_JS) 提取前 N 个帖子骨架          │
  │    → LLMAdapter::ensure_llm() 懒启动 DeepSeekEngine               │
  │    → LLM 分析骨架，生成 JS extractPosts() 函数                     │
  │    → CDP Runtime.evaluate(extractPosts) 批量提取                    │
  │    → 缓存（按 URL host + path），后续翻页零 LLM 调用               │
  └─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘

  ┌─ CDP DOM 回退路径（llm_config 为空或 LLM 生成失败）─ ─ ─ ─ ┐
  │  硬编码 CDP DOM.querySelectorAll 选择器，行为不变              │
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

文件：src/core/adapters/imp/llm_adapter.h / llm_adapter.cpp

核心属性：
  BrowserManager* browser_;         ← 注入
  AdapterConfig config_;            ← 注入
  LLMConfig llm_config_;            ← 注入（注意：不是 LLMEngine！）
  LLMEngine* llm_ = nullptr;       ← ensure_llm() 懒创建
  std::map<std::string, std::string> extract_fn_cache_; ← URL pattern → JS extract function

懒初始化：
  void LLMAdapter::ensure_llm() {
      if (!llm_ && !llm_config_.api_key.empty()) {
          LLMFactory factory;
          llm_ = factory.create(llm_config_);
          llm_->start();
      }
  }
  
  llm_config 由 AdapterFactory 透传（来自 main.cpp → Config.llm）。
  llm_config.api_key 为空时 ensure_llm() 不创建 Engine → 自动走 CDP DOM 回退。

LLM 提取协议：

[1] 输入：页面骨架 (CDP Runtime.evaluate → json)
   通用 DOM walker（不依赖平台选择器）遍历 DOM，找包含子文本、链接、图片的容器节点。
   只取前 3 个帖子的结构样本（~500 tokens），发送给 LLM。

   格式：[{
     "tag":"section", "cls":"note-item", "nth":1,
     "children":[
       {"sel":"div.title>span",        "txt":"GPT-5 发布"},
       {"sel":"div.author span.name",  "txt":"科技观察者"},
       ...
     ]}, ...
   ]

   提示词（send to LLM）：
     "你是浏览器自动化专家。根据以下页面骨架生成 JavaScript 函数 extractPosts()。
      函数遍历 DOM，提取每个帖子的字段：item_id, title, author_name, content,
      likes, url, images。
      字段名必须是英文 camelCase，值必须是字符串或数组。
      返回纯 JavaScript 代码，不含 ``` 标记，不含注释。"

[2] 输出：JS 提取函数 (LLM → Adapter)
   通过 CDP Runtime.evaluate(extractPosts) 执行，结果解析为 UnifiedItem。

[3] 输出：UnifiedItem (CDP Runtime.evaluate → json → UnifiedItem)
   字段映射与现有逻辑一致。
```



### W1. BrowserManager（wheel，待实现）

```
职责：CDP 生命周期 + 会话管理。

  class BrowserManager : public Module {
      void start() override;
      void stop() override;
      // CDP 命令发送（回调模式）
      void evaluate(
          const std::string& js,
          std::function<void(nlohmann::json)> on_result,
          std::function<void(const std::string&)> on_error
      ) = 0;
      void navigate(const std::string& url, std::function<void()> on_done) = 0;
      // 会话管理
      void save_session(const std::string& platform,
                        std::function<void()> on_done) = 0;
      void login_flow(const std::string& platform, const std::string& url) = 0;
      bool health_check() override;
  };

实现：cdp_browser_manager.h / .cpp
  - 子进程启动 chrome --headless --remote-debugging-port=9222
  - IXWebSocket 连接 CDP WebSocket (ws://.../devtools/browser/...)
  - JSON-RPC via WebSocket: {"id":1, "method":"Page.navigate", "params":{...}}
  - Session 持久化：CDP Storage.getCookies + session 文件
```

### W2. LLMEngine（wheel ✅）

```
职责：封装大模型调用。v4.0 删除 filter/extract/generate_report，
     只保留一个裸调用方法。

接口：
  class LLMEngine : public Module {
      // 回调模式（替代 async/await）
      void chat(
          const nlohmann::json& messages,
          std::function<void(nlohmann::json)> on_result,
          std::function<void(const std::string&)> on_error,
          const nlohmann::json& tools = nullptr
      ) = 0;
      virtual void step() = 0;   // 驱动 libuv 事件循环一轮
      virtual void run() = 0;    // 阻塞运行，直到 stop()
  };

  struct LLMConfig {
      std::string provider;      // "deepseek" | "openai" | "custom"
      std::string api_key;
      std::string base_url;
      std::string model;
      double temperature = 0.7;
      int max_tokens = 4096;
      int timeout = 60;
      std::string ca_cert_path;  // SSL 证书路径（自动检测）
  };

与 v3.1 的差异：
  - 删除 filter() / extract() / generate_report()
  - 删除 model_filter / model_extract / model_report
  - 删除 Jinja2 模板目录
  - 新增 tools 参数（支持 function-calling）
  - 新增 ca_cert_path（SSL 证书自动检测）

实现：cpp-httplib::SSLClient + scheduler->run_blocking()
  文件：src/wheel/llm/imp/deepseek_engine.h / .cpp
```

### W3. FileStorage（wheel ✅）

```
职责：不变。每天一个目录，JSONL 格式存储。

接口：
  class Storage : public Module {
      void save_items(const std::vector<UnifiedItem>& items,
                      const std::string& date) = 0;
      std::vector<UnifiedItem> load_items(const std::string& date,
                                          const std::string& platform = "") = 0;
      void save_report(const std::string& markdown,
                       const std::string& date) = 0;
      std::string load_report(const std::string& date) = 0;
      void save_summary(const nlohmann::json& summary,
                        const std::string& date) = 0;
      nlohmann::json load_summary(const std::string& date) = 0;
      std::vector<std::string> list_dates() = 0;
  };

  实现：同步文件 I/O（std::ifstream/ofstream），不需要 scheduler
```

### W4. Config（wheel ✅）

```
职责：不变。

接口：
  class ConfigLoader : public Module {
      Config* load(const std::string& path = "config.json") = 0;
      Config* reload() = 0;
  };

  struct Config {
      ServerConfig server;
      LLMConfig llm;
      BrowserConfig browser;
      std::map<std::string, PlatformConfig> platforms;
      std::vector<TaskConfig> tasks;
  };

  实现：JsonConfigLoader（nlohmann::json::parse + std::ifstream）
  支持：DEEPSEEK_API_KEY 环境变量回退、SSL_CERT_FILE 自动检测
```

### W5. Scheduler（wheel ✅）

```
职责：通用 libuv 调度器。管理定时任务，按 interval 触发 callback。不对 Agent 有任何导入。

接口：
  class Scheduler : public Module {
      void run_blocking(std::function<void()> work,
                        std::function<void()> on_done) = 0;
      void post(std::function<void()> callback) = 0;
      void add_job(const std::string& name, int interval_ms,
                   std::function<void()> callback) = 0;
      void remove_job(const std::string& name) = 0;
  };

libuv 映射：
  run_blocking → uv_queue_work （libuv 内置 4 线程池）
  post          → uv_async_send （跨线程唤醒事件循环）
  add_job       → uv_timer_start（重复定时器）
  remove_job    → uv_timer_stop + uv_close

Agent 的绑定在 main.cpp 完成：
  scheduler->add_job("AI日报", 4*3600*1000, [agent](){
      agent->run(intent, platforms, on_done, on_error);
  });
```

### W6. Logger（wheel ✅）

```
职责：spdlog 封装，支持模块化 logger。

  void init_logger(const LogConfig& config);
  // 全局 spdlog::get("module") 使用
  日志文件：data/lynne.log
  脱敏：API Key 自动过滤
```

### W7. WsClient（wheel ✅）

```
职责：IXWebSocket 封装，提供统一的 WebSocket 客户端接口。

接口：
  class WsClient : public Module {
      void connect(const std::string& url,
                   std::function<void()> on_connected) = 0;
      void disconnect() = 0;
      void send(const std::string& message) = 0;
      // 事件回调注册
      void on_message(std::function<void(const std::string&)>) = 0;
      void on_error(std::function<void(const std::string&)>) = 0;
      // 状态查询
      bool ready_state() const = 0;
      // 事件循环驱动（libuv 集成）
      void step() = 0;
      void run() = 0;
  };

实现：IxWsClient（IXWebSocket wrapper）
  - 构造函数注入 Scheduler*（用于 cross-thread callback 桥接）
  - disableAutoReconnect()（由框架层管理重连）
  - 线程安全的消息队列

应用：CDP 浏览器通信、WebSocket API
```

---

## 5. Tool 系统

Agent 通过 Tool 与外界交互。每个 Tool 包装一个已开发的模块方法。

```cpp
class Tool {
public:
    std::string name;           // "search", "read_storage", "generate_report"
    std::string description;    // Agent 读这段文字来决定什么时候用它
    nlohmann::json args_schema; // OpenAI function-calling 参数格式

    virtual ~Tool() {}
    virtual void execute(const nlohmann::json& args,
                         std::function<void(const std::string&)> on_result,
                         std::function<void(const std::string&)> on_error) = 0;
    // 返回 str（Observation 必须是文本）
};
```

### 内置 Tool

| Tool | name | 包装 | 参数 | Observation |
|------|------|------|------|-------------|
| SearchTool | `search` | `Adapter.search()` | `platform`, `keywords`, `limit` | JSON 列表摘要 |
| ReadStorageTool | `read_storage` | `Storage.load_items()` | `date`, `filters` | JSON 历史数据 |
| SaveStorageTool | `save_storage` | `Storage.save_items()` | `date`, `items_json` | "saved N items" |

**SearchTool 说明**（v4.1）：
  SearchTool 调用 Adapter->search()，Adapter 内部已使用 LLM 动态生成提取规则。
  Tool 层无感——它收到的仍是 `std::vector<UnifiedItem>`，只负责序列化为 JSON 摘要发给 Agent。

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
1. 加载 config.json (Config)
2. 初始化 wheel 模块（LLMEngine / FileStorage / Logger / Scheduler / WsClient）
3. 初始化 core 模块（BrowserManager + Agent + Adapters）
4. Agent.start() → LLMEngine.start() → BrowserManager.start()
5. Scheduler.add_job() → 添加定时任务
6. 启动 HTTP 服务（cpp-httplib 或类似）
7. uv_run(uv_default_loop(), UV_RUN_DEFAULT) 进入事件循环
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

**工厂规则（硬）**：工厂是纯分发器，不持有、不创建任何运行时依赖。实现的依赖由实现自身内部创建（如 `LLMAdapter::ensure_llm()` 懒创建 `LLMEngine`），或由 `main.cpp` 直接传入实现构造器。工厂只在 include 层面接触 `imp/`。
  - `Agent → wheel`（调 LLMEngine/Storage/Adapter）
  - `LLMAdapter → LLMEngine`（通过 LLMFactory 自建，LLMConfig 来自 main.cpp）
  - `API → Agent`（封装 HTTP 请求）
  - `Scheduler` 是通用定时回调器，不含 Agent include，绑定在 `main.cpp` 完成。

**Config 传递规则**：
  - 唯一读文件：`JsonConfigLoader`
  - 构建 LLMConfig：`main.cpp`（唯一 Composition Root）
  - 透传：`AdapterFactory`（只传不建 Engine）
  - 自建 Engine：`LLMAdapter::ensure_llm()`（从 LLMConfig 懒创建）

---

## 9. 实现阶段

| 阶段 | 模块 | 状态 |
|------|------|------|
| 1 | common (Module ABC + UnifiedItem) | ✅ C++ |
| 2 | wheel/config (JsonConfigLoader) | ✅ C++ |
| 3 | wheel/logger (SpdlogLogger) | ✅ C++ |
| 4 | wheel/storage (JsonlStorage) | ✅ C++ |
| 5 | wheel/scheduler (UvScheduler, libuv) | ✅ C++ |
| 6 | wheel/llm (DeepSeekEngine, HTTP + SSL) | ✅ C++ |
| 7 | wheel/ws_client (IxWsClient, IXWebSocket) | ✅ C++ |
| 8 | wheel/browser (CdpBrowserManager, CDP) | 待实现 |
| 9 | core/adapters (RedNoteAdapter + LLMAdapter) | 待实现 |
| 10 | core/agent (ReAct Agent + Tools) | 待实现 |
| 11 | src/main.cpp (Composition Root) | 待实现 |
| 12 | API Layer + Web UI | 待实现 |

---

## 10. 执行模型（线程模型）

```
┌─────────────────────────────────────────────────────────┐
│                    uv_default_loop()                     │
│                                                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ uv_timer     │  │ uv_async     │  │ uv_tcp       │  │
│  │ (定时任务)   │  │ (跨线程唤醒) │  │ (CDP WS连接) │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│                                                         │
│  ┌──────────────────────────────────────────────────┐   │
│  │         uv_queue_work (线程池, 默认4线程)          │   │
│  │  ├─ LLM HTTP 调用 (cpp-httplib)                   │   │
│  │  ├─ JSONL 文件写入                                │   │
│  │  └─ 其他阻塞操作                                   │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

**硬规则**：

1. **单一 libuv 事件循环** — 所有 I/O 注册到 `uv_default_loop()`，一切异步操作通过 libuv 原语完成。
2. **`wheel/scheduler/` 是唯一授权的线程池** — 其他模块禁止创建 `std::thread`、调用 `std::async`、或直接使用 `uv_queue_work`。
3. **阻塞工作** — 只能通过 `scheduler.run_blocking(work_fn, on_done_fn)` 提交到 libuv 工作线程池，禁止直接 `std::thread`。
4. **定时任务** — 只能用 `uv_timer_start`，禁止 `std::this_thread::sleep_for`。
5. **跨线程回调** — 用 `uv_async_send` 从工作线程通知主循环。
6. **所有 I/O 必须 async** — IXWebSocket（CDP/WebSocket）、uv_timer、uv_async。文件 I/O 走 `uv_queue_work` 或直接同步（启动阶段）。

**架构理由**：单事件循环避免竞态条件、简化调试。Scheduler 集中管理线程资源防止线程泄漏。`run_blocking` 提供统一的阻塞→异步桥接。

---

*文档版本：v4.2-cpp | 最后更新：2026-05-02*
