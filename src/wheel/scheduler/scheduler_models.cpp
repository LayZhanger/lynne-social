#include "wheel/scheduler/scheduler_models.h"

namespace lynne {
namespace wheel {

void from_json(const nlohmann::json& j, SchedulerConfig& c) {
    c.timezone = j.value("timezone", "Asia/Shanghai");
    c.max_workers = j.value("max_workers", 4);
}

} // namespace wheel
} // namespace lynne
