#include "ws_echo_server.h"

#include <cstdio>

WsEchoServer::WsEchoServer() {
}

WsEchoServer::~WsEchoServer() {
    stop();
}

int WsEchoServer::start() {
    if (running_) return port_;

    // Try ports 21000-21099 until one works
    for (int try_port = 21000; try_port < 21100; ++try_port) {
        auto* srv = new ix::WebSocketServer(try_port);
        srv->setOnClientMessageCallback(
            [](std::shared_ptr<ix::ConnectionState> /*connectionState*/,
               ix::WebSocket& client,
               const ix::WebSocketMessagePtr& msg) {
                if (msg->type == ix::WebSocketMessageType::Message) {
                    client.sendText(msg->str);
                }
            }
        );

        if (srv->listenAndStart()) {
            server_ = srv;
            port_ = try_port;
            running_ = true;
            return port_;
        }
        delete srv;
    }

    printf("  [FAIL] echo server: no available port\n");
    return -1;
}

void WsEchoServer::stop() {
    if (!running_) return;
    server_->stop();
    delete server_;
    server_ = nullptr;
    running_ = false;
}

int WsEchoServer::port() const {
    return port_;
}

bool WsEchoServer::is_running() const {
    return running_;
}
