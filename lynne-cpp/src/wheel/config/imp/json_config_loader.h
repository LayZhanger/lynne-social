#pragma once

#include "wheel/config/config_loader.h"
#include <string>

namespace lynne {
namespace wheel {

class JsonConfigLoader : public ConfigLoader {
public:
    explicit JsonConfigLoader(const std::string& path);

    Config load() override;
    Config reload() override;

    const Config& config() const;

private:
    std::string read_file() const;
    Config parse_and_resolve(const std::string& raw) const;

    std::string path_;
    Config config_;
    bool loaded_ = false;
};

void resolve_env_vars(nlohmann::json& j);

} // namespace wheel
} // namespace lynne
