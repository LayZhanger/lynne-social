#include "wheel/logger/imp/spdlog_logger.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <cctype>

namespace lynne {
namespace wheel {

SpdlogLogger::SpdlogLogger(const LogConfig& config)
    : config_(config) {}

SpdlogLogger::~SpdlogLogger() {
    stop();
}

void SpdlogLogger::start() {
    if (started_) return;

    std::vector<spdlog::sink_ptr> sinks;

    auto console_sink =
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("%H:%M:%S | %^%-5l%$ | %v");
    sinks.push_back(console_sink);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        config_.log_file, true);
    file_sink->set_pattern("%H:%M:%S | %^%-5l%$ | %v");
    sinks.push_back(file_sink);

    auto logger = std::make_shared<spdlog::logger>(
        "lynne", sinks.begin(), sinks.end());

    auto level_str = config_.level;
    for (auto& c : level_str) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto level = spdlog::level::from_str(level_str);
    logger->set_level(level);
    logger->flush_on(level);

    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);

    started_ = true;
}

void SpdlogLogger::stop() {
    if (started_) {
        auto logger = spdlog::get("lynne");
        if (logger) {
            logger->flush();
        }
        spdlog::drop("lynne");
        started_ = false;
    }
}

bool SpdlogLogger::health_check() {
    return started_ && spdlog::get("lynne") != nullptr;
}

std::string SpdlogLogger::name() const {
    return "SpdlogLogger";
}

void SpdlogLogger::log(LogLevel level, const std::string& message) {
    if (!started_) return;

    auto logger = spdlog::get("lynne");
    if (!logger) return;

    spdlog::level::level_enum spd_level;
    switch (level) {
        case LogLevel::Debug: spd_level = spdlog::level::debug; break;
        case LogLevel::Info:  spd_level = spdlog::level::info;  break;
        case LogLevel::Warn:  spd_level = spdlog::level::warn;  break;
        case LogLevel::Error: spd_level = spdlog::level::err;   break;
        default:              spd_level = spdlog::level::info;  break;
    }

    logger->log(spd_level, "{}", message);
}

} // namespace wheel
} // namespace lynne
