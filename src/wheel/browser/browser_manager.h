#pragma once

#include "common/module.h"
#include "wheel/browser/browser_models.h"

#include <functional>
#include <memory>
#include <string>

#include <json.hpp>

namespace lynne {
namespace wheel {

// ============================================================
// BrowserContext — 平台隔离的页面浏览操作
// ============================================================
class BrowserContext {
public:
    virtual ~BrowserContext() = default;

    virtual void navigate(const std::string& url,
        std::function<void()> on_loaded,
        std::function<void(const std::string&)> on_error) = 0;

    virtual void evaluate(const std::string& js,
        std::function<void(nlohmann::json)> on_result,
        std::function<void(const std::string&)> on_error) = 0;

    virtual void add_init_script(const std::string& js,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) = 0;

    virtual void current_url(
        std::function<void(const std::string&)> on_url,
        std::function<void(const std::string&)> on_error) = 0;

    virtual void screenshot(const std::string& filepath,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) = 0;

    virtual void send_command(const std::string& method,
        const nlohmann::json& params,
        std::function<void(nlohmann::json)> on_ok,
        std::function<void(const std::string&)> on_error) = 0;

    virtual void close(
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) = 0;
};

// ============================================================
// BrowserManager — 浏览器引擎生命周期
// ============================================================
class BrowserManager : public common::Module {
public:
    virtual void get_context(const std::string& platform,
        std::function<void(BrowserContext*)> on_ok,
        std::function<void(const std::string&)> on_error) = 0;

    virtual void save_session(const std::string& platform,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) = 0;

    virtual void restore_session(const std::string& platform,
        std::function<void(bool)> on_done,
        std::function<void(const std::string&)> on_error) = 0;
};

} // namespace wheel
} // namespace lynne
