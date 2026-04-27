#pragma once

#include "common/module.h"
#include "common/models.h"
#include <json.hpp>
#include <string>
#include <vector>

namespace lynne {
namespace wheel {

class Storage : public common::Module {
public:
    virtual void save_items(const std::vector<common::UnifiedItem>& items,
                            const std::string& date = "") = 0;
    virtual std::vector<common::UnifiedItem> load_items(const std::string& date = "",
                                                        const std::string& platform = "") = 0;
    virtual void save_report(const std::string& markdown,
                             const std::string& date = "") = 0;
    virtual std::string load_report(const std::string& date = "") = 0;
    virtual void save_summary(const nlohmann::json& summary,
                              const std::string& date = "") = 0;
    virtual nlohmann::json load_summary(const std::string& date = "") = 0;
    virtual std::vector<std::string> list_dates() = 0;
};

} // namespace wheel
} // namespace lynne
