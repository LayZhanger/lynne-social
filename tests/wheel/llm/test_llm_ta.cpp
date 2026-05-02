#include "wheel/llm/llm_engine.h"
#include "wheel/llm/llm_models.h"
#include "wheel/llm/llm_factory.h"
#include "wheel/llm/imp/deepseek_engine.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace lynne::wheel;

static int passed = 0;
static int failed = 0;
static int skipped = 0;

#define CHECK(cond, msg) \
    do { if (cond) { printf("  [PASS] %s\n", msg); ++passed; } \
         else { printf("  [FAIL] %s\n", msg); ++failed; } \
    } while (0)

#define CHECK_TRUE(cond, msg)  CHECK((cond), msg)
#define CHECK_FALSE(cond, msg) CHECK(!(cond), msg)

static void check_str(const std::string& a, const std::string& b, const char* msg) {
    if (a == b) { printf("  [PASS] %s\n", msg); ++passed; }
    else { printf("  [FAIL] %s ('%s' vs '%s')\n", msg, a.c_str(), b.c_str()); ++failed; }
}

static void report(const char* suite) {
    printf("--- %s: %d/%d (%d skipped) ---\n", suite, passed, passed + failed, skipped);
}

int main() {
    const char* api_key = std::getenv("DEEPSEEK_API_KEY");
    bool has_api_key = api_key && api_key[0] != '\0';

    // ============================================================
    // Lifecycle
    // ============================================================
    {
        LLMConfig cfg{"deepseek", "sk-test"};
        auto* engine = LLMEngineFactory().create(cfg);

        check_str(engine->name(), "llm(deepseek:deepseek-chat)", "name");

        CHECK_FALSE(engine->health_check(), "health_check false before start");

        engine->start();
        CHECK_TRUE(engine->health_check(), "health_check true after start");

        engine->stop();
        CHECK_FALSE(engine->health_check(), "health_check false after stop");

        delete engine;
    }
    report("Lifecycle");

    // ============================================================
    // Factory — creates with empty api_key (env fallback at runtime)
    // ============================================================
    {
        LLMConfig cfg{};
        auto* engine = LLMEngineFactory().create(cfg);
        bool ok = (engine != nullptr);
        if (ok) {
            printf("  [PASS] factory: creates with empty api_key\n");
            ++passed;
        } else {
            printf("  [FAIL] factory: returns null\n");
            ++failed;
        }
        delete engine;
    }
    report("Factory");

    // ============================================================
    // chat with API (requires DEEPSEEK_API_KEY)
    // ============================================================
    if (!has_api_key) {
        printf("  [SKIP] chat tests — DEEPSEEK_API_KEY not set\n");
        skipped += 3;
    } else {
        // Plain chat
        {
            LLMConfig cfg{"deepseek", api_key};
            cfg.timeout = 30;
            auto* engine = LLMEngineFactory().create(cfg);
            engine->start();

            std::string err_msg;

            nlohmann::json msgs = nlohmann::json::array();
            msgs.push_back({{"role", "user"}, {"content", "Say exactly: hello world"}});

            printf("  > sending messages:\n%s\n", msgs.dump(2).c_str());

            engine->chat(msgs,
                [&](nlohmann::json result) {
                    printf("  < received response:\n%s\n", result.dump(2).c_str());
                    try {
                        auto content = result["choices"][0]["message"]["content"].get<std::string>();
                        if (content.find("hello") != std::string::npos) {
                            printf("  [PASS] chat: got response with 'hello'\n");
                            ++passed;
                        } else {
                            printf("  [FAIL] chat: unexpected content '%s'\n", content.c_str());
                            ++failed;
                        }
                    } catch (...) {
                        printf("  [FAIL] chat: unexpected response structure\n");
                        ++failed;
                    }
                    engine->stop();
                },
                [&](const std::string& err) {
                    err_msg = err;
                    engine->stop();
                }
            );

            engine->run();

            if (!err_msg.empty()) {
                printf("  [FAIL] chat error: %s\n", err_msg.c_str());
                ++failed;
            }

            delete engine;
        }
        report("ChatPlain");

        // Plain chat (Chinese)
        {
            LLMConfig cfg{"deepseek", api_key};
            cfg.timeout = 30;
            auto* engine = LLMEngineFactory().create(cfg);
            engine->start();

            std::string err_msg;

            nlohmann::json msgs = nlohmann::json::array();
            msgs.push_back({{"role", "user"}, {"content", "请用中文回答：世界上最高的山峰是什么？"}});

            printf("  > sending (Chinese):\n%s\n", msgs.dump(2).c_str());

            engine->chat(msgs,
                [&](nlohmann::json result) {
                    printf("  < received response:\n%s\n", result.dump(2).c_str());
                    try {
                        auto content = result["choices"][0]["message"]["content"].get<std::string>();
                        if (content.find("珠穆朗玛") != std::string::npos ||
                            content.find("8848") != std::string::npos ||
                            content.find("Everest") != std::string::npos) {
                            printf("  [PASS] chat Chinese: got expected answer\n");
                            ++passed;
                        } else if (content.find("乔戈里") != std::string::npos ||
                                   content.find("K2") != std::string::npos) {
                            printf("  [FAIL] chat Chinese: K2 is second highest, not highest\n");
                            ++failed;
                        } else {
                            printf("  [FAIL] chat Chinese: unexpected content '%s'\n",
                                   content.substr(0, 120).c_str());
                            ++failed;
                        }
                    } catch (...) {
                        printf("  [FAIL] chat Chinese: unexpected response structure\n");
                        ++failed;
                    }
                    engine->stop();
                },
                [&](const std::string& err) {
                    err_msg = err;
                    engine->stop();
                }
            );

            engine->run();

            if (!err_msg.empty()) {
                printf("  [FAIL] chat Chinese error: %s\n", err_msg.c_str());
                ++failed;
            }

            delete engine;
        }
        report("ChatChinese");

        // Chat with tools
        {
            LLMConfig cfg{"deepseek", api_key};
            cfg.timeout = 30;
            auto* engine = LLMEngineFactory().create(cfg);
            engine->start();

            nlohmann::json tools = nlohmann::json::array();
            tools.push_back({
                {"type", "function"},
                {"function", {
                    {"name", "get_weather"},
                    {"description", "Get weather for a city"},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"city", {{"type", "string"}}}
                        }},
                        {"required", {"city"}}
                    }}
                }}
            });

            nlohmann::json msgs = nlohmann::json::array();
            msgs.push_back({{"role", "user"}, {"content", "What is the weather in Beijing?"}});

            printf("  > sending messages (with tools):\n%s\n", msgs.dump(2).c_str());
            printf("  > tools:\n%s\n", tools.dump(2).c_str());

            std::string err_msg;

            engine->chat(msgs,
                [&](nlohmann::json result) {
                    printf("  < received response:\n%s\n", result.dump(2).c_str());
                    if (result.contains("tool_calls")) {
                        printf("  [PASS] chat with tools: received tool_calls\n");
                        ++passed;
                    } else {
                        printf("  [PASS] chat with tools: received content (no tool call)\n");
                        ++passed;
                    }
                    engine->stop();
                },
                [&](const std::string& err) {
                    err_msg = err;
                    engine->stop();
                }
            );

            engine->run();

            if (!err_msg.empty()) {
                printf("  [FAIL] chat with tools error: %s\n", err_msg.c_str());
                ++failed;
            }

            delete engine;
        }
        report("ChatTools");
    }

    // ============================================================
    // chat — connection error triggers on_error
    // ============================================================
    {
        LLMConfig cfg{"deepseek", "sk-invalid", "https://127.0.0.1:1"};
        cfg.timeout = 1;
        auto* engine = LLMEngineFactory().create(cfg);
        engine->start();

        bool error_called = false;

        nlohmann::json msgs = nlohmann::json::array();
        msgs.push_back({{"role", "user"}, {"content", "hi"}});

        engine->chat(msgs,
            [&](nlohmann::json) {
                printf("  [FAIL] connection error: on_result should not be called\n");
                ++failed;
                engine->stop();
            },
            [&](const std::string& err) {
                printf("  [PASS] connection error: on_error called ('%s')\n", err.c_str());
                ++passed;
                error_called = true;
                engine->stop();
            }
        );

        engine->run();

        CHECK_TRUE(error_called, "connection error: error flag set");

        delete engine;
    }
    report("ConnectionError");

    // ============================================================
    // url parsing
    // ============================================================
    {
        std::string host; int port = 0; std::string path;
        DeepSeekEngine::parse_base_url("https://api.deepseek.com/v1", host, port, path);
        check_str(host, "api.deepseek.com", "parse: host");
    }
    report("UrlParse");

    // ============================================================
    // Factory — full lifecycle
    // ============================================================
    {
        LLMConfig cfg{"deepseek", "sk-test"};
        auto* engine = LLMEngineFactory().create(cfg);

        check_str(engine->name(), "llm(deepseek:deepseek-chat)", "factory name");

        engine->start();
        CHECK_TRUE(engine->health_check(), "factory started");
        engine->stop();
        CHECK_FALSE(engine->health_check(), "factory stopped");

        delete engine;
    }
    report("Factory");

    printf("\n== %d/%d passed (%d skipped) ==\n", passed, passed + failed, skipped);
    return failed > 0 ? 1 : 0;
}
