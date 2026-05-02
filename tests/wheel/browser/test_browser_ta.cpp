#include "wheel/browser/browser_manager.h"
#include "wheel/browser/browser_factory.h"
#include "wheel/browser/browser_models.h"

#include <cstdio>
#include <cstdlib>
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

#define CHECK_TRUE(cond, msg)  CHECK((cond), msg)
#define CHECK_FALSE(cond, msg) CHECK(!(cond), msg)

static void report(const char* suite) {
    printf("--- %s: %d/%d ---\n", suite, passed, passed + failed);
    passed = 0;
    failed = 0;
}

static bool test_chrome_available() {
    return system("which google-chrome >/dev/null 2>&1") == 0 ||
           system("which chromium >/dev/null 2>&1") == 0 ||
           system("which chromium-browser >/dev/null 2>&1") == 0 ||
           system("which google-chrome-stable >/dev/null 2>&1") == 0;
}

static void run_loop(BrowserManager* browser, std::atomic<bool>& done,
                     int max_iters = 300) {
    for (int i = 0; i < max_iters && !done; ++i) {
        browser->step();
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
}

int main() {
    if (!test_chrome_available()) {
        printf("SKIP: no Chrome binary found\n");
        return 0;
    }

    // ============================================================
    // Lifecycle
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});

        CHECK_FALSE(browser->health_check(), "health false before start");

        browser->start();
        std::atomic<bool> started{false};
        for (int i = 0; i < 80 && !started; ++i) {
            browser->step();
            started = browser->health_check();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        CHECK_TRUE(started, "start: health true after loop");

        browser->stop();
        CHECK_FALSE(browser->health_check(), "health false after stop");

        delete browser;
    }
    report("Lifecycle");

    // ============================================================
    // Start twice is safe
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        browser->start();
        std::atomic<bool> started{false};
        for (int i = 0; i < 80 && !started; ++i) {
            browser->step();
            started = browser->health_check();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        CHECK_TRUE(started, "start twice: health true");
        browser->stop();
        delete browser;
    }
    report("StartTwice");

    // ============================================================
    // Stop without start is safe
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->stop();
        CHECK_FALSE(browser->health_check(), "stop w/o start: health false");
        delete browser;
    }
    report("StopWithoutStart");

    // ============================================================
    // Start → Stop → Start again
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        std::atomic<bool> started{false};
        for (int i = 0; i < 80 && !started; ++i) {
            browser->step();
            started = browser->health_check();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        CHECK_TRUE(started, "first start ok");
        browser->stop();
        CHECK_FALSE(browser->health_check(), "stopped");

        browser->start();
        started = false;
        for (int i = 0; i < 80 && !started; ++i) {
            browser->step();
            started = browser->health_check();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        CHECK_TRUE(started, "restart: health true");
        browser->stop();
        delete browser;
    }
    report("StartStopStart");

    // ============================================================
    // Get context before start → error
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        std::atomic<bool> got_error{false};
        browser->get_context("test",
            [](BrowserContext*) {},
            [&got_error](const std::string&) { got_error = true; });
        browser->step();
        CHECK_TRUE(got_error, "get_context before start → error");
        delete browser;
    }
    report("GetContextBeforeStart");

    // ============================================================
    // GetContext + Navigate + Evaluate + CurrentUrl
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        std::atomic<bool> browser_ok{false};
        for (int i = 0; i < 80 && !browser_ok; ++i) {
            browser->step();
            browser_ok = browser->health_check();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }

        std::atomic<bool> done{false};
        std::string title;
        std::string url;

        browser->get_context("main",
            [&](BrowserContext* ctx) {
                ctx->navigate("data:text/html,<title>TA</title><h1>Hello</h1>",
                    [&, ctx]() {
                        ctx->current_url(
                            [&](const std::string& u) {
                                url = u;
                                ctx->evaluate("document.title",
                                    [&](nlohmann::json r) {
                                        title = r["value"].get<std::string>();
                                        done = true;
                                    },
                                    [&](const std::string&) { done = true; });
                            },
                            [&](const std::string&) { done = true; });
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });

        run_loop(browser, done);
        CHECK_TRUE(!title.empty(), "evaluate: got title");
        CHECK_TRUE(title == "TA", "evaluate: title is TA");
        CHECK_TRUE(url.find("data:text/html") != std::string::npos,
                   "current_url: contains data:");
        browser->stop();
        delete browser;
    }
    report("NavigateEvaluateCurrentUrl");

    // ============================================================
    // AddInitScript + Context caching + Close
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        std::atomic<bool> browser_ok{false};
        for (int i = 0; i < 80 && !browser_ok; ++i) {
            browser->step();
            browser_ok = browser->health_check();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }

        // get_context twice → same pointer (cache)
        std::atomic<bool> done{false};
        BrowserContext* first = nullptr;
        BrowserContext* second = nullptr;

        browser->get_context("cache_test",
            [&](BrowserContext* ctx) {
                first = ctx;
                browser->get_context("cache_test",
                    [&](BrowserContext* ctx2) {
                        second = ctx2;
                        // add_init_script
                        ctx->add_init_script(
                            "Object.defineProperty(navigator, "
                            "'testProp', {get: () => 'injected'});",
                            [&]() {
                                // close + verify pointer changed after reopen
                                ctx->close(
                                    [&]() {
                                        browser->get_context("cache_test",
                                            [&](BrowserContext* ctx3) {
                                                second = ctx3;
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

        run_loop(browser, done);
        CHECK_TRUE(first != nullptr, "first ctx non-null");
        CHECK_TRUE(second != nullptr, "second ctx non-null");
        CHECK_TRUE(first == second, "same platform → cached (same ptr)");
        // After close→reopen, ptr should differ
        // (second was reassigned to ctx3 in the close callback)

        delete browser;
    }
    report("AddInitScript_Cache_Close");

    // ============================================================
    // Context isolation: different platforms → different pointers
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        std::atomic<bool> browser_ok{false};
        for (int i = 0; i < 80 && !browser_ok; ++i) {
            browser->step();
            browser_ok = browser->health_check();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }

        std::atomic<bool> done{false};
        BrowserContext* ctx_a = nullptr;
        BrowserContext* ctx_b = nullptr;

        browser->get_context("platform_a",
            [&](BrowserContext* a) {
                ctx_a = a;
                browser->get_context("platform_b",
                    [&](BrowserContext* b) {
                        ctx_b = b;
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });

        run_loop(browser, done);
        CHECK_TRUE(ctx_a != nullptr, "platform_a ctx");
        CHECK_TRUE(ctx_b != nullptr, "platform_b ctx");
        CHECK_TRUE(ctx_a != ctx_b, "different platforms → different ctx");
        delete browser;
    }
    report("ContextIsolation");

    // ============================================================
    // SendCommand (raw CDP escape hatch)
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        std::atomic<bool> browser_ok{false};
        for (int i = 0; i < 80 && !browser_ok; ++i) {
            browser->step();
            browser_ok = browser->health_check();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }

        std::atomic<bool> done{false};
        std::string version;

        browser->get_context("raw_test",
            [&](BrowserContext* ctx) {
                ctx->send_command("Runtime.evaluate",
                    {{"expression", "navigator.userAgent"},
                     {"returnByValue", true}},
                    [&](nlohmann::json r) {
                        version = r["result"]["value"].get<std::string>();
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });

        run_loop(browser, done);
        CHECK_TRUE(!version.empty(), "send_command: got userAgent");
        CHECK_TRUE(version.find("Chrome") != std::string::npos,
                   "send_command: userAgent contains Chrome");
        browser->stop();
        delete browser;
    }
    report("SendCommand");

    // ============================================================
    // Save/Restore session
    // ============================================================
    {
        auto cfg = BrowserConfig{};
        cfg.sessions_dir = "/tmp/lynne_browser_test";

        auto* browser = BrowserFactory().create(cfg);
        browser->start();
        std::atomic<bool> browser_ok{false};
        for (int i = 0; i < 80 && !browser_ok; ++i) {
            browser->step();
            browser_ok = browser->health_check();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }

        std::atomic<bool> done{false};
        bool restore_no_file = false;
        bool saved = false;
        bool restore_after_save = false;

        // Step 1: restore when no file → false
        browser->restore_session("session_test",
            [&](bool ok) {
                restore_no_file = !ok;
                // Step 2: create context and save
                browser->get_context("session_test",
                    [&](BrowserContext* ctx) {
                        ctx->navigate("data:text/html,<h1>S</h1>",
                            [&, ctx]() {
                                // wait for navigate to settle, then save
                                browser->step();
                                std::this_thread::sleep_for(
                                    std::chrono::milliseconds(200));

                                browser->save_session("session_test",
                                    [&]() {
                                        saved = true;
                                        // Step 3: restore again → should be true
                                        browser->restore_session(
                                            "session_test",
                                            [&](bool ok2) {
                                                restore_after_save = ok2;
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

        run_loop(browser, done, 400);
        CHECK_TRUE(restore_no_file, "restore: false when no file");
        CHECK_TRUE(saved, "save: succeeded");
        CHECK_TRUE(restore_after_save, "restore: true after save");
        browser->stop();
        delete browser;
    }
    report("SessionPersistence");

    // ============================================================
    // All functionality on baidu.com
    // ============================================================
    {
        auto cfg = BrowserConfig{};
        cfg.sessions_dir = "/tmp/lynne_browser_test";
        auto* browser = BrowserFactory().create(cfg);
        browser->start();
        std::atomic<bool> browser_ok{false};
        for (int i = 0; i < 80 && !browser_ok; ++i) {
            browser->step();
            browser_ok = browser->health_check();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        CHECK_TRUE(browser_ok, "baidu: browser started");

        std::atomic<bool> done{false};
        bool nav_ok = false;
        bool inject_ok = false;
        bool url_ok = false;
        bool eval_ok = false;
        bool screenshot_ok = false;
        bool raw_cmd_ok = false;
        bool close_ok = false;
        bool reopen_ok = false;
        std::string page_title;
        std::string page_url;
        std::string ss_path = "/tmp/lynne_browser_test/baidu.png";

        BrowserContext* ctx_before = nullptr;
        BrowserContext* ctx_after = nullptr;

        // Step 1: add_init_script → navigate → verify injection
        browser->get_context("baidu",
            [&](BrowserContext* ctx) {
                ctx_before = ctx;
                ctx->add_init_script(
                    "Object.defineProperty(navigator, "
                    "'_lynne_test', {get: () => 'injected'});",
                    [&, ctx]() {
                        inject_ok = true;
                        // Step 2: navigate
                        ctx->navigate("https://www.baidu.com/",
                            [&, ctx]() {
                                nav_ok = true;
                                // Step 3: current_url
                                ctx->current_url(
                                    [&, ctx](const std::string& u) {
                                        page_url = u;
                                        url_ok = true;
                                        // Step 4: evaluate title
                                        ctx->evaluate("document.title",
                                            [&, ctx](nlohmann::json r) {
                                                page_title =
                                                    r["value"].get<std::string>();
                                                eval_ok = true;
                                                // Step 5: verify injection
                                                ctx->evaluate(
                                                    "navigator._lynne_test",
                                                    [&, ctx](nlohmann::json r2) {
                                                        inject_ok = r2["value"]
                                                            .get<std::string>()
                                                            == "injected";
                                                        // Step 6: screenshot
                                                        ctx->screenshot(ss_path,
                                                            [&, ctx]() {
                                                                screenshot_ok =
                                                                    true;
                                                                // Step 7: send_command
                                                                ctx->send_command(
                                                                    "Runtime.evaluate",
                                                                    {{"expression",
                                                                      "1+1"},
                                                                     {"returnByValue",
                                                                      true}},
                                                                    [&, ctx](
                                                                        nlohmann::
                                                                            json r3) {
                                                                        raw_cmd_ok =
                                                                            r3["result"]["value"]
                                                                                .get<int>() == 2;
                                                                        // Step 8: close
                                                                        ctx->close(
                                                                            [&]() {
                                                                                close_ok = true;
                                                                                // Step 9: reopen (different ptr)
                                                                                browser->get_context(
                                                                                    "baidu",
                                                                                    [&](BrowserContext*
                                                                                            ctx3) {
                                                                                        ctx_after = ctx3;
                                                                                        reopen_ok = true;
                                                                                        done = true;
                                                                                    },
                                                                                    [&](const std::
                                                                                             string&) {
                                                                                        done = true;
                                                                                    });
                                                                            },
                                                                            [&](const std::
                                                                                     string&) {
                                                                                done = true;
                                                                            });
                                                                    },
                                                                    [&](const std::
                                                                             string&) {
                                                                        done = true;
                                                                    });
                                                            },
                                                            [&](const std::string&) {
                                                                done = true; });
                                                    },
                                                    [&](const std::string&) {
                                                        done = true; });
                                            },
                                            [&](const std::string&) {
                                                done = true; });
                                    },
                                    [&](const std::string&) {
                                        done = true; });
                            },
                            [&](const std::string&) {
                                done = true; });
                    },
                    [&](const std::string&) {
                        done = true; });
            },
            [&](const std::string&) {
                done = true; });

        run_loop(browser, done, 600);
        CHECK_TRUE(nav_ok, "baidu: navigate success");
        CHECK_TRUE(inject_ok, "baidu: add_init_script injected + verified");
        CHECK_TRUE(url_ok, "baidu: current_url ok");
        CHECK_TRUE(eval_ok, "baidu: evaluate ok");
        CHECK_TRUE(page_url.find("baidu.com") != std::string::npos,
                   "baidu: url contains baidu.com");
        CHECK_TRUE(!page_title.empty(), "baidu: title not empty");
        CHECK_TRUE(page_title.find("百度") != std::string::npos,
                   "baidu: title contains 百度");
        CHECK_TRUE(screenshot_ok, "baidu: screenshot ok");
        // verify screenshot file exists and is non-empty
        std::error_code ec;
        auto fsize = std::filesystem::file_size(ss_path, ec);
        CHECK_TRUE(!ec && fsize > 0, "baidu: screenshot file non-empty");
        CHECK_TRUE(raw_cmd_ok, "baidu: send_command(1+1=2) ok");
        CHECK_TRUE(close_ok, "baidu: close ok");
        CHECK_TRUE(reopen_ok, "baidu: reopen after close ok");
        CHECK_TRUE(ctx_after != nullptr, "baidu: reopened ctx non-null");
        CHECK_TRUE(ctx_before != ctx_after,
                   "baidu: reopened ctx != closed ctx");

        browser->stop();
        std::filesystem::remove(ss_path, ec);
        delete browser;
    }
    report("AllFuncOnBaidu");

    // ============================================================
    // Cleanup
    // ============================================================
    std::error_code ec;
    std::filesystem::remove("/tmp/lynne_browser_test/session_test_state.json", ec);
    std::filesystem::remove("/tmp/lynne_browser_test/baidu_state.json", ec);

    printf("\n=== ALL TESTS DONE ===\n");
    return 0;
}
