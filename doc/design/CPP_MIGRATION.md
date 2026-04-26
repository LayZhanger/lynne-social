# Lynne C++ 迁移方案 v0.2

> 将现有 Python 实现（~1800 行/52 文件）迁移为 C++（预估~2200 行），保留原分层架构（core → wheel → common）。
>
> **语法约束**：禁泛型、禁 `std::future<T>` / `std::promise`、禁 `std::thread` 裸创。允许 lambda、`std::function`、裸指针、`std::vector`/`std::map` 等具体容器。

---

## 1. 核心决策

| 维度 | Python v0.2 | C++ 目标 |
|------|------------|----------|
| 语言标准 | Python 3.11 | C++17 |
| 构建系统 | pip + setuptools | CMake 3.16+ |
| 配置格式 | YAML (`${ENV_VAR}`) | JSON (`${ENV_VAR}`) |
| 数据建模 | Pydantic BaseModel | 普通 struct + `nlohmann/json` 宏 |
| 事件循环 | asyncio | libuv (v1.48) |
| 线程池 | scheduler ThreadPool | `uv_queue_work` (libuv 内置) |
| 定时任务 | scheduler + threading | `uv_timer` |
| HTTP 客户端 | urllib + scheduler.run_blocking | cpp-httplib + `uv_queue_work` |
| 浏览器自动化 | Playwright | **裸 CDP** via WebSocket (IXWebSocket) |
| 日志 | Loguru | spdlog |
| 测试框架 | pytest + pytest-asyncio | GTest |
| 文件 I/O | pathlib (同步) | `std::fstream` / `uv_fs_read` |
| 序列化 | PyYAML + json | `nlohmann/json` |
| 抽象基类 | ABC | 纯虚类 (virtual = 0) |
| 工厂模式 | Factory[T] 泛型 | 具体工厂类，返回裸指针 |

---

## 2. 外部依赖

所有依赖以源码形式存放在 `third_party/` 下，通过 CMake `FetchContent` 或直接 `add_subdirectory` 引入，不依赖系统包管理器。

| # | 库 | 版本 | 类型 | 引入方式 | 大小 |
|---|-----|------|------|---------|------|
| 1 | `nlohmann/json` | v3.11.3 | 单头 | `wget` 下 `json.hpp` | ~900KB |
| 2 | `spdlog` | v1.14.1 | 全头 | `git clone` + `add_library(INTERFACE)` | ~2MB |
| 3 | `libuv` | v1.48.0 | C 源码 | `git clone` + `add_subdirectory` | ~3MB |
| 4 | `cpp-httplib` | v0.17.0 | 单头 | `wget` 下 `httplib.h` | ~300KB |
| 5 | `IXWebSocket` | v11.4.6 | 头+cp | `git clone` + `add_subdirectory` | ~1MB |
| 6 | `googletest` | v1.14.0 | 源码 | `git clone` + `add_subdirectory` | ~6MB |

### 获取命令

```bash
mkdir -p third_party && cd third_party

# 1. nlohmann/json
mkdir -p nlohmann && cd nlohmann
wget -q https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
cd ..

# 2. spdlog
git clone --depth 1 --branch v1.14.1 https://github.com/gabime/spdlog.git

# 3. libuv
git clone --depth 1 --branch v1.48.0 https://github.com/libuv/libuv.git

# 4. cpp-httplib
mkdir -p cpp-httplib && cd cpp-httplib
wget -q https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.17.0/httplib.h
cd ..

# 5. IXWebSocket
git clone --depth 1 --branch v11.4.6 https://github.com/machinezone/IXWebSocket.git

# 6. Google Test
git clone --depth 1 --branch v1.14.0 https://github.com/google/googletest.git
```

---

## 3. 项目目录结构

