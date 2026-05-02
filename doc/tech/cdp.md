# Chrome DevTools Protocol（CDP）

> CDP 是 Chrome/Chromium 内置的调试与控制协议。Lynne 通过 CDP 与 Chrome 子进程通信，完成页面加载、JS 执行、Cookie 管理等浏览器操作。

---

## 1. 协议基础

| 概念 | 说明 |
|------|------|
| **底层传输** | WebSocket（Chrome 启动时暴露 `--remote-debugging-port`，进程内建 HTTP + WS 服务器） |
| **消息格式** | JSON-RPC 2.0 — 请求: `{id, method, params}`，响应: `{id, result}`，事件: `{method, params}`（无 id） |
| **Domain** | CDP 按功能域划分方法，如 `Page.navigate`、`Runtime.evaluate`、`Storage.getCookies` |
| **Target** | 一个浏览器标签页/页面。每个 Target 有唯一 `targetId` |
| **Session** | 附加到 Target 后的会话标识。**有 sessionId 的消息只在该 Target 上下文中执行** |
| **Event** | 异步推送（`Runtime.consoleAPICalled`、`Page.frameStoppedLoading` 等） |

---

## 2. 消息格式

### 2.1 请求

```json
{"id": 1, "method": "Runtime.evaluate", "params": {"expression": "document.title"}}
```

- `id`: 自增整数，响应会回带相同 id
- `method`: Domain.method 格式
- `params`: 方法参数

### 2.2 响应

```json
{"id": 1, "result": {"result": {"type": "string", "value": "首页"}}}
```

- `id`: 对应请求的 id
- `result`: 执行结果

### 2.3 事件（无 id 推送）

```json
{"method": "Runtime.consoleAPICalled", "params": {"type": "log", "args": [...]}}
```

- 无 `id` 字段，靠 `method` 区分

---

## 3. 关键 Domain

| Domain | 方法 | 用途 |
|--------|------|------|
| `Page` | `navigate` | 导航到 URL |
| `Page` | `captureScreenshot` | 页面截图 |
| `Page` | `addScriptToEvaluateOnNewDocument` | 注入脚本（每次新 document 执行） |
| `Runtime` | `evaluate` | 执行 JS 表达式，返回结果 |
| `Runtime` | `callFunctionOn` | 在指定对象上调用函数 |
| `Target` | `createTarget` | 创建新标签页（返回 targetId） |
| `Target` | `attachToTarget` | 附加到 target，返回 sessionId |
| `Target` | `closeTarget` | 关闭标签页 |
| `Storage` | `getCookies` | 导出当前页面的 cookies |
| `Storage` | `setCookies` | 恢复 cookies |
| `Input` | `dispatchMouseEvent` | 模拟鼠标点击/滚动 |
| `Input` | `dispatchKeyEvent` | 模拟键盘输入 |

---

## 4. 会话（Session）机制

CDP 支持两种执行上下文：

### 4.1 无 session（根目标）

连接到 `webSocketDebuggerUrl` 后，不传 sessionId 的命令操作浏览器根目标（Browser 域）。

### 4.2 有 session（页面目标）

```json
{"id": 10, "sessionId": "A", "method": "Page.navigate", "params": {"url": "https://x.com"}}
```

每个新标签页（Target.createTarget）附加后获得独立 sessionId。**平台隔离通过不同 sessionId 实现**：
- `session{A}` → twitter 的 cookies、DOM、localStorage 完全独立
- `session{B}` → rednote 的 cookies、DOM、localStorage 完全独立

### 4.3 Target 与 Session 关系

```
CDP WebSocket (主连接)
  │
  ├─ Target.createTarget → targetId: "T1"
  │   └─ Target.attachToTarget → sessionId: "A"
  │       └─ 所有命令带 sessionId: "A"
  │
  └─ Target.createTarget → targetId: "T2"
      └─ Target.attachToTarget → sessionId: "B"
          └─ 所有命令带 sessionId: "B"
```

---

## 5. 与 Playwright 的对比

| 维度 | Playwright | CDP 直连 |
|------|-----------|----------|
| 部署依赖 | Python + Node.js + 浏览器驱动 | 仅 Chrome 二进制 |
| 协议 | 封装 CDP，额外抽象层 | 裸 CDP JSON-RPC |
| 自动等待 | 内置 `waitForSelector` 等 | 需自己处理 `Page.frameStoppedLoading` 事件 |
| 反检测 | 内置 `playwright-stealth` | 需手动注入 `addScriptToEvaluateOnNewDocument` |
| 语言绑定 | JavaScript/Python | C++（`nlohmann/json` + `ixwebsocket`） |

Lynne 选择 CDP 直连的原因是：零额外运行时依赖，WebSocket 和 JSON 解析由已有模块（`ws_client`、`nlohmann`）提供，没有理由引入 Playwright 的整个技术栈。

---

## 6. 在 Lynne 中的架构位置

```
Chrome 子进程 (headless)
  └── CDP WebSocket (:9222)
       └── wheel/browser/imp/cdp_browser_manager.cpp
            ├── send_command(method, params, on_ok, on_error)
            │     └── id→callback 映射 (pending_ map)
            │
            ├── session{A} ── BrowserContext("twitter")
            │   ├── Page.navigate / Runtime.evaluate
            │   └── Storage.getCookies → save_session
            │
            └── session{B} ── BrowserContext("rednote")
                ├── Page.navigate / Runtime.evaluate
                └── Storage.getCookies → save_session
```

CDP 协议细节封装在 `imp/` 内部，抽象层 `BrowserManager` + `BrowserContext` 对上层屏蔽协议差异。

---

*参考: [Chrome DevTools Protocol 官方文档](https://chromedevtools.github.io/devtools-protocol/)*
