# ADAPTER — Platform Adapter 模块设计

> 本文档定义 `core/adapters/` 模块的接口设计、架构边界、使用指南和测试策略。

---

## 1. 模块定位

| 维度 | 说明 |
|------|------|
| **所属层** | `core/`（引擎层） |
| **职责** | 平台数据采集 + LLM 驱动的自适应内容提取 |
| **依赖** | `BrowserManager`（CDP 浏览器），`LLMEngine`（通过 LLMAdapter 中间层），`common/`（UnifiedItem） |
| **被依赖** | `Agent`（通过 SearchTool 调用 Adapter 执行采集） |
| **不继承 Module** | Adapter 是短生命周期工具，每次采集内部创建 page → 操作 → 关闭 |

---

## 2. 接口定义（纯虚类）

### 2.1 BaseAdapter

```cpp
class BaseAdapter {
public:
    virtual ~BaseAdapter() {}

    virtual std::string platform_name() = 0;

    // 所有方法同步返回 vector（或通过 callback 通知）
    virtual std::vector<UnifiedItem> search(
        const std::vector<std::string>& keywords, int limit) = 0;

    virtual std::vector<UnifiedItem> get_user_posts(
        const std::string& user_id, int limit) = 0;

    virtual std::vector<UnifiedItem> get_trending(int limit) = 0;

    virtual UnifiedItem extract(const nlohmann::json& data) = 0;

    virtual bool health_check() = 0;
};
```

| 方法 | 返回 | 语义 |
|------|------|------|
| `search(kw, limit)` | `vector<UnifiedItem>` | 关键词搜索，批量返回 |
| `get_user_posts(uid, limit)` | `vector<UnifiedItem>` | 获取指定用户的最新帖子 |
| `get_trending(limit)` | `vector<UnifiedItem>` | 获取平台热门内容 |
| `extract(data)` | `UnifiedItem` | 将原始 JSON 映射为 UnifiedItem |
| `health_check()` | `bool` | 检查平台页面是否可达 |

### 2.2 LLMAdapter（v4.2 新增）

```cpp
class LLMAdapter : public BaseAdapter {
public:
    LLMAdapter(BrowserManager* browser, const AdapterConfig& config,
               LLMConfig* llm_config = nullptr)
        : browser_(browser), config_(config), llm_config_(llm_config) {}

    // lazy init LLM engine
    void ensure_llm();
    // 获取或通过 LLM 生成 extract JS 函数
    std::string get_or_generate_extract_fn(const std::string& url);
    // 滚动 + 提取循环
    std::vector<UnifiedItem> scroll_and_extract(
        const std::string& url, const std::string& extract_fn, int limit);

protected:
    BrowserManager* browser_;
    AdapterConfig config_;
    LLMConfig* llm_config_;  // 注意：不是 LLMEngine！
    LLMEngine* llm_ = nullptr;
    std::map<std::string, std::string> extract_fn_cache_;
};
```

LLMAdapter 封装了 LLM 驱动的自适应内容提取：
- **懒创建 LLMEngine**：`ensure_llm()` 从 `llm_config_` 通过 `LLMFactory` 创建
- **骨架提取**：通用 JS 遍历 DOM 取结构样本 → LLM 生成 `extractPosts()` → CDP Runtime.evaluate
- **缓存**：按 URL host + path 缓存 JS 提取函数，后续翻页零 LLM 调用
- **CDP DOM 回退**：`llm_config_` 为空时走 CDP querySelectorAll 路径

类层次：
```
BaseAdapter(纯虚)
  └─ LLMAdapter(纯虚)                   ← 中间层（LLM 提取通用逻辑）
       └─ RedNoteAdapter(LLMAdapter)    ← 平台 shell
       └─ (未来) TwitterAdapter         ← 平台 shell
```

---

## 3. 配置模型

### 3.1 AdapterConfig（平台无关）

```cpp
struct AdapterConfig {
    std::string platform;   // 平台标识（discriminator），必填
    int max_scrolls = 20;   // 最大滚动次数
    int scroll_delay_ms = 1500;  // 每次滚动间隔（ms）
    int page_timeout_ms = 30000; // 页面加载超时（ms）
};
```

### 3.2 LLMConfig（来自 wheel/llm，由 main.cpp 注入）

```cpp
// 来自 src/wheel/llm/llm_models.h
struct LLMConfig {
    std::string provider = "deepseek";
    std::string api_key;
    std::string base_url;
    std::string model = "deepseek-chat";
    double temperature = 0.7;
    int max_tokens = 4096;
    int timeout = 60;
    std::string ca_cert_path;
};
```

### 3.3 Config 传递链

```
config.json → JsonConfigLoader.load()  ← 唯一读文件的地方
  → Config.llm (struct)
    → main.cpp: LLMConfig 转换
      → AdapterFactory::create(browser, config, &llm_config)  ← 只传不建
        → LLMAdapter(browser, config, &llm_config)           ← 只存不读
          → ensure_llm(): LLMFactory().create(llm_config_)    ← 父类自建
```