```
lynne-cpp/
├── CMakeLists.txt                 # 顶层 CMake
├── config.json                    # 运行时配置（原 config.yaml）
├── data/                          # 运行时数据目录
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp                   # Composition Root（原 main.py）
│   ├── common/
│   │   ├── module.h               # Module 抽象基类
│   │   ├── models.h               # UnifiedItem 等通用模型
│   │   └── factory.h              # 简化工厂模板（可选）
│   ├── wheel/
│   │   ├── config/
│   │   │   ├── config_loader.h     # ConfigLoader 抽象接口
│   │   │   ├── config_models.h    # Config/ServerConfig/LLMConfig 等 struct
│   │   │   ├── config_factory.h   # create_config_loader()
│   │   │   └── imp/
│   │   │       ├── json_config_loader.h
│   │   │       └── json_config_loader.cpp
│   │   ├── logger/
│   │   │   ├── logger.h
│   │   │   └── logger.cpp
│   │   ├── storage/
│   │   │   ├── storage.h          # Storage 抽象接口
│   │   │   ├── storage_models.h
│   │   │   ├── storage_factory.h
│   │   │   └── imp/
│   │   │       ├── jsonl_storage.h
│   │   │       └── jsonl_storage.cpp
│   │   ├── scheduler/
│   │   │   ├── scheduler.h        # Scheduler 抽象接口（libuv 封装）
│   │   │   ├── scheduler_models.h
│   │   │   ├── scheduler_factory.h
│   │   │   └── imp/
│   │   │       ├── uv_scheduler.h
│   │   │       └── uv_scheduler.cpp
│   │   ├── llm/
│   │   │   ├── llm_engine.h       # LLMEngine 抽象接口
│   │   │   ├── llm_models.h
│   │   │   ├── llm_factory.h
│   │   │   └── imp/
│   │   │       ├── deepseek_engine.h
│   │   │       └── deepseek_engine.cpp
│   │   ├── browser/
│   │   │   ├── browser_manager.h  # BrowserManager 抽象接口
│   │   │   ├── browser_models.h
│   │   │   ├── browser_factory.h
│   │   │   └── imp/
│   │   │       ├── cdp_browser_manager.h
│   │   │       └── cdp_browser_manager.cpp
│   └── core/
│       ├── adapters/
│       │   ├── base_adapter.h
│       │   ├── adapter_models.h
│       │   ├── adapter_factory.h
│       │   └── imp/
│       │       ├── llm_adapter.h / .cpp
│       │       ├── rednote_adapter.h / .cpp
│       └── agent/
│           ├── agent.h / .cpp
│           ├── agent_models.h
│           ├── agent_factory.h
│           └── tools.h / .cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── common/
│   ├── wheel/
│   │   ├── config/
│   │   ├── logger/
│   │   ├── storage/
│   │   ├── scheduler/
│   │   ├── llm/
│   │   └── browser/
│   ├── core/
│   │   ├── adapters/
│   │   └── agent/
│   └── main.cpp                   # 测试入口（GTest）
└── third_party/                   # 外部依赖源码
    ├── nlohmann/
    │   └── json.hpp
    ├── spdlog/
    ├── libuv/
    ├── cpp-httplib/
    │   └── httplib.h
    ├── IXWebSocket/
    └── googletest/
```

---

## 4. CMake 构建体系

### 顶层 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(lynne VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 单头文件 include 路径
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party)

# 需编译的库
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${THIRD_PARTY_DIR}/libuv)
add_subdirectory(${THIRD_PARTY_DIR}/IXWebSocket)

add_subdirectory(src)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

### src/CMakeLists.txt

```cmake
# 头文件搜索路径
set(COMMON_INCLUDES
    ${THIRD_PARTY_DIR}/nlohmann
    ${THIRD_PARTY_DIR}/cpp-httplib
    ${THIRD_PARTY_DIR}/spdlog/include
    ${THIRD_PARTY_DIR}/IXWebSocket
    ${THIRD_PARTY_DIR}/libuv/include
)

# 各模块编译为 OBJECT 或 STATIC 库
add_subdirectory(common)
add_subdirectory(wheel/config)
add_subdirectory(wheel/logger)
add_subdirectory(wheel/storage)
add_subdirectory(wheel/scheduler)
add_subdirectory(wheel/llm)
add_subdirectory(wheel/browser)
add_subdirectory(core/adapters)
add_subdirectory(core/agent)

# 主可执行文件（Composition Root）
add_executable(lynne main.cpp)
target_include_directories(lynne PRIVATE ${COMMON_INCLUDES})
target_link_libraries(lynne PRIVATE
    lynne_common
    lynne_config
    lynne_logger
    lynne_storage
    lynne_scheduler
    lynne_llm
    lynne_browser
    lynne_adapters
    lynne_agent
    uv_a
    ixwebsocket
)
```

### tests/CMakeLists.txt

```cmake
add_subdirectory(${THIRD_PARTY_DIR}/googletest gtest)

add_executable(test_config
    wheel/config/test_config_loader.cpp
)
target_include_directories(test_config PRIVATE ${COMMON_INCLUDES})
target_link_libraries(test_config
    lynne_config
    gtest gtest_main
)
add_test(NAME config COMMAND test_config)

# ... 其他测试目标类似
```

