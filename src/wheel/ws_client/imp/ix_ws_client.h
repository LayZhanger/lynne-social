#pragma once

#include "wheel/ws_client/ws_client.h"
#include "wheel/scheduler/scheduler.h"

#include <ixwebsocket/IXWebSocket.h>
#include <map>
#include <memory>
#include <string>

namespace lynne {
namespace wheel {

class IxWsClient : public WsClient {
public:
    IxWsClient();
    ~IxWsClient() override;

    std::string name() const override;
    void start() override;
    void stop() override;
    bool health_check() override;

    void connect(
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        OnMessageCallback on_message,
        OnErrorCallback on_error
    ) override;

    void disconnect() override;
    void send(const std::string& data, bool is_binary) override;
    WsReadyState ready_state() const override;
    void step() override;
    void run() override;

private:
    ix::WebSocket ws_;
    Scheduler* scheduler_ = nullptr;
    bool started_ = false;
    OnMessageCallback on_message_;
    OnErrorCallback on_error_;
    WsReadyState state_ = WsReadyState::Closed;
};

} // namespace wheel
} // namespace lynne
