#include "wheel/config/config_loader.h"
#include "wheel/config/config_models.h"
#include "wheel/config/config_factory.h"
#include "wheel/config/imp/json_config_loader.h"

#include <cstdio>

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

static void check_int(int a, int b, const char* msg) {
    if (a == b) { printf("  [PASS] %s\n", msg); ++passed; }
    else { printf("  [FAIL] %s (%d vs %d)\n", msg, a, b); ++failed; }
}

static void check_str(const std::string& a, const std::string& b, const char* msg) {
    if (a == b) { printf("  [PASS] %s\n", msg); ++passed; }
    else { printf("  [FAIL] %s ('%s' vs '%s')\n", msg, a.c_str(), b.c_str()); ++failed; }
}

static void check_bool(bool a, bool b, const char* msg) {
    if (a == b) { printf("  [PASS] %s\n", msg); ++passed; }
    else { printf("  [FAIL] %s (%d vs %d)\n", msg, a, b); ++failed; }
}

static void check_size(size_t a, size_t b, const char* msg) {
    if (a == b) { printf("  [PASS] %s\n", msg); ++passed; }
    else { printf("  [FAIL] %s (%zu vs %zu)\n", msg, a, b); ++failed; }
}

static void report(const char* suite) {
    printf("--- %s: %d/%d ---\n", suite, passed, passed + failed);
}

static void write_json(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
    f.close();
}

