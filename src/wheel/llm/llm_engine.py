from abc import abstractmethod

from src.common import Module


class LLMEngine(Module):
    @abstractmethod
    async def chat(self, messages: list[dict], tools: list[dict] | None = None) -> dict: ...
