#pragma once

#include "wheel/scheduler/scheduler.h"
#include "wheel/scheduler/scheduler_models.h"

#include <uv.h>
#include <mutex>
#include <vector>
#include <map>
#include <string>

namespace lynne {
namespace wheel {

class UvScheduler : public Scheduler {
public:
    explicit UvScheduler(const SchedulerConfig& config);
    ~UvScheduler() override;

    std::string name() const override;
    void start() override;
    void stop() override;
    bool health_check() override;

    void run_blocking(
        std::function<void()> work,
        std::function<void()> on_done
    ) override;
    void post(std::function<void()> callback) override;
    void add_job(
        const std::string& name,
        uint64_t interval_ms,
        std::function<void()> callback
    ) override;
    void remove_job(const std::string& name) override;

private:
    struct WorkCtx {
        std::function<void()> work;
        std::function<void()> on_done;
    };

    struct TimerCtx {
        std::string name;
        std::function<void()> callback;
        std::map<std::string, TimerCtx*>* owner;
        uv_timer_t handle;
    };

    void drain_post_queue();

    static void work_cb(uv_work_t* req);
    static void after_work_cb(uv_work_t* req, int status);
    static void async_cb(uv_async_t* handle);
    static void timer_cb(uv_timer_t* handle);
    static void timer_close_cb(uv_handle_t* handle);

    SchedulerConfig config_;
    bool started_ = false;

    uv_async_t async_handle_;
    bool async_inited_ = false;
    std::mutex post_mutex_;
    std::vector<std::function<void()>> post_queue_;

    std::map<std::string, TimerCtx*> timers_;
};

} // namespace wheel
} // namespace lynne
