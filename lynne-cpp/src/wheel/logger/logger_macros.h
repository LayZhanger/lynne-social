#pragma once

#include <cstdio>
#include <string>

#include "wheel/logger/logger.h"

namespace lynne {
namespace wheel {

extern Logger* g_logger_ptr;

inline Logger& g_logger() {
    return *g_logger_ptr;
}

} // namespace wheel
} // namespace lynne

#define LOG_DEBUG(module, fmt, ...)                                       \
    do {                                                                  \
        if (::lynne::wheel::g_logger_ptr != nullptr) {                    \
            char _buf[4096];                                              \
            std::snprintf(_buf, sizeof(_buf), "[%s][%s:%d] " fmt,         \
                          (module), __FILE__, __LINE__, ##__VA_ARGS__);   \
            ::lynne::wheel::g_logger().log(                               \
                ::lynne::wheel::LogLevel::Debug, _buf);                   \
        }                                                                 \
    } while (0)

#define LOG_INFO(module, fmt, ...)                                        \
    do {                                                                  \
        if (::lynne::wheel::g_logger_ptr != nullptr) {                    \
            char _buf[4096];                                              \
            std::snprintf(_buf, sizeof(_buf), "[%s][%s:%d] " fmt,         \
                          (module), __FILE__, __LINE__, ##__VA_ARGS__);   \
            ::lynne::wheel::g_logger().log(                               \
                ::lynne::wheel::LogLevel::Info, _buf);                    \
        }                                                                 \
    } while (0)

#define LOG_WARN(module, fmt, ...)                                        \
    do {                                                                  \
        if (::lynne::wheel::g_logger_ptr != nullptr) {                    \
            char _buf[4096];                                              \
            std::snprintf(_buf, sizeof(_buf), "[%s][%s:%d] " fmt,         \
                          (module), __FILE__, __LINE__, ##__VA_ARGS__);   \
            ::lynne::wheel::g_logger().log(                               \
                ::lynne::wheel::LogLevel::Warn, _buf);                    \
        }                                                                 \
    } while (0)

#define LOG_ERROR(module, fmt, ...)                                       \
    do {                                                                  \
        if (::lynne::wheel::g_logger_ptr != nullptr) {                    \
            char _buf[4096];                                              \
            std::snprintf(_buf, sizeof(_buf), "[%s][%s:%d] " fmt,         \
                          (module), __FILE__, __LINE__, ##__VA_ARGS__);   \
            ::lynne::wheel::g_logger().log(                               \
                ::lynne::wheel::LogLevel::Error, _buf);                   \
        }                                                                 \
    } while (0)
