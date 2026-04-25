import pytest
from pydantic import ValidationError

from src.wheel.config.config_models import (
    BrowserConfig,
    Config,
    LLMConfig,
    PlatformConfig,
    ServerConfig,
    TaskConfig,
)


class TestServerConfigUT:
    def test_defaults(self):
        cfg = ServerConfig()
        assert cfg.port == 7890
        assert cfg.auto_open_browser is True

    def test_custom(self):
        cfg = ServerConfig(port=3000, auto_open_browser=False)
        assert cfg.port == 3000
        assert cfg.auto_open_browser is False


class TestLLMConfigUT:
    def test_defaults(self):
        cfg = LLMConfig()
        assert cfg.provider == "deepseek"
        assert cfg.model == "deepseek-chat"
        assert cfg.temperature == 0.7
        assert cfg.max_tokens == 4096
        assert cfg.timeout == 60
        assert cfg.api_key == ""

    def test_api_key_assignment(self):
        cfg = LLMConfig(api_key="sk-xxx")
        assert cfg.api_key == "sk-xxx"

    def test_provider_custom(self):
        cfg = LLMConfig(provider="openai", base_url="https://api.openai.com/v1")
        assert cfg.provider == "openai"
        assert cfg.base_url == "https://api.openai.com/v1"


class TestBrowserConfigUT:
    def test_defaults(self):
        cfg = BrowserConfig()
        assert cfg.headless is False
        assert cfg.slow_mo == 500
        assert cfg.viewport_width == 1920
        assert cfg.viewport_height == 1080
        assert cfg.locale == "zh-CN"
        assert cfg.timeout == 30000

    def test_headless(self):
        cfg = BrowserConfig(headless=True)
        assert cfg.headless is True


class TestPlatformConfigUT:
    def test_defaults(self):
        cfg = PlatformConfig()
        assert cfg.enabled is False
        assert cfg.session_file == ""
        assert cfg.base_url == ""


class TestTaskConfigUT:
    def test_requires_name(self):
        with pytest.raises(ValidationError):
            TaskConfig()

    def test_defaults_with_name(self):
        cfg = TaskConfig(name="test_task")
        assert cfg.name == "test_task"
        assert cfg.platforms == []
        assert cfg.intent == ""
        assert cfg.schedule == "manual"

    def test_full(self):
        cfg = TaskConfig(
            name="AI监控",
            platforms=["twitter", "rednote"],
            intent="大模型进展",
            schedule="every 4 hours",
        )
        assert cfg.name == "AI监控"
        assert len(cfg.platforms) == 2
        assert cfg.intent == "大模型进展"


class TestConfigUT:
    def test_empty_config(self):
        cfg = Config()
        assert cfg.server.port == 7890
        assert cfg.llm.provider == "deepseek"
        assert cfg.browser.headless is False
        assert cfg.platforms == {}
        assert cfg.tasks == []

    def test_validate_from_dict(self):
        data = {
            "server": {"port": 9999},
            "llm": {"provider": "openai", "api_key": "sk-test"},
        }
        cfg = Config.model_validate(data)
        assert cfg.server.port == 9999
        assert cfg.llm.provider == "openai"

    def test_validate_from_full_dict(self):
        data = {
            "server": {"port": 8888, "auto_open_browser": False},
            "llm": {
                "provider": "deepseek",
                "api_key": "${DEEPSEEK_API_KEY}",
                "model": "deepseek-chat",
                "temperature": 0.3,
                "max_tokens": 4096,
                "timeout": 90,
            },
            "browser": {
                "headless": True,
                "slow_mo": 200,
                "viewport_width": 1280,
                "viewport_height": 720,
            },
            "platforms": {
                "twitter": {
                    "enabled": True,
                    "session_file": "data/sessions/twitter.json",
                    "base_url": "https://x.com",
                }
            },
            "tasks": [
                {
                    "name": "AI动态",
                    "platforms": ["twitter"],
                    "intent": "关注AI新闻",
                    "schedule": "every 4 hours",
                }
            ],
        }
        cfg = Config.model_validate(data)
        assert cfg.server.port == 8888
        assert cfg.browser.headless is True
        assert cfg.platforms["twitter"].enabled is True
        assert len(cfg.tasks) == 1
        assert cfg.tasks[0].name == "AI动态"

    def test_json_serialization(self):
        cfg = Config(server=ServerConfig(port=5555))
        data = cfg.model_dump()
        assert data["server"]["port"] == 5555

    def test_platforms_default_type(self):
        cfg = Config()
        cfg.platforms["twitter"] = PlatformConfig(enabled=True)
        assert cfg.platforms["twitter"].enabled is True

    def test_tasks_default_type(self):
        cfg = Config()
        cfg.tasks.append(TaskConfig(name="t1"))
        assert len(cfg.tasks) == 1