---

## 5. 模块迁移对照

### M1. common（~100 行 Python → ~100 行 C++）

| Python | C++17 |
|--------|-------|
| `class Module(ABC)` | `class Module { virtual ~Module(){} virtual void start() = 0; virtual void stop() = 0; virtual bool health_check() = 0; virtual const char* name() = 0; };` |
| `class Factory[T]` 泛型 | 具体工厂类，`create()` 返回裸指针（调用方负责 `delete` 或用 `std::unique_ptr`） |
| `UnifiedItem` Pydantic 模型 | `struct UnifiedItem` + `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` |
| `@abstractmethod` | `virtual ... = 0;` |
| `async def` 方法 | 纯虚方法 + 回调参数 `std::function<void(Result)> on_done` |

**异步回调模式**（替代 `async/await` + `std::future`）：

```cpp
// 所有异步操作用回调通知完成，接受 lambda
using ResultCallback = std::function<void(Config)>;
using ErrorCallback  = std::function<void(const std::string&)>;

class ConfigLoader {
public:
    virtual ~ConfigLoader() {}
    virtual void load(ResultCallback on_ok, ErrorCallback on_err) = 0;
    virtual void reload(ResultCallback on_ok, ErrorCallback on_err) = 0;
    virtual Config* config() = 0;
};
```

### M2. config（~130 行 Python → ~150 行 C++）

| Python | C++17 |
|--------|-------|
| `Config(BaseModel)` | `struct Config` + `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Config, server, llm, browser, platforms, tasks)` |
| `ConfigLoader(ABC)` | `class ConfigLoader { virtual Config load() = 0; virtual Config reload() = 0; }` |
| `ConfigLoaderFactory(Factory)` | 具体工厂类 `ConfigLoaderFactory`，`create()` 返回 `ConfigLoader*` |
| `yaml.safe_load()` | `nlohmann::json::parse(ifstream)` |
| `Pydantic.model_validate()` | 手动逐字段赋值到 struct（或 `json.get<Config>()`，nlohmann 宏非泛型用户代码） |
| `_resolve_env` regex | `std::regex R"(\$\{(\w+)\})"` + `getenv()` |
| `pathlib.Path.read_text()` | `std::ifstream` + `std::istreambuf_iterator` |
| `scheduler.run_blocking` | 启动阶段直接同步读文件即可，不阻塞事件循环 |

**关键片段**：
```cpp
// config_models.h
struct Config {
    ServerConfig server;
    LLMConfig llm;
    BrowserConfig browser;
    std::map<std::string, PlatformConfig> platforms;
    std::vector<TaskConfig> tasks;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Config, server, llm, browser, platforms, tasks)
```

### M3. logger（~40 行 Python → ~80 行 C++）

| Python | C++17 |
|--------|-------|
| `loguru.logger` | `spdlog::logger` |
| `get_logger("module")` | `spdlog::get("module")` 或全局 `spdlog::default_logger()` |
| `logger.info("...", args)` | `spdlog::info("...", args)` (fmt 风格) |
| 脱敏 API Key → `sensitive_filter` | `spdlog::set_pattern()` 或自定义 sink |

**关键片段**：
```cpp
// logger.h
class Logger {
public:
    static void init(const std::string& log_dir = "data");
    static std::shared_ptr<spdlog::logger> get(const std::string& name);
};
```

### M4. storage（~140 行 Python → ~150 行 C++）

| Python | C++17 |
|--------|-------|
| `json.loads(line)` | `nlohmann::json::parse(line)` |
| `pathlib.Path.mkdir()` | `std::filesystem::create_directories()` |
| `pathlib.Path.glob("*.jsonl")` | `std::filesystem::directory_iterator` |
| `async def save_items()` | 同步 `save_items()` + 外层包 `uv_queue_work` |
| `List[UnifiedItem]` | `std::vector<UnifiedItem>` |

### M5. scheduler（~230 行 Python → ~250 行 C++）

| Python | C++17 (libuv) |
|--------|------|
| `asyncio.get_running_loop()` | `uv_default_loop()` |
| `threading.Thread` → wrap blocking | `uv_queue_work(loop, &req, work_cb, after_work_cb)` |
| `threading.Semaphore` | `uv_sem_t` |
| `threading.RLock` | `std::recursive_mutex` |
| `threading.Thread` + `daemon=True` 轮询 | `uv_timer_start(loop, &timer, callback, timeout, repeat)` |
| `scheduler.run_blocking(fn)` → `await` | 回调模式：`run_blocking(fn, [](Result r){ /* done */ })` |
| `scheduler.add_job(name, schedule, cb)` | `uv_timer_start` 绑定回调 |
| `self._loop` 后台轮询线程 | 不需要！`uv_run()` 本身就是事件循环 |

