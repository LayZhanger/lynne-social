#include "wheel/logger/logger_models.h"
#include "wheel/logger/logger.h"
#include <gtest/gtest.h>

using namespace lynne::wheel;

// ============================================================
// LogConfig — defaults
// ============================================================

TEST(LogConfigDefaults, AllFieldsHaveSaneDefaults) {
    LogConfig c{};
    EXPECT_EQ(c.level, "INFO");
    EXPECT_EQ(c.rotation, "10 MB");
    EXPECT_EQ(c.retention, "7 days");
    EXPECT_EQ(c.log_file, "data/lynne.log");
}

// ============================================================
// LogConfig — from_json
// ============================================================

TEST(LogConfigJson, FromJsonMinimal) {
    auto j = nlohmann::json::parse("{}");
    LogConfig c;
    from_json(j, c);
    EXPECT_EQ(c.level, "INFO");
    EXPECT_EQ(c.log_file, "data/lynne.log");
}

TEST(LogConfigJson, FromJsonAllFields) {
    std::string json = R"({
        "level": "DEBUG",
        "rotation": "5 MB",
        "retention": "30 days",
        "log_file": "/tmp/test.log"
    })";
    auto j = nlohmann::json::parse(json);
    LogConfig c;
    from_json(j, c);

    EXPECT_EQ(c.level, "DEBUG");
    EXPECT_EQ(c.rotation, "5 MB");
    EXPECT_EQ(c.retention, "30 days");
    EXPECT_EQ(c.log_file, "/tmp/test.log");
}

TEST(LogConfigJson, PartialOverride) {
    std::string json = R"({"level": "ERROR"})";
    auto j = nlohmann::json::parse(json);
    LogConfig c;
    from_json(j, c);

    EXPECT_EQ(c.level, "ERROR");
    EXPECT_EQ(c.log_file, "data/lynne.log");  // default preserved
    EXPECT_EQ(c.rotation, "10 MB");           // default preserved
}

// ============================================================
// LogLevel — enum values
// ============================================================

TEST(LogLevelEnum, ValuesAreDistinct) {
    EXPECT_NE(static_cast<int>(LogLevel::Debug), static_cast<int>(LogLevel::Info));
    EXPECT_NE(static_cast<int>(LogLevel::Info), static_cast<int>(LogLevel::Warn));
    EXPECT_NE(static_cast<int>(LogLevel::Warn), static_cast<int>(LogLevel::Error));
}
