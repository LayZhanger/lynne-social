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
           system("which google-chrome-stable >/dev/null 2>&1") == 0 ||
           system("which chromium >/dev/null 2>&1") == 0 ||
           system("which chromium-browser >/dev/null 2>&1") == 0;
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
        CHECK_TRUE(started, "start ok");

        browser->stop();
        CHECK_FALSE(browser->health_check(), "health false after stop");
        delete browser;
    }
    report("Lifecycle");

    // ============================================================
    // StartStopStart (restart)
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
        CHECK_TRUE(started, "first start");
        browser->stop();
        CHECK_FALSE(browser->health_check(), "stopped");

        browser->start();
        started = false;
        for (int i = 0; i < 80 && !started; ++i) {
            browser->step();
            started = browser->health_check();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        CHECK_TRUE(started, "restart ok");
        browser->stop();
        delete browser;
    }
    report("StartStopStart");

    // ============================================================
    // Error: get_context before start
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
    report("ErrorBeforeStart");

    // ============================================================
    // Navigate + Evaluate + CurrentUrl
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

        std::atomic<bool> done{false};
        std::string title, url;

        browser->get_context("main",
            [&](BrowserContext* ctx) {
                ctx->navigate("data:text/html,<title>TA</title>",
                    [&, ctx]() {
                        ctx->current_url(
                            [&, ctx](const std::string& u) {
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
        CHECK_TRUE(!title.empty(), "title not empty");
        CHECK_TRUE(title == "TA", "title is TA");
        CHECK_TRUE(url.find("data:text/html") != std::string::npos,
                   "url contains data:text/html");
        browser->stop();
        delete browser;
    }
    report("NavigateEvaluate");

    // ============================================================
    // AddInitScript + Close + Reopen
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

        std::atomic<bool> done{false};
        BrowserContext* first = nullptr;
        BrowserContext* after = nullptr;

        browser->get_context("test_close",
            [&](BrowserContext* ctx) {
                first = ctx;
                ctx->add_init_script(
                    "Object.defineProperty(navigator, "
                    "'_lt', {get: () => 'ok'});",
                    [&, ctx]() {
                        ctx->navigate("data:text/html,<title>C</title>",
                            [&, ctx]() {
                                ctx->close(
                                    [&]() {
                                        browser->get_context("test_close",
                                            [&](BrowserContext* c) {
                                                after = c;
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
        CHECK_TRUE(first != nullptr, "first non-null");
        CHECK_TRUE(after != nullptr, "after non-null");
        CHECK_TRUE(first != after, "close+reopen: different ptr");
        browser->stop();
        delete browser;
    }
    report("AddInitScript_Close");

    // ============================================================
    // Context isolation: a != b
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

        std::atomic<bool> done{false};
        BrowserContext* a = nullptr;
        BrowserContext* b = nullptr;

        browser->get_context("plat_a",
            [&](BrowserContext* ca) {
                a = ca;
                browser->get_context("plat_b",
                    [&](BrowserContext* cb) {
                        b = cb;
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });

        run_loop(browser, done);
        CHECK_TRUE(a != nullptr, "plat_a non-null");
        CHECK_TRUE(b != nullptr, "plat_b non-null");
        CHECK_TRUE(a != b, "different platforms → different ctx");
        browser->stop();
        delete browser;
    }
    report("ContextIsolation");

    // ============================================================
    // SendCommand (raw CDP escape)
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

        std::atomic<bool> done{false};
        int sum = 0;

        browser->get_context("raw",
            [&](BrowserContext* ctx) {
                ctx->send_command("Runtime.evaluate",
                    {{"expression", "1+1"}, {"returnByValue", true}},
                    [&](nlohmann::json r) {
                        sum = r["result"]["value"].get<int>();
                        done = true;
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });

        run_loop(browser, done);
        CHECK_TRUE(sum == 2, "send_command 1+1=2");
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
        std::filesystem::create_directories("/tmp/lynne_browser_test");
        auto* browser = BrowserFactory().create(cfg);
        browser->start();
        std::atomic<bool> started{false};
        for (int i = 0; i < 80 && !started; ++i) {
            browser->step();
            started = browser->health_check();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }

        std::atomic<bool> done{false};
        bool restore_empty = false;
        bool saved = false;

        // Step 1: restore when no file → false
        browser->restore_session("sess_test",
            [&](bool ok) {
                restore_empty = !ok;
                // Step 2: create context + navigate + save
                browser->get_context("sess_test",
                    [&](BrowserContext* ctx) {
                        ctx->navigate("data:text/html,<title>S</title>",
                            [&, ctx]() {
                                browser->step();
                                std::this_thread::sleep_for(
                                    std::chrono::milliseconds(200));
                                browser->save_session("sess_test",
                                    [&]() {
                                        saved = true;
                                        done = true;
                                    },
                                    [&](const std::string&) { done = true; });
                            },
                            [&](const std::string&) { done = true; });
                    },
                    [&](const std::string&) { done = true; });
            },
            [&](const std::string&) { done = true; });

        run_loop(browser, done, 400);
        CHECK_TRUE(restore_empty, "restore: false when no file");
        CHECK_TRUE(saved, "save: ok");

        // Verify file was written (use same path logic as save_session)
        std::string sess_dir = cfg.sessions_dir;
        if (!sess_dir.empty() && sess_dir.back() != '/') sess_dir += '/';
        std::string sess_path = sess_dir + "sess_test_state.json";
        std::error_code ec;
        auto fsize = std::filesystem::file_size(sess_path, ec);
        CHECK_TRUE(!ec && fsize > 0, "save: session file non-empty");
        browser->stop();
        delete browser;
    }
    report("SessionPersistence");

    // ============================================================
    // Real URL: baidu.com (simplified: just navigate)
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

        std::atomic<bool> done{false};
        std::string title;

        browser->get_context("baidu",
            [&](BrowserContext* ctx) {
                ctx->navigate("https://www.baidu.com/",
                    [&, ctx]() {
                        // wait for page to render
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(3000));
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

        run_loop(browser, done, 600);
        CHECK_TRUE(!title.empty(), "baidu: title not empty");
        CHECK_TRUE(title.find("百度") != std::string::npos,
                   "baidu: title contains 百度");
        browser->stop();
        delete browser;
    }
    report("RealUrlBaidu");

    // ============================================================
    // Cleanup temp files
    // ============================================================
    std::error_code ec;
    std::filesystem::remove("/tmp/lynne_browser_test/sess_test_state.json", ec);

    printf("\n=== ALL TESTS DONE ===\n");
    return 0;
}
