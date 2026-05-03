#include "wheel/browser/imp/cdp_browser_manager.h"
#include "wheel/scheduler/scheduler_factory.h"
#include "wheel/logger/logger_macros.h"

#include <json.hpp>

#include <cstdio>
#include <cstring>
#include <chrono>
#include <fstream>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <arpa/inet.h>

namespace lynne {
namespace wheel {

// ============================================================
// 工具
// ============================================================

static std::string js_selector(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '\\') r += "\\\\";
        else if (c == '\'') r += "\\'";
        else r += c;
    }
    return r;
}

// ============================================================
// 文件读写
// ============================================================

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    return {std::istreambuf_iterator<char>(f),
            std::istreambuf_iterator<char>()};
}

static void write_file(const std::string& path, const std::string& data) {
    std::ofstream f(path);
    f << data;
}

// ============================================================
// 简易 HTTP GET（仅 localhost，用于 /json/version）
// ============================================================

static std::string http_get(const std::string& host, int port,
                            const std::string& path) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return {};

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return {};
    }

    std::string req = "GET " + path + " HTTP/1.0\r\nHost: " +
                      host + "\r\nConnection: close\r\n\r\n";
    ::send(fd, req.data(), req.size(), 0);

    std::string resp;
    char buf[4096];
    ssize_t n;
    while ((n = ::recv(fd, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = '\0';
        resp += buf;
    }
    close(fd);

    // 找到空行后的 body
    auto body_start = resp.find("\r\n\r\n");
    if (body_start == std::string::npos) return {};
    return resp.substr(body_start + 4);
}

// ============================================================
// 简易 base64 解码（用于 screenshot）
// ============================================================

static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::vector<char> base64_decode(const std::string& input) {
    std::string clean;
    for (char c : input)
        if (!std::isspace(static_cast<unsigned char>(c)))
            clean += c;

    std::vector<char> out;
    out.reserve(clean.size() * 3 / 4);

    int val = 0, bits = -8;
    for (char c : clean) {
        if (c == '=') break;
        const char* p = strchr(BASE64_CHARS, c);
        if (!p) continue;
        val = (val << 6) | (p - BASE64_CHARS);
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

// ============================================================
// CdpBrowserContext
// ============================================================

CdpBrowserContext::CdpBrowserContext(CdpBrowserManager* manager,
    const std::string& platform,
    const std::string& target_id,
    const std::string& session_id)
    : manager_(manager)
    , platform_(platform)
    , target_id_(target_id)
    , session_id_(session_id) {}

CdpBrowserContext::~CdpBrowserContext() = default;

void CdpBrowserContext::navigate(const std::string& url,
    std::function<void()> on_loaded,
    std::function<void(const std::string&)> on_error) {
    manager_->send_command("Page.navigate", {{"url", url}},
        [on_loaded](const nlohmann::json&) {
            on_loaded();
        },
        on_error,
        session_id_);
}

void CdpBrowserContext::evaluate(const std::string& js,
    std::function<void(nlohmann::json)> on_result,
    std::function<void(const std::string&)> on_error) {
    nlohmann::json params = {
        {"expression", js},
        {"returnByValue", true}
    };
    manager_->send_command("Runtime.evaluate", params,
        [on_result](const nlohmann::json& result) {
            on_result(result["result"]);
        },
        on_error,
        session_id_);
}

void CdpBrowserContext::add_init_script(const std::string& js,
    std::function<void()> on_done,
    std::function<void(const std::string&)> on_error) {
    manager_->send_command("Page.addScriptToEvaluateOnNewDocument",
        {{"source", js}},
        [on_done](const nlohmann::json&) { on_done(); },
        on_error,
        session_id_);
}

void CdpBrowserContext::current_url(
    std::function<void(const std::string&)> on_url,
    std::function<void(const std::string&)> on_error) {
    nlohmann::json params = {
        {"expression", "document.URL"},
        {"returnByValue", true}
    };
    manager_->send_command("Runtime.evaluate", params,
        [on_url](const nlohmann::json& result) {
            on_url(result["result"]["value"].get<std::string>());
        },
        on_error,
        session_id_);
}

void CdpBrowserContext::screenshot(const std::string& filepath,
    std::function<void()> on_done,
    std::function<void(const std::string&)> on_error) {
    manager_->send_command("Page.captureScreenshot",
        {{"format", "png"}},
        [filepath, on_done, on_error](const nlohmann::json& result) {
            auto b64 = result["data"].get<std::string>();
            auto decoded = base64_decode(b64);
            std::ofstream f(filepath, std::ios::binary);
            if (!f) {
                on_error("screenshot: cannot write file");
                return;
            }
            f.write(decoded.data(), decoded.size());
            on_done();
        },
        on_error,
        session_id_);
}

void CdpBrowserContext::send_command(const std::string& method,
    const nlohmann::json& params,
    std::function<void(nlohmann::json)> on_ok,
    std::function<void(const std::string&)> on_error) {
    manager_->send_command(method, params, std::move(on_ok),
                           std::move(on_error), session_id_);
}

void CdpBrowserContext::close(
    std::function<void()> on_done,
    std::function<void(const std::string&)> on_error) {
    if (closed_) {
        on_error("already closed");
        return;
    }
    closed_ = true;
    manager_->send_command("Target.closeTarget", {{"targetId", target_id_}},
        [this, on_done](const nlohmann::json&) {
            manager_->remove_context(platform_);
            on_done();
        },
        [on_error](const std::string& err) {
            LOG_ERROR("browser", "closeTarget error: %s", err.c_str());
            on_error(err);
        });
}

void CdpBrowserContext::click(const std::string& css_selector,
    std::function<void()> on_done,
    std::function<void(const std::string&)> on_error) {
    if (closed_) { on_error("context closed"); return; }
    std::string sel = js_selector(css_selector);
    std::string js = R"((()=>{
        const e=document.querySelector(')" + sel + R"(');
        if(!e)return false;
        e.dispatchEvent(new MouseEvent('mousedown',{bubbles:true,cancelable:true,view:window}));
        e.dispatchEvent(new MouseEvent('mouseup',{bubbles:true,cancelable:true,view:window}));
        e.dispatchEvent(new MouseEvent('click',{bubbles:true,cancelable:true,view:window}));
        if(e.tagName==='A'&&e.href)window.location.href=e.href;
        else if(e.form&&(e.type==='submit'||e.tagName==='BUTTON'))e.form.submit();
        return true;
    })())";
    evaluate(js,
        [on_done, on_error](nlohmann::json res) {
            if (res.contains("value") && !res["value"].is_null()
                && res["value"].get<bool>()) {
                on_done();
            } else {
                on_error("click: element not found");
            }
        },
        on_error);
}

