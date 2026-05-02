# Lynne 测试规范 v2.0（C++）

> 所有模块的测试必须遵循本规范。
> 新增模块时，按此模板创建 `tests/` 下对应的测试文件。

---

## 1. 测试类型定义

| 缩写 | 全称 | 范围 | 框架 | 被测目标 |
|------|------|------|------|----------|
| **UT** | Unit Test | 最小单元，无副作用 | GTest (`add_lynne_test`) | 一个类/一个函数：默认值、序列化、校验、边界 |
| **TA** | 单模块 E2E | 一个模块全链路 | standalone (`add_lynne_ta`) | factory → impl → 全部 public 方法 + 生命周期 + 边界 + 错误路径 |

---

## 2. 目录镜像规则

```
src/xxx.h/cpp                              → tests/xxx/test_xxx.cpp
src/{module}/{module}_models.h             → tests/{module}/test_{module}_ut.cpp    (UT)
src/{module}/{module}_factory.h            → tests/{module}/test_{module}_ut.cpp    (UT)
src/{module}/imp/{impl}.h / {impl}.cpp     → tests/{module}/test_{module}_ta.cpp    (TA)
```

**规则**：
- 每个模块对应两个测试文件（UT + TA），不是每个源文件一个
- ABC 接口文件（`{module}.h`）不单独测试（无实例可测）
- logger 工具函数类不单独测试
- 测试文件放在 `tests/` 下，目录层级与 `src/` 镜像

---

## 3. 文件命名

```
tests/{module}/test_{module}_ut.cpp       ← UT: GTest
tests/{module}/test_{module}_ta.cpp       ← TA: standalone
```

---

## 4. UT vs TA 职责边界

| | UT | TA |
|---|---|---|
| 测什么 | 一个类/函数 | 一个模块的完整实现 |
| 隔离方式 | 纯内存，无外部依赖 | 真文件系统（`temp_directory_path`）、真环境变量 |
| 侧重点 | 边界值、默认值、序列化、类型校验、异常 | 往返验证、状态变更、顺序性、生命周期、错误路径 |
| 示例 | `LLMConfig cfg{}; cfg.provider == "deepseek"` | `JsonConfigLoader` load → reload → env 替换全流程 |
| **不测** | 文件 IO、网络、多类协作 | 跨模块协作 |

---

## 5. 每个模块的覆盖标准

```
_source files_               _test type_    _至少应覆盖_

{module}_models.h              UT           struct 默认值、字段必填/可选校验、
                                            from_json / to_json 序列化/反序列化、边界空值/空列表

{module}_factory.h             UT           Factory create(默认) → 默认实现
                                            Factory create(config) → 指定参数
                                            返回类型 dynamic_cast 断言

imp/{impl}.h / .cpp            TA           启动 → 停止 生命周期
                                            health_check 通过/不通过
                                            全部 public 方法 × (正常 + 空 + 异常)
                                            往返一致性: 写 → 读 → 相等
                                            顺序性: list_dates 排序
                                            name 属性
```

---

## 6. UT 模板（GTest）

```cpp
#include <gtest/gtest.h>
#include "wheel/config/config_models.h"
#include "nlohmann/json.hpp"

using namespace lynne::wheel;

TEST(LLMConfigDefaults, AllFieldsHaveSaneDefaults) {
    LLMConfig cfg{};
    EXPECT_EQ(cfg.provider, "deepseek");
    EXPECT_EQ(cfg.model, "deepseek-chat");
    EXPECT_EQ(cfg.temperature, 0.7);
    EXPECT_EQ(cfg.max_tokens, 4096);
}

TEST(LLMConfigJson, FromJsonMinimal) {
    auto j = nlohmann::json::parse("{}");
    LLMConfig cfg;
    from_json(j, cfg);
    EXPECT_EQ(cfg.provider, "deepseek");  // default survived
}

TEST(LLMConfigJson, PartialOverride) {
    auto j = nlohmann::json::parse(R"({"model":"gpt-4"})");
    LLMConfig cfg;
    from_json(j, cfg);
    EXPECT_EQ(cfg.model, "gpt-4");
    EXPECT_EQ(cfg.provider, "deepseek");  // default
}

// 工厂 create 三种方式
TEST(StorageFactoryTest, CreateDefault) {
    StorageFactory factory;
    Storage* s = factory.create();
    EXPECT_NE(s, nullptr);
    EXPECT_TRUE(dynamic_cast<JsonlStorage*>(s) != nullptr);
    delete s;
}
```

---

## 7. TA 模板（standalone）

TA 是自带 `main()` 的可执行程序，不依赖 GTest，用 `printf` 输出 pass/fail，返回非零退出码表示失败。

