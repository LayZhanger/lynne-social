#pragma once

#include "wheel/logger/logger.h"
#include "wheel/logger/logger_models.h"

namespace lynne {
namespace wheel {

class LoggerFactory {
public:
    Logger* create(const LogConfig& config);
};

} // namespace wheel
} // namespace lynne
