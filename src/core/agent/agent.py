from abc import abstractmethod

from src.common import Module
from src.common.models import TaskResult


class Agent(Module):
    @abstractmethod
    async def run(
        self,
        intent: str,
        platforms: list[str],
        max_steps: int = 10,
        history_dates: list[str] | None = None,
    ) -> TaskResult: ...
