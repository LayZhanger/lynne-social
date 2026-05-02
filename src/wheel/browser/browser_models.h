#pragma once

#include <json.hpp>

#include <string>

namespace lynne {
namespace wheel {

struct BrowserConfig {
    bool headless = true;
    int slow_mo_ms = 500;
    int viewport_width = 1920;
    int viewport_height = 1080;
    std::string locale = "zh-CN";
    int timeout_ms = 30000;
    std::string sessions_dir = "data/sessions";
    int cdp_port = 0;
    std::string chrome_path;
};

void to_json(nlohmann::json& j, const BrowserConfig& cfg);
void from_json(const nlohmann::json& j, BrowserConfig& cfg);

} // namespace wheel
} // namespace lynne
