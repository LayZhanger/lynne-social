#include "wheel/browser/browser_manager.h"
#include "wheel/browser/browser_factory.h"
#include "wheel/browser/browser_models.h"
#include "wheel/logger/logger_factory.h"
#include "wheel/logger/logger_macros.h"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
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
    passed = 0; failed = 0;
}

static bool test_chrome_available() {
    return system("which google-chrome >/dev/null 2>&1") == 0 ||
           system("which google-chrome-stable >/dev/null 2>&1") == 0 ||
           system("which chromium >/dev/null 2>&1") == 0 ||
           system("which chromium-browser >/dev/null 2>&1") == 0;
}

static void pump(BrowserManager* b, std::atomic<bool>& done, int max_ms = 7500) {
    auto t0 = std::chrono::steady_clock::now();
    while (!done) { b->step(); usleep(5000); if (std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count() >= max_ms) break; }
    if (!done) printf("  [WARN] pump timeout (%dms)\n", max_ms);
}

static bool wait_started(BrowserManager* b) {
    std::atomic<bool> ok{false};
    auto t0 = std::chrono::steady_clock::now();
    while (!ok) { b->step(); usleep(5000); ok = b->health_check(); if (std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count() >= 3000) break; }
    return ok;
}

// 动态创建 div + link 辅助
#define SETUP_CTX done, ctx
#define EVAL(js, ...) ctx->evaluate(js, __VA_ARGS__)

