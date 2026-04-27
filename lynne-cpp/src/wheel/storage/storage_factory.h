#pragma once

#include "wheel/storage/storage.h"
#include "wheel/storage/storage_models.h"

namespace lynne {
namespace wheel {

class StorageFactory {
public:
    Storage* create(const StorageConfig& config) const;
    Storage* create(const char* path) const;
    Storage* create() const;
};

} // namespace wheel
} // namespace lynne
