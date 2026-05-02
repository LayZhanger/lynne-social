#include "wheel/browser/imp/cdp_browser_manager.h"
#include "wheel/scheduler/scheduler_factory.h"

#include <json.hpp>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
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
    auto self = this;

    // 注册一次性 Page.loadEventFired 事件监听
    manager_->on_event(session_id_, "Page.loadEventFired",
        [on_loaded](const nlohmann::json&) {
            on_loaded();
        });

    // 设置超时
    int timeout = manager_->config().timeout_ms;

    // 发送 Page.navigate
    manager_->send_command("Page.navigate", {{"url", url}},
        [self, timeout](const nlohmann::json& result) {
            (void)result;
            // navigate 命令已发出，等待 loadEventFired
        },
        [self, on_error](const std::string& err) {
            on_error(err);
        },
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
        on_error);
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

    int stderr_pipe[2];
    if (pipe(stderr_pipe) < 0) {
        out_error = "pipe() failed";
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        out_error = "fork() failed";
        return;
    }

    if (pid == 0) {
        // child process
        close(stderr_pipe[0]);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stderr_pipe[1]);

        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            close(devnull);
        }

        std::string headless_flag = config_.headless ? "--headless" : "";
        std::string debug_port = "--remote-debugging-port=" + port_str;

        execlp(chrome.c_str(), chrome.c_str(),
               headless_flag.c_str(),
               "--disable-gpu",
               "--no-sandbox",
               "--disable-dev-shm-usage",
               debug_port.c_str(),
               nullptr);

        // execlp 失败
        _exit(1);
    }

    // parent
    chrome_pid_ = pid;
    close(stderr_pipe[1]);

    // 从 stderr 读取 "DevTools listening on ws://..."
    char buf[4096];
    ssize_t n;
    std::string accumulated;

    struct pollfd pfd;
    pfd.fd = stderr_pipe[0];
    pfd.events = POLLIN;

    int poll_ret = poll(&pfd, 1, 10000); // 10s timeout
    if (poll_ret <= 0) {
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        close(stderr_pipe[0]);
        out_error = poll_ret == 0 ? "chrome launch timeout"
                                  : "chrome launch poll error";
        chrome_pid_ = -1;
        return;
    }

    n = read(stderr_pipe[0], buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        accumulated.assign(buf, n);
    }

    // 继续读取剩余数据（Chrome 可能分多次输出）
    while ((n = read(stderr_pipe[0], buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        accumulated += std::string(buf, n);
    }
    close(stderr_pipe[0]);

    // 解析 CDP URL
    auto pos = accumulated.find("DevTools listening on ws://");
    if (pos == std::string::npos) {
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        chrome_pid_ = -1;
        out_error = "chrome did not output CDP URL";
        return;
    }

    auto ws_start = accumulated.find("ws://", pos);
    auto ws_end = accumulated.find_first_of(" \t\r\n", ws_start);
    out_url = accumulated.substr(ws_start, ws_end - ws_start);
}

// ============================================================
// WS 消息回调
// ============================================================

void CdpBrowserManager::on_ws_message(const std::string& data) {
    auto msg = nlohmann::json::parse(data, nullptr, false);
    if (msg.is_discarded()) return;

    if (msg.contains("id")) {
        // JSON-RPC 响应
        int id = msg["id"].get<int>();
        auto it = pending_.find(id);
        if (it == pending_.end()) return;

        if (msg.contains("error")) {
            auto err = msg["error"]["message"].get<std::string>();
            it->second.on_error(err);
        } else {
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
    scheduler_->start();

    std::string cdp_url;
    std::string launch_error;

    scheduler_->run_blocking(
        [this, &cdp_url, &launch_error]() {
            launch_chrome(cdp_url, launch_error);
        },
        [this, &cdp_url, &launch_error]() {
            if (!launch_error.empty() || cdp_url.empty()) {
                started_ = false;
                return;
            }

            ws_.setUrl(cdp_url);
            ws_.disableAutomaticReconnection();

            auto self = this;
            ws_.setOnMessageCallback([self](const ix::WebSocketMessagePtr& msg) {
                if (msg->type == ix::WebSocketMessageType::Message) {
                    self->scheduler_->post([self, data = msg->str]() {
                        self->on_ws_message(data);
                    });
                } else if (msg->type == ix::WebSocketMessageType::Error) {
                    self->chrome_crashed_ = true;
                }
            });

            ws_.start();

            // 启用 Page 域以接收 loadEventFired 等事件
            send_command("Page.enable", {}, [](const nlohmann::json&) {},
                [](const std::string&) {});

            started_ = true;
        }
    );
}

void CdpBrowserManager::stop() {
    if (!started_) return;

    contexts_.clear();
    ws_.stop();

    if (chrome_pid_ > 0) {
        kill(chrome_pid_, SIGTERM);
        int status;
        waitpid(chrome_pid_, &status, 0);
        chrome_pid_ = -1;
    }

    scheduler_->stop();
    scheduler_->step();
    started_ = false;
    chrome_crashed_ = false;
}

bool CdpBrowserManager::health_check() {
    if (!started_) return false;
    if (chrome_crashed_) return false;
    if (ws_.getReadyState() != ix::ReadyState::Open) return false;
    if (chrome_pid_ > 0 && kill(chrome_pid_, 0) != 0) {
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

                    auto ctx = std::make_unique<CdpBrowserContext>(
                        this, platform, target_id, session_id);

                    // 注入反检测脚本
                    std::string stealth = R"(
Object.defineProperty(navigator, 'webdriver', {get: () => false});
Object.defineProperty(navigator, 'plugins', {get: () => [1,2,3,4,5]});
window.chrome = {runtime: {}};
)";

                    send_command("Page.addScriptToEvaluateOnNewDocument",
                        {{"source", stealth}},
                        [this, platform, ctx = ctx.release(), on_ok](
                            const nlohmann::json&) {
                            auto* raw = ctx;
                            contexts_[platform].reset(ctx);
                            on_ok(raw);
                        },
                        [this, platform, target_id, on_error](
                            const std::string& err) {
                            // 反检测注入失败不阻断，继续
                            on_error(err);
                        },
                        session_id);

                    // 为这个 context 启用 Page 域
                    send_command("Page.enable", {},
                        [](const nlohmann::json&) {},
                        [](const std::string&) {},
                        session_id);

                },
                on_error,
                "");  // attachToTarget 是根级命令，无 sessionId
        },
        on_error,
        "");  // createTarget 是根级命令，无 sessionId
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

                    write_file(dir + platform + "_state.json",
                               session.dump(2));
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
    auto content = read_file(dir + platform + "_state.json");
    if (content.empty()) {
        on_done(false);
        return;
    }

    auto session = nlohmann::json::parse(content, nullptr, false);
    if (session.is_discarded() || !session.contains("cookies")) {
        on_done(false);
        return;
    }

    do_get_context(platform,
        [this, session, on_done, on_error](CdpBrowserContext* ctx) {
            send_command("Storage.setCookies",
                {{"cookies", session["cookies"]}},
                [on_done](const nlohmann::json&) { on_done(true); },
                on_error,
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
