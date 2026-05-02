#include "wheel/storage/storage_models.h"

namespace lynne {
namespace wheel {

void from_json(const nlohmann::json& j, StorageConfig& c) {
    c.data_dir = j.value("data_dir", "data");
}

} // namespace wheel
} // namespace lynne
