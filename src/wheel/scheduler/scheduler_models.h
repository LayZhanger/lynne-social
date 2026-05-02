#pragma once

#include <json.hpp>
#include <string>

namespace lynne {
namespace wheel {

struct SchedulerConfig {
    std::string timezone = "Asia/Shanghai";
    int max_workers = 4;
};

void from_json(const nlohmann::json& j, SchedulerConfig& c);

} // namespace wheel
} // namespace lynne
