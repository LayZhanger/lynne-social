#pragma once

#include "wheel/browser/browser_manager.h"
#include <functional>
#include <string>

namespace lynne {
namespace wheel {

// 收集当前页面上所有可交互元素的推荐 selector，返回 JSON 字符串
void dump_page_structure(
    BrowserContext* ctx,
    std::function<void(const std::string&)> on_result,
    std::function<void(const std::string&)> on_error);

} // namespace wheel
} // namespace lynne
