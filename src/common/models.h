#pragma once

#include <json.hpp>
#include <string>
#include <vector>

namespace lynne {
namespace common {

struct UnifiedItem {
    std::string platform;
    std::string item_id;
    std::string item_type = "post";
    std::string author_id;
    std::string author_name;
    std::string content_text;
    std::vector<std::string> content_media;
    std::string url;
    std::string published_at;
    std::string fetched_at;
    nlohmann::json metrics;

    int llm_relevance_score = 0;
    std::string llm_relevance_reason;
    std::string llm_summary;
    std::vector<std::string> llm_tags;
    std::vector<std::string> llm_key_points;
};

struct RunStatus {
    bool running = false;
    std::string current_task;
    std::string progress;
};

void from_json(const nlohmann::json& j, UnifiedItem& item);
void to_json(nlohmann::json& j, const UnifiedItem& item);
void from_json(const nlohmann::json& j, RunStatus& s);
void to_json(nlohmann::json& j, const RunStatus& s);

} // namespace common
} // namespace lynne
