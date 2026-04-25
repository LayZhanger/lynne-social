from abc import ABC, abstractmethod


class Tool(ABC):
    name: str = ""
    description: str = ""
    args_schema: dict = {}

    @abstractmethod
    async def execute(self, **kwargs) -> str: ...
