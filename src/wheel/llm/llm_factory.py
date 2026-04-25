from src.common import Factory

from .llm_engine import LLMEngine
from .llm_models import LLMConfig


class LLMEngineFactory(Factory[LLMEngine]):
    def create(self, config: object) -> LLMEngine:
        raise NotImplementedError
