#include "wheel/storage/storage_factory.h"
#include "wheel/storage/imp/jsonl_storage.h"

namespace lynne {
namespace wheel {

Storage* StorageFactory::create(const StorageConfig& config) const {
    return new JsonlStorage(config.data_dir);
}

Storage* StorageFactory::create(const char* path) const {
    return new JsonlStorage(std::string(path));
}

Storage* StorageFactory::create() const {
    return new JsonlStorage("data");
}

} // namespace wheel
} // namespace lynne
