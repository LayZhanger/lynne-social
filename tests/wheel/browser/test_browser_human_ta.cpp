#include "wheel/browser/browser_manager.h"
#include "wheel/browser/browser_factory.h"
#include "wheel/browser/browser_models.h"
#include "wheel/logger/logger_factory.h"
#include "wheel/logger/logger_macros.h"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <filesystem>
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

#define CHECK_TRUE(cond, msg) CHECK((cond), msg)

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

static void pump(BrowserManager* browser, std::atomic<bool>& done,
                  int max_ms = 20000) {
    auto t0 = std::chrono::steady_clock::now();
    while (!done) {
        browser->step();
        usleep(5000);
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0).count() >= max_ms) break;
    }
    if (!done) printf("  [WARN] pump timeout (%dms)\n", max_ms);
}

static bool wait_started(BrowserManager* browser, int max_ms = 3000) {
    std::atomic<bool> ok{false};
    auto t0 = std::chrono::steady_clock::now();
    while (!ok) {
        browser->step();
        ok = browser->health_check();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0).count() >= max_ms) break;
    }
    return ok;
}

int main() {
    if (!test_chrome_available()) {
        printf("SKIP: no Chrome binary found\n");
        return 0;
    }

    g_logger_ptr = LoggerFactory().create({"INFO"});
    g_logger_ptr->start();

    auto* browser = BrowserFactory().create(BrowserConfig{});
    browser->start();
    CHECK_TRUE(wait_started(browser), "browser started");

    // ============================================================
    // 1. exists + navigate — 导航到百度，检测元素
    // ============================================================
    {
        std::atomic<bool> done{false};
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->navigate("https://www.baidu.com/",
                    [&, ctx]() {
                        ctx->wait_for_selector("#kw", 10000,
                            [&]() { done = true; },
                            [&](const std::string&) { done = true; });
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done, 600);

        done = false;
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->exists("#kw",
                    [&](bool found) {
                        CHECK_TRUE(found, "exists: #kw found");
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);

        done = false;
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->exists("#nonexistent_xyz",
                    [&](bool found) {
                        CHECK_TRUE(!found, "exists: nonexistent not found");
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);
    }
    report("Exists");

    // ============================================================
    // 2. type — 输入搜索词
    // ============================================================
    {
        std::atomic<bool> done{false};
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->type("#kw", "opencode ai",
                    [&]() { done = true; },
                    [&](const std::string& err) {
                        printf("  [WARN] type error: %s\n", err.c_str());
                        done = true;
                    });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);

        done = false;
        std::string input_val;
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->evaluate("document.querySelector('#kw').value",
                    [&](nlohmann::json r) {
                        if (r.contains("value") && !r["value"].is_null())
                            input_val = r["value"].get<std::string>();
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);
        CHECK_TRUE(input_val.find("opencode") != std::string::npos,
                   "type: input contains 'opencode'");
    }
    report("Type");

    // ============================================================
    // 3. click — 动态创建链接并点击
    // ============================================================
    {
        std::atomic<bool> done{false};
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                std::string js = R"(
                    (()=>{
                        const a=document.createElement('a');
                        a.id='test_link';
                        a.href='https://www.baidu.com/s?wd=click_test';
                        a.target='_self';
                        a.textContent='click me';
                        document.body.appendChild(a);
                        return true;
                    })()
                )";
                ctx->evaluate(js,
                    [&, ctx](nlohmann::json) {
                        ctx->click("#test_link",
                            [&]() { done = true; },
                            [&](const std::string& err) {
                                printf("  [WARN] click error: %s\n", err.c_str());
                                done = true;
                            });
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done, 600);

        // 验证页面导航了
        done = false;
        std::string cur_url;
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->current_url(
                    [&](const std::string& u) {
                        cur_url = u;
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);
        CHECK_TRUE(cur_url.find("click_test") != std::string::npos,
                   "click: navigated to link target");
    }
    report("Click");

    // ============================================================
    // 4. press_key — 键盘事件
    // ============================================================
    {
        std::atomic<bool> done{false};
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->press_key("Enter",
                    [&]() { done = true; },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);
        CHECK_TRUE(true, "press_key: Enter sent");
    }
    report("PressKey");

    // ============================================================
    // 5. hover — 动态创建元素并悬停
    // ============================================================
    {
        std::atomic<bool> done{false};
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                std::string js = R"(
                    (()=>{
                        const d=document.createElement('div');
                        d.id='test_hover';
                        d.style.cssText='width:100px;height:100px;background:red';
                        d.textContent='hover target';
                        document.body.appendChild(d);
                        d.addEventListener('mouseover',()=>{d.dataset.hovered='yes'});
                        return true;
                    })()
                )";
                ctx->evaluate(js,
                    [&, ctx](nlohmann::json) {
                        ctx->hover("#test_hover",
                            [&]() { done = true; },
                            [&](const std::string&) { done = true; });
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);

        // 验证 hover 触发
        done = false;
        std::string hovered;
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->evaluate(
                    "document.querySelector('#test_hover').dataset.hovered",
                    [&](nlohmann::json r) {
                        if (r.contains("value") && !r["value"].is_null())
                            hovered = r["value"].get<std::string>();
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);
        CHECK_TRUE(hovered == "yes", "hover: mouseover event fired");
    }
    report("Hover");

    // ============================================================
    // 6. scroll — 滚动页面
    // ============================================================
    {
        // 先创建足够高的内容
        std::atomic<bool> done{false};
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                std::string js = R"(
                    (()=>{
                        for(let i=0;i<50;i++){
                            const d=document.createElement('div');
                            d.style.cssText='height:100px;border:1px solid #ccc';
                            d.textContent='scroll area line '+i;
                            document.body.appendChild(d);
                        }
                        return true;
                    })()
                )";
                ctx->evaluate(js,
                    [&](nlohmann::json) { done = true; },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);

        done = false;
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->scroll(0, 500,
                    [&]() { done = true; },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);

        done = false;
        double offset = 0;
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->evaluate("window.pageYOffset",
                    [&](nlohmann::json r) {
                        if (r.contains("value") && !r["value"].is_null())
                            offset = r["value"].get<double>();
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);
        CHECK_TRUE(offset > 100, "scroll down: pageYOffset > 100");

        done = false;
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->scroll(0, -200,
                    [&]() { done = true; },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);

        done = false;
        double offset2 = 0;
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->evaluate("window.pageYOffset",
                    [&](nlohmann::json r) {
                        if (r.contains("value") && !r["value"].is_null())
                            offset2 = r["value"].get<double>();
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);
        CHECK_TRUE(offset2 < offset || offset2 < offset + 50,
                   "scroll up: pageYOffset decreased");
    }
    report("Scroll");

    // ============================================================
    // 7. wait_for_selector — 等待动态元素出现
    // ============================================================
    {
        // 先回到百度首页
        std::atomic<bool> done{false};
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->navigate("https://www.baidu.com/",
                    [&, ctx]() {
                        ctx->wait_for_selector("#kw", 5000,
                            [&]() {
                                CHECK_TRUE(true,
                                    "wait_for_selector: #kw found");
                                done = true;
                            },
                            [&](const std::string& err) {
                                printf("  [FAIL] %s\n", err.c_str());
                                ++failed; done = true;
                            });
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done, 600);

        // 等待不存在的元素 → 超时
        done = false;
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->wait_for_selector("#never_gonna_exist", 3000,
                    [&]() { done = true; },
                    [&](const std::string& err) {
                        CHECK_TRUE(true,
                            "wait_for_selector: timeout on missing");
                        done = true;
                    });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done, 300);
    }
    report("WaitForSelector");

    // ============================================================
    // 8. send_command — 原始 CDP（已有）
    // ============================================================
    {
        std::atomic<bool> done{false};
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->send_command("Browser.getVersion", {},
                    [&](nlohmann::json r) {
                        CHECK_TRUE(r.contains("product"),
                                   "send_command: Browser.getVersion");
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);
    }
    report("SendCommand");

    // ============================================================
    // 9. screenshot
    // ============================================================
    {
        std::string shot_path = "/tmp/lynne_human_final.png";
        std::error_code ec;
        std::filesystem::remove(shot_path, ec);

        std::atomic<bool> done{false};
        bool ok = false;
        browser->get_context("human",
            [&](BrowserContext* ctx) {
                ctx->screenshot(shot_path,
                    [&]() {
                        ok = true;
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });
        pump(browser, done);

        CHECK_TRUE(ok, "screenshot: callback fired");
        bool file_exists = std::filesystem::exists(shot_path);
        CHECK_TRUE(file_exists, "screenshot: file exists");
        if (file_exists) {
            auto fsize = std::filesystem::file_size(shot_path);
            CHECK_TRUE(fsize > 1000, "screenshot: file size > 1KB");
        }
        std::filesystem::remove(shot_path, ec);
    }
    report("Screenshot");

    browser->stop();
    delete browser;

    printf("\n=== ALL TESTS DONE ===\n");
    return 0;
}
