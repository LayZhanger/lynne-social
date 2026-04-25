from dataclasses import dataclass


@dataclass
class StorageConfig:
    data_dir: str = "data"
