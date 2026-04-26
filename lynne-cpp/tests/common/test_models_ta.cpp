#include "common/models.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace lynne::common;

static int passed = 0;
static int failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { printf("  [PASS] %s\n", msg); ++passed; } \
         else { printf("  [FAIL] %s\n", msg); ++failed; } \
    } while (0)

#define CHECK_EQ(a, b, msg) \
    do { if ((a) == (b)) { printf("  [PASS] %s\n", msg); ++passed; } \
         else { printf("  [FAIL] %s (got '%s' vs '%s')\n", msg, \
                       std::to_string(a).c_str(), std::to_string(b).c_str()); ++failed; } \
    } while (0)

#define CHECK_STR(a, b, msg) \
    do { std::string _sa(a); std::string _sb(b); \
         if (_sa == _sb) { printf("  [PASS] %s\n", msg); ++passed; } \
         else { printf("  [FAIL] %s ('%s' vs '%s')\n", msg, _sa.c_str(), _sb.c_str()); ++failed; } \
    } while (0)

#define CHECK_TRUE(cond, msg)  CHECK((cond), msg)
#define CHECK_FALSE(cond, msg) CHECK(!(cond), msg)

static void check_bool(bool a, bool b, const char* msg) {
    if (a == b) { printf("  [PASS] %s\n", msg); ++passed; }
    else { printf("  [FAIL] %s (%d vs %d)\n", msg, a, b); ++failed; }
}

static void report(const char* suite) {
    printf("--- %s: %d/%d ---\n", suite, passed, passed + failed);
}

int main() {
    // ============================================================
    // Full round-trip: construct → serialize → deserialize → compare
    // ============================================================
    {
        UnifiedItem item{};
        item.platform = "rednote";
        item.item_id = "r/abc";
        item.item_type = "post";
        item.author_id = "au123";
        item.author_name = "test_user";
        item.content_text = "some content here";
        item.content_media = {"p1.jpg", "p2.png"};
        item.url = "https://www.xiaohongshu.com/explore/abc";
        item.published_at = "2026-04-20T10:00:00Z";
        item.fetched_at = "2026-04-26T12:00:00Z";
        item.metrics["likes"] = 42;
        item.metrics["comments"] = 7;
        item.metrics["shares"] = 3;
        item.llm_relevance_score = 9;
        item.llm_relevance_reason = "exact match";
        item.llm_summary = "A summary of the post";
        item.llm_tags = {"tag-a", "tag-b", "tag-c"};
        item.llm_key_points = {"key point 1", "key point 2"};

        nlohmann::json j;
        to_json(j, item);

        std::string serialized = j.dump();

        UnifiedItem restored;
        from_json(nlohmann::json::parse(serialized), restored);

        CHECK_STR(restored.platform, item.platform, "platform round-trip");
        CHECK_STR(restored.item_id, item.item_id, "item_id round-trip");
        CHECK_STR(restored.item_type, item.item_type, "item_type round-trip");
        CHECK_STR(restored.author_id, item.author_id, "author_id round-trip");
        CHECK_STR(restored.author_name, item.author_name, "author_name round-trip");
        CHECK_STR(restored.content_text, item.content_text, "content_text round-trip");
        CHECK_TRUE(restored.content_media == item.content_media, "content_media round-trip");
        CHECK_STR(restored.url, item.url, "url round-trip");
        CHECK_STR(restored.published_at, item.published_at, "published_at round-trip");
        CHECK_STR(restored.fetched_at, item.fetched_at, "fetched_at round-trip");
        CHECK_TRUE(restored.metrics["likes"] == 42, "metrics likes round-trip");
        CHECK_TRUE(restored.metrics["comments"] == 7, "metrics comments round-trip");
        CHECK_TRUE(restored.metrics["shares"] == 3, "metrics shares round-trip");
        CHECK_TRUE(restored.llm_relevance_score == 9, "relevance_score round-trip");
        CHECK_STR(restored.llm_relevance_reason, "exact match", "relevance_reason round-trip");
        CHECK_STR(restored.llm_summary, "A summary of the post", "summary round-trip");
        CHECK_TRUE(restored.llm_tags.size() == 3u, "llm_tags count");
        CHECK_STR(restored.llm_tags[0], "tag-a", "llm_tags[0]");
        CHECK_TRUE(restored.llm_key_points.size() == 2u, "llm_key_points count");
        CHECK_STR(restored.llm_key_points[1], "key point 2", "llm_key_points[1]");
    }

    report("FullRoundTrip");

    // ============================================================
    // Empty arrays handled correctly
    // ============================================================
    {
        std::string json = R"({
            "platform": "empty_test",
            "item_id": "e1",
            "content_media": [],
            "llm_tags": [],
            "llm_key_points": []
        })";

        UnifiedItem item;
        from_json(nlohmann::json::parse(json), item);

        CHECK_TRUE(item.content_media.empty(), "content_media empty");
        CHECK_TRUE(item.llm_tags.empty(), "llm_tags empty");
        CHECK_TRUE(item.llm_key_points.empty(), "llm_key_points empty");

        nlohmann::json j;
        to_json(j, item);
        std::string dumped = j.dump();
        UnifiedItem restored;
        from_json(nlohmann::json::parse(dumped), restored);

        CHECK_TRUE(restored.content_media.empty(), "restored content_media empty");
        CHECK_TRUE(restored.llm_tags.empty(), "restored llm_tags empty");
        CHECK_TRUE(restored.llm_key_points.empty(), "restored llm_key_points empty");
    }

    report("EmptyArrays");

    // ============================================================
    // Batch round-trip (20 items)
    // ============================================================
    {
        std::vector<UnifiedItem> items;
        for (int i = 0; i < 20; ++i) {
            UnifiedItem item{};
            item.platform = "test_platform";
            item.item_id = "id_" + std::to_string(i);
            item.content_text = "content_" + std::to_string(i);
            item.llm_relevance_score = i;
            items.push_back(item);
        }

        nlohmann::json batch = nlohmann::json::array();
        for (const auto& item : items) {
            nlohmann::json j;
            to_json(j, item);
            batch.push_back(j);
        }

        std::string serialized = batch.dump();
        auto parsed = nlohmann::json::parse(serialized);
        CHECK_TRUE(parsed.size() == 20u, "batch size 20");

        std::vector<UnifiedItem> restored;
        for (const auto& elem : parsed) {
            UnifiedItem item;
            from_json(elem, item);
            restored.push_back(item);
        }

        CHECK_TRUE(restored.size() == items.size(), "restored size matches");
        for (size_t i = 0; i < items.size(); ++i) {
            CHECK_STR(restored[i].platform, items[i].platform, "batch platform");
            CHECK_STR(restored[i].item_id, items[i].item_id, "batch item_id");
            CHECK_STR(restored[i].content_text, items[i].content_text, "batch content_text");
            CHECK_TRUE(restored[i].llm_relevance_score == items[i].llm_relevance_score, "batch relevance_score");
        }
    }

    report("BatchRoundTrip");

    printf("\n== %d/%d passed ==\n", passed, passed + failed);
    return failed > 0 ? 1 : 0;
}
