# BrowserManager 详细设计

> 所属层：`wheel/` 基础设施 — 浏览器引擎
> 依赖：`wheel/logger`（日志），IXWebSocket + libuv
> 源码：`src/wheel/browser/`
>
> **本文分两部分**：第 1-3 节定义抽象规范（与实现无关），第 4-10 节描述 CDP 实现。
>
> CDP 协议基础见 [`doc/tech/cdp.md`](../../../tech/cdp.md)。

---

## 1. 模块定位

BrowserManager 是浏览器引擎的抽象。它定义了 "管理浏览器生命周期、为各平台提供隔离的浏览会话" 的**能力契约**，不绑定任何特定浏览器驱动。

上层模块（PlatformAdapter、Agent）依赖**接口**，不关心底层是 CDP / Selenium，换实现只需换工厂。

### 1.1 边界定义

```
                           ┌─────────────────────────┐
                           │     BrowserManager       │
                           │     (抽象接口)             │
                           │                          │
 BrowserConfig ──────────► │  启动 / 停止浏览器引擎     │
                           │                          │
                           │  获取/创建浏览上下文       │ ──► 浏览上下文对象
 platform: str ──────────► │  每个平台一个，互不干扰     │
                           │                          │
                           │  会话持久化               │ ──► 会话文件（落盘）
                           │  （从文件恢复 / 写回文件）  │
                           │                          │
                           │  CDP 命令发送              │ ──► evaluate / navigate
                           │  （回调模式）               │
                           └──────────────────────────┘
```

---

## 2. 接口定义

### 2.1 BrowserManager

```cpp
class BrowserManager : public Module {
public:
    // 生命周期
    void start() override = 0;
    void stop() override = 0;
    bool health_check() override = 0;

    // 平台上下文（每个平台隔离的浏览会话，缓存）
    virtual void get_context(const std::string& platform,
        std::function<void(BrowserContext*)> on_ok,
        std::function<void(const std::string&)> on_error) = 0;

    // 会话持久化
    virtual void save_session(const std::string& platform,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) = 0;
    virtual void restore_session(const std::string& platform,
        std::function<void(bool)> on_done,
        std::function<void(const std::string&)> on_error) = 0;

    // 事件循环推进（等价于 uv_run 的 step/run）
    virtual void step() = 0;
    virtual void run() = 0;
};
```

| 方法 | 语义 |
|------|------|
| `start()` | 启动 Chrome 子进程 + 连接 CDP WebSocket |
| `stop()` | 关闭所有 context → 断开 WS → kill 子进程 |
| `health_check()` | 检查 Chrome 进程 + CDP 连接是否正常 |
| `get_context(platform)` | 获取/创建平台隔离的浏览上下文（缓存） |
| `save_session(platform)` | CDP `Storage.getCookies` → 写入 `{platform}_state.json` |
| `restore_session(platform)` | 读文件 → CDP `Storage.setCookies`，无文件时 `on_done(false)` |
| `step()` | `uv_run(UV_RUN_NOWAIT)`，推进一次事件循环 |
| `run()` | `uv_run(UV_RUN_DEFAULT)`，阻塞直到事件循环结束 |

### 2.2 BrowserContext

```cpp
class BrowserContext {
public:
    virtual ~BrowserContext() = default;

    // 导航到 URL。调用后 on_loaded 从 CDP 响应回调触发。
    // CDP 在同一 session 内串行化命令，后续 evaluate 会在导航完成后执行。
    virtual void navigate(const std::string& url,
        std::function<void()> on_loaded,
        std::function<void(const std::string&)> on_error) = 0;

    // 执行 JS，返回结果
    virtual void evaluate(const std::string& js,
        std::function<void(nlohmann::json)> on_result,
        std::function<void(const std::string&)> on_error) = 0;

    // 注入反检测脚本（每次新 document 自动执行）
    virtual void add_init_script(const std::string& js,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) = 0;

    // 获取当前页面 URL
    virtual void current_url(
        std::function<void(const std::string&)> on_url,
        std::function<void(const std::string&)> on_error) = 0;

    // 截图（调试用）
    virtual void screenshot(const std::string& filepath,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) = 0;

    // 原始 CDP 命令（Input.* / Network.* 等逃生口）
    virtual void send_command(const std::string& method,
        const nlohmann::json& params,
        std::function<void(nlohmann::json)> on_ok,
        std::function<void(const std::string&)> on_error) = 0;

    // 关闭此上下文（关标签页、清缓存、指针失效）
    virtual void close(
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) = 0;
};
```

