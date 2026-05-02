#pragma once

#include <json.hpp>
#include <string>

namespace lynne {
namespace wheel {

struct LLMConfig {
    std::string provider = "deepseek";
    std::string api_key;
    std::string base_url;
    std::string model = "deepseek-chat";
    double temperature = 0.7;
    int max_tokens = 4096;
    int timeout = 60;
    std::string ca_cert_path;
};

void from_json(const nlohmann::json& j, LLMConfig& c);

} // namespace wheel
} // namespace lynne
