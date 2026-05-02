#include "wheel/config/config_factory.h"
#include "wheel/config/imp/json_config_loader.h"

namespace lynne {
namespace wheel {

ConfigLoader* ConfigLoaderFactory::create(const char* path) const {
    if (path != nullptr && path[0] != '\0') {
        return new JsonConfigLoader(std::string(path));
    }
    return new JsonConfigLoader("config.json");
}

} // namespace wheel
} // namespace lynne