void CdpBrowserContext::type(const std::string& css_selector, const std::string& text,
    std::function<void()> on_done,
    std::function<void(const std::string&)> on_error) {
    if (closed_) { on_error("context closed"); return; }
    std::string sel = js_selector(css_selector);
    std::string escaped;
    escaped.reserve(text.size());
    for (char c : text) {
        if (c == '\\') escaped += "\\\\";
        else if (c == '\'') escaped += "\\'";
        else if (c == '\n') escaped += "\\n";
        else if (c == '\r') escaped += "\\r";
        else escaped += c;
    }
    std::string js = "(()=>{const e=document.querySelector('"
        + sel + "');if(!e)return false;"
        "e.focus();e.value='" + escaped + "';"
        "e.dispatchEvent(new Event('input',{bubbles:true}));"
        "e.dispatchEvent(new Event('change',{bubbles:true}));"
        "return true;})()";
    evaluate(js,
        [on_done, on_error](nlohmann::json res) {
            if (res.contains("value") && !res["value"].is_null()
                && res["value"].get<bool>()) {
                on_done();
            } else {
                on_error("type: element not found");
            }
        },
        on_error);
}

void CdpBrowserContext::scroll(int delta_x, int delta_y,
    std::function<void()> on_done,
    std::function<void(const std::string&)> on_error) {
    if (closed_) { on_error("context closed"); return; }
    std::string js = "window.scrollBy(" + std::to_string(delta_x)
                     + "," + std::to_string(delta_y) + ")";
    evaluate(js,
        [on_done](nlohmann::json) { on_done(); },
        on_error);
}

