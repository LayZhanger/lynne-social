#include "wheel/config/imp/json_config_loader.h"

#include <fstream>

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

    Config cfg;
    from_json(j, cfg);
    return cfg;
}

} // namespace wheel
} // namespace lynne
