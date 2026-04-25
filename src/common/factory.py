from abc import ABC, abstractmethod
from typing import Generic, TypeVar

T = TypeVar("T")


class Factory(ABC, Generic[T]):
    @abstractmethod
    def create(self, config: object) -> T: ...
