#include "wheel/logger/logger.h"
#include "wheel/logger/logger_models.h"
#include "wheel/logger/logger_factory.h"
#include "wheel/logger/imp/spdlog_logger.h"
#include "wheel/logger/logger_macros.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <string>

using namespace lynne::wheel;
namespace fs = std::filesystem;

static int passed = 0;
static int failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { printf("  [PASS] %s\n", msg); ++passed; } \
         else { printf("  [FAIL] %s\n", msg); ++failed; } \
    } while (0)

#define CHECK_TRUE(cond, msg)  CHECK((cond), msg)
#define CHECK_FALSE(cond, msg) CHECK(!(cond), msg)

static void check_str(const std::string& a, const std::string& b, const char* msg) {
    if (a == b) { printf("  [PASS] %s\n", msg); ++passed; }
    else { printf("  [FAIL] %s ('%s' vs '%s')\n", msg, a.c_str(), b.c_str()); ++failed; }
}

static void report(const char* suite) {
    printf("--- %s: %d/%d ---\n", suite, passed, passed + failed);
}

int main() {
    // ============================================================
    // TA — Full SpdlogLogger lifecycle
    // ============================================================

    fs::path tmp_dir = fs::temp_directory_path() / "lynne_test_logger";
    fs::create_directories(tmp_dir);

    std::string log_file_path = (tmp_dir / "test.log").string();

    LogConfig config{};
    config.log_file = log_file_path;
    config.level = "DEBUG";

    LoggerFactory factory;
    Logger* logger = factory.create(config);

    // Name
    check_str(logger->name(), "SpdlogLogger", "logger name");

    // HealthCheck before start
    CHECK_FALSE(logger->health_check(), "health_check before start");

    // Start and health_check
    logger->start();
    CHECK_TRUE(logger->health_check(), "health_check after start");

    // Log after start writes to file
    logger->log(LogLevel::Info, "[test] hello world");
    logger->log(LogLevel::Debug, "[test] debug msg");
    logger->log(LogLevel::Warn, "[test] warning msg");
    logger->log(LogLevel::Error, "[test] error msg");

    logger->stop();

    {
        std::ifstream f(log_file_path);
        CHECK_TRUE(f.is_open(), "log file is openable");

        std::string content(
            (std::istreambuf_iterator<char>(f)),
            std::istreambuf_iterator<char>());

        CHECK_TRUE(content.find("[test] hello world") != std::string::npos, "log contains hello");
        CHECK_TRUE(content.find("[test] debug msg") != std::string::npos, "log contains debug");
        CHECK_TRUE(content.find("[test] warning msg") != std::string::npos, "log contains warn");
        CHECK_TRUE(content.find("[test] error msg") != std::string::npos, "log contains error");
    }

    report("LogWrite");

    // HealthCheck false after stop
    CHECK_FALSE(logger->health_check(), "health_check after stop");

    // Double start does not crash
    logger->start();
    logger->start();
    CHECK_TRUE(logger->health_check(), "health_check after double start");
    logger->stop();

    report("DoubleStart");

    // Log before start does not crash
    {
        LoggerFactory f2;
        LogConfig cfg{};
        cfg.log_file = (tmp_dir / "test2.log").string();
        Logger* l2 = f2.create(cfg);
        l2->log(LogLevel::Info, "[test] should not crash");
        CHECK_FALSE(l2->health_check(), "health_check false (never started)");
        delete l2;
    }

    report("BeforeStart");

    // Log after stop does not crash
    {
        LoggerFactory f3;
        LogConfig cfg{};
        cfg.log_file = (tmp_dir / "test3.log").string();
        Logger* l3 = f3.create(cfg);
        l3->start();
        l3->stop();
        l3->log(LogLevel::Info, "[test] should not crash after stop");
        delete l3;
    }

    report("AfterStop");

    // Log file created
    {
        LoggerFactory f4;
        LogConfig cfg{};
        std::string path = (tmp_dir / "test4.log").string();
        cfg.log_file = path;
        Logger* l4 = f4.create(cfg);
        l4->start();
        l4->log(LogLevel::Info, "[test] content");
        l4->stop();

        CHECK_TRUE(fs::exists(path), "log file created on disk");
        auto size = fs::file_size(path);
        CHECK_TRUE(size > 0u, "log file has content (>0 bytes)");
        delete l4;
    }

    report("FileCreated");

    // Factory returns valid logger
    CHECK_TRUE(logger != nullptr, "factory returns non-null");

    // Factory default config
    {
        LogConfig cfg{};
        LoggerFactory f5;
        Logger* l5 = f5.create(cfg);
        CHECK_TRUE(l5 != nullptr, "factory default config non-null");
        check_str(l5->name(), "SpdlogLogger", "factory default config name");
        delete l5;
    }

    report("Factory");

    delete logger;
    fs::remove_all(tmp_dir);

    printf("\n== %d/%d passed ==\n", passed, passed + failed);
    return failed > 0 ? 1 : 0;
}