int main() {
    if (!test_chrome_available()) { printf("SKIP: no Chrome\n"); return 0; }
    g_logger_ptr = LoggerFactory().create({"WARN"});
    g_logger_ptr->start();

    auto* b = BrowserFactory().create(BrowserConfig{});
    b->start(); CHECK_TRUE(wait_started(b), "browser started");

    // ========== 导航到空白页 + 搭建测试 DOM ==========
    {
        std::atomic<bool> done{false};
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->navigate("about:blank", [&, ctx]() {
                ctx->evaluate(R"JS(
                    (()=>{
                        document.body.innerHTML=`
                            <input id='t1' value=''>
                            <input id='t2' value='hello'>
                            <button id='b1'>clickme</button>
                            <a id='l1' href='about:blank#test'>link</a>
                            <div id='d1' style='width:2000px;height:2000px;background:linear-gradient(red,blue)'></div>
                        `;
                        document.querySelector('#b1').onclick=function(){
                            this.dataset.clicked='yes';
                        };
                        var h=document.querySelector('#d1');
                        h.addEventListener('mouseover',function(){h.dataset.hovered='yes';});
                        return true;
                    })()
                )JS", [&](nlohmann::json) { done = true; }, [&](const std::string&) { done = true; });
            }, [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done, 200);
    }

    // ============================================================
    // 1. 错误路径 — 不存在的元素
    // ============================================================
    {
        std::atomic<bool> done{false};
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->click("#nonexistent",
                [&]() { done = true; },
                [&](const std::string&) { CHECK_TRUE(true, "click missing → on_error"); done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);

        done = false;
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->type("#nonexistent", "x",
                [&]() { done = true; },
                [&](const std::string&) { CHECK_TRUE(true, "type missing → on_error"); done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);

        done = false;
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->hover("#nonexistent",
                [&]() { done = true; },
                [&](const std::string&) { CHECK_TRUE(true, "hover missing → on_error"); done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);
    }
    report("ErrorPaths");

    // ============================================================
    // 2. type 特殊字符
    // ============================================================
    {
        std::atomic<bool> done{false};
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->type("#t1", "it's \"ok\" \\fine",
                [&]() {
                    // 验证
                    ctx->evaluate("document.querySelector('#t1').value",
                        [&](nlohmann::json r) {
                            auto v = r.contains("value") && !r["value"].is_null() ? r["value"].get<std::string>() : "";
                            CHECK_TRUE(v == "it's \"ok\" \\fine", "type: special chars preserved");
                            done = true;
                        },
                        [&](const std::string&) { done = true; });
                },
                [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);

        // 中文 + emoji
        done = false;
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->type("#t1", "你好 👋 test",
                [&]() {
                    ctx->evaluate("document.querySelector('#t1').value",
                        [&](nlohmann::json r) {
                            auto v = r.contains("value") && !r["value"].is_null() ? r["value"].get<std::string>() : "";
                            CHECK_TRUE(v.find("你好") != std::string::npos, "type: CJK chars");
                            done = true;
                        },
                        [&](const std::string&) { done = true; });
                },
                [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);
    }
    report("TypeSpecialChars");

    // ============================================================
    // 3. click 按钮 → 验证 DOM 事件触发
    // ============================================================
    {
        std::atomic<bool> done{false};
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->click("#b1",
                [&]() {
                    ctx->evaluate("document.querySelector('#b1').dataset.clicked",
                        [&](nlohmann::json r) {
                            auto v = r.contains("value") && !r["value"].is_null() ? r["value"].get<std::string>() : "";
                            CHECK_TRUE(v == "yes", "click: button onclick fired");
                            done = true;
                        },
                        [&](const std::string&) { done = true; });
                },
                [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);
    }
    report("ClickButtonEvent");

    // ============================================================
    // 4. hover → 验证 mouseover 事件
    // ============================================================
    {
        std::atomic<bool> done{false};
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->hover("#d1",
                [&]() {
                    ctx->evaluate("document.querySelector('#d1').dataset.hovered",
                        [&](nlohmann::json r) {
                            auto v = r.contains("value") && !r["value"].is_null() ? r["value"].get<std::string>() : "";
                            CHECK_TRUE(v == "yes", "hover: mouseover fired");
                            done = true;
                        },
                        [&](const std::string&) { done = true; });
                },
                [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);
    }
    report("HoverEvent");

    // ============================================================
    // 5. exists — 复杂选择器
    // ============================================================
    {
        std::atomic<bool> done{false};
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->exists("div, span", [&](bool f) { CHECK_TRUE(f, "exists: div,span→true"); done = true; },
                [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);

        done = false;
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->exists("input#t1",
                [&](bool f) {
                    CHECK_TRUE(f, "exists: input#t1→true");
                    done = true;
                },
                [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);

        done = false;
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->exists("div#d1",
                [&](bool f) {
                    CHECK_TRUE(f, "exists: div#d1→true");
                    done = true;
                },
                [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);
    }
    report("ExistsComplex");

    // ============================================================
    // 6. scroll 多方向
    // ============================================================
    {
        // 先滚动到 0,0
        std::atomic<bool> done{false};
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->evaluate("window.scrollTo(0,0)", [&](nlohmann::json) { done = true; }, [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);

        // 垂直
        done = false;
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->scroll(0, 300, [&]() { done = true; }, [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);

        double scrollY = 0;
        done = false;
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->evaluate("window.pageYOffset", [&](nlohmann::json r) {
                if (r.contains("value") && !r["value"].is_null()) scrollY = r["value"].get<double>(); done = true;
            }, [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);
        CHECK_TRUE(scrollY > 100, "scroll vertical: > 100");

        // 水平
        done = false;
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->scroll(200, 0, [&]() { done = true; }, [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);

        double scrollX = 0;
        done = false;
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->evaluate("window.pageXOffset", [&](nlohmann::json r) {
                if (r.contains("value") && !r["value"].is_null()) scrollX = r["value"].get<double>(); done = true;
            }, [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);
        CHECK_TRUE(scrollX > 50, "scroll horizontal: > 50");
    }
    report("ScrollMultiDir");

    // ============================================================
    // 7. press_key Tab
    // ============================================================
    {
        std::atomic<bool> done{false};
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->press_key("Tab",
                [&]() { CHECK_TRUE(true, "press_key: Tab sent"); done = true; },
                [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);
    }
    report("PressKeyTab");

    // ============================================================
    // 8. wait_for_selector — 立即找到 + 立即超时
    // ============================================================
    {
        std::atomic<bool> done{false};
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->wait_for_selector("#t1", 5000,
                [&]() { CHECK_TRUE(true, "wait: immediate find"); done = true; },
                [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done, 30);

        done = false;
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->wait_for_selector("#impossible__id__xyz", 2000,
                [&]() { done = true; },
                [&](const std::string&) { CHECK_TRUE(true, "wait: timeout on missing"); done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done, 200);
    }
    report("WaitForSelectorEdge");

    // ============================================================
    // 9. screenshot /tmp/lynne_edge.png
    // ============================================================
    {
        std::string shot = "/tmp/lynne_edge.png";
        std::error_code ec;
        std::filesystem::remove(shot, ec);
        std::atomic<bool> done{false};
        bool ok = false;
        b->get_context("edge", [&](BrowserContext* ctx) {
            ctx->screenshot(shot,
                [&]() { ok = true; done = true; },
                [&](const std::string&) { done = true; });
        }, [&](const std::string&) { done = true; });
        pump(b, done);
        CHECK_TRUE(ok, "screenshot: callback");
        bool ex = std::filesystem::exists(shot);
        CHECK_TRUE(ex, "screenshot: file exists");
        if (ex) { auto sz = std::filesystem::file_size(shot); CHECK_TRUE(sz > 1000, "screenshot: > 1KB"); }
        std::filesystem::remove(shot, ec);
    }
    report("Screenshot");

    b->stop(); delete b;
    printf("\n=== ALL TESTS DONE ===\n");
    return 0;
}
