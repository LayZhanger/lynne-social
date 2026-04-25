from src.common import Factory

from .llm_engine import LLMEngine
from .llm_models import LLMConfig
from .imp.deepseek_engine import DeepSeekEngine


class LLMEngineFactory(Factory[LLMEngine]):
    def create(self, config: object) -> LLMEngine:
        if not isinstance(config, LLMConfig):
            config = LLMConfig()
        if not config.api_key:
            raise ValueError("LLMConfig.api_key is required")
        return DeepSeekEngine(config)
