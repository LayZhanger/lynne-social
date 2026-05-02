#include "wheel/ws_client/ws_client_models.h"
#include "wheel/ws_client/ws_client.h"
#include "wheel/ws_client/ws_client_factory.h"
#include "wheel/ws_client/imp/ix_ws_client.h"

#include <gtest/gtest.h>

using namespace lynne::wheel;

TEST(WsMessageDefaults, DefaultFields) {
    WsMessage m{};
    EXPECT_EQ(m.data, "");
    EXPECT_FALSE(m.is_binary);
}

TEST(WsMessageDefaults, CustomFields) {
    WsMessage m{"hello", true};
    EXPECT_EQ(m.data, "hello");
    EXPECT_TRUE(m.is_binary);
}

TEST(WsReadyState, EnumValues) {
    EXPECT_NE(static_cast<int>(WsReadyState::Connecting),
              static_cast<int>(WsReadyState::Closed));
}

TEST(WsClientFactory, CreateReturnsNonNull) {
    WsClientFactory factory;
    auto* client = factory.create();
    ASSERT_NE(client, nullptr);
    delete client;
}

TEST(WsClientFactory, CreateReturnsIxWsClient) {
    WsClientFactory factory;
    auto* client = factory.create();
    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->name(), "ws_client");
    delete client;
}
