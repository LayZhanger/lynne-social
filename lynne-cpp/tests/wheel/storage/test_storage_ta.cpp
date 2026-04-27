#include "wheel/storage/storage.h"
#include "wheel/storage/storage_models.h"
#include "wheel/storage/storage_factory.h"
#include "wheel/storage/imp/jsonl_storage.h"
#include "common/models.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

using namespace lynne::wheel;
using namespace lynne::common;
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

static void check_size(size_t a, size_t b, const char* msg) {
    if (a == b) { printf("  [PASS] %s\n", msg); ++passed; }
    else { printf("  [FAIL] %s (%zu vs %zu)\n", msg, a, b); ++failed; }
}

static void report(const char* suite) {
    printf("--- %s: %d/%d ---\n", suite, passed, passed + failed);
}

static std::vector<UnifiedItem> make_sample_items() {
    UnifiedItem i1{};
    i1.platform = "twitter";
    i1.item_id = "1";
    i1.author_name = "Alice";
    i1.content_text = "Hello";

    UnifiedItem i2{};
    i2.platform = "rednote";
    i2.item_id = "2";
    i2.author_name = "Bob";
    i2.content_text = "nihao";

    UnifiedItem i3{};
    i3.platform = "twitter";
    i3.item_id = "3";
    i3.author_name = "Charlie";
    i3.content_text = "Bonjour";

    return {i1, i2, i3};
}

