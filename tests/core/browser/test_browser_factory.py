from src.core.browser.browser_factory import BrowserManagerFactory
from src.core.browser.browser_manager import BrowserManager
from src.core.browser.browser_models import BrowserConfig
from src.core.browser.imp.playwright_browser_manager import PlaywrightBrowserManager


class TestBrowserManagerFactoryUT:
    def test_create_default(self):
        manager = BrowserManagerFactory().create(None)
        assert isinstance(manager, PlaywrightBrowserManager)
        assert isinstance(manager, BrowserManager)

    def test_create_with_config(self):
        cfg = BrowserConfig(headless=True, slow_mo=0)
        manager = BrowserManagerFactory().create(cfg)
        assert isinstance(manager, PlaywrightBrowserManager)
        assert manager._config.headless is True
        assert manager._config.slow_mo == 0

    def test_create_with_non_config_returns_default(self):
        manager = BrowserManagerFactory().create("not a config")
        assert isinstance(manager, PlaywrightBrowserManager)
        assert manager._config.headless is False

    def test_create_returns_browser_manager_interface(self):
        manager = BrowserManagerFactory().create(BrowserConfig())
        assert isinstance(manager, BrowserManager)
