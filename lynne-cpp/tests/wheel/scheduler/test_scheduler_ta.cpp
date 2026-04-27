#include "wheel/scheduler/scheduler.h"
#include "wheel/scheduler/scheduler_models.h"
#include "wheel/scheduler/scheduler_factory.h"
#include "wheel/scheduler/imp/uv_scheduler.h"

#include <cstdio>
#include <cstdlib>
#include <uv.h>
#include <thread>
#include <chrono>
#include <stdexcept>

using namespace lynne::wheel;

static int passed = 0;
static int failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { printf("  [PASS] %s\n", msg); ++passed; } \
         else { printf("  [FAIL] %s\n", msg); ++failed; } \
    } while (0)

#define CHECK_TRUE(cond, msg)  CHECK((cond), msg)
#define CHECK_FALSE(cond, msg) CHECK(!(cond), msg)

static void check_int(int a, int b, const char* msg) {
    if (a == b) { printf("  [PASS] %s\n", msg); ++passed; }
    else { printf("  [FAIL] %s (%d vs %d)\n", msg, a, b); ++failed; }
}

static void report(const char* suite) {
    printf("--- %s: %d/%d ---\n", suite, passed, passed + failed);
}

// pump pending close callbacks on the default loop
static void drain() {
    uv_run(uv_default_loop(), UV_RUN_NOWAIT);
}

