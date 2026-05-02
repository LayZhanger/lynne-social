#include "wheel/ws_client/ws_client_factory.h"
#include "wheel/ws_client/imp/ix_ws_client.h"

namespace lynne {
namespace wheel {

WsClient* WsClientFactory::create() const {
    return new IxWsClient();
}

} // namespace wheel
} // namespace lynne
