---
name: lynne-browser-lessons
description: Lynne browser/CDP lessons — CDP vs DOM API, anti-bot stealth, selector escaping, cross-site JS robustness, dump strategy
---

# Lynne 浏览器 / CDP 踩坑经验

> 浏览器模块开发过程中总结的硬教训。每个条目都对应一次真正的故障。

## 1. CDP 低层命令不可靠 → 用 DOM API

CDP `Input.dispatchMouseEvent` / `Input.insertText` / `Input.dispatchKeyEvent` 发送的是**物理层协议事件**，不是浏览器 DOM 事件。

| CDP 命令 | 问题 | 替代 |
|---------|------|------|
| `Input.dispatchMouseEvent` (pressed+released) | 不触发 `<input type="submit">` 的表单提交 | `e.dispatchEvent(new MouseEvent('click', ...))` + `form.submit()` / `window.location.href = ...` |
| `Input.insertText` | 不触发 `input`/`change` 事件；元素必须已聚焦 | `e.focus()` + `e.value = text` + `e.dispatchEvent(new Event('input', {bubbles: true}))` |
| `Input.dispatchKeyEvent` | Enter 键可能不被网页的 event listener 识别 | `keyDown` + `windowsVirtualKeyCode` + `text: "\r"` 三合一 |

**规则**：浏览器 DOM 交互走 JS DOM API。CDP Input 命令仅用于必须精确模拟物理输入的场景（如拖拽、画布操作）。

具体实现位置：`src/wheel/browser/imp/cdp_browser_manager.cpp` — `CdpBrowserContext::click/type/hover/press_key`

## 2. `Page.navigate` 回调 ≠ 页面加载完

```cpp
// ❌ 陷阱
ctx->navigate(url, [&]() {
    ctx->evaluate("document.title", ...);  // 页面还没渲染！
}, ...);

// ✅ 正确
ctx->navigate(url, [&, ctx]() {
    ctx->wait_for_selector("body", 10000, [&]() {
        ctx->evaluate("document.title", ...);
    }, ...);
}, ...);
```

`Page.navigate` 的 CDP 响应在 Chrome **收到导航命令**时就触发。此时 HTML 可能还没下载。必须后续 `wait_for_selector` 或 sleep 等待页面渲染。

## 3. 反爬是多层体系，不是一道墙

XHS（小红书）的检测层次及对应的破解方法：

| 层 | 检测手段 | 如何暴露 | 破解方法 |
|---|---------|---------|---------|
| Blink | `navigator.webdriver` boolean | headless Chrome 默认 true | `--disable-blink-features=AutomationControlled`（比 JS override 更底层） |
| User-Agent | `navigator.userAgent` 含 `HeadlessChrome` | 默认 UA | `--user-agent=Mozilla/5.0 ... Chrome/147.0.0.0` |
| Plugins 指纹 | `navigator.plugins` 项数和类型 | headless 下空数组或假数组 | `Object.setPrototypeOf` 构建真对象 + 原生原型 |
| 登录墙 | Cookie/Session | 未登录用户看不到内容 | **这是业务逻辑，不是反爬** — 需要模拟登录或注入 cookie |

**假的 plugin 数组示范**（反例）：

```js
// ❌ [1,2,3,4,5] — 一测就穿
// plugins[0] instanceof Plugin → false
Object.defineProperty(navigator, 'plugins', {
    get: () => [1,2,3,4,5]
});
```

**真的 plugin 数组**（正确做法）：

```js
// ✅ Plugin 对象 + 原生原型
var proto = Object.getPrototypeOf(navigator.plugins);
Object.defineProperty(navigator, 'plugins', {
    get: () => Object.setPrototypeOf([
        {name:'Chrome PDF Plugin', filename:'internal-pdf-viewer', ...},
        ...
    ], proto)
});
```

**注意**：`PluginArray`、`Plugin`、`MimeType`、`MimeTypeArray` 不是全局构造函数，不能直接 `new PluginArray()`。必须用 `Object.getPrototypeOf(navigator.plugins)` 拿到原生原型。

## 4. CSS Selector 嵌入 JS 字符串必须 Escape

所有交互方法（click/type/hover/exists/wait_for_selector）都构造 `document.querySelector('SELECTOR')` 字符串。selector 含单引号时截断 JS 字符串：

```
querySelector('input[type='text']')   // ← ' 在 'text' 处提前闭合
```

**修复**：加 `js_selector()` 函数，在构造 JS 前 escape：

```cpp
static std::string js_selector(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '\\') r += "\\\\";
        else if (c == '\'') r += "\\'";
        else r += c;
    }
    return r;
}

// 使用：
std::string js = "document.querySelector('" + js_selector(sel) + "')";
```

**受影响方法**：`click()`、`type()`、`hover()`、`exists()`、`wait_for_selector()` — 共 5 个。

## 5. 跨站 JS 不能假设 DOM 属性类型

`querySelectorAll('*')` 捞到的元素可能是任何 HTML/SVG/自定义元素。不能假设 `.className` 是 `String`：

```js
// ❌ 在 SVG 元素上崩溃
e.className.slice(0, 60)
// SVGAnimatedString 没有 .slice() → TypeError → 整段 JS 炸

// ✅ 防御式写法
String(e.className || '').trim().slice(0, 60)
```

**规则**：所有 DOM 属性访问必须走：
1. `typeof` guard
2. `String()` 包裹
3. `try/catch` 在 map/filter 回调里

## 6. `dump_page_structure` 的 trade-off

第一版做了语义分类（input/button/link/form 分桶输出），结果是**过滤过头**——丢掉了容器 div、section 等结构信息。

现在的策略：
- 用 `querySelectorAll('*')` 平面扫描，拿所有有 id/class/role/label 的元素
- 输出扁平 list（tag + id + class + text + href + type），不递归
- 用短 key 减小 JSON 体积（`i` 替代 `id`，`c` 替代 `class`）
- **不替 LLM 做语义判断**——分类留给 LLM

```json
{
  "u": "https://example.com",   // url
  "t": "页面标题",               // title
  "n": [
    {"t": "input", "i": "kw", "c": "s_ipt", "ty": "text"},
    {"t": "button", "i": "su", "x": "百度一下"},
    ...
  ]
}
```

## 7. C++ 原始字符串里带括号的 JS 是雷

```cpp
// ❌ R"JS(...)JS" 里 IIFE 的 })() 结尾跟分隔符 )JS" 冲突
static const char* js = R"JS(
(function(){...})()JS");
//                             ^^^^ 编译器把 )JS" 当成字符串闭合，剩 false `

// ✅ 用长分隔符 R"JSON_END(...)JSON_END" 避免冲突
static const char* js = R"JSON_END(
(function(){...})()JSON_END";
```

**规则**：
- JS 内容含 `)'` `'(` 时不要用短分隔符
- 选一个不可能出现在 JS 正文中的分隔符（如 `JSON_END`）
- 或用平面表达式替代 IIFE
