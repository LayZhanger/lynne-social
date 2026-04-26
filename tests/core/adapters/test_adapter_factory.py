from src.core.adapters.adapter_factory import AdapterFactory
from src.core.adapters.adapter_models import AdapterConfig
from src.core.adapters.base_adapter import BaseAdapter
from src.core.adapters.imp.rednote_adapter import RedNoteAdapter
from src.wheel.browser.browser_manager import BrowserManager


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


class TestAdapterFactoryUT:
    def test_create_rednote_default(self):
        factory = AdapterFactory()
        browser = _StubBrowserManager()
        adapter = factory.create(browser, None)
        assert isinstance(adapter, RedNoteAdapter)
        assert isinstance(adapter, BaseAdapter)
        assert adapter.platform_name == "rednote"

    def test_create_rednote_with_config(self):
        factory = AdapterFactory()
        browser = _StubBrowserManager()
        cfg = AdapterConfig(platform="rednote", max_scrolls=5)
        adapter = factory.create(browser, cfg)
        assert isinstance(adapter, RedNoteAdapter)
        assert adapter._config.max_scrolls == 5

    def test_create_unknown_platform_raises(self):
        factory = AdapterFactory()
        browser = _StubBrowserManager()
        import pytest

        with pytest.raises(ValueError, match="unknown platform"):
            factory.create(browser, AdapterConfig(platform="douyin"))

    def test_create_non_config_uses_default(self):
        factory = AdapterFactory()
        browser = _StubBrowserManager()
        adapter = factory.create(browser, "not a config")
        assert isinstance(adapter, RedNoteAdapter)
        assert adapter._config.max_scrolls == 20

    def test_llm_config_passed_through(self):
        from src.wheel.llm.llm_models import LLMConfig

        factory = AdapterFactory()
        browser = _StubBrowserManager()
        llm_cfg = LLMConfig(api_key="sk-test")
        adapter = factory.create(browser, None, llm_config=llm_cfg)
        assert adapter._llm_config is llm_cfg
