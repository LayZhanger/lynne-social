#pragma once

#include "wheel/config/config_models.h"

namespace lynne {
namespace wheel {

class ConfigLoader {
public:
    virtual Config load() = 0;
    virtual Config reload() = 0;
    virtual ~ConfigLoader() = default;
};

} // namespace wheel
} // namespace lynne
