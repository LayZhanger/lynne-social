#pragma once

#include <string>
#include <json.hpp>

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

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BrowserConfig, headless, slow_mo_ms,
    viewport_width, viewport_height, locale, timeout_ms, sessions_dir,
    cdp_port, chrome_path)

} // namespace wheel
} // namespace lynne
