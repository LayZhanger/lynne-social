#include "wheel/config/config_models.h"
#include "wheel/config/config_loader.h"
#include "wheel/config/config_factory.h"
#include "wheel/config/imp/json_config_loader.h"

#include <gtest/gtest.h>
#include <cstdlib>
#include <stdexcept>

using namespace lynne::wheel;

// ============================================================
// Config — defaults
// ============================================================

TEST(ConfigDefaults, ServerConfigDefaults) {
    ServerConfig c{};
    EXPECT_EQ(c.port, 7890);
    EXPECT_TRUE(c.auto_open_browser);
}

TEST(ConfigDefaults, LLMConfigDefaults) {
    LLMConfig c{};
    EXPECT_EQ(c.provider, "deepseek");
    EXPECT_EQ(c.model, "deepseek-chat");
    EXPECT_DOUBLE_EQ(c.temperature, 0.7);
    EXPECT_EQ(c.max_tokens, 4096);
    EXPECT_EQ(c.timeout, 60);
    EXPECT_EQ(c.api_key, "");
    EXPECT_EQ(c.base_url, "");
}

TEST(ConfigDefaults, BrowserConfigDefaults) {
    BrowserConfig c{};
    EXPECT_FALSE(c.headless);
    EXPECT_EQ(c.slow_mo, 500);
    EXPECT_EQ(c.viewport_width, 1920);
    EXPECT_EQ(c.viewport_height, 1080);
    EXPECT_EQ(c.locale, "zh-CN");
    EXPECT_EQ(c.timeout, 30000);
}

TEST(ConfigDefaults, PlatformConfigDefaults) {
    PlatformConfig c{};
    EXPECT_FALSE(c.enabled);
    EXPECT_EQ(c.session_file, "");
    EXPECT_EQ(c.base_url, "");
    EXPECT_TRUE(c.account.empty());
}

TEST(ConfigDefaults, TaskConfigDefaults) {
    TaskConfig c{};
    EXPECT_EQ(c.name, "");
    EXPECT_TRUE(c.platforms.empty());
    EXPECT_EQ(c.intent, "");
    EXPECT_EQ(c.schedule, "manual");
}

TEST(ConfigDefaults, ConfigEmptyDefaults) {
    Config c{};
    EXPECT_EQ(c.server.port, 7890);
    EXPECT_EQ(c.llm.provider, "deepseek");
    EXPECT_FALSE(c.browser.headless);
    EXPECT_TRUE(c.platforms.empty());
    EXPECT_TRUE(c.tasks.empty());
}

TEST(ConfigDefaults, CustomFields) {
    ServerConfig s;
    s.port = 3000;
    s.auto_open_browser = false;
    EXPECT_EQ(s.port, 3000);
    EXPECT_FALSE(s.auto_open_browser);

    LLMConfig l;
    l.api_key = "sk-xxx";
    EXPECT_EQ(l.api_key, "sk-xxx");

    PlatformConfig p;
    p.account["phone"] = "13800138000";
    EXPECT_EQ(p.account["phone"], "13800138000");
}

// ============================================================
// Config — from_json
// ============================================================

TEST(ConfigFromJson, ServerConfigMinimal) {
    auto j = nlohmann::json::parse("{}");
    ServerConfig c;
    from_json(j, c);
    EXPECT_EQ(c.port, 7890);
    EXPECT_TRUE(c.auto_open_browser);
}

TEST(ConfigFromJson, ServerConfigAllFields) {
    auto j = nlohmann::json::parse(R"({"port": 9999, "auto_open_browser": false})");
    ServerConfig c;
    from_json(j, c);
    EXPECT_EQ(c.port, 9999);
    EXPECT_FALSE(c.auto_open_browser);
}

