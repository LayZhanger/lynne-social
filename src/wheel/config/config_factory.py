from src.common import Factory
from .config_loader import ConfigLoader
from .imp.yaml_config_loader import YamlConfigLoader


class ConfigLoaderFactory(Factory[ConfigLoader]):
    def create(self, config: object) -> ConfigLoader:
        if isinstance(config, str):
            return YamlConfigLoader(config)
        return YamlConfigLoader()
