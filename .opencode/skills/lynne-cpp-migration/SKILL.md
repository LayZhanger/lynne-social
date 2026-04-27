---
name: lynne-cpp-migration
description: Lynne C++ migration conventions — syntax constraints, callback async pattern, libuv rules, module mapping, dependency list, CMake build
---

# Lynne C++ 迁移规范

> 本项目的 C++ 迁移必须遵循以下规范。原 Python 版架构不变（core → wheel → common 单向分层、依赖倒置、工厂模式）。

---

## 0. 语法约束（硬规则）

**禁止使用**：

| 禁止项 | 替代方案 |
|--------|---------|
| 泛型 / 模板函数 / 模板类 | 具体类型、裸指针、`std::vector`/`std::map` 等标准容器 |
| `std::future<T>` | 回调模式 `std::function<void(T)>` |
| `std::promise<T>` | 同上，不需要 |
| `std::thread` 直接创建 | `uv_queue_work` / `uv_timer_start` |
| `std::async` | 同上 |
| `CRTP` / `concepts` / `SFINAE` | 纯虚类 + `dynamic_cast`（如需要） |
| 模板元编程 | 不需要 |

**允许使用**：

| 允许项 | 说明 |
|--------|------|
| `std::function<void(Arg)>` | 类型擦除，非用户模板 |
| lambda 表达式 | `[&](){ ... }`、`[this](){ ... }` |
| `std::vector<T>` / `std::map<K,V>` | 标准容器，T/V 为具体类型 |
| `std::unique_ptr<T>` | T 为具体类 |
| `std::recursive_mutex` | 锁 |
| `nlohmann::json` | JSON 库，`NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` 宏展开为普通函数，可用 |
| `#pragma once` | 头文件保护 |

---

## 1. 全局选型

| 维度 | Python v0.2 | C++ 目标 |
|------|------------|----------|
| 语言标准 | Python 3.11 | C++17 |
| 构建系统 | pip + setuptools | CMake 3.16+ |
| 配置格式 | YAML `${ENV_VAR}` | JSON `${ENV_VAR}` |
| 数据建模 | Pydantic BaseModel | struct + nlohmann/json 宏 |
| 事件循环 | asyncio | libuv v1.48 |
| 阻塞工作 | scheduler ThreadPool | `uv_queue_work` (libuv 内置 4 线程) |
| 定时任务 | scheduler + threading | `uv_timer_start` |
| HTTP 客户端 | urllib + scheduler.run_blocking | cpp-httplib v0.17 + `uv_queue_work` |
| 浏览器自动化 | Playwright | 裸 CDP via WebSocket (IXWebSocket v11.4.6) |
| WebSocket | N/A | IXWebSocket |
| 日志 | Loguru | spdlog v1.14.1 |
| 测试框架 | pytest + pytest-asyncio | GTest v1.14.0 |
| 工厂模式 | `Factory[T]` 泛型 | 具体工厂类，`create()` 返回裸指针 |

---

## 2. 外部依赖

所有依赖源码放 `third_party/`，CMake `add_subdirectory` 引入，不依赖系统包管理器。

| # | 库 | 版本 | 类型 | 获取方式 |
|---|-----|------|------|---------|
| 1 | `nlohmann/json` | v3.11.3 | 单头 | `wget` json.hpp |
| 2 | `spdlog` | v1.14.1 | 全头 | `git clone --branch v1.14.1` |
| 3 | `libuv` | v1.48.0 | C 源码 | `git clone --branch v1.48.0` |
| 4 | `cpp-httplib` | v0.17.0 | 单头 | `wget` httplib.h |
| 5 | `IXWebSocket` | v11.4.6 | 头+cp | `git clone --branch v11.4.6` |
| 6 | `googletest` | v1.14.0 | 源码 | `git clone --branch v1.14.0` |

```bash
mkdir -p third_party && cd third_party

# 1
mkdir -p nlohmann && cd nlohmann
wget -q https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
cd ..

# 2
git clone --depth 1 --branch v1.14.1 https://github.com/gabime/spdlog.git

# 3
git clone --depth 1 --branch v1.48.0 https://github.com/libuv/libuv.git

# 4
mkdir -p cpp-httplib && cd cpp-httplib
wget -q https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.17.0/httplib.h
cd ..

# 5
git clone --depth 1 --branch v11.4.6 https://github.com/machinezone/IXWebSocket.git

# 6
git clone --depth 1 --branch v1.14.0 https://github.com/google/googletest.git
```

---

## 3. CMake 构建规范

