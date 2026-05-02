#pragma once

#include "common/module.h"
#include "wheel/ws_client/ws_client_models.h"

#include <functional>
#include <map>
#include <string>

namespace lynne {
namespace wheel {

class WsClient : public common::Module {
public:
    using OnMessageCallback = std::function<void(WsMessage)>;
    using OnErrorCallback = std::function<void(const std::string&)>;

    virtual void connect(
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        OnMessageCallback on_message,
        OnErrorCallback on_error
    ) = 0;

    virtual void disconnect() = 0;
    virtual void send(const std::string& data, bool is_binary = false) = 0;
    virtual WsReadyState ready_state() const = 0;

    virtual void step() = 0;
    virtual void run() = 0;
};

} // namespace wheel
} // namespace lynne
