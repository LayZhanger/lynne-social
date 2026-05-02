#pragma once

#include "wheel/browser/browser_manager.h"
#include "wheel/scheduler/scheduler.h"

#include <ixwebsocket/IXWebSocket.h>

#include <map>
#include <memory>
#include <string>

namespace lynne {
namespace wheel {

struct PendingRequest {
    std::function<void(nlohmann::json)> on_ok;
    std::function<void(const std::string&)> on_error;
};

// ============================================================
// CdpBrowserContext — 单个平台 CDP session 的封装
// ============================================================
class CdpBrowserContext : public BrowserContext {
public:
    CdpBrowserContext(class CdpBrowserManager* manager,
                      const std::string& platform,
                      const std::string& target_id,
                      const std::string& session_id);
    ~CdpBrowserContext() override;

    std::string session_id() const { return session_id_; }
    std::string target_id() const { return target_id_; }
    std::string platform() const { return platform_; }
    bool is_closed() const { return closed_; }

    void navigate(const std::string& url,
        std::function<void()> on_loaded,
        std::function<void(const std::string&)> on_error) override;

    void evaluate(const std::string& js,
        std::function<void(nlohmann::json)> on_result,
        std::function<void(const std::string&)> on_error) override;

    void add_init_script(const std::string& js,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) override;

    void current_url(
        std::function<void(const std::string&)> on_url,
        std::function<void(const std::string&)> on_error) override;

    void screenshot(const std::string& filepath,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) override;

    void send_command(const std::string& method,
        const nlohmann::json& params,
        std::function<void(nlohmann::json)> on_ok,
        std::function<void(const std::string&)> on_error) override;

    void close(
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) override;

private:
    class CdpBrowserManager* manager_;
    std::string platform_;
    std::string target_id_;
    std::string session_id_;
    bool closed_ = false;
};

// ============================================================
// CdpBrowserManager — 浏览器引擎 CDP 实现
// ============================================================
class CdpBrowserManager : public BrowserManager {
public:
    explicit CdpBrowserManager(const BrowserConfig& config);
    ~CdpBrowserManager() override;

    std::string name() const override;
    void start() override;
    void stop() override;
    bool health_check() override;

    void get_context(const std::string& platform,
        std::function<void(BrowserContext*)> on_ok,
        std::function<void(const std::string&)> on_error) override;

    void save_session(const std::string& platform,
        std::function<void()> on_done,
        std::function<void(const std::string&)> on_error) override;

    void restore_session(const std::string& platform,
        std::function<void(bool)> on_done,
        std::function<void(const std::string&)> on_error) override;

    void step() override;
    void run() override;

    // 被 CdpBrowserContext 调用：发送带 sessionId 的 CDP 命令
    void send_command(
        const std::string& method,
        const nlohmann::json& params,
        std::function<void(nlohmann::json)> on_ok,
        std::function<void(const std::string&)> on_error,
        const std::string& session_id = "");

    // 注册/注销一次性事件监听器 (sessionId:method → callback)
    using EventHandler = std::function<void(const nlohmann::json& params)>;
    void on_event(const std::string& session_id,
                  const std::string& method,
                  EventHandler handler);
    void remove_event(const std::string& session_id,
                      const std::string& method);

    // 上下文管理（被 CdpBrowserContext::close 调用）
    void remove_context(const std::string& platform);
    Scheduler* scheduler() { return scheduler_; }
    const BrowserConfig& config() const { return config_; }

private:
    BrowserConfig config_;
    pid_t chrome_pid_ = -1;
    ix::WebSocket ws_;
    int cmd_id_ = 0;
    bool started_ = false;
    bool chrome_crashed_ = false;

    std::map<int, PendingRequest> pending_;
    std::map<std::string, EventHandler> event_handlers_;
    std::map<std::string, std::unique_ptr<CdpBrowserContext>> contexts_;
    Scheduler* scheduler_ = nullptr;

    std::string find_chrome();
    void launch_chrome(std::string& out_url, std::string& out_error);
    void on_ws_message(const std::string& data);
    void do_get_context(const std::string& platform,
        std::function<void(CdpBrowserContext*)> on_ok,
        std::function<void(const std::string&)> on_error);
};

} // namespace wheel
} // namespace lynne
