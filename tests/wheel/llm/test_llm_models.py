from src.wheel.llm.llm_models import LLMConfig


class TestLLMConfigUT:
    def test_defaults(self):
        cfg = LLMConfig()
        assert cfg.provider == "deepseek"
        assert cfg.api_key == ""
        assert cfg.base_url == ""
        assert cfg.model == "deepseek-chat"
        assert cfg.temperature == 0.7
        assert cfg.max_tokens == 4096
        assert cfg.timeout == 60

    def test_custom_values(self):
        cfg = LLMConfig(
            provider="openai",
            api_key="sk-123",
            base_url="https://api.openai.com/v1",
            model="gpt-4o",
            temperature=0.3,
            max_tokens=1024,
            timeout=30,
        )
        assert cfg.provider == "openai"
        assert cfg.api_key == "sk-123"
        assert cfg.base_url == "https://api.openai.com/v1"
        assert cfg.model == "gpt-4o"
        assert cfg.temperature == 0.3
        assert cfg.max_tokens == 1024
        assert cfg.timeout == 30

    def test_equality(self):
        a = LLMConfig()
        b = LLMConfig()
        assert a == b

    def test_inequality(self):
        a = LLMConfig(api_key="sk-a")
        b = LLMConfig(api_key="sk-b")
        assert a != b
