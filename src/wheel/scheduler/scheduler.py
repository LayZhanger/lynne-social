from abc import ABC, abstractmethod
from typing import Any, Callable

from src.common.module import Module


class JobStatus:
    """Status of a scheduled job."""
    name: str
    schedule: str
    next_run: str | None
    running: bool

    def __init__(self, name: str, schedule: str, next_run: str | None = None, running: bool = False):
        self.name = name
        self.schedule = schedule
        self.next_run = next_run
        self.running = running


class Scheduler(Module, ABC):
    """
    Generic timed callback scheduler.

    This module has ZERO imports from core/ — it is a pure wheel component.
    Agent binding happens in main.py (composition root).
    """

    @abstractmethod
    def add_job(self, name: str, schedule: str, callback: Callable[..., Any], *args: Any, **kwargs: Any) -> None:
        """Register a timed job. schedule is a cron expression or human-readable string."""
        ...

    @abstractmethod
    def remove_job(self, name: str) -> None:
        """Remove a job by name."""
        ...

    @abstractmethod
    def start(self) -> None:
        """Start the scheduler (begin firing jobs)."""
        ...

    @abstractmethod
    def stop(self) -> None:
        """Stop the scheduler (cease firing, clean up)."""
        ...

    @abstractmethod
    def get_jobs(self) -> list[JobStatus]:
        """Return current job list with statuses."""
        ...

    @abstractmethod
    def health_check(self) -> bool:
        """Check if the underlying scheduler engine is alive."""
        ...