**回调模式替代 async/await**：

```cpp
// uv_scheduler.h
class UvScheduler : public Scheduler {
    uv_loop_t* loop_;
    std::map<std::string, uv_timer_t> timers_;
    std::recursive_mutex mutex_;

public:
    void start() override;
    void stop() override;

    // 定时任务 —— 回调是普通 std::function
    void add_job(const std::string& name, const std::string& schedule,
                 std::function<void()> callback) override;

    void remove_job(const std::string& name) override;

    // 异步阻塞工作 —— 结果通过回调通知（不用 std::future）
    // work 在 libuv 线程池执行，on_done 在主线程回调
    void run_blocking(
        std::function<void()> work,
        std::function<void()> on_done
    );
};
```

**run_blocking 实现**（用 `uv_queue_work` + lambda）：

```cpp
struct WorkCtx {
    std::function<void()> work;
    std::function<void()> on_done;
};

static void work_cb(uv_work_t* req) {
    auto* ctx = static_cast<WorkCtx*>(req->data);
    ctx->work();    // 在工作线程执行阻塞逻辑
}

static void after_cb(uv_work_t* req, int status) {
    auto* ctx = static_cast<WorkCtx*>(req->data);
    if (status == 0) ctx->on_done();  // 回到主线程回调
    delete ctx;
    delete req;
}

void UvScheduler::run_blocking(
    std::function<void()> work,
    std::function<void()> on_done
) {
    auto* req = new uv_work_t;
    auto* ctx = new WorkCtx{std::move(work), std::move(on_done)};
    req->data = ctx;
    uv_queue_work(loop_, req, work_cb, after_cb);
}
```

> `uv_queue_work` 在 libuv 默认 4 线程池中执行 work，完成后 `after_cb` 回到事件循环线程。调用方用 lambda 捕获上下文：
> ```cpp
> scheduler->run_blocking(
>     [&](){ result = api_call(); },           // 工作线程
>     [&](){ spdlog::info("done: {}", result); }  // 主线程
> );
> ```

### M6. llm（~130 行 Python → ~120 行 C++）

| Python | C++17 |
|--------|-------|
| `urllib.request` | `cpp-httplib::Client("https://api.deepseek.com")` |
| `scheduler.run_blocking` 包同步 HTTP | `UvScheduler::run_blocking(work, on_done)` 回调模式 |
| `async def chat()` → `dict` | `void chat(messages, tools, on_ok, on_err)` |

**关键片段**（回调模式）：

```cpp
// llm_engine.h
class LLMEngine : public Module {
public:
    using Json = nlohmann::json;
    // 结果通过 on_ok / on_err 回调
    virtual void chat(
        const Json& messages,
        const Json* tools,   // nullable
        std::function<void(const Json&)> on_ok,
        std::function<void(const std::string&)> on_err
    ) = 0;
};

// deepseek_engine.cpp
void DeepSeekEngine::chat(
    const Json& messages, const Json* tools,
    std::function<void(const Json&)> on_ok,
    std::function<void(const std::string&)> on_err
) {
    scheduler_->run_blocking(
        // work thread: 同步 HTTP 调用
        [this, messages, tools, on_ok, on_err]() {
            httplib::Client cli(base_url_);
            cli.set_default_headers({
                {"Authorization", "Bearer " + api_key_},
                {"Content-Type", "application/json"}
            });
            auto res = cli.Post("/v1/chat/completions", body_, "application/json");
            if (res && res->status == 200) {
                result_ = json::parse(res->body);
                ok_ = true;
            } else {
                err_msg_ = res ? res->body : "connection failed";
                ok_ = false;
            }
        },
        // after work (main loop): 回调调用方
        [this, on_ok, on_err]() {
            if (ok_) on_ok(result_);
            else on_err(err_msg_);
        }
    );
}
```

### M7. browser（~220 行 Python → ~500-800 行 C++）

**这是迁移中工程量最大的模块**。Playwright 没有 C++ 绑定，替代方案为直接通过 Chrome DevTools Protocol (CDP) 操控 headless Chrome。

