from src.wheel.storage.storage_factory import StorageFactory
from src.wheel.storage.storage import Storage
from src.wheel.storage.storage_models import StorageConfig
from src.wheel.storage.imp.jsonl_storage import JsonlStorage


class TestStorageFactoryUT:
    def test_create_default(self):
        storage = StorageFactory().create(None)
        assert isinstance(storage, JsonlStorage)

    def test_create_with_storage_config(self):
        storage = StorageFactory().create(StorageConfig(data_dir="/tmp/test_data"))
        assert isinstance(storage, JsonlStorage)

    def test_create_with_string(self):
        storage = StorageFactory().create("/custom/path")
        assert isinstance(storage, JsonlStorage)

    def test_create_returns_storage_interface(self):
        storage = StorageFactory().create(None)
        assert isinstance(storage, Storage)