| 方法 | 语义 |
|------|------|
| `navigate(url)` | CDP `Page.navigate`，`on_loaded` 从 CDP 响应回调直接触发（不等待页面事件）。CDP 在同一 session 内串行化命令，后续 `evaluate` 自动排队等导航完成 |
| `evaluate(js)` | CDP `Runtime.evaluate`，返回 `result.value` |
| `add_init_script(js)` | CDP `Page.addScriptToEvaluateOnNewDocument` |
| `current_url()` | `Runtime.evaluate("document.URL")` |
| `screenshot(path)` | CDP `Page.captureScreenshot` → base64 解码 → 写文件 |
| `send_command(m, p)` | 任意 CDP `{method, params}`，`on_ok(result)` |
| `close()` | CDP `Target.closeTarget` → 从 BrowserManager 缓存移除 → 指针失效 |

---

## 3. 使用指南

### 3.1 拿到实例

```cpp
#include "wheel/browser/browser_factory.h"
#include "wheel/browser/browser_models.h"

BrowserConfig cfg;
cfg.headless = true;
cfg.sessions_dir = "data/sessions";

BrowserFactory factory;
BrowserManager* browser = factory.create(cfg);
```

### 3.2 标准采集流程

```cpp
browser->start();                         // Step 1: 启动 Chrome + CDP

browser->get_context("twitter",           // Step 2: 拿隔离上下文（缓存）
    [](BrowserContext* ctx) {
        // Step 3: 导航 + JS 提取
        ctx->navigate("https://x.com/search?...", [ctx](){
            ctx->evaluate("document.title", [](nlohmann::json r){
                printf("title: %s\n", r.get<std::string>().c_str());
            }, nullptr);
        }, nullptr);
    },
    [](const std::string& err) {
        printf("error: %s\n", err.c_str());
    });

// stop() 由 Agent / main.cpp 决定何时调用
```

### 3.3 登录编排（上层实现，BrowserManager 不感知）

登录流程由 API Layer 或 `main.cpp` 用公开原语组装：

```cpp
browser->get_context("rednote", [browser](BrowserContext* ctx) {
    ctx->navigate("https://www.xiaohongshu.com/login", [ctx, browser]() {
        // 用户手动在浏览器中登录
        // 登录完成后触发:
        browser->save_session("rednote", [](){
            printf("session saved\n");
        }, nullptr);
    }, nullptr);
}, on_error);
```

不需要 `login_flow` / `set_login_complete` 接口——登录只是一个 navigate + 手动操作 + save_session 的编排。

### 3.4 反检测（Stealth）

`get_context()` 创建新 context 时自动注入反检测脚本，Adapter 不感知：

```cpp
// get_context("rednote") 内部自动执行:
//   Target.createTarget → Target.attachToSessionId
//   → Page.addScriptToEvaluateOnNewDocument(stealth.js)
//   → cache context
```

隐藏的 CDP 特征包括：`navigator.webdriver`、`navigator.plugins`、`window.chrome` 等。

### 3.5 注意事项

| 要点 | 说明 |
|------|------|
| `start()` 幂等 | 已启动时直接 return |
| `get_context` 有缓存 | 同一 platform 第二次直接返回缓存 |
| `get_context` 前必须 `start()` | 否则 `RuntimeError("not started")` |
| `context->close()` 后指针失效 | 调用方保证不再使用该指针 |
| 会话文件路径固定 | `{sessions_dir}/{platform}_state.json` |
| 会话损坏不阻断 | 降级为全新上下文，记录 warning |
| `stop()` 后一切失效 | 下次需重新 `start()` |

---

## 4. 配置模型

```cpp
struct BrowserConfig {
    bool headless = true;
    int slow_mo_ms = 500;           // 操作间延迟(ms)，模拟人类
    int viewport_width = 1920;
    int viewport_height = 1080;
    std::string locale = "zh-CN";
    int timeout_ms = 30000;
    std::string sessions_dir = "data/sessions";
    int cdp_port = 0;               // 0=自动随机端口，>0=固定端口
    std::string chrome_path;        // 空则自动查找系统 Chrome
};
```

`config.json` 中 `browser` 节点直接映射。

---

# 实现部分 — CdpBrowserManager

> 以下描述当前唯一实现。替换为其他浏览器驱动时，只需实现 `BrowserManager` 接口。

