#pragma once

#include "wheel/scheduler/scheduler.h"
#include "wheel/scheduler/scheduler_models.h"

namespace lynne {
namespace wheel {

class SchedulerFactory {
public:
    Scheduler* create(const SchedulerConfig& config) const;
    Scheduler* create() const;
};

} // namespace wheel
} // namespace lynne
