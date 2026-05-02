# BrowserManager 详细设计

> 所属层：`wheel/` 基础设施 — 浏览器引擎
> 依赖：`wheel/logger`（日志），IXWebSocket + libuv
> 源码：`src/wheel/browser/`
>
> **本文分两部分**：第 1-3 节定义抽象规范（与实现无关），第 4-6 节描述 CDP 实现。
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

```cpp
class BrowserManager : public Module {
public:
    // 生命周期
    void start() override = 0;
    void stop() override = 0;
    bool health_check() override = 0;

    // 平台上下文（每个平台隔离的浏览会话）
    virtual void get_context(const std::string& platform,
        std::function<void(BrowserContext*)> on_ok,
        std::function<void(const std::string&)> on_error) = 0;

    // 会话持久化
    virtual void save_session(const std::string& platform,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) = 0;

    // 登录流程
    virtual void login_flow(const std::string& platform,
        const std::string& url) = 0;
    virtual void set_login_complete(const std::string& platform) = 0;
};
```

| 方法 | 语义 |
|------|------|
| `start()` | 启动浏览器子进程 + 连接 CDP WebSocket 端口 |
| `stop()` | 关闭所有上下文 → kill 子进程 |
| `get_context(platform)` | 获取指定平台的浏览上下文（缓存） |
| `save_session(platform)` | 持久化当前认证状态（CDP Storage.getCookies） |
| `login_flow(platform, url)` | 打开登录页（CDP Page.navigate），进入等待状态 |
| `set_login_complete(platform)` | 确认登录完成，保存会话 |

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
browser->start();                         // Step 1: 启动浏览器 + CDP 连接

browser->get_context("twitter",           // Step 2: 拿隔离上下文（缓存）
    [](BrowserContext* ctx) {
        // Step 3: CDP evaluate / navigate
        ctx->navigate("https://x.com/search?...", [](){
            printf("navigated\n");
        });
    },
    [](const std::string& err) {
        printf("error: %s\n", err.c_str());
    });

browser->save_session("twitter",          // Step 4: 持久化会话
    [](){ printf("session saved\n"); },
    nullptr);

// stop() 由 Agent / main.cpp 决定何时调用
```

### 3.3 CDP 命令示例

BrowserManager 下层提供 CDP 原始命令接口：

```
Page.navigate(url)              → 导航到目标页面
Runtime.evaluate(js)            → 执行 JS，返回结果
Page.addScriptToEvaluateOnNewDocument(js)  → 反检测脚本注入
Storage.getCookies              → 导出 cookies
```

### 3.4 注意事项

| 要点 | 说明 |
|------|------|
| `start()` 幂等 | 已启动时直接 return |
| `get_context` 有缓存 | 同一 platform 第二次直接返回缓存 |
| `get_context` 前必须 `start()` | 否则 `RuntimeError("not started")` |
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
    int cdp_port = 9222;            // Chrome remote debugging port
    std::string chrome_path = "";   // 空则自动查找系统 Chrome
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
├── contexts_: map<string, BrowserContext>  // platform → 上下文缓存
├── login_pending_: set<string>         // 登录中的平台
└── scheduler_: Scheduler*              // 用于 run_blocking / post
```

## 6. 关键流程

### 6.1 启动

```
start()
  ├─ 1. 子进程启动 Chrome:
  │    chrome --headless --disable-gpu
  │           --remote-debugging-port=config_.cdp_port
  │           --no-sandbox --disable-dev-shm-usage
  │
  ├─ 2. HTTP GET http://localhost:{port}/json/version
  │    获取 webSocketDebuggerUrl
  │
  └─ 3. IXWebSocket 连接 CDP WebSocket:
       ws_.connect(webSocketDebuggerUrl)
       ws_.start()  // 集成 libuv 事件循环
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
    std::function<void(const std::string&)> on_error
) {
    int id = cmd_id_++;
    nlohmann::json msg = {
        {"id", id},
        {"method", method},
        {"params", params}
    };
    pending_[id] = {std::move(on_ok), std::move(on_error)};
    ws_.send(msg.dump(), nullptr);
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

### 6.5 登录流程

```
login_flow(platform, url)
  ├─ get_context(platform)             → 获取/创建平台上下文
  ├─ CDP Page.navigate(url)            → 打开登录页
  ├─ CDP Page.addScriptToEvaluateOnNewDocument(stealth.js) → 反检测
  └─ login_pending_.insert(platform)

                    ↓
         用户手动在浏览器中登录
                    ↓

set_login_complete(platform)
  ├─ save_session(platform)            → 保存 cookies
  ├─ login_pending_.erase(platform)
  └─ CDP Page.close                    → 关闭登录页面
```

登录是人工操作，BrowserManager 只负责 "打开页面" 和 "确认完成后的保存"。

### 6.6 反检测（Stealth）

CDP 启动时注入 `Page.addScriptToEvaluateOnNewDocument`，注入反检测 JS（隐藏 webdriver 特征、覆盖 navigator 属性等）。

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
| `set_login_complete` 时无 login flow | `RuntimeError("no login flow active")` |
| 反检测注入失败 | 跳过，warning 日志，继续执行 |

---

## 10. 测试

TA 使用真实 CDP headless Chrome。

| 类型 | 文件 | 覆盖 |
|------|------|------|
| UT | `test_browser_models_ut.cpp` | 默认值、自定义、相等性 |
| UT | `test_browser_factory_ut.cpp` | 创建、配置传递、接口返回 |
| TA | `test_browser_impl_ta.cpp` | 生命周期、context 管理、session 读写、login flow、反检测 |

---

*文档版本：v1.2-cpp | 最后更新：2026-05-02*
