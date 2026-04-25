from abc import ABC, abstractmethod


class ConfigLoader(ABC):
    @abstractmethod
    async def load(self) -> object: ...

    @abstractmethod
    async def reload(self) -> object: ...
