#include "wheel/llm/imp/deepseek_engine.h"
#include "wheel/scheduler/scheduler_factory.h"

#include <httplib.h>

namespace lynne {
namespace wheel {

static const char* kDefaultBaseUrl = "https://api.deepseek.com/v1";

DeepSeekEngine::DeepSeekEngine(const LLMConfig& config)
    : config_(config) {
    scheduler_ = SchedulerFactory().create();
}

DeepSeekEngine::~DeepSeekEngine() {
    stop();
    delete scheduler_;
}

std::string DeepSeekEngine::name() const {
    std::string n = "llm(" + config_.provider + ":" + config_.model + ")";
    return n;
}

void DeepSeekEngine::start() {
    if (started_) return;
    scheduler_->start();
    started_ = true;
}

void DeepSeekEngine::stop() {
    if (!started_) return;
    scheduler_->stop();
    scheduler_->step();
    started_ = false;
}

bool DeepSeekEngine::health_check() {
    return started_;
}

void DeepSeekEngine::chat(
    const nlohmann::json& messages,
    std::function<void(nlohmann::json)> on_result,
    std::function<void(const std::string&)> on_error,
    const nlohmann::json& tools
) {
    auto base_url = config_.base_url.empty() ? kDefaultBaseUrl : config_.base_url;

    std::string host;
    int port;
    std::string path_prefix;
    if (!parse_base_url(base_url, host, port, path_prefix)) {
        if (on_error) on_error("invalid base_url: " + base_url);
        return;
    }

    nlohmann::json body;
    body["model"] = config_.model;
    body["messages"] = messages;
    body["temperature"] = config_.temperature;
    body["max_tokens"] = config_.max_tokens;
    if (!tools.is_null()) {
        body["tools"] = tools;
    }
    std::string body_str = body.dump();

    auto ctx = std::make_shared<ChatCtx>();
    ctx->host = host;
    ctx->port = port;
    ctx->path = path_prefix + "/chat/completions";
    ctx->body = body_str;
    ctx->api_key = config_.api_key;
    ctx->timeout_sec = config_.timeout;

    scheduler_->run_blocking(
        [ctx]() {
            try {
                httplib::SSLClient cli(ctx->host, ctx->port);
                cli.set_connection_timeout(ctx->timeout_sec, 0);
                cli.set_read_timeout(ctx->timeout_sec, 0);
                cli.set_write_timeout(ctx->timeout_sec, 0);

                httplib::Headers headers = {
                    {"Content-Type", "application/json"},
                    {"Authorization", "Bearer " + ctx->api_key},
                };

                auto res = cli.Post(ctx->path, headers, ctx->body, "application/json");

                if (res) {
                    if (res->status == 200) {
                        ctx->result = nlohmann::json::parse(res->body);
                        ctx->ok = true;
                    } else {
                        std::string err_body = res->body.substr(0, 300);
                        ctx->error = "LLM API returned " + std::to_string(res->status)
                                   + ": " + err_body;
                        ctx->ok = false;
                    }
                } else {
                    auto err = res.error();
                    ctx->error = "LLM API connection failed: "
                               + httplib::to_string(err);
                    ctx->ok = false;
                }
            } catch (const std::exception& e) {
                ctx->error = "LLM exception: " + std::string(e.what());
                ctx->ok = false;
            }
        },
        [ctx, on_result, on_error]() {
            if (ctx->ok && on_result) {
                on_result(ctx->result);
            } else if (!ctx->ok && on_error) {
                on_error(ctx->error);
            }
        }
    );
}

bool DeepSeekEngine::parse_base_url(const std::string& url,
                                     std::string& host, int& port, std::string& path) {
    std::string s = url;
    size_t scheme_len = 0;
    if (s.rfind("https://", 0) == 0) {
        scheme_len = 8;
        port = 443;
    } else if (s.rfind("http://", 0) == 0) {
        scheme_len = 7;
        port = 80;
    } else {
        return false;
    }
    s = s.substr(scheme_len);

    auto slash = s.find('/');
    std::string host_port;
    if (slash == std::string::npos) {
        host_port = s;
        path = "";
    } else {
        host_port = s.substr(0, slash);
        path = s.substr(slash);
    }

    auto colon = host_port.find(':');
    if (colon == std::string::npos) {
        host = host_port;
    } else {
        host = host_port.substr(0, colon);
        port = std::stoi(host_port.substr(colon + 1));
    }
    return true;
}

void DeepSeekEngine::step() {
    scheduler_->step();
}

void DeepSeekEngine::run() {
    scheduler_->run();
}

} // namespace wheel
} // namespace lynne
