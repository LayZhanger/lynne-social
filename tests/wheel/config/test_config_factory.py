import os

from src.wheel.config.config_factory import ConfigLoaderFactory
from src.wheel.config.config_loader import ConfigLoader
from src.wheel.config.imp.yaml_config_loader import YamlConfigLoader


class TestConfigLoaderFactoryUT:
    def test_create_default(self, monkeypatch):
        monkeypatch.setenv("DEEPSEEK_API_KEY", "sk-test")
        loader = ConfigLoaderFactory().create(None)
        assert isinstance(loader, YamlConfigLoader)

    def test_create_with_path(self, tmp_path, monkeypatch):
        monkeypatch.setenv("DEEPSEEK_API_KEY", "sk-test")
        path = str(tmp_path / "custom.yaml")
        path_obj = tmp_path / "custom.yaml"
        path_obj.write_text("", encoding="utf-8")
        loader = ConfigLoaderFactory().create(path)
        assert isinstance(loader, YamlConfigLoader)

    def test_create_returns_config_loader(self, monkeypatch):
        monkeypatch.setenv("DEEPSEEK_API_KEY", "sk-test")
        loader = ConfigLoaderFactory().create(None)
        assert isinstance(loader, ConfigLoader)
