#include "wheel/logger/logger_models.h"
#include "wheel/logger/logger_macros.h"

namespace lynne {
namespace wheel {

Logger* g_logger_ptr = nullptr;

void from_json(const nlohmann::json& j, LogConfig& c) {
    c.level = j.value("level", "INFO");
    c.rotation = j.value("rotation", "10 MB");
    c.retention = j.value("retention", "7 days");
    c.log_file = j.value("log_file", "data/lynne.log");
}

} // namespace wheel
} // namespace lynne
