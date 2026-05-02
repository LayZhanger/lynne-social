#include "wheel/browser/browser_models.h"
#include <gtest/gtest.h>

using namespace lynne::wheel;

TEST(BrowserConfig, DefaultValues) {
    BrowserConfig cfg;
    EXPECT_TRUE(cfg.headless);
    EXPECT_EQ(cfg.slow_mo_ms, 500);
    EXPECT_EQ(cfg.viewport_width, 1920);
    EXPECT_EQ(cfg.viewport_height, 1080);
    EXPECT_EQ(cfg.locale, "zh-CN");
    EXPECT_EQ(cfg.timeout_ms, 30000);
    EXPECT_EQ(cfg.sessions_dir, "data/sessions");
    EXPECT_EQ(cfg.cdp_port, 0);
    EXPECT_TRUE(cfg.chrome_path.empty());
}

TEST(BrowserConfig, CustomValues) {
    BrowserConfig cfg;
    cfg.headless = false;
    cfg.cdp_port = 9999;
    cfg.chrome_path = "/usr/bin/chromium";
    cfg.slow_mo_ms = 1000;

    EXPECT_FALSE(cfg.headless);
    EXPECT_EQ(cfg.cdp_port, 9999);
    EXPECT_EQ(cfg.chrome_path, "/usr/bin/chromium");
    EXPECT_EQ(cfg.slow_mo_ms, 1000);
}

TEST(BrowserConfig, JsonSerialization) {
    BrowserConfig cfg;
    cfg.cdp_port = 9333;
    cfg.headless = false;

    nlohmann::json j = cfg;
    EXPECT_EQ(j["cdp_port"], 9333);
    EXPECT_FALSE(j["headless"].get<bool>());

    auto cfg2 = j.get<BrowserConfig>();
    EXPECT_EQ(cfg2.cdp_port, 9333);
    EXPECT_FALSE(cfg2.headless);
}

TEST(BrowserConfig, FromJsonEmpty) {
    BrowserConfig cfg;
    cfg.slow_mo_ms = 999;
    nlohmann::json j = nlohmann::json::object();
    from_json(j, cfg);
    EXPECT_EQ(cfg.slow_mo_ms, 500);
    EXPECT_TRUE(cfg.headless);
    EXPECT_EQ(cfg.cdp_port, 0);
    EXPECT_TRUE(cfg.chrome_path.empty());
    EXPECT_EQ(cfg.timeout_ms, 30000);
    EXPECT_EQ(cfg.sessions_dir, "data/sessions");
}

TEST(BrowserConfig, FromJsonPartial) {
    auto j = nlohmann::json::parse(R"({"headless": false, "cdp_port": 9333})");
    BrowserConfig cfg;
    from_json(j, cfg);
    EXPECT_FALSE(cfg.headless);
    EXPECT_EQ(cfg.cdp_port, 9333);
    EXPECT_EQ(cfg.slow_mo_ms, 500);
    EXPECT_EQ(cfg.timeout_ms, 30000);
    EXPECT_EQ(cfg.sessions_dir, "data/sessions");
}

TEST(BrowserConfig, FromJsonAllFields) {
    auto j = nlohmann::json::parse(R"({
        "headless": false, "slow_mo_ms": 1000, "viewport_width": 1280,
        "viewport_height": 720, "locale": "en-US", "timeout_ms": 15000,
        "sessions_dir": "/tmp/s", "cdp_port": 9999,
        "chrome_path": "/usr/bin/chromium"
    })");
    BrowserConfig cfg;
    from_json(j, cfg);
    EXPECT_FALSE(cfg.headless);
    EXPECT_EQ(cfg.slow_mo_ms, 1000);
    EXPECT_EQ(cfg.viewport_width, 1280);
    EXPECT_EQ(cfg.viewport_height, 720);
    EXPECT_EQ(cfg.locale, "en-US");
    EXPECT_EQ(cfg.timeout_ms, 15000);
    EXPECT_EQ(cfg.sessions_dir, "/tmp/s");
    EXPECT_EQ(cfg.cdp_port, 9999);
    EXPECT_EQ(cfg.chrome_path, "/usr/bin/chromium");
}
