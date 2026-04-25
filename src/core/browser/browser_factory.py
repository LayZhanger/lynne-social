from src.common import Factory

from .browser_manager import BrowserManager
from .browser_models import BrowserConfig
from .imp.playwright_browser_manager import PlaywrightBrowserManager


class BrowserManagerFactory(Factory[BrowserManager]):
    def create(self, config: object) -> BrowserManager:
        if isinstance(config, BrowserConfig):
            return PlaywrightBrowserManager(config)
        return PlaywrightBrowserManager(BrowserConfig())