TEST(ConfigFromJson, LLMConfigAllFields) {
    auto j = nlohmann::json::parse(R"({
        "provider": "openai",
        "api_key": "sk-test",
        "base_url": "https://api.openai.com/v1",
        "model": "gpt-4",
        "temperature": 0.3,
        "max_tokens": 2048,
        "timeout": 90
    })");
    LLMConfig c;
    from_json(j, c);
    EXPECT_EQ(c.provider, "openai");
    EXPECT_EQ(c.api_key, "sk-test");
    EXPECT_EQ(c.base_url, "https://api.openai.com/v1");
    EXPECT_EQ(c.model, "gpt-4");
    EXPECT_DOUBLE_EQ(c.temperature, 0.3);
    EXPECT_EQ(c.max_tokens, 2048);
    EXPECT_EQ(c.timeout, 90);
}

TEST(ConfigFromJson, LLMConfigPartial) {
    auto j = nlohmann::json::parse(R"({"api_key": "sk-test"})");
    LLMConfig c;
    from_json(j, c);
    EXPECT_EQ(c.api_key, "sk-test");
    EXPECT_EQ(c.provider, "deepseek");
    EXPECT_EQ(c.model, "deepseek-chat");
}

TEST(ConfigFromJson, BrowserConfigAllFields) {
    auto j = nlohmann::json::parse(R"({
        "headless": true,
        "slow_mo": 200,
        "viewport_width": 1280,
        "viewport_height": 720,
        "locale": "en-US",
        "timeout": 15000
    })");
    BrowserConfig c;
    from_json(j, c);
    EXPECT_TRUE(c.headless);
    EXPECT_EQ(c.slow_mo, 200);
    EXPECT_EQ(c.viewport_width, 1280);
    EXPECT_EQ(c.viewport_height, 720);
    EXPECT_EQ(c.locale, "en-US");
    EXPECT_EQ(c.timeout, 15000);
}

TEST(ConfigFromJson, PlatformConfigAllFields) {
    auto j = nlohmann::json::parse(R"({
        "enabled": true,
        "session_file": "data/sessions/twitter.json",
        "base_url": "https://x.com",
        "account": {"phone": "13800138000", "password": "secret"}
    })");
    PlatformConfig c;
    from_json(j, c);
    EXPECT_TRUE(c.enabled);
    EXPECT_EQ(c.session_file, "data/sessions/twitter.json");
    EXPECT_EQ(c.base_url, "https://x.com");
    EXPECT_EQ(c.account["phone"], "13800138000");
    EXPECT_EQ(c.account["password"], "secret");
}

TEST(ConfigFromJson, TaskConfigAllFields) {
    auto j = nlohmann::json::parse(R"({
        "name": "AI\u76d1\u63a7",
        "platforms": ["twitter", "rednote"],
        "intent": "\u5927\u6a21\u578b\u8fdb\u5c55",
        "schedule": "every 4 hours"
    })");
    TaskConfig c;
    from_json(j, c);
    EXPECT_EQ(c.name, "AI监控");
    EXPECT_EQ(c.platforms.size(), 2u);
    EXPECT_EQ(c.platforms[0], "twitter");
    EXPECT_EQ(c.platforms[1], "rednote");
    EXPECT_EQ(c.intent, "大模型进展");
    EXPECT_EQ(c.schedule, "every 4 hours");
}

