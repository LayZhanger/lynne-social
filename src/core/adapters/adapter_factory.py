from src.wheel.logger import get_logger

from .base_adapter import BaseAdapter
from .adapter_models import AdapterConfig
from .imp.rednote_adapter import RedNoteAdapter


class AdapterFactory:
    """Stateless dispatcher: maps platform → adapter implementation."""

    def create(self, browser, config, *, llm_config=None) -> BaseAdapter:
        if not isinstance(config, AdapterConfig):
            config = AdapterConfig(platform="rednote")
        log = get_logger("adapter_factory")
        log.info("creating adapter for platform {}", config.platform)
        if config.platform == "rednote":
            return RedNoteAdapter(
                browser, config, llm_config=llm_config,
            )
        raise ValueError(f"unknown platform: {config.platform}")
