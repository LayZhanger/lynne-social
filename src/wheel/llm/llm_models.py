from dataclasses import dataclass


@dataclass
class LLMConfig:
    provider: str = "deepseek"
    api_key: str = ""
    base_url: str = ""
    model: str = "deepseek-chat"
    temperature: float = 0.7
    max_tokens: int = 4096
    timeout: int = 60