---

## 5. 内部状态

```
CdpBrowserManager
├── config_: BrowserConfig
├── chrome_pid_: pid_t                  // Chrome 子进程 PID
├── ws_: unique_ptr<ix::WebSocket>      // CDP WebSocket 连接
├── cmd_id_: int                        // JSON-RPC 消息 ID 自增
├── pending_: map<int, PendingRequest>  // id → (on_ok, on_error) 回调
├── contexts_: map<string, unique_ptr<CdpBrowserContext>>  // platform → 上下文
└── scheduler_: Scheduler*              // 用于 run_blocking / post
```

## 6. 关键流程

### 6.1 启动

```
start()
  ├─ 1. scheduler_->start()        ← 初始化 libuv async handle
  │
  ├─ 2. scheduler_->run_blocking(work, done)  ← 在 worker 线程中执行:
  │      work:
  │        └─ fork()+execvp() 启动 Chrome:
  │             chrome --headless --disable-gpu --no-sandbox
  │                    --disable-dev-shm-usage
  │                    --remote-allow-origins=*         ← Chrome 147 必需，允许 WS 连接
  │                    --remote-debugging-address=127.0.0.1
  │                    --remote-debugging-port=0        ← 随机端口，避免冲突
  │
  │        └─ 从 stdout 读取 "DevTools listening on ws://..."
  │             → 解析 ws://127.0.0.1:{port}/devtools/browser/{uuid}
  │
  │        └─ HTTP GET http://127.0.0.1:{port}/json/version
  │             轮询直到返回 200（等待 HTTP 服务就绪）
  │
  │      done:
  │        └─ ws_.setUrl(cdp_ws_url)
  │        └─ ws_.disableAutomaticReconnection()
  │        └─ ws_.setOnMessageCallback(on_ws_message)
  │        └─ ws_.start()  ← 异步连接 CDP WebSocket
  │        └─ started_ = true
  │
  └─ 3. IXWebSocket 异步连接完成后触发 Open 回调:
         → ws_ready_ = true
         → flush pending_commands_
         → send_command("Page.enable")
```

### 6.2 关闭

```
stop()
  ├─ for each context: 清理 context 缓存
  ├─ ws_.stop() / ws_.disconnect()    ← 关闭 CDP 连接
  └─ kill(chrome_pid_, SIGTERM)       ← 关闭子进程
       waitpid(chrome_pid_, ...)      ← 等待进程退出
```

### 6.3 CDP 命令发送

```cpp
void CdpBrowserManager::send_command(
    const std::string& method,
    const nlohmann::json& params,
    std::function<void(nlohmann::json)> on_ok,
    std::function<void(const std::string&)> on_error,
    const std::string& session_id = ""
) {
    int id = cmd_id_++;
    nlohmann::json msg = {
        {"id", id},
        {"method", method},
        {"params", params}
    };
    if (!session_id.empty()) {
        msg["sessionId"] = session_id;
    }

    pending_[id] = {std::move(on_ok), std::move(on_error)};

    if (!ws_ready_) {
        // WS 未就绪→入队，Open 后批量 flush
        pending_commands_.push_back(std::move(msg));
        return;
    }
    ws_.sendText(msg.dump());
}
```

### 6.4 会话持久化

```
save_session(platform)
  ├─ 发送 CDP Storage.getCookies
  ├─ 构建会话 JSON: {cookies: [...]}
  ├─ sessions_dir 不存在则 fs::create_directories
  └─ 写入 {platform}_state.json
```

### 6.5 打开上下文

```
get_context(platform)
  ├─ if cached: 返回缓存 context
  ├─ CDP Target.createTarget {url: "about:blank"}
  ├─ CDP Target.attachToTarget → sessionId
  ├─ CDP Page.addScriptToEvaluateOnNewDocument(stealth.js)  → 反检测
  ├─ 缓存: contexts_[platform] = context(sessionId)
  └─ on_ok(context)
```

反检测脚本在每次新 document 时自动执行，覆盖 `navigator.webdriver`、`navigator.plugins`、`window.chrome` 等 CDP 特征。

### 6.6 上下文关闭

```
context->close()
  ├─ CDP Target.closeTarget(targetId)
  ├─ 从 BrowserManager 缓存移除
  └─ context 指针失效
```

---

## 7. 会话文件格式

```json
{
  "cookies": [
    {"name": "auth_token", "value": "xxx", "domain": ".x.com", ...}
  ]
}
```

