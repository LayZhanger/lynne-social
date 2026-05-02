#pragma once

#include "common/module.h"
#include "wheel/browser/browser_models.h"

#include <cstdint>
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

    // ——— 人机交互 ——————————————————————————————

    virtual void click(const std::string& css_selector,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) = 0;

    virtual void type(const std::string& css_selector, const std::string& text,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) = 0;

    virtual void scroll(int delta_x, int delta_y,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) = 0;

    virtual void press_key(const std::string& key,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) = 0;

    virtual void hover(const std::string& css_selector,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) = 0;

    virtual void exists(const std::string& css_selector,
        std::function<void(bool)> on_result,
        std::function<void(const std::string&)> on_error) = 0;

    virtual void wait_for_selector(const std::string& css_selector,
        uint64_t timeout_ms,
        std::function<void()> on_found,
        std::function<void(const std::string&)> on_timeout) = 0;
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

    virtual void step() = 0;
    virtual void run() = 0;
};

} // namespace wheel
} // namespace lynne
