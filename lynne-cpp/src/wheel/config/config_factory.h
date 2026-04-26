#pragma once

#include "wheel/config/config_loader.h"

namespace lynne {
namespace wheel {

class ConfigLoaderFactory {
public:
    ConfigLoader* create(const char* path = nullptr) const;
};

} // namespace wheel
} // namespace lynne
