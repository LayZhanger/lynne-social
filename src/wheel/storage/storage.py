from abc import ABC, abstractmethod

from src.common import Module, UnifiedItem


class Storage(Module):
    @abstractmethod
    async def save_items(self, items: list[UnifiedItem], date: str | None = None) -> None: ...

    @abstractmethod
    async def load_items(self, date: str | None = None, *, platform: str | None = None) -> list[UnifiedItem]: ...

    @abstractmethod
    async def save_report(self, markdown: str, date: str | None = None) -> None: ...

    @abstractmethod
    async def load_report(self, date: str | None = None) -> str | None: ...

    @abstractmethod
    async def save_summary(self, summary: dict, date: str | None = None) -> None: ...

    @abstractmethod
    async def load_summary(self, date: str | None = None) -> dict | None: ...

    @abstractmethod
    async def list_dates(self) -> list[str]: ...
