from src.common import Factory
from src.core.adapters.adapter_factory import AdapterFactory
from src.core.adapters.adapter_models import AdapterConfig
from src.core.adapters.base_adapter import BaseAdapter
from src.core.adapters.imp.rednote_adapter import RedNoteAdapter
from src.wheel.browser.browser_manager import BrowserManager
from src.wheel.browser.browser_models import BrowserConfig


class _StubBrowserManager(BrowserManager):
    async def get_context(self, platform: str):
        raise NotImplementedError

    async def save_session(self, platform: str):
        raise NotImplementedError

    async def login_flow(self, platform: str, url: str):
        raise NotImplementedError

    async def set_login_complete(self, platform: str):
        raise NotImplementedError

    async def start(self):
        pass

    async def stop(self):
        pass

    def health_check(self):
        return True

    @property
    def name(self):
        return "stub"


class _StubBrowserFactory(Factory[BrowserManager]):
    def create(self, config: object) -> BrowserManager:
        return _StubBrowserManager()


class TestAdapterFactoryUT:
    def test_create_rednote_deafult(self):
        factory = AdapterFactory(
            browser_factory=_StubBrowserFactory(),
            browser_config=BrowserConfig(),
        )
        adapter = factory.create(None)
        assert isinstance(adapter, RedNoteAdapter)
        assert isinstance(adapter, BaseAdapter)
        assert adapter.platform_name == "rednote"

    def test_create_rednote_with_config(self):
        factory = AdapterFactory(
            browser_factory=_StubBrowserFactory(),
            browser_config=BrowserConfig(),
        )
        cfg = AdapterConfig(platform="rednote", max_scrolls=5)
        adapter = factory.create(cfg)
        assert isinstance(adapter, RedNoteAdapter)
        assert adapter._config.max_scrolls == 5

    def test_create_unknown_platform_raises(self):
        factory = AdapterFactory(
            browser_factory=_StubBrowserFactory(),
            browser_config=BrowserConfig(),
        )
        import pytest

        with pytest.raises(ValueError, match="unknown platform"):
            factory.create(AdapterConfig(platform="douyin"))

    def test_create_non_config_uses_default(self):
        factory = AdapterFactory(
            browser_factory=_StubBrowserFactory(),
            browser_config=BrowserConfig(),
        )
        adapter = factory.create("not a config")
        assert isinstance(adapter, RedNoteAdapter)
        assert adapter._config.max_scrolls == 20
