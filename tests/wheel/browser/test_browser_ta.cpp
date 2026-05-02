#include "wheel/browser/browser_manager.h"
#include "wheel/browser/browser_factory.h"
#include "wheel/browser/browser_models.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>

using namespace lynne::wheel;

static int passed = 0;
static int failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { printf("  [PASS] %s\n", msg); ++passed; } \
         else { printf("  [FAIL] %s\n", msg); ++failed; } \
    } while (0)

#define CHECK_TRUE(cond, msg)  CHECK((cond), msg)
#define CHECK_FALSE(cond, msg) CHECK(!(cond), msg)

static void check_str(const std::string& a, const std::string& b,
                      const char* msg) {
    if (a == b) { printf("  [PASS] %s\n", msg); ++passed; }
    else { printf("  [FAIL] %s ('%s' vs '%s')\n", msg, a.c_str(), b.c_str());
           ++failed; }
}

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

int main() {
    if (!test_chrome_available()) {
        printf("SKIP: no Chrome binary found\n");
        return 0;
    }

    // ============================================================
    // Lifecycle
    // ============================================================
    {
        BrowserConfig cfg;
        cfg.headless = true;
        cfg.cdp_port = 0;

        auto* browser = BrowserFactory().create(cfg);

        check_str(browser->name(), "browser", "name");
        CHECK_FALSE(browser->health_check(), "health_check false before start");

        browser->start();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        CHECK_TRUE(browser->health_check(), "health_check true after start");

        browser->stop();
        CHECK_FALSE(browser->health_check(), "health_check false after stop");

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
        std::this_thread::sleep_for(std::chrono::seconds(2));
        CHECK_TRUE(browser->health_check(), "start twice: health_check true");
        browser->stop();
        delete browser;
    }
    report("StartTwice");

    // ============================================================
    // GetContext + Navigate + Evaluate
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        std::this_thread::sleep_for(std::chrono::seconds(2));

        bool ctx_ok = false;
        bool nav_ok = false;
        bool eval_ok = false;
        std::string eval_result;

        browser->get_context("test",
            [&](BrowserContext* ctx) {
                ctx_ok = true;
                ctx->navigate("data:text/html,<h1>Hello</h1>",
                    [&, ctx]() {
                        nav_ok = true;
                        ctx->evaluate("document.title",
                            [&](nlohmann::json r) {
                                eval_ok = true;
                                eval_result = r["value"].get<std::string>();
                                browser->stop();
                            },
                            [&](const std::string& err) {
                                printf("  evaluate error: %s\n", err.c_str());
                                browser->stop();
                            });
                    },
                    [&](const std::string& err) {
                        printf("  navigate error: %s\n", err.c_str());
                        browser->stop();
                    });
            },
            [&](const std::string& err) {
                printf("  get_context error: %s\n", err.c_str());
                browser->stop();
            });

        // run event loop until stop
        // In TA test, we use a simple polling approach
        // Note: in production, uv_run handles this
        // For tests, we manually step the scheduler
        // Since we don't have the scheduler ref here, use sleep
        std::this_thread::sleep_for(std::chrono::seconds(3));

        CHECK_TRUE(ctx_ok, "get_context succeeded");
        CHECK_TRUE(nav_ok, "navigate succeeded");
        CHECK_TRUE(eval_ok, "evaluate succeeded");

        delete browser;
    }
    report("GetContextNavigateEvaluate");

    // ============================================================
    // Context caching
    // ============================================================
    {
        auto* browser = BrowserFactory().create(BrowserConfig{});
        browser->start();
        std::this_thread::sleep_for(std::chrono::seconds(2));

        BrowserContext* first = nullptr;
        BrowserContext* second = nullptr;

        browser->get_context("cache_test",
            [&](BrowserContext* ctx) {
                first = ctx;
                browser->get_context("cache_test",
                    [&](BrowserContext* ctx2) {
                        second = ctx2;
                        browser->stop();
                    },
                    [&](const std::string& err) {
                        printf("  second get_context error: %s\n", err.c_str());
                        browser->stop();
                    });
            },
            [&](const std::string& err) {
                printf("  first get_context error: %s\n", err.c_str());
                browser->stop();
            });

        std::this_thread::sleep_for(std::chrono::seconds(2));
        CHECK_TRUE(first != nullptr, "first context not null");
        CHECK_TRUE(second != nullptr, "second context not null");
        CHECK_TRUE(first == second, "contexts are cached (same pointer)");

        delete browser;
    }
    report("ContextCaching");

    // ============================================================
    // Save/Restore session
    // ============================================================
    {
        auto cfg = BrowserConfig{};
        cfg.sessions_dir = "/tmp/lynne_browser_test";

        auto* browser = BrowserFactory().create(cfg);
        browser->start();
        std::this_thread::sleep_for(std::chrono::seconds(2));

        bool restored = false;

        // First restore (no session yet -> false)
        browser->restore_session("test_platform",
            [&](bool ok) {
                CHECK_FALSE(ok, "restore_session false when no file");
                browser->stop();
            },
            [&](const std::string& err) {
                printf("  restore_session error: %s\n", err.c_str());
                browser->stop();
            });

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Save session
        browser->start(); // re-start
        std::this_thread::sleep_for(std::chrono::seconds(1));

        bool saved = false;
        browser->get_context("save_test",
            [&](BrowserContext*) {
                browser->save_session("save_test",
                    [&]() {
                        saved = true;
                        // Now restore
                        browser->restore_session("save_test",
                            [&](bool ok2) {
                                CHECK_TRUE(ok2, "restore_session true after save");
                                browser->stop();
                            },
                            [&](const std::string& err) {
                                printf("  restore2 error: %s\n", err.c_str());
                                browser->stop();
                            });
                    },
                    [&](const std::string& err) {
                        printf("  save error: %s\n", err.c_str());
                        browser->stop();
                    });
            },
            [&](const std::string& err) {
                printf("  get_context for save error: %s\n", err.c_str());
                browser->stop();
            });

        std::this_thread::sleep_for(std::chrono::seconds(2));
        CHECK_TRUE(saved, "save_session succeeded");

        delete browser;
    }
    report("SessionPersistence");

    // ============================================================
    // Cleanup temp files
    // ============================================================
    std::remove("/tmp/lynne_browser_test/save_test_state.json");

    printf("\n=== ALL TESTS DONE ===\n");
    return 0;
}
