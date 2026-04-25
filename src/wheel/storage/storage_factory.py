from src.common import Factory
from .storage import Storage
from .storage_models import StorageConfig
from .imp.jsonl_storage import JsonlStorage


class StorageFactory(Factory[Storage]):
    def create(self, config: object) -> Storage:
        if isinstance(config, StorageConfig):
            return JsonlStorage(config.data_dir)
        if isinstance(config, str):
            return JsonlStorage(config)
        return JsonlStorage()
