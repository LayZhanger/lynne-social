#include "wheel/llm/llm_models.h"
#include "wheel/llm/llm_engine.h"
#include "wheel/llm/llm_factory.h"
#include "wheel/llm/imp/deepseek_engine.h"

#include <gtest/gtest.h>

using namespace lynne::wheel;

// ============================================================
// LLMConfig — defaults
// ============================================================

TEST(LLMConfigDefaults, AllFields) {
    LLMConfig c{};
    EXPECT_EQ(c.provider, "deepseek");
    EXPECT_EQ(c.api_key, "");
    EXPECT_EQ(c.base_url, "");
    EXPECT_EQ(c.model, "deepseek-chat");
    EXPECT_DOUBLE_EQ(c.temperature, 0.7);
    EXPECT_EQ(c.max_tokens, 4096);
    EXPECT_EQ(c.timeout, 60);
    EXPECT_EQ(c.ca_cert_path, "");
}

TEST(LLMConfigDefaults, CustomValues) {
    LLMConfig c{"openai", "sk-123", "https://api.openai.com/v1", "gpt-4o",
                 0.3, 1024, 30};
    EXPECT_EQ(c.provider, "openai");
    EXPECT_EQ(c.api_key, "sk-123");
    EXPECT_EQ(c.base_url, "https://api.openai.com/v1");
    EXPECT_EQ(c.model, "gpt-4o");
    EXPECT_DOUBLE_EQ(c.temperature, 0.3);
    EXPECT_EQ(c.max_tokens, 1024);
    EXPECT_EQ(c.timeout, 30);
    EXPECT_EQ(c.ca_cert_path, "");
}

// ============================================================
// LLMConfig — from_json
// ============================================================

TEST(LLMConfigJson, FromJsonMinimal) {
    auto j = nlohmann::json::parse("{}");
    LLMConfig c;
    from_json(j, c);
    EXPECT_EQ(c.provider, "deepseek");
    EXPECT_EQ(c.model, "deepseek-chat");
    EXPECT_DOUBLE_EQ(c.temperature, 0.7);
}

TEST(LLMConfigJson, FromJsonAllFields) {
    auto j = nlohmann::json::parse(R"({
        "provider": "openai",
        "api_key": "sk-test",
        "base_url": "https://api.openai.com/v1",
        "model": "gpt-4o",
        "temperature": 0.3,
        "max_tokens": 1024,
        "timeout": 30
    })");
    LLMConfig c;
    from_json(j, c);
    EXPECT_EQ(c.provider, "openai");
    EXPECT_EQ(c.api_key, "sk-test");
    EXPECT_EQ(c.model, "gpt-4o");
    EXPECT_DOUBLE_EQ(c.temperature, 0.3);
}

TEST(LLMConfigJson, PartialOverride) {
    auto j = nlohmann::json::parse(R"({"api_key": "sk-partial"})");
    LLMConfig c;
    from_json(j, c);
    EXPECT_EQ(c.api_key, "sk-partial");
    EXPECT_EQ(c.provider, "deepseek");
    EXPECT_EQ(c.model, "deepseek-chat");
}

TEST(LLMConfigJson, CaCertPath) {
    auto j = nlohmann::json::parse(R"({"ca_cert_path": "/custom/ca.pem"})");
    LLMConfig c;
    from_json(j, c);
    EXPECT_EQ(c.ca_cert_path, "/custom/ca.pem");
}

// ============================================================
// LLMEngineFactory
// ============================================================

TEST(LLMEngineFactory, CreateWithConfig) {
    LLMConfig cfg{"deepseek", "sk-test", "", "deepseek-chat", 0.7, 4096, 60};
    LLMEngineFactory factory;
    auto* engine = factory.create(cfg);
    ASSERT_NE(engine, nullptr);
    EXPECT_EQ(engine->name(), "llm(deepseek:deepseek-chat)");
    delete engine;
}

TEST(LLMEngineFactory, CreateWithCustomModel) {
    LLMConfig cfg{"openai", "sk-test", "", "gpt-4o", 0.7, 4096, 60};
    LLMEngineFactory factory;
    auto* engine = factory.create(cfg);
    ASSERT_NE(engine, nullptr);
    EXPECT_EQ(engine->name(), "llm(openai:gpt-4o)");
    delete engine;
}

TEST(LLMEngineFactory, CreateWithEmptyApiKey) {
    LLMConfig cfg{};
    LLMEngineFactory factory;
    auto* engine = factory.create(cfg);
    ASSERT_NE(engine, nullptr);
    EXPECT_EQ(engine->name(), "llm(deepseek:deepseek-chat)");
    delete engine;
}

// ============================================================
// DeepSeekEngine — URL parsing
// ============================================================

TEST(UrlParse, HttpsDefaultPort) {
    std::string host; int port = 0; std::string path;
    EXPECT_TRUE(DeepSeekEngine::parse_base_url("https://api.deepseek.com/v1", host, port, path));
    EXPECT_EQ(host, "api.deepseek.com");
    EXPECT_EQ(port, 443);
    EXPECT_EQ(path, "/v1");
}

TEST(UrlParse, HttpsCustomPort) {
    std::string host; int port = 0; std::string path;
    EXPECT_TRUE(DeepSeekEngine::parse_base_url("https://localhost:8443/v1", host, port, path));
    EXPECT_EQ(host, "localhost");
    EXPECT_EQ(port, 8443);
    EXPECT_EQ(path, "/v1");
}

TEST(UrlParse, HttpDefaultPort) {
    std::string host; int port = 0; std::string path;
    EXPECT_TRUE(DeepSeekEngine::parse_base_url("http://localhost:8080/api", host, port, path));
    EXPECT_EQ(host, "localhost");
    EXPECT_EQ(port, 8080);
    EXPECT_EQ(path, "/api");
}

TEST(UrlParse, NoPath) {
    std::string host; int port = 0; std::string path;
    EXPECT_TRUE(DeepSeekEngine::parse_base_url("https://api.deepseek.com", host, port, path));
    EXPECT_EQ(host, "api.deepseek.com");
    EXPECT_EQ(port, 443);
    EXPECT_TRUE(path.empty());
}

TEST(UrlParse, Invalid) {
    std::string host; int port = 0; std::string path;
    EXPECT_FALSE(DeepSeekEngine::parse_base_url("not-a-url", host, port, path));
}

TEST(UrlParse, TrailingSlash) {
    std::string host; int port = 0; std::string path;
    EXPECT_TRUE(DeepSeekEngine::parse_base_url("https://api.deepseek.com/v1/", host, port, path));
    EXPECT_EQ(path, "/v1/");
}