TEST(ConfigFromJson, ConfigFullStructure) {
    auto j = nlohmann::json::parse(R"({
        "server": {"port": 8888, "auto_open_browser": false},
        "llm": {"provider": "openai", "api_key": "sk-test", "model": "gpt-4"},
        "browser": {"headless": true, "slow_mo": 200},
        "platforms": {
            "twitter": {
                "enabled": true,
                "session_file": "data/sessions/twitter.json",
                "base_url": "https://x.com"
            }
        },
        "tasks": [
            {
                "name": "AI\u52a8\u6001",
                "platforms": ["twitter"],
                "intent": "\u5173\u6ce8AI\u5927\u6a21\u578b\u7684\u6700\u65b0\u8fdb\u5c55",
                "schedule": "every 4 hours"
            }
        ]
    })");
    Config c;
    from_json(j, c);

    EXPECT_EQ(c.server.port, 8888);
    EXPECT_FALSE(c.server.auto_open_browser);
    EXPECT_EQ(c.llm.provider, "openai");
    EXPECT_EQ(c.llm.model, "gpt-4");
    EXPECT_TRUE(c.browser.headless);
    EXPECT_EQ(c.browser.slow_mo, 200);

    ASSERT_TRUE(c.platforms.count("twitter"));
    EXPECT_TRUE(c.platforms["twitter"].enabled);
    EXPECT_EQ(c.platforms["twitter"].base_url, "https://x.com");

    ASSERT_EQ(c.tasks.size(), 1u);
    EXPECT_EQ(c.tasks[0].name, "AI动态");
    EXPECT_EQ(c.tasks[0].platforms.size(), 1u);
    EXPECT_EQ(c.tasks[0].platforms[0], "twitter");
}

// ============================================================
// Config — env var resolution
// ============================================================

TEST(EnvResolution, PlainStringNoPlaceholder) {
    auto j = nlohmann::json("plain_text");
    resolve_env_vars(j);
    EXPECT_EQ(j.get<std::string>(), "plain_text");
}

TEST(EnvResolution, SingleEnvVar) {
    ::setenv("TEST_KEY", "test_value", 1);

    auto j = nlohmann::json("prefix_${TEST_KEY}_suffix");
    resolve_env_vars(j);
    EXPECT_EQ(j.get<std::string>(), "prefix_test_value_suffix");

    ::unsetenv("TEST_KEY");
}

TEST(EnvResolution, MissingEnvVarThrows) {
    auto j = nlohmann::json("${MISSING_VAR}");
    EXPECT_THROW(resolve_env_vars(j), std::runtime_error);
}

TEST(EnvResolution, EmptyEnvVarThrows) {
    ::setenv("EMPTY_KEY", "", 1);

    auto j = nlohmann::json("${EMPTY_KEY}");
    EXPECT_THROW(resolve_env_vars(j), std::runtime_error);

    ::unsetenv("EMPTY_KEY");
}

TEST(EnvResolution, RecursiveObject) {
    ::setenv("KEY", "val", 1);

    auto j = nlohmann::json::parse(R"({"a": "${KEY}", "b": {"c": "${KEY}"}})");
    resolve_env_vars(j);
    EXPECT_EQ(j["a"].get<std::string>(), "val");
    EXPECT_EQ(j["b"]["c"].get<std::string>(), "val");

    ::unsetenv("KEY");
}

TEST(EnvResolution, RecursiveArray) {
    ::setenv("K", "v", 1);

    auto j = nlohmann::json::parse(R"(["${K}", "plain", {"nested": "${K}"}])");
    resolve_env_vars(j);
    EXPECT_EQ(j[0].get<std::string>(), "v");
    EXPECT_EQ(j[1].get<std::string>(), "plain");
    EXPECT_EQ(j[2]["nested"].get<std::string>(), "v");

    ::unsetenv("K");
}

TEST(EnvResolution, NonStringUnchanged) {
    auto j = nlohmann::json(42);
    resolve_env_vars(j);
    EXPECT_EQ(j.get<int>(), 42);

    auto jb = nlohmann::json(true);
    resolve_env_vars(jb);
    EXPECT_TRUE(jb.get<bool>());
}

// ============================================================
// Factory — ConfigLoaderFactory
// ============================================================

TEST(ConfigFactory, CreateDefaultReturnsJsonConfigLoader) {
    ConfigLoaderFactory factory;
    auto* loader = factory.create();
    ASSERT_NE(loader, nullptr);
    delete loader;
}

TEST(ConfigFactory, CreateWithPathReturnsJsonConfigLoader) {
    ConfigLoaderFactory factory;
    auto* loader = factory.create("custom.json");
    ASSERT_NE(loader, nullptr);
    delete loader;
}