### 依赖产出物（dist/）

`build-deps.sh` 将 third_party/ 编译后输出到 `dist/`。**项目自身编译产物也输出到 dist/**：

```
dist/
├── include/                  # 第三方头文件
│   ├── json.hpp              # nlohmann/json（单头，扁平，无子目录！）
│   ├── httplib.h             # cpp-httplib（单头）
│   ├── spdlog/               # spdlog（header-only）
│   ├── uv.h + uv/            # libuv 头文件
│   └── ixwebsocket/          # IXWebSocket 头文件
├── lib/                      # 所有静态库（第三方 + 项目）
│   ├── libuv.a / .so         # libuv（build-deps.sh 产出）
│   ├── libixwebsocket.a      # IXWebSocket（build-deps.sh 产出）
│   ├── liblynne_common.a     # 项目模块（build.sh 产出）
│   ├── liblynne_logger.a     # 项目模块（build.sh 产出）
│   ├── libgtest.a / ...      # googletest（build.sh 产出）
│   └── libgmock.a / ...      # googlemock（build.sh 产出）
└── bin/                      # 所有可执行文件
    └── test_*                # 测试程序（build.sh 产出）
```

**重要**：`json.hpp` 是扁平安装的，include 用 `#include <json.hpp>`，不是 `<nlohmann/json.hpp>`。

### 顶层 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(lynne VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DIST_DIR ${CMAKE_SOURCE_DIR}/dist)

# All build outputs → dist/
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${DIST_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${DIST_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${DIST_DIR}/bin)

add_subdirectory(src)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

### src/CMakeLists.txt

```cmake
add_subdirectory(common)
add_subdirectory(wheel/logger)
# ... 其他模块按需添加
```

只做聚合，不在这一层设 include 路径。

### 模块 CMakeLists.txt（以 common 为例）

```cmake
add_library(lynne_common STATIC models.cpp)

target_include_directories(lynne_common PUBLIC
    ${CMAKE_SOURCE_DIR}/src            # 内部头文件： #include "common/module.h"
    ${CMAKE_SOURCE_DIR}/dist/include   # 第三方头文件： #include <json.hpp>
)
```

### 模块 CMakeLists.txt（以 logger 为例，依赖 common）

```cmake
add_library(lynne_logger STATIC
    logger_models.cpp
    logger_factory.cpp
    imp/spdlog_logger.cpp
)

target_include_directories(lynne_logger PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/dist/include
)

target_link_libraries(lynne_logger PUBLIC lynne_common)
```

### 依赖规则（硬规则）

| 依赖类型 | 来源 | include 写法 | CMake 配置 |
|---------|------|-------------|-----------|
| **第三方头文件** | `dist/include/` | `#include <json.hpp>` | `target_include_directories(... PUBLIC ${DIST_DIR}/include)` |
| **项目头文件** | `src/` | `#include "common/module.h"` | `target_include_directories(... PUBLIC ${CMAKE_SOURCE_DIR}/src)` |
| **库文件 (.a)** | `dist/lib/` | —（CMake target 自动解析完整路径） | `target_link_libraries(... PUBLIC lynne_foo)` |

**原理**：
- 项目头文件从 `src/` 直接引用，不安装到 dist。`src/` 是唯一的项目源码树，所有 `#include "common/..."` `#include "wheel/logger/..."` 都从此解析。
- 第三方头文件从 `dist/include/` 引用。`dist/` 是唯一的第三方产物树。
- CMake target 系统自动使用完整路径 `dist/lib/liblynne_foo.a` 链接，不需要 `link_directories()`。
- 每个 target 自己声明 `target_include_directories`，不用全局 `include_directories()`。

**禁止**：
- ❌ 不要把项目头文件装到 `dist/include/` — 保持 `src/` → 项目、`dist/` → 第三方 的清晰边界
- ❌ 不要用 `link_directories()` — target 系统自动处理
- ❌ 不要用全局 `include_directories()` — 每个 target 自声明
- ❌ `#include` 不要写 `src/` 前缀 — include path 是 `src/`，写 `"common/module.h"` 而不是 `"src/common/module.h"`

**模块 CMakeLists.txt 必须包含的两个 include**：
```cmake
target_include_directories(lynne_foo PUBLIC
    ${CMAKE_SOURCE_DIR}/src            # ← 项目头文件
    ${CMAKE_SOURCE_DIR}/dist/include   # ← 第三方头文件
)
```

**规则**：
- 每个模块是一个 `STATIC` 库
- 库间依赖用 `target_link_libraries`，只链实际需要的

### tests/CMakeLists.txt

```cmake
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party)
set(DIST_DIR ${CMAKE_SOURCE_DIR}/dist)

# googletest 从源码编译
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${THIRD_PARTY_DIR}/googletest gtest)

# googletest internal_utils.cmake 强制设 ARCHIVE_OUTPUT → ${CMAKE_BINARY_DIR}/lib
# 必须覆盖，使 gtest/gmock .a 也进 dist/lib/
set_target_properties(gtest gtest_main gmock gmock_main PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${DIST_DIR}/lib"
)

set(GTEST_INCLUDE_DIR ${THIRD_PARTY_DIR}/googletest/googletest/include)

set(COMMON_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/dist/include
    ${GTEST_INCLUDE_DIR}
)

function(add_lynne_test TARGET)
    add_executable(${TARGET} ${ARGN})
    target_include_directories(${TARGET} PRIVATE ${COMMON_TEST_INCLUDES})
    target_link_libraries(${TARGET} gtest gtest_main)
    # gtest_main provides main(); order: gtest then gtest_main
    add_test(NAME ${TARGET} COMMAND ${TARGET})
endfunction()

function(add_lynne_ta TARGET)
    add_executable(${TARGET} ${ARGN})
    target_include_directories(${TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/dist/include
    )
    # standalone main() — no gtest dependency
    add_test(NAME ${TARGET} COMMAND ${TARGET})
endfunction()

add_subdirectory(common)
add_subdirectory(wheel/logger)
```

### tests/module/CMakeLists.txt（以 common 为例）

```cmake
# UT — 使用 gtest
add_lynne_test(test_models_ut test_models_ut.cpp)
target_link_libraries(test_models_ut lynne_common)

# TA — standalone 程序，不依赖 gtest
add_lynne_ta(test_models_ta test_models_ta.cpp)
target_link_libraries(test_models_ta lynne_common)
```

**规则**：
- UT 用 `add_lynne_test`（链接 gtest），TA 用 `add_lynne_ta`（无 gtest，自带 `main()`）
- 测试文件按模块分目录：`tests/common/`, `tests/wheel/logger/`
- googletest 从 third_party/ 源码编译，不走 dist/

### 构建流程

```bash
./build.sh            # 增量编译（configure + build → dist/）
./build.sh --clean    # 清 build/ + dist/ 产出，重编译
./build.sh --test     # 编译 + 运行全部测试
./build.sh --deps     # 先编译依赖，再编译项目
./build.sh --all      # 全流程：deps → clean → configure → build → test
```

---

## 4. 异步回调模式（核心范式）

**禁止使用 `async/await`、`std::future`、`std::promise`。所有异步操作统一使用回调 + lambda。**

### 通用模式

```cpp
#include <functional>
#include <string>

// 结果回调
template<typename T>
using Callback = std::function<void(T)>;

using ErrorCallback = std::function<void(const std::string&)>;
```

> 注意：上面 `template` 仅在 skill 文档中用于说明模式。实际代码中按具体类型写，如在 `ConfigLoader` 中：

```cpp
// config_loader.h —— 具体类型回调，无模板
class ConfigLoader {
public:
    virtual ~ConfigLoader() {}
    virtual void load(
        std::function<void(Config)> on_ok,
        std::function<void(const std::string&)> on_err
    ) = 0;
};
```

### libuv run_blocking 封装

```cpp
// scheduler.h
class Scheduler {
public:
    virtual ~Scheduler() {}
    // work: 在线程池执行的阻塞任务
    // on_done: 回到事件循环线程后回调
    virtual void run_blocking(
        std::function<void()> work,
        std::function<void()> on_done
    ) = 0;

    virtual void add_job(
        const std::string& name,
        const std::string& schedule,
        std::function<void()> callback
    ) = 0;

    virtual void remove_job(const std::string& name) = 0;
};

// uv_scheduler.cpp
struct WorkCtx {
    std::function<void()> work;
    std::function<void()> on_done;
};

static void work_cb(uv_work_t* req) {
    auto* ctx = static_cast<WorkCtx*>(req->data);
    ctx->work();
}

static void after_cb(uv_work_t* req, int status) {
    auto* ctx = static_cast<WorkCtx*>(req->data);
    if (status == 0) ctx->on_done();
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

### 调用示例

```cpp
// 阻塞 HTTP 调用转异步回调
scheduler->run_blocking(
    // 在线程池执行（可阻塞）
    [this]() {
        httplib::Client cli(base_url_);
        auto res = cli.Post("/api/chat", body_, "application/json");
        if (res && res->status == 200) {
            result_ = nlohmann::json::parse(res->body);
            ok_ = true;
        } else {
            err_ = res ? res->body : "fail";
            ok_ = false;
        }
    },
    // 回到主线程回调
    [this, on_ok, on_err]() {
        if (ok_) on_ok(result_);
        else on_err(err_);
    }
);
```

---

## 5. libuv 使用规则

### 事件循环（单一）

```cpp
// main.cpp — 唯一的事件循环
int main() {
    auto* loop = uv_default_loop();

    // 初始化所有模块，注册到 loop
    auto scheduler = UvSchedulerFactory().create(loop, nullptr);
    auto browser   = CdpBrowserManagerFactory().create(loop, browser_cfg);
    // ...

    // 运行事件循环（阻塞直到 uv_stop）
    uv_run(loop, UV_RUN_DEFAULT);
    return 0;
}
```

### 规则

| 规则 | 说明 |
|------|------|
| 一切 I/O 注册到 `uv_default_loop()` | 不使用其他 loop |
| 阻塞工作 → `uv_queue_work` | 禁止裸 `std::thread` |
| 定时任务 → `uv_timer_start` | 禁止 `std::this_thread::sleep_for` |
| 跨线程通知 → `uv_async_send` | 工作线程完成后通知主线程 |
| 所有回调在主线程执行 | `uv_queue_work` 的 `after_cb` 在 loop 线程 |

### libuv 事件循环模型

```
┌──────────────────────────────────────────────────┐
│              uv_default_loop()                    │
│                                                   │
│  ┌──────────┐ ┌──────────┐ ┌──────────────────┐  │
│  │ uv_timer │ │ uv_async │ │ uv_tcp (CDP WS)  │  │
│  │ 定时任务 │ │ 跨线程通知│ │ IXWebSocket      │  │
│  └──────────┘ └──────────┘ └──────────────────┘  │
│                                                   │
│  ┌────────────────────────────────────────────┐   │
│  │       uv_queue_work (默认 4 线程)           │   │
│  │  ├─ LLM HTTP (httplib)                     │   │
│  │  ├─ JSONL 文件写入                          │   │
│  │  └─ 其他阻塞操作                             │   │
│  └────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────┘
```

---

## 6. 模块迁移对照

| # | Python 模块 | 行数 | C++ 模块 | 预计行数 | 难度 | C++ 关键依赖 |
|---|-----------|:---:|---------|:---:|:---:|------------|
| 1 | common | 100 | common | 100 | ★★ | nlohmann/json |
| 2 | logger | 40 | wheel/logger | 130 | ★ | spdlog |
| 3 | config | 130 | wheel/config | 150 | ★ | nlohmann/json |
| 4 | storage | 140 | wheel/storage | 150 | ★ | nlohmann/json |
| 5 | scheduler | 230 | wheel/scheduler | 250 | ★★★ | libuv |
| 6 | llm | 130 | wheel/llm | 120 | ★★ | cpp-httplib + libuv |
| 7 | browser | 220 | wheel/browser | 600 | ★★★★★ | IXWebSocket + libuv |
| 8 | adapters | 730 | core/adapters | 700 | ★★★★ | browser |
| 9 | agent | 40 | core/agent | 80 | ★★ | adapters |

> **已实现**：common + logger + config + storage + scheduler（含 UT/TA 测试，编译通过）
> **推荐顺序**：common → logger → config → storage → scheduler → llm → browser → adapters → agent

**每个模块的 C++ 文件布局**（镜像原 Python）：

```
wheel/{module}/
├── {module}.h             # 抽象接口（extends Module）
├── {module}_models.h      # 数据结构（Pydantic BaseModel → struct + from_json）
├── {module}_models.cpp    # JSON 序列化实现（每个 struct 1 个 cpp）
├── {module}_factory.h     # 工厂声明
├── {module}_factory.cpp   # 工厂 create() 实现（唯一引入 imp/ 的文件）
├── {module}_macros.h      # [可选] 宏 / 头文件辅助（如 logger_macros.h）
├── CMakeLists.txt
└── imp/
    ├── {impl}.h           # 实现类声明
    └── {impl}.cpp         # 实现
```

### logger 模块实际文件清单（参考实现）

```
wheel/logger/
├── CMakeLists.txt
├── logger.h               # Logger ABC : public Module, 新增 LogLevel 枚举
├── logger_models.h        # LogConfig struct
├── logger_models.cpp      # from_json(LogConfig)
├── logger_factory.h       # LoggerFactory 声明
├── logger_factory.cpp     # create() → new SpdlogLogger(config)
├── logger_macros.h        # LOG_INFO(module, fmt, ...) printf 宏
└── imp/
    ├── spdlog_logger.h    # SpdlogLogger 声明
    └── spdlog_logger.cpp  # 实现：sink 创建、log 分发、生命周期
```

### common 模块实际文件清单（参考实现）

```
common/
├── CMakeLists.txt
├── module.h               # Module ABC: start/stop/health_check/name
├── models.h               # UnifiedItem, RunStatus struct + from_json/to_json 声明
└── models.cpp             # JSON 序列化实现
```

### config 模块实际文件清单

```
wheel/config/
├── CMakeLists.txt
├── config_loader.h        # ConfigLoader ABC: load()/reload()
├── config_models.h        # Config + sub-configs (ServerConfig, LLMConfig, etc.)
├── config_models.cpp      # from_json() for all config structs
├── config_factory.h       # ConfigLoaderFactory 声明
├── config_factory.cpp     # create(path?) → new JsonConfigLoader(path)
└── imp/
    ├── json_config_loader.h    # JsonConfigLoader 声明
    └── json_config_loader.cpp  # 实现：文件读取 + JSON 解析 + ${ENV} 替换
```

**关键决策 — JSON 格式**：配置从 YAML 改为 JSON。文件名为 `config.json`（默认），格式与 Python `config.yaml` 等效，但使用 JSON 语法。

**Config 结构**（`config_models.h`）：
- `ServerConfig`: port (7890), auto_open_browser (true)
- `LLMConfig`: provider, api_key, base_url, model, temperature, max_tokens, timeout
- `BrowserConfig`: headless, slow_mo, viewport_width/height, locale, timeout
- `PlatformConfig`: enabled, session_file, base_url, account (map<string, string>)
- `TaskConfig`: name, platforms (vec<string>), intent, schedule ("manual")
- `Config`: 聚合以上所有字段

**`${ENV_VAR}` 替换**：`resolve_env_vars(nlohmann::json&)` 是公开的自由函数（在 `imp/json_config_loader.h` 中声明），递归遍历 JSON 树，对所有字符串值执行 `${VAR}` ⇒ `getenv(VAR)` 替换。若环境变量未设置或为空，抛出 `std::runtime_error`。

**ConfigLoader ABC**（不继承 Module）：
```cpp
class ConfigLoader {
public:
    virtual Config load() = 0;
    virtual Config reload() = 0;      // 重新读取文件
    virtual ~ConfigLoader() = default;
};
```

**JsonConfigLoader 行为**：
- 文件不存在 → 返回全默认 `Config{}`
- 文件为空 → 返回全默认 `Config{}`
- 文件内容为 `null` → 返回全默认 `Config{}`
- 部分字段 → 缺失字段保留默认值
- `reload()` = `load()`（重新读取文件）
- `config()` 访问器返回上次 load 的结果
- **同步文件 I/O**（`std::ifstream`），不需要 scheduler

**测试文件**：
- `test_config_models_ut.cpp`：结构体默认值、from_json、env var 替换、工厂
- `test_config_loader_ta.cpp`：文件加载、reload、env 替换、工厂文件集成

### storage 模块实际文件清单

```
wheel/storage/
├── CMakeLists.txt
├── storage.h               # Storage ABC : public Module
├── storage_models.h        # StorageConfig struct
├── storage_models.cpp      # from_json(StorageConfig)
├── storage_factory.h       # StorageFactory 声明
├── storage_factory.cpp     # create()/create(path)/create(config) → new JsonlStorage
└── imp/
    ├── jsonl_storage.h     # JsonlStorage 声明
    └── jsonl_storage.cpp   # 实现：JSONL 文件读写、report/md、summary/json、日期管理
```

**Storage ABC**（继承 Module）：
```cpp
class Storage : public common::Module {
    virtual void save_items(items, date) = 0;
    virtual vector<UnifiedItem> load_items(date, platform?) = 0;
    virtual void save_report(markdown, date) = 0;
    virtual string load_report(date) = 0;
    virtual void save_summary(json, date) = 0;
    virtual json load_summary(date) = 0;
    virtual vector<string> list_dates() = 0;
};
```

**目录结构**（`data_dir/YYYY-MM-DD/`）：
```
data/2026-01-01/items.jsonl    # 每条 UnifiedItem 一行 JSON
data/2026-01-01/report.md       # Markdown 报告
data/2026-01-01/summary.json    # 摘要 JSON
data/2026-01-02/...
```

**JsonlStorage 行为**：
- `start()` — 创建 data_dir 目录
- `save_items()` — 追加写（`ios::app`），每行 `to_json(item).dump()`
- `load_items()` — 逐行解析 JSONL，可选按 `platform` 过滤
- `save_report()` — 覆盖写 markdown 文件
- `load_report()` — 文件不存在返回空字符串
- `save_summary()` — `dump(2)` 缩进 JSON
- `load_summary()` — 文件不存在返回 null JSON
- `list_dates()` — 降序排列日期目录，排除 `sessions/`
- **同步文件 I/O**（`std::ifstream`/`std::ofstream`），不需要 scheduler

**测试文件**：
- `test_storage_ut.cpp`：StorageConfig 默认值、from_json、工厂三种创建方式
- `test_storage_ta.cpp`：全生命周期（start→save→load→round-trip）、platform 过滤、追加、report 覆盖、summary、list_dates 排序与 sessions 排除、多日期隔离、LLM 字段序列化

### scheduler 模块实际文件清单

```
wheel/scheduler/
├── CMakeLists.txt
├── scheduler.h               # Scheduler ABC : public Module
├── scheduler_models.h        # SchedulerConfig struct
├── scheduler_models.cpp      # from_json(SchedulerConfig)
├── scheduler_factory.h       # SchedulerFactory 声明
├── scheduler_factory.cpp     # create(loop, config) → new UvScheduler
└── imp/
    ├── uv_scheduler.h        # UvScheduler 声明
    └── uv_scheduler.cpp      # 实现：uv_queue_work / uv_timer / uv_async_send
```

**Scheduler ABC**（继承 Module）：
```cpp
class Scheduler : public common::Module {
    virtual void run_blocking(work, on_done) = 0;
    virtual void post(callback) = 0;
    virtual void add_job(name, interval_ms, callback) = 0;
    virtual void remove_job(name) = 0;
};
```

**libuv 映射**：
| 接口 | libuv 原语 | 调用方 |
|------|-----------|--------|
| `run_blocking(work, on_done)` | `uv_queue_work` | storage、llm、browser |
| `post(callback)` | `uv_async_send` | browser（IXWebSocket→loop） |
| `add_job(name, interval_ms, cb)` | `uv_timer_start` | agent（定时爬取） |
| `remove_job(name)` | `uv_timer_stop` + `uv_close` | agent |

**UvScheduler 实现要点**：
- Factory 需要 `uv_loop_t*`（由 `main.cpp` Composition Root 创建传入），不持有 loop 所有权
- `start()` — 初始化 async handle（ref'd，保持 loop 存活）
- `stop()` — 停止所有 timer + 关闭 async handle
- `run_blocking` — 分配 `WorkCtx` + `uv_work_t`，`work` 在线程池执行，`on_done` 回 loop 线程
- `add_job` — 重复定时器（`uv_timer_start(repeat)`），同名覆盖写入旧 handle
- `remove_job` — `uv_timer_stop` + `uv_close`，close callback 中释放 `TimerCtx`
- `post` — `std::mutex` 保护队列，`uv_async_send` 唤醒 loop，`drain_post_queue` 串行执行
- **所有用户回调都包在 try/catch 中** — 异常不崩溃 scheduler

**测试文件**：
- `test_scheduler_ut.cpp`：SchedulerConfig 默认值、from_json、Factory（需要 libuv loop）
- `test_scheduler_ta.cpp`：全生命周期（name/health/start/stop）、run_blocking 线程池执行、post 回调、add_job 一次性/重复/多 job 独立/异常安全、factory 集成、析构清理

### common 模块实际文件清单（参考实现）

---

## 7. 编程规范

### 命名

| 元素 | 规范 | 示例 |
|------|------|------|
| 类型名 | PascalCase | `class JsonConfigLoader` |
| 函数/方法 | snake_case | `void load(...)` |
| 成员变量 | 尾下划线 `_` | `std::string path_;` |
| 局部变量 | snake_case | `auto result = ...` |
| 常量 | kPascalCase | `const char* kBaseUrl` |
| 命名空间 | 小写 | `namespace lynne::wheel` |

### 所有权

| 场景 | 方式 |
|------|------|
| 独占所有权 | 裸指针 + Ctor 分配 / Dtor delete |
| 工厂返回 | 裸指针，调用方负责 delete |
| 参数传递 | 指针或 const 引用 |
| 回调捕获 | lambda `[this]` 或按值捕获 |

### 头文件

```cpp
#pragma once

#include <string>
#include <vector>
// ... 只 include 实际使用的

namespace lynne { namespace wheel {

class SomeClass {
public:
    virtual ~SomeClass() {}
    virtual void do_work(std::function<void(int)> on_done) = 0;
};

}} // namespace
```

### 错误处理

- 抛 `std::runtime_error("message")`
- 回调通知：`on_err("message")`
- 禁止 `abort()` / `exit()` / `assert()` 用于业务逻辑
- 禁止 `longjmp`

---

## 8. 测试规范

### 文件命名

每个模块两个测试文件：

```
tests/{module}/
├── test_{module}_ut.cpp    # UT — 纯内存，默认值、序列化、枚举
└── test_{module}_ta.cpp    # TA — 全链路，factory→impl→round-trip
```

| 类型 | C++ 框架 | 构建宏 | 特点 |
|------|---------|--------|------|
| UT | **gtest** | `add_lynne_test` | 无 I/O，`TEST(Suite, Case)`，链接 `gtest gtest_main` |
| TA | **standalone** | `add_lynne_ta` | 有 I/O，自定义 `main()`，不依赖 gtest，手动 pass/fail 计数 |

### UT 模式（gtest，以 LogConfig 为例）

```cpp
#include "wheel/logger/logger_models.h"
#include "wheel/logger/logger.h"
#include <gtest/gtest.h>

using namespace lynne::wheel;

TEST(LogConfigDefaults, AllFieldsHaveSaneDefaults) {
    LogConfig c{};
    EXPECT_EQ(c.level, "INFO");
    EXPECT_EQ(c.log_file, "data/lynne.log");
}

TEST(LogConfigJson, FromJsonMinimal) {
    auto j = nlohmann::json::parse("{}");
    LogConfig c;
    from_json(j, c);
    EXPECT_EQ(c.level, "INFO");  // default survived
}

TEST(LogConfigJson, PartialOverride) {
    auto j = nlohmann::json::parse(R"({"level":"ERROR"})");
    LogConfig c;
    from_json(j, c);
    EXPECT_EQ(c.level, "ERROR");
    EXPECT_EQ(c.log_file, "data/lynne.log");  // default
}
```

要点：
- `using namespace lynne::wheel;` or `lynne::common;` 简化命名空间
- 测试默认值、最小 JSON、部分覆盖、全字段 JSON
- 枚举类型单独测试值互异

### TA 模式（standalone，以 SpdlogLogger 为例）

TA 是**自带 `main()` 的可执行程序**，不依赖 gtest。用 `printf` 输出 pass/fail，返回非零退出码表示失败，仍注册在 ctest 中。

```cpp
#include "wheel/logger/logger.h"
#include "wheel/logger/logger_factory.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace lynne::wheel;

static int passed = 0;
static int failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { printf("  [PASS] %s\n", msg); ++passed; } \
         else { printf("  [FAIL] %s\n", msg); ++failed; } \
    } while (0)

int main() {
    fs::path tmp_dir = fs::temp_directory_path() / "lynne_test_logger";
    fs::create_directories(tmp_dir);

    std::string log_path = (tmp_dir / "test.log").string();
    LogConfig cfg;
    cfg.log_file = log_path;
    cfg.level = "DEBUG";

    LoggerFactory factory;
    Logger* logger = factory.create(cfg);

    logger->start();
    logger->log(LogLevel::Info, "[test] hello world");
    logger->log(LogLevel::Error, "[test] error");
    logger->stop();

    std::ifstream f(log_path);
    std::string content(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());
    CHECK(content.find("[test] hello world") != std::string::npos, "log contains hello");
    CHECK(content.find("[test] error") != std::string::npos, "log contains error");

    delete logger;
    fs::remove_all(tmp_dir);

    printf("\n== %d/%d passed ==\n", passed, passed + failed);
    return failed > 0 ? 1 : 0;
}
```

要点：
- 没有 `SetUp/TearDown` — 所有资源在局部作用域内手动管理
- `fs::remove_all(tmp_dir)` 在 `return` 前清理
- `return failed > 0 ? 1 : 0` — ctest 通过退出码判断 pass/fail
- 仍用 `add_test(NAME ... COMMAND ...)` 注册到 ctest
- TA 源文件的 include 路径不含 `${GTEST_INCLUDE_DIR}`（`add_lynne_ta` 不引入 gtest）

---

## 9. 迁移阶段顺序

| 阶段 | 模块 | 预计行数 | 可并行测试 |
|:---:|------|:---:|:---:|
| 1 | config → logger → storage | ~380 | 是 |
| 2 | common + scheduler | ~350 | scheduler 需 libuv |
| 3 | llm | ~120 | 需 scheduler |
| 4 | browser (CDP) | ~600 | 需 libuv + IXWebSocket |
| 5 | adapters | ~700 | 需 browser |
| 6 | agent | ~80 | 需 adapters |

---

## 10. 项目目录结构（实际）

```
lynne-cpp/
├── CMakeLists.txt
├── build-deps.sh            # 一键编译依赖（third_party → dist）
├── build.sh                 # 一键编译项目（cmake build + test）
├── dist/                    # 依赖产出物（build-deps.sh 生成）
│   ├── include/             # json.hpp, httplib.h, spdlog/, uv.h, ixwebsocket/
│   └── lib/                 # libuv.a, libixwebsocket.a
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp                                 # [待实现]
│   ├── common/
│   │   ├── CMakeLists.txt
│   │   ├── module.h        # Module ABC
│   │   ├── models.h        # UnifiedItem, RunStatus
│   │   └── models.cpp      # JSON 序列化
│   ├── wheel/
│   │   ├── config/          ✅ 已实现
│   │   │   ├── CMakeLists.txt
│   │   │   ├── config_loader.h
│   │   │   ├── config_models.h / .cpp
│   │   │   ├── config_factory.h / .cpp
│   │   │   └── imp/json_config_loader.h / .cpp
│   │   ├── logger/          ✅ 已实现
│   │   │   ├── CMakeLists.txt
│   │   │   ├── logger.h / logger_models.h / .cpp
│   │   │   ├── logger_factory.h / .cpp
│   │   │   ├── logger_macros.h
│   │   │   └── imp/spdlog_logger.h / .cpp
│   │   ├── storage/         ✅ 已实现
│   │   ├── scheduler/       ✅ 已实现
│   │   ├── llm/                                 # [待实现]
│   │   └── browser/                             # [待实现]
│   └── core/                                    # [待实现]
│       ├── adapters/
│       └── agent/
├── tests/
│   ├── CMakeLists.txt
│   ├── common/
│   │   ├── CMakeLists.txt
│   │   ├── test_models_ut.cpp    ✅
│   │   └── test_models_ta.cpp    ✅
│   └── wheel/
│       └── logger/
│           ├── CMakeLists.txt
│           ├── test_logger_ut.cpp ✅
│           └── test_logger_ta.cpp ✅
└── third_party/
    ├── CMakeLists.txt          # 依赖编译定义
    ├── nlohmann/json.hpp
    ├── spdlog/
    ├── libuv/
    ├── cpp-httplib/httplib.h
    ├── IXWebSocket/
    └── googletest/
```

---

## 11. 已知陷阱 & 解决方案

### spdlog `from_str()` 大小写敏感

`spdlog::level::from_str("INFO")` 返回 `off`(6)，不是 `info`(2)。
只有小写字符串才能正确解析。

**解决**：在传给 `from_str` 前转小写：

```cpp
#include <cctype>
auto level_str = config_.level;  // "INFO"
for (auto& c : level_str) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
auto level = spdlog::level::from_str(level_str);  // 正确得到 info
```

### spdlog `log()` 不出输出

如果日志配置了但文件/控制台无输出，检查：
1. **level 值**：`from_str` 返回了 off 吗？打印 `static_cast<int>(level)` 验证
2. **flush**：`stop()` 前调用 `logger->flush()` 或析构前保证 spdlog::drop 触发 flush
3. **sink**：文件 sink 创建是否抛异常被静默吞掉（不要空 catch）

### json.hpp 是扁平安装的

`dist/include/json.hpp`（不是 `dist/include/nlohmann/json.hpp`）：
```cpp
#include <json.hpp>           // 正确
// #include <nlohmann/json.hpp> // 错误！找不到
```

### GTest 从源码编译

googletest **不要**装到 dist/，直接 `add_subdirectory(third_party/googletest)`，且必须设置 `set(BUILD_TESTING OFF CACHE BOOL "" FORCE)` 避免 gtest 自己的测试被触发。

googletest 的 `internal_utils.cmake` 强制把输出路径设为 `${CMAKE_BINARY_DIR}/lib`，需要通过 `set_target_properties` 覆盖为 `${DIST_DIR}/lib`，否则 gtest 的 .a 不会进入 `dist/lib/`。

---

*Skill 版本：v0.2 | 最后更新：2026-04-27*
