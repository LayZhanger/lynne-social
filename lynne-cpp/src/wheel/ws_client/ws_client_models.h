#pragma once

#include <string>

namespace lynne {
namespace wheel {

enum class WsReadyState {
    Connecting,
    Open,
    Closing,
    Closed
};

struct WsMessage {
    std::string data;
    bool is_binary = false;
};

} // namespace wheel
} // namespace lynne