void CdpBrowserContext::press_key(const std::string& key,
    std::function<void()> on_done,
    std::function<void(const std::string&)> on_error) {
    if (closed_) { on_error("context closed"); return; }
    int vk = 0;
    if (key == "Enter") vk = 13;
    else if (key == "Tab") vk = 9;
    else if (key == "Escape") vk = 27;
    else if (key == "Backspace") vk = 8;
    else if (key == "Delete") vk = 46;
    nlohmann::json down = {{"type", "keyDown"}, {"key", key}};
    if (vk > 0) down["windowsVirtualKeyCode"] = vk;
    if (key == "Enter") down["text"] = "\r";
    manager_->send_command("Input.dispatchKeyEvent", down,
        [this, key, vk, on_done, on_error](const nlohmann::json&) {
            nlohmann::json up = {{"type", "keyUp"}, {"key", key}};
            if (vk > 0) up["windowsVirtualKeyCode"] = vk;
            manager_->send_command("Input.dispatchKeyEvent", up,
                [on_done](const nlohmann::json&) { on_done(); },
                on_error, session_id_);
        },
        on_error, session_id_);
}

void CdpBrowserContext::hover(const std::string& css_selector,
    std::function<void()> on_done,
    std::function<void(const std::string&)> on_error) {
    if (closed_) { on_error("context closed"); return; }
    std::string sel = js_selector(css_selector);
    std::string js = R"((()=>{
        const e=document.querySelector(')" + sel + R"(');
        if(!e)return false;
        e.dispatchEvent(new MouseEvent('mouseover',{bubbles:true,cancelable:true,view:window}));
        e.dispatchEvent(new MouseEvent('mouseenter',{bubbles:false,cancelable:true,view:window}));
        return true;
    })())";
    evaluate(js,
        [on_done, on_error](nlohmann::json res) {
            if (res.contains("value") && !res["value"].is_null()
                && res["value"].get<bool>()) {
                on_done();
            } else {
                on_error("hover: element not found");
            }
        },
        on_error);
}

void CdpBrowserContext::exists(const std::string& css_selector,
    std::function<void(bool)> on_result,
    std::function<void(const std::string&)> on_error) {
    if (closed_) { on_error("context closed"); return; }
    std::string sel = js_selector(css_selector);
    std::string js = "!!document.querySelector('" + sel + "')";
    evaluate(js,
        [on_result](nlohmann::json res) {
            bool found = false;
            if (res.contains("value") && !res["value"].is_null())
                found = res["value"].get<bool>();
            on_result(found);
        },
        [on_error](const std::string& err) { on_error(err); });
}

void CdpBrowserContext::wait_for_selector(const std::string& css_selector,
    uint64_t timeout_ms,
    std::function<void()> on_found,
    std::function<void(const std::string&)> on_timeout) {
    if (closed_) { on_timeout("context closed"); return; }
    auto start = std::chrono::steady_clock::now();
    auto done = std::make_shared<bool>(false);

    auto check = std::make_shared<std::function<void()>>();
    *check = [this, css_selector, timeout_ms, on_found, on_timeout,
              start, done, check]() {
        if (*done || closed_) { *done = true; return; }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= (int64_t)timeout_ms) {
            *done = true;
            on_timeout("wait_for_selector timeout (" + css_selector + ")");
            return;
        }
        std::string sel = js_selector(css_selector);
        evaluate("!!document.querySelector('" + sel + "')",
            [done, on_found, check](nlohmann::json res) {
                if (*done) return;
                bool found = false;
                if (res.contains("value") && !res["value"].is_null())
                    found = res["value"].get<bool>();
                if (found) { *done = true; on_found(); }
                else { check->operator()(); }
            },
            [done, on_timeout, check](const std::string&) {
                if (*done) return;
                check->operator()();
            });
    };
    manager_->scheduler()->post(*check);
}

