#pragma once

#include <json.hpp>
#include <string>

namespace lynne {
namespace wheel {

struct StorageConfig {
    std::string data_dir = "data";
};

void from_json(const nlohmann::json& j, StorageConfig& c);

} // namespace wheel
} // namespace lynne
