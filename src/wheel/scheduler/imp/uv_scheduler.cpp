#include "wheel/scheduler/imp/uv_scheduler.h"

namespace lynne {
namespace wheel {

UvScheduler::UvScheduler(const SchedulerConfig& config)
    : config_(config) {}

UvScheduler::~UvScheduler() {
    stop();
}

std::string UvScheduler::name() const {
    return "UvScheduler";
}

void UvScheduler::start() {
    if (started_) return;

    uv_async_init(uv_default_loop(), &async_handle_, async_cb);
    async_handle_.data = this;
    async_inited_ = true;
    started_ = true;
}

void UvScheduler::stop() {
    if (!started_) return;
    started_ = false;

    for (auto& [name, ctx] : timers_) {
        uv_timer_stop(&ctx->handle);
        ctx->owner = nullptr;
        uv_close((uv_handle_t*)&ctx->handle, timer_close_cb);
    }
    timers_.clear();

    if (async_inited_) {
        uv_close((uv_handle_t*)&async_handle_, nullptr);
        async_inited_ = false;
    }
}

bool UvScheduler::health_check() {
    return started_;
}

void UvScheduler::run_blocking(
    std::function<void()> work,
    std::function<void()> on_done
) {
    auto* req = new uv_work_t;
    auto* ctx = new WorkCtx{std::move(work), std::move(on_done)};
    req->data = ctx;
    uv_queue_work(uv_default_loop(), req, work_cb, after_work_cb);
}

void UvScheduler::post(std::function<void()> callback) {
    {
        std::lock_guard<std::mutex> lock(post_mutex_);
        post_queue_.push_back(std::move(callback));
    }
    uv_async_send(&async_handle_);
}

void UvScheduler::add_job(
    const std::string& name,
    uint64_t interval_ms,
    std::function<void()> callback
) {
    remove_job(name);

    auto* ctx = new TimerCtx{name, std::move(callback), &timers_, {}};
    uv_timer_init(uv_default_loop(), &ctx->handle);
    ctx->handle.data = ctx;

    auto* named_ctx = ctx;
    timers_[name] = named_ctx;

    uv_timer_start(&ctx->handle, timer_cb, interval_ms, interval_ms);
}

void UvScheduler::remove_job(const std::string& name) {
    auto it = timers_.find(name);
    if (it != timers_.end()) {
        auto* ctx = it->second;
        timers_.erase(it);
        uv_timer_stop(&ctx->handle);
        ctx->owner = nullptr;
        uv_close((uv_handle_t*)&ctx->handle, timer_close_cb);
    }
}

void UvScheduler::after(uint64_t delay_ms,
                         std::function<void()> callback) {
    // Reuse add_job, but make it oneshot (fires once then auto-removes)
    static std::atomic<int> counter{0};
    std::string name = "_a_" + std::to_string(++counter);
    remove_job(name);
    auto* ctx = new TimerCtx{name, std::move(callback), &timers_, {}};
    uv_timer_init(uv_default_loop(), &ctx->handle);
    ctx->handle.data = ctx;
    timers_[name] = ctx;
    uv_timer_start(&ctx->handle, oneshot_timer_cb, delay_ms, 0);
}

void UvScheduler::step() {
    uv_run(uv_default_loop(), UV_RUN_ONCE);
}

void UvScheduler::run() {
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

void UvScheduler::drain_post_queue() {
    std::vector<std::function<void()>> pending;
    {
        std::lock_guard<std::mutex> lock(post_mutex_);
        pending.swap(post_queue_);
    }
    for (auto& cb : pending) {
        try {
            cb();
        } catch (...) {}
    }
}

// ---- static callbacks ----

void UvScheduler::work_cb(uv_work_t* req) {
    auto* ctx = static_cast<WorkCtx*>(req->data);
    if (ctx->work) {
        try {
            ctx->work();
        } catch (...) {}
    }
}

void UvScheduler::after_work_cb(uv_work_t* req, int status) {
    auto* ctx = static_cast<WorkCtx*>(req->data);
    if (status == 0 && ctx->on_done) {
        try {
            ctx->on_done();
        } catch (...) {}
    }
    delete ctx;
    delete req;
}

void UvScheduler::async_cb(uv_async_t* handle) {
    auto* self = static_cast<UvScheduler*>(handle->data);
    self->drain_post_queue();
}

void UvScheduler::timer_cb(uv_timer_t* handle) {
    auto* ctx = static_cast<TimerCtx*>(handle->data);
    if (ctx->callback) {
        try {
            ctx->callback();
        } catch (...) {}
    }
}

void UvScheduler::oneshot_timer_cb(uv_timer_t* handle) {
    auto* ctx = static_cast<TimerCtx*>(handle->data);
    if (ctx->callback) {
        try { ctx->callback(); } catch (...) {}
    }
    uv_timer_stop(handle);
    // Self-remove from owner map
    if (ctx->owner) ctx->owner->erase(ctx->name);
    uv_close((uv_handle_t*)handle, timer_close_cb);
}

void UvScheduler::timer_close_cb(uv_handle_t* handle) {
    auto* ctx = static_cast<TimerCtx*>(handle->data);
    delete ctx;
}

} // namespace wheel
} // namespace lynne