int main() {
    fs::path tmp_dir = fs::temp_directory_path() / "lynne_test_storage";
    fs::create_directories(tmp_dir);
    std::string data_dir = (tmp_dir / "data").string();

    auto items = make_sample_items();

    // ============================================================
    // Name
    // ============================================================
    {
        JsonlStorage storage(data_dir);
        check_str(storage.name(), "JsonlStorage", "name");
    }
    report("Name");

    // ============================================================
    // Start creates directory
    // ============================================================
    {
        std::string d = (tmp_dir / "start_test").string();
        JsonlStorage storage(d);
        CHECK_FALSE(storage.health_check(), "health_check false before start");
        storage.start();
        CHECK_TRUE(storage.health_check(), "health_check true after start");
    }
    report("StartCreatesDir");

    // ============================================================
    // Start idempotent
    // ============================================================
    {
        JsonlStorage storage(data_dir);
        storage.start();
        storage.start();
        CHECK_TRUE(storage.health_check(), "health_check after double start");
    }
    report("StartIdempotent");

    // ============================================================
    // Health check false before start
    // ============================================================
    {
        std::string d = (tmp_dir / "no_init").string();
        JsonlStorage storage(d);
        CHECK_FALSE(storage.health_check(), "health_check false before start");
    }
    report("HealthBeforeStart");

    // ============================================================
    // Save and load items
    // ============================================================
    {
        JsonlStorage storage(data_dir);
        storage.start();
        storage.save_items(items, "2026-01-01");
        auto loaded = storage.load_items("2026-01-01");
        check_size(loaded.size(), 3u, "save & load 3 items");
    }
    report("SaveLoadItems");

    // ============================================================
    // Load items empty dir (no file)
    // ============================================================
    {
        JsonlStorage storage(data_dir);
        storage.start();
        auto loaded = storage.load_items("2099-01-01");
        CHECK_TRUE(loaded.empty(), "load from missing date = empty");
    }
    report("LoadEmptyDir");

    // ============================================================
    // Load items filter by platform
    // ============================================================
    {
        std::string d = (tmp_dir / "filter_test").string();
        JsonlStorage storage(d);
        storage.start();
        storage.save_items(items, "2026-01-01");
        auto tw = storage.load_items("2026-01-01", "twitter");
        check_size(tw.size(), 2u, "filter: 2 twitter items");
        CHECK_TRUE(tw[0].platform == "twitter" && tw[1].platform == "twitter", "filter: both twitter");
        auto rn = storage.load_items("2026-01-01", "rednote");
        check_size(rn.size(), 1u, "filter: 1 rednote item");
    }
    report("LoadFilterPlatform");

    // ============================================================
    // Save items append
    // ============================================================
    {
        std::string d = (tmp_dir / "append_test").string();
        JsonlStorage storage(d);
        storage.start();
        storage.save_items({items[0]}, "2026-01-01");
        storage.save_items({items[1], items[2]}, "2026-01-01");
        auto loaded = storage.load_items("2026-01-01");
        check_size(loaded.size(), 3u, "append: 3 total items");
    }
    report("SaveItemsAppend");

    // ============================================================
    // Load items filters nonexistent platform
    // ============================================================
    {
        std::string d = (tmp_dir / "nonexist_platform").string();
        JsonlStorage storage(d);
        storage.start();
        storage.save_items(items, "2026-01-01");
        auto fb = storage.load_items("2026-01-01", "facebook");
        CHECK_TRUE(fb.empty(), "filter nonexistent platform = empty");
    }
    report("FilterNonexistentPlatform");

    // ============================================================
    // Save and load report
    // ============================================================
    {
        std::string d = (tmp_dir / "report_test").string();
        JsonlStorage storage(d);
        storage.start();
        storage.save_report("# Hello World", "2026-01-01");
        auto report = storage.load_report("2026-01-01");
        check_str(report, "# Hello World", "save & load report");
    }
    report("ReportSaveLoad");

    // ============================================================
    // Load report missing
    // ============================================================
    {
        JsonlStorage storage(data_dir);
        storage.start();
        auto report = storage.load_report("2099-01-01");
        CHECK_TRUE(report.empty(), "load missing report = empty");
    }
    report("ReportMissing");

    // ============================================================
    // Save report overwrite
    // ============================================================
    {
        std::string d = (tmp_dir / "overwrite_test").string();
        JsonlStorage storage(d);
        storage.start();
        storage.save_report("# v1", "2026-01-01");
        storage.save_report("# v2", "2026-01-01");
        auto report = storage.load_report("2026-01-01");
        check_str(report, "# v2", "report overwrite");
    }
    report("ReportOverwrite");

    // ============================================================
    // Save and load summary
    // ============================================================
    {
        std::string d = (tmp_dir / "summary_test").string();
        JsonlStorage storage(d);
        storage.start();
        nlohmann::json summary;
        summary["count"] = 5;
        summary["platforms"] = {"twitter", "rednote"};
        storage.save_summary(summary, "2026-01-01");
        auto loaded = storage.load_summary("2026-01-01");
        CHECK_TRUE(loaded["count"] == 5, "summary count = 5");
        CHECK_TRUE(loaded["platforms"].size() == 2u, "summary 2 platforms");
        CHECK_TRUE(loaded["platforms"][0] == "twitter", "summary platform[0] = twitter");
    }
    report("SummarySaveLoad");

    // ============================================================
    // Load summary missing
    // ============================================================
    {
        JsonlStorage storage(data_dir);
        storage.start();
        auto summary = storage.load_summary("2099-01-01");
        CHECK_TRUE(summary.is_null(), "load missing summary = null");
    }
    report("SummaryMissing");

    // ============================================================
    // List dates empty
    // ============================================================
    {
        std::string d = (tmp_dir / "empty_dates").string();
        JsonlStorage storage(d);
        storage.start();
        auto dates = storage.list_dates();
        CHECK_TRUE(dates.empty(), "list_dates empty initially");
    }
    report("ListDatesEmpty");

    // ============================================================
    // List dates — reverse chronological order
    // ============================================================
    {
        std::string d = (tmp_dir / "dates_order").string();
        JsonlStorage storage(d);
        storage.start();
        storage.save_items(items, "2026-01-01");
        storage.save_items(items, "2026-01-03");
        storage.save_items(items, "2026-01-02");
        auto dates = storage.list_dates();
        check_size(dates.size(), 3u, "list_dates 3 dates");
        check_str(dates[0], "2026-01-03", "dates[0] = newest");
        check_str(dates[1], "2026-01-02", "dates[1] = middle");
        check_str(dates[2], "2026-01-01", "dates[2] = oldest");
    }
    report("ListDatesOrder");

    // ============================================================
    // List dates filters sessions
    // ============================================================
    {
        std::string d = (tmp_dir / "sessions_filter").string();
        fs::create_directories(fs::path(d) / "sessions");
        JsonlStorage storage(d);
        storage.start();
        storage.save_items({}, "2026-01-01");
        auto dates = storage.list_dates();
        bool has_sessions = false;
        for (const auto& date : dates) {
            if (date == "sessions") has_sessions = true;
        }
        CHECK_FALSE(has_sessions, "list_dates excludes sessions");
    }
    report("ListDatesExcludesSessions");

    // ============================================================
    // Full round-trip with LLM fields
    // ============================================================
    {
        std::string d = (tmp_dir / "llm_roundtrip").string();
        JsonlStorage storage(d);
        storage.start();

        UnifiedItem item{};
        item.platform = "twitter";
        item.item_id = "rt1";
        item.author_name = "Test";
        item.content_text = "Content";
        item.llm_relevance_score = 8;
        item.llm_relevance_reason = "relevant";
        item.llm_summary = "test summary";
        item.llm_tags = {"tag1", "tag2"};
        item.llm_key_points = {"kp1", "kp2"};
        item.metrics["likes"] = 10;
        item.metrics["comments"] = 3;

        storage.save_items({item}, "2026-01-15");
        auto loaded = storage.load_items("2026-01-15");
        check_size(loaded.size(), 1u, "llm roundtrip: 1 item");
        auto& l = loaded[0];
        check_str(l.platform, "twitter", "llm: platform");
        check_str(l.item_id, "rt1", "llm: item_id");
        check_int(l.llm_relevance_score, 8, "llm: relevance_score");
        CHECK_TRUE(l.llm_tags == item.llm_tags, "llm: tags");
        CHECK_TRUE(l.llm_key_points == item.llm_key_points, "llm: key_points");
        CHECK_TRUE(l.metrics == item.metrics, "llm: metrics");
    }
    report("LlmRoundTrip");

    // ============================================================
    // Multi-date isolation
    // ============================================================
    {
        std::string d = (tmp_dir / "multi_date").string();
        JsonlStorage storage(d);
        storage.start();
        storage.save_items({items[0]}, "2026-04-01");
        storage.save_items({items[1], items[2]}, "2026-04-02");
        auto d1 = storage.load_items("2026-04-01");
        auto d2 = storage.load_items("2026-04-02");
        check_size(d1.size(), 1u, "multi-date: d1 has 1");
        check_size(d2.size(), 2u, "multi-date: d2 has 2");
        check_str(d1[0].item_id, "1", "multi-date: d1[0].item_id");
    }
    report("MultiDateIsolation");

    // ============================================================
    // Factory
    // ============================================================
    {
        StorageFactory factory;
        auto* s = factory.create();
        CHECK_TRUE(s != nullptr, "factory create() non-null");
        check_str(s->name(), "JsonlStorage", "factory: name");
        delete s;

        auto* s2 = factory.create("/tmp/custom");
        CHECK_TRUE(s2 != nullptr, "factory create(path) non-null");
        delete s2;

        StorageConfig cfg{"/tmp/cfg_path"};
        auto* s3 = factory.create(cfg);
        CHECK_TRUE(s3 != nullptr, "factory create(config) non-null");
        delete s3;
    }
    report("Factory");

    fs::remove_all(tmp_dir);

    printf("\n== %d/%d passed ==\n", passed, passed + failed);
    return failed > 0 ? 1 : 0;
}