// ============================================================
// CdpBrowserManager
// ============================================================

CdpBrowserManager::CdpBrowserManager(const BrowserConfig& config)
    : config_(config) {
    scheduler_ = SchedulerFactory().create();
}

CdpBrowserManager::~CdpBrowserManager() {
    stop();
    delete scheduler_;
}

std::string CdpBrowserManager::name() const {
    return "browser";
}

// ============================================================
// 查找 Chrome 可执行文件
// ============================================================

std::string CdpBrowserManager::find_chrome() {
    if (!config_.chrome_path.empty()) {
        return config_.chrome_path;
    }

    static const char* candidates[] = {
        "google-chrome",
        "google-chrome-stable",
        "chromium",
        "chromium-browser",
        "/usr/bin/google-chrome",
        "/usr/bin/google-chrome-stable",
        "/usr/bin/chromium",
        "/usr/bin/chromium-browser",
        "/snap/bin/chromium",
        nullptr
    };

    for (int i = 0; candidates[i]; ++i) {
        if (access(candidates[i], X_OK) == 0) {
            return candidates[i];
        }
    }
    return {};
}

// ============================================================
// 启动 Chrome 子进程（在 worker 线程中执行）
// ============================================================

void CdpBrowserManager::launch_chrome(std::string& out_url,
                                       std::string& out_error) {
    std::string chrome = find_chrome();
    if (chrome.empty()) {
        out_error = "chrome not found";
        return;
    }

    int port = config_.cdp_port;
    if (port == 0) port = 9222;
    std::string port_str = std::to_string(port);

    int stdout_pipe[2];
    if (pipe(stdout_pipe) < 0) {
        out_error = "pipe() failed";
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        out_error = "fork() failed";
        return;
    }

    if (pid == 0) {
        close(stdout_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);

        execlp(chrome.c_str(), chrome.c_str(),
               "--headless",
               "--disable-gpu",
               "--no-sandbox",
               "--disable-dev-shm-usage",
               "--remote-allow-origins=*",
               "--remote-debugging-address=127.0.0.1",
               "--remote-debugging-port=0",
               nullptr);
        _exit(1);
    }

    chrome_pid_ = pid;
    close(stdout_pipe[1]);

    char buf[4096];
    std::string acc;
    ssize_t n;

    struct pollfd pfd;
    pfd.fd = stdout_pipe[0];
    pfd.events = POLLIN;

    for (int tries = 0; tries < 60; ++tries) {
        int poll_ret = poll(&pfd, 1, 500);
        if (poll_ret < 0) break;
        if (poll_ret == 0) continue;

        n = read(stdout_pipe[0], buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = '\0';
        acc.append(buf, n);

        size_t pos = acc.find("ws://");
        if (pos != std::string::npos) {
            auto end = acc.find_first_of(" \t\r\n", pos);
            out_url = acc.substr(pos, end - pos);
            break;
        }
    }
    close(stdout_pipe[0]);

    if (out_url.empty()) {
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        chrome_pid_ = -1;
        out_error = "chrome did not output WS URL";
        return;
    }

    // 从 URL 解析端口，等 HTTP 服务就绪
    int port_from_url = 0;
    auto col_a = out_url.find(':', 5);
    auto col_b = out_url.find(':', col_a + 1);
    if (col_b != std::string::npos) {
        auto sl = out_url.find('/', col_b);
        port_from_url = std::stoi(out_url.substr(col_b + 1, sl - col_b - 1));
    }
    int ready_port = port_from_url > 0 ? port_from_url : port;

    for (int tries = 0; tries < 5; ++tries) {
        auto resp = http_get("127.0.0.1", ready_port, "/json/version");
        if (!resp.empty()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

// ============================================================
// WS 消息回调
// ============================================================

void CdpBrowserManager::on_ws_message(const std::string& data) {
    auto msg = nlohmann::json::parse(data, nullptr, false);
    if (msg.is_discarded()) {
        LOG_WARN("browser", "on_ws_message: JSON parse failed");
        return;
    }

    if (msg.contains("id")) {
        int id = msg["id"].get<int>();
        auto it = pending_.find(id);
        if (it == pending_.end()) {
            LOG_WARN("browser", "on_ws_message: unknown id=%d", id);
            return;
        }

        if (msg.contains("error")) {
            auto err = msg["error"]["message"].get<std::string>();
            LOG_ERROR("browser", "CDP error id=%d: %s", id, err.c_str());
            it->second.on_error(err);
        } else {
            LOG_INFO("browser", "CDP ok id=%d", id);
            it->second.on_ok(msg["result"]);
        }
        pending_.erase(it);

    } else if (msg.contains("method")) {
        // 事件
        std::string method = msg["method"].get<std::string>();
        std::string sid = msg.value("sessionId", "");
        std::string key = sid + ":" + method;

        auto it = event_handlers_.find(key);
        if (it != event_handlers_.end()) {
            auto cb = std::move(it->second);
            event_handlers_.erase(it);
            cb(msg.value("params", nlohmann::json::object()));
        }
    }
}

// ============================================================
// 发送 CDP 命令
// ============================================================

void CdpBrowserManager::send_command(
    const std::string& method,
    const nlohmann::json& params,
    std::function<void(nlohmann::json)> on_ok,
    std::function<void(const std::string&)> on_error,
    const std::string& session_id) {

    int id = cmd_id_++;
    nlohmann::json msg = {
        {"id", id},
        {"method", method},
        {"params", params}
    };
    if (!session_id.empty()) {
        msg["sessionId"] = session_id;
    }

    pending_[id] = {std::move(on_ok), std::move(on_error)};

    if (!ws_ready_) {
        LOG_INFO("browser", "queue cmd=%s", method.c_str());
        pending_commands_.push_back(msg);
        return;
    }
    ws_.sendText(msg.dump());
}

// ============================================================
// 事件监听器管理
// ============================================================

void CdpBrowserManager::on_event(const std::string& session_id,
                                  const std::string& method,
                                  EventHandler handler) {
    event_handlers_[session_id + ":" + method] = std::move(handler);
}

void CdpBrowserManager::remove_event(const std::string& session_id,
                                      const std::string& method) {
    event_handlers_.erase(session_id + ":" + method);
}

// ============================================================
// 生命周期
// ============================================================

void CdpBrowserManager::start() {
    if (started_) return;

    // 清理上次残留的 Chrome（前一次 stop 未执行完，或进程崩溃导致 stop 未调用）
    if (chrome_pid_ > 0) {
        LOG_WARN("browser", "cleaning stale chrome pid=%d", chrome_pid_);
        kill(chrome_pid_, SIGKILL);
        waitpid(chrome_pid_, nullptr, 0);
        chrome_pid_ = -1;
    }

    ws_ready_ = false;
    pending_commands_.clear();
    chrome_crashed_ = false;
    scheduler_->start();

    auto state = std::make_shared<LaunchState>();

    scheduler_->run_blocking(
        [this, state]() {
            LOG_INFO("browser", "launching Chrome...");
            launch_chrome(state->url, state->error);
            LOG_INFO("browser", "launch_chrome done: url='%s' err='%s'",
                     state->url.c_str(), state->error.c_str());
        },
        [this, state]() {
            LOG_INFO("browser", "on_done fired: url='%s' err='%s'",
                     state->url.c_str(), state->error.c_str());
            if (!state->error.empty() || state->url.empty()) {
                return;
            }

            ws_.setUrl(state->url);
            ws_.disableAutomaticReconnection();

            auto self = this;
            ws_.setOnMessageCallback([self](const ix::WebSocketMessagePtr& msg) {
                if (msg->type == ix::WebSocketMessageType::Open) {
                    self->scheduler_->post([self]() {
                        self->ws_ready_ = true;
                        for (auto& cmd : self->pending_commands_) {
                            self->ws_.sendText(cmd.dump());
                        }
                        self->pending_commands_.clear();
                        int id = self->cmd_id_++;
                        nlohmann::json enable = {
                            {"id", id},
                            {"method", "Page.enable"},
                            {"params", nlohmann::json::object()}
                        };
                        self->pending_[id] = {
                            [](const nlohmann::json&) {},
                            [](const std::string&) {}
                        };
                        self->ws_.sendText(enable.dump());
                    });
                } else if (msg->type == ix::WebSocketMessageType::Message) {
                    self->scheduler_->post([self, data = msg->str]() {
                        self->on_ws_message(data);
                    });
                } else if (msg->type == ix::WebSocketMessageType::Error) {
                    LOG_ERROR("browser", "WS ERROR: %s",
                              msg->errorInfo.reason.c_str());
                    self->chrome_crashed_ = true;
                }
            });

            ws_.start();
            started_ = true;
        }
    );
}

void CdpBrowserManager::stop() {
    if (!started_) return;

    LOG_INFO("browser", "stopping...");
    contexts_.clear();
    ws_.stop();

    if (chrome_pid_ > 0) {
        LOG_INFO("browser", "killing chrome pid %d", chrome_pid_);
        kill(chrome_pid_, SIGTERM);
        int status;
        waitpid(chrome_pid_, &status, 0);
        LOG_INFO("browser", "chrome exited status=%d", WEXITSTATUS(status));
        chrome_pid_ = -1;
    }

    scheduler_->stop();
    scheduler_->step();
    started_ = false;
    chrome_crashed_ = false;
}

bool CdpBrowserManager::health_check() {
    if (!started_) return false;
    if (chrome_crashed_) {
        LOG_WARN("browser", "health: chrome crashed");
        return false;
    }
    if (chrome_pid_ > 0 && kill(chrome_pid_, 0) != 0) {
        LOG_WARN("browser", "health: chrome pid %d dead", chrome_pid_);
        chrome_crashed_ = true;
        return false;
    }
    return true;
}

// ============================================================
// 上下文管理
// ============================================================

void CdpBrowserManager::do_get_context(const std::string& platform,
    std::function<void(CdpBrowserContext*)> on_ok,
    std::function<void(const std::string&)> on_error) {

    if (!started_) {
        on_error("not started");
        return;
    }

    // 缓存命中
    auto it = contexts_.find(platform);
    if (it != contexts_.end()) {
        LOG_INFO("browser", "get_context cache hit: platform=%s", platform.c_str());
        on_ok(it->second.get());
        return;
    }

    // 创建新 Target
    send_command("Target.createTarget", {{"url", "about:blank"}},
        [this, platform, on_ok, on_error](const nlohmann::json& result) {
            std::string target_id = result["targetId"].get<std::string>();

            // 附加到 Target 获取 sessionId
            send_command("Target.attachToTarget",
                {{"targetId", target_id}, {"flatten", true}},
                [this, platform, target_id, on_ok, on_error](const nlohmann::json& r) {
                    std::string session_id = r["sessionId"].get<std::string>();
                    LOG_INFO("browser", "attachToTarget ok: platform=%s session=%s",
                             platform.c_str(), session_id.c_str());

                    auto ctx = std::make_unique<CdpBrowserContext>(
                        this, platform, target_id, session_id);

                    // 注入反检测脚本
                    std::string stealth = R"(
Object.defineProperty(navigator, 'webdriver', {get: () => false});
Object.defineProperty(navigator, 'plugins', {get: () => [1,2,3,4,5]});
window.chrome = {runtime: {}};
)";

                    auto* raw_ctx = ctx.release();
                    send_command("Page.addScriptToEvaluateOnNewDocument",
                        {{"source", stealth}},
                        [this, platform, raw_ctx, on_ok](
                            const nlohmann::json&) {
                            contexts_[platform].reset(raw_ctx);
                            LOG_INFO("browser", "addInitScript ok: platform=%s",
                                     platform.c_str());
                            on_ok(raw_ctx);
                        },
                        [this, platform, raw_ctx](const std::string& err) {
                            LOG_WARN("browser", "addInitScript error (discarding ctx): %s",
                                     err.c_str());
                            delete raw_ctx;
                        },
                        session_id);

                    // 为这个 context 启用 Page 域
                    send_command("Page.enable", {},
                        [](const nlohmann::json&) {},
                        [](const std::string&) {},
                        session_id);

                },
                [on_error](const std::string& err) {
                    LOG_ERROR("browser", "attachToTarget error: %s", err.c_str());
                    on_error(err);
                },
                "");  // attachToTarget 是根级命令，无 sessionId
        },
                [this, platform, on_error](const std::string& err) {
                    LOG_ERROR("browser", "createTarget error: %s", err.c_str());
                    on_error(err);
                },
                "");
}

void CdpBrowserManager::get_context(const std::string& platform,
    std::function<void(BrowserContext*)> on_ok,
    std::function<void(const std::string&)> on_error) {
    do_get_context(platform,
        [on_ok](CdpBrowserContext* ctx) { on_ok(ctx); },
        on_error);
}

void CdpBrowserManager::remove_context(const std::string& platform) {
    contexts_.erase(platform);
}

// ============================================================
// 会话持久化
// ============================================================

void CdpBrowserManager::save_session(const std::string& platform,
    std::function<void()> on_done,
    std::function<void(const std::string&)> on_error) {
    do_get_context(platform,
        [this, platform, on_done, on_error](CdpBrowserContext* ctx) {
            send_command("Storage.getCookies", {},
                [this, platform, on_done](const nlohmann::json& result) {
                    nlohmann::json session;
                    session["cookies"] = result["cookies"];

                    auto dir = config_.sessions_dir;
                    if (!dir.empty() && dir.back() != '/') dir += '/';
                    auto path = dir + platform + "_state.json";
                    size_t n = result["cookies"].size();
                    write_file(path, session.dump(2));
                    LOG_INFO("browser", "saved %zu cookies to %s", n, path.c_str());
                    on_done();
                },
                on_error,
                ctx->session_id());
        },
        on_error);
}

void CdpBrowserManager::restore_session(const std::string& platform,
    std::function<void(bool)> on_done,
    std::function<void(const std::string&)> on_error) {
    auto dir = config_.sessions_dir;
    if (!dir.empty() && dir.back() != '/') dir += '/';
    auto path = dir + platform + "_state.json";
    auto content = read_file(path);
    if (content.empty()) {
        LOG_INFO("browser", "restore_session: no file at %s", path.c_str());
        on_done(false);
        return;
    }

    auto session = nlohmann::json::parse(content, nullptr, false);
    if (session.is_discarded() || !session.contains("cookies")) {
        LOG_WARN("browser", "restore_session: invalid session file %s", path.c_str());
        on_done(false);
        return;
    }

    size_t n = session["cookies"].size();
    do_get_context(platform,
        [this, session, on_done, on_error, n, path](CdpBrowserContext* ctx) {
            send_command("Storage.setCookies",
                {{"cookies", session["cookies"]}},
                [on_done, n, path](const nlohmann::json&) {
                    LOG_INFO("browser", "restored %zu cookies from %s", n, path.c_str());
                    on_done(true);
                },
                [on_error, path](const std::string& err) {
                    LOG_ERROR("browser", "restore_session: setCookies error: %s", err.c_str());
                    on_error(err);
                },
                ctx->session_id());
        },
        [on_error](const std::string& err) { on_error(err); });
}

void CdpBrowserManager::step() {
    scheduler_->step();
}

void CdpBrowserManager::run() {
    scheduler_->run();
}

} // namespace wheel
} // namespace lynne
