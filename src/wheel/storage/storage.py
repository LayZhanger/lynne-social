from abc import ABC, abstractmethod

from src.common import Module, UnifiedItem


class Storage(Module):
    @abstractmethod
    def save_items(self, items: list[UnifiedItem], date: str | None = None) -> None: ...

    @abstractmethod
    def load_items(self, date: str | None = None, *, platform: str | None = None) -> list[UnifiedItem]: ...

    @abstractmethod
    def save_report(self, markdown: str, date: str | None = None) -> None: ...

    @abstractmethod
    def load_report(self, date: str | None = None) -> str | None: ...

    @abstractmethod
    def save_summary(self, summary: dict, date: str | None = None) -> None: ...

    @abstractmethod
    def load_summary(self, date: str | None = None) -> dict | None: ...

    @abstractmethod
    def list_dates(self) -> list[str]: ...
