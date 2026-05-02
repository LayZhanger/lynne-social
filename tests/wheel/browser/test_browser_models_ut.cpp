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
