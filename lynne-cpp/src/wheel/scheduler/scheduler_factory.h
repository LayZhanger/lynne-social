#pragma once

#include "wheel/scheduler/scheduler.h"
#include "wheel/scheduler/scheduler_models.h"

#include <uv.h>

namespace lynne {
namespace wheel {

class SchedulerFactory {
public:
    Scheduler* create(uv_loop_t* loop, const SchedulerConfig& config) const;
    Scheduler* create(uv_loop_t* loop) const;
};

} // namespace wheel
} // namespace lynne
