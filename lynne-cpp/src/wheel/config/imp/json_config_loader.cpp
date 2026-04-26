#include "wheel/config/imp/json_config_loader.h"

#include <fstream>
#include <regex>
#include <cstdlib>
#include <stdexcept>

namespace lynne {
namespace wheel {

JsonConfigLoader::JsonConfigLoader(const std::string& path)
    : path_(path) {}

Config JsonConfigLoader::load() {
    loaded_ = true;
    auto raw = read_file();
    config_ = parse_and_resolve(raw);
    return config_;
}

Config JsonConfigLoader::reload() {
    return load();
}

const Config& JsonConfigLoader::config() const {
    return config_;
}

std::string JsonConfigLoader::read_file() const {
    std::ifstream f(path_);
    if (!f.is_open()) {
        return "";
    }
    std::string content(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());
    return content;
}

Config JsonConfigLoader::parse_and_resolve(const std::string& raw) const {
    if (raw.empty()) {
        return Config{};
    }

    auto j = nlohmann::json::parse(raw);
    if (j.is_null()) {
        return Config{};
    }

    resolve_env_vars(j);

    Config cfg;
    from_json(j, cfg);
    return cfg;
}

void resolve_env_vars(nlohmann::json& j) {
    if (j.is_string()) {
        std::string s = j.get<std::string>();
        std::regex re(R"(\$\{(\w+)\})");
        std::smatch match;
        while (std::regex_search(s, match, re)) {
            auto var_name = match[1].str();
            auto* env_val = std::getenv(var_name.c_str());
            if (env_val == nullptr || env_val[0] == '\0') {
                throw std::runtime_error("Environment variable " + var_name + " not set");
            }
            s.replace(match.position(), match.length(), env_val);
        }
        j = s;
    } else if (j.is_object()) {
        for (auto& [key, val] : j.items()) {
            resolve_env_vars(val);
        }
    } else if (j.is_array()) {
        for (auto& el : j) {
            resolve_env_vars(el);
        }
    }
}

} // namespace wheel
} // namespace lynne
