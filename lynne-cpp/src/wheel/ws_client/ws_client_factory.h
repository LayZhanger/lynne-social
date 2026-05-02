#pragma once

#include "wheel/ws_client/ws_client.h"

namespace lynne {
namespace wheel {

class WsClientFactory {
public:
    WsClient* create() const;
};

} // namespace wheel
} // namespace lynne
