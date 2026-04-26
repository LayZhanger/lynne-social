#pragma once

#include "wheel/logger/logger.h"
#include "wheel/logger/logger_models.h"

#include <memory>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace lynne {
namespace wheel {

class SpdlogLogger : public Logger {
public:
    explicit SpdlogLogger(const LogConfig& config);
    ~SpdlogLogger() override;

    void start() override;
    void stop() override;
    bool health_check() override;
    std::string name() const override;

    void log(LogLevel level, const std::string& message) override;

private:
    LogConfig config_;
    bool started_ = false;
};

} // namespace wheel
} // namespace lynne
