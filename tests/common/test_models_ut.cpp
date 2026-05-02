#include "common/models.h"
#include <gtest/gtest.h>

using namespace lynne::common;

// ============================================================
// UnifiedItem — defaults
// ============================================================

TEST(UnifiedItemDefaults, AllFieldsHaveSaneDefaults) {
    UnifiedItem item{};
    EXPECT_EQ(item.platform, "");
    EXPECT_EQ(item.item_id, "");
    EXPECT_EQ(item.item_type, "post");
    EXPECT_EQ(item.author_id, "");
    EXPECT_EQ(item.author_name, "");
    EXPECT_EQ(item.content_text, "");
    EXPECT_TRUE(item.content_media.empty());
    EXPECT_EQ(item.url, "");
    EXPECT_EQ(item.published_at, "");
    EXPECT_EQ(item.fetched_at, "");
    EXPECT_EQ(item.llm_relevance_score, 0);
    EXPECT_EQ(item.llm_relevance_reason, "");
    EXPECT_EQ(item.llm_summary, "");
    EXPECT_TRUE(item.llm_tags.empty());
    EXPECT_TRUE(item.llm_key_points.empty());
}

TEST(UnifiedItemDefaults, MetricsDefaultIsEmpty) {
    UnifiedItem item{};
    EXPECT_TRUE(item.metrics.empty());
}

// ============================================================
// RunStatus — defaults
// ============================================================

TEST(RunStatusDefaults, AllFieldsHaveSaneDefaults) {
    RunStatus s{};
    EXPECT_FALSE(s.running);
    EXPECT_EQ(s.current_task, "");
    EXPECT_EQ(s.progress, "");
}

// ============================================================
// UnifiedItem — from_json / to_json
// ============================================================

TEST(UnifiedItemJson, FromJsonMinimalFields) {
    std::string json = R"({"platform":"rednote","item_id":"abc123"})";
    auto j = nlohmann::json::parse(json);
    UnifiedItem item;
    from_json(j, item);

    EXPECT_EQ(item.platform, "rednote");
    EXPECT_EQ(item.item_id, "abc123");
    EXPECT_EQ(item.item_type, "post");         // default
    EXPECT_EQ(item.author_name, "");           // default
    EXPECT_EQ(item.llm_relevance_score, 0);    // default
}

TEST(UnifiedItemJson, FromJsonAllFields) {
    std::string json = R"({
        "platform": "rednote",
        "item_id": "abc123",
        "item_type": "video",
        "author_id": "u1",
        "author_name": "Alice",
        "content_text": "hello world",
        "content_media": ["img1.jpg", "img2.jpg"],
        "url": "https://a.com/1",
        "published_at": "2026-04-01",
        "fetched_at": "2026-04-26",
        "metrics": {"likes": 100, "shares": 5},
        "llm_relevance_score": 8,
        "llm_relevance_reason": "highly relevant",
        "llm_summary": "summary text",
        "llm_tags": ["tech", "ai"],
        "llm_key_points": ["point a", "point b"]
    })";
    auto j = nlohmann::json::parse(json);
    UnifiedItem item;
    from_json(j, item);

    EXPECT_EQ(item.platform, "rednote");
    EXPECT_EQ(item.item_id, "abc123");
    EXPECT_EQ(item.item_type, "video");
    EXPECT_EQ(item.author_id, "u1");
    EXPECT_EQ(item.author_name, "Alice");
    EXPECT_EQ(item.content_text, "hello world");
    ASSERT_EQ(item.content_media.size(), 2u);
    EXPECT_EQ(item.content_media[0], "img1.jpg");
    EXPECT_EQ(item.url, "https://a.com/1");
    EXPECT_EQ(item.published_at, "2026-04-01");
    EXPECT_EQ(item.fetched_at, "2026-04-26");
    EXPECT_EQ(item.metrics["likes"], 100);
    EXPECT_EQ(item.metrics["shares"], 5);
    EXPECT_EQ(item.llm_relevance_score, 8);
    EXPECT_EQ(item.llm_relevance_reason, "highly relevant");
    EXPECT_EQ(item.llm_summary, "summary text");
    ASSERT_EQ(item.llm_tags.size(), 2u);
    EXPECT_EQ(item.llm_tags[0], "tech");
    ASSERT_EQ(item.llm_key_points.size(), 2u);
    EXPECT_EQ(item.llm_key_points[0], "point a");
}

TEST(UnifiedItemJson, ToJsonRoundTrip) {
    UnifiedItem orig{};
    orig.platform = "weibo";
    orig.item_id = "wb001";
    orig.content_text = "test";
    orig.metrics["views"] = 500;
    orig.llm_tags = {"tag1"};

    nlohmann::json j;
    to_json(j, orig);

    UnifiedItem restored;
    from_json(j, restored);

    EXPECT_EQ(restored.platform, orig.platform);
    EXPECT_EQ(restored.item_id, orig.item_id);
    EXPECT_EQ(restored.content_text, orig.content_text);
    EXPECT_EQ(restored.metrics["views"], 500);
    ASSERT_EQ(restored.llm_tags.size(), 1u);
    EXPECT_EQ(restored.llm_tags[0], "tag1");
}

// ============================================================
// RunStatus — from_json / to_json
// ============================================================

TEST(RunStatusJson, FromJsonMinimal) {
    auto j = nlohmann::json::parse("{}");
    RunStatus s;
    from_json(j, s);
    EXPECT_FALSE(s.running);
    EXPECT_EQ(s.current_task, "");
    EXPECT_EQ(s.progress, "");
}

TEST(RunStatusJson, FromJsonAllFields) {
    auto j = nlohmann::json::parse(R"({"running":true,"current_task":"scan","progress":"50% done"})");
    RunStatus s;
    from_json(j, s);
    EXPECT_TRUE(s.running);
    EXPECT_EQ(s.current_task, "scan");
    EXPECT_EQ(s.progress, "50% done");
}

TEST(RunStatusJson, ToJsonRoundTrip) {
    RunStatus orig{};
    orig.running = true;
    orig.current_task = "fetch";
    orig.progress = "3/10";

    nlohmann::json j;
    to_json(j, orig);

    RunStatus restored;
    from_json(j, restored);

    EXPECT_TRUE(restored.running);
    EXPECT_EQ(restored.current_task, "fetch");
    EXPECT_EQ(restored.progress, "3/10");
}