| Python (Playwright) | C++17 (裸 CDP) |
|---------------------|----------------|
| `playwright.chromium.launch()` | 子进程启动 `chrome --headless --remote-debugging-port=9222` |
| `browser.new_context()` | CDP `Target.createTarget` + `Target.attachToTarget` |
| `context.new_page()` | CDP `Target.createTarget(url="about:blank")` |
| `page.goto(url)` | CDP `Page.navigate(url)` |
| `page.evaluate(js)` | CDP `Runtime.evaluate(expression=js)` |
| `page.close()` | CDP `Page.close` |
| `context.storage_state()` | CDP `Storage.getCookies` + `IndexedDB` 相关 |
| `playwright_stealth.Stealth` | 手动 CDP `Page.addScriptToEvaluateOnNewDocument` |
| `browser.close()` | 关闭子进程 + CDP `Browser.close` |

#### CDP 通信流程

```
C++ 程序
  │
  ├─ 1. 启动子进程
  │    chrome --headless --disable-gpu --remote-debugging-port=9222 --no-sandbox
  │
  ├─ 2. HTTP GET http://localhost:9222/json/version
  │    获取 webSocketDebuggerUrl: "ws://localhost:9222/devtools/browser/xxx"
  │
  ├─ 3. IXWebSocket 连接 ws://localhost:9222/devtools/browser/xxx
  │
  ├─ 4. 发送 CDP 命令 (JSON-RPC via WebSocket)
  │    {"id":1, "method":"Page.navigate", "params":{"url":"https://example.com"}}
  │    收到响应
  │    {"id":1, "result":{"frameId":"ABC123"}}
  │
  └─ 5. 程序退出时 kill 子进程
```

#### WebSocket 选择

| 库 | 特点 |
|----|------|
| **IXWebSocket** ★1500 | 项目活跃，内置 libuv 集成选项，API 简洁 |
| Boost.Beast | Boost 全家桶，太重 |
| websocketpp ★4100 | 较老，仅 WebSocket，需额外事件循环 |

选 **IXWebSocket**，因为它可以无缝嵌入 libuv 事件循环。

**关键片段**：
```cpp
class CdpBrowserManager : public BrowserManager {
    std::unique_ptr<ix::WebSocket> ws_;
    int cmd_id_ = 1;
    std::map<int, std::promise<nlohmann::json>> pending_;

    // 发送 CDP 命令并等待结果
    nlohmann::json send(const std::string& method, nlohmann::json params = {}) {
        int id = cmd_id_++;
        nlohmann::json msg = {{"id", id}, {"method", method}, {"params", params}};
        std::promise<nlohmann::json> p;
        auto f = p.get_future();
        pending_[id] = std::move(p);
        ws_->send(msg.dump());
        return f.get();  // 阻塞等待响应
    }

    // launch chrome subprocess
    void launch_chrome() {
        pid_t pid = fork();
        if (pid == 0) {
            execlp("chromium", "chromium",
                   "--headless", "--disable-gpu",
                   "--remote-debugging-port=9222",
                   "--no-sandbox", nullptr);
        }
        // wait for port 9222 to be ready...
    }
};
```

### M8. adapters（~730 行 Python → ~600-800 行 C++）

主要依赖 browser 模块（CDP），核心改动是 `AsyncIterator[UnifiedItem]` → `std::vector<UnifiedItem>` 回调或同步批量返回。

| Python | C++17 |
|--------|-------|
| `async def search() -> AsyncIterator` | `std::vector<UnifiedItem> search(...)` 批量返回 |
| `page.evaluate(extractPosts)` | CDP `Runtime.evaluate` |
| `page.evaluate(structure_js)` → 给 LLM | CDP `Runtime.evaluate` → 字符串结果 |
| `AsyncIterator` yield | `std::vector` 攒满后返回 |
| CSS selector → DOM | CDP `DOM.querySelector` / `DOM.querySelectorAll` |

### M9. agent（~40 行 Python → ~80 行 C++）

最轻量模块，主要是 ReAct 循环和 Tool 调度逻辑。无外部依赖。

| Python | C++17 |
|--------|-------|
| `class Agent(Module)` | `class Agent : public Module` |
| `async def run()` | `TaskResult run(...)` 同步或在 uv 循环中回调 |
| `tools: list[Tool]` | `std::vector<std::unique_ptr<Tool>>` |
| function-calling → LLM | 同 LLM 格式 JSON 传递 |

---

