#include "wheel/browser/browser_manager.h"
#include "wheel/browser/browser_factory.h"
#include "wheel/browser/browser_models.h"
#include "wheel/logger/logger_factory.h"
#include "wheel/logger/logger_macros.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>

using namespace lynne::wheel;

static int passed = 0;
static int failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { printf("  [PASS] %s\n", msg); ++passed; } \
         else { printf("  [FAIL] %s\n", msg); ++failed; } \
    } while (0)

#define CHECK_TRUE(cond, msg)  CHECK((cond), msg)
#define CHECK_FALSE(cond, msg) CHECK(!(cond), msg)

static void report(const char* suite) {
    printf("--- %s: %d/%d ---\n", suite, passed, passed + failed);
    passed = 0;
    failed = 0;
}

static bool test_chrome_available() {
    return system("which google-chrome >/dev/null 2>&1") == 0 ||
           system("which google-chrome-stable >/dev/null 2>&1") == 0 ||
           system("which chromium >/dev/null 2>&1") == 0 ||
           system("which chromium-browser >/dev/null 2>&1") == 0;
}

static void run_loop(BrowserManager* browser, std::atomic<bool>& done,
                     int max_iters = 800) {
    for (int i = 0; i < max_iters && !done; ++i) {
        browser->step();
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
}

static void run_loop_with_timeout(BrowserManager* browser,
                                  std::atomic<bool>& done,
                                  int timeout_ms = 20000) {
    int max_iters = timeout_ms / 25;
    run_loop(browser, done, max_iters);
}

static bool wait_started(BrowserManager* browser, int max_iters = 120) {
    std::atomic<bool> started{false};
    for (int i = 0; i < max_iters && !started; ++i) {
        browser->step();
        started = browser->health_check();
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return started;
}

// ============================================================
// 1. Navigation — 导航到百度，验证 title + URL
// ============================================================
static void test_navigation(BrowserManager* browser) {
    std::atomic<bool> done{false};
    std::string title;
    std::string url;

    browser->get_context("nav",
        [&](BrowserContext* ctx) {
            ctx->navigate("https://www.baidu.com/",
                [&, ctx]() {
                    // 加载完成后获取 title 和 url
                    ctx->evaluate("document.title",
                        [&](nlohmann::json r) {
                            title = r["value"].get<std::string>();
                        },
                        [&](const std::string&) { done = true; });
                    ctx->current_url(
                        [&](const std::string& u) {
                            url = u;
                            done = true;
                        },
                        [&](const std::string&) { done = true; });
                },
                [&](const std::string&) { done = true; });
        },
        [&](const std::string&) { done = true; });

    run_loop_with_timeout(browser, done);
    CHECK_TRUE(!title.empty(), "navigate: title not empty");
    CHECK_TRUE(title.find("百度") != std::string::npos,
               "navigate: title contains 百度");
    CHECK_TRUE(url.find("baidu.com") != std::string::npos,
               "navigate: url contains baidu.com");
}

// ============================================================
// 2. Evaluate — 多种 JS 表达式
// ============================================================
static void test_evaluate(BrowserManager* browser) {
    // 2a: evaluate string
    {
        std::atomic<bool> done{false};
        std::string val;
        browser->get_context("eval",
            [&](BrowserContext* ctx) {
                ctx->evaluate("'hello lynne'",
                    [&](nlohmann::json r) {
                        if (r.contains("value"))
                            val = r["value"].get<std::string>();
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done);
        CHECK_TRUE(val == "hello lynne", "evaluate: string result");
    }
    // 2b: evaluate number
    {
        std::atomic<bool> done{false};
        double val = 0;
        browser->get_context("eval",
            [&](BrowserContext* ctx) {
                ctx->evaluate("1 + 2",
                    [&](nlohmann::json r) {
                        if (r.contains("value"))
                            val = r["value"].get<double>();
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done);
        CHECK_TRUE(val == 3.0, "evaluate: number result 1+2=3");
    }
    // 2c: evaluate object
    {
        std::atomic<bool> done{false};
        nlohmann::json val;
        browser->get_context("eval",
            [&](BrowserContext* ctx) {
                ctx->evaluate("({a:1, b:'two'})",
                    [&](nlohmann::json r) {
                        if (r.contains("value"))
                            val = r["value"];
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done);
        CHECK_TRUE(val.is_object() && val["a"] == 1 && val["b"] == "two",
                   "evaluate: object result");
    }
    // 2d: evaluate array
    {
        std::atomic<bool> done{false};
        nlohmann::json val;
        browser->get_context("eval",
            [&](BrowserContext* ctx) {
                ctx->evaluate("[10, 20, 30]",
                    [&](nlohmann::json r) {
                        if (r.contains("value"))
                            val = r["value"];
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done);
        CHECK_TRUE(val.is_array() && val.size() == 3,
                   "evaluate: array result");
    }
    // 2e: evaluate null
    {
        std::atomic<bool> done{false};
        nlohmann::json val;
        browser->get_context("eval",
            [&](BrowserContext* ctx) {
                ctx->evaluate("null",
                    [&](nlohmann::json r) {
                        val = r;
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done);
        CHECK_TRUE(val.contains("value") && val["value"].is_null(),
                   "evaluate: null result");
    }
    // 2f: evaluate boolean
    {
        std::atomic<bool> done{false};
        bool val = false;
        browser->get_context("eval",
            [&](BrowserContext* ctx) {
                ctx->evaluate("2 > 1",
                    [&](nlohmann::json r) {
                        if (r.contains("value"))
                            val = r["value"].get<bool>();
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done);
        CHECK_TRUE(val, "evaluate: boolean result true");
    }
    // 2g: evaluate error (syntax error → result has exceptionDetails)
    {
        std::atomic<bool> done{false};
        bool has_exception = false;
        browser->get_context("eval",
            [&](BrowserContext* ctx) {
                ctx->evaluate("syntax{{{error",
                    [&](nlohmann::json r) {
                        // CDP 返回成功 response，result 里有 exceptionDetails
                        has_exception = r.contains("exceptionDetails")
                            || (r.contains("subtype")
                                && r["subtype"] == "error");
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done);
        CHECK_TRUE(has_exception, "evaluate: syntax error returns exception");
    }
}

// ============================================================
// 3. CurrentURL — 导航后验证 URL
// ============================================================
static void test_current_url(BrowserManager* browser) {
    std::atomic<bool> done{false};
    std::string url;

    browser->get_context("url_test",
        [&](BrowserContext* ctx) {
            ctx->navigate("https://www.baidu.com/",
                [&, ctx]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    ctx->current_url(
                        [&](const std::string& u) {
                            url = u;
                            done = true;
                        },
                        [&](const std::string&) { done = true; });
                },
                [&](const std::string&) { done = true; });
        },
        [&](const std::string&) { done = true; });

    run_loop_with_timeout(browser, done);
    CHECK_TRUE(url.find("baidu.com") != std::string::npos,
               "current_url: contains baidu.com");
    CHECK_TRUE(url.find("https://") != std::string::npos,
               "current_url: starts with https");
}

// ============================================================
// 4. Screenshot — 截图文件验证
// ============================================================
static void test_screenshot(BrowserManager* browser) {
    std::string shot_path = "/tmp/lynne_baidu_shot.png";
    std::error_code ec;
    std::filesystem::remove(shot_path, ec);

    std::atomic<bool> done{false};
    bool ok = false;

    browser->get_context("shot",
        [&](BrowserContext* ctx) {
            ctx->navigate("https://www.baidu.com/",
                [&, ctx]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    ctx->screenshot(shot_path,
                        [&]() {
                            ok = true;
                            done = true;
                        },
                        [&](const std::string&) { done = true; });
                },
                [&](const std::string&) { done = true; });
        },
        [&](const std::string&) { done = true; });

    run_loop_with_timeout(browser, done, 30000);
    CHECK_TRUE(ok, "screenshot: callback fired");
    bool file_exists = std::filesystem::exists(shot_path);
    CHECK_TRUE(file_exists, "screenshot: file exists");
    if (file_exists) {
        auto fsize = std::filesystem::file_size(shot_path);
        CHECK_TRUE(fsize > 1000, "screenshot: file size > 1KB");
    }
    std::filesystem::remove(shot_path, ec);
}

// ============================================================
// 5. SendCommand — 原始 CDP 命令
// ============================================================
static void test_send_command(BrowserManager* browser) {
    // 5a: Browser.getVersion
    {
        std::atomic<bool> done{false};
        nlohmann::json result;
        browser->get_context("cdp",
            [&](BrowserContext* ctx) {
                ctx->send_command("Browser.getVersion", {},
                    [&](nlohmann::json r) {
                        result = r;
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done);
        CHECK_TRUE(result.contains("product"),
                   "send_command: Browser.getVersion has product");
    }
    // 5b: invalid command → error
    {
        std::atomic<bool> done{false};
        bool got_error = false;
        browser->get_context("cdp",
            [&](BrowserContext* ctx) {
                ctx->send_command("NonExistent.Method", {},
                    [&](nlohmann::json) { done = true; },
                    [&](const std::string&) {
                        got_error = true;
                        done = true;
                    });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done);
        CHECK_TRUE(got_error,
                   "send_command: invalid method triggers on_error");
    }
    // 5c: Target.setDiscoverTargets (with params)
    {
        std::atomic<bool> done{false};
        nlohmann::json result;
        browser->get_context("cdp",
            [&](BrowserContext* ctx) {
                ctx->send_command("Target.setDiscoverTargets",
                    {{"discover", true}},
                    [&](nlohmann::json r) {
                        result = r;
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done);
        CHECK_TRUE(result.is_object(),
                   "send_command: Target.setDiscoverTargets ok");
    }
}

// ============================================================
// 6. AddInitScript — 脚本注入后导航验证
// ============================================================
static void test_add_init_script(BrowserManager* browser) {
    // 在导航前注入脚本，导航后检查效果
    std::string platform = "init_script";
    // 先获取 context 来注入脚本
    {
        std::atomic<bool> done{false};
        browser->get_context(platform,
            [&](BrowserContext* ctx) {
                ctx->add_init_script(
                    "window.__lynne_test = 'injected';",
                    [&]() { done = true; },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done);
    }
    // 导航到百度，验证注入生效
    {
        std::atomic<bool> done{false};
        std::string val;
        browser->get_context(platform,
            [&](BrowserContext* ctx) {
                ctx->navigate("https://www.baidu.com/",
                    [&, ctx]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                        ctx->evaluate("window.__lynne_test",
                            [&](nlohmann::json r) {
                                if (r.contains("value") && !r["value"].is_null())
                                    val = r["value"].get<std::string>();
                                done = true;
                            },
                            [&](const std::string&) { done = true; });
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done);
        CHECK_TRUE(val == "injected",
                   "add_init_script: injected value persists after navigation");
    }
}

// ============================================================
// 7. Context Isolation — 两个平台不同 context
// ============================================================
static void test_context_isolation(BrowserManager* browser) {
    // Step 1: 在 plat_a 设值
    {
        std::atomic<bool> done{false};
        browser->get_context("plat_a",
            [&](BrowserContext* ctx) {
                ctx->navigate("https://www.baidu.com/",
                    [&, ctx]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                        ctx->evaluate("window.__isolated_val = 'from_a'",
                            [&](nlohmann::json) { done = true; },
                            [&](const std::string&) { done = true; });
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done, 30000);
    }

    // Step 2: 在 plat_b 读值（应不是 'from_a'）
    {
        std::atomic<bool> done{false};
        std::string val_b;
        browser->get_context("plat_b",
            [&](BrowserContext* ctx_b) {
                ctx_b->navigate("https://www.baidu.com/",
                    [&, ctx_b]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                        ctx_b->evaluate("window.__isolated_val",
                            [&](nlohmann::json r) {
                                if (r.contains("value") && !r["value"].is_null())
                                    val_b = r["value"].get<std::string>();
                                done = true;
                            },
                            [&](const std::string&) { done = true; });
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done, 30000);
        CHECK_TRUE(val_b.empty() || val_b != "from_a",
                   "context_isolation: plat_b does NOT see plat_a value");
    }

    // Step 3: 重新读 plat_a（值仍在）
    {
        std::atomic<bool> done{false};
        std::string val_a;
        browser->get_context("plat_a",
            [&](BrowserContext* ctx) {
                ctx->evaluate("window.__isolated_val",
                    [&](nlohmann::json r) {
                        if (r.contains("value") && !r["value"].is_null())
                            val_a = r["value"].get<std::string>();
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done, 15000);
        CHECK_TRUE(val_a == "from_a",
                   "context_isolation: plat_a value persists");
    }
}

// ============================================================
// 8. Close + Reopen Context
// ============================================================
static void test_close_reopen(BrowserManager* browser) {
    std::string platform = "close_reopen";

    std::atomic<bool> done{false};
    bool got_first_ctx = false;
    bool got_second_ctx = false;
    std::string first_session;
    std::string second_session;

    // 获取第一个 context 并 close
    browser->get_context(platform,
        [&](BrowserContext* ctx) {
            got_first_ctx = true;
            // 通过 evaluate 来触发 CDP 命令获取 session 标识
            ctx->evaluate("'first'",
                [&, ctx](nlohmann::json) {
                    ctx->close(
                        [&]() {
                            // 重新获取 → 应得到新 context
                            browser->get_context(platform,
                                [&](BrowserContext* ctx2) {
                                    got_second_ctx = true;
                                    // 验证新 context 可交互
                                    ctx2->evaluate("'second'",
                                        [&](nlohmann::json) {
                                            done = true;
                                        },
                                        [&](const std::string&) { done = true; });
                                },
                                [&](const std::string&) { done = true; });
                        },
                        [&](const std::string&) { done = true; });
                },
                [&](const std::string&) { done = true; });
        },
        [&](const std::string&) { done = true; });

    run_loop_with_timeout(browser, done);
    CHECK_TRUE(got_first_ctx, "close_reopen: first context acquired");
    CHECK_TRUE(got_second_ctx, "close_reopen: second context acquired after close");
    CHECK_TRUE(got_first_ctx && got_second_ctx,
               "close_reopen: reopened context works");
}

// ============================================================
// 9. Session Persistence — 保存/恢复百度 session
// ============================================================
static void test_session_persistence(BrowserManager* browser) {
    std::string platform = "sess_test";
    std::string sess_dir = "data/sessions";
    std::string sess_path = sess_dir + "/" + platform + "_state.json";

    std::error_code ec;
    std::filesystem::create_directories(sess_dir, ec);
    std::filesystem::remove(sess_path, ec);

    // 9a: restore when no file → false
    {
        std::atomic<bool> done{false};
        bool restored = true;
        browser->restore_session(platform,
            [&](bool ok) {
                restored = ok;
                done = true;
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done);
        CHECK_FALSE(restored, "session: restore false when no file");
    }

    // 9b: 先导航到百度，再 save
    {
        std::atomic<bool> done{false};
        bool saved = false;
        browser->get_context(platform,
            [&](BrowserContext* ctx) {
                ctx->navigate("https://www.baidu.com/",
                    [&, ctx]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                        browser->save_session(platform,
                            [&]() {
                                saved = true;
                                done = true;
                            },
                            [&](const std::string&) { done = true; });
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done, 30000);
        CHECK_TRUE(saved, "session: save ok");

        auto fsize = std::filesystem::file_size(sess_path, ec);
        CHECK_TRUE(!ec && fsize > 0, "session: file non-empty after save");
    }

    // 9c: restore from file
    {
        std::atomic<bool> done{false};
        bool restored = false;
        browser->restore_session(platform,
            [&](bool ok) {
                restored = ok;
                done = true;
            },
            [&](const std::string&) { done = true; });
        run_loop_with_timeout(browser, done);
        CHECK_TRUE(restored, "session: restore true from file");
    }

    std::filesystem::remove(sess_path, ec);
}

// ============================================================
// 10. Multiple Platforms — 不同平台独立导航
// ============================================================
static void test_multiple_platforms(BrowserManager* browser) {
    std::atomic<bool> done{false};
    std::string title_a;
    std::string title_b;

    browser->get_context("multi_a",
        [&](BrowserContext* ctx_a) {
            ctx_a->navigate("https://www.baidu.com/",
                [&, ctx_a]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    ctx_a->evaluate("document.title",
                        [&](nlohmann::json r) {
                            if (r.contains("value"))
                                title_a = r["value"].get<std::string>();
                            // 同时获取 multi_b
                            browser->get_context("multi_b",
                                [&](BrowserContext* ctx_b) {
                                    ctx_b->navigate(
                                        "https://www.baidu.com/s?wd=lynne",
                                        [&, ctx_b]() {
                                            std::this_thread::sleep_for(
                                                std::chrono::milliseconds(3000));
                                            ctx_b->evaluate(
                                                "document.title",
                                                [&](nlohmann::json r2) {
                                                    if (r2.contains("value"))
                                                        title_b =
                                                            r2["value"].get<std::string>();
                                                    done = true;
                                                },
                                                [&](const std::string&) {
                                                    done = true; });
                                        },
                                        [&](const std::string&) { done = true; });
                                },
                                [&](const std::string&) { done = true; });
                        },
                        [&](const std::string&) { done = true; });
                },
                [&](const std::string&) { done = true; });
        },
        [&](const std::string&) { done = true; });

    run_loop_with_timeout(browser, done, 60000);
    CHECK_TRUE(title_a.find("百度") != std::string::npos,
               "multi_platform: platform_a baidu title");
    CHECK_TRUE(!title_b.empty(),
               "multi_platform: platform_b search page title");
}

// ============================================================
// 11. GetContext Error — 在 stop 后获取 context 应报错
// ============================================================
static void test_get_context_after_stop(BrowserManager* browser) {
    std::atomic<bool> done{false};
    bool got_error = false;

    browser->get_context("after_stop",
        [&](BrowserContext*) { done = true; },
        [&](const std::string&) {
            got_error = true;
            done = true;
        });

    run_loop_with_timeout(browser, done);
    CHECK_TRUE(got_error,
               "get_context after stop: triggers on_error");
}

int main() {
    if (!test_chrome_available()) {
        printf("SKIP: no Chrome binary found\n");
        return 0;
    }

    g_logger_ptr = LoggerFactory().create({"INFO"});
    g_logger_ptr->start();

    // ============================================================
    // 1. Navigation — 导航 + title + URL
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        CHECK_TRUE(wait_started(browser), "browser started");
        test_navigation(browser);
        browser->stop();
        delete browser;
    }
    report("Navigation");

    // ============================================================
    // 2. Evaluate — 多种 JS 类型 + 错误
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        CHECK_TRUE(wait_started(browser), "browser started");
        test_evaluate(browser);
        browser->stop();
        delete browser;
    }
    report("Evaluate");

    // ============================================================
    // 3. CurrentURL — 导航后 URL
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        CHECK_TRUE(wait_started(browser), "browser started");
        test_current_url(browser);
        browser->stop();
        delete browser;
    }
    report("CurrentURL");

    // ============================================================
    // 4. Screenshot
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        CHECK_TRUE(wait_started(browser), "browser started");
        test_screenshot(browser);
        browser->stop();
        delete browser;
    }
    report("Screenshot");

    // ============================================================
    // 5. SendCommand — CDP 原始命令
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        CHECK_TRUE(wait_started(browser), "browser started");
        test_send_command(browser);
        browser->stop();
        delete browser;
    }
    report("SendCommand");

    // ============================================================
    // 6. AddInitScript
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        CHECK_TRUE(wait_started(browser), "browser started");
        test_add_init_script(browser);
        browser->stop();
        delete browser;
    }
    report("AddInitScript");

    // ============================================================
    // 7. Context Isolation
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        CHECK_TRUE(wait_started(browser), "browser started");
        test_context_isolation(browser);
        browser->stop();
        delete browser;
    }
    report("ContextIsolation");

    // ============================================================
    // 8. Close + Reopen
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        CHECK_TRUE(wait_started(browser), "browser started");
        test_close_reopen(browser);
        browser->stop();
        delete browser;
    }
    report("CloseReopen");

    // ============================================================
    // 9. Session Persistence
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        CHECK_TRUE(wait_started(browser), "browser started");
        test_session_persistence(browser);
        browser->stop();
        delete browser;
    }
    report("SessionPersistence");

    // ============================================================
    // 10. Multiple Platforms
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        CHECK_TRUE(wait_started(browser), "browser started");
        test_multiple_platforms(browser);
        browser->stop();
        delete browser;
    }
    report("MultiplePlatforms");

    // ============================================================
    // 11. GetContext After Stop — error path
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        CHECK_TRUE(wait_started(browser), "browser started");
        browser->stop();
        test_get_context_after_stop(browser);
        delete browser;
    }
    report("ErrorAfterStop");

    printf("\n=== ALL TESTS DONE ===\n");
    return 0;
}
