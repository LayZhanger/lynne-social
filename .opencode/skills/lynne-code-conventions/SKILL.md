---
name: lynne-code-conventions
description: Lynne project code conventions — layering, dependency inversion, factory pattern, testing standards, file layout
---

# Lynne C++ 代码规范

> 本项目的所有 C++ 代码必须遵循以下规范。

---

## 1. 目录分层架构

```
src/
├── common/          # 公共资源 — 所有模块共享的基类和规约
├── wheel/           # 通用基础设施 — 与业务无关，可跨项目复用
└── core/            # 业务逻辑 — Lynne 特有功能
```

**依赖方向**：`core → wheel → common` （单向，不可反向）

---

## 2. 模块内部结构

每个模块目录统一包含以下文件：

```
{module}/
├── {module}.h                 # 抽象接口 (纯虚类)
├── {module}_models.h          # 模块专属数据结构 (struct)
├── {module}_models.cpp        # from_json / to_json 实现
├── {module}_factory.h         # 工厂类声明
├── {module}_factory.cpp       # create() 分发实现
└── imp/
    ├── {impl}.h               # 具体实现声明
    └── {impl}.cpp             # 具体实现
```

| 文件 | 内容 | 示例 |
|------|------|------|
| `{module}.h` | 抽象接口类（纯虚） | `class Storage : public Module` |
| `{module}_models.h` | 本模块数据结构 | `struct StorageConfig` + `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` |
| `{module}_factory.h` | 工厂类 | `class StorageFactory` |
| `imp/{impl}.h/.cpp` | 具体实现 | `class JsonlStorage : public Storage` |

---

## 3. 依赖倒置原则（最重要）

1. **模块之间只依赖接口（纯虚类），不依赖具体实现**
2. **任何模块的代码中，不允许出现 `#include "imp/..."`**
3. **具体实现通过工厂在 `main.cpp` 中注入**

```cpp
// ✅ 正确：构造参数用接口
class DefaultAgent : public Agent {
    DefaultAgent(
        LLMEngine* llm,      // 接口，不是 DeepSeekEngine
        Storage* storage,    // 接口，不是 JsonlStorage
        Scheduler* scheduler // 接口，不是 UvScheduler
    );
};

// ❌ 禁止：直接引用实现
#include "wheel/llm/imp/deepseek_engine.h"
auto* llm = new DeepSeekEngine(config, scheduler);
```

---

## 4. 工厂模式（工厂不持有依赖）

**硬规则**：工厂是纯分发器，不持有、不创建任何运行时依赖。

```
工厂只做一件事：config → 选择实现类 → new 返回。
实现的依赖由实现自身内部创建（通过各自的工厂、懒初始化），
或由 main.cpp 直接传入实现构造器，不经过工厂。
```

```cpp
// ✅ 正确：工厂零依赖，纯分发
class AdapterFactory {
    BaseAdapter* create(const AdapterConfig& config);
};

// ✅ 正确：实现内部自建子依赖
class LLMAdapter : public BaseAdapter {
    LLMConfig config_;
    LLMEngine* llm_ = nullptr;

    void ensure_llm() {
        if (!llm_) {
            LLMFactory factory;
            llm_ = factory.create(config_);
            llm_->start();
        }
    }
};

// ❌ 禁止：工厂持有依赖或创建资源
class AdapterFactory {
    BrowserManager* browser_; // ❌ 工厂不应持有依赖
};
```

**工厂可以 include `imp/` 文件**（这是它存在的目的——把 platform 映射到具体类）。除此以外的代码禁止跨层 include `imp/`。

---

## 5. Module 基类

所有需要生命周期的模块继承 `common::Module`：

```cpp
class Module {
public:
    virtual ~Module() {}
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool health_check() = 0;
    virtual const char* name() = 0;
};
```

---

## 6. 公共数据规约

`common/models.h` 定义跨模块共享的数据结构：
- `UnifiedItem` — 跨平台统一内容
- `RunStatus` — 运行状态

跨模块共享的结构体必须放 `common/models.h`，不能放各自模块的 `_models.h`。

---

## 7. 启动入口职责

`main.cpp` 是唯一的 **组合根（Composition Root）**，负责加载配置、创建工厂、组装依赖。`main.cpp` 是唯一可以引用具体实现和工厂的地方。

---

## 8. 语法约束

| 禁止 | 替代方案 |
|------|---------|
| 模板 / 泛型 | 具体类型、裸指针、标准容器 |
| `std::future<T>` / `std::promise<T>` | `std::function<void(T)>` 回调 |
| `std::thread` 直接创建 | `uv_queue_work` / `uv_timer_start` |
| CRTP / concepts / SFINAE | 纯虚类 + `dynamic_cast` |
| `std::make_shared` / `std::make_unique` 在接口中 | 裸指针 |

| 允许 | 说明 |
|------|------|
| `std::function<void(Arg)>` | 类型擦除，非用户模板 |
| lambda | `[&](){ ... }`, `[this](){ ... }` |
| `std::vector<T>` / `std::map<K,V>` | 标准容器，T/V 为具体类型 |
| `std::unique_ptr<T>` | T 为具体类 |
| `std::recursive_mutex` | 锁 |
| `nlohmann::json` | JSON 库 + `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` |
| `#pragma once` | 头文件保护 |

---

## 9. 命名规范

| 元素 | 规范 | 示例 |
|------|------|------|
| 类型名 | PascalCase | `class JsonConfigLoader` |
| 函数/方法 | snake_case | `void load(...)` |
| 成员变量 | 尾下划线 `_` | `std::string path_;` |
| 局部变量 | snake_case | `auto result = ...` |
| 常量 | kPascalCase | `const char* kBaseUrl` |
| 命名空间 | 小写 | `namespace lynne::wheel` |

---

## 10. 所有权

| 场景 | 方式 |
|------|------|
| 独占所有权 | 裸指针 + Ctor 分配 / Dtor delete |
| 工厂返回 | 裸指针，调用方负责 delete |
| 参数传递 | 指针或 const 引用 |
| 回调捕获 | lambda `[this]` 或按值捕获 |

---

## 11. 测试规范

| 缩写 | 全称 | 框架 |
|------|------|------|
| **UT** | Unit Test | GTest (`add_lynne_test`) |
| **TA** | 单模块 E2E | standalone (`add_lynne_ta`) |

目录镜像：
```
src/{module}/{module}_models.h  →  tests/{module}/test_{module}_ut.cpp   (UT)
src/{module}/{module}_factory.h →  tests/{module}/test_{module}_ut.cpp   (UT)
src/{module}/imp/{impl}.h/.cpp  →  tests/{module}/test_{module}_ta.cpp   (TA)
```

运行命令：
```bash
./build.sh --test                           # 全部
ctest --test-dir build --output-on-failure  # 全部
dist/bin/test_storage_ta                    # 单个 TA
DEEPSEEK_API_KEY="sk-xxx" dist/bin/test_llm_ta  # LLM TA（需 API key）
```

---

## 12. 头文件模板

```cpp
#pragma once

#include <string>
#include <vector>

namespace lynne { namespace wheel {

class SomeClass {
public:
    virtual ~SomeClass() {}
    virtual void do_work(std::function<void(int)> on_done) = 0;
};

}} // namespace
```