int main() {
    // ============================================================
    // Lifecycle: name, health_check, start, stop
    // ============================================================
    {
        SchedulerFactory factory;
        Scheduler* s = factory.create();

        CHECK(s->name() == std::string("UvScheduler"), "name = UvScheduler");
        CHECK_FALSE(s->health_check(), "health_check false before start");

        s->start();
        CHECK_TRUE(s->health_check(), "health_check true after start");

        s->start();
        CHECK_TRUE(s->health_check(), "health_check after double start");

        s->stop();
        drain();
        CHECK_FALSE(s->health_check(), "health_check false after stop");

        Scheduler* s2 = factory.create();
        s2->stop();
        drain();
        CHECK_FALSE(s2->health_check(), "health_check after stop without start");

        delete s;
        delete s2;
        drain();
    }
    report("Lifecycle");

    // ============================================================
    // run_blocking — works on thread pool, on_done on loop, then stop
    // ============================================================
    {
        SchedulerFactory factory;
        Scheduler* s = factory.create();
        s->start();

        int work_result = 0;
        bool done = false;

        s->run_blocking(
            [&]() { work_result = 42; },
            [&, s]() { done = true; s->stop(); }
        );

        uv_run(uv_default_loop(), UV_RUN_DEFAULT);
        drain();

        CHECK_TRUE(done, "run_blocking: on_done called");
        check_int(work_result, 42, "run_blocking: work result = 42");

        delete s;
        drain();
    }
    report("RunBlocking");

    // ============================================================
    // post — callback enqueued and processed on loop
    // ============================================================
    {
        SchedulerFactory factory;
        Scheduler* s = factory.create();
        s->start();

        bool called = false;

        s->post([&]() { called = true; });

        uv_run(uv_default_loop(), UV_RUN_ONCE);

        CHECK_TRUE(called, "post: callback executed");
        CHECK_TRUE(s->health_check(), "post: scheduler still alive");

        s->stop();
        drain();
        delete s;
        drain();
    }
    report("Post");

    // ============================================================
    // add_job — self-removing one-shot timer
    // ============================================================
    {
        SchedulerFactory factory;
        Scheduler* s = factory.create();
        s->start();

        int fires = 0;
        s->add_job("onetimer", 1, [&, s]() {
            fires++;
            s->remove_job("onetimer");
            s->stop();
        });

        uv_run(uv_default_loop(), UV_RUN_DEFAULT);
        drain();

        check_int(fires, 1, "add_job: timer fired exactly once");

        delete s;
        drain();
    }
    report("AddJobOnce");

    // ============================================================
    // add_job — repeating timer, external stop after 50ms
    // ============================================================
    {
        SchedulerFactory factory;
        Scheduler* s = factory.create();
        s->start();

        int fires = 0;
        s->add_job("rep", 1, [&]() { fires++; });

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start).count() < 50) {
            uv_run(uv_default_loop(), UV_RUN_NOWAIT);
        }

        s->remove_job("rep");
        s->stop();
        drain();

        CHECK_TRUE(fires >= 2, "add_job: repeating timer fired >= 2 times");

        delete s;
        drain();
    }
    report("AddJobRepeat");

    // ============================================================
    // remove_job — nonexistent name is safe
    // ============================================================
    {
        SchedulerFactory factory;
        Scheduler* s = factory.create();
        s->start();

        s->remove_job("no-such");
        CHECK_TRUE(s->health_check(), "remove nonexistent: no crash");

        s->stop();
        drain();
        delete s;
        drain();
    }
    report("RemoveNonexistent");

    // ============================================================
    // add_job — overwriting same name
    // ============================================================
    {
        SchedulerFactory factory;
        Scheduler* s = factory.create();
        s->start();

        int a_fires = 0;
        int b_fires = 0;

        s->add_job("same", 10000, [&]() { a_fires++; });
        s->add_job("same", 1, [&, s]() { b_fires++; s->remove_job("same"); s->stop(); });

        uv_run(uv_default_loop(), UV_RUN_DEFAULT);
        drain();

        check_int(a_fires, 0, "overwrite: old callback not called");
        check_int(b_fires, 1, "overwrite: new callback called");

        delete s;
        drain();
    }
    report("AddJobOverwrite");

    // ============================================================
    // Multiple jobs independent
    // ============================================================
    {
        SchedulerFactory factory;
        Scheduler* s = factory.create();
        s->start();

        int a = 0, b = 0;
        s->add_job("a", 1, [&]() { a++; });
        s->add_job("b", 2, [&]() { b++; });

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start).count() < 30) {
            uv_run(uv_default_loop(), UV_RUN_NOWAIT);
        }

        s->remove_job("a");
        s->remove_job("b");
        s->stop();
        drain();

        CHECK_TRUE(a >= 10, "multi-job: timer A fired >= 10 times");
        CHECK_TRUE(b >= 5, "multi-job: timer B fired >= 5 times");

        delete s;
        drain();
    }
    report("MultiJob");

    // ============================================================
    // Callback exception does not crash scheduler
    // ============================================================
    {
        SchedulerFactory factory;
        Scheduler* s = factory.create();
        s->start();

        int good_fires = 0;
        s->add_job("good", 1, [&, s]() {
            good_fires++;
            if (good_fires >= 5) { s->remove_job("good"); s->stop(); }
        });

        s->post([]() { throw std::runtime_error("expected"); });

        uv_run(uv_default_loop(), UV_RUN_DEFAULT);
        drain();

        CHECK_TRUE(good_fires >= 5, "exception: good timer still fired");

        delete s;
        drain();
    }
    report("CallbackException");

    // ============================================================
    // Factory — full lifecycle
    // ============================================================
    {
        SchedulerFactory factory;
        SchedulerConfig cfg{"UTC", 1};
        Scheduler* s = factory.create(cfg);

        CHECK(s->name() == std::string("UvScheduler"), "factory: correct name");
        CHECK_FALSE(s->health_check(), "factory: not started");

        s->start();
        CHECK_TRUE(s->health_check(), "factory: started");

        s->stop();
        drain();
        CHECK_FALSE(s->health_check(), "factory: stopped");

        delete s;
        drain();
    }
    report("Factory");

    // ============================================================
    // Destructor calls stop safely with active jobs
    // ============================================================
    {
        {
            SchedulerFactory factory;
            Scheduler* s = factory.create();
            s->start();
            s->add_job("t", 1000, []() {});
            delete s;
        }
        drain();
    }
    report("DestructorSafe");

    printf("\n== %d/%d passed ==\n", passed, passed + failed);
    return failed > 0 ? 1 : 0;
}