int main() {
    fs::path tmp_dir = fs::temp_directory_path() / "lynne_test_config";
    fs::create_directories(tmp_dir);
    std::string config_path = (tmp_dir / "config.json").string();

    // ============================================================
    // Load missing file returns defaults
    // ============================================================
    {
        fs::path missing = tmp_dir / "nonexistent.json";
        JsonConfigLoader loader(missing.string());
        auto cfg = loader.load();

        check_int(cfg.server.port, 7890, "missing file: port = 7890");
        check_str(cfg.llm.provider, "deepseek", "missing file: provider = deepseek");
        CHECK_FALSE(cfg.browser.headless, "missing file: headless = false");
        CHECK_TRUE(cfg.platforms.empty(), "missing file: platforms empty");
        CHECK_TRUE(cfg.tasks.empty(), "missing file: tasks empty");
    }

    report("MissingFile");

    // ============================================================
    // Load empty file returns defaults
    // ============================================================
    {
        write_json(config_path, "");
        JsonConfigLoader loader(config_path);
        auto cfg = loader.load();
        check_int(cfg.server.port, 7890, "empty file: port = 7890");
        check_str(cfg.llm.provider, "deepseek", "empty file: provider = deepseek");
    }

    report("EmptyFile");

    // ============================================================
    // Load null file returns defaults
    // ============================================================
    {
        write_json(config_path, "null");
        JsonConfigLoader loader(config_path);
        auto cfg = loader.load();
        check_int(cfg.server.port, 7890, "null file: port = 7890");
        check_str(cfg.llm.provider, "deepseek", "null file: provider = deepseek");
    }

    report("NullFile");

    // ============================================================
    // Load valid JSON full structure
    // ============================================================
    {
        write_json(config_path, R"({
            "server": {"port": 8888, "auto_open_browser": false},
            "llm": {"provider": "openai", "api_key": "sk-test", "model": "gpt-4"},
            "browser": {"headless": true, "slow_mo": 200},
            "platforms": {
                "twitter": {
                    "enabled": true,
                    "session_file": "data/sessions/twitter.json",
                    "base_url": "https://x.com"
                }
            },
            "tasks": [
                {
                    "name": "AI\u52a8\u6001",
                    "platforms": ["twitter"],
                    "intent": "\u5173\u6ce8AI\u5927\u6a21\u578b\u7684\u6700\u65b0\u8fdb\u5c55",
                    "schedule": "every 4 hours"
                }
            ]
        })");

        JsonConfigLoader loader(config_path);
        auto cfg = loader.load();

        check_int(cfg.server.port, 8888, "full: port = 8888");
        CHECK_FALSE(cfg.server.auto_open_browser, "full: auto_open_browser = false");
        check_str(cfg.llm.provider, "openai", "full: provider = openai");
        check_str(cfg.llm.model, "gpt-4", "full: model = gpt-4");
        CHECK_TRUE(cfg.browser.headless, "full: headless = true");
        check_int(cfg.browser.slow_mo, 200, "full: slow_mo = 200");

        CHECK_TRUE(cfg.platforms.count("twitter") > 0, "full: platforms has twitter");
        CHECK_TRUE(cfg.platforms["twitter"].enabled, "full: twitter enabled = true");
        check_str(cfg.platforms["twitter"].base_url, "https://x.com", "full: twitter base_url");

        CHECK_TRUE(cfg.tasks.size() == 1u, "full: 1 task");
        check_str(cfg.tasks[0].name, "AI动态", "full: task name");
        check_size(cfg.tasks[0].platforms.size(), 1u, "full: task platforms count");
        check_str(cfg.tasks[0].platforms[0], "twitter", "full: task platform = twitter");
    }

    report("FullStructure");

    // ============================================================
    // Partial JSON preserves defaults
    // ============================================================
    {
        write_json(config_path, R"({"server": {"port": 9999}})");
        JsonConfigLoader loader(config_path);
        auto cfg = loader.load();

        check_int(cfg.server.port, 9999, "partial: port = 9999");
        CHECK_TRUE(cfg.server.auto_open_browser, "partial: auto_open_browser default");
        check_str(cfg.llm.provider, "deepseek", "partial: provider default");
        CHECK_TRUE(cfg.platforms.empty(), "partial: platforms empty");
    }

    report("PartialJson");

    // ============================================================
    // Reload picks up changes
    // ============================================================
    {
        write_json(config_path, R"({"server": {"port": 8888}})");
        JsonConfigLoader loader(config_path);
        auto cfg1 = loader.load();
        check_int(cfg1.server.port, 8888, "reload: first port = 8888");

        write_json(config_path, R"({"server": {"port": 9999}})");
        auto cfg2 = loader.reload();
        check_int(cfg2.server.port, 9999, "reload: second port = 9999");
    }

    report("Reload");

    // ============================================================
    // Config accessor after load
    // ============================================================
    {
        write_json(config_path, R"({"server": {"port": 8888}})");
        JsonConfigLoader loader(config_path);
        loader.load();
        check_int(loader.config().server.port, 8888, "config() accessor after load");
    }

    report("ConfigAccessor");

    // ============================================================
    // Factory loads from file
    // ============================================================
    {
        write_json(config_path, R"({"server": {"port": 7777}})");
        ConfigLoaderFactory factory;
        auto* loader = factory.create(config_path.c_str());
        CHECK_TRUE(loader != nullptr, "factory: non-null");

        auto cfg = loader->load();
        check_int(cfg.server.port, 7777, "factory: port = 7777");
        delete loader;
    }

    report("FactoryFromFile");

    // ============================================================
    // Factory default loads missing file (config.json)
    // ============================================================
    {
        ConfigLoaderFactory factory;
        auto* loader = factory.create();
        CHECK_TRUE(loader != nullptr, "factory default: non-null");

        auto cfg = loader->load();
        check_int(cfg.server.port, 7890, "factory default: port = 7890");
        delete loader;
    }

    report("FactoryDefault");

    // ============================================================
    // config() before load returns default-initialized Config
    // ============================================================
    {
        JsonConfigLoader loader(config_path);
        check_int(loader.config().server.port, 7890, "config() before load: port = 7890");
    }

    report("ConfigBeforeLoad");

    fs::remove_all(tmp_dir);

    printf("\n== %d/%d passed ==\n", passed, passed + failed);
    return failed > 0 ? 1 : 0;
}
