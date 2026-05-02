#include "wheel/storage/storage_models.h"
#include "wheel/storage/storage.h"
#include "wheel/storage/storage_factory.h"
#include "wheel/storage/imp/jsonl_storage.h"

#include <gtest/gtest.h>

using namespace lynne::wheel;

// ============================================================
// StorageConfig — defaults
// ============================================================

TEST(StorageConfigDefaults, DefaultDataDir) {
    StorageConfig c{};
    EXPECT_EQ(c.data_dir, "data");
}

TEST(StorageConfigDefaults, CustomDataDir) {
    StorageConfig c{"/tmp/mydata"};
    EXPECT_EQ(c.data_dir, "/tmp/mydata");
}

// ============================================================
// StorageConfig — from_json
// ============================================================

TEST(StorageConfigJson, FromJsonMinimal) {
    auto j = nlohmann::json::parse("{}");
    StorageConfig c;
    from_json(j, c);
    EXPECT_EQ(c.data_dir, "data");
}

TEST(StorageConfigJson, FromJsonCustom) {
    auto j = nlohmann::json::parse(R"({"data_dir": "/custom/path"})");
    StorageConfig c;
    from_json(j, c);
    EXPECT_EQ(c.data_dir, "/custom/path");
}

// ============================================================
// StorageFactory
// ============================================================

TEST(StorageFactory, CreateDefault) {
    StorageFactory factory;
    auto* s = factory.create();
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->name(), "JsonlStorage");
    delete s;
}

TEST(StorageFactory, CreateWithPath) {
    StorageFactory factory;
    auto* s = factory.create("/tmp/test_data");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->name(), "JsonlStorage");
    delete s;
}

TEST(StorageFactory, CreateWithConfig) {
    StorageConfig cfg{"/tmp/test_cfg"};
    StorageFactory factory;
    auto* s = factory.create(cfg);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->name(), "JsonlStorage");
    delete s;
}
