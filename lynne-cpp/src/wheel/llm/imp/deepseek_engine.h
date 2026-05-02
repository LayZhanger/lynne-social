#pragma once

#include "wheel/llm/llm_engine.h"
#include "wheel/llm/llm_models.h"
#include "wheel/scheduler/scheduler.h"

#include <json.hpp>
#include <string>
#include <memory>

namespace lynne {
namespace wheel {

class DeepSeekEngine : public LLMEngine {
public:
    explicit DeepSeekEngine(const LLMConfig& config);
    ~DeepSeekEngine() override;

    std::string name() const override;
    void start() override;
    void stop() override;
    bool health_check() override;

    void chat(
        const nlohmann::json& messages,
        std::function<void(nlohmann::json)> on_result,
        std::function<void(const std::string&)> on_error,
        const nlohmann::json& tools = nullptr
    ) override;
    void step() override;
    void run() override;

private:
    struct ChatCtx {
        std::string host;
        int port = 443;
        std::string path;
        std::string body;
        std::string api_key;
        int timeout_sec = 60;
        std::string ca_cert_path;
        nlohmann::json result;
        bool ok = false;
        std::string error;
    };

    LLMConfig config_;
    Scheduler* scheduler_ = nullptr;
    bool started_ = false;

public:
    static bool parse_base_url(const std::string& url, std::string& host, int& port, std::string& path);
};

} // namespace wheel
} // namespace lynne
