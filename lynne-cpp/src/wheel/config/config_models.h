#pragma once

#include <json.hpp>
#include <string>
#include <vector>
#include <map>

namespace lynne {
namespace wheel {

struct ServerConfig {
    int port = 7890;
    bool auto_open_browser = true;
};

struct LLMConfig {
    std::string provider = "deepseek";
    std::string api_key;
    std::string base_url;
    std::string model = "deepseek-chat";
    double temperature = 0.7;
    int max_tokens = 4096;
    int timeout = 60;
};

struct BrowserConfig {
    bool headless = false;
    int slow_mo = 500;
    int viewport_width = 1920;
    int viewport_height = 1080;
    std::string locale = "zh-CN";
    int timeout = 30000;
};

struct PlatformConfig {
    bool enabled = false;
    std::string session_file;
    std::string base_url;
    std::map<std::string, std::string> account;
};

struct TaskConfig {
    std::string name;
    std::vector<std::string> platforms;
    std::string intent;
    std::string schedule = "manual";
};

struct Config {
    ServerConfig server;
    LLMConfig llm;
    BrowserConfig browser;
    std::map<std::string, PlatformConfig> platforms;
    std::vector<TaskConfig> tasks;
};

void from_json(const nlohmann::json& j, ServerConfig& c);
void from_json(const nlohmann::json& j, LLMConfig& c);
void from_json(const nlohmann::json& j, BrowserConfig& c);
void from_json(const nlohmann::json& j, PlatformConfig& c);
void from_json(const nlohmann::json& j, TaskConfig& c);
void from_json(const nlohmann::json& j, Config& c);

} // namespace wheel
} // namespace lynne