```cpp
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include "wheel/storage/storage.h"
#include "wheel/storage/storage_factory.h"

namespace fs = std::filesystem;
using namespace lynne::wheel;

static int passed = 0;
static int failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { printf("  [PASS] %s\n", msg); ++passed; } \
         else { printf("  [FAIL] %s\n", msg); ++failed; } \
    } while (0)

int main() {
    fs::path tmp = fs::temp_directory_path() / "lynne_test_storage";
    fs::create_directories(tmp);

    StorageConfig cfg;
    cfg.data_dir = tmp / "data";
    StorageFactory factory;
    Storage* s = factory.create(cfg);
    CHECK(s != nullptr, "factory create");

    s->start();
    CHECK(s->health_check(), "health check after start");
    CHECK(std::string(s->name()) == "JsonlStorage", "name");

    // 往返验证
    UnifiedItem item;
    item.platform = "twitter";
    item.item_id = "123";
    s->save_items({item}, "2026-01-01");
    auto loaded = s->load_items("2026-01-01");
    CHECK(loaded.size() == 1, "save and load round-trip");
    CHECK(loaded[0].item_id == "123", "item_id preserved");

    s->stop();
    delete s;
    fs::remove_all(tmp);

    printf("\n== %d/%d passed ==\n", passed, passed + failed);
    return failed > 0 ? 1 : 0;
}
```

---

## 8. 构建集成

在 `tests/{module}/CMakeLists.txt` 中注册：

```cmake
# UT
add_lynne_test(test_config_models_ut
    test_config_models_ut.cpp
)
target_link_libraries(test_config_models_ut
    lynne_config
)

# TA
add_lynne_ta(test_config_loader_ta
    test_config_loader_ta.cpp
)
target_link_libraries(test_config_loader_ta
    lynne_config
)
```

---

## 9. 运行命令

```bash
# 全部测试
./build.sh --test

# 等同于：
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure -j$(nproc)

# 单个测试
dist/bin/test_storage_ta

# LLM TA（需 API key）
DEEPSEEK_API_KEY="sk-xxx" dist/bin/test_llm_ta
```

---

## 10. 测试检查清单

- [ ] UT 覆盖 `_models.h` — 至少 创建默认值 + from_json / to_json
- [ ] UT 覆盖 `_factory.h` — 至少 3 种 create 入参
- [ ] TA 覆盖 `imp/{impl}.h/.cpp` — 生命周期 + 所有 public 方法 × (正常 + 空 + 异常)
- [ ] TA 包含往返验证 — 写后立即读，数据一致
- [ ] 使用 `std::filesystem::temp_directory_path()` 隔离文件系统
- [ ] TA 返回 `failed > 0 ? 1 : 0` — ctest 通过退出码判断

## 11. 测试文档规范

> 每个模块完成测试后，必须在 `doc/tests/` 下产出两份文档。

### 目录结构（与 `src/` 镜像）

```
doc/tests/
├── common/
│   ├── 测试方案.md
│   └── 测试结果.md
├── wheel/
│   ├── config/
│   │   ├── 测试方案.md
│   │   └── 测试结果.md
│   ├── storage/
│   │   ├── 测试方案.md
│   │   └── 测试结果.md
│   ├── scheduler/
│   │   ├── 测试方案.md
│   │   └── 测试结果.md
│   ├── llm/
│   │   ├── 测试方案.md
│   │   └── 测试结果.md
│   ├── ws_client/
│   │   ├── 测试方案.md
│   │   └── 测试结果.md
│   └── browser/
│       ├── 测试方案.md
│       └── 测试结果.md
└── core/
    └── adapters/
        ├── 测试方案.md
        └── 测试结果.md
```

### 测试方案.md 模板

```markdown
# {模块名} 测试方案

## 1. 被测目标
| 源文件 | 类型 | 测试文件 |
|--------|------|---------|
| ... | UT/TA | test_{module}_ut.cpp / _ta.cpp |

## 2. 测试点清单
### {类名/方法组}
- [ ] 场景1 — 描述

## 3. 公共依赖
- fixture: xxx — 用途

## 4. 运行命令
./build.sh --test
# 或
dist/bin/test_{module}_ta
```

### 测试结果.md 模板

```markdown
# {模块名} 测试结果

## 1. 汇总
| 总数 | 通过 | 失败 | 退出码 |
|------|------|------|--------|
| N | N | 0 | 0 |

## 2. 用例明细
| 用例名 | 输入 | 预期 | 状态 |
|--------|------|------|------|
| test_xxx | ... | ... | ✅ |
```

---

*文档版本：v2.0 | 最后更新：2026-05-02*
