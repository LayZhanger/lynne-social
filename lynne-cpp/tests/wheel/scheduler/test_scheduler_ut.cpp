#include "wheel/scheduler/scheduler_models.h"
#include "wheel/scheduler/scheduler.h"
#include "wheel/scheduler/scheduler_factory.h"
#include "wheel/scheduler/imp/uv_scheduler.h"

#include <gtest/gtest.h>

using namespace lynne::wheel;

// ============================================================
// SchedulerConfig — defaults
// ============================================================

TEST(SchedulerConfigDefaults, DefaultTimezone) {
    SchedulerConfig c{};
    EXPECT_EQ(c.timezone, "Asia/Shanghai");
    EXPECT_EQ(c.max_workers, 4);
}

TEST(SchedulerConfigDefaults, CustomValues) {
    SchedulerConfig c{"UTC", 8};
    EXPECT_EQ(c.timezone, "UTC");
    EXPECT_EQ(c.max_workers, 8);
}

// ============================================================
// SchedulerConfig — from_json
// ============================================================

TEST(SchedulerConfigJson, FromJsonMinimal) {
    auto j = nlohmann::json::parse("{}");
    SchedulerConfig c;
    from_json(j, c);
    EXPECT_EQ(c.timezone, "Asia/Shanghai");
    EXPECT_EQ(c.max_workers, 4);
}

TEST(SchedulerConfigJson, FromJsonCustom) {
    auto j = nlohmann::json::parse(R"({"timezone": "UTC", "max_workers": 2})");
    SchedulerConfig c;
    from_json(j, c);
    EXPECT_EQ(c.timezone, "UTC");
    EXPECT_EQ(c.max_workers, 2);
}

// ============================================================
// SchedulerFactory
// ============================================================

TEST(SchedulerFactory, CreateWithConfig) {
    SchedulerFactory factory;
    SchedulerConfig cfg{"UTC", 1};
    auto* s = factory.create(cfg);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->name(), "UvScheduler");
    delete s;
}

TEST(SchedulerFactory, CreateDefault) {
    SchedulerFactory factory;
    auto* s = factory.create();
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->name(), "UvScheduler");
    delete s;
}
