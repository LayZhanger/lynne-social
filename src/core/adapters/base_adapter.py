from abc import ABC, abstractmethod
from typing import AsyncIterator

from src.common.models import UnifiedItem


class BaseAdapter(ABC):
    platform_name: str = ""

    @abstractmethod
    async def search(self, keywords: list[str], limit: int) -> AsyncIterator[UnifiedItem]: ...

    @abstractmethod
    async def get_user_posts(self, user_id: str, limit: int) -> AsyncIterator[UnifiedItem]: ...

    @abstractmethod
    async def get_trending(self, limit: int) -> AsyncIterator[UnifiedItem]: ...

    @abstractmethod
    def extract(self, data: dict) -> UnifiedItem: ...

    @abstractmethod
    async def health_check(self) -> bool: ...
