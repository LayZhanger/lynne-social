from abc import ABC, abstractmethod


class ConfigLoader(ABC):
    @abstractmethod
    def load(self) -> object: ...

    @abstractmethod
    def reload(self) -> object: ...
