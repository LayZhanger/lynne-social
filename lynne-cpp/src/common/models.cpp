#include "common/models.h"

namespace lynne {
namespace common {

void from_json(const nlohmann::json& j, UnifiedItem& item) {
    item.platform = j.value("platform", "");
    item.item_id = j.value("item_id", "");
    item.item_type = j.value("item_type", "post");
    item.author_id = j.value("author_id", "");
    item.author_name = j.value("author_name", "");
    item.content_text = j.value("content_text", "");
    if (j.contains("content_media")) {
        item.content_media = j.at("content_media").get<std::vector<std::string>>();
    }
    item.url = j.value("url", "");
    item.published_at = j.value("published_at", "");
    item.fetched_at = j.value("fetched_at", "");
    if (j.contains("metrics")) {
        item.metrics = j.at("metrics");
    }
    item.llm_relevance_score = j.value("llm_relevance_score", 0);
    item.llm_relevance_reason = j.value("llm_relevance_reason", "");
    item.llm_summary = j.value("llm_summary", "");
    if (j.contains("llm_tags")) {
        item.llm_tags = j.at("llm_tags").get<std::vector<std::string>>();
    }
    if (j.contains("llm_key_points")) {
        item.llm_key_points = j.at("llm_key_points").get<std::vector<std::string>>();
    }
}

void to_json(nlohmann::json& j, const UnifiedItem& item) {
    j = nlohmann::json{
        {"platform", item.platform},
        {"item_id", item.item_id},
        {"item_type", item.item_type},
        {"author_id", item.author_id},
        {"author_name", item.author_name},
        {"content_text", item.content_text},
        {"content_media", item.content_media},
        {"url", item.url},
        {"published_at", item.published_at},
        {"fetched_at", item.fetched_at},
        {"metrics", item.metrics},
        {"llm_relevance_score", item.llm_relevance_score},
        {"llm_relevance_reason", item.llm_relevance_reason},
        {"llm_summary", item.llm_summary},
        {"llm_tags", item.llm_tags},
        {"llm_key_points", item.llm_key_points},
    };
}

void from_json(const nlohmann::json& j, RunStatus& s) {
    s.running = j.value("running", false);
    s.current_task = j.value("current_task", "");
    s.progress = j.value("progress", "");
}

void to_json(nlohmann::json& j, const RunStatus& s) {
    j = nlohmann::json{
        {"running", s.running},
        {"current_task", s.current_task},
        {"progress", s.progress},
    };
}

} // namespace common
} // namespace lynne