## 6. libuv 事件循环模型

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
│  │              uv_queue_work (线程池, 默认4线程)      │   │
│  │  ├─ LLM HTTP 调用 (cpp-httplib)                   │   │
│  │  ├─ JSONL 文件写入                                │   │
│  │  └─ 其他阻塞操作                                   │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

**执行规则**（同 Python 线程模型）：

1. **单一事件循环** — 一切 I/O 注册到 `uv_default_loop()`
2. **阻塞工作** — 只能通过 `uv_queue_work` 提交，禁止直接 `std::thread`
3. **定时任务** — 只能用 `uv_timer_start`，禁止 `std::this_thread::sleep_for`
4. **跨线程回调** — 用 `uv_async_send` 从工作线程通知主循环

---

## 7. 迁移分阶段计划

| 阶段 | 模块 | 预计 C++ 行数 | 难度 | 依赖就绪 |
|:---:|------|:---:|:---:|------|
| 1 | config → logger → storage | ~380 行 | ★ | 无 |
| 2 | common + scheduler | ~300 行 | ★★★ | libuv |
| 3 | llm | ~120 行 | ★★ | libuv + cpp-httplib |
| 4 | browser (CDP) | ~600 行 | ★★★★★ | libuv + IXWebSocket |
| 5 | adapters | ~700 行 | ★★★★ | browser |
| 6 | agent | ~80 行 | ★★ | adapters |
| — | main.cpp (Composition Root) | ~100 行 | ★★ | 全部 |
| **合计** | | **~2280 行** | | |

---

## 8. 测试策略

| 类型 | Python (pytest) | C++ (GTest) |
|------|----------------|-------------|
| UT | `@pytest.mark.parametrize` | `TEST_P` / `INSTANTIATE_TEST_SUITE_P` |
| TA | `@pytest.mark.asyncio` + `tmp_path` | 同上 + `std::filesystem::temp_directory_path()` |
| Mock | `unittest.mock` | 手写 fake 实现（不引入 GMock，降低依赖） |

**测试文件映射**：
```
tests/wheel/config/test_config_loader.py → tests/wheel/config/test_config_loader.cpp
tests/wheel/storage/test_storage.py      → tests/wheel/storage/test_storage.cpp
...（一一对应）
```

**示例**：
```cpp
TEST(ConfigLoaderTest, LoadsDefaultsForMissingFile) {
    JsonConfigLoader loader("nonexistent.json");
    Config cfg = loader.load();
    EXPECT_EQ(cfg.server.port, 7890);
    EXPECT_EQ(cfg.llm.model, "deepseek-chat");
}

TEST_F(JsonlStorageTest, SaveAndLoadRoundTrip) {
    storage_->save_items("2026-04-26", items_);
    auto loaded = storage_->load_items("2026-04-26");
    EXPECT_EQ(loaded.size(), items_.size());
    EXPECT_EQ(loaded[0].title, items_[0].title);
}
```

---

## 9. 编码约定

| 规则 | 说明 |
|------|------|
| **头文件保护** | `#pragma once`（简洁，所有主流编译器支持） |
| **命名空间** | `lynne::wheel::xxx`, `lynne::core::xxx` |
| **所有权** | `std::unique_ptr` 优先，`std::shared_ptr` 仅在共享场景 |
| **返回值** | 值语义优先，大对象用 `std::vector` 移动 |
| **错误处理** | `std::runtime_error` 抛异常，不用 `abort()` / `exit()` |
| **日志** | 统一 `spdlog`，不混用 `printf` / `std::cout` |
| **纯虚接口** | 基类虚析构 `virtual ~Foo() = default;` |
| **禁止** | 全局 mutable 变量、裸 `new`/`delete`、`longjmp`、`std::thread` 直接创建 |

---

## 10. 待定事项

| 事项 | 状态 | 说明 |
|------|:---:|------|
| WebSocket 库最终确认 | ✅ IXWebSocket | 已定 |
| 浏览器 stealth 注入方案 | 待试验 | CDP `Page.addScriptToEvaluateOnNewDocument` 理论上可行，需实际验证 |
| 跨平台构建 Windows 支持 | 后期 | 初版只 Linux |
| FastAPI → C++ HTTP Server | 未定 | 待 Web UI 阶段再定，候选：Simple-Web-Server (header-only) / Crow |
| 错误码 vs 异常 | 未定 | 暂定用异常，如果 perf 敏感再讨论 `std::expected`（C++23）或返回码 |

---

*文档版本：v0.1 | 最后更新：2026-04-26*
