#include "wheel/browser/browser_models.h"

namespace lynne {
namespace wheel {

void to_json(nlohmann::json& j, const BrowserConfig& cfg) {
    j = nlohmann::json{
        {"headless", cfg.headless},
        {"slow_mo_ms", cfg.slow_mo_ms},
        {"viewport_width", cfg.viewport_width},
        {"viewport_height", cfg.viewport_height},
        {"locale", cfg.locale},
        {"timeout_ms", cfg.timeout_ms},
        {"sessions_dir", cfg.sessions_dir},
        {"cdp_port", cfg.cdp_port},
        {"chrome_path", cfg.chrome_path}
    };
}

void from_json(const nlohmann::json& j, BrowserConfig& cfg) {
    cfg.headless = j.value("headless", true);
    cfg.slow_mo_ms = j.value("slow_mo_ms", 500);
    cfg.viewport_width = j.value("viewport_width", 1920);
    cfg.viewport_height = j.value("viewport_height", 1080);
    cfg.locale = j.value("locale", "zh-CN");
    cfg.timeout_ms = j.value("timeout_ms", 30000);
    cfg.sessions_dir = j.value("sessions_dir", "data/sessions");
    cfg.cdp_port = j.value("cdp_port", 0);
    cfg.chrome_path = j.value("chrome_path", "");
}

} // namespace wheel
} // namespace lynne