文件路径：`{BrowserConfig.sessions_dir}/{platform}_state.json`

---

## 8. 工厂

```cpp
class BrowserFactory {
public:
    BrowserManager* create(const BrowserConfig& config = {});
};
```

- 无构造注入 — BrowserManager 不依赖其他 core 模块
- 返回类型是 `BrowserManager*`（接口），调用方负责 delete
- 支持扩展：将来可加 `if config.type == "selenium"` 分支

---

## 9. 错误处理

| 场景 | 行为 |
|------|------|
| Chrome 启动失败（port 被占等） | `RuntimeError("chrome launch failed")` |
| CDP WebSocket 连接失败 | `RuntimeError("cdp connection failed")` |
| `get_context` 时引擎未启动 | `RuntimeError("not started")` |
| 会话 JSON 损坏 | 丢弃，全新上下文启动，warning 日志 |
| `save_session` 时上下文不存在 | `RuntimeError("no context")` |
| `restore_session` 时文件不存在 | `on_done(false)`，非错误 |
| `context->close()` 后再次调用 | `RuntimeError("already closed")` |
| CDP 命令超时（> timeout_ms） | `on_error("timeout")`，清理 pending |
| Chrome 子进程意外退出 | `RuntimeError("chrome process crashed")` |

---

## 10. 测试

TA 使用真实 CDP headless Chrome（若 Chrome 不存在则 skip）。

| 类型 | 文件 | 覆盖 |
|------|------|------|
| UT | `test_browser_models_ut.cpp` | BrowserConfig 默认值、序列化、自定义 |
| TA | `test_browser_ta.cpp` | 生命周期、get_context、navigate、evaluate、screenshot、save/restore_session、close |

---

## 11. 手动验证

### 11.1 前置条件

```bash
# 确保 Chrome 已安装
google-chrome-stable --version
# 或 chromium --version
```

### 11.2 运行自动化测试

```bash
# UT（不依赖 Chrome）
dist/bin/test_browser_models_ut

# TA（需要 Chrome，无 Chrome 时自动 skip）
dist/bin/test_browser_ta

# 全量回归
cmake --build build --target test
```

### 11.3 手动端到端验证

编译测试二进制后，直接执行 TA 测试验证完整流程。以下是 TA 覆盖的场景：

| 验证场景 | 预期结果 |
|---------|---------|
| 启动 Chrome → CDP WS 连接 → health_check=true | `[PASS] start ok` |
| 多次 start 幂等 | `[PASS] start twice: health true` |
| start → stop → restart | `[PASS] first start` + `[PASS] restart ok` |
| get_context（创建 Target + sessionId） | `[PASS] title not empty` + `[PASS] title is TA` |
| get_context 缓存（同 platform 同指针） | `[PASS] cached: same pointer` |
| 不同 platform 隔离 | `[PASS] different platforms → different ctx` |
| add_init_script → close → reopen（不同指针） | `[PASS] close+reopen: different ptr` |
| send_command 原始 CDP（Runtime.evaluate） | `[PASS] send_command 1+1=2` |
| save_session → session JSON 文件落盘 | `[PASS] save: session file non-empty` |
| **真实 URL: baidu.com** → navigate + evaluate 标题 | `[PASS] baidu: title contains 百度` |

### 11.4 排查日志

启动日志输出到 stderr，以 `[browser]` 为前缀：

```text
[browser] launching Chrome...                              ← fork+exec 开始
[browser] launch_chrome done: url='ws://127.0.0.1:...     ← URL 解析成功
[browser] on_done fired: url='...                          ← Chrome 就绪
[browser] queue cmd=Target.createTarget                    ← WS 未就绪，命令入队
[browser] WS OPEN callback fired                           ← WS 连接成功
[browser] flushing N queued commands                       ← 积压命令发出
```

常见错误：

| 日志 | 问题 | 修复 |
|------|------|------|
| `chrome did not output WS URL` | Chrome 未启动或端口冲突 | `pkill -9 chrome` 后再试 |
| `WS ERROR: Failed reading HTTP status line / 403` | Chrome 147 缺少 `--remote-allow-origins=*` | 已自动添加 |
| `QUEUE cmd=... (ws not ready)` 但无 OPEN | WS 连接失败 | 检查 Chrome 是否存活 (`health_check`) |

---

*文档版本：v2.1-cpp | 最后更新：2026-05-02*
