#pragma once

#include "common/module.h"
#include <functional>
#include <string>
#include <cstdint>

namespace lynne {
namespace wheel {

class Scheduler : public common::Module {
public:
    virtual void run_blocking(
        std::function<void()> work,
        std::function<void()> on_done
    ) = 0;

    virtual void post(std::function<void()> callback) = 0;

    virtual void add_job(
        const std::string& name,
        uint64_t interval_ms,
        std::function<void()> callback
    ) = 0;

    virtual void remove_job(const std::string& name) = 0;

    virtual void after(uint64_t delay_ms,
                       std::function<void()> callback) = 0;

    virtual void step() = 0;
    virtual void run() = 0;
};

} // namespace wheel
} // namespace lynne
