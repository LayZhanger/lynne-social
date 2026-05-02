#pragma once

#include <ixwebsocket/IXWebSocketServer.h>
#include <string>

class WsEchoServer {
public:
    WsEchoServer();
    ~WsEchoServer();

    int start();
    void stop();
    int port() const;
    bool is_running() const;

private:
    ix::WebSocketServer* server_ = nullptr;
    int port_ = 0;
    bool running_ = false;
};
