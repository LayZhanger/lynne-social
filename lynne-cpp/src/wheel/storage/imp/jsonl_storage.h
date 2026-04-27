#pragma once

#include "wheel/storage/storage.h"
#include "wheel/storage/storage_models.h"
#include <filesystem>
#include <string>

namespace lynne {
namespace wheel {

class JsonlStorage : public Storage {
public:
    explicit JsonlStorage(const std::string& data_dir = "data");

    std::string name() const override;
    void start() override;
    void stop() override;
    bool health_check() override;

    void save_items(const std::vector<common::UnifiedItem>& items,
                    const std::string& date = "") override;
    std::vector<common::UnifiedItem> load_items(const std::string& date = "",
                                                const std::string& platform = "") override;
    void save_report(const std::string& markdown,
                     const std::string& date = "") override;
    std::string load_report(const std::string& date = "") override;
    void save_summary(const nlohmann::json& summary,
                      const std::string& date = "") override;
    nlohmann::json load_summary(const std::string& date = "") override;
    std::vector<std::string> list_dates() override;

private:
    static std::string today_str();
    std::filesystem::path date_dir(const std::string& date) const;

    std::filesystem::path data_dir_;
    bool started_ = false;
};

} // namespace wheel
} // namespace lynne
