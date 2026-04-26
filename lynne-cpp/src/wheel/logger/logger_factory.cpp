#include "wheel/logger/logger_factory.h"
#include "wheel/logger/imp/spdlog_logger.h"

namespace lynne {
namespace wheel {

Logger* LoggerFactory::create(const LogConfig& config) {
    return new SpdlogLogger(config);
}

} // namespace wheel
} // namespace lynne
