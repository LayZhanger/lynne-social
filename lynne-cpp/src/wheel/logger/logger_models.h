#pragma once

#include <json.hpp>
#include <string>

namespace lynne {
namespace wheel {

struct LogConfig {
    std::string level = "INFO";
    std::string rotation = "10 MB";
    std::string retention = "7 days";
    std::string log_file = "data/lynne.log";
};

void from_json(const nlohmann::json& j, LogConfig& c);

} // namespace wheel
} // namespace lynne