---

## 4. 使用指南

### 4.1 CDP DOM 路径（无 LLM）

```cpp
#include "core/adapters/adapter_factory.h"
#include "wheel/browser/browser_factory.h"

BrowserFactory browser_factory;
BrowserManager* browser = browser_factory.create(BrowserConfig{});
browser->start();

AdapterFactory adapter_factory;
BaseAdapter* adapter = adapter_factory.create(
    browser,
    AdapterConfig{.platform = "rednote"},
    nullptr  // 不传 LLMConfig → CDP DOM 路径
);

auto items = adapter->search({"AI 模型"}, 20);
for (const auto& item : items) {
    printf("%s\n", item.content_text.c_str());
}

browser->stop();
delete adapter;
delete browser;
```

### 4.2 LLM 路径

```cpp
LLMConfig llm_cfg;
llm_cfg.api_key = "sk-xxx";
llm_cfg.model = "deepseek-chat";

BaseAdapter* adapter = adapter_factory.create(
    browser,
    AdapterConfig{.platform = "rednote"},
    &llm_cfg  // 传入 → LLM 路径
);

auto items = adapter->search({"AI 模型"}, 20);
// 内部自动：骨架提取 → LLM 生成 extractPosts() → 缓存 → CDP 批量提取
```

### 4.3 数据映射

```cpp
nlohmann::json data = {
    {"item_id", "N001"},
    {"author_name", "用户A"},
    {"title", "标题文本"},
    {"likes", 1234},
};
UnifiedItem item = adapter->extract(data);
// → UnifiedItem{.platform = "rednote", .item_id = "N001", ...}
```

`extract()` 方法在 CDP DOM 路径和 LLM 路径下共用。

---

## 5. 实现方案

### 5.1 LLM 提取协议

```
CDP Page.navigate(url)
  → CDP Runtime.evaluate(SKELETON_JS) 提取前 3 个帖子的结构骨架  [~500 tokens]
  → LLM 分析骨架，生成 JS extractPosts() 函数                   [1 次 LLM 调用]
  → CDP Runtime.evaluate(extractPosts) 批量提取全部帖子           [零 LLM，规则执行]
  → 缓存按 URL host + path，后续翻页直接复用
```

### 5.2 RedNoteAdapter（平台 shell）

继承 `LLMAdapter`，提供：
- 平台 URL 常量
- CDP DOM 选择器常量（fallback 用）
- `search()` / `get_user_posts()` / `get_trending()` — 调 `get_or_generate_extract_fn()`
- `extract()` — 映射逻辑

### 5.3 防封策略

| 层级 | 负责模块 | 措施 |
|------|----------|------|
| 全局 | BrowserManager | CDP stealth 注入、统一 viewport/locale |
| 平台级 | AdapterConfig | scroll_delay_ms、max_scrolls |
| 调度级 | Agent | 平台间暂停、重试 |

---

## 6. Factory 设计

```cpp
class AdapterFactory {
public:
    BaseAdapter* create(BrowserManager* browser,
                        const AdapterConfig& config,
                        LLMConfig* llm_config = nullptr);
};
```

- 零构造注入 — `AdapterFactory` 是纯分发函数
- `browser` 由外部（`main.cpp`）创建后传入，工厂不持有
- `llm_config` 只传不建；Engine 由 `LLMAdapter::ensure_llm()` 懒创建
- 返回类型是 `BaseAdapter*`，调用方负责 delete

---

## 7. 错误处理

| 场景 | 行为 |
|------|------|
| CDP Page.navigate 网络错误 | 异常传播给调用方 |
| 页面无帖子元素 | 静默返回空 vector |
| 单条帖子 DOM 损坏 | 该条返回后 caller 跳过 |
| LLM 生成 extractPosts 失败 | 返回空 → 自动 fallback 到 CDP DOM 路径 |
| LLM 生成的 JS 不可执行 | CDP Runtime.evaluate 抛异常 → fallback CDP DOM |
| 重复帖子 | 按 item_id 去重 |

---

## 8. 测试策略

### UT（纯内存）
- **models**: `AdapterConfig` 默认值、自定义值
- **factory**: 创建、未知平台异常、`llm_config` 透传
- **extract**: 全字段映射、最小输入

### TA（真实 CDP headless Chrome + HTML 测试页）
- **search + extract**: 12 项全体提取、limit=3 截断、空页面
- **scroll 懒加载**: 15 项分 4 批展示
- **去重**: 无重复 ID 验证
- **各方法端到端**: search / get_user_posts / get_trending
- **health_check**: OK（file URL）和 fail（不可达端口）
- **CDP stealth**: 验证正常运行
- **LLM 提取**（待 LLMAdapter 完成后）：Mock LLM → 验证 extractPosts 生成和缓存

---

*文档版本：v2.0-cpp | 最后更新：2026-05-02*
