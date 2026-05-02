#include "wheel/scheduler/scheduler_factory.h"
#include "wheel/scheduler/imp/uv_scheduler.h"

namespace lynne {
namespace wheel {

Scheduler* SchedulerFactory::create(const SchedulerConfig& config) const {
    return new UvScheduler(config);
}

Scheduler* SchedulerFactory::create() const {
    return new UvScheduler(SchedulerConfig{});
}

} // namespace wheel
} // namespace lynne
