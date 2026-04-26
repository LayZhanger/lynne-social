#pragma once

#include "common/module.h"
#include "wheel/logger/logger_models.h"

namespace lynne {
namespace wheel {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error,
};

class Logger : public common::Module {
public:
    virtual void log(LogLevel level, const std::string& message) = 0;
};

} // namespace wheel
} // namespace lynne
