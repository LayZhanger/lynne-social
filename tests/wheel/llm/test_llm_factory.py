import pytest

from src.wheel.llm.llm_factory import LLMEngineFactory
from src.wheel.llm.llm_models import LLMConfig
from src.wheel.llm.imp.deepseek_engine import DeepSeekEngine


class TestLLMEngineFactoryUT:
    def test_create_with_config(self):
        factory = LLMEngineFactory()
        cfg = LLMConfig(api_key="sk-test")
        engine = factory.create(cfg)
        assert isinstance(engine, DeepSeekEngine)
        assert engine.name == "llm(deepseek:deepseek-chat)"

    def test_create_non_config_uses_default(self):
        factory = LLMEngineFactory()
        engine = factory.create(LLMConfig(api_key="sk-test"))
        assert isinstance(engine, DeepSeekEngine)

    def test_create_missing_api_key_raises(self):
        factory = LLMEngineFactory()
        with pytest.raises(ValueError, match="api_key"):
            factory.create(LLMConfig(api_key=""))

    def test_create_with_custom_model(self):
        factory = LLMEngineFactory()
        cfg = LLMConfig(api_key="sk-test", model="deepseek-reasoner")
        engine = factory.create(cfg)
        assert "deepseek-reasoner" in engine.name
