#include "wheel/ws_client/imp/ix_ws_client.h"
#include "wheel/scheduler/scheduler_factory.h"

#include <ixwebsocket/IXWebSocket.h>

namespace lynne {
namespace wheel {

IxWsClient::IxWsClient() {
    scheduler_ = SchedulerFactory().create();
}

IxWsClient::~IxWsClient() {
    ws_.stop();
    if (started_) {
        scheduler_->stop();
        scheduler_->step();
    }
    delete scheduler_;
}

std::string IxWsClient::name() const {
    return "ws_client";
}

void IxWsClient::start() {
    if (started_) return;
    scheduler_->start();
    started_ = true;
}

void IxWsClient::stop() {
    if (!started_) return;
    ws_.stop();
    state_ = WsReadyState::Closed;
    scheduler_->stop();
    scheduler_->step();
    started_ = false;
}

bool IxWsClient::health_check() {
    return started_;
}

void IxWsClient::connect(
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    OnMessageCallback on_message,
    OnErrorCallback on_error
) {
    on_message_ = std::move(on_message);
    on_error_ = std::move(on_error);
    state_ = WsReadyState::Connecting;

    ws_.setUrl(url);

    ix::WebSocketHttpHeaders ix_headers;
    for (auto& [k, v] : headers) {
        ix_headers[k] = v;
    }
    ws_.setExtraHeaders(ix_headers);
    ws_.disableAutomaticReconnection();

    auto self = this;
    ws_.setOnMessageCallback([self](const ix::WebSocketMessagePtr& msg) {
        switch (msg->type) {
            case ix::WebSocketMessageType::Open:
                self->state_ = WsReadyState::Open;
                break;
            case ix::WebSocketMessageType::Close:
                self->state_ = WsReadyState::Closed;
                break;
            case ix::WebSocketMessageType::Error:
                self->state_ = WsReadyState::Closed;
                self->scheduler_->post([self, reason = msg->errorInfo.reason]() {
                    if (self->on_error_) self->on_error_(reason);
                });
                break;
            case ix::WebSocketMessageType::Message:
                self->scheduler_->post([self, data = msg->str, binary = msg->binary]() {
                    if (self->on_message_) {
                        self->on_message_(WsMessage{data, binary});
                    }
                });
                break;
            default:
                break;
        }
    });

    ws_.start();
}

void IxWsClient::disconnect() {
    if (state_ == WsReadyState::Closed) return;
    ws_.stop();
    state_ = WsReadyState::Closed;
}

void IxWsClient::send(const std::string& data, bool is_binary) {
    if (is_binary) {
        ws_.sendBinary(data);
    } else {
        ws_.sendText(data);
    }
}

WsReadyState IxWsClient::ready_state() const {
    return state_;
}

void IxWsClient::step() {
    scheduler_->step();
}

void IxWsClient::run() {
    scheduler_->run();
}

} // namespace wheel
} // namespace lynne
