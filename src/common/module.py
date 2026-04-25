from abc import ABC, abstractmethod


class Module(ABC):
    @abstractmethod
    async def start(self) -> None: ...

    @abstractmethod
    async def stop(self) -> None: ...

    @abstractmethod
    def health_check(self) -> bool: ...

    @property
    @abstractmethod
    def name(self) -> str: ...
