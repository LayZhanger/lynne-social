from abc import ABC, abstractmethod
from typing import Any

from src.common import Module


class BrowserPage(ABC):
    @abstractmethod
    async def goto(self, url: str, **kwargs) -> None: ...

    @abstractmethod
    async def close(self) -> None: ...

    @abstractmethod
    def raw(self) -> Any: ...


class BrowserContext(ABC):
    @abstractmethod
    async def new_page(self) -> BrowserPage: ...

    @abstractmethod
    async def close(self) -> None: ...

    @abstractmethod
    async def storage_state(self) -> dict: ...

    @property
    @abstractmethod
    def pages(self) -> list[BrowserPage]: ...

    @abstractmethod
    def raw(self) -> Any: ...


class BrowserManager(Module):
    @abstractmethod
    async def get_context(self, platform: str) -> BrowserContext: ...

    @abstractmethod
    async def save_session(self, platform: str) -> None: ...

    @abstractmethod
    async def login_flow(self, platform: str, url: str) -> None: ...

    @abstractmethod
    async def set_login_complete(self, platform: str) -> None: ...
