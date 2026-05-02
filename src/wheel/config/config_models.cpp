#include "wheel/config/config_models.h"

namespace lynne {
namespace wheel {

void from_json(const nlohmann::json& j, ServerConfig& c) {
    c.port = j.value("port", 7890);
    c.auto_open_browser = j.value("auto_open_browser", true);
}

void from_json(const nlohmann::json& j, LLMConfig& c) {
    c.provider = j.value("provider", "deepseek");
    c.api_key = j.value("api_key", "");
    c.base_url = j.value("base_url", "");
    c.model = j.value("model", "deepseek-chat");
    c.temperature = j.value("temperature", 0.7);
    c.max_tokens = j.value("max_tokens", 4096);
    c.timeout = j.value("timeout", 60);
}

void from_json(const nlohmann::json& j, BrowserConfig& c) {
    c.headless = j.value("headless", false);
    c.slow_mo = j.value("slow_mo", 500);
    c.viewport_width = j.value("viewport_width", 1920);
    c.viewport_height = j.value("viewport_height", 1080);
    c.locale = j.value("locale", "zh-CN");
    c.timeout = j.value("timeout", 30000);
}

void from_json(const nlohmann::json& j, PlatformConfig& c) {
    c.enabled = j.value("enabled", false);
    c.session_file = j.value("session_file", "");
    c.base_url = j.value("base_url", "");

    if (j.contains("account") && j.at("account").is_object()) {
        for (auto& [key, val] : j.at("account").items()) {
            c.account[key] = val.get<std::string>();
        }
    }
}

void from_json(const nlohmann::json& j, TaskConfig& c) {
    c.name = j.value("name", "");
    c.intent = j.value("intent", "");
    c.schedule = j.value("schedule", "manual");

    if (j.contains("platforms") && j.at("platforms").is_array()) {
        for (auto& p : j.at("platforms")) {
            c.platforms.push_back(p.get<std::string>());
        }
    }
}

void from_json(const nlohmann::json& j, Config& c) {
    if (j.contains("server") && j.at("server").is_object()) {
        from_json(j.at("server"), c.server);
    }
    if (j.contains("llm") && j.at("llm").is_object()) {
        from_json(j.at("llm"), c.llm);
    }
    if (j.contains("browser") && j.at("browser").is_object()) {
        from_json(j.at("browser"), c.browser);
    }
    if (j.contains("platforms") && j.at("platforms").is_object()) {
        for (auto& [key, val] : j.at("platforms").items()) {
            PlatformConfig pc;
            from_json(val, pc);
            c.platforms[key] = pc;
        }
    }
    if (j.contains("tasks") && j.at("tasks").is_array()) {
        for (auto& t : j.at("tasks")) {
            TaskConfig tc;
            from_json(t, tc);
            c.tasks.push_back(tc);
        }
    }
}

} // namespace wheel
} // namespace lynne
