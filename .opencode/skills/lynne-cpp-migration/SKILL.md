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

### 顶层 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(lynne VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party)

# 需编译的依赖
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
set(COMMON_INCLUDES
    ${THIRD_PARTY_DIR}/nlohmann
    ${THIRD_PARTY_DIR}/cpp-httplib
    ${THIRD_PARTY_DIR}/spdlog/include
    ${THIRD_PARTY_DIR}/IXWebSocket
    ${THIRD_PARTY_DIR}/libuv/include
)

add_subdirectory(common)
add_subdirectory(wheel/config)
add_subdirectory(wheel/logger)
add_subdirectory(wheel/storage)
add_subdirectory(wheel/scheduler)
add_subdirectory(wheel/llm)
add_subdirectory(wheel/browser)
add_subdirectory(core/adapters)
add_subdirectory(core/agent)

add_executable(lynne main.cpp)
target_include_directories(lynne PRIVATE ${COMMON_INCLUDES})
target_link_libraries(lynne PRIVATE
    lynne_common lynne_config lynne_logger lynne_storage
    lynne_scheduler lynne_llm lynne_browser lynne_adapters lynne_agent
    uv_a ixwebsocket
)
```

### tests/CMakeLists.txt

```cmake
add_subdirectory(${THIRD_PARTY_DIR}/googletest gtest)

function(add_lynne_test NAME)
    add_executable(${NAME} ${ARGN})
    target_include_directories(${NAME} PRIVATE ${COMMON_INCLUDES})
    target_link_libraries(${NAME} ${ARGN})
    add_test(NAME ${NAME} COMMAND ${NAME})
endfunction()

add_lynne_test(test_config wheel/config/test_config_loader.cpp lynne_config gtest gtest_main)
# ... 其他测试
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
| 1 | config | 130 | wheel/config | 150 | ★ | nlohmann/json |
| 2 | logger | 40 | wheel/logger | 80 | ★ | spdlog |
| 3 | storage | 140 | wheel/storage | 150 | ★ | nlohmann/json |
| 4 | common | 100 | common | 100 | ★★ | 无 |
| 5 | scheduler | 230 | wheel/scheduler | 250 | ★★★ | libuv |
| 6 | llm | 130 | wheel/llm | 120 | ★★ | cpp-httplib + libuv |
| 7 | browser | 220 | wheel/browser | 600 | ★★★★★ | IXWebSocket + libuv |
| 8 | adapters | 730 | core/adapters | 700 | ★★★★ | browser |
| 9 | agent | 40 | core/agent | 80 | ★★ | adapters |

**每个模块的 C++ 文件布局**（镜像原 Python）：

```
wheel/{module}/
├── {module}.h             # 抽象接口（替代 ABC .py）
├── {module}_models.h      # 数据结构（替代 Pydantic _models.py）
├── {module}_factory.h     # 工厂类声明
├── {module}_factory.cpp   # 工厂 create() 实现
└── imp/
    ├── {impl}.h           # 实现类声明
    └── {impl}.cpp         # 实现
```

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

| 类型 | 对应 Python | C++ GTest |
|------|-----------|-----------|
| UT | 单类/函数，纯内存 | `TEST(Suite, Case)` 或 `TEST_F` |
| TA | 单模块全链路 | `TEST_F` + `std::filesystem::temp_directory_path()` |

**文件镜像**：
```
tests/wheel/config/test_config_loader.cpp   ← 原 test_config_loader.py
tests/wheel/storage/test_jsonl_storage.cpp  ← 原 test_storage.py
```

**示例**：
```cpp
#include <gtest/gtest.h>
#include "src/wheel/config/imp/json_config_loader.h"

TEST(ConfigLoaderTest, LoadsDefaultsForMissingFile) {
    JsonConfigLoader loader("nonexistent.json");
    Config cfg;
    bool done = false;
    loader.load(
        [&](Config c) { cfg = std::move(c); done = true; },
        [&](const std::string&) { FAIL() << "should not fail"; done = true; }
    );
    // 注：如果 ConfigLoader 改为异步回调，此处需要事件循环支持
    EXPECT_EQ(cfg.server.port, 7890);
}
```

> 注：对于纯数据模块（config、storage），`load()` 在启动阶段可保持同步以简化测试。

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

## 10. 项目目录结构（完整）

```
lynne-cpp/
├── CMakeLists.txt
├── config.json
├── data/
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── common/
│   │   ├── module.h
│   │   └── models.h
│   ├── wheel/
│   │   ├── config/
│   │   │   ├── config_loader.h
│   │   │   ├── config_models.h
│   │   │   ├── config_factory.h
│   │   │   ├── config_factory.cpp
│   │   │   └── imp/
│   │   │       ├── json_config_loader.h
│   │   │       └── json_config_loader.cpp
│   │   ├── logger/
│   │   │   ├── logger.h
│   │   │   └── logger.cpp
│   │   ├── storage/
│   │   │   ├── storage.h
│   │   │   ├── storage_models.h
│   │   │   ├── storage_factory.h / .cpp
│   │   │   └── imp/
│   │   │       ├── jsonl_storage.h
│   │   │       └── jsonl_storage.cpp
│   │   ├── scheduler/
│   │   │   ├── scheduler.h
│   │   │   ├── scheduler_models.h
│   │   │   ├── scheduler_factory.h / .cpp
│   │   │   └── imp/
│   │   │       ├── uv_scheduler.h
│   │   │       └── uv_scheduler.cpp
│   │   ├── llm/
│   │   │   ├── llm_engine.h
│   │   │   ├── llm_models.h
│   │   │   ├── llm_factory.h / .cpp
│   │   │   └── imp/
│   │   │       ├── deepseek_engine.h
│   │   │       └── deepseek_engine.cpp
│   │   └── browser/
│   │       ├── browser_manager.h
│   │       ├── browser_models.h
│   │       ├── browser_factory.h / .cpp
│   │       └── imp/
│   │           ├── cdp_browser_manager.h
│   │           └── cdp_browser_manager.cpp
│   └── core/
│       ├── adapters/
│       │   ├── base_adapter.h
│       │   ├── adapter_models.h
│       │   ├── adapter_factory.h / .cpp
│       │   └── imp/
│       │       ├── llm_adapter.h / .cpp
│       │       └── rednote_adapter.h / .cpp
│       └── agent/
│           ├── agent.h / .cpp
│           ├── agent_models.h
│           └── tools.h / .cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── wheel/config/test_config_loader.cpp
│   └── ...
└── third_party/
    ├── nlohmann/json.hpp
    ├── spdlog/
    ├── libuv/
    ├── cpp-httplib/httplib.h
    ├── IXWebSocket/
    └── googletest/
```

---

*Skill 版本：v0.1 | 最后更新：2026-04-26*
