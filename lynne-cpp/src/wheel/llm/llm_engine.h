#pragma once

#include "common/module.h"
#include <json.hpp>
#include <functional>

namespace lynne {
namespace wheel {

class LLMEngine : public common::Module {
public:
    virtual void chat(
        const nlohmann::json& messages,
        std::function<void(nlohmann::json)> on_result,
        std::function<void(const std::string&)> on_error,
        const nlohmann::json& tools = nullptr
    ) = 0;

    virtual void step() = 0;
    virtual void run() = 0;
};

} // namespace wheel
} // namespace lynne
