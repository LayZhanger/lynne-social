from src.common import Factory
from src.core.browser.browser_manager import BrowserManager
from src.core.browser.browser_models import BrowserConfig
from src.wheel.logger import get_logger

from .base_adapter import BaseAdapter
from .adapter_models import AdapterConfig
from .imp.rednote_adapter import RedNoteAdapter


class AdapterFactory(Factory[BaseAdapter]):
    def __init__(self, browser_factory: Factory[BrowserManager], browser_config: BrowserConfig):
        self._browser_factory = browser_factory
        self._browser_config = browser_config
        self._log = get_logger("adapter_factory")

    def create(self, config: object) -> BaseAdapter:
        if not isinstance(config, AdapterConfig):
            config = AdapterConfig()
        browser = self._browser_factory.create(self._browser_config)
        self._log.info("creating adapter for platform {}", config.platform)
        if config.platform == "rednote":
            return RedNoteAdapter(browser, config)
        raise ValueError(f"unknown platform: {config.platform}")
