#include "wheel/storage/imp/jsonl_storage.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <algorithm>

namespace lynne {
namespace wheel {

JsonlStorage::JsonlStorage(const std::string& data_dir)
    : data_dir_(data_dir) {}

std::string JsonlStorage::name() const {
    return "JsonlStorage";
}

void JsonlStorage::start() {
    if (started_) return;
    std::filesystem::create_directories(data_dir_);
    started_ = true;
}

void JsonlStorage::stop() {
    // no-op
}

bool JsonlStorage::health_check() {
    return std::filesystem::exists(data_dir_);
}

std::string JsonlStorage::today_str() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_r(&time, &tm);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return std::string(buf);
}

std::filesystem::path JsonlStorage::date_dir(const std::string& date) const {
    auto dir = data_dir_ / date;
    std::filesystem::create_directories(dir);
    return dir;
}

void JsonlStorage::save_items(const std::vector<common::UnifiedItem>& items,
                              const std::string& date) {
    auto d = date.empty() ? today_str() : date;
    auto path = date_dir(d) / "items.jsonl";

    std::ofstream f(path, std::ios::app);
    for (const auto& item : items) {
        nlohmann::json j;
        to_json(j, item);
        f << j.dump() << '\n';
    }
}

std::vector<common::UnifiedItem> JsonlStorage::load_items(const std::string& date,
                                                          const std::string& platform) {
    auto d = date.empty() ? today_str() : date;
    auto path = data_dir_ / d / "items.jsonl";

    std::vector<common::UnifiedItem> items;

    if (!std::filesystem::exists(path)) {
        return items;
    }

    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        common::UnifiedItem item;
        from_json(nlohmann::json::parse(line), item);
        if (!platform.empty() && item.platform != platform) continue;
        items.push_back(item);
    }
    return items;
}

void JsonlStorage::save_report(const std::string& markdown,
                               const std::string& date) {
    auto d = date.empty() ? today_str() : date;
    auto path = date_dir(d) / "report.md";

    std::ofstream f(path);
    f << markdown;
}

std::string JsonlStorage::load_report(const std::string& date) {
    auto d = date.empty() ? today_str() : date;
    auto path = data_dir_ / d / "report.md";

    if (!std::filesystem::exists(path)) {
        return "";
    }

    std::ifstream f(path);
    return std::string(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());
}

void JsonlStorage::save_summary(const nlohmann::json& summary,
                                const std::string& date) {
    auto d = date.empty() ? today_str() : date;
    auto path = date_dir(d) / "summary.json";

    std::ofstream f(path);
    f << summary.dump(2);
}

nlohmann::json JsonlStorage::load_summary(const std::string& date) {
    auto d = date.empty() ? today_str() : date;
    auto path = data_dir_ / d / "summary.json";

    if (!std::filesystem::exists(path)) {
        return nlohmann::json{};
    }

    std::ifstream f(path);
    std::string content(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());
    return nlohmann::json::parse(content);
}

std::vector<std::string> JsonlStorage::list_dates() {
    std::vector<std::string> dates;

    if (!std::filesystem::exists(data_dir_)) {
        return dates;
    }

    for (const auto& entry : std::filesystem::directory_iterator(data_dir_)) {
        if (entry.is_directory() && entry.path().filename() != "sessions") {
            dates.push_back(entry.path().filename().string());
        }
    }
    std::sort(dates.begin(), dates.end(), std::greater<std::string>());
    return dates;
}

} // namespace wheel
} // namespace lynne
