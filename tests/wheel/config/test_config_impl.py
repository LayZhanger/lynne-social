import os

import pytest

from src.wheel.config.imp.yaml_config_loader import (
    YamlConfigLoader,
    _resolve_env,
    _resolve_env_recursive,
)
from src.wheel.config.config_models import Config


class TestEnvResolutionUT:
    def test_resolve_env_value(self, monkeypatch):
        monkeypatch.setenv("TEST_KEY", "test_value")
        result = _resolve_env("prefix_${TEST_KEY}_suffix")
        assert result == "prefix_test_value_suffix"

    def test_resolve_env_missing_var(self):
        with pytest.raises(ValueError, match="MISSING_VAR"):
            _resolve_env("${MISSING_VAR}")

    def test_resolve_env_no_placeholder(self):
        result = _resolve_env("plain_text")
        assert result == "plain_text"

    def test_resolve_env_recursive_dict(self, monkeypatch):
        monkeypatch.setenv("KEY", "val")
        data = {"a": "${KEY}", "b": {"c": "${KEY}"}}
        result = _resolve_env_recursive(data)
        assert result == {"a": "val", "b": {"c": "val"}}

    def test_resolve_env_recursive_list(self, monkeypatch):
        monkeypatch.setenv("KEY", "val")
        data = ["${KEY}", "plain"]
        result = _resolve_env_recursive(data)
        assert result == ["val", "plain"]

    def test_resolve_env_recursive_non_string(self):
        result = _resolve_env_recursive(42)
        assert result == 42


class TestYamlConfigLoaderTA:
    def test_load_missing_file_returns_defaults(self, tmp_path):
        loader = YamlConfigLoader(str(tmp_path / "nonexistent.yaml"))
        cfg = loader.load()
        assert isinstance(cfg, Config)
        assert cfg.server.port == 7890
        assert cfg.llm.provider == "deepseek"

    def test_load_yaml_structure(self, sample_yaml_file):
        loader = YamlConfigLoader(str(sample_yaml_file))
        cfg = loader.load()
        assert cfg.server.port == 8888
        assert cfg.server.auto_open_browser is False
        assert cfg.llm.provider == "openai"
        assert cfg.llm.model == "gpt-4"
        assert cfg.browser.headless is True
        assert cfg.browser.slow_mo == 200

    def test_load_platforms(self, sample_yaml_file):
        loader = YamlConfigLoader(str(sample_yaml_file))
        cfg = loader.load()
        assert "twitter" in cfg.platforms
        assert cfg.platforms["twitter"].enabled is True
        assert cfg.platforms["twitter"].base_url == "https://x.com"

    def test_load_tasks(self, sample_yaml_file):
        loader = YamlConfigLoader(str(sample_yaml_file))
        cfg = loader.load()
        assert len(cfg.tasks) == 1
        task = cfg.tasks[0]
        assert task.name == "AI动态"
        assert task.platforms == ["twitter"]
        assert task.intent == "关注AI大模型的最新进展"
        assert task.schedule == "every 4 hours"

    def test_reload(self, sample_yaml_file, tmp_path):
        loader = YamlConfigLoader(str(sample_yaml_file))
        cfg1 = loader.load()
        assert cfg1.server.port == 8888

        new_content = "server:\n  port: 9999\n"
        sample_yaml_file.write_text(new_content, encoding="utf-8")

        cfg2 = loader.reload()
        assert cfg2.server.port == 9999

    def test_config_property_after_load(self, sample_yaml_file):
        loader = YamlConfigLoader(str(sample_yaml_file))
        loader.load()
        assert loader.config.server.port == 8888

    def test_env_var_substitution(self, tmp_path, monkeypatch):
        monkeypatch.setenv("API_KEY", "sk-secret-123")
        content = "llm:\n  api_key: ${API_KEY}\n"
        path = tmp_path / "config.yaml"
        path.write_text(content, encoding="utf-8")

        loader = YamlConfigLoader(str(path))
        cfg = loader.load()
        assert cfg.llm.api_key == "sk-secret-123"

    def test_env_var_in_nested_dict(self, tmp_path, monkeypatch):
        monkeypatch.setenv("TOKEN", "tk-abc")
        content = (
            "platforms:\n"
            "  twitter:\n"
            "    enabled: true\n"
            "    session_file: ${TOKEN}\n"
        )
        path = tmp_path / "config.yaml"
        path.write_text(content, encoding="utf-8")

        loader = YamlConfigLoader(str(path))
        cfg = loader.load()
        assert cfg.platforms["twitter"].session_file == "tk-abc"

    def test_env_var_in_list(self, tmp_path, monkeypatch):
        monkeypatch.setenv("PLATFORM", "twitter")
        content = "tasks:\n  - name: test\n    platforms:\n      - ${PLATFORM}\n"
        path = tmp_path / "config.yaml"
        path.write_text(content, encoding="utf-8")

        loader = YamlConfigLoader(str(path))
        cfg = loader.load()
        assert cfg.tasks[0].platforms == ["twitter"]

    def test_config_property_raises_before_load(self, tmp_path):
        loader = YamlConfigLoader(str(tmp_path / "nofile.yaml"))
        loader.load()
        assert loader.config is not None

    def test_empty_yaml_file(self, tmp_path):
        path = tmp_path / "empty.yaml"
        path.write_text("", encoding="utf-8")
        loader = YamlConfigLoader(str(path))
        cfg = loader.load()
        assert isinstance(cfg, Config)
        assert cfg.server.port == 7890

    def test_none_yaml_file(self, tmp_path):
        path = tmp_path / "none.yaml"
        path.write_text("null\n", encoding="utf-8")
        loader = YamlConfigLoader(str(path))
        cfg = loader.load()
        assert isinstance(cfg, Config)
