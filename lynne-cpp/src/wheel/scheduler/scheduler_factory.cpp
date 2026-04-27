#include "wheel/scheduler/scheduler_factory.h"
#include "wheel/scheduler/imp/uv_scheduler.h"

namespace lynne {
namespace wheel {

Scheduler* SchedulerFactory::create(uv_loop_t* loop, const SchedulerConfig& config) const {
    return new UvScheduler(loop, config);
}

Scheduler* SchedulerFactory::create(uv_loop_t* loop) const {
    return new UvScheduler(loop, SchedulerConfig{});
}

} // namespace wheel
} // namespace lynne
