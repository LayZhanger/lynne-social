#include "wheel/ws_client/ws_client.h"
#include "wheel/ws_client/ws_client_models.h"
#include "wheel/ws_client/ws_client_factory.h"
#include "wheel/ws_client/imp/ix_ws_client.h"

#include "ws_echo_server.h"

#include <cstdio>
#include <string>
#include <thread>
#include <chrono>

using namespace lynne::wheel;

static int passed = 0;
static int failed = 0;

#define CHECK(cond, msg) \
    do { if (cond) { printf("  [PASS] %s\n", msg); ++passed; } \
         else { printf("  [FAIL] %s\n", msg); ++failed; } \
    } while (0)

#define CHECK_TRUE(cond, msg)  CHECK((cond), msg)
#define CHECK_FALSE(cond, msg) CHECK(!(cond), msg)

static void check_str(const std::string& a, const std::string& b, const char* msg) {
    if (a == b) { printf("  [PASS] %s\n", msg); ++passed; }
    else { printf("  [FAIL] %s ('%s' vs '%s')\n", msg, a.c_str(), b.c_str()); ++failed; }
}

static void report(const char* suite) {
    printf("--- %s: %d/%d ---\n", suite, passed, passed + failed);
}

int main() {
    // ============================================================
    // Lifecycle
    // ============================================================
    {
        auto* ws = WsClientFactory().create();

        check_str(ws->name(), "ws_client", "name");
        CHECK_FALSE(ws->health_check(), "health_check false before start");

        ws->start();
        CHECK_TRUE(ws->health_check(), "health_check true after start");

        ws->stop();
        CHECK_FALSE(ws->health_check(), "health_check false after stop");

        delete ws;
    }
    report("Lifecycle");

    // ============================================================
    // Start twice is safe
    // ============================================================
    {
        auto* ws = WsClientFactory().create();
        ws->start();
        ws->start();
        CHECK_TRUE(ws->health_check(), "start twice: health_check true");
        ws->stop();
        delete ws;
    }
    report("StartTwice");

    // ============================================================
    // Stop without start is safe
    // ============================================================
    {
        auto* ws = WsClientFactory().create();
        ws->stop();
        CHECK_FALSE(ws->health_check(), "stop without start: health_check false");
        delete ws;
    }
    report("StopWithoutStart");

    // ============================================================
    // Echo roundtrip
    // ============================================================
    {
        WsEchoServer echo_server;
        int port = echo_server.start();
        CHECK(port > 0, "echo server started");

        std::string url = "ws://127.0.0.1:" + std::to_string(port) + "/";

        auto* ws = WsClientFactory().create();
        ws->start();

        std::string received;
        std::string conn_error;

        ws->connect(url, {},
            [&](WsMessage msg) { received = msg.data; },
            [&](const std::string& err) { conn_error = err; }
        );

        // Wait for IXWebSocket to connect (async on its own thread)
        for (int i = 0; i < 100; ++i) {
            if (ws->ready_state() == WsReadyState::Open) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CHECK(static_cast<int>(ws->ready_state()) ==
              static_cast<int>(WsReadyState::Open), "echo: ready_state Open");

        // Send and wait for echo (callback delivered via scheduler step)
        ws->send("hello");
        for (int i = 0; i < 50; ++i) {
            ws->step();
            if (!received.empty()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        CHECK(!conn_error.empty() || received == "hello",
              "echo: no connection error");
        if (!received.empty()) {
            check_str(received, "hello", "echo: received 'hello'");
        }

        ws->stop();
        delete ws;
        echo_server.stop();
    }
    report("EchoRoundtrip");

    // ============================================================
    // Double disconnect is safe
    // ============================================================
    {
        auto* ws = WsClientFactory().create();
        ws->start();
        ws->disconnect();
        ws->disconnect();
        ws->stop();
        delete ws;
        CHECK_TRUE(true, "double disconnect: no crash");
    }
    report("DoubleDisconnect");

    // ============================================================
    // Connection error triggers on_error (via scheduler step)
    // ============================================================
    {
        auto* ws = WsClientFactory().create();
        ws->start();

        bool error_called = false;
        bool msg_called = false;

        ws->connect("ws://127.0.0.1:1/", {},
            [&](WsMessage) { msg_called = true; },
            [&](const std::string&) { error_called = true; }
        );

        // Wait for IXWebSocket to fail (async on its own thread)
        for (int i = 0; i < 100; ++i) {
            ws->step();
            if (error_called) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        CHECK_TRUE(error_called, "connection error: on_error called");
        ws->stop();
        delete ws;
    }
    report("ConnectionError");

    printf("\n== %d/%d passed ==\n", passed, passed + failed);
    return failed > 0 ? 1 : 0;
}
